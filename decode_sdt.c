#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/psi.h>
#include <dvbpsi/demux.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/sdt.h>

#include "streamfiletoip.h"


/*****************************************************************************
 * ReadPacket
 *****************************************************************************/
static bool ReadPacket(int i_fd, uint8_t* p_dst)
{
    int i = 187;
    int i_rc = 1;

    p_dst[0] = 0;

    while((p_dst[0] != 0x47) && (i_rc > 0))
    {
        i_rc = read(i_fd, p_dst, 1);
    }

    while((i != 0) && (i_rc > 0))
    {
        i_rc = read(i_fd, p_dst + 188 - i, i);
        if(i_rc >= 0)
            i -= i_rc;
    }

    return (i == 0) ? true : false;
}

/*****************************************************************************
 * DumpSDT
 *****************************************************************************/
static void DumpSDT(struct st *info, dvbpsi_sdt_t* p_sdt)
{
    dvbpsi_sdt_service_t* p_service = p_sdt->p_first_service;
    dvbpsi_descriptor_t *p_descriptor;

    printf(  "ts_id : %d\n", p_sdt->i_extension);
    printf("service_id - service_name\n*********************\n");
    while(p_service)
    {
        p_descriptor = p_service->p_first_descriptor;
        if(p_service->i_service_id == info->SID){
            info->SID_found = 1;
            info->service_name = malloc(p_descriptor->i_length);
            memcpy(info->service_name, p_descriptor->p_data, p_descriptor->i_length);
        }
        printf("  %02d 0x%02x - ", p_service->i_service_id, p_service->i_service_id);

        while(p_descriptor)
        {
            int i;
            for(i = 0; i < p_descriptor->i_length; i++){
                if (p_descriptor->p_data[i] != '\n')
                    printf("%c", p_descriptor->p_data[i]);
            }
            printf("\n");
            p_descriptor = p_descriptor->p_next;
        }
        p_service = p_service->p_next;
    }
    dvbpsi_sdt_delete(p_sdt);
}

/*****************************************************************************
 * DVBPSI messaging callback
 *****************************************************************************/
static void message(dvbpsi_t *handle, const dvbpsi_msg_level_t level, const char* msg)
{
    switch(level)
    {
    case DVBPSI_MSG_ERROR: fprintf(stderr, "Error: "); break;
    case DVBPSI_MSG_WARN:  fprintf(stderr, "Warning: "); break;
    case DVBPSI_MSG_DEBUG: fprintf(stderr, "Debug: "); break;
    default: /* do nothing */
        return;
    }
    fprintf(stderr, "%s\n", msg);
}

/*****************************************************************************
 * NewSubtable
 *****************************************************************************/
static void NewSubtable(dvbpsi_t *p_dvbpsi, uint8_t i_table_id, uint16_t i_extension, struct st *info, void * p_zero)
{
    if(i_table_id == 0x42)
    {
        if (!dvbpsi_sdt_attach(p_dvbpsi, i_table_id, i_extension, DumpSDT, info))
            fprintf(stderr, "Failed to attach SDT subdecoder\n");
    }
}

void decode_sdt(struct st *info)
{
    uint8_t data[188];
    dvbpsi_t *p_dvbpsi;
    bool b_ok;

    p_dvbpsi = dvbpsi_new(&message, DVBPSI_MSG_WARN);
    if (p_dvbpsi == NULL)
        goto out;

    if (!dvbpsi_AttachDemux(p_dvbpsi, NewSubtable, info))
        goto out;

    b_ok = ReadPacket(info->fd, data);

    while(b_ok)
    {
        uint16_t i_pid = ((uint16_t)(data[1] & 0x1f) << 8) + data[2];
        if(i_pid == 0x11)
            dvbpsi_packet_push(p_dvbpsi, data);
        b_ok = ReadPacket(info->fd, data);
    }

out:
    if (p_dvbpsi)
    {
        dvbpsi_DetachDemux(p_dvbpsi);
        dvbpsi_delete(p_dvbpsi);
    }
}
