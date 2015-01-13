#ifndef STREAMFILETOIP_H
#define STREAMFILETOIP_H
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

struct st{
    int addrlen;
    uint8_t *biss_key;
    char *biss_string;
    int fd;
    char *file_name;
    char *IP;
    struct dvbcsa_key_s	*key_s;
    struct sockaddr_in multicast_addr;
    uint16_t *multicast_PID_list;
    bool PMT_found;
    uint16_t PMT_PID;
    bool PMT_PID_found;
    unsigned short port;
    int service_length;
    char *service_name;
    int SID;
    bool SID_found;
    int sock;
};

uint8_t ts_packet_get_payload_offset(uint8_t *ts_packet);
int decode_hex_string(char *hex, uint8_t *bin, int asc_len);
int decode_hex_char(char c);
bool will_send(struct st *p, uint16_t i_pid);
void init_st(struct st *p);
void delete_st(struct st *p);
void init_socket(struct st *p);
void init_file(struct st *p);
void init_biss(struct st *p);

#endif
