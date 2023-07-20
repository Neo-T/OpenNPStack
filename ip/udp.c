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
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "onps_input.h"
#define SYMBOL_GLOBALS
#include "ip/udp.h"
#include "ip/udp_frame.h" 
#undef SYMBOL_GLOBALS

#if SUPPORT_IPV6 && SUPPORT_ETHERNET
#include "ethernet/dhcpv6.h"
#endif

#if SUPPORT_IPV6
static INT udp_send_packet(CHAR bFamily, void *pvSrcAddr, USHORT usSrcPort, void *pvDstAddr, USHORT usDstPort, UCHAR *pubData, INT nDataLen, EN_ONPSERR *penErr)
#else
static INT udp_send_packet(in_addr_t unSrcAddr, USHORT usSrcPort, in_addr_t unDstAddr, USHORT usDstPort, UCHAR *pubData, INT nDataLen, EN_ONPSERR *penErr)
#endif
{
#if SUPPORT_IPV6	
	//* 仅支持ipv4/ipv6地址族
	if (AF_INET != bFamily && AF_INET6 != bFamily)
	{
		if (penErr)
			*penErr = ERRUNSUPPORTEDFAMILY;
		return -1;
	}
#endif

    //* 挂载用户数据
    SHORT sBufListHead = -1;
    SHORT sDataNode = -1;
    if (pubData)
    {
        sDataNode = buf_list_get_ext(pubData, (UINT)nDataLen, penErr);
        if (sDataNode < 0)
            return -1;
        buf_list_put_head(&sBufListHead, sDataNode);
    }

    //* 挂载头部数据
    ST_UDP_HDR stHdr;   
#if SUPPORT_IPV6
    stHdr.usSrcPort = htons(usSrcPort);
    stHdr.usDstPort = htons(usDstPort); 
#else
	stHdr.usSrcPort = htons(usSrcPort);
	stHdr.usDstPort = htons(usDstPort);
#endif
    stHdr.usPacketLen = htons(sizeof(ST_UDP_HDR) + nDataLen); 
    stHdr.usChecksum = 0; 

    //* 挂载到链表头部
    SHORT sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (UINT)sizeof(ST_UDP_HDR), penErr);
    if (sHdrNode < 0)
    {
        if (sDataNode >= 0)
            buf_list_free(sDataNode);
        return -1;
    }
    buf_list_put_head(&sBufListHead, sHdrNode);
			
	//* 计算校验和
	EN_ONPSERR enErr = ERRNO; 
#if SUPPORT_IPV6
	if (AF_INET == bFamily)
		stHdr.usChecksum = tcpip_checksum_ipv4(*(in_addr_t *)pvSrcAddr, *(in_addr_t *)pvDstAddr, (USHORT)(sizeof(ST_UDP_HDR) + nDataLen), IPPROTO_UDP, sBufListHead, &enErr);
	else
		stHdr.usChecksum = tcpip_checksum_ipv6((UCHAR *)pvSrcAddr, (UCHAR *)pvDstAddr, (UINT)(sizeof(ST_UDP_HDR) + nDataLen), IPPROTO_UDP, sBufListHead, &enErr);
#else
	stHdr.usChecksum = tcpip_checksum_ipv4(unSrcAddr, unDstAddr, (USHORT)(sizeof(ST_UDP_HDR) + nDataLen), IPPROTO_UDP, sBufListHead, &enErr);
#endif	
    if (ERRNO != enErr)
    {
        if (sDataNode >= 0)
            buf_list_free(sDataNode); 
        buf_list_free(sHdrNode);

		if (penErr)
			*penErr = enErr; 

        return -1;
    }

    //* 如果校验和为0，根据协议要求必须反转为0xFFFF
    if (0 == stHdr.usChecksum)
        stHdr.usChecksum = 0xFFFF; 

    //* 发送之
#if SUPPORT_IPV6
	INT nRtnVal; 
	if (AF_INET == bFamily)
		nRtnVal = ip_send_ext(*(in_addr_t *)pvSrcAddr, *(in_addr_t *)pvDstAddr, UDP, IP_TTL_DEFAULT, sBufListHead, penErr);
	else
	{
		UINT unFlowLabel = ipv6_flow_label_cal((UCHAR *)pvDstAddr, (UCHAR *)pvSrcAddr, IPPROTO_UDP, usDstPort, usSrcPort);
		nRtnVal = ipv6_send_ext((UCHAR *)pvSrcAddr, (UCHAR *)pvDstAddr, IPPROTO_UDP, sBufListHead, unFlowLabel, penErr);
	}
#else
    INT nRtnVal = ip_send_ext(unSrcAddr, unDstAddr, UDP, IP_TTL_DEFAULT, sBufListHead, penErr);
#endif

    //* 释放刚才申请的buf list节点
    if (sDataNode >= 0)
        buf_list_free(sDataNode);    
    buf_list_free(sHdrNode);

    return nRtnVal;
}

INT udp_send(INT nInput, UCHAR *pubData, INT nDataLen)
{
    EN_ONPSERR enErr;
    
    //* 获取链路信息存储节点
    PST_UDPLINK pstLink;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPLINK, &pstLink, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

    //* 链路为空或端口为0意味着当前udp通讯尚未绑定对端服务器地址，那就不能采用未指定地址的udp_send()函数发送udp报文
    if (pstLink == NULL || /*pstLink->stPeerAddr.unIp == 0 || */pstLink->stPeerAddr.usPort == 0)
    {
        onps_set_last_error(nInput, ERRSENDADDR);
        return -1; 
    }

    //* 获取udp链路句柄访问地址，该地址保存当前udp链路由协议栈自动分配的端口及本地网络接口地址
    PST_TCPUDP_HANDLE pstHandle;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

#if SUPPORT_IPV6
	//* 尚未分配本地地址
	if (pstHandle->stSockAddr.usPort == 0)	
	{
		if (AF_INET == pstHandle->bFamily)
		{
			//* 寻址，看看使用哪个netif
			UINT unNetifIp = route_get_netif_ip(pstLink->stPeerAddr.saddr_ipv4);
			if (!unNetifIp)
			{				
				onps_set_last_error(nInput, ERRADDRESSING);
				return -1;
			}
			//* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
			pstHandle->stSockAddr.saddr_ipv4 = unNetifIp;
			pstHandle->stSockAddr.usPort = onps_input_port_new(AF_INET, IPPROTO_UDP);
		}
		else
		{
			if (NULL == route_ipv6_get_source_ip(pstLink->stPeerAddr.saddr_ipv6, pstHandle->stSockAddr.saddr_ipv6))
			{				
				onps_set_last_error(nInput, ERRADDRESSING);
				return -1;
			}

			pstHandle->stSockAddr.usPort = onps_input_port_new(AF_INET6, IPPROTO_UDP);
		}
	}	
#else
    //* 尚未分配本地地址
    if (pstHandle->stSockAddr.saddr_ipv4 == 0 && pstHandle->stSockAddr.usPort == 0)
    {
        //* 寻址，看看使用哪个netif
        UINT unNetifIp = route_get_netif_ip(pstLink->stPeerAddr.saddr_ipv4);
        if (!unNetifIp)
        {
            onps_set_last_error(nInput, ERRADDRESSING);
            return -1;
        }
        //* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
        pstHandle->stSockAddr.saddr_ipv4 = unNetifIp;
        pstHandle->stSockAddr.usPort = onps_input_port_new(IPPROTO_UDP);
    }
#endif

#if SUPPORT_IPV6
	//* 获取流标签
	INT nRtnVal; 
	if(AF_INET == pstHandle->bFamily)
		nRtnVal = udp_send_packet(pstHandle->bFamily, &pstHandle->stSockAddr.saddr_ipv4, pstHandle->stSockAddr.usPort, &pstLink->stPeerAddr.saddr_ipv4, pstLink->stPeerAddr.usPort, pubData, nDataLen, &enErr);
	else
		nRtnVal = udp_send_packet(pstHandle->bFamily, pstHandle->stSockAddr.saddr_ipv6, pstHandle->stSockAddr.usPort, pstLink->stPeerAddr.saddr_ipv6, pstLink->stPeerAddr.usPort, pubData, nDataLen, &enErr);
#else
    INT nRtnVal = udp_send_packet(pstHandle->stSockAddr.saddr_ipv4, pstHandle->stSockAddr.usPort, pstLink->stPeerAddr.saddr_ipv4, pstLink->stPeerAddr.usPort, pubData, nDataLen, &enErr);
#endif
    if (nRtnVal > 0)
        return nDataLen;
    else
    {
        if (nRtnVal < 0)
            onps_set_last_error(nInput, enErr);

        return nRtnVal;
    }
}

INT udp_send_ext(INT nInput, SHORT sBufListHead, in_addr_t unDstIp, USHORT usDstPort, in_addr_t unSrcIp, PST_NETIF pstNetif, EN_ONPSERR *penErr)
{
    //* 获取udp链路句柄访问地址，该地址保存当前udp链路由协议栈自动分配或用户手动设置的端口及本地网络接口地址
    PST_TCPUDP_HANDLE pstHandle;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, penErr))            
        return -1;
    
    //* 这个函数用于udp服务器发送，端口号在初始设置阶段就应该由用户分配才对，所以这里不应该为0
    if (pstHandle->stSockAddr.usPort == 0)
    {
        if (penErr)
            *penErr = ERRPORTEMPTY;
        return -1; 
    }

    //* 挂载udp报文头部数据
    USHORT usDataLen = (USHORT)buf_list_get_len(sBufListHead); 
    ST_UDP_HDR stHdr;
    stHdr.usSrcPort = htons(pstHandle->stSockAddr.usPort);
    stHdr.usDstPort = htons(usDstPort);
    stHdr.usPacketLen = htons(sizeof(ST_UDP_HDR) + usDataLen);
    stHdr.usChecksum = 0;

    //* 挂载到链表头部
    SHORT sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (UINT)sizeof(ST_UDP_HDR), penErr);
    if (sHdrNode < 0)            
        return -1;     
    buf_list_put_head(&sBufListHead, sHdrNode); 

	//* 计算校验和
	EN_ONPSERR enErr = ERRNO;
	stHdr.usChecksum = tcpip_checksum_ipv4(unSrcIp, unDstIp, (USHORT)(sizeof(ST_UDP_HDR) + usDataLen), IPPROTO_UDP, sBufListHead, &enErr); 
    if (ERRNO != enErr)
    {        
        buf_list_free(sHdrNode);
		if (*penErr)
			*penErr = enErr; 

        return -1;
    }

    //* 如果校验和为0，根据协议要求必须反转为0xFFFF
    if (0 == stHdr.usChecksum)
        stHdr.usChecksum = 0xFFFF;

    //* 发送之    
    INT nRtnVal = ip_send(pstNetif, NULL, unSrcIp, unDstIp, UDP, IP_TTL_DEFAULT, sBufListHead, penErr);

    //* 释放刚才申请的buf list节点    
    buf_list_free(sHdrNode);

    return nRtnVal;
}

INT udp_sendto(INT nInput, in_addr_t unDstIP, USHORT usDstPort, UCHAR *pubData, INT nDataLen)
{
    EN_ONPSERR enErr;

    //* 获取udp链路句柄访问地址，该地址保存当前udp链路由协议栈自动分配的端口及本地网络接口地址
    PST_TCPUDP_HANDLE pstHandle;
    if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
    {
        onps_set_last_error(nInput, enErr);
        return -1;
    }

    //* 端口号为0意味着尚未分配本地地址(端口号是不允许为0的)
    if (/*pstHandle->stSockAddr.saddr_ipv4 == 0 && */pstHandle->stSockAddr.usPort == 0)
    {
        //* 寻址，看看使用哪个neiif
        UINT unNetifIp = route_get_netif_ip(unDstIP);
        if (!unNetifIp)
        {
            onps_set_last_error(nInput, ERRADDRESSING);
            return -1;
        }
        //* 更新当前input句柄，以便收到应答报文时能够准确找到该链路
        pstHandle->stSockAddr.saddr_ipv4 = unNetifIp;
#if SUPPORT_IPV6
        pstHandle->stSockAddr.usPort = onps_input_port_new(AF_INET, IPPROTO_UDP);
#else
		pstHandle->stSockAddr.usPort = onps_input_port_new(IPPROTO_UDP);
#endif
    }

#if SUPPORT_IPV6	
    INT nRtnVal = udp_send_packet(AF_INET, &pstHandle->stSockAddr.saddr_ipv4, pstHandle->stSockAddr.usPort, &unDstIP, usDstPort, pubData, nDataLen, &enErr);
#else
	INT nRtnVal = udp_send_packet(pstHandle->stSockAddr.saddr_ipv4, pstHandle->stSockAddr.usPort, unDstIP, usDstPort, pubData, nDataLen, &enErr);
#endif
    if (nRtnVal > 0)
        return nDataLen;
    else
    {
        if(nRtnVal < 0)
            onps_set_last_error(nInput, enErr); 
        return nRtnVal;
    }
}

void udp_recv(in_addr_t unSrcAddr, in_addr_t unDstAddr, UCHAR *pubPacket, INT nPacketLen)
{
    EN_ONPSERR enErr = ERRNO;
    PST_UDP_HDR pstHdr = (PST_UDP_HDR)pubPacket; 
    
    //* 如果校验和为0则意味着不需要计算校验和，反之就需要进行校验计算
    if (pstHdr->usChecksum)
    {
        //* 计算校验和
        USHORT usPktChecksum = pstHdr->usChecksum;
        pstHdr->usChecksum = 0;		
		USHORT usChecksum = tcpip_checksum_ipv4_ext(unSrcAddr, unDstAddr, IPPROTO_UDP, pubPacket, (USHORT)nPacketLen, &enErr);
		if (ERRNO != enErr)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("tcpip_checksum_ipv4_ext() failed, %s, the udp packet will be dropped\r\n", onps_error(enErr));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif			
			return;
		}                  
		//* 如果计算结果为0，则校验和反转
        if(0 == usChecksum)
            usChecksum = 0xFFFF; 

		//* 判断校验和是否正确
        if (usPktChecksum != usChecksum)
        {
    #if SUPPORT_PRINTF && DEBUG_LEVEL > 3
            pstHdr->usChecksum = usPktChecksum;
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("checksum error (%04X, %04X), the udp packet will be dropped\r\n", usChecksum, usPktChecksum);
            printf_hex(pubPacket, nPacketLen, 48);
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
#endif
            return;
        }
    }

    //* 校验和计算通过，则查找当前udp链路是否存在
    USHORT usSrcPort = htons(pstHdr->usSrcPort); 
    USHORT usDstPort = htons(pstHdr->usDstPort);
    PST_UDPLINK pstLink;
#if SUPPORT_IPV6
	INT nInput = onps_input_get_handle(AF_INET, IPPROTO_UDP, &unDstAddr, usDstPort, &pstLink);
#else
    INT nInput = onps_input_get_handle(IPPROTO_UDP, unDstAddr, usDstPort, &pstLink);
#endif
    if (nInput < 0)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
        UCHAR *pubAddr = (UCHAR *)&unDstAddr;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif        
        printf("The udp link of %d.%d.%d.%d:%d isn't found, the packet will be dropped\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3], usDstPort);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return;
    }

    //* 如果当前链路存在附加信息，意味着这是一个已经与udp服务器绑定的链路，需要判断源地址是否也匹配，不匹配的直接丢弃
    in_addr_t unFromIP = unSrcAddr; 
    if (pstLink)
    {
        if (pstLink->stPeerAddr.saddr_ipv4 != unFromIP || pstLink->stPeerAddr.usPort != usSrcPort)
        {
    #if SUPPORT_PRINTF && DEBUG_LEVEL > 3
            UCHAR *pubFromAddr = (UCHAR *)&unSrcAddr;
            UCHAR *pubSrvAddr = (UCHAR *)&pstLink->stPeerAddr.saddr_ipv4;
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif        
            printf("udp packet from address %d.%d.%d.%d:%d are not allowed (connected to udp server %d.%d.%d.%d:%d), the packet will be dropped\r\n", pubFromAddr[0], pubFromAddr[1], pubFromAddr[2], pubFromAddr[3], usSrcPort, pubSrvAddr[0], pubSrvAddr[1], pubSrvAddr[2], pubSrvAddr[3], pstLink->stPeerAddr.usPort);
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
            return;
        }
    }

    //* 看看有数据吗？            
    INT nDataLen = nPacketLen - sizeof(ST_UDP_HDR);
    if (nDataLen)
    {
        //* 将数据搬运到input层
	#if SUPPORT_IPV6
		if (!onps_input_recv(nInput, (const UCHAR *)(pubPacket + sizeof(ST_UDP_HDR)), nDataLen, &unFromIP, usSrcPort, &enErr))
	#else
        if (!onps_input_recv(nInput, (const UCHAR *)(pubPacket + sizeof(ST_UDP_HDR)), nDataLen, unFromIP, usSrcPort, &enErr))
	#endif
        {
    #if SUPPORT_PRINTF && DEBUG_LEVEL
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("onps_input_recv() failed, %s, the udp packet will be dropped\r\n", onps_error(enErr));
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif            
        }
    }    
}

INT udp_recv_upper(INT nInput, UCHAR *pubDataBuf, UINT unDataBufSize, void *pvFromIP, USHORT *pusFromPort, CHAR bRcvTimeout)
{
    EN_ONPSERR enErr;
    INT nRcvedBytes;

    //* 读取数据
    nRcvedBytes = onps_input_recv_upper(nInput, pubDataBuf, unDataBufSize, pvFromIP, pusFromPort, &enErr);
    if (nRcvedBytes > 0)
    {
        if (bRcvTimeout > 0)
            onps_input_sem_pend(nInput, 1, NULL); //* 因为收到数据了，所以一定存在这个信号，所以这里主动消除该信号，确保用户端的延时准确
        return nRcvedBytes;
    }
    else
    {
        if (nRcvedBytes < 0)
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
        nRcvedBytes = onps_input_recv_upper(nInput, pubDataBuf, unDataBufSize, pvFromIP, pusFromPort, &enErr);
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

#if SUPPORT_IPV6
INT ipv6_udp_send_ext(INT nInput, SHORT sBufListHead, UCHAR ubaDstAddr[16], USHORT usDstPort, UCHAR ubaSrcAddr[16], PST_NETIF pstNetif, EN_ONPSERR *penErr)
{
	//* 获取udp链路句柄访问地址，该地址保存当前udp链路由协议栈自动分配的端口及本地网络接口地址
	PST_TCPUDP_HANDLE pstHandle;
	if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, penErr)) 
		return -1;

	if (AF_INET6 != pstHandle->bFamily)
	{
		if (penErr)
			*penErr = ERRUNSUPPORTEDFAMILY;

		return -1;
	}

	//* 挂载udp报文头部数据
	USHORT usDataLen = (USHORT)buf_list_get_len(sBufListHead);
	ST_UDP_HDR stHdr;
	stHdr.usSrcPort = htons(pstHandle->stSockAddr.usPort);
	stHdr.usDstPort = htons(usDstPort);
	stHdr.usPacketLen = htons(sizeof(ST_UDP_HDR) + usDataLen);
	stHdr.usChecksum = 0;

	//* 挂载到链表头部
	SHORT sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (UINT)sizeof(ST_UDP_HDR), penErr);
	if (sHdrNode < 0)
		return -1;
	buf_list_put_head(&sBufListHead, sHdrNode);

	//* 计算校验和
	EN_ONPSERR enErr = ERRNO;
	stHdr.usChecksum = tcpip_checksum_ipv6(ubaSrcAddr, ubaDstAddr, (UINT)(sizeof(ST_UDP_HDR) + usDataLen), IPPROTO_UDP, sBufListHead, &enErr); 
	if (ERRNO != enErr)
	{
		buf_list_free(sHdrNode);
		if (*penErr)
			*penErr = enErr;

		return -1;
	}

	//* 如果校验和为0，根据协议要求必须反转为0xFFFF
	if (0 == stHdr.usChecksum)
		stHdr.usChecksum = 0xFFFF;

	//* 发送之    
	UINT unFlowLabel = ipv6_flow_label_cal(ubaDstAddr, ubaSrcAddr, IPPROTO_UDP, usDstPort, pstHandle->stSockAddr.usPort);
	INT nRtnVal = ipv6_send(pstNetif, NULL, ubaSrcAddr, ubaDstAddr, IPPROTO_UDP, sBufListHead, unFlowLabel, penErr); 

	//* 释放刚才申请的buf list节点    
	buf_list_free(sHdrNode);

	return nRtnVal;
}

INT ipv6_udp_sendto(INT nInput, const UCHAR ubaDstAddr[16], USHORT usDstPort, UCHAR *pubData, INT nDataLen)
{
	EN_ONPSERR enErr;

	//* 获取udp链路句柄访问地址，该地址保存当前udp链路由协议栈自动分配的端口及本地网络接口地址
	PST_TCPUDP_HANDLE pstHandle;
	if (!onps_input_get(nInput, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
	{
		onps_set_last_error(nInput, enErr);
		return -1;
	}

	//* 端口号为0意味着尚未分配本地地址(端口号是不允许为0的)
	if (pstHandle->stSockAddr.usPort == 0)
	{
		//* 更新当前input句柄，以便收到应答报文时能够准确找到该链路，首先寻址确定源地址
		if (NULL == route_ipv6_get_source_ip(ubaDstAddr, pstHandle->stSockAddr.saddr_ipv6))
		{
			onps_set_last_error(nInput, ERRADDRESSING);
			return -1;
		}

		pstHandle->stSockAddr.usPort = onps_input_port_new(AF_INET6, IPPROTO_UDP);
	}

	//* 发送
	INT nRtnVal = udp_send_packet(AF_INET6, pstHandle->stSockAddr.saddr_ipv6, pstHandle->stSockAddr.usPort, (void *)ubaDstAddr, usDstPort, pubData, nDataLen, &enErr);
	if (nRtnVal > 0)
		return nDataLen;
	else
	{
		if (nRtnVal < 0)
			onps_set_last_error(nInput, enErr);
		return nRtnVal;
	}
}

void ipv6_udp_recv(PST_NETIF pstNetif, UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UCHAR *pubPacket, INT nPacketLen)
{
	EN_ONPSERR enErr = ERRNO;
	PST_UDP_HDR pstHdr = (PST_UDP_HDR)pubPacket;

	//* 如果校验和为0则意味着不需要计算校验和，反之就需要进行校验计算
	if (pstHdr->usChecksum)
	{
		//* 计算校验和
		USHORT usPktChecksum = pstHdr->usChecksum;
		pstHdr->usChecksum = 0;
		USHORT usChecksum = tcpip_checksum_ipv6_ext(ubaSrcAddr, ubaDstAddr, IPPROTO_UDP, pubPacket, (UINT)nPacketLen, &enErr); 
		if (ERRNO != enErr)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("tcpip_checksum_ipv6() failed, %s, the udp packet will be dropped\r\n", onps_error(enErr));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif			
			return;
		}
		//* 如果计算结果为0，则校验和反转
		if (0 == usChecksum) 
			usChecksum = 0xFFFF;

		//* 判断校验和是否正确
		if (usPktChecksum != usChecksum)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
			pstHdr->usChecksum = usPktChecksum;
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("checksum error (%04X, %04X), the udp packet will be dropped\r\n", usChecksum, usPktChecksum);
			printf_hex(pubPacket, nPacketLen, 48);
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
			return;
		}
	}

	//* 校验和计算通过，则查找当前udp链路是否存在
	USHORT usSrcPort = htons(pstHdr->usSrcPort);
	USHORT usDstPort = htons(pstHdr->usDstPort);
	PST_UDPLINK pstLink;
	INT nInput = onps_input_get_handle(AF_INET6, IPPROTO_UDP, ubaDstAddr, usDstPort, &pstLink);
	if (nInput < 0)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
		CHAR szIpv6[40]; 
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif        
		printf("The udp link of [%s]:%d isn't found, the packet will be dropped\r\n", inet6_ntoa(ubaDstAddr, szIpv6), usDstPort);
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
		return;
	}

	//* 如果当前链路存在附加信息，意味着这是一个已经与udp服务器绑定的链路，需要判断源地址是否也匹配，不匹配的直接丢弃	
	if (pstLink)
	{		
		if (memcmp(ubaSrcAddr, pstLink->stPeerAddr.saddr_ipv6, 16) || pstLink->stPeerAddr.usPort != usSrcPort)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL// > 3
			CHAR szIpv6[40];			
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif        
			printf("udp packet from address [%s]:%d are not allowed (connected to udp server", inet6_ntoa(ubaSrcAddr, szIpv6), usSrcPort);
			printf(" [%s]:%d), the packet will be dropped\r\n", inet6_ntoa(pstLink->stPeerAddr.saddr_ipv6, szIpv6), pstLink->stPeerAddr.usPort);
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
			return;
		}
	}

	//* 看看有数据吗？            
	INT nDataLen = nPacketLen - sizeof(ST_UDP_HDR);
	if (nDataLen)
	{
		//* 首先看看端口号是否是DHCPv6的，如果是，则上传给专门的DHCPv6接收函数
		if (DHCPv6_CLT_PORT == usDstPort && DHCPv6_SRV_PORT == usSrcPort)
		{
			dhcpv6_recv(pstNetif, ubaSrcAddr, ubaDstAddr, (UCHAR *)pubPacket + sizeof(ST_UDP_HDR), (USHORT)nDataLen); 
			return; 
		}

		//* 将数据搬运到input层	
		if (!onps_input_recv(nInput, (const UCHAR *)(pubPacket + sizeof(ST_UDP_HDR)), nDataLen, ubaSrcAddr, usSrcPort, &enErr))
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("onps_input_recv() failed, %s, the udp packet will be dropped\r\n", onps_error(enErr));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif            
		}
	}
}
#endif

