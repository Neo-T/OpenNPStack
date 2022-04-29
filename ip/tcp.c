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



static INT tcp_send_packet(in_addr_t unSrvAddr, USHORT usSrvPort)
{

}

INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort)
{


    return -1; 
}

