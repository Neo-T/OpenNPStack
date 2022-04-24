#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "onps_input.h"
#define SYMBOL_GLOBALS
#include "ip/icmp.h"
#undef SYMBOL_GLOBALS

ST_ICMP_REPORT_RESULT o_stLastReportResult = { ICMP_MAX };
static const CHAR *get_icmp_errdest_description(EN_ERRDST enCode)
{
    switch (enCode)
    {
    case NET_UNREACHABLE: 
        return "Network unreachable"; 

    case HOST_UNREACHABLE: 
        return "Host unreachable"; 

    case PROTO_UNREACHABLE: 
        return "Protocol unreachable"; 

    case PORT_UNREACHABLE: 
        return "Port unreachable"; 

    case NO_FRAGMENT: 
        return "Fragmentation needed but no frag. bit set"; 

    case SRCROUTE_FAILED:
        return "Source routing failed"; 

    case DSTNET_UNKNOWN: 
        return "Destination network unknown"; 

    case DSTHOST_UNKNOWN: 
        return "Destination host unknown"; 

    case SRCHOST_ISOLATED: 
        return "Source host isolated (obsolete)"; 

    case DSTNET_PROHIBITED: 
        return "Destination network administratively prohibited"; 

    case DSTHOST_PROHIBITED: 
        return "Destination host administratively prohibited"; 

    case TOS_NETUNREACHABLE: 
        return "Network unreachable for TOS"; 

    case TOS_HOST_UNREACHABLE: 
        return "Host unreachable for TOS"; 

    case COMMU_PROHIBITED: 
        return "Communication administratively prohibited by filtering"; 

    case HOST_PRECE_VIOLATION: 
        return "Host precedence violation"; 

    case PRECE_CUTOFF_EFFECT: 
        return "Precedence cutoff in effect"; 

    default:
        return "Unrecognized"; 
    }
}

static const CHAR *get_icmp_errredirect_description(EN_ERRREDIRECT enCode)
{
    switch (enCode)
    {
    case NET_REDIRECT: 
        return "Redirect for network"; 

    case HOST_REDIRECT:
        return "Redirect for host"; 

    case TOSNET_REDIRECT: 
        return "Redirect for TOS and network"; 

    case TOSHOST_REDIRECT: 
        return "Redirect for TOS and host"; 

    default:
        return "Unrecognized";
    }
}

const CHAR *icmp_get_description(UCHAR ubType, UCHAR ubCode)
{
    switch ((EN_ICMPTYPE)ubType)
    {
    case ICMP_ERRDST:
        return get_icmp_errdest_description((EN_ERRDST)ubCode);

    case ICMP_ERRSRC:
        return "Source quench"; 

    case ICMP_ERRREDIRECT:
        return get_icmp_errredirect_description((EN_ERRREDIRECT)ubCode); 

    case ICMP_ROUTEADVERT:
        return "Router advertisement"; 

    case ICMP_ROUTESOLIC:
        return "Router solicitation"; 

    case ICMP_ERRTTL:
        if (ubCode)
            return "TTL equals 0 during reassembly";
        else
            return "TTL equals 0 during transit"; 

    case ICMP_ERRIP:
        if (ubCode)
            return "Required options missing";
        else
            return "IP header bad (catch all error)"; 

    case ICMP_MAX:
        return "no icmp report"; 

    default:
        return ""; 
    }
}

static INT icmp_send(in_addr_t unDstAddr, EN_ICMPTYPE enType, UCHAR ubCode, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr)
{
    //* 填充头部字段
    ST_ICMP_HDR stHdr; 
    stHdr.ubType = (UCHAR)enType; 
    stHdr.ubCode = ubCode;   
    stHdr.usChecksum = 0;  //* 该字段必须为0，因为校验和计算范围覆盖该字段，其初始值必须为0才可
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (USHORT)sizeof(ST_ICMP_HDR), penErr);
    if (sHdrNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 计算校验和
    stHdr.usChecksum = tcpip_checksum_ext(sBufListHead); 

    //* 完成发送
    INT nRtnVal = ip_send(unDstAddr, ICMP, ubTTL, sBufListHead, penErr);

    //* 释放刚才申请的buf list节点
    buf_list_free(sHdrNode);

    return nRtnVal; 
}

INT icmp_send_echo_reqest(INT nInput, USHORT usIdentifier, USHORT usSeqNum, UCHAR ubTTL, UINT unDstAddr, UCHAR *pubData, UINT unDataSize, EN_ONPSERR *penErr)
{
    //* 申请一个buf list节点
    SHORT sBufListHead = -1;
    SHORT sDataNode = buf_list_get_ext(pubData, (USHORT)unDataSize, penErr);
    if (sDataNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sDataNode);

    ST_ICMP_ECHO_HDR stEchoHdr; 
    stEchoHdr.usIdentifier = usIdentifier; 
    stEchoHdr.usSeqNum = usSeqNum; 
    //* 挂载到buf list头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stEchoHdr, (USHORT)sizeof(ST_ICMP_ECHO_HDR), penErr);
    if (sHdrNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sHdrNode);    

    //* 记录echo identifier，以便区分echo应答报文
    INT nRtnVal;    
    if (onps_input_set(nInput, IOPT_SETICMPECHOID, &usIdentifier, penErr))    
        nRtnVal = icmp_send(unDstAddr, ICMP_ECHOREQ, 0, ubTTL, sBufListHead, penErr);     
    else
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("onps_input_set() failed (the option is IOPT_SETICMPECHOID), %s\r\n", onps_error(*penErr)); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }

    //* 释放刚才申请的buf list节点
    buf_list_free(sDataNode);
    buf_list_free(sHdrNode);

    return nRtnVal; 
}

static void icmp_send_echo_reply(UCHAR *pubPacket, INT nPacketLen)
{
    EN_ONPSERR enErr; 
    PST_IP_HDR pstReqIpHdr = (PST_IP_HDR)pubPacket;
    UCHAR usIpHdrLen = pstReqIpHdr->bitHdrLen * 4; 
    PST_ICMP_ECHO_HDR pstReqEchoHdr = (PST_ICMP_ECHO_HDR)(pubPacket + usIpHdrLen + sizeof(ST_ICMP_HDR));

    //* 封装echo reply报文
    UCHAR ubData[100];
    PST_ICMP_ECHO_HDR pstRepRchoHdr = (PST_ICMP_ECHO_HDR)ubData; 
    pstRepRchoHdr->usIdentifier = pstReqEchoHdr->usIdentifier; 
    pstRepRchoHdr->usSeqNum = pstReqEchoHdr->usSeqNum; 
    USHORT usEchoDataLen = pstReqIpHdr->usPacketLen - usIpHdrLen - sizeof(ST_ICMP_HDR) - sizeof(ST_ICMP_ECHO_HDR);
    USHORT usCpyBytes = usEchoDataLen < sizeof(ubData) - sizeof(ST_ICMP_ECHO_HDR) ? usEchoDataLen : sizeof(ubData) - sizeof(ST_ICMP_ECHO_HDR); 
    memcpy(&ubData[sizeof(ST_ICMP_ECHO_HDR)], pubPacket + usIpHdrLen + sizeof(ST_ICMP_HDR) + sizeof(ST_ICMP_ECHO_HDR), usCpyBytes); 

    //* 申请一个buf list节点
    SHORT sBufListHead = -1;
    SHORT sDataNode = buf_list_get_ext(ubData, (USHORT)(usCpyBytes + sizeof(ST_ICMP_ECHO_HDR)), &enErr);
    if (sDataNode < 0)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("icmp_send_echo_reply() failed, %s\r\n", onps_error(enErr)); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif

        return; 
    }
    buf_list_put_head(&sBufListHead, sDataNode); 

    //* 发送数据
    if (!icmp_send(htonl(pstReqIpHdr->unSrcIP), ICMP_ECHOREPLY, 0, IP_TTL_DEFAULT, sBufListHead, &enErr))
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("icmp_send_echo_reply() failed, %s\r\n", onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }      

    //* 释放刚才申请的buf list节点
    buf_list_free(sDataNode);
}

static void icmp_rcv_handler_echoreply(UCHAR *pubPacket, INT nPacketLen)
{
    PST_IP_HDR pstIpHdr = (PST_IP_HDR)pubPacket;
    UCHAR usIpHdrLen = pstIpHdr->bitHdrLen * 4;
    PST_ICMP_ECHO_HDR pstEchoHdr = (PST_ICMP_ECHO_HDR)(pubPacket + usIpHdrLen + sizeof(ST_ICMP_HDR));
    INT nInput = onps_input_get_icmp(pstEchoHdr->usIdentifier);
    if (nInput < 0)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("The icmp echo request packet with ID %d is not found, the packet will be dropped\r\n", pstEchoHdr->usIdentifier);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    //* 将数据搬运到用户的接收缓冲区，然后发送一个信号量通知用户数据已到达
    HSEM hSem;
    UINT unRcvedBytes = (UINT)nPacketLen;
    UCHAR *pubRcvBuf = onps_input_get_rcv_buf(nInput, &hSem, &unRcvedBytes);
    memcpy(pubRcvBuf, pubPacket, unRcvedBytes);
    os_thread_sem_post(hSem);
}

static void icmp_rcv_handler_err(UCHAR *pubPacket, INT nPacketLen)
{
    PST_IP_HDR pstReqIpHdr = (PST_IP_HDR)pubPacket; 
    UCHAR usReqIpHdrLen = pstReqIpHdr->bitHdrLen * 4;
    PST_ICMP_HDR pstIcmpHdr = (PST_ICMP_HDR)(pubPacket + usReqIpHdrLen);
    PST_IP_HDR pstErrIpHdr = (PST_IP_HDR)(pubPacket + usReqIpHdrLen + sizeof(ST_ICMP_HDR) + 4);  //* icmp通知报文携带的ip首部
    struct in_addr stSrcAddr, stDstAddr; 

    os_critical_init();
    os_enter_critical();
    {
        o_stLastReportResult.ubType = pstIcmpHdr->ubType; 
        o_stLastReportResult.ubCode = pstIcmpHdr->ubCode; 
        o_stLastReportResult.ubProtocol = pstErrIpHdr->ubProto;
        o_stLastReportResult.unSrcAddr = pstErrIpHdr->unSrcIP; 
        o_stLastReportResult.unDstAddr = pstErrIpHdr->unDstIP; 
    }
    os_exit_critical();

#if SUPPORT_PRINTF    
    stSrcAddr.s_addr = pstErrIpHdr->unSrcIP;
    stDstAddr.s_addr = pstErrIpHdr->unDstIP;
    #if PRINTF_THREAD_MUTEX
    os_thread_mutex_lock(o_hMtxPrintf);
    #endif
    
    printf("%s, protocol %s, source %s, destination %s\r\n", icmp_get_description(pstIcmpHdr->ubType, pstIcmpHdr->ubCode), get_ip_proto_name(pstErrIpHdr->ubProto), inet_ntoa(stSrcAddr), inet_ntoa(stDstAddr));
    
    #if PRINTF_THREAD_MUTEX
    os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
}

void icmp_recv(UCHAR *pubPacket, INT nPacketLen)
{
    PST_IP_HDR pstIpHdr = (PST_IP_HDR)pubPacket; 
    UCHAR usIpHdrLen = pstIpHdr->bitHdrLen * 4;
    PST_ICMP_HDR pstIcmpHdr = (PST_ICMP_HDR)(pubPacket + usIpHdrLen);

    //* 先看看校验和是否正确
    USHORT usPktChecksum = pstIcmpHdr->usChecksum;
    pstIcmpHdr->usChecksum = 0;
    USHORT usChecksum = tcpip_checksum((USHORT *)pstIcmpHdr, nPacketLen - sizeof(ST_IP_HDR));
    if (usPktChecksum != usChecksum)
    {
#if SUPPORT_PRINTF
        pstIcmpHdr->usChecksum = usPktChecksum;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("checksum error (%04X, %04X), and the icmp packet will be dropped\r\n", usChecksum, usPktChecksum);
        printf_hex(pubPacket, nPacketLen, 48);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    switch ((EN_ICMPTYPE)pstIcmpHdr->ubType)
    {
    case ICMP_ECHOREPLY: 
        icmp_rcv_handler_echoreply(pubPacket, nPacketLen);
        break; 

    case ICMP_ECHOREQ:
        icmp_send_echo_reply(pubPacket, nPacketLen);
        break; 

    case ICMP_ERRDST:
    case ICMP_ERRSRC:
    case ICMP_ERRREDIRECT:
    case ICMP_ERRTTL:
    case ICMP_ERRIP:
        icmp_rcv_handler_err(pubPacket, nPacketLen);
        break; 

    default:
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("Unsupported icmp packet type (%d), and the packet will be dropped\r\n", (UINT)pstIcmpHdr->ubType); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        break;
    }
}

void icmp_get_last_report(PST_ICMP_REPORT_RESULT pstResult)
{
    os_critical_init(); 

    os_enter_critical();
    {
        *pstResult = o_stLastReportResult; 
    }
    os_exit_critical(); 
}
