#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "utils.h"
#include "netif/netif.h"

#define SYMBOL_GLOBALS
#include "ip/ip.h"
#undef SYMBOL_GLOBALS
#include "ip/ip_frame.h" 

static USHORT l_usIPIdentifier = 0; 

INT ip_send(UINT unDstAddr, EN_IPPROTO enProto, UCHAR ubTTL, SHORT sBufListHead, EN_ERROR_CODE *penErrCode)
{
    PST_NETIF pstNetif = netif_get(); 
    if (NULL == pstNetif)
        return -1; 

    os_critical_init();
    ST_IP_HDR stHdr; 
    stHdr.bitHdrLen = sizeof(ST_IP_HDR) / sizeof(UINT); //* IP头长度，单位：UINT
    stHdr.bitVer = 4; //* IPv4
    stHdr.bitMustBeZero = 0; 
    stHdr.bitTOS = 0; //* 一般服务
    stHdr.bitPrior = 0; 
    stHdr.usPacketLen = htons(sizeof(ST_IP_HDR) + (USHORT)buf_list_get_len(sBufListHead)); 
    os_enter_critical();
    {
        stHdr.usID = htons(l_usIPIdentifier);
        l_usIPIdentifier++;
    }    
    os_exit_critical(); 
    stHdr.bitFragOffset0 = 0;
    stHdr.bitFlag = 1 << 1;  //* Don't fragment
    stHdr.bitFragOffset1 = 0;
    stHdr.ubTTL = ubTTL;
    stHdr.ubProto = (UCHAR)enProto;
    stHdr.usChksum = 0;
    stHdr.unSrcIP = pstNetif->stIPv4.unAddr; 
    stHdr.unDestIP = unDstAddr; 
}
