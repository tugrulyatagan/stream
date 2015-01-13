#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <dvbcsa/dvbcsa.h>

#include "streamfiletoip.h"

#define BIT_RATE_COUNTER 5000
#define BILLION 1E9

inline uint8_t ts_packet_get_payload_offset(uint8_t *ts_packet) {
    if (ts_packet[0] != 0x47)
        return 0;

    uint8_t adapt_field   = (ts_packet[3] &~ 0xDF) >> 5; // 11x11111
    uint8_t payload_field = (ts_packet[3] &~ 0xEF) >> 4; // 111x1111

    if (!adapt_field && !payload_field) // Not allowed
        return 0;

    if (adapt_field) {
        uint8_t adapt_len = ts_packet[4];
        if (payload_field && adapt_len > 182) // Validity checks
            return 0;
        if (!payload_field && adapt_len > 183)
            return 0;
        if (adapt_len + 4 > 188) // adaptation field takes the whole packet
            return 0;
        return 4 + 1 + adapt_len; // ts header + adapt_field_len_byte + adapt_field_len
    } else {
        return 4; // No adaptation, data starts directly after TS header
    }
}

int decode_hex_char(char c) {
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
    if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
    return -1;
}

int decode_hex_string(char *hex, uint8_t *bin, int asc_len) {
    int i;
    for (i = 0; i < asc_len; i += 2) {
        int n1 = decode_hex_char(hex[i + 0]);
        int n2 = decode_hex_char(hex[i + 1]);
        if (n1 == -1 || n2 == -1)
            return -1;
        bin[i / 2] = (n1 << 4) | (n2 & 0xf);
    }
    return asc_len / 2;
}

inline bool will_send(struct st *p, uint16_t i_pid){
    if (i_pid == 0x00 || p->PMT_PID == i_pid){
        return 1;
    }
    int i;
    for (i=0; i < p->service_length; i++) {
        if (i_pid == p->multicast_PID_list[i])
            return 1;
    }
    return 0;
}

void init_socket(struct st *p){
    if ((p->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }
    bzero((char *)&p->multicast_addr, sizeof(p->multicast_addr));
    p->multicast_addr.sin_family      = AF_INET;
    p->multicast_addr.sin_addr.s_addr = inet_addr(p->IP);
    p->multicast_addr.sin_port        = htons(p->port);
    p->addrlen = sizeof(p->multicast_addr);
}


void init_file(struct st *p){
    p->fd = open(p->file_name, 0);
    if (p->fd < 0){
        printf("File %s couldnt open\n",p->file_name);
        exit(1);
    }
}

void init_biss(struct st *p){
    p->biss_key = malloc(16*sizeof(uint8_t));
    if (strlen(p->biss_string) == 16) {
        if (decode_hex_string(p->biss_string, p->biss_key, strlen(p->biss_string)) < 0) {
            fprintf(stderr, "ERROR: Invalid hex string for BISS key: %s\n", p->biss_string);
            exit(EXIT_FAILURE);
        }
    } else {
        // BISS key without checksum (12 symbols, 6 bytes)
        if (strlen(p->biss_string) != 12) {
            fprintf(stderr, "ERROR: BISS key should be 12 characters long.\n");
            exit(EXIT_FAILURE);
        }
        if (decode_hex_string(p->biss_string, p->biss_key, strlen(p->biss_string)) < 0) {
            fprintf(stderr, "ERROR: Invalid hex string for BISS key: %s\n", p->biss_string);
            exit(EXIT_FAILURE);
        }
        // Calculate BISS KEY crc
        memmove(p->biss_key + 4, p->biss_key + 3, 3);
        p->biss_key[3] = (uint8_t)(p->biss_key[0] + p->biss_key[1] + p->biss_key[2]);
        p->biss_key[7] = (uint8_t)(p->biss_key[4] + p->biss_key[5] + p->biss_key[6]);
    }
    memcpy(p->biss_key + 8, p->biss_key, 8);

    p->key_s = dvbcsa_key_alloc();
    dvbcsa_key_set(p->biss_key, p->key_s);
}

void init_st(struct st *p){
    p->addrlen = 0;
    p->biss_key = NULL;
    p->biss_string = NULL;
    p->fd = 0;
    p->file_name = NULL;
    p->IP = NULL;
    p->key_s = NULL;
    p->multicast_PID_list = NULL;
    p->PMT_found = 0;
    p->PMT_PID = 0;
    p->PMT_PID_found = 0;
    p->port = 0;
    p->service_length = 0;
    p->service_name = NULL;
    p->SID = 0;
    p->SID_found = 0;
    p->sock = 0;
}

void delete_st(struct st *p){
    if(p->biss_key)
        dvbcsa_key_free(p->biss_key);
    close(p->fd);
    free(p->multicast_PID_list);
    free(p->service_name);
    close(p->sock);
}


int main(int argc, char* argv[]){
    if (!(argc == 9 || argc == 11)){
        printf("Correct usage: .\\stream -f dump.ts -s 3 -i 192.168.0.1 -p 1234\n");
        exit(1);
    }
    int option;
    char *i_fileName, *i_SID, *i_IP, *i_port, *i_biss;
    struct st *param = malloc(sizeof *param);
    init_st(param);

    opterr = 0;
    while ( (option=getopt(argc, argv, "f:s:i:p:b:")) != EOF ) {
        switch ( option ) {
        case 'f':
            i_fileName = optarg;
            param->file_name = i_fileName;
            init_file(param);
            break;
        case 's':
            i_SID = optarg;
            param->SID = atoi(i_SID);
            break;
        case 'i':
            i_IP = optarg;
            param->IP = i_IP;
            break;
        case 'p':
            i_port = optarg;
            param->port = atoi(i_port);
            break;
        case 'b':
            i_biss = optarg;
            param->biss_string = i_biss;
            init_biss(param);
            break;
        case '?':
            fprintf(stderr,"Unknown option %c\n", optopt);
            printf("Correct usage: .\\stream -f dump.ts -s 3 -i 239.0.0.1 -p 6000 \n");
            exit(1);
            break;
        }
    }


    decode_sdt(param);
    if(!param->SID_found){
        printf("SERVICE ID NOT FOUND\n");
        exit(1);
    }
    else{
        printf("\nService found: %s\n",param->service_name);
    }

    lseek(param->fd,0,SEEK_SET);

    decode_pat(param);
    if (param->PMT_PID_found){
        printf("PMT PID found: %d 0x%02x\n",param->PMT_PID, param->PMT_PID);
    }
    else{
        printf("PMT PID not found\n");
        exit(1);
    }

    lseek(param->fd,0,SEEK_SET);

    decode_pmt(param);
    if (!param->PMT_found){
        printf("PMT not found\n");
        exit(1);
    }

    init_socket(param);

    lseek(param->fd,0,SEEK_SET);

    unsigned int bit_rate_counter, bit_rate, payload_offset;
    double accum;
    struct timespec requestStart, requestEnd, totalStart,totalEnd;
    clock_gettime(CLOCK_MONOTONIC, &requestStart);
    clock_gettime(CLOCK_MONOTONIC, &totalStart);

    uint8_t data[188];
    uint16_t i_pid;

    for(bit_rate_counter = 0; read(param->fd, data, 188) > 0; bit_rate_counter++){
        i_pid = ((uint16_t)(data[1] & 0x1f) << 8) + data[2];
        if (will_send(param,i_pid)){
            if ((data[3] >> 6) > 1) { // odd and even scrambled
                payload_offset = ts_packet_get_payload_offset(data);
                data[3] = data[3] &~ 0xc0; //ts_packet_set_not_scrambled
                dvbcsa_decrypt(param->key_s, data + payload_offset, 188 - payload_offset);
            }

            sendto(param->sock, data, 188, 0,(struct sockaddr *) &param->multicast_addr, param->addrlen);

            clock_gettime(CLOCK_MONOTONIC, &requestEnd);
            accum = ( requestEnd.tv_sec - requestStart.tv_sec ) + ( requestEnd.tv_nsec - requestStart.tv_nsec ) / BILLION;
            if(accum > 1){
                bit_rate = (bit_rate_counter*188*8/1000);
                printf("\r%d kbps", bit_rate);
                fflush(stdout);
                clock_gettime(CLOCK_MONOTONIC, &requestStart);
                bit_rate_counter=0;
            }
            usleep(150);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &totalEnd);
    accum = ( totalEnd.tv_sec - totalStart.tv_sec ) + ( totalEnd.tv_nsec - totalStart.tv_nsec ) / BILLION;
    printf("\nTotal elapsed: %lf second\n", accum);

    delete_st(param);
    free(param);
    return 0;
}
