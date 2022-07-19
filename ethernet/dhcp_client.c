#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "ip/ip.h"
#include "ip/udp.h"
#include "onps_input.h"

#if SUPPORT_ETHERNET
#include "ethernet/dhcp_frame.h"
#include "ethernet/ethernet.h"
#define SYMBOL_GLOBALS
#include "ethernet/dhcp_client.h"
#undef SYMBOL_GLOBAL

static INT dhcp_send_packet(INT nInput, PST_NETIF pstNetif, UCHAR ubOptCode, UCHAR *pubOptions, UCHAR ubOptionsLen, UINT unTransId, 
                                in_addr_t unClientIp, UCHAR *pubClientMacAddr, in_addr_t unDstIP, EN_ONPSERR *penErr)
{    
    PST_DHCP_HDR pstDhcpHdr = (PST_DHCP_HDR)buddy_alloc(sizeof(ST_DHCP_HDR), penErr); 
    if (!pstDhcpHdr)
        return -1; 

    //* 先把dhcp选项数据挂载到链表上
    SHORT sBufListHead = -1;
    SHORT sOptionsNode = buf_list_get_ext(pubOptions, (UINT)ubOptionsLen, penErr);
    if (sOptionsNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sOptionsNode);

    //* 填充dhcp报文头
    pstDhcpHdr->ubOptCode = ubOptCode; 
    pstDhcpHdr->ubHardwareType = 1;                     //* 以太网卡mac地址类型
    pstDhcpHdr->ubHardwareAddrLen = ETH_MAC_ADDR_LEN;   //* mac地址长度
    pstDhcpHdr->ubHops = 0;                             //* 中继跳数初始为0
    pstDhcpHdr->unTransId = unTransId;                  //* 唯一标识当前一连串DHCP请求操作的事务id
    pstDhcpHdr->usElapsedSecs = 0;                      //* 固定为0
    pstDhcpHdr->usFlags = 0;                            //* 固定单播
    pstDhcpHdr->unClientIp = (UINT)unClientIp;          //* 客户端ip地址
    pstDhcpHdr->unYourIp = 0;                           //* 对于客户端来说这个地址固定为0
    pstDhcpHdr->unSrvIp = 0;                            //* 固定为0，不支持dhcp中继
    pstDhcpHdr->unGatewayIp = 0;                        //* 固定为0，同样不支持dhcp中继    
    memcpy(pstDhcpHdr->ubaClientMacAddr, pubClientMacAddr, ETH_MAC_ADDR_LEN); 
    memset(&pstDhcpHdr->ubaClientMacAddr[ETH_MAC_ADDR_LEN], 0, sizeof(pstDhcpHdr->ubaClientMacAddr) - ETH_MAC_ADDR_LEN + sizeof(pstDhcpHdr->szSrvName) + sizeof(pstDhcpHdr->szBootFileName)); 
    
    //* 将dhcp报文头挂载到链表头部
    SHORT sDhcpHdrNode;
    sDhcpHdrNode = buf_list_get_ext((UCHAR *)pstDhcpHdr, (UINT)sizeof(ST_DHCP_HDR), penErr);
    if (sDhcpHdrNode < 0)
    {
        //* 回收相关资源
        buf_list_free(sOptionsNode);
        buddy_free(pstDhcpHdr);

        return -1;
    }
    buf_list_put_head(&sBufListHead, sDhcpHdrNode); 

    //* 发送之，源地址固定为0
    INT nRtnVal = udp_send_ext(nInput, sBufListHead, unDstIP, DHCP_SRV_PORT, 0, pstNetif, penErr); 

    //* 回收相关资源    
    buf_list_free(sOptionsNode);    
    buf_list_free(sDhcpHdrNode);
    buddy_free(pstDhcpHdr);

    return nRtnVal; 
}

//* dhcp客户端启动
static INT  dhcp_client_start(EN_ONPSERR *penErr)
{
    INT nInput = onps_input_new(IPPROTO_UDP, penErr);
    if (nInput < 0)
        return nInput; 

    //* 首先看看指定的端口是否已被使用
    if (onps_input_port_used(IPPROTO_UDP, DHCP_CLT_PORT))
    {
        if(penErr)
            *penErr = ERRPORTOCCUPIED;
        goto __lblErr;
    }

    //* 设置地址
    ST_TCPUDP_HANDLE stHandle;    
    stHandle.unNetifIp = 0; //* 作为udp服务器启动，不绑定任何地址，当然也无法绑定因为还没获得合法ip地址
    stHandle.usPort = DHCP_CLT_PORT;
    if (onps_input_set(nInput, IOPT_SETTCPUDPADDR, &stHandle, penErr))
        return nInput; 

__lblErr: 
    onps_input_free(nInput);
    return -1; 
}

BOOL dhcp_req_addr(PST_NETIF pstNetif, EN_ONPSERR *penErr)
{
    //* 启动dhcp客户端（其实就是作为一个udp服务器启动）
    INT nInput = dhcp_client_start(penErr); 
    if (nInput < 0)
        return FALSE; 

    //* 
}

#endif
