#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/psi.h>
#include <dvbpsi/pat.h>

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
 * DumpPAT
 *****************************************************************************/
static void DumpPAT(struct st *info, dvbpsi_pat_t* p_pat)
{
    dvbpsi_pat_program_t* p_program = p_pat->p_first_program;
    while(p_program)
    {
        if(info->SID == p_program->i_number){
            info->PMT_PID = p_program->i_pid;
            info->PMT_PID_found = 1;
        }
        p_program = p_program->p_next;
    }
    dvbpsi_pat_delete(p_pat);
}

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

void decode_pat(struct st *info)
{
    uint8_t data[188];
    dvbpsi_t *p_dvbpsi;
    bool b_ok;

    p_dvbpsi = dvbpsi_new(&message, DVBPSI_MSG_WARN);
    if (p_dvbpsi == NULL)
        goto out;

    if (!dvbpsi_pat_attach(p_dvbpsi, DumpPAT, info))
        goto out;

    b_ok = ReadPacket(info->fd, data);

    while(b_ok)
    {
        uint16_t i_pid = ((uint16_t)(data[1] & 0x1f) << 8) + data[2];
        if(i_pid == 0x0)
            dvbpsi_packet_push(p_dvbpsi, data);
        b_ok = ReadPacket(info->fd, data);
    }

out:
    if (p_dvbpsi)
    {
        dvbpsi_pat_detach(p_dvbpsi);
        dvbpsi_delete(p_dvbpsi);
    }
}
