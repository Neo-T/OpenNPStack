#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "onps_input.h"
#include "ip/tcp_link.h"
#include "ip/tcp_frame.h"
#include "ip/tcp_options.h" 
#define SYMBOL_GLOBALS
#include "ip/tcp.h"
#undef SYMBOL_GLOBALS

static USHORT tcp_port_new(void)
{
    USHORT usPort; 

__lblPortNew:    
    INT nRand = rand() % 20001;
    usPort = 65535 - (USHORT)(rand() % (TCP_PORT_START + 1)); 

    //* 确定是否正在使用，如果未使用则没问题
    if (onps_input_tcp_port_used(usPort))
        goto __lblPortNew;
    else
        return usPort; 
}

static INT tcp_send_packet(in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort, 
                            UINT unSeqNum, UINT unAckSeqNum, UNI_TCP_FLAG uniFlag, USHORT usWndSize, UCHAR *pubOptions, INT nOptionsBytes)
{
    
}

INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort)
{
    EN_ONPSERR enErr;    

    //* 获取链路信息存储节点
    PST_TCPLINK pstLink; 
    if (!onps_input_get(nInput, IOPT_GETATTACH, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr); 
        return -1; 
    }

    //* 获取tcp链路句柄访问地址，该地址保存当前tcp链路由协议栈自动分配的端口及本地网络接口地址
    PST_TCPUDP_HANDLE pstHandle; 
    if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

    //* 先寻址，因为tcp校验和计算需要用到本地地址
    UINT unNetifIp = route_get_netif_ip(unSrvAddr);
    if (!unNetifIp)
    {
        onps_set_last_error(nInput, ERRADDRESSING);
        return -1;
    }

    //* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
    pstHandle->unIP = unNetifIp;
    pstHandle->usPort = tcp_port_new(); 

    UNI_TCP_FLAG uniFlag; 
    uniFlag.usVal = 0; 
    uniFlag.stb16.syn = 1;      
    pstLink->usWndSize = TCPRCVBUF_SIZE_DEFAULT - sizeof(ST_TCP_HDR) - TCP_OPTIONS_SIZE_MAX;

    UCHAR ubaOptions[TCP_OPTIONS_SIZE_MAX]; 
    INT nOptionsSize = tcp_options_attach(ubaOptions, sizeof(ubaOptions));

    return tcp_send_packet(pstHandle->unIP, pstHandle->usPort, unSrvAddr, usSrvPort, pstLink->unSeqNum, pstLink->unAckNum, uniFlag, pstLink->usWndSize, ubaOptions, nOptionsSize);
}

