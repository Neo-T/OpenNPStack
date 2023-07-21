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
#include "onps_input.h"
#include "ip/tcp_frame.h"
#include "ip/tcp_options.h" 
#define SYMBOL_GLOBALS
#include "ip/tcp.h"
#undef SYMBOL_GLOBALS

static void tcp_send_fin(PST_TCPLINK pstLink); 
static void tcpsrv_send_syn_ack_with_start_timer(PST_TCPLINK pstLink, void *pvSrcAddr, USHORT usSrcPort, void *pvDstAddr, USHORT usDstPort); 

void tcpsrv_syn_recv_timeout_handler(void *pvParam)
{
    PST_TCPLINK pstLink = (PST_TCPLINK)pvParam;

    //* 是否在溢出的时候收到应答
    if (!pstLink->stcbWaitAck.bIsAcked)
    {
        if (pstLink->stcbWaitAck.bRcvTimeout < 32)
        {
            pstLink->stcbWaitAck.bRcvTimeout *= 2; 
		#if SUPPORT_IPV6
			if(AF_INET == pstLink->stLocal.pstHandle->bFamily)
				tcpsrv_send_syn_ack_with_start_timer(pstLink, (in_addr_t *)&pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, (in_addr_t *)&pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort);
			else
				tcpsrv_send_syn_ack_with_start_timer(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv6, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv6, pstLink->stPeer.stSockAddr.usPort);
		#else
            tcpsrv_send_syn_ack_with_start_timer(pstLink, (in_addr_t *)&pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, (in_addr_t *)&pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort);
		#endif
        }
        else
        {
            onps_input_free(pstLink->stcbWaitAck.nInput); 

    #if SUPPORT_PRINTF && DEBUG_LEVEL > 1            
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
		#if SUPPORT_IPV6
			if (AF_INET == pstLink->stLocal.pstHandle->bFamily)
			{
				CHAR szAddr[20], szAddrClt[20];
				printf("The tcp server@%s:%d waits for the client's syn ack to time out, and the current link will be closed (cleint@%s:%d)\r\n",
						inet_ntoa_safe_ext(pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, szAddr), pstLink->stLocal.pstHandle->stSockAddr.usPort, inet_ntoa_safe_ext(pstLink->stPeer.stSockAddr.saddr_ipv4, szAddrClt), pstLink->stPeer.stSockAddr.usPort);
			}
			else
			{
				CHAR szIpv6[40];
				printf("The tcp server@[%s]:%d waits for the client's syn ack to time out, and the current link will be closed (cleint@[", inet6_ntoa(pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv6, szIpv6), pstLink->stLocal.pstHandle->stSockAddr.usPort);
				printf("%s]:%d)\r\n", inet6_ntoa(pstLink->stPeer.stSockAddr.saddr_ipv6, szIpv6), pstLink->stPeer.stSockAddr.usPort);
			}
		#else
			CHAR szAddr[20], szAddrClt[20];
            printf("The tcp server@%s:%d waits for the client's syn ack to time out, and the current link will be closed (cleint@%s:%d)\r\n", 
                        inet_ntoa_safe_ext(pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, szAddr), pstLink->stLocal.pstHandle->stSockAddr.usPort, inet_ntoa_safe_ext(pstLink->stPeer.stSockAddr.saddr_ipv4, szAddrClt), pstLink->stPeer.stSockAddr.usPort);
		#endif
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
        }
    }
}

static void tcp_ack_timeout_handler(void *pvParam)
{        
    PST_TCPLINK pstLink = (PST_TCPLINK)pvParam;     	

    if (!pstLink->stcbWaitAck.bIsAcked)
    {
        if (TLSCONNECTED == pstLink->bState)
        {
			if (TDSSENDING == pstLink->stLocal.bDataSendState)
				pstLink->stLocal.bDataSendState = (CHAR)TDSTIMEOUT;			
        }
        else
            pstLink->bState = TLSACKTIMEOUT;       		
                
		if (pstLink->stcbWaitAck.bRcvTimeout)					
			onps_input_sem_post(pstLink->stcbWaitAck.nInput);
    }

	pstLink->stcbWaitAck.pstTimer = NULL; 
}

static void tcp_close_timeout_handler(void *pvParam)
{
    PST_TCPLINK pstLink = (PST_TCPLINK)pvParam;
    INT nRtnVal; 

    switch ((EN_TCPLINKSTATE)pstLink->bState)
    {
    case TLSFINWAIT1: 
        nRtnVal = onps_input_tcp_close_time_count(pstLink->stcbWaitAck.nInput);
        if (nRtnVal == 1)        
            tcp_send_fin(pstLink);
        else if (nRtnVal == 2) //*一直没收到对端的ACK报文，直接进入FIN_WAIT2态        
            onps_input_set_tcp_close_state(pstLink->stcbWaitAck.nInput, TLSFINWAIT2); 
        else; 
        break; 

    case TLSFINWAIT2:
        nRtnVal = onps_input_tcp_close_time_count(pstLink->stcbWaitAck.nInput);        
        if (nRtnVal == 2) //* 一直未收到对端发送的FIN报文        
            onps_input_set_tcp_close_state(pstLink->stcbWaitAck.nInput, TLSTIMEWAIT);
        else; 
        break; 

    case TLSCLOSING:
        nRtnVal = onps_input_tcp_close_time_count(pstLink->stcbWaitAck.nInput);
        if (nRtnVal == 2) //* 一直未收到对端发送的FIN报文        
            onps_input_set_tcp_close_state(pstLink->stcbWaitAck.nInput, TLSTIMEWAIT); 
        else;         
        break; 

    case TLSTIMEWAIT:             
        nRtnVal = onps_input_tcp_close_time_count(pstLink->stcbWaitAck.nInput);         
        if (nRtnVal == 1)
        {
            if (pstLink->bIsPassiveFin)
                tcp_send_fin(pstLink);
        }
        if (nRtnVal == 2) //* 超时，则FIN操作结束，释放input资源
        {           
            onps_input_free(pstLink->stcbWaitAck.nInput);             
            return; 
        }
        else;        
        break;

    case TLSCLOSED:         
        //* FIN操作结束，释放input资源
        onps_input_free(pstLink->stcbWaitAck.nInput);        
        return;  
    }       

    //* 重新启动定时器
    pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_close_timeout_handler, pstLink, 1);     
}

#if SUPPORT_SACK
static INT tcp_send_packet(PST_TCPLINK pstLink, in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort, UNI_TCP_FLAG uniFlag, 
							UCHAR *pubOptions, USHORT usOptionsBytes, UCHAR *pubData, USHORT usDataBytes, BOOL blIsSpecSeqNum, UINT unSeqNum, EN_ONPSERR *penErr)
#else
static INT tcp_send_packet(PST_TCPLINK pstLink, in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort, 
							UNI_TCP_FLAG uniFlag, UCHAR *pubOptions, USHORT usOptionsBytes, UCHAR *pubData, USHORT usDataBytes, EN_ONPSERR *penErr)
#endif
{
    //* 挂载用户数据
    SHORT sBufListHead = -1; 
    SHORT sDataNode = -1; 
    if (pubData)
    {        
        sDataNode = buf_list_get_ext(pubData, (UINT)usDataBytes, penErr); 
        if (sDataNode < 0)                    
            return -1;        
        buf_list_put_head(&sBufListHead, sDataNode);
    }

    //* 挂载tcp options选项
    SHORT sOptionsNode = -1; 
    if (usOptionsBytes)
    {
        sOptionsNode = buf_list_get_ext(pubOptions, (UINT)usOptionsBytes, penErr);
        if (sOptionsNode < 0)
        {            
            if (sDataNode >= 0)
                buf_list_free(sDataNode);
            return -1;
        }
        buf_list_put_head(&sBufListHead, sOptionsNode);
    }

    //* 要确保本地Sequence Number和对端Sequence Number不乱序就必须加锁，因为tcp接收线程与发送线程并不属于同一个，因为线程优先级问题导致发送线程在发送前一刻被接收线程强行打断并率先发送了
    //* 应答报文，此时序号有可能已大于发送线程携带的序号，乱序问题就此产生
    //* 在这里加锁，当接收线程与发送线程同时调用这个函数时，会因为锁的存在使得调用按顺序进行，这样就确保sequence num不会乱序    
    onps_input_lock(pstLink->stcbWaitAck.nInput); 

    //* 填充tcp头
    ST_TCP_HDR stHdr; 
    stHdr.usSrcPort = htons(usSrcPort);
    stHdr.usDstPort = htons(usDstPort);
#if SUPPORT_SACK
    if (blIsSpecSeqNum)
        stHdr.unSeqNum = htonl(unSeqNum); 
    else
        stHdr.unSeqNum = htonl(pstLink->stLocal.unHasSndBytes + 1);
#else
    stHdr.unSeqNum = htonl(pstLink->stLocal.unSeqNum);
#endif
    stHdr.unAckNum = htonl(pstLink->stPeer.unSeqNum);
    uniFlag.stb16.hdr_len = (UCHAR)(sizeof(ST_TCP_HDR) / 4) + (UCHAR)(usOptionsBytes / 4); //* TCP头部字段实际长度（单位：32位整型）
    stHdr.usFlag = uniFlag.usVal;
    stHdr.usWinSize = htons(pstLink->stLocal.usWndSize/* - sizeof(ST_TCP_HDR) - TCP_OPTIONS_SIZE_MAX*/);
    stHdr.usChecksum = 0;
    stHdr.usUrgentPointer = 0; 
    //* 挂载到链表头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (UINT)sizeof(ST_TCP_HDR), penErr);
    if (sHdrNode < 0)
    {        
        if (sDataNode >= 0)
            buf_list_free(sDataNode);
        if (sOptionsNode >= 0)
            buf_list_free(sOptionsNode);
        
        onps_input_unlock(pstLink->stcbWaitAck.nInput);

        return -1;
    }
    buf_list_put_head(&sBufListHead, sHdrNode); 

	//* 计算校验和
	EN_ONPSERR enErr = ERRNO; 
	stHdr.usChecksum = tcpip_checksum_ipv4(unSrcAddr, unDstAddr, sizeof(ST_TCP_HDR) + usOptionsBytes + usDataBytes, IPPROTO_TCP, sBufListHead, &enErr);
	if (ERRNO != enErr)
	{        
		if (sDataNode >= 0)
			buf_list_free(sDataNode);
		if (sOptionsNode >= 0)
			buf_list_free(sOptionsNode);
		buf_list_free(sHdrNode);

        onps_input_unlock(pstLink->stcbWaitAck.nInput);

		return -1;
	}

    //* 发送之
    INT nRtnVal = ip_send_ext(unSrcAddr, unDstAddr, TCP, IP_TTL_DEFAULT, sBufListHead, penErr);
#if SUPPORT_SACK
    if (!blIsSpecSeqNum)
        pstLink->stLocal.unHasSndBytes += (UINT)usDataBytes; 
#endif
    onps_input_unlock(pstLink->stcbWaitAck.nInput);

    //* 释放刚才申请的buf list节点
    if (sDataNode >= 0)
        buf_list_free(sDataNode);
    if (sOptionsNode >= 0)
        buf_list_free(sOptionsNode);
    buf_list_free(sHdrNode);

    return nRtnVal; 
}

#if SUPPORT_IPV6
#if SUPPORT_SACK
static INT tcpv6_send_packet(PST_TCPLINK pstLink, UCHAR ubaSrcAddr[16], USHORT usSrcPort, UCHAR ubaDstAddr[16], USHORT usDstPort, UNI_TCP_FLAG uniFlag, 
								UCHAR *pubOptions, USHORT usOptionsBytes, UCHAR *pubData, USHORT usDataBytes, BOOL blIsSpecSeqNum, UINT unSeqNum, EN_ONPSERR *penErr)
#else
static INT tcpv6_send_packet(PST_TCPLINK pstLink, UCHAR ubaSrcAddr[16], USHORT usSrcPort, UCHAR ubaDstAddr[16], USHORT usDstPort, 
								UNI_TCP_FLAG uniFlag, UCHAR *pubOptions, USHORT usOptionsBytes, UCHAR *pubData, USHORT usDataBytes, EN_ONPSERR *penErr)
#endif
{
	//* 挂载用户数据
	SHORT sBufListHead = -1;
	SHORT sDataNode = -1;
	if (pubData)
	{       
		sDataNode = buf_list_get_ext(pubData, (UINT)usDataBytes, penErr);
		if (sDataNode < 0)
			return -1;
		buf_list_put_head(&sBufListHead, sDataNode);
	}

	//* 挂载tcp options选项
	SHORT sOptionsNode = -1;
	if (usOptionsBytes)
	{
		sOptionsNode = buf_list_get_ext(pubOptions, (UINT)usOptionsBytes, penErr);
		if (sOptionsNode < 0)
		{            
			if (sDataNode >= 0)
				buf_list_free(sDataNode);
			return -1;
		}
		buf_list_put_head(&sBufListHead, sOptionsNode);
	}

	//* 将tcp头挂载到链表头部
	ST_TCP_HDR stHdr;
	SHORT sHdrNode;
	sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (UINT)sizeof(ST_TCP_HDR), penErr);
	if (sHdrNode < 0)
	{        
		if (sDataNode >= 0)
			buf_list_free(sDataNode);
		if (sOptionsNode >= 0)
			buf_list_free(sOptionsNode);

		return -1;
	}
	buf_list_put_head(&sBufListHead, sHdrNode);

	//* 加锁，当接收线程与发送线程同时调用这个函数时，会因为锁的存在使得调用按顺序进行，这样就确保sequence num不会乱序
	INT nRtnVal = -1;    
    onps_input_lock(pstLink->stcbWaitAck.nInput);
	{
		//* 填充tcp头		
		stHdr.usSrcPort = htons(usSrcPort);
		stHdr.usDstPort = htons(usDstPort);
	#if SUPPORT_SACK
		if (blIsSpecSeqNum)
			stHdr.unSeqNum = htonl(unSeqNum);
		else
			stHdr.unSeqNum = htonl(pstLink->stLocal.unHasSndBytes + 1);
	#else
		stHdr.unSeqNum = htonl(pstLink->stLocal.unSeqNum);
	#endif
		stHdr.unAckNum = htonl(pstLink->stPeer.unSeqNum);
		uniFlag.stb16.hdr_len = (UCHAR)(sizeof(ST_TCP_HDR) / 4) + (UCHAR)(usOptionsBytes / 4); //* TCP头部字段实际长度（单位：32位整型）
		stHdr.usFlag = uniFlag.usVal;
		stHdr.usWinSize = htons(pstLink->stLocal.usWndSize);
		stHdr.usChecksum = 0;
		stHdr.usUrgentPointer = 0;

		//* 计算校验和
		EN_ONPSERR enErr = ERRNO;
		stHdr.usChecksum = tcpip_checksum_ipv6(ubaSrcAddr, ubaDstAddr, sizeof(ST_TCP_HDR) + usOptionsBytes + usDataBytes, IPPROTO_TCP, sBufListHead, &enErr);
		if (ERRNO == enErr)
		{
			//* 发送之
			UINT unFlowLabel = ipv6_flow_label_cal(ubaDstAddr, ubaSrcAddr, IPPROTO_TCP, usDstPort, usSrcPort);
			nRtnVal = ipv6_send_ext(ubaSrcAddr, ubaDstAddr, IPPROTO_TCP, sBufListHead, unFlowLabel, penErr);
		#if SUPPORT_SACK
			if (!blIsSpecSeqNum)
				pstLink->stLocal.unHasSndBytes += (UINT)usDataBytes;
		#endif
		}
		else
		{            
			if (penErr)
				*penErr = enErr;

			nRtnVal = -1;
		}
	}
    onps_input_unlock(pstLink->stcbWaitAck.nInput);

	//* 释放刚才申请的buf list节点
	if (sDataNode >= 0)
		buf_list_free(sDataNode);
	if (sOptionsNode >= 0)
		buf_list_free(sOptionsNode);
	buf_list_free(sHdrNode);

	return nRtnVal;
}
#endif

static void tcp_send_ack_of_syn_ack(INT nInput, PST_TCPLINK pstLink, void *pvNetifIp, USHORT usSrcPort, UINT unSrvAckNum)
{
    //* 标志字段syn域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;
    uniFlag.stb16.ack = 1;

    //* 更新tcp序号
    pstLink->stLocal.unSeqNum = unSrvAckNum; 
    pstLink->stPeer.unSeqNum += 1; 
    //pstLink->stPeer.unNextSeqNum = pstLink->stPeer.unSeqNum;

    //* 发送
    EN_ONPSERR enErr;
#if SUPPORT_IPV6
	INT nRtnVal; 
	if (AF_INET == pstLink->stLocal.pstHandle->bFamily)
	{
	#if SUPPORT_SACK
		nRtnVal = tcp_send_packet(pstLink, *((in_addr_t *)pvNetifIp), usSrcPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, TRUE, pstLink->stLocal.unSeqNum, &enErr);
	#else        
		nRtnVal = tcp_send_packet(pstLink, *((in_addr_t *)pvNetifIp), usSrcPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, &enErr);        
	#endif
	}
	else
	{
	#if SUPPORT_SACK
		nRtnVal = tcpv6_send_packet(pstLink, (UCHAR *)pvNetifIp, usSrcPort, pstLink->stPeer.stSockAddr.saddr_ipv6, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, TRUE, pstLink->stLocal.unSeqNum, &enErr);
	#else
		nRtnVal = tcpv6_send_packet(pstLink, (UCHAR *)pvNetifIp, usSrcPort, pstLink->stPeer.stSockAddr.saddr_ipv6, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, &enErr);
	#endif
	}
#else
#if SUPPORT_SACK
    INT nRtnVal = tcp_send_packet(pstLink, *((in_addr_t *)pvNetifIp), usSrcPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, TRUE, pstLink->stLocal.unSeqNum, &enErr); 
#else
	INT nRtnVal = tcp_send_packet(pstLink, *((in_addr_t *)pvNetifIp), usSrcPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, &enErr);
#endif
#endif
    if (nRtnVal > 0)
    {                  
        //* 连接成功
        pstLink->bState = (CHAR)TLSCONNECTED; 
    }
    else 
    {        
        pstLink->bState = (CHAR)TLSSYNACKACKSENTFAILED;

        if (nRtnVal < 0)
            onps_set_last_error(nInput, enErr);
        else
            onps_set_last_error(nInput, ERRSENDZEROBYTES);
    }    

	if (pstLink->stcbWaitAck.bRcvTimeout)			
		onps_input_sem_post(pstLink->stcbWaitAck.nInput);	
}

INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort, int nConnTimeout)
{
    EN_ONPSERR enErr;    

    //* 获取链路信息存储节点
    PST_TCPLINK pstLink; 
    if (!onps_input_get(nInput, IOPT_GETTCPUDPLINK, &pstLink, &enErr))
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

    //* 先寻址，因为tcp校验和计算需要用到本地地址，同时当前tcp链路句柄也需要用此标识
    UINT unNetifIp = route_get_netif_ip(unSrvAddr);
    if (!unNetifIp)
    {
        onps_set_last_error(nInput, ERRADDRESSING);
        return -1;
    }
    //* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
    pstHandle->stSockAddr.saddr_ipv4 = unNetifIp;
#if SUPPORT_IPV6
    pstHandle->stSockAddr.usPort = onps_input_port_new(AF_INET, IPPROTO_TCP);
#else
	pstHandle->stSockAddr.usPort = onps_input_port_new(IPPROTO_TCP);
#endif

    //* 标志字段syn域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag; 
    uniFlag.usVal = 0; 
    uniFlag.stb16.syn = 1;        

    //* 填充tcp头部选项数据
    UCHAR ubaOptions[TCP_OPTIONS_SIZE_MAX]; 
    INT nOptionsSize = tcp_options_attach(ubaOptions, sizeof(ubaOptions));    

    //* 加入定时器队列
    pstLink->stcbWaitAck.bRcvTimeout = nConnTimeout; 
    pstLink->stcbWaitAck.bIsAcked = FALSE;
    pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_ack_timeout_handler, pstLink, nConnTimeout ? nConnTimeout : TCP_CONN_TIMEOUT);        
    if (!pstLink->stcbWaitAck.pstTimer)
    {
        onps_set_last_error(nInput, ERRNOIDLETIMER);
        return -1;
    }

    //* 完成实际的发送
    pstLink->stLocal.unSeqNum = 0; 
    pstLink->bState = TLSSYNSENT;
#if SUPPORT_SACK
    INT nRtnVal = tcp_send_packet(pstLink, pstHandle->stSockAddr.saddr_ipv4, pstHandle->stSockAddr.usPort, unSrvAddr, usSrvPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, TRUE, 0, &enErr); 
#else
	INT nRtnVal = tcp_send_packet(pstLink, pstHandle->stSockAddr.saddr_ipv4, pstHandle->stSockAddr.usPort, unSrvAddr, usSrvPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, &enErr);
#endif
    if (nRtnVal > 0)
    {
        //* 加入定时器队列
        /*
        pstLink->stcbWaitAck.bIsAcked = FALSE; 
        pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_ack_timeout_handler, pstLink, nConnTimeout ? nConnTimeout : TCP_CONN_TIMEOUT); 
        if (!pstLink->stcbWaitAck.pstTimer)
        {
            onps_set_last_error(nInput, ERRNOIDLETIMER);
            return -1;             
        }        

        pstLink->bState = TLSSYNSENT; // 只有定时器申请成功了才会将链路状态迁移到syn报文已发送状态，以确保收到syn ack时能够进行正确匹配        
        */
    }
    else
    {
        pstLink->bState = TLSINIT; 
        one_shot_timer_safe_free(pstLink->stcbWaitAck.pstTimer);

        if(nRtnVal < 0)
            onps_set_last_error(nInput, enErr);
        else
            onps_set_last_error(nInput, ERRSENDZEROBYTES);
    }

    return nRtnVal; 
}

INT tcp_send_data(INT nInput, UCHAR *pubData, INT nDataLen, int nWaitAckTimeout)
{
    EN_ONPSERR enErr;

    //* 获取链路信息存储节点
    PST_TCPLINK pstLink;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPLINK, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

#if !SUPPORT_SACK
    //* 首先看看对端的mss能够接收多少数据
    INT nSndDataLen = nDataLen < (INT)pstLink->stPeer.usMSS ? nDataLen : (INT)pstLink->stPeer.usMSS; 
    //* 再看看对端的接收窗口是否足够大
    INT nWndSize = ((INT)pstLink->stPeer.usWndSize) * (INT)pow(2, pstLink->stPeer.bWndScale); 
    nSndDataLen = nSndDataLen < nWndSize ? nSndDataLen : nWndSize; 
#endif
    
    //* 标志字段push、ack域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;
    uniFlag.stb16.ack = 1;
    uniFlag.stb16.push = 1;

	//* 提前赋值，因为存在发送即接收到的情况（常见于本地以太网）
    pstLink->stcbWaitAck.bRcvTimeout = nWaitAckTimeout; //* 必须更新这个值，因为send_nb()函数不等待semaphore信号，所以需要显式地告知接收逻辑收到数据发送ack时不再投递semaphore

#if !SUPPORT_SACK
	pstLink->stcbWaitAck.bIsAcked = FALSE;                      
	pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_ack_timeout_handler, pstLink, nWaitAckTimeout ? nWaitAckTimeout : TCP_ACK_TIMEOUT);
	if (!pstLink->stcbWaitAck.pstTimer)
	{
		onps_set_last_error(nInput, ERRNOIDLETIMER);
		return -1;
	}    
#endif

    pstLink->stPeer.bIsNotAcked = FALSE; 

#if !SUPPORT_SACK
    pstLink->stcbWaitAck.usSendDataBytes = (USHORT)nSndDataLen; //* 记录当前实际发送的字节数
#else
    pstLink->stcbWaitAck.usSendDataBytes = (USHORT)nDataLen; //* 记录当前实际发送的字节数
#endif
    pstLink->stLocal.bDataSendState = TDSSENDING;

#if SUPPORT_IPV6
	INT nRtnVal; 
	if (AF_INET == pstLink->stLocal.pstHandle->bFamily)
	{
	#if SUPPORT_SACK
		nRtnVal = tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4,
									pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nDataLen, FALSE, 0, &enErr); 
	#else
		nRtnVal = tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4, 
									pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nSndDataLen, &enErr);
	#endif
	}
	else
	{
	#if SUPPORT_SACK
		nRtnVal = tcpv6_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv6, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv6,
										pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nDataLen, FALSE, 0, &enErr); 	
	#else
		nRtnVal = tcpv6_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv6, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv6,
										pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nSndDataLen, &enErr);
	#endif
	}
#else
#if SUPPORT_SACK
    INT nRtnVal = tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4,
                                    pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nDataLen, FALSE, 0, &enErr); 						
#else
	INT nRtnVal = tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4, 
									pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nSndDataLen, &enErr); 
#endif
#endif
    if (nRtnVal > 0)
    {        
        //* 加入定时器队列
        //pstLink->stcbWaitAck.bIsAcked = FALSE;
        //pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_ack_timeout_handler, pstLink, nWaitAckTimeout ? nWaitAckTimeout : TCP_ACK_TIMEOUT);
        //if (!pstLink->stcbWaitAck.pstTimer)
        //{
        //    onps_set_last_error(nInput, ERRNOIDLETIMER);
        //    return -1;
        //}
        
        //* 记录当前实际发送的字节数
        //pstLink->stcbWaitAck.usSendDataBytes = (USHORT)nSndDataLen; 
        //pstLink->stLocal.bDataSendState = TDSSENDING; 
#if SUPPORT_SACK
        return nDataLen;
#else
        return nSndDataLen; 
#endif
    }
    else
    {
        pstLink->stLocal.bDataSendState = TDSSENDRDY;

    #if !SUPPORT_SACK
		one_shot_timer_safe_free(pstLink->stcbWaitAck.pstTimer);
    #endif

        if (nRtnVal < 0)
            onps_set_last_error(nInput, enErr);
        else
            onps_set_last_error(nInput, ERRSENDZEROBYTES);
    }

    return nRtnVal;
}

#if SUPPORT_SACK
INT tcp_send_data_ext(INT nInput, UCHAR *pubData, INT nDataLen, UINT unSeqNum)
{
    EN_ONPSERR enErr;

    //* 获取链路信息存储节点
    PST_TCPLINK pstLink;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPLINK, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }    

    //* 标志字段push、ack域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;
    uniFlag.stb16.ack = 1;
    uniFlag.stb16.push = 1;

    //* 提前赋值，因为存在发送即接收到的情况（常见于本地以太网）
    pstLink->stcbWaitAck.bRcvTimeout = 0; //* 必须更新这个值，因为send_nb()函数不等待semaphore信号，所以需要显式地告知接收逻辑收到数据发送ack时不再投递semaphore

    pstLink->stPeer.bIsNotAcked = FALSE;

    pstLink->stcbWaitAck.usSendDataBytes = (USHORT)nDataLen; //* 记录当前实际发送的字节数
    pstLink->stLocal.bDataSendState = TDSSENDING; 

#if SUPPORT_IPV6
	INT nRtnVal; 
	if (AF_INET == pstLink->stLocal.pstHandle->bFamily)
	{
		nRtnVal = tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4,
									pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nDataLen, TRUE, unSeqNum, &enErr);
	}
	else
	{
		nRtnVal = tcpv6_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv6, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv6,
									pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nDataLen, TRUE, unSeqNum, &enErr);
	}
#else
    INT nRtnVal = tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4,
                                    pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, pubData, (USHORT)nDataLen, TRUE, unSeqNum, &enErr);
#endif
    if (nRtnVal > 0)
        return nDataLen;    
    else
    {
        pstLink->stLocal.bDataSendState = TDSSENDRDY;

        if (nRtnVal < 0)
            onps_set_last_error(nInput, enErr);
        else
            onps_set_last_error(nInput, ERRSENDZEROBYTES);
    }

    return nRtnVal;
}
#endif

static void tcp_send_fin(PST_TCPLINK pstLink)
{
    //* 标志字段fin、ack域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;
    uniFlag.stb16.ack = 1; 
    uniFlag.stb16.fin = 1;    

    //* 发送链路结束报文
#if SUPPORT_IPV6
	if (AF_INET == pstLink->stLocal.pstHandle->bFamily)
	{
	#if SUPPORT_SACK
		tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, FALSE, 0, NULL);
	#else
		tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, NULL);
	#endif
	} 
	else
	{
	#if SUPPORT_SACK
		tcpv6_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv6, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv6, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, FALSE, 0, NULL);
	#else
		tcpv6_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv6, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv6, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, NULL);
	#endif
	}
#else
#if SUPPORT_SACK
    tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, FALSE, 0, NULL); 
#else
	tcp_send_packet(pstLink, pstLink->stLocal.pstHandle->stSockAddr.saddr_ipv4, pstLink->stLocal.pstHandle->stSockAddr.usPort, pstLink->stPeer.stSockAddr.saddr_ipv4, pstLink->stPeer.stSockAddr.usPort, uniFlag, NULL, 0, NULL, 0, NULL);
#endif
#endif
}

void tcp_disconnect(INT nInput)
{
    EN_ONPSERR enErr;

    //* 获取链路信息存储节点
    PST_TCPLINK pstLink;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPLINK, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return;
    }

    //* 未连接的话直接返回，没必要发送结束连接报文
    if (TLSCONNECTED != (EN_TCPLINKSTATE)pstLink->bState)
        return;

    //* 一旦进入fin操作，bIsAcked不再被使用，为了节约内存，这里用于close操作计时
    pstLink->stcbWaitAck.bIsAcked = 0;
    pstLink->bIsPassiveFin = FALSE;

    //* 只有状态迁移成功才会发送fin报文
    if (onps_input_set_tcp_close_state(nInput, TLSFINWAIT1))
    {        
        tcp_send_fin(pstLink);

        //* 加入定时器队列          
        pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_close_timeout_handler, pstLink, 1);
        if (!pstLink->stcbWaitAck.pstTimer)
            onps_set_last_error(pstLink->stcbWaitAck.nInput, ERRNOIDLETIMER);
    }
}

void tcp_send_ack(PST_TCPLINK pstLink, in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort)
{
    //* 标志字段ack域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;
    uniFlag.stb16.ack = 1;
    pstLink->stPeer.bIsNotAcked = FALSE;

    //* 发送应答报文 
#if SUPPORT_SACK
    tcp_send_packet(pstLink, unSrcAddr, usSrcPort, unDstAddr, usDstPort, uniFlag, NULL, 0, NULL, 0, FALSE, 0, NULL); 
#else
	tcp_send_packet(pstLink, unSrcAddr, usSrcPort, unDstAddr, usDstPort, uniFlag, NULL, 0, NULL, 0, NULL);
#endif
}

static INT tcpsrv_send_syn_ack(PST_TCPLINK pstLink, void *pvSrcAddr, USHORT usSrcPort, void *pvDstAddr, USHORT usDstPort, EN_ONPSERR *penErr)
{
    //* 标志字段ack域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;     
    uniFlag.stb16.syn = 1;    
    uniFlag.stb16.ack = 1;

    //* 填充tcp头部选项数据
    UCHAR ubaOptions[TCP_OPTIONS_SIZE_MAX];    
    INT nOptionsSize = tcp_options_attach(ubaOptions, sizeof(ubaOptions)); 

    //* 完成实际的发送
#if SUPPORT_IPV6
	if (AF_INET == pstLink->stLocal.pstHandle->bFamily)
	{
	#if SUPPORT_SACK
		return tcp_send_packet(pstLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, TRUE, 0, penErr);
	#else
		return tcp_send_packet(pstLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, penErr);
	#endif
	}
	else
	{
	#if SUPPORT_SACK
		return tcpv6_send_packet(pstLink, (UCHAR *)pvSrcAddr, usSrcPort, (UCHAR *)pvDstAddr, usDstPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, TRUE, 0, penErr); 
	#else
		return tcpv6_send_packet(pstLink, (UCHAR *)pvSrcAddr, usSrcPort, (UCHAR *)pvDstAddr, usDstPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, penErr);
	#endif
	}
#else
#if SUPPORT_SACK
    return tcp_send_packet(pstLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, TRUE, 0, penErr);
#else
	return tcp_send_packet(pstLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0, penErr);
#endif
#endif
}

static INT tcpsrv_send_reset(CHAR bFamily, void *pvSrcAddr, USHORT usSrcPort, void *pvDstAddr, USHORT usDstPort, UINT unAckSeqNum, EN_ONPSERR *penErr)
{
    //* 标志字段ack域置1，其它标志域为0
    UNI_TCP_FLAG uniFlag;
    uniFlag.usVal = 0;    
    uniFlag.stb16.reset = 1;
    uniFlag.stb16.ack = 1;    

    ST_TCPLINK stTcpLink; 
    //stTcpLink.stcbWaitAck.nInput = -1; 
    stTcpLink.stLocal.unSeqNum = 0; 
    stTcpLink.stPeer.unSeqNum = unAckSeqNum; 
    stTcpLink.stLocal.usWndSize = 1024; 

    //* 完成实际的发送
#if SUPPORT_IPV6
    if (AF_INET == bFamily)
    {
    #if SUPPORT_SACK
        return tcp_send_packet(&stTcpLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, NULL, 0, NULL, 0, TRUE, 0, penErr);
    #else
        return tcp_send_packet(&stTcpLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, NULL, 0, NULL, 0, penErr);
    #endif
    }
    else
    {
    #if SUPPORT_SACK
        return tcpv6_send_packet(&stTcpLink, (UCHAR *)pvSrcAddr, usSrcPort, (UCHAR *)pvDstAddr, usDstPort, uniFlag, NULL, 0, NULL, 0, TRUE, 0, penErr);
    #else
        return tcpv6_send_packet(&stTcpLink, (UCHAR *)pvSrcAddr, usSrcPort, (UCHAR *)pvDstAddr, usDstPort, uniFlag, NULL, 0, NULL, 0, penErr);
    #endif
    }
#else
#if SUPPORT_SACK
    return tcp_send_packet(&stTcpLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, NULL, 0, NULL, 0, TRUE, 0, penErr);
#else
    return tcp_send_packet(&stTcpLink, *((in_addr_t *)pvSrcAddr), usSrcPort, *((in_addr_t *)pvDstAddr), usDstPort, uniFlag, NULL, 0, NULL, 0, penErr);
#endif
#endif
}

static void tcpsrv_send_syn_ack_with_start_timer(PST_TCPLINK pstLink, void *pvSrcAddr, USHORT usSrcPort, void *pvDstAddr, USHORT usDstPort)
{
    EN_ONPSERR enErr; 

    //* 启动一个定时器等待对端的ack到达，只有收到ack才能真正建立tcp双向链路，否则只能算是半开链路（或称作半连接链路，客户端只能发送无法接收）            
    pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcpsrv_syn_recv_timeout_handler, pstLink, pstLink->stcbWaitAck.bRcvTimeout);
    if (pstLink->stcbWaitAck.pstTimer)
    {
        //* 发送syn ack报文给对端
        pstLink->bState = TLSSYNACKSENT;         
        if (tcpsrv_send_syn_ack(pstLink, pvSrcAddr, usSrcPort, pvDstAddr, usDstPort, &enErr) < 0)
        {
            pstLink->bState = TLSRCVEDSYN;
            one_shot_timer_safe_free(pstLink->stcbWaitAck.pstTimer);
            onps_input_free(pstLink->stcbWaitAck.nInput); 

    #if SUPPORT_PRINTF && DEBUG_LEVEL > 1            
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
		#if SUPPORT_IPV6
			if (AF_INET == pstLink->stLocal.pstHandle->bFamily)
			{
				CHAR szAddr[20], szAddrClt[20];
				printf("tcpsrv_send_syn_ack() failed (server %s:%d, client %s:%d), %s\r\n", inet_ntoa_safe_ext(*((in_addr_t *)pvSrcAddr), szAddr), usSrcPort,
						inet_ntoa_safe_ext(*((in_addr_t *)pvDstAddr), szAddrClt), usDstPort, onps_error(enErr));
			}
			else
			{
				CHAR szIpv6[40];
				printf("tcpsrv_send_syn_ack() failed (server [%s]:%d, client ", inet6_ntoa((UCHAR *)pvSrcAddr, szIpv6), usSrcPort);
				printf("[%s]:%d), %s\r\n", inet6_ntoa((UCHAR *)pvDstAddr, szIpv6), usDstPort, onps_error(enErr));
			}			
		#else
			CHAR szAddr[20], szAddrClt[20];
            printf("tcpsrv_send_syn_ack() failed (server %s:%d, client %s:%d), %s\r\n", inet_ntoa_safe_ext(*((in_addr_t *)pvSrcAddr), szAddr), usSrcPort, inet_ntoa_safe_ext(*((in_addr_t *)pvDstAddr), szAddrClt), usDstPort, onps_error(enErr));
		#endif
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
        }        
    }
    else
    {
        onps_input_free(pstLink->stcbWaitAck.nInput); 

#if SUPPORT_PRINTF            
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("%s\r\n", onps_error(ERRNOIDLETIMER));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }
}

#if SUPPORT_SACK
static void tcp_send_timer_retransmit(PST_TCPLINK pstLink, PSTCB_TCPSENDTIMER pstcbSndTimer)
{
    //* 取出数据重新发送
    EN_ONPSERR enErr;
    UCHAR *pubData = (UCHAR *)buddy_alloc(pstcbSndTimer->unRight - pstcbSndTimer->unLeft, &enErr);
    if (pubData)
    {
        UINT unStartReadIdx = pstcbSndTimer->unLeft % TCPSNDBUF_SIZE;
        UINT unEndReadIdx = pstcbSndTimer->unRight % TCPSNDBUF_SIZE;
        if (unEndReadIdx > unStartReadIdx)
            memcpy(pubData, pstLink->stcbSend.pubSndBuf + unStartReadIdx, unEndReadIdx - unStartReadIdx);
        else
        {
            UINT unCpyBytes = TCPSNDBUF_SIZE - unStartReadIdx;
            memcpy(pubData, pstLink->stcbSend.pubSndBuf + unStartReadIdx, unCpyBytes);
            memcpy(pubData + unCpyBytes, pstLink->stcbSend.pubSndBuf, unEndReadIdx);
        }

        //* 重发dup ack的数据块
        tcp_send_data_ext(pstLink->stcbWaitAck.nInput, pubData, pstcbSndTimer->unRight - pstcbSndTimer->unLeft, pstcbSndTimer->unLeft + 1);
        buddy_free(pubData);             

        //* 将timer从当前位置转移到队列的尾部，并重新开启重传计时
        pstcbSndTimer->unSendMSecs = os_get_system_msecs();
        //pstcbSndTimer->bIsNotSacked = TRUE;
        tcp_send_timer_node_del_unsafe(pstcbSndTimer);
        tcp_send_timer_node_put_unsafe(pstcbSndTimer);

        //os_critical_init();
        //os_enter_critical();
        //pstLink->stcbSend.unRetransSeqNum = 0;
        //os_exit_critical();
    }
    else
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("tcp_send_timer_retransmit() failed, %s\r\n", onps_error(enErr));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }
}

static void tcp_link_fast_retransmit(PST_TCPLINK pstLink, UINT unRetransSeqNum)
{    
    tcp_send_timer_lock();
    {
        PSTCB_TCPSENDTIMER pstcbLastSackedSndTimer = NULL; 
        PSTCB_TCPSENDTIMER pstcbSndTimerRetrans = NULL;
        PSTCB_TCPSENDTIMER pstcbSndTimer = pstLink->stcbSend.pstcbSndTimer;

        //* 定位最后一个sacked的报文及dup ack次数超过3次的报文
        while (pstcbSndTimer)
        {
            //* 尚未sacked的报文
            if (pstcbSndTimer->bIsNotSacked)
            {
                //* dup ack超过3次的报文
                if (pstcbSndTimer->unLeft == unRetransSeqNum - 1)
                    pstcbSndTimerRetrans = pstcbSndTimer;
            }
            else
                pstcbLastSackedSndTimer = pstcbSndTimer;                   

            pstcbSndTimer = pstcbSndTimer->pstcbNextForLink;
        }

        //* 重发dup ack次数超过3次的报文
        if (pstcbSndTimerRetrans)
        {
            tcp_send_timer_retransmit(pstLink, pstcbSndTimerRetrans);
            pstcbSndTimer = pstcbSndTimerRetrans->pstcbNextForLink; 
        }
        else
            pstcbSndTimer = pstLink->stcbSend.pstcbSndTimer;

        //* 发送尚未sacked的报文，因为pstcbLastSackedSndTimer不为空意味着其指向的报文已经被对端收到，该报文之前如果未被sacked则可以认为丢了（当然存在因路由路径导致的晚到情形，但为了实现快速重传这里认为丢了）
        if (pstcbLastSackedSndTimer)
        {            
            while (pstcbSndTimer)
            {
                if (pstcbSndTimer != pstcbLastSackedSndTimer)
                {
                    if(pstcbSndTimer->bIsNotSacked)
                        tcp_send_timer_retransmit(pstLink, pstcbSndTimer);
                }
                else
                    break; 

                pstcbSndTimer = pstcbSndTimer->pstcbNextForLink;
            }
        }
    }
    tcp_send_timer_unlock();    
}

static void tcp_packet_sacked(PST_TCPLINK pstLink, UINT unLeft, UINT unRight)
{    
    tcp_send_timer_lock();
    {
        PSTCB_TCPSENDTIMER pstcbSndTimer = pstLink->stcbSend.pstcbSndTimer;
        while (pstcbSndTimer)
        {
            //* 不在sack left的前面，则其在sack确认范围内
            if (!uint_before(pstcbSndTimer->unLeft, unLeft))
            {                
                //* right不在sack right的后面
                if (!uint_after(pstcbSndTimer->unRight, unRight))
                {							
                    pstcbSndTimer->bIsNotSacked = FALSE;
                    unLeft = pstcbSndTimer->unRight;
                }
                else
                {
                    pstcbSndTimer->unLeft = unRight; 
                    break; 
                }                
            }

            pstcbSndTimer = pstcbSndTimer->pstcbNextForLink;            
        }
    }
    tcp_send_timer_unlock(); 
}

static void tcp_link_sack_handler(PST_TCPLINK pstLink, CHAR bSackNum)
{    
    UINT unStartSeq = pstLink->stLocal.unSeqNum;
    CHAR i;
    
    //* 发送数据    
    for (i = 0; i < bSackNum; i++)
    {                
        //* 先判断sack范围是否合理       
        if (uint_after(pstLink->stcbSend.staSack[i].unLeft, unStartSeq) && !uint_after(pstLink->stcbSend.staSack[i].unRight - 1, pstLink->stcbSend.unWriteBytes))            
            tcp_packet_sacked(pstLink, pstLink->stcbSend.staSack[i].unLeft - 1, pstLink->stcbSend.staSack[i].unRight - 1);         

        unStartSeq = pstLink->stcbSend.staSack[i].unRight;
    }    
}

static void tcp_link_ack_handler(PST_TCPLINK pstLink)
{
    if (pstLink->stLocal.unAckedSeqNum == pstLink->stLocal.unSeqNum)
        return; 

    pstLink->stLocal.unAckedSeqNum = pstLink->stLocal.unSeqNum; 
    tcp_send_timer_lock();
    {
        PSTCB_TCPSENDTIMER pstSendTimer = pstLink->stcbSend.pstcbSndTimer;
        UINT unCurSeqNum = pstLink->stLocal.unAckedSeqNum - 1;
        while (pstSendTimer)
        {
            //* 首先看看sequence num是否在left之前，如果是，则停止循环，因为这之后的所有已发送报文均比当前报文还要靠后
            if (!uint_after(unCurSeqNum, pstSendTimer->unLeft))
                break;

            //* 已发送数据在确认序号之后，说明这之后的数据尚未成功送达对端，退出循环不再继续查找剩下的了
            if (uint_after(pstSendTimer->unRight, unCurSeqNum))
            {
                //* 将left指针后移至已确认的sequense number
                pstSendTimer->unLeft = unCurSeqNum;
                pstSendTimer->bIsNotSacked = TRUE;

                break;
            }

            //* 执行到这里，说明当前报文已经成功送达对端，从当前链表队列中删除                        
            pstLink->stcbSend.pstcbSndTimer = pstSendTimer->pstcbNextForLink;
            pstLink->stcbSend.bSendPacketNum--;

            tcp_send_timer_node_del_unsafe(pstSendTimer);   //* 从定时器队列中删除当前节点                
            tcp_send_timer_node_free_unsafe(pstSendTimer);  //* 归还当前节点            

			//* 继续取出下一个节点
            pstSendTimer = pstSendTimer->pstcbNextForLink;
        }
    }
    tcp_send_timer_unlock();
}
#endif

void tcp_recv(void *pvSrcAddr, void *pvDstAddr, UCHAR *pubPacket, INT nPacketLen, EN_NPSPROTOCOL enProtocol)
{
	EN_ONPSERR enErr = ERRNO;
	PST_TCP_HDR pstHdr = (PST_TCP_HDR)pubPacket;

#if !SUPPORT_IPV6
	enProtocol = enProtocol;
#endif

    os_critical_init();            

    //* 计算校验和
    USHORT usPktChecksum = pstHdr->usChecksum;
    pstHdr->usChecksum = 0; 
#if SUPPORT_IPV6
	USHORT usChecksum; 
	if (IPV4 == enProtocol)	
		usChecksum = tcpip_checksum_ipv4_ext(*((in_addr_t *)pvSrcAddr), *((in_addr_t *)pvDstAddr), IPPROTO_TCP, pubPacket, (USHORT)nPacketLen, &enErr);
	else
		usChecksum = tcpip_checksum_ipv6_ext((UCHAR *)pvSrcAddr, (UCHAR *)pvDstAddr, IPPROTO_TCP, pubPacket, (UINT)nPacketLen, &enErr);
#else
    USHORT usChecksum = tcpip_checksum_ipv4_ext(*((in_addr_t *)pvSrcAddr), *((in_addr_t *)pvDstAddr), IPPROTO_TCP, pubPacket, (USHORT)nPacketLen, &enErr);
#endif
	if (ERRNO != enErr)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("tcpip_checksum_ipv4_ext() failed, %s, the tcp packet will be dropped\r\n", onps_error(enErr));
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif			
		return;
	}

	//* 是否通过校验和计算
    if (usPktChecksum != usChecksum)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
        pstHdr->usChecksum = usPktChecksum;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("checksum error (%04X, %04X), the tcp packet will be dropped\r\n", usChecksum, usPktChecksum);
        printf_hex(pubPacket, nPacketLen, 48);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    //* 先查找当前链路是否存在
    USHORT usDstPort = htons(pstHdr->usDstPort);
#if SUPPORT_IPV6
	union {
		UINT unVal; 
		UCHAR *pubVal; 
	} uniCltIp;
	if (IPV4 == enProtocol)
		uniCltIp.unVal = *((in_addr_t *)pvSrcAddr);
	else
		uniCltIp.pubVal = (UCHAR *)pvSrcAddr; 
#else
    UINT unCltIp = *((in_addr_t *)pvSrcAddr);
#endif
    USHORT usCltPort = htons(pstHdr->usSrcPort);
    PST_TCPLINK pstLink;     
#if SUPPORT_IPV6
    INT nInput = onps_input_get_handle((IPV4 == enProtocol) ? AF_INET : AF_INET6, IPPROTO_TCP, pvDstAddr, usDstPort, &pstLink);
#else
	INT nInput = onps_input_get_handle(IPPROTO_TCP, *((in_addr_t *)pvDstAddr), usDstPort, &pstLink);
#endif
    if (nInput < 0)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
        UCHAR *pubAddr = (UCHAR *)pvDstAddr;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif     
	#if SUPPORT_IPV6
		if (IPV4 == enProtocol)
			printf("The tcp link of %d.%d.%d.%d:%d isn't found, the packet will be dropped\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3], usDstPort); 
		else
		{
			CHAR szIpv6[40]; 
			printf("The tcp link of [%s]:%d isn't found, the packet will be dropped\r\n", inet6_ntoa(pubAddr, szIpv6), usDstPort); 
		}
	#else
		printf("The tcp link of %d.%d.%d.%d:%d isn't found, the packet will be dropped\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3], usDstPort);
	#endif
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return; 
    }

    //* 依据报文头部标志字段确定下一步的处理逻辑        
    UINT unSrcAckNum = htonl(pstHdr->unAckNum);
    UINT unPeerSeqNum = htonl(pstHdr->unSeqNum); 
    UNI_TCP_FLAG uniFlag; 
    uniFlag.usVal = pstHdr->usFlag;
	INT nTcpHdrLen = uniFlag.stb16.hdr_len * 4;
    if (uniFlag.stb16.ack || uniFlag.stb16.reset || uniFlag.stb16.reset)
    {              
    #if SUPPORT_ETHERNET
        //* 如果为NULL则说明是tcp服务器，需要再次遍历input链表找出其先前分配的链路信息及input句柄
        if(!pstLink) 
        { 
		#if SUPPORT_IPV6
			INT nRmtCltInput = onps_input_get_handle_of_tcp_rclient(pvDstAddr, usDstPort, (IPV4 == enProtocol) ? (void *)&uniCltIp.unVal : (void *)uniCltIp.pubVal, usCltPort, &pstLink);
		#else
            INT nRmtCltInput = onps_input_get_handle_of_tcp_rclient(pvDstAddr, usDstPort, (in_addr_t *)&unCltIp, usCltPort, &pstLink); 
		#endif
            if (nRmtCltInput < 0) //* 尚未收到任何syn连接请求报文，这里就直接丢弃该报文
                return; 

            //* 说明尚未收到过ACK报文，这里就需要先等待这个报文
            if (TLSSYNACKSENT == pstLink->bState)
            {
                //* 序号完全相等才意味着这是一个合法的syn ack's ack报文
                if (unSrcAckNum == pstLink->stLocal.unSeqNum + 1 && unPeerSeqNum == pstLink->stPeer.unSeqNum)
                {                                        
                    //* 投递信号给accept()函数
                    onps_input_sem_post_tcpsrv_accept(nInput, nRmtCltInput, unSrcAckNum); 
                }
            }

            nInput = nRmtCltInput; 
        }
    #endif

        //* 更新对端的窗口信息                                                    
        os_enter_critical();
        {
            pstLink->stPeer.usWndSize = htons(pstHdr->usWinSize);
		#if SUPPORT_SACK
            pstLink->stcbSend.bIsWndSizeUpdated = TRUE; 
		#endif
        }        
        os_exit_critical();

        //* 连接请求的应答报文
        if (uniFlag.stb16.syn)
        {           
            if (TLSSYNSENT == pstLink->bState && unSrcAckNum == pstLink->stLocal.unSeqNum + 1) //* 确定这是一个有效的syn ack报文才可进入下一个处理流程，否则报文将被直接抛弃
            {                
                pstLink->stcbWaitAck.bIsAcked = TRUE; 
                one_shot_timer_safe_free(pstLink->stcbWaitAck.pstTimer);                                 
                //one_shot_timer_recount(pstLink->stcbWaitAck.pstTimer, 1); //* 通知定时器结束计时，释放占用的非常宝贵的定时器资源

                //* 记录当前链路信息
                pstLink->stPeer.unSeqNum = unPeerSeqNum;
                //pstLink->stPeer.usWndSize = htons(pstHdr->usWinSize); 
			#if SUPPORT_IPV6
				if (IPV4 == enProtocol)
					pstLink->stPeer.stSockAddr.saddr_ipv4 = uniCltIp.unVal;
				else
					memcpy(pstLink->stPeer.stSockAddr.saddr_ipv6, uniCltIp.pubVal, 16);

			#else
                pstLink->stPeer.stSockAddr.saddr_ipv4 = unCltIp/*htonl(unSrcAddr)*/;
			#endif
                pstLink->stPeer.stSockAddr.usPort = usCltPort/*htons(pstHdr->usSrcPort)*/;

                //* 截取tcp头部选项字段
                tcp_options_get(pstLink, pubPacket + sizeof(ST_TCP_HDR), nTcpHdrLen - (INT)sizeof(ST_TCP_HDR));

                //* 状态迁移到已接收到syn ack报文
                pstLink->bState = TLSRCVEDSYNACK;

                //* 发送syn ack的ack报文
                tcp_send_ack_of_syn_ack(nInput, pstLink, pvDstAddr, usDstPort, unSrcAckNum);
            }
        }
        else if (uniFlag.stb16.reset)
        {   
            if (pstLink->bState < TLSFINWAIT1)
            {
                //* 状态迁移到链路已被对端复位的状态
                pstLink->bState = TLSRESET;
            }
            if (pstLink->stLocal.bDataSendState == TDSSENDING)
                pstLink->stLocal.bDataSendState = TDSLINKRESET; 

            //if (INVALID_HSEM != pstLink->stcbWaitAck.hSem)
            //    os_thread_sem_post(pstLink->stcbWaitAck.hSem); 			
            onps_input_sem_post(nInput);
        }
        else if (uniFlag.stb16.fin)
        {                       
            if (pstLink->stLocal.bDataSendState == TDSSENDING)
            {
                pstLink->stLocal.bDataSendState = TDSLINKCLOSED;                                 

				if (pstLink->stcbWaitAck.bRcvTimeout)									
					onps_input_sem_post(pstLink->stcbWaitAck.nInput);
            }            

            //* 发送ack
            pstLink->stPeer.unSeqNum = /*pstLink->stPeer.unNextSeqNum = */unPeerSeqNum + 1;
		#if SUPPORT_IPV6
			if (IPV4 == enProtocol)
				tcp_send_ack(pstLink, *((in_addr_t *)pvDstAddr), usDstPort, uniCltIp.unVal, usCltPort);
			else
				tcpv6_send_ack(pstLink, (UCHAR *)pvDstAddr, usDstPort, uniCltIp.pubVal, usCltPort); 
		#else
            tcp_send_ack(pstLink, *((in_addr_t *)pvDstAddr), usDstPort, unCltIp/*htonl(unSrcAddr)*/, usCltPort/*htons(pstHdr->usSrcPort)*/);
		#endif
            //* 迁移到相关状态
            if (TLSCONNECTED == (EN_TCPLINKSTATE)pstLink->bState)
            {                       
                if (onps_input_set_tcp_close_state(nInput, TLSFINWAIT1))
                {
                    onps_input_set_tcp_close_state(nInput, TLSFINWAIT2); //* 进入FIN_WAIT2态
                    pstLink->bIsPassiveFin = TRUE;

                    //* 同样立即下发结束报文，本地也不再继续发送了（而不是按照协议约定允许上层用户继续在半关闭状态下发送数据）
                    tcp_send_fin(pstLink);                   

                    //* 加入定时器队列                      
                    pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_close_timeout_handler, pstLink, 1);                    

                    if (!pstLink->stcbWaitAck.pstTimer)
                        onps_set_last_error(pstLink->stcbWaitAck.nInput, ERRNOIDLETIMER);
                    onps_input_set_tcp_close_state(nInput, TLSTIMEWAIT); //* 等待对端的ACK，然后结束
                }                                
            }
            else if(TLSFINWAIT1 == (EN_TCPLINKSTATE)pstLink->bState)
                onps_input_set_tcp_close_state(nInput, TLSCLOSING);
            else if(TLSFINWAIT2 == (EN_TCPLINKSTATE)pstLink->bState)
                onps_input_set_tcp_close_state(nInput, TLSTIMEWAIT); 
            else; 
        }
        else
        {               
            //* 看看有数据吗？            
            INT nDataLen = nPacketLen - nTcpHdrLen;

            //* 处于关闭状态
            if ((EN_TCPLINKSTATE)pstLink->bState > TLSRESET && (EN_TCPLINKSTATE)pstLink->bState < TLSCLOSED)
            {
                if(TLSFINWAIT1 == (EN_TCPLINKSTATE)pstLink->bState)
                    onps_input_set_tcp_close_state(nInput, TLSFINWAIT2); 
                else if(TLSCLOSING == (EN_TCPLINKSTATE)pstLink->bState)                       
                    onps_input_set_tcp_close_state(nInput, TLSTIMEWAIT);
                else if(TLSTIMEWAIT == (EN_TCPLINKSTATE)pstLink->bState)
                {
                    if(pstLink->bIsPassiveFin)
                        onps_input_set_tcp_close_state(nInput, TLSCLOSED); 
                }
                else;                 

                if (nDataLen)
                    onps_input_tcp_recv(nInput, NULL, 0, NULL);
            }            

        #if SUPPORT_SACK
            //* 看看是否存在sack选项                          
            if (nTcpHdrLen > sizeof(ST_TCP_HDR))
            {                
                CHAR bSackNum = tcp_options_get_sack(pstLink, pubPacket + sizeof(ST_TCP_HDR), nTcpHdrLen - (INT)sizeof(ST_TCP_HDR));
				if (bSackNum)		
					tcp_link_sack_handler(pstLink, bSackNum);				
            }
           
            if (unSrcAckNum == pstLink->stcbSend.unPrevSeqNum) //* 记录dup ack数量
            {
                pstLink->stcbSend.bDupAckNum++;                
                if (pstLink->stcbSend.bDupAckNum > 3)
                {                    
                    tcp_link_fast_retransmit(pstLink, unSrcAckNum); 
                    pstLink->stcbSend.bDupAckNum = 0; 
                }                
            }
            else
                pstLink->stcbSend.bDupAckNum = 1;

            //* 看看是否存在sack选项
            /*
            if (nTcpHdrLen > sizeof(ST_TCP_HDR))
            {
                CHAR bSackNum = tcp_options_get_sack(pstLink, pubPacket + sizeof(ST_TCP_HDR), nTcpHdrLen - (INT)sizeof(ST_TCP_HDR));
                if(bSackNum)
                    tcp_link_sack_handler(pstLink, bSackNum);
            }
            else
            {
                // 如果不存在sack选项，看看是否出现dup ack情形，存在则立即重发丢失的数据
                if (pstLink->stcbSend.bDupAckNum > 3)
                {
                    tcp_link_fast_retransmit(pstLink, unSrcAckNum); 
                    pstLink->stcbSend.bDupAckNum = 0;
                }
            }
            */

            //* 收到应答，更新当前数据发送序号，条件是ack序号一定要在之前已经确认的序号之后（相等或之前不更新，因为存在ack乱序的情形）
            if (uint_after(unSrcAckNum, pstLink->stLocal.unSeqNum))
            {
                pstLink->stcbSend.unPrevSeqNum = unSrcAckNum;
                pstLink->stLocal.unSeqNum = unSrcAckNum; 

                tcp_link_for_send_data_put(pstLink);
                tcp_send_sem_post();
            }            

            //tcp_link_for_send_data_put(pstLink);
            //tcp_send_sem_post();
        #else
            //* 已经发送了数据，看看是不是对应的ack报文            			
            if (TDSSENDING == (EN_TCPDATASNDSTATE)pstLink->stLocal.bDataSendState && unSrcAckNum == pstLink->stLocal.unSeqNum + (UINT)pstLink->stcbWaitAck.usSendDataBytes)
            {                
                //* 收到应答，更新当前数据发送序号            
                pstLink->stLocal.unSeqNum = unSrcAckNum;				

                pstLink->stcbWaitAck.bIsAcked = TRUE; 				
                one_shot_timer_safe_free(pstLink->stcbWaitAck.pstTimer);                 				

                //* 数据发送状态迁移至已收到ACK报文状态，并通知发送者当前数据已发送成功
                pstLink->stLocal.bDataSendState = (CHAR)TDSACKRCVED;                 
				if (pstLink->stcbWaitAck.bRcvTimeout)									
					onps_input_sem_post(pstLink->stcbWaitAck.nInput);
            }
			/*
			else
			{
				os_thread_mutex_lock(o_hMtxPrintf);
				printf("<%d> %08X %d %d %d data: %d\r\n", pstLink->stLocal.bDataSendState, unPeerSeqNum, unSrcAckNum, pstLink->stLocal.unSeqNum, (UINT)pstLink->stcbWaitAck.usSendDataBytes, nDataLen);
				os_thread_mutex_unlock(o_hMtxPrintf);
			}
			*/
        #endif
            
            //* 有数据则处理之
            if (nDataLen)
            {                          
                //* 只有得到期望序号的数据报文才会被上传给用户层，乱序报文直接丢弃
                if (/*unPeerSeqNum == pstLink->stPeer.unSeqNum || */!uint_after(unPeerSeqNum, pstLink->stPeer.unSeqNum) && uint_after(unPeerSeqNum + nDataLen, pstLink->stPeer.unSeqNum))
                {
                    //* 必须是非零窗口探测报文才搬运数据
                    if (!(nDataLen == 1 && pstLink->stLocal.bIsZeroWnd))
                    {
                        //* 将数据搬运到input层						
                        nDataLen = onps_input_tcp_recv(nInput, (const UCHAR *)(pubPacket + nTcpHdrLen), (UINT)(unPeerSeqNum + (UINT)nDataLen - pstLink->stPeer.unSeqNum), &enErr);
                        if (nDataLen < 0)
                        {
                    #if SUPPORT_PRINTF && DEBUG_LEVEL
                        #if PRINTF_THREAD_MUTEX
                            os_thread_mutex_lock(o_hMtxPrintf);
                        #endif
                            printf("onps_input_tcp_recv() failed, %s, the tcp packet will be dropped\r\n", onps_error(enErr));
                        #if PRINTF_THREAD_MUTEX
                            os_thread_mutex_unlock(o_hMtxPrintf);
                        #endif
                    #endif
                            return;
                        }
                    }
                    else //* 零窗口探测报文则只更新当前窗口大小并通知给对端
                    {
                        USHORT usWndSize = pstLink->stLocal.usWndSize;
                        if (usWndSize)
                            pstLink->stLocal.bIsZeroWnd = FALSE;
                        //pstLink->stPeer.unSeqNum = /*pstLink->stPeer.unNextSeqNum = */unPeerSeqNum;
					#if SUPPORT_IPV6
						if (IPV4 == enProtocol)
							tcp_send_ack(pstLink, *((in_addr_t *)pvDstAddr), usDstPort, uniCltIp.unVal, usCltPort);
						else
							tcpv6_send_ack(pstLink, (UCHAR *)pvDstAddr, usDstPort, uniCltIp.pubVal, usCltPort); 
					#else
                        tcp_send_ack(pstLink, *((in_addr_t *)pvDstAddr), usDstPort, unCltIp/*htonl(unSrcAddr)*/, usCltPort/*htons(pstHdr->usSrcPort)*/);
					#endif

                        return; 
                    }
                }
                else
                    nDataLen = 0; //* 为了节省内存，这里直接丢弃乱序的报文，而不是先缓存

                //* 更新对端的窗口信息                                                    
                //pstLink->stPeer.usWndSize = htons(pstHdr->usWinSize);

                pstLink->stPeer.unSeqNum += nDataLen;
				if (pstLink->uniFlags.stb16.no_delay_ack || (EN_TCPLINKSTATE)pstLink->bState != TLSCONNECTED || (nDataLen > 0 && pstLink->stPeer.bIsNotAcked)/* 最后一个条件为当连续收到两个对端的数据报文后立即发送ack */)
				{
				#if SUPPORT_IPV6
					if (IPV4 == enProtocol)
						tcp_send_ack(pstLink, *((in_addr_t *)pvDstAddr), usDstPort, uniCltIp.unVal, usCltPort);
					else
						tcpv6_send_ack(pstLink, (UCHAR *)pvDstAddr, usDstPort, uniCltIp.pubVal, usCltPort);
				#else
					tcp_send_ack(pstLink, *((in_addr_t *)pvDstAddr), usDstPort, unCltIp/*htonl(unSrcAddr)*/, usCltPort/*htons(pstHdr->usSrcPort)*/);
				#endif
				}
                else
                {
                    pstLink->stPeer.unStartMSecs = os_get_system_msecs();
                    pstLink->stPeer.bIsNotAcked = TRUE;
                }
            }            
        }
    }
#if SUPPORT_ETHERNET
    //* 只有tcp syn连接请求报文才没有ack标志
    else
    {       
        //* 首先看看是否已针对当前请求申请了input
	#if SUPPORT_IPV6
		INT nRmtCltInput = onps_input_get_handle_of_tcp_rclient(pvDstAddr, usDstPort, (IPV4 == enProtocol) ? (void *)&uniCltIp.unVal : (void *)uniCltIp.pubVal, usCltPort, &pstLink);
	#else
        INT nRmtCltInput = onps_input_get_handle_of_tcp_rclient(pvDstAddr, usDstPort, (in_addr_t *)&unCltIp, usCltPort, &pstLink); 
	#endif
        if (nRmtCltInput < 0) //* 尚未申请input节点，这里需要先申请一个
        {                            
		#if SUPPORT_IPV6
			nRmtCltInput = onps_input_new_tcp_remote_client(nInput, usDstPort, pvDstAddr, usCltPort, (IPV4 == enProtocol) ? (void *)&uniCltIp.unVal : (void *)uniCltIp.pubVal, &pstLink, &enErr);
		#else
            nRmtCltInput = onps_input_new_tcp_remote_client(nInput, usDstPort, pvDstAddr, usCltPort, (in_addr_t *)&unCltIp, &pstLink, &enErr); 
		#endif
            if (nRmtCltInput < 0)
            {
                if (ERRNOFREEMEM == enErr)
                {                    
                #if SUPPORT_IPV6
                    tcpsrv_send_reset((IPV4 == enProtocol) ? AF_INET : AF_INET6, pvDstAddr, usDstPort, (IPV4 == enProtocol) ? (void *)&uniCltIp.unVal : (void *)uniCltIp.pubVal, usCltPort, unPeerSeqNum + 1, &enErr);
                #else
                    tcpsrv_send_reset(AF_INET, pvDstAddr, usDstPort, (in_addr_t *)&unCltIp, usCltPort, unPeerSeqNum + 1, &enErr);
                #endif
                    return; 
                }

        #if SUPPORT_PRINTF                
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif
                printf("onps_input_new_tcp_remote_client() failed, %s\r\n", onps_error(enErr));
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
        #endif
                return; 
            }            

            //* 截取tcp头部选项字段
            tcp_options_get(pstLink, pubPacket + sizeof(ST_TCP_HDR), nTcpHdrLen - (INT)sizeof(ST_TCP_HDR));

            //* 发送syn ack报文给对端
            pstLink->stPeer.unSeqNum = unPeerSeqNum + 1;
		#if SUPPORT_IPV6
			tcpsrv_send_syn_ack_with_start_timer(pstLink, pvDstAddr, usDstPort, (IPV4 == enProtocol) ? (void *)&uniCltIp.unVal : (void *)uniCltIp.pubVal, usCltPort);
		#else
            tcpsrv_send_syn_ack_with_start_timer(pstLink, pvDstAddr, usDstPort, (in_addr_t *)&unCltIp, usCltPort);
		#endif
        }
        else //* 已经申请，说明对端没收到syn ack，我们已经通过定时器在重复发送syn ack报文，所以这里直接发送一个应答但不开启定时器
        {
            //* 截取tcp头部选项字段
            tcp_options_get(pstLink, pubPacket + sizeof(ST_TCP_HDR), nTcpHdrLen - (INT)sizeof(ST_TCP_HDR)); 

            //* 发送syn ack报文给对端
            pstLink->stPeer.unSeqNum = unPeerSeqNum + 1;
        #if SUPPORT_IPV6
            tcpsrv_send_syn_ack(pstLink, pvDstAddr, usDstPort, (IPV4 == enProtocol) ? (void *)&uniCltIp.unVal : (void *)uniCltIp.pubVal, usCltPort, &enErr);
        #else
            tcpsrv_send_syn_ack(pstLink, pvDstAddr, usDstPort, (in_addr_t *)&unCltIp, usCltPort, &enErr); 
        #endif
        }                
    }
#endif
}

INT tcp_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, CHAR bRcvTimeout)
{
    EN_ONPSERR enErr;
    INT nRcvedBytes; 

    //* 读取数据
    nRcvedBytes = onps_input_recv_upper(nInput, pubDataBuf, unDataBufSize, NULL, NULL, &enErr);
	if (nRcvedBytes > 0)
	{    		
		if (bRcvTimeout > 0)
			onps_input_sem_pend(nInput, 1, NULL); //* 因为收到数据了，所以一定存在这个信号，所以这里主动消除该信号，确保用户端的延时准确
		return nRcvedBytes;
	}
    else
    {
        if(nRcvedBytes < 0)
            goto __lblErr;
    }

    //* 等待后
    if (bRcvTimeout)
    {
        CHAR bWaitSecs = bRcvTimeout;
        
__lblWaitRecv:         
        if (bRcvTimeout > 0)
        {			
            if (onps_input_sem_pend(nInput, 1, &enErr) < 0)            
                goto __lblErr;            

            bWaitSecs--; 			
        }
        else
        {
            if (onps_input_sem_pend(nInput, 0, &enErr) < 0)            
                goto __lblErr;             
        }
				
        //* 读取数据
        nRcvedBytes = onps_input_recv_upper(nInput, pubDataBuf, unDataBufSize, NULL, NULL, &enErr);		
        if (nRcvedBytes > 0)
            return nRcvedBytes;
        else
        {			
            if (nRcvedBytes < 0)
                goto __lblErr;

            if (bWaitSecs <= 0) 
                return 0; 
            goto __lblWaitRecv; 
        }
    }
    else
        return 0; 


__lblErr: 
    onps_set_last_error(nInput, enErr);
    return -1;
}

#if SUPPORT_SACK
static BOOL tcp_link_send_data(PST_TCPLINK pstLink)
{    
    //* 这一段代码能够成功运行的前提是tcp_link_ack_handler()函数必须与本函数放在同一个线程，且其在本函数之前，否则会出现因收到对应ack sequence num删除send timer导致系统宕机的问题
    //* 即使在这里加了tcp_send_timer_lock()线程锁，也无法保证得到的timer指针依然有效，所以必须同一线程且其在本函数之前执行
    STCB_TCPSENDTIMER **ppstcbSndTimer = (STCB_TCPSENDTIMER **)&pstLink->stcbSend.pstcbSndTimer;
    PSTCB_TCPSENDTIMER pstcbSndTimer = pstLink->stcbSend.pstcbSndTimer;        
    CHAR i; 
    for (i = 0; i < pstLink->stcbSend.bSendPacketNum; i++)
    {        
        ppstcbSndTimer = (STCB_TCPSENDTIMER **)&pstcbSndTimer->pstcbNextForLink;
        pstcbSndTimer = pstcbSndTimer->pstcbNextForLink; 
    }

    os_critical_init();
    
    //* 存在数据
    UINT unStartSeqNum = pstLink->stLocal.unHasSndBytes;
    if (uint_before(unStartSeqNum, pstLink->stcbSend.unWriteBytes))
    {            
        INT nPeerWndSize = -1;
        os_enter_critical();
        {            
            if (pstLink->stcbSend.bIsWndSizeUpdated)
            {
                nPeerWndSize = ((INT)pstLink->stPeer.usWndSize) * (pstLink->stPeer.bWndScale > 0 ? (INT)pow(2, pstLink->stPeer.bWndScale) : 1);
                if(nPeerWndSize > 0)
                    pstLink->stcbSend.bIsZeroWnd = FALSE;
                pstLink->stcbSend.unWndSize = (UINT)nPeerWndSize;
                pstLink->stcbSend.bIsWndSizeUpdated = FALSE; 
            }
        }
        os_exit_critical();

        //* 存在数据且对端接收窗口为0，则定期发送零窗口探测报文（固定RTO时长）           
        if (!nPeerWndSize)
        {
            if(pstLink->stcbSend.bIsZeroWnd)
            {    
                //* 超过RTO间隔，发送零窗口探测报文
                if (os_get_system_msecs() - pstLink->stcbSend.unLastSndZeroWndPktMSecs > RTO)
                {                    
                    tcp_send_data_ext(pstLink->stcbWaitAck.nInput, pstLink->stcbSend.pubSndBuf + (unStartSeqNum % TCPSNDBUF_SIZE), 1, unStartSeqNum + 1);
                    pstLink->stcbSend.unLastSndZeroWndPktMSecs = os_get_system_msecs();
                }
            }
            else
            {                
                pstLink->stcbSend.bIsZeroWnd = TRUE; 
                pstLink->stcbSend.unLastSndZeroWndPktMSecs = os_get_system_msecs();
            }            

            return TRUE; 
        }
        //else
        //    pstLink->stcbSend.bIsZeroWnd = FALSE; 
                        
        if (!pstLink->stcbSend.unWndSize)                    
            return TRUE;        

        /* *ppstcbSndTimer */pstcbSndTimer = tcp_send_timer_node_get();
        if (NULL != pstcbSndTimer)
        {             
            pstcbSndTimer->pstLink = pstLink; 
            
            //* 看看剩下还有多少数据要发送
            UINT unCpyBytes = pstLink->stcbSend.unWriteBytes - unStartSeqNum; 
            //* 再看看对端的mss能够接收多少数据
            unCpyBytes = unCpyBytes < (INT)pstLink->stPeer.usMSS ? unCpyBytes : (INT)pstLink->stPeer.usMSS;            
            //* 最后再看看对端的接收窗口是否足够大            
            unCpyBytes = unCpyBytes < pstLink->stcbSend.unWndSize ? unCpyBytes : pstLink->stcbSend.unWndSize;                       
            
            EN_ONPSERR enErr;
            UCHAR *pubData = (UCHAR *)buddy_alloc(unCpyBytes, &enErr);
            if (pubData)
            {
                pstcbSndTimer->unLeft = unStartSeqNum; 
                pstcbSndTimer->unRight = unStartSeqNum + unCpyBytes;

                UINT unStartReadIdx = pstcbSndTimer->unLeft % TCPSNDBUF_SIZE;
                UINT unEndReadIdx = pstcbSndTimer->unRight % TCPSNDBUF_SIZE;                

                if (unEndReadIdx > unStartReadIdx)
                    memcpy(pubData, pstLink->stcbSend.pubSndBuf + unStartReadIdx, unEndReadIdx - unStartReadIdx);
                else
                {                  
                    UINT unFirstCpyBytes = TCPSNDBUF_SIZE - unStartReadIdx;
                    memcpy(pubData, pstLink->stcbSend.pubSndBuf + unStartReadIdx, unFirstCpyBytes);
                    memcpy(pubData + unFirstCpyBytes, pstLink->stcbSend.pubSndBuf, unEndReadIdx);                    
                }                
                
                //* 计时并将其放入发送定时器队列
                pstcbSndTimer->unSendMSecs = os_get_system_msecs();
                pstcbSndTimer->usRto = RTO; 
				tcp_send_timer_lock();
				{
					*ppstcbSndTimer = pstcbSndTimer;
					tcp_send_timer_node_put_unsafe(pstcbSndTimer);
					pstLink->stcbSend.bSendPacketNum++;
				}                
				tcp_send_timer_unlock();

                //* 发送数据  
                tcp_send_data(pstLink->stcbWaitAck.nInput, pubData, unCpyBytes, 0);
                
                buddy_free(pubData);  
                pstLink->stcbSend.unWndSize -= unCpyBytes;                
            }
            else
            {
        #if SUPPORT_PRINTF && DEBUG_LEVEL
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif
                printf("tcp_link_send_data() failed, %s\r\n", onps_error(enErr));
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
        #endif

                tcp_send_timer_node_free(pstcbSndTimer);
            }
        }
        else
        {
    #if SUPPORT_PRINTF && DEBUG_LEVEL
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("The tcp sending timer queue is empty.\r\n");
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif            
        }
        return TRUE; 
    }
    
    return FALSE; //* 没有要发送的数据了，返回FALSE            
}

void thread_tcp_handler(void *pvParam)
{
    INT nRtnVal; 
    BOOL blIsExistData; 
    PST_TCPLINK pstSndDataLink;     

    while (TRUE)
    {
        //* 等待信号到来
        nRtnVal = tcp_send_sem_pend(1);
        if (nRtnVal)
        {
            if (nRtnVal < 0)
            {
    #if SUPPORT_PRINTF && DEBUG_LEVEL
        #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
        #endif
                printf("tcp_send_sem_pend() failed, %s\r\n", onps_error(ERRINVALIDSEM));
        #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
                os_sleep_secs(1);

                continue;
            }            
        }

        blIsExistData = FALSE;
        pstSndDataLink = NULL;

__lblSend:         
        //* 遍历所有tcp链路处理发送相关的逻辑        
        pstSndDataLink = tcp_link_for_send_data_get_next(pstSndDataLink); 
        if (pstSndDataLink)
        {
            //blIsExistData = TRUE; 

            //* 先看看状态是否还是CONNECTED状态
            if (pstSndDataLink->bState != TLSCONNECTED)
                goto __lblDelNode; 

            //* 先看看是否触发了快速重传（Fast Retransmit）机制或者存在sack选项吗？这两个有其一就应当重发数据
            //* 2022-12-17 下午开始发烧（这之后4天左右开始喉咙巨痛），2022-12-29基本康复，继续当前算法
            //* 将其转移到接收线程，一旦收到dup ack或sack直接回馈对应报文，而不是在这里收到信号后再去处理
            
            //* 看看ack序号到哪里了
            tcp_link_ack_handler(pstSndDataLink);
            
            //* 发送数据
            if (pstSndDataLink->stcbSend.bSendPacketNum < 4)
            {
                if(tcp_link_send_data(pstSndDataLink))
                    blIsExistData = TRUE;
            }                                       

            goto __lblSend;

__lblDelNode: 
            //* 直接从数据队列中删除
            tcp_link_for_send_data_del(pstSndDataLink);
            pstSndDataLink = NULL; //* 重新开始取发送节点
            blIsExistData = FALSE; 
            goto __lblSend; 
        }
        else //* 到尾部了或者没有要发送数据的tcp链路了
        {            
            if (blIsExistData) //* 到尾部了
            {
                blIsExistData = FALSE; 
                goto __lblSend; 
            }
            else; //* 回到头部，等待下一个信号或者超时后再检查数据发送队列
        }
    }
}
#endif

#if SUPPORT_IPV6
INT tcpv6_send_syn(INT nInput, UCHAR ubaSrvAddr[16], USHORT usSrvPort, int nConnTimeout)
{
	EN_ONPSERR enErr;

	//* 获取链路信息存储节点
	PST_TCPLINK pstLink;
	if (!onps_input_get(nInput, IOPT_GETTCPUDPLINK, &pstLink, &enErr))
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

	//* 先寻址，因为tcp校验和计算需要用到本地地址，同时当前tcp链路句柄也需要用此标识
	if (NULL == route_ipv6_get_source_ip(ubaSrvAddr, pstHandle->stSockAddr.saddr_ipv6))
	{
		onps_set_last_error(nInput, ERRADDRESSING);
		return -1;
	}
	pstHandle->stSockAddr.usPort = onps_input_port_new(AF_INET, IPPROTO_TCP);

	//* 标志字段syn域置1，其它标志域为0
	UNI_TCP_FLAG uniFlag;
	uniFlag.usVal = 0;
	uniFlag.stb16.syn = 1;

	//* 填充tcp头部选项数据
	UCHAR ubaOptions[TCP_OPTIONS_SIZE_MAX];
	INT nOptionsSize = tcp_options_attach(ubaOptions, sizeof(ubaOptions));

	//* 加入定时器队列
	pstLink->stcbWaitAck.bRcvTimeout = nConnTimeout;
	pstLink->stcbWaitAck.bIsAcked = FALSE;
	pstLink->stcbWaitAck.pstTimer = one_shot_timer_new(tcp_ack_timeout_handler, pstLink, nConnTimeout ? nConnTimeout : TCP_CONN_TIMEOUT);
	if (!pstLink->stcbWaitAck.pstTimer)
	{
		onps_set_last_error(nInput, ERRNOIDLETIMER);
		return -1;
	}

	//* 完成实际的发送
	pstLink->stLocal.unSeqNum = 0;
	pstLink->bState = TLSSYNSENT;
	INT nRtnVal = tcpv6_send_packet(pstLink, pstHandle->stSockAddr.saddr_ipv6, pstHandle->stSockAddr.usPort, ubaSrvAddr, usSrvPort, uniFlag, ubaOptions, (USHORT)nOptionsSize, NULL, 0,
#if SUPPORT_SACK
		TRUE, 0,
#endif        
		&enErr);
	if (nRtnVal <= 0)
	{
		pstLink->bState = TLSINIT;
		one_shot_timer_safe_free(pstLink->stcbWaitAck.pstTimer);

		if (nRtnVal < 0)
			onps_set_last_error(nInput, enErr);
		else
			onps_set_last_error(nInput, ERRSENDZEROBYTES);
	}

	return nRtnVal;
}

void tcpv6_send_ack(PST_TCPLINK pstLink, UCHAR ubaSrcAddr[16], USHORT usSrcPort, UCHAR ubaDstAddr[16], USHORT usDstPort)
{
	//* 标志字段ack域置1，其它标志域为0
	UNI_TCP_FLAG uniFlag;
	uniFlag.usVal = 0;
	uniFlag.stb16.ack = 1;
	pstLink->stPeer.bIsNotAcked = FALSE;

	//* 发送应答报文 
#if SUPPORT_SACK
	tcpv6_send_packet(pstLink, ubaSrcAddr, usSrcPort, ubaDstAddr, usDstPort, uniFlag, NULL, 0, NULL, 0, FALSE, 0, NULL);
#else
	tcpv6_send_packet(pstLink, ubaSrcAddr, usSrcPort, ubaDstAddr, usDstPort, uniFlag, NULL, 0, NULL, 0, NULL);
#endif
}
#endif
