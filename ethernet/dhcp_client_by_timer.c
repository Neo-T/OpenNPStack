#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "one_shot_timer.h"
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
#include "ethernet/arp.h"
#define SYMBOL_GLOBALS
#include "ethernet/dhcp_client_by_timer.h"
#undef SYMBOL_GLOBAL

extern INT dhcp_send_packet(INT nInput, PST_NETIF pstNetif, UCHAR ubOptCode, UCHAR *pubOptions, UCHAR ubOptionsLen, UINT unTransId, in_addr_t unClientIp, in_addr_t unDstIP, in_addr_t unSrcIp, EN_ONPSERR *penErr); 

static INT dhcp_recv_ack(PSTCB_RENEWAL_INFO pstcbRenewalInfo, INT(*pfunAckHandler)(PSTCB_RENEWAL_INFO pstcbRenewalInfo, UCHAR *pubAckPacket, INT nAckPacketLen))
{
    EN_ONPSERR enErr; 
    INT nRtnVal = 1; 

    pstcbRenewalInfo->bWaitAckSecs++;
    if (pstcbRenewalInfo->bWaitAckSecs > 3) //* 等待超时           
        return -1;    

    //* 申请一个接收缓冲区，预先分配给mmu管理的内存资源一定要足够大，否则会使得dhcp客户端停止运行，无法确保网卡能够继续租用ip地址
    UCHAR *pubRcvBuf = (UCHAR *)buddy_alloc(512, &enErr);
    if (!pubRcvBuf)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("dhcp_recv_discover_ack() failed, the NIC name is %s, %s\r\n", pstcbRenewalInfo->pstNetif->szName, onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return -1;
    }

    //* 读取应答数据，立即读取（超时时间为0），不阻塞
    INT nRcvedBytes = udp_recv_upper(pstcbRenewalInfo->nInput, pubRcvBuf, 512, NULL, NULL, 0);
    if (nRcvedBytes > 0)
    {
        PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstcbRenewalInfo->pstNetif->pvExtra;
        PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubRcvBuf;

        //* 1）如果不是dhcp报文则判定为不是合法的应答报文；
        //* 2）当前请求的事务id不匹配； 
        //* 3）客户端mac地址也要匹配目前只支持ethernet网卡的mac地址类型
        //* 满足以上情形之一则认为当前收到的报文并不是“我”正在等待的报文，需要继续等待接收
        if (DHCP_MAGIC_COOKIE != htonl(pstHdr->unMagicCookie)
            || pstcbRenewalInfo->unTransId != htonl(pstHdr->unTransId)
            || pstHdr->ubHardwareAddrLen != ETH_MAC_ADDR_LEN
            || !ethernet_mac_matched(pstExtra->ubaMacAddr, pstHdr->ubaClientMacAddr))
            goto __lblEnd; 

        nRtnVal = pfunAckHandler(pstcbRenewalInfo, pubRcvBuf, nRcvedBytes); 
    }

__lblEnd:     
    buddy_free(pubRcvBuf); //* 释放先前申请的接收缓冲区
    return nRtnVal;
}

//* 发送discover报文
static BOOL dhcp_send_discover(PSTCB_RENEWAL_INFO pstcbRenewalInfo)
{
    EN_ONPSERR enErr; 

    //* ethernet网卡的附加信息
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstcbRenewalInfo->pstNetif->pvExtra;
    UINT unOptionsOffset = 0;

    //* dhcp选项分配一块填充缓冲区并清零
    UCHAR ubaOptions[DHCP_OPTIONS_LEN_MIN];
    memset(ubaOptions, 0, sizeof(ubaOptions));

    //* 填充消息类型
    PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)ubaOptions;
    pstMsgType->stHdr.ubOption = DHCPOPT_MSGTYPE;
    pstMsgType->stHdr.ubLen = 1;
    pstMsgType->ubTpVal = DHCPMSGTP_DISCOVER;
    unOptionsOffset += sizeof(ST_DHCPOPT_MSGTYPE);

    //* 填充发起申请的客户端id    
    PST_DHCPOPT_CLTID pstCltId = (PST_DHCPOPT_CLTID)&ubaOptions[unOptionsOffset];
    pstCltId->stHdr.ubOption = DHCPOPT_CLIENTID;
    pstCltId->stHdr.ubLen = sizeof(ST_DHCPOPT_CLTID) - sizeof(ST_DHCPOPT_HDR);
    pstCltId->ubHardwareType = 1;
    memcpy(pstCltId->ubaMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
    unOptionsOffset += sizeof(ST_DHCPOPT_CLTID);

    //* 填充vendor标识信息
    PST_DHCPOPT_VENDORID pstVendorId = (PST_DHCPOPT_VENDORID)&ubaOptions[unOptionsOffset];
    pstVendorId->stHdr.ubOption = DHCPOPT_VENDORID;
    pstVendorId->stHdr.ubLen = sizeof(ST_DHCPOPT_VENDORID) - sizeof(ST_DHCPOPT_HDR);
    memcpy(pstVendorId->ubaTag, "RuBao;-)", sizeof("RuBao;-)") - 1);
    unOptionsOffset += sizeof(ST_DHCPOPT_VENDORID);

    //* 填充请求的参数列表
    PST_DHCPOPT_HDR pstParamListHdr = (PST_DHCPOPT_HDR)&ubaOptions[unOptionsOffset];
    pstParamListHdr->ubOption = DHCPOPT_REQLIST;
    pstParamListHdr->ubLen = 3;
    UCHAR *pubParamItem = ((UCHAR *)pstParamListHdr) + sizeof(ST_DHCPOPT_HDR);
    pubParamItem[0] = DHCPOPT_SUBNETMASK;
    pubParamItem[1] = DHCPOPT_ROUTER;
    pubParamItem[2] = DHCPOPT_DNS;

    //* 选项结束
    pubParamItem[3] = DHCPOPT_END;

    //* 发送
    pstcbRenewalInfo->unTransId = (UINT)rand();
    if (dhcp_send_packet(pstcbRenewalInfo->nInput, pstcbRenewalInfo->pstNetif, DHCP_OPT_REQUEST, ubaOptions, sizeof(ubaOptions), pstcbRenewalInfo->unTransId, 0, 0xFFFFFFFF, 0, NULL) > 0)
        return TRUE;
    else
        return FALSE;
}

static INT dhcp_discover_ack_handler(PSTCB_RENEWAL_INFO pstcbRenewalInfo, UCHAR *pubAckPacket, INT nAckPacketLen)
{
    PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubAckPacket; 
    UCHAR *pubOptions = pubAckPacket + sizeof(ST_DHCP_HDR);
    USHORT usOptionsLen = (USHORT)(nAckPacketLen - sizeof(ST_DHCP_HDR));
    PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_MSGTYPE);
    if (!pstMsgType || DHCPMSGTP_OFFER != pstMsgType->ubTpVal) //* 必须携带dhcp报文类型并且一定是offer报文才可以,如果不是则立即认为超时，重新发送   
        return -1;     

    //* 取出回馈offer报文的dhcp服务器identifier，如果无法取出dhcp服务器的identifier，给个超时认定以便重新发送
    PST_DHCPOPT_SRVID pstSrvId = (PST_DHCPOPT_SRVID)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_SRVID);
    if (!pstSrvId)
        return -1; 

    //* 取出分配的ip地址及dhcp服务器的ip地址
    pstcbRenewalInfo->stReqAddr.unOfferIp = pstHdr->unYourIp;
    pstcbRenewalInfo->unDhcpSrvIp = pstSrvId->unSrvIp; 

    return 0;
}

//* 发送request请求报文
static BOOL dhcp_send_request(PSTCB_RENEWAL_INFO pstcbRenewalInfo)
{
    EN_ONPSERR enErr;
    pstcbRenewalInfo->stReqAddr.bSndNum++;     

    //* ethernet网卡的附加信息
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstcbRenewalInfo->pstNetif->pvExtra;
    UINT unOptionsOffset = 0; 

    //* dhcp选项分配一块填充缓冲区并清零
    UCHAR ubaOptions[DHCP_OPTIONS_LEN_MIN];
    memset(ubaOptions, 0, sizeof(ubaOptions)); 

    //* 填充消息类型
    PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)ubaOptions;
    pstMsgType->stHdr.ubOption = DHCPOPT_MSGTYPE;
    pstMsgType->stHdr.ubLen = 1;
    pstMsgType->ubTpVal = DHCPMSGTP_REQUEST;
    unOptionsOffset += sizeof(ST_DHCPOPT_MSGTYPE);

    //* 填充发起申请的客户端id    
    PST_DHCPOPT_CLTID pstCltId = (PST_DHCPOPT_CLTID)&ubaOptions[unOptionsOffset];
    pstCltId->stHdr.ubOption = DHCPOPT_CLIENTID;
    pstCltId->stHdr.ubLen = sizeof(ST_DHCPOPT_CLTID) - sizeof(ST_DHCPOPT_HDR);
    pstCltId->ubHardwareType = 1;
    memcpy(pstCltId->ubaMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
    unOptionsOffset += sizeof(ST_DHCPOPT_CLTID);

    //* 填充由服务器分配的ip地址
    PST_DHCPOPT_REQIP pstReqIp = (PST_DHCPOPT_REQIP)&ubaOptions[unOptionsOffset];
    pstReqIp->stHdr.ubOption = DHCPOPT_REQIP;
    pstReqIp->stHdr.ubLen = sizeof(ST_DHCPOPT_REQIP) - sizeof(ST_DHCPOPT_HDR);
    pstReqIp->unVal = pstcbRenewalInfo->stReqAddr.unOfferIp;
    unOptionsOffset += sizeof(ST_DHCPOPT_REQIP);

    //* 填充dhcp服务器identifier
    PST_DHCPOPT_SRVID pstSrvId = (PST_DHCPOPT_SRVID)&ubaOptions[unOptionsOffset];
    pstSrvId->stHdr.ubOption = DHCPOPT_SRVID;
    pstSrvId->stHdr.ubLen = sizeof(ST_DHCPOPT_SRVID) - sizeof(ST_DHCPOPT_HDR);
    pstSrvId->unSrvIp = pstcbRenewalInfo->unDhcpSrvIp;
    unOptionsOffset += sizeof(ST_DHCPOPT_SRVID);

    //* 填充vendor标识信息
    PST_DHCPOPT_VENDORID pstVendorId = (PST_DHCPOPT_VENDORID)&ubaOptions[unOptionsOffset];
    pstVendorId->stHdr.ubOption = DHCPOPT_VENDORID;
    pstVendorId->stHdr.ubLen = sizeof(ST_DHCPOPT_VENDORID) - sizeof(ST_DHCPOPT_HDR);
    memcpy(pstVendorId->ubaTag, "RuBao;-)", sizeof("RuBao;-)") - 1);
    unOptionsOffset += sizeof(ST_DHCPOPT_VENDORID);

    //* 填充请求的参数列表
    PST_DHCPOPT_HDR pstParamListHdr = (PST_DHCPOPT_HDR)&ubaOptions[unOptionsOffset];
    pstParamListHdr->ubOption = DHCPOPT_REQLIST;
    pstParamListHdr->ubLen = 3;
    UCHAR *pubParamItem = ((UCHAR *)pstParamListHdr) + sizeof(ST_DHCPOPT_HDR);
    pubParamItem[0] = DHCPOPT_SUBNETMASK;
    pubParamItem[1] = DHCPOPT_ROUTER;
    pubParamItem[2] = DHCPOPT_DNS;

    //* 选项结束
    pubParamItem[3] = DHCPOPT_END;

    //* 发送
    if (dhcp_send_packet(pstcbRenewalInfo->nInput, pstcbRenewalInfo->pstNetif, DHCP_OPT_REQUEST, ubaOptions, sizeof(ubaOptions), pstcbRenewalInfo->unTransId, 0, 0xFFFFFFFF, 0, NULL) > 0)
        return TRUE;
    else
        return FALSE;
}

static INT dhcp_request_ack_handler(PSTCB_RENEWAL_INFO pstcbRenewalInfo, UCHAR *pubAckPacket, INT nAckPacketLen)
{         
    //* 解析应答报文取出具体的选项相关信息
    PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubAckPacket;
    UCHAR *pubOptions = pubAckPacket + sizeof(ST_DHCP_HDR);
    USHORT usOptionsLen = (USHORT)(nAckPacketLen - sizeof(ST_DHCP_HDR));
    PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_MSGTYPE);
    if (!pstMsgType || (DHCPMSGTP_ACK != pstMsgType->ubTpVal && DHCPMSGTP_NAK != pstMsgType->ubTpVal)) //* 必须携带dhcp报文类型并且一定是ack/nack报文才可以，如果不是则认为超时，重新发送
        return -1; 

    //* 说明地址冲突，需要回到discover阶段
    if (DHCPMSGTP_NAK == pstMsgType->ubTpVal)
        return 2; 

    pstcbRenewalInfo->stReqAddr.stIPv4.unAddr = pstHdr->unYourIp;

    //* 取出子网掩码
    PST_DHCPOPT_SUBNETMASK pstNetmask = (PST_DHCPOPT_SUBNETMASK)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_SUBNETMASK);
    if (!pstNetmask)
        return -1; 
    pstcbRenewalInfo->stReqAddr.stIPv4.unSubnetMask = pstNetmask->unVal;

    //* 取出网关地址
    PST_DHCPOPT_ROUTER pstRouter = (PST_DHCPOPT_ROUTER)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_ROUTER);
    if (!pstRouter)
        return -1; 
    pstcbRenewalInfo->stReqAddr.stIPv4.unGateway = pstRouter->unVal;

    //* 取出dns服务器地址
    PST_DHCPOPT_DNS pstDns = (PST_DHCPOPT_DNS)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_DNS);
    if (!pstDns)
        return -1; 
    pstcbRenewalInfo->stReqAddr.stIPv4.unPrimaryDNS = pstDns->unPrimary; 
    pstcbRenewalInfo->stReqAddr.stIPv4.unSecondaryDNS = pstDns->unSecondary; 

    //* 取出租约信息
    PST_DHCPOPT_LEASETIME pstLeaseTime = (PST_DHCPOPT_LEASETIME)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_LEASETIME);
    if (!pstLeaseTime)
        return -1; 
    pstcbRenewalInfo->unLeaseTime = htonl(pstLeaseTime->unVal); 

    return 0; 
}

static void dhcp_req_addr_timer_new(PSTCB_RENEWAL_INFO pstcbRenewalInfo, INT nTimeout)
{
    //* 失败的唯一原因就是定时器资源不够用，所以必须确保定时器资源足够使用，否则将影响协议栈dhcp客户端的正常运行
    if (!one_shot_timer_new(dhcp_req_addr_timeout_handler, pstcbRenewalInfo, nTimeout))
    {        
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("dhcp_req_addr_timer_new() failed, the NIC name is %s, %s\r\n", pstcbRenewalInfo->pstNetif->szName, onps_error(ERRNOIDLETIMER));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }
}

//* 使用定时器序列完成ip地址请求的主处理函数
void dhcp_req_addr_timeout_handler(void *pvParam)
{
    INT nTimeout, nRtnVal;
    PSTCB_RENEWAL_INFO pstcbRenewalInfo = (PSTCB_RENEWAL_INFO)pvParam; 
    UCHAR ubaDstMac[ETH_MAC_ADDR_LEN]; 
    
    //* dhcp客户端地址请求状态机
    switch (pstcbRenewalInfo->bState)
    {
    case 0: //* discover
        if (dhcp_send_discover(pstcbRenewalInfo))
        {
            nTimeout = 1;

            //* 发送成功，等待应答的开始时间清零，并将状态机迁移到等待应答报文到达的阶段
            pstcbRenewalInfo->bWaitAckSecs = 0;
            pstcbRenewalInfo->bState = 1;
        }
        else //* 发送失败的可能性理论上是存在的（给协议栈分配的资源不够），所以需要处理这种情况
        {
            //* 延时一小段时间后再次重新发送
            nTimeout = 15;             
        }
        dhcp_req_addr_timer_new(pstcbRenewalInfo, nTimeout);

        break; 

    case 1: //* discover ack
        nRtnVal = dhcp_recv_ack(pstcbRenewalInfo, dhcp_discover_ack_handler); 
        if (0 == nRtnVal) //* 收到应答
        {                              
            nTimeout = 1; 
            pstcbRenewalInfo->stReqAddr.bSndNum = 0; 
            pstcbRenewalInfo->bState = 2;   //* 进入request阶段
        }
        else 
        {
            //* 尚未超时，继续等待
            if (nRtnVal > 0)
                nTimeout = 1;
            else
            {
                
                nTimeout = 8;                   //* 直接使用协议栈要求的中间延时时长，而不是2/4/8/16渐次延长
                pstcbRenewalInfo->bState = 0;   //* 回到discover阶段
            }
        }        
        dhcp_req_addr_timer_new(pstcbRenewalInfo, nTimeout);

        break; 

    case 2: //* request        
        if (dhcp_send_request(pstcbRenewalInfo))
        {
            nTimeout = 1;
            pstcbRenewalInfo->bWaitAckSecs = 0;
            pstcbRenewalInfo->bState = 3;   //* 进入request ack阶段
        }
        else //* 处理理论上存在的发送失败
        {
            //* 与之前的处理手段相同，延时稍长的一小段时间确保需要的资源可用了
            nTimeout = 15; 

            //* 是否已经超出了重试次数，超出则回到discover阶段
            if (pstcbRenewalInfo->stReqAddr.bSndNum > 2) 
                pstcbRenewalInfo->bState = 0;
        }
        dhcp_req_addr_timer_new(pstcbRenewalInfo, nTimeout); 

        break; 

    case 3: //* request ack
        nRtnVal = dhcp_recv_ack(pstcbRenewalInfo, dhcp_request_ack_handler); 
        if (0 == nRtnVal)
        {
            pstcbRenewalInfo->stReqAddr.bSndNum = 0; 
            pstcbRenewalInfo->bState = 4;   //* 进入arp探测阶段
        }
        else
        {
            //* 超时了并且到达重试次数的上限或新分配的地址冲突
            if (2 == nRtnVal || pstcbRenewalInfo->stReqAddr.bSndNum > 2)                            
                pstcbRenewalInfo->bState = 0; //* 回到discover阶段重新请求分配ip地址
            else if(nRtnVal < 0)
                pstcbRenewalInfo->bState = 2; //* 接收超时，则回到request阶段
            else; 
        }
        dhcp_req_addr_timer_new(pstcbRenewalInfo, 1); 
        
        break; 

    case 4: //* gratuitous arp request
        if (pstcbRenewalInfo->stReqAddr.bSndNum++ > 4) //* 不存在，可以使用这个地址了
        {

        }
        else
        {
            nRtnVal = arp_get_mac(pstcbRenewalInfo->pstNetif, pstcbRenewalInfo->stReqAddr.stIPv4.unAddr, pstcbRenewalInfo->stReqAddr.stIPv4.unAddr, ubaDstMac, NULL);
            if (nRtnVal)
            {

            }
            else //* 存在该条目，则确定新分配的地址存在ip冲突，需要重新申请                         
                pstcbRenewalInfo->bState = 5; //* 进入decline阶段
            dhcp_req_addr_timer_new(pstcbRenewalInfo, 1);
        }        

        break; 
    }
}

#endif
