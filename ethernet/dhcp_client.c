/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
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
#include "ethernet/ethernet.h"
#include "ethernet/arp.h"
#include "ethernet/dhcp_client_by_timer.h"
#define SYMBOL_GLOBALS
#include "ethernet/dhcp_client.h"
#undef SYMBOL_GLOBAL
#include "ethernet/dhcp_frame.h"

//* 保存租约信息，注意这个设计意味着在同一台设备上协议栈仅支持单路dhcp客户端，如果设备存在双网卡，另一路网卡必须指定静态ip地址
static STCB_RENEWAL_INFO l_stcbRenewalInfo; 

INT dhcp_send_packet(INT nInput, PST_NETIF pstNetif, UCHAR ubOptCode, UCHAR *pubOptions, UCHAR ubOptionsLen, UINT unTransId, in_addr_t unClientIp, in_addr_t unDstIP, in_addr_t unSrcIp, EN_ONPSERR *penErr)
{        
    PST_DHCP_HDR pstDhcpHdr = (PST_DHCP_HDR)buddy_alloc(sizeof(ST_DHCP_HDR), penErr); 
    if (!pstDhcpHdr)
        return -1; 

    //* 先把dhcp选项数据挂载到链表上
    SHORT sBufListHead = -1;
    SHORT sOptionsNode = buf_list_get_ext(pubOptions, (UINT)ubOptionsLen, penErr);
    if (sOptionsNode < 0)
    {
        buddy_free(pstDhcpHdr); 
        return -1;
    }
    buf_list_put_head(&sBufListHead, sOptionsNode);

    //* 填充dhcp报文头
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
    pstDhcpHdr->ubOptCode = ubOptCode; 
    pstDhcpHdr->ubHardwareType = 1;                     //* 以太网卡mac地址类型
    pstDhcpHdr->ubHardwareAddrLen = ETH_MAC_ADDR_LEN;   //* mac地址长度
    pstDhcpHdr->ubHops = 0;                             //* 中继跳数初始为0
    pstDhcpHdr->unTransId = htonl(unTransId);           //* 唯一标识当前一连串DHCP请求操作的事务id
    pstDhcpHdr->usElapsedSecs = 0;                      //* 固定为0
    pstDhcpHdr->usFlags = 0;                            //* 固定单播
    pstDhcpHdr->unClientIp = (UINT)unClientIp;          //* 客户端ip地址
    pstDhcpHdr->unYourIp = 0;                           //* 对于客户端来说这个地址固定为0
    pstDhcpHdr->unSrvIp = 0;                            //* 固定为0，不支持dhcp中继
    pstDhcpHdr->unGatewayIp = 0;                        //* 固定为0，同样不支持dhcp中继    
    memcpy(pstDhcpHdr->ubaClientMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN); 
    memset(&pstDhcpHdr->ubaClientMacAddr[ETH_MAC_ADDR_LEN], 0, sizeof(pstDhcpHdr->ubaClientMacAddr) - ETH_MAC_ADDR_LEN + sizeof(pstDhcpHdr->szSrvName) + sizeof(pstDhcpHdr->szBootFileName)); 
    pstDhcpHdr->unMagicCookie = htonl(DHCP_MAGIC_COOKIE); 
    
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

    //* 发送
    INT nRtnVal = udp_send_ext(nInput, sBufListHead, htonl(unDstIP), DHCP_SRV_PORT, unSrcIp, pstNetif, penErr);

    //* 回收相关资源    
    buf_list_free(sOptionsNode);    
    buf_list_free(sDhcpHdrNode);
    buddy_free(pstDhcpHdr);

    return nRtnVal; 
}

//* 发送并等待应答
static INT dhcp_send_and_wait_ack(INT nInput, PST_NETIF pstNetif, UCHAR ubOptCode, UCHAR *pubOptions, UCHAR ubOptionsLen, UINT unTransId,
                                in_addr_t unClientIp, in_addr_t unDstIP, UCHAR *pubRcvBuf, USHORT usRcvBufSize, EN_ONPSERR *penErr)
{
    //* 发送报文
    INT nRtnVal = dhcp_send_packet(nInput, pstNetif, ubOptCode, pubOptions, ubOptionsLen, unTransId, unClientIp, unDstIP, 0, penErr); 
    if (nRtnVal <= 0)
        return nRtnVal; 

    //* 等待应答到来
    //* ==========================================================================================================
    INT nRcvBytes; 
    CHAR bWaitSecs = 0; 
    
__lblRecv:       
    if (bWaitSecs++ > 3) //* 等待超时
    {
        if (penErr)
            *penErr = ERRWAITACKTIMEOUT;        
        return 0;
    }

    //* 接收
    nRcvBytes = udp_recv_upper(nInput, pubRcvBuf, (UINT)usRcvBufSize, NULL, NULL, 1);     
    if (nRcvBytes > 0)
    {
        PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
        PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubRcvBuf; 

        //* 1）如果不是dhcp报文则判定为不是合法的应答报文；
        //* 2）当前请求的事务id不匹配； 
        //* 3）客户端mac地址也要匹配目前只支持ethernet网卡的mac地址类型
        //* 满足以上情形之一则认为当前收到的报文并不是“我”正在等待的报文，只要不超时就需要继续等待
        if (DHCP_MAGIC_COOKIE != htonl(pstHdr->unMagicCookie)
            || unTransId != htonl(pstHdr->unTransId)
            || pstHdr->ubHardwareAddrLen != ETH_MAC_ADDR_LEN
            || !ethernet_mac_matched(pstExtra->ubaMacAddr, pstHdr->ubaClientMacAddr))
            goto __lblRecv;         
    }
    else
    {
        if (nRcvBytes < 0)
            onps_get_last_error(nInput, penErr);
        else
            goto __lblRecv; 
    }
    //* ==========================================================================================================    

    return nRcvBytes; 
}

//* 分析dhcp选项，截取dhcp报文类型
UCHAR *dhcp_get_option(UCHAR *pubOptions, USHORT usOptionsLen, UCHAR ubOptionCode)
{
    PST_DHCPOPT_HDR pstOptHdr = (PST_DHCPOPT_HDR)pubOptions; 
    USHORT usParseBytes = 0; 
    while (usParseBytes < usOptionsLen)
    {
        if (ubOptionCode == pstOptHdr->ubOption)
            return (UCHAR *)pstOptHdr; 

        usParseBytes += (USHORT)pstOptHdr->ubLen + (USHORT)sizeof(ST_DHCPOPT_HDR); 
        pstOptHdr = (PST_DHCPOPT_HDR)(pubOptions + usParseBytes); 
    }

    return NULL; 
}

//* dhcp客户端启动
static INT dhcp_client_start(EN_ONPSERR *penErr)
{
#if SUPPORT_IPV6
    INT nInput = onps_input_new(AF_INET, IPPROTO_UDP, penErr);
#else
	INT nInput = onps_input_new(IPPROTO_UDP, penErr);
#endif
    if (nInput < 0)
        return nInput; 

    //* 首先看看指定的端口是否已被使用
#if SUPPORT_IPV6
	if (onps_input_port_used(AF_INET, IPPROTO_UDP, DHCP_CLT_PORT))
#else
    if (onps_input_port_used(IPPROTO_UDP, DHCP_CLT_PORT))
#endif
    {
        if(penErr)
            *penErr = ERRPORTOCCUPIED;
        goto __lblErr;
    }

    //* 设置地址
    ST_TCPUDP_HANDLE stHandle;    
    stHandle.stSockAddr.saddr_ipv4 = 0; //* 作为udp服务器启动，不绑定任何地址，当然也无法绑定因为还没获得合法ip地址
    stHandle.stSockAddr.usPort = DHCP_CLT_PORT;
    if (onps_input_set(nInput, IOPT_SETTCPUDPADDR, &stHandle, penErr))
        return nInput; 

__lblErr: 
    onps_input_free(nInput);
    return -1; 
}

static void dhcp_client_stop(INT nInput)
{
    onps_input_free(nInput); 
}

//* dhcp服务发现
static BOOL dhcp_discover(INT nInput, PST_NETIF pstNetif, UINT unTransId, in_addr_t *punOfferIp, in_addr_t *punSrvIp, EN_ONPSERR *penErr)
{
    //* ethernet网卡的附加信息
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
    UINT unOptionsOffset = 0; 

    //* dhcp选项分配一块填充缓冲区并清零
    UCHAR ubaOptions[DHCP_OPTIONS_LEN_MIN]; 
    memset(ubaOptions, 0, sizeof(ubaOptions));     

    //* 申请一个接收缓冲区
    UCHAR *pubRcvBuf = (UCHAR *)buddy_alloc(512, penErr);
    if (!pubRcvBuf)            
        return FALSE;     

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

    //* 发送并等待接收应答
    //* ================================================================================================
    BOOL blRtnVal = FALSE; 
    UINT unDelaySecs = 1;
    CHAR bRetryNum = 0;
    INT nRcvedBytes; 

__lblSend: 
    if (bRetryNum++ >= 6)
        goto __lblEnd; 

    nRcvedBytes = dhcp_send_and_wait_ack(nInput, pstNetif, DHCP_OPT_REQUEST, ubaOptions, sizeof(ubaOptions), unTransId, 0, 0xFFFFFFFF, pubRcvBuf, 512, penErr);
    if (nRcvedBytes > 0)
    {        
        do {
            //* 解析应答报文取出offer相关信息
            PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubRcvBuf;
            UCHAR *pubOptions = pubRcvBuf + sizeof(ST_DHCP_HDR);
            USHORT usOptionsLen = (USHORT)(nRcvedBytes - sizeof(ST_DHCP_HDR));            
            PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_MSGTYPE);
            if (!pstMsgType || DHCPMSGTP_OFFER != pstMsgType->ubTpVal) //* 必须携带dhcp报文类型并且一定是offer报文才可以,如果不是则认为超时，重新发送
            {
                *penErr = ERRWAITACKTIMEOUT;                 
                break;
            }

            //* 取出回馈offer报文的dhcp服务器identifier，如果无法取出dhcp服务器的identifier，给个超时认定以便重新发送
            PST_DHCPOPT_SRVID pstSrvId = (PST_DHCPOPT_SRVID)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_SRVID); 
            if (!pstSrvId)
            {
                *penErr = ERRWAITACKTIMEOUT;                 
                break;
            }

            //* 取出分配的ip地址及dhcp服务器的ip地址
            *punOfferIp = pstHdr->unYourIp; 
            *punSrvIp = pstSrvId->unSrvIp; 

            blRtnVal = TRUE; 
        } while (FALSE);         
    }

    if (!blRtnVal)
    {
        if (ERRWAITACKTIMEOUT == *penErr)
        {
            //* 未发现dhcp服务器，重新发送报文
            if (unDelaySecs >= 16)
                unDelaySecs = 16;
            else
                unDelaySecs *= 2;
            os_sleep_secs(unDelaySecs);

            goto __lblSend;
        }
    }
    //* ================================================================================================

__lblEnd: 
    //* 释放申请的缓冲区
    buddy_free(pubRcvBuf);

    return blRtnVal;
}

static BOOL dhcp_request(INT nInput, PST_NETIF pstNetif, UINT unTransId, in_addr_t unOfferIp, in_addr_t unSrvIp, PST_IPV4 pstIPv4, UINT *punLeaseTime, EN_ONPSERR *penErr)
{
    //* ethernet网卡的附加信息
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 
    UINT unOptionsOffset = 0; 

    //* dhcp选项分配一块填充缓冲区并清零
    UCHAR ubaOptions[DHCP_OPTIONS_LEN_MIN];
    memset(ubaOptions, 0, sizeof(ubaOptions));

    //* 申请一个接收缓冲区
    UCHAR *pubRcvBuf = (UCHAR *)buddy_alloc(512, penErr);
    if (!pubRcvBuf)
        return FALSE;

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
    pstReqIp->unVal = unOfferIp; 
    unOptionsOffset += sizeof(ST_DHCPOPT_REQIP);

    //* 填充dhcp服务器identifier
    PST_DHCPOPT_SRVID pstSrvId = (PST_DHCPOPT_SRVID)&ubaOptions[unOptionsOffset];
    pstSrvId->stHdr.ubOption = DHCPOPT_SRVID; 
    pstSrvId->stHdr.ubLen = sizeof(ST_DHCPOPT_SRVID) - sizeof(ST_DHCPOPT_HDR); 
    pstSrvId->unSrvIp = unSrvIp; 
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

    //* 发送并等待接收应答
    //* ================================================================================================
    BOOL blRtnVal = FALSE;    
    CHAR bRetryNum = 0;
    INT nRcvedBytes;

__lblSend:
    if (bRetryNum++ >= 4)
        goto __lblEnd; 

    nRcvedBytes = dhcp_send_and_wait_ack(nInput, pstNetif, DHCP_OPT_REQUEST, ubaOptions, sizeof(ubaOptions), unTransId, 0, 0xFFFFFFFF, pubRcvBuf, 512, penErr);
    if (nRcvedBytes > 0)
    {
        do {
            //* 解析应答报文取出具体的选项相关信息
            PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubRcvBuf;
            UCHAR *pubOptions = pubRcvBuf + sizeof(ST_DHCP_HDR);
            USHORT usOptionsLen = (USHORT)(nRcvedBytes - sizeof(ST_DHCP_HDR));
            PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_MSGTYPE);
            if (!pstMsgType || (DHCPMSGTP_ACK != pstMsgType->ubTpVal && DHCPMSGTP_NAK != pstMsgType->ubTpVal)) //* 必须携带dhcp报文类型并且一定是ack/nack报文才可以，如果不是则认为超时，重新发送
            {                
                *penErr = ERRWAITACKTIMEOUT;                
                break;
            }

            //* 说明地址冲突，需要重新请求
            if (DHCPMSGTP_NAK == pstMsgType->ubTpVal)
            {
                *penErr = ERRIPCONFLICT; 
                break; 
            }

            pstIPv4->unAddr = pstHdr->unYourIp; 

            //* 取出子网掩码
            PST_DHCPOPT_SUBNETMASK pstNetmask = (PST_DHCPOPT_SUBNETMASK)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_SUBNETMASK);
            if (!pstNetmask)
            {
                *penErr = ERRWAITACKTIMEOUT;                
                break;
            }
            pstIPv4->unSubnetMask = pstNetmask->unVal; 

            //* 取出网关地址
            PST_DHCPOPT_ROUTER pstRouter = (PST_DHCPOPT_ROUTER)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_ROUTER); 
            if (!pstRouter)
            {
                *penErr = ERRWAITACKTIMEOUT;                
                break;
            }
            pstIPv4->unGateway = pstRouter->unVal; 

            //* 取出dns服务器地址
            PST_DHCPOPT_DNS pstDns = (PST_DHCPOPT_DNS)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_DNS); 
            if (!pstDns)
            {
                *penErr = ERRWAITACKTIMEOUT;                
                break; 
            }			
            pstIPv4->unPrimaryDNS = pstDns->unPrimary; 
			if(pstDns->stHdr.ubLen >= 8)
				pstIPv4->unSecondaryDNS = pstDns->unSecondary; 
			else
				pstIPv4->unSecondaryDNS = 0; 

            //* 取出租约信息
            PST_DHCPOPT_LEASETIME pstLeaseTime = (PST_DHCPOPT_LEASETIME)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_LEASETIME); 
            if (!pstLeaseTime)
            {
                *penErr = ERRWAITACKTIMEOUT;                
                break; 
            }
            *punLeaseTime = htonl(pstLeaseTime->unVal); 

            blRtnVal = TRUE;
        } while (FALSE);
    }

    if (!blRtnVal)
    {
        if (ERRWAITACKTIMEOUT == *penErr)
        {            
            os_sleep_secs(1); 
            goto __lblSend;
        }
    }
    //* ================================================================================================

__lblEnd:
    //* 释放申请的缓冲区
    buddy_free(pubRcvBuf);

    return blRtnVal;
}

void dhcp_decline(INT nInput, PST_NETIF pstNetif, UINT unTransId, in_addr_t unDeclineIp, in_addr_t unSrvIp)
{
    //* ethernet网卡的附加信息
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
    UINT unOptionsOffset = 0;

    //* dhcp选项分配一块填充缓冲区并清零
    UCHAR ubaOptions[DHCP_OPTIONS_LEN_MIN];
    memset(ubaOptions, 0, sizeof(ubaOptions));

    //* 填充消息类型
    PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)ubaOptions;
    pstMsgType->stHdr.ubOption = DHCPOPT_MSGTYPE;
    pstMsgType->stHdr.ubLen = 1;
    pstMsgType->ubTpVal = DHCPMSGTP_DECLINE;
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
    pstReqIp->unVal = unDeclineIp;
    unOptionsOffset += sizeof(ST_DHCPOPT_REQIP);

    //* 填充dhcp服务器identifier
    PST_DHCPOPT_SRVID pstSrvId = (PST_DHCPOPT_SRVID)&ubaOptions[unOptionsOffset];
    pstSrvId->stHdr.ubOption = DHCPOPT_SRVID;
    pstSrvId->stHdr.ubLen = sizeof(ST_DHCPOPT_SRVID) - sizeof(ST_DHCPOPT_HDR);
    pstSrvId->unSrvIp = unSrvIp;
    unOptionsOffset += sizeof(ST_DHCPOPT_SRVID); 

    //* 选项结束
    ubaOptions[unOptionsOffset] = DHCPOPT_END; 

    dhcp_send_packet(nInput, pstNetif, DHCP_OPT_REQUEST, ubaOptions, sizeof(ubaOptions), unTransId, unDeclineIp, 0xFFFFFFFF, 0, NULL);	
}

static void dhcp_release(INT nInput, PST_NETIF pstNetif, UINT unTransId, in_addr_t unReleasedIp, in_addr_t unSrvIp)
{
    //* ethernet网卡的附加信息
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
    UINT unOptionsOffset = 0;

    //* dhcp选项分配一块填充缓冲区并清零
    UCHAR ubaOptions[DHCP_OPTIONS_LEN_MIN];
    memset(ubaOptions, 0, sizeof(ubaOptions)); 

    //* 填充消息类型
    PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)ubaOptions;
    pstMsgType->stHdr.ubOption = DHCPOPT_MSGTYPE;
    pstMsgType->stHdr.ubLen = 1;
    pstMsgType->ubTpVal = DHCPMSGTP_RELEASE;
    unOptionsOffset += sizeof(ST_DHCPOPT_MSGTYPE); 

    //* 填充dhcp服务器identifier
    PST_DHCPOPT_SRVID pstSrvId = (PST_DHCPOPT_SRVID)&ubaOptions[unOptionsOffset];
    pstSrvId->stHdr.ubOption = DHCPOPT_SRVID;
    pstSrvId->stHdr.ubLen = sizeof(ST_DHCPOPT_SRVID) - sizeof(ST_DHCPOPT_HDR);
    pstSrvId->unSrvIp = unSrvIp;
    unOptionsOffset += sizeof(ST_DHCPOPT_SRVID);

    //* 填充发起申请的客户端id    
    PST_DHCPOPT_CLTID pstCltId = (PST_DHCPOPT_CLTID)&ubaOptions[unOptionsOffset];
    pstCltId->stHdr.ubOption = DHCPOPT_CLIENTID;
    pstCltId->stHdr.ubLen = sizeof(ST_DHCPOPT_CLTID) - sizeof(ST_DHCPOPT_HDR);
    pstCltId->ubHardwareType = 1;
    memcpy(pstCltId->ubaMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN);
    unOptionsOffset += sizeof(ST_DHCPOPT_CLTID);

    //* 选项结束
    ubaOptions[unOptionsOffset] = DHCPOPT_END; 

    dhcp_send_packet(nInput, pstNetif, DHCP_OPT_REQUEST, ubaOptions, sizeof(ubaOptions), unTransId, unReleasedIp, unSrvIp, unReleasedIp, NULL);
}

//* 发送一个gratuitous arp request（免费arp请求），用于探测当前分配的ip地址是否已被使用
BOOL dhcp_ip_conflict_detect(PST_NETIF pstNetif, UINT unDetectedIp)
{
    UCHAR ubaDstMac[ETH_MAC_ADDR_LEN]; 
    INT nRtnVal;
    UCHAR ubRetryNum = 0; 

__lblDetect: 
    ubRetryNum++; 
    if (ubRetryNum > 5)
        return FALSE; //* 说明不存在

    nRtnVal = arp_get_mac(pstNetif, unDetectedIp, unDetectedIp, ubaDstMac, NULL); 
    if (!nRtnVal) //* 存在该条目，返回TRUE，通知dhcp客户端存在ip冲突
        return TRUE; 
    else
    {
        os_sleep_secs(1); 
        goto __lblDetect; 
    }
}

//* 发送续租报文
static BOOL dhcp_send_renewal(PSTCB_RENEWAL_INFO pstcbRenewalInfo, in_addr_t unDstIp)
{
    //* 重置事务id
    pstcbRenewalInfo->unTransId = (UINT)rand();    

    //* 填充dhcp续租选项
    //* ============================================================================================
    //* ethernet网卡的附加信息
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstcbRenewalInfo->pstNetif->pvExtra;
    UCHAR ubaOptions[DHCP_OPTIONS_LEN_MIN]; //* dhcp续租选项缓冲区
    UINT unOptionsOffset = 0;

    //* 清零dhcp续租选项缓冲区        
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
    //* ============================================================================================

    //* 发送
    if (dhcp_send_packet(pstcbRenewalInfo->nInput, pstcbRenewalInfo->pstNetif, DHCP_OPT_REQUEST, ubaOptions, sizeof(ubaOptions), pstcbRenewalInfo->unTransId, pstcbRenewalInfo->pstNetif->stIPv4.unAddr, unDstIp, pstcbRenewalInfo->pstNetif->stIPv4.unAddr, NULL) > 0)
        return TRUE;
    else
        return FALSE; 
}

//* 等待接收dhcp服务器回馈的续租应答报文
static INT dhcp_recv_renewal_ack(PSTCB_RENEWAL_INFO pstcbRenewalInfo)
{
    EN_ONPSERR enErr; 
    INT nRtnVal; 

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
        printf("dhcp_recv_renewal_ack() failed, the NIC name is %s, %s\r\n", pstcbRenewalInfo->pstNetif->szName, onps_error(enErr));
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
        //* 满足以上情形之一则认为当前收到的报文并不是“我”正在等待的报文，需要继续等待
        if (DHCP_MAGIC_COOKIE != htonl(pstHdr->unMagicCookie)
            || pstcbRenewalInfo->unTransId != htonl(pstHdr->unTransId)
            || pstHdr->ubHardwareAddrLen != ETH_MAC_ADDR_LEN
            || !ethernet_mac_matched(pstExtra->ubaMacAddr, pstHdr->ubaClientMacAddr))
        {
            nRtnVal = 1; 
            goto __lblEnd; 
        }            

        //* 请求的地址与本地地址必须匹配
        if (pstHdr->unClientIp != pstHdr->unYourIp)
        {
            nRtnVal = 1;
            goto __lblEnd;
        }
        
        //* 收到应答，取出需要的dhcp报文选项信息
        //* =============================================================================================        
        UCHAR *pubOptions = pubRcvBuf + sizeof(ST_DHCP_HDR);
        USHORT usOptionsLen = (USHORT)(nRcvedBytes - sizeof(ST_DHCP_HDR));

        //* 报文类型，必须携带dhcp报文类型并且一定是ack报文才可以,如果不是则需要继续等待
        PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_MSGTYPE);        
        if (!pstMsgType || DHCPMSGTP_ACK != pstMsgType->ubTpVal)
        {
            nRtnVal = 1;
            goto __lblEnd;
        }

        //* 取出租约信息
        PST_DHCPOPT_LEASETIME pstLeaseTime = (PST_DHCPOPT_LEASETIME)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_LEASETIME);
        if (!pstLeaseTime)
        {
            nRtnVal = 1;
            goto __lblEnd;
        }
        pstcbRenewalInfo->unLeaseTime = htonl(pstLeaseTime->unVal); 
        //* =============================================================================================

        nRtnVal = 0; 
    }

__lblEnd: 
    //* 释放刚才申请的接收缓冲区
    buddy_free(pubRcvBuf);

    return nRtnVal; 
}

void dhcp_timer_new(PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler, PSTCB_RENEWAL_INFO pstcbRenewalInfo, INT nTimeout)
{
    //* 失败的唯一原因就是定时器资源不够用，所以必须确保定时器资源足够使用，否则将影响协议栈dhcp客户端的正常运行
    if (!one_shot_timer_new(pfunTimeoutHandler, pstcbRenewalInfo, nTimeout))
    {
        dhcp_client_stop(pstcbRenewalInfo->nInput); //* 停止dhcp客户端，定时器资源一旦耗尽，客户端将无法继续正常运行，所以在这里直接停止

#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("dhcp_timer_new() failed, the NIC name is %s, %s\r\n", pstcbRenewalInfo->pstNetif->szName, onps_error(ERRNOIDLETIMER));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }
}

/*
static void renewal_timer_new(PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler, PSTCB_RENEWAL_INFO pstcbRenewalInfo, INT nTimeout)
{
    // 理论上不会失败，因为系统预先分配的定时器足够用，但世事无常，协议栈的目标应用环境极其复杂，一旦遇到系统预先分配的定时器资源耗尽的情形，dhcp客户端就无法完成正常的业务逻辑了
    // 此时只能停止当前dhcp客户端，不再进行续租工作，并且重新申请地址的工作也被迫中止，所以，必须确保定时器资源足够使用，否则将影响协议栈dhcp客户端的正常运行
    if (!one_shot_timer_new(dhcp_renewal_timeout_handler, pstcbRenewalInfo, nTimeout))
    {
        dhcp_client_stop(pstcbRenewalInfo->nInput);

#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("renewal_timer_new() failed, the NIC name is %s, %s\r\n", pstcbRenewalInfo->pstNetif->szName, onps_error(ERRNOIDLETIMER));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }
}
*/

//* 续租定时器
void dhcp_renewal_timeout_handler(void *pvParam)
{
    INT nTimeout, nRtnVal; 

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    CHAR szAddr[20];
#endif

    PSTCB_RENEWAL_INFO pstcbRenewalInfo = (PSTCB_RENEWAL_INFO)pvParam; 
    switch (pstcbRenewalInfo->bState)
    {
    case 0: //* 单播发送续租请求（1/2租期）        
        if (dhcp_send_renewal(pstcbRenewalInfo, pstcbRenewalInfo->unDhcpSrvIp))
        {
            nTimeout = 1;

            //* 发送成功，等待应答的开始时间清零，并将状态机迁移到等待应答报文到达的阶段
            pstcbRenewalInfo->bWaitAckSecs = 0;
            pstcbRenewalInfo->bState = 1;
        }
        else 
        {
            //* 报文发送失败，这里就不再尝试重新发送了，直接进入下一个阶段——发送广播续租报文，根据dhcp协议广播续租报文的启动时间
            //* 应为租期的7/8，扣除已经消逝的4/8，则只需再等待3/8租期就该发送广播报文进行续租了
            nTimeout = (pstcbRenewalInfo->unLeaseTime * 3) / 8; 
            pstcbRenewalInfo->bState = 2;  

			//os_thread_mutex_lock(o_hMtxPrintf);
			//printf("<0> dhcp_send_renewal() failed\r\n");
			//os_thread_mutex_unlock(o_hMtxPrintf);
        }        
        dhcp_timer_new(dhcp_renewal_timeout_handler, pstcbRenewalInfo, nTimeout);

        break;

    case 1: //* 等待单播续租请求的应答报文
		nRtnVal = dhcp_recv_renewal_ack(pstcbRenewalInfo); 
        if (0 == nRtnVal)
        {
#if 0
			//* 未收到续租应答，那就再过一段时间（7/8租期）发送广播续租报文            
			nTimeout = (INT)((pstcbRenewalInfo->unLeaseTime * 3) / 8);
			pstcbRenewalInfo->bState = 2;

#else
    #if SUPPORT_PRINTF && DEBUG_LEVEL > 1
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("<0> The ip address %s of the NIC %s has been successfully renewed, The lease period is %d seconds.\r\n", inet_ntoa_safe_ext(pstcbRenewalInfo->pstNetif->stIPv4.unAddr, szAddr), pstcbRenewalInfo->pstNetif->szName, pstcbRenewalInfo->unLeaseTime);
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif            

            //* 收到应答，则回到单播报文续租阶段
            nTimeout = (INT)(pstcbRenewalInfo->unLeaseTime / 2);                 
            pstcbRenewalInfo->bState = 0;
#endif
        }
        else
        {
			//* 尚未超时，继续等待
			if (nRtnVal > 0)
				nTimeout = 1; 
			else
			{
				//* 未收到续租应答，那就再过一段时间（7/8租期）发送广播续租报文            
				nTimeout = (INT)((pstcbRenewalInfo->unLeaseTime * 3) / 8);
				pstcbRenewalInfo->bState = 2;

				//os_thread_mutex_lock(o_hMtxPrintf);
				//printf("<1> dhcp_recv_renewal_ack() failed\r\n"); 
				//os_thread_mutex_unlock(o_hMtxPrintf);
			}            
        }
        //* 重启续租定时器
        dhcp_timer_new(dhcp_renewal_timeout_handler, pstcbRenewalInfo, nTimeout);
        break;    

    case 2: //* 单播续租未成功，需要广播发送续租请求（7/8租期）
        if (dhcp_send_renewal(pstcbRenewalInfo, 0xFFFFFFFF))
        {
            nTimeout = 1;

            //* 发送成功，等待应答的开始时间清零，并将状态机迁移到等待应答报文到达的阶段
            pstcbRenewalInfo->bWaitAckSecs = 0;
            pstcbRenewalInfo->bState = 3;
        }
        else
        {
            //* 报文发送失败，这里就不再尝试重新发送了，直接进入下一个阶段——等待租期到达释放当前ip地址并开启新的ip地址租用流程
            nTimeout = (pstcbRenewalInfo->unLeaseTime * 1) / 8;
            pstcbRenewalInfo->bState = 4;

			//os_thread_mutex_lock(o_hMtxPrintf);
			//printf("<2> dhcp_send_renewal() failed\r\n");
			//os_thread_mutex_unlock(o_hMtxPrintf);
        }  
        //* 重启续租定时器
        dhcp_timer_new(dhcp_renewal_timeout_handler, pstcbRenewalInfo, nTimeout);
        break;

    case 3: //* 等待广播续租请求的应答报文
		nRtnVal = dhcp_recv_renewal_ack(pstcbRenewalInfo); 
        if (0 == nRtnVal)
        {
#if 0
			//* 未收到续租应答，只好进入下一个阶段——等待租期到达释放当前ip地址并开启新的ip地址租用流程
			nTimeout = (pstcbRenewalInfo->unLeaseTime * 1) / 8;
			pstcbRenewalInfo->bState = 4; 

#else
    #if SUPPORT_PRINTF && DEBUG_LEVEL > 1     
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("<1> The ip address %s of the NIC %s has been successfully renewed, The lease period is %d seconds.\r\n", inet_ntoa_safe_ext(pstcbRenewalInfo->pstNetif->stIPv4.unAddr, szAddr), pstcbRenewalInfo->pstNetif->szName, pstcbRenewalInfo->unLeaseTime); 
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif            

            //* 收到应答，则重新回到单播报文续租阶段
            nTimeout = (INT)(pstcbRenewalInfo->unLeaseTime / 2);            
            pstcbRenewalInfo->bState = 0;
#endif
        }
        else
        {
			//* 尚未超时，继续等待
			if (nRtnVal > 0)
				nTimeout = 1;
			else
			{
				//* 未收到续租应答，只好进入下一个阶段——等待租期到达释放当前ip地址并开启新的ip地址租用流程
				nTimeout = (pstcbRenewalInfo->unLeaseTime * 1) / 8;
				pstcbRenewalInfo->bState = 4;

				//os_thread_mutex_lock(o_hMtxPrintf);
				//printf("<3> dhcp_recv_renewal_ack() failed\r\n");
				//os_thread_mutex_unlock(o_hMtxPrintf);
			}            
        }
        //* 重启续租定时器
        dhcp_timer_new(dhcp_renewal_timeout_handler, pstcbRenewalInfo, nTimeout);
        break;

    case 4: //* 续租未成功，等待租期结束后释放当前ip地址并启动再次申请流程
        dhcp_release(pstcbRenewalInfo->nInput, pstcbRenewalInfo->pstNetif, (UINT)rand(), pstcbRenewalInfo->pstNetif->stIPv4.unAddr, pstcbRenewalInfo->unDhcpSrvIp);        

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("The ip address %s of the NIC %s has been released.\r\n", inet_ntoa_safe_ext(pstcbRenewalInfo->pstNetif->stIPv4.unAddr, szAddr), pstcbRenewalInfo->pstNetif->szName);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif

        //* 重新开启ip地址申请流程
        pstcbRenewalInfo->bState = 0; 
        dhcp_timer_new(dhcp_req_addr_timeout_handler, pstcbRenewalInfo, 3);         
       
        break;     
    }
}

BOOL dhcp_req_addr(PST_NETIF pstNetif, EN_ONPSERR *penErr)
{
    EN_ONPSERR enErr; 

    //* 启动dhcp客户端（其实就是作为一个udp服务器启动）
    INT nInput = dhcp_client_start(penErr); 
    if (nInput < 0)
        return FALSE; 

    //* 开启dhcp客户端申请分配一个合法ip地址的逻辑：discover->offer->request->ack
    //* ==================================================================================
    in_addr_t unOfferIp, unSrvIp;
    ST_IPV4 stIPv4; 
    UINT unLeaseTime; 
    UINT unTransId; 
    do {        
    __lblDiscover: 
        unTransId = (UINT)rand();
        if (!dhcp_discover(nInput, pstNetif, unTransId, &unOfferIp, &unSrvIp, &enErr))
            break; 

        //* 发送request请求报文        
        if (!dhcp_request(nInput, pstNetif, unTransId, unOfferIp, unSrvIp, &stIPv4, &unLeaseTime, &enErr))
        {
            //* 收到nak则意味着ip地址冲突需要重新申请
            if (ERRIPCONFLICT != enErr)
                break;
			else            
				goto __lblDiscover; 			
        }

        //* 确定ip地址可用，采用arp探测的方式
        if (!dhcp_ip_conflict_detect(pstNetif, stIPv4.unAddr))
        {            
            pstNetif->stIPv4 = stIPv4; 
            if (route_add(pstNetif, 0, stIPv4.unGateway, stIPv4.unSubnetMask, &enErr))
            {
        #if SUPPORT_PRINTF && DEBUG_LEVEL > 1
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf); 
            #endif
                printf("          ip addr %s\r\n", inet_ntoa_ext(stIPv4.unAddr));
                printf("     sub net mask %s\r\n", inet_ntoa_ext(stIPv4.unSubnetMask));
                printf("          gateway %s\r\n", inet_ntoa_ext(stIPv4.unGateway));

                printf("  primary dns srv %s\r\n", inet_ntoa_ext(stIPv4.unPrimaryDNS));
                printf("secondary dns srv %s\r\n", inet_ntoa_ext(stIPv4.unSecondaryDNS));
                printf("       lease time %d\r\n", unLeaseTime); 
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf); 
            #endif
        #endif
            }
            else
            {
        #if SUPPORT_PRINTF && DEBUG_LEVEL
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif
                printf("route_add() failed, the NIC name is %s, %s\r\n", pstNetif->szName, onps_error(enErr)); 
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
        #endif
                break; 
            }

            //* 根据租约信息，启动一个续租定时器
            l_stcbRenewalInfo.nInput = nInput;
            l_stcbRenewalInfo.pstNetif = pstNetif;
            l_stcbRenewalInfo.unLeaseTime = unLeaseTime;             
            l_stcbRenewalInfo.unDhcpSrvIp = unSrvIp; 
            l_stcbRenewalInfo.bState = 0;  //* 初始阶段为发送续租报文
            dhcp_timer_new(dhcp_renewal_timeout_handler, &l_stcbRenewalInfo, unLeaseTime / 2); 
            /*
            if (!one_shot_timer_new(dhcp_renewal_timeout_handler, &l_stcbRenewalInfo, unLeaseTime / 2))
            {
                dhcp_client_stop(nInput); // 这时停止就可以了，不再需要这个资源了

        #if SUPPORT_PRINTF && DEBUG_LEVEL
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif
                printf("one_shot_timer_new() failed, the NIC name is %s, %s\r\n", pstNetif->szName, onps_error(ERRNOIDLETIMER)); 
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
        #endif
            }
            */
        }
        else
        {
            //* 通知dhcp服务器当前ip地址冲突，需要重新分配一个
            dhcp_decline(nInput, pstNetif, unTransId, stIPv4.unAddr, unSrvIp);
            os_sleep_secs(3); //* 休眠一小段时间，给dhcp服务器留出处理时间
            goto __lblDiscover; 
        }
        
        //dhcp_client_stop(nInput);
        return TRUE;
    } while (FALSE);         
    //* ==================================================================================

    //* 结束dhcp客户端
    dhcp_client_stop(nInput);

    if (penErr)
        *penErr = enErr;
    return FALSE; 
}

/*
printf("          ip addr %s\r\n", inet_ntoa_ext(stIPv4.unAddr));
printf("     sub net mask %s\r\n", inet_ntoa_ext(stIPv4.unSubnetMask));
printf("          gateway %s\r\n", inet_ntoa_ext(stIPv4.unGateway));

printf("  primary dns srv %s\r\n", inet_ntoa_ext(stIPv4.unPrimaryDNS));
printf("secondary dns srv %s\r\n", inet_ntoa_ext(stIPv4.unSecondaryDNS));
*/

#endif
