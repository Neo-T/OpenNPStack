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

static INT dhcp_send_packet(INT nInput, PST_NETIF pstNetif, UCHAR ubOptCode, UCHAR *pubOptions, UCHAR ubOptionsLen, UINT unTransId, in_addr_t unClientIp, in_addr_t unDstIP, EN_ONPSERR *penErr)
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

    //* 发送之，源地址固定为0
    INT nRtnVal = udp_send_ext(nInput, sBufListHead, unDstIP, DHCP_SRV_PORT, 0, pstNetif, penErr); 

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
    INT nRtnVal = dhcp_send_packet(nInput, pstNetif, ubOptCode, pubOptions, ubOptionsLen, unTransId, unClientIp, unDstIP, penErr); 
    if (nRtnVal <= 0)
        return nRtnVal; 
    
    //* 等待应答到来
    INT nRcvBytes = udp_recv_upper(nInput, pubRcvBuf, (UINT)usRcvBufSize, NULL, NULL, 3); 
    if (nRcvBytes > 0)
    {
        PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
        PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubRcvBuf; 

        //* 1）如果不是dhcp报文则判定为不是合法的应答报文；
        //* 2）当前请求的事务id不匹配； 
        //* 3）客户端mac地址也要匹配目前只支持ethernet网卡的mac地址类型
        //* 满足以上情形之一即直接认定为应答超时
        if (DHCP_MAGIC_COOKIE != htonl(pstHdr->unMagicCookie)
            || unTransId != htonl(pstHdr->unTransId)
            || pstHdr->ubHardwareAddrLen != ETH_MAC_ADDR_LEN
            || !ethernet_mac_matched(pstExtra->ubaMacAddr, pstHdr->ubaClientMacAddr))
        {
            if (penErr)
                *penErr = ERRWAITACKTIMEOUT;
            nRcvBytes = 0;
        }        
    }
    else
    {
        if(nRcvBytes < 0)
            onps_get_last_error(nInput, penErr);
        else
        {
            if (penErr)
                *penErr = ERRWAITACKTIMEOUT;
        }        
    }

    return nRcvBytes; 
}

//* 分析dhcp选项，截取dhcp报文类型
static UCHAR *dhcp_get_option(UCHAR *pubOptions, USHORT usOptionsLen, UCHAR ubOptionCode)
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
    if (bRetryNum++ >= 4)
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
            //* 解析应答报文取出offer相关信息
            PST_DHCP_HDR pstHdr = (PST_DHCP_HDR)pubRcvBuf;
            UCHAR *pubOptions = pubRcvBuf + sizeof(ST_DHCP_HDR);
            USHORT usOptionsLen = (USHORT)(nRcvedBytes - sizeof(ST_DHCP_HDR));
            PST_DHCPOPT_MSGTYPE pstMsgType = (PST_DHCPOPT_MSGTYPE)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_MSGTYPE);
            if (!pstMsgType || DHCPMSGTP_ACK != pstMsgType->ubTpVal) //* 必须携带dhcp报文类型并且一定是ack报文才可以,如果不是则认为超时，重新发送
            {
                *penErr = ERRWAITACKTIMEOUT;
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
            pstIPv4->unSecondaryDNS = pstDns->unSecondary; 

            //* 取出租约信息
            PST_DHCPOPT_LEASETIME pstLeaseTime = (PST_DHCPOPT_LEASETIME)dhcp_get_option(pubOptions, usOptionsLen, DHCPOPT_LEASETIME); 
            if (!pstLeaseTime)
            {
                *penErr = ERRWAITACKTIMEOUT;
                break; 
            }
            *punLeaseTime = pstLeaseTime->unVal; 

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

BOOL dhcp_req_addr(PST_NETIF pstNetif, EN_ONPSERR *penErr)
{
    EN_ONPSERR enErr; 

    //* 启动dhcp客户端（其实就是作为一个udp服务器启动）
    INT nInput = dhcp_client_start(penErr); 
    if (nInput < 0)
        return FALSE;             

    UINT unTransId = (UINT)rand(); 

    //* 开启dhcp客户端申请分配一个合法ip地址的逻辑：discover->offer->request->ack
    //* ==================================================================================
    do {
        in_addr_t unOfferIp, unSrvIp;
        if (!dhcp_discover(nInput, pstNetif, unTransId, &unOfferIp, &unSrvIp, &enErr))
            break; 

        //* 发送request请求报文
        ST_IPV4 stIPv4; 
        UINT unLeaseTime;         
        if (!dhcp_request(nInput, pstNetif, unTransId, unOfferIp, unSrvIp, &stIPv4, &unLeaseTime, &enErr))
            break; 
                
        printf("          ip addr %s\r\n", inet_ntoa_ext(stIPv4.unAddr)); 
        printf("     sub net mask %s\r\n", inet_ntoa_ext(stIPv4.unSubnetMask)); 
        printf("          gateway %s\r\n", inet_ntoa_ext(stIPv4.unGateway)); 

        printf("  primary dns srv %s\r\n", inet_ntoa_ext(stIPv4.unPrimaryDNS)); 
        printf("secondary dns srv %s\r\n", inet_ntoa_ext(stIPv4.unSecondaryDNS)); 

        //* 结束请求
        dhcp_client_stop(nInput);
        return TRUE;
    } while (FALSE);         
    //* ==================================================================================

    dhcp_client_stop(nInput);

    if (penErr)
        *penErr = enErr;
    return FALSE; 
}

#endif
