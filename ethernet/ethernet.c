#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "ip/ip.h"

#if SUPPORT_ETHERNET
#include "ethernet/ethernet_frame.h"
#define SYMBOL_GLOBALS
#include "ethernet/ethernet.h"
#undef SYMBOL_GLOBALS

//* ethernet ii层协议支持的上层协议
typedef struct _ST_ETHIIPROTOCOL_ {
    USHORT usType;
    void(*pfunUpper)(UCHAR *pubPacket, INT nPacketLen);
} ST_ETHIIPROTOCOL, *PST_ETHIIPROTOCOL;
static const ST_ETHIIPROTOCOL lr_staProtocol[] = { 
}; 

#endif
