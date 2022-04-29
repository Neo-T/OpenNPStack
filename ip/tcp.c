#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "ip/tcp_link.h"
#include "ip/tcp_frame.h"
#include "ip/tcp_options.h" 
#define SYMBOL_GLOBALS
#include "ip/tcp.h"
#undef SYMBOL_GLOBALS

static INT tcp_send_packet(in_addr_t unSrvAddr, USHORT usSrvPort, UINT unSeqNum, UINT unAckSeqNum, 
                            UNI_TCP_FLAG uniFlag, USHORT usWndSize, UCHAR *pubOptions, INT nOptionsBytes)
{

}

INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort)
{
    EN_ONPSERR enErr;
    PST_TCPLINK pstLink; 
    if (!onps_input_get(nInput, IOPT_GETATTACH, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr); 
        return -1; 
    }

    UNI_TCP_FLAG uniFlag; 
    uniFlag.usVal = 0; 
    uniFlag.stb16.syn = 1; 
    pstLink->unSeqNum = 0; 
    pstLink->usWndSize = TCPRCVBUF_SIZE_DEFAULT - TCP_HDR_SIZE_MAX; 

    return -1; 
}

