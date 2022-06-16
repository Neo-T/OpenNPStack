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
#include "ethernet/arp_frame.h"
#define SYMBOL_GLOBALS
#include "ethernet/arp.h"
#undef SYMBOL_GLOBAL

//* arp条目表
typedef struct _ST_ETHIIENTRY_IPV4_ {
    UINT unCacheTime;       //* arp条目缓存时间
    UINT unIPAddr;          //* IP地址
    UCHAR ubaMacAddr[6];    //* 对应的ip地址
} ST_ETHIIENTRY_IPV4, *PST_ETHIIENTRY_IPV4; 

static 

#endif
