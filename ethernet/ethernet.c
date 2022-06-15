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

const ST_NETIFEXTRA_ETH l_staExtraOfEth[ETHERNET_NUM]; 

void ethernet_init(void)
{
    memset(l_staExtraOfEth, 0, sizeof(l_staExtraOfEth)); 
}

//* 添加ethernet网卡
PST_NETIF ethernet_add(const CHAR *pszIfName, const UCHAR ubaMacAddr[6], PST_IPV4 pstIPv4, PFUN_NETIF_SEND pfunSend, EN_ONPSERR *penErr)
{
    PST_NETIF pstNetif = NULL; 
    PST_NETIFEXTRA_ETH pstExtra = NULL; 
    INT i; 
    os_critical_init(); 
    os_enter_critical();
    {
        for (i = 0; i < ETHERNET_NUM; i++)
        {
            if (!l_staExtraOfEth[i].bIsUsed)
            {
                pstExtra = &l_staExtraOfEth[i]; 
                pstExtra->bIsUsed = TRUE;                  
                break; 
            }
        }
    }
    os_exit_critical();    

    if (!pstExtra)
    {
        if (penErr)
            *penErr = ERRETHEXTRAEMPTY;
        return NULL;
    }

    PST_NETIF_NODE pstIfNode = netif_add(NIF_ETHERNET, pszIfName, pstIPv4, pfunSend, pstExtra, penErr); 
    if (pstIfNode)
    {
        pstNetif = &pstIfNode->stIf;

        pstExtra->pstIPList = NULL;
        memcpy(pstExtra->ubaMacAddr, ubaMacAddr, 6); 
        if (pstIPv4->unAddr) //* 地址不为0则为静态地址，需要将其添加到路由表中
        {
            pstExtra->bIsStaticAddr = TRUE;

            //* 添加到路由表，使其成为缺省路由
            if (!route_add(pstNetif, 0, 0, 0, penErr))
            {
                netif_del(pstIfNode);
                pstNetif = NULL; 

                //* 归还刚刚占用的附加信息节点，不需要关中断进行保护，获取节点的时候需要
                pstExtra->bIsUsed = FALSE;
            }
        }
        else        
            pstExtra->bIsStaticAddr = FALSE;         
    }

    return pstNetif;
}

//* 删除ethernet网卡
void ethernet_del(PST_NETIF pstNetif)
{
    //* 先从路由表删除
    route_del_ext(pstNetif); 

    //* 再从网卡链表删除
    netif_del_ext(pstNetif); 
}

#endif
