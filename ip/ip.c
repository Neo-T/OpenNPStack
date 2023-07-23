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
#include "ethernet/arp.h"

#define SYMBOL_GLOBALS
#include "ip/ip.h"
#undef SYMBOL_GLOBALS
#include "ethernet/ethernet.h"
#include "ip/icmp.h"
#if SUPPORT_IPV6
#include "ip/icmpv6.h"
#include "ip/ipv6_configure.h"
#endif
#include "ip/tcp.h"
#include "ip/udp.h"

//* 必须严格按照EN_NPSPROTOCOL类型定义的顺序指定IP上层协议值
static const EN_IPPROTO lr_enaIPProto[] = {
    IPPROTO_MAX,
    IPPROTO_MAX,
    IPPROTO_MAX,
    IPPROTO_MAX,
#if SUPPORT_IPV6
    IPPROTO_MAX,
#endif
    IPPROTO_MAX,
#if SUPPORT_IPV6
    IPPROTO_MAX,
#endif
    IPPROTO_ICMP, 
#if SUPPORT_IPV6
	IPPROTO_ICMPv6,
#endif
    IPPROTO_MAX,
    IPPROTO_TCP, 
    IPPROTO_UDP
};
static USHORT l_usIPIdentifier = 0; 

static INT netif_ip_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, in_addr_t unSrcAddr, in_addr_t unDstAddr, in_addr_t unArpDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr)
{
    INT nRtnVal; 
    BOOL blNetifFreedEn = TRUE; 

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
    stHdr.ubProto = (UCHAR)lr_enaIPProto[enProtocol];
    stHdr.usChecksum = 0;
    stHdr.unSrcIP = unSrcAddr/*pstNetif->stIPv4.unAddr*/;
    stHdr.unDstIP = unDstAddr;

    //* 挂载到buf list头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (USHORT)sizeof(ST_IP_HDR), penErr);
    if (sHdrNode < 0)
    {
        //* 使用计数减一，释放对网卡资源的占用
        netif_freed(pstNetif);
        return -1;
    }
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 计算校验和
    stHdr.usChecksum = tcpip_checksum((USHORT *)&stHdr, sizeof(ST_IP_HDR))/*tcpip_checksum_ext(sBufListHead)*/;

#if SUPPORT_ETHERNET
    //* 看看选择的网卡是否是ethernet类型，如果是则首先需要在此获取目标mac地址
    if (NIF_ETHERNET == pstNetif->enType)
    {
		if (pubDstMacAddr)
		{
			PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
			if (pubDstMacAddr != pstExtra->ubaMacAddr)
				nRtnVal = pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, pubDstMacAddr, penErr);
			else
				nRtnVal = ethernet_loopback_put_packet(pstNetif, sBufListHead, LPPROTO_IP); 
		}        
		else
		{
			UCHAR ubaDstMac[ETH_MAC_ADDR_LEN];            
			nRtnVal = arp_get_mac_ext(pstNetif, unSrcAddr, unArpDstAddr, ubaDstMac, sBufListHead, &blNetifFreedEn, penErr); 
			if (!nRtnVal) //* 存在该条目，则直接调用ethernet接口注册的发送函数即可			
			{
				PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
				if (memcmp(ubaDstMac, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN))
					nRtnVal = pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, ubaDstMac, penErr);
				else
					nRtnVal = ethernet_loopback_put_packet(pstNetif, sBufListHead, LPPROTO_IP); 
			}
		}        
    }
    else
    {
#endif
        //* 完成发送
        nRtnVal = pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, NULL, penErr);
#if SUPPORT_ETHERNET
    }    
#endif

    //* 如果不需要等待arp查询结果，则立即释放对网卡的使用权
    if (blNetifFreedEn)
        netif_freed(pstNetif); 

    //* 释放刚才申请的buf list节点
    buf_list_free(sHdrNode);

    return nRtnVal;
}

INT ip_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, in_addr_t unSrcAddr, in_addr_t unDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr)
{
	in_addr_t unSrcAddrUsed = unSrcAddr, unArpDstAddr = unDstAddr;

	//* 如果未指定netif则通过路由表选择一个适合的netif
	PST_NETIF pstNetifUsed = pstNetif; 
    if (pstNetifUsed)
		netif_used(pstNetifUsed);
	else
    {
        pstNetifUsed = route_get_netif(unDstAddr, TRUE, &unSrcAddrUsed, &unArpDstAddr);
        if (NULL == pstNetifUsed)
        {
            if (penErr)
                *penErr = ERRADDRESSING;

            return -1;
        }
    }
        
    return netif_ip_send(pstNetifUsed, pubDstMacAddr, unSrcAddrUsed, unDstAddr, unArpDstAddr, enProtocol, ubTTL, sBufListHead, penErr);
}

INT ip_send_ext(in_addr_t unSrcAddr, in_addr_t unDstAddr, EN_NPSPROTOCOL enProtocol, UCHAR ubTTL, SHORT sBufListHead, EN_ONPSERR *penErr)
{
    in_addr_t unRouteSrcAddr, unArpDstAddr = unDstAddr;

    PST_NETIF pstNetif = route_get_netif(unDstAddr, TRUE, &unRouteSrcAddr, &unArpDstAddr);
    if (NULL == pstNetif)
    {
        if (penErr)
            *penErr = ERRADDRESSING;

        return -1;
    }

    //* 再次寻址与上层协议寻址结果不一致，则直接放弃该报文
    if (unSrcAddr != unRouteSrcAddr)
    {
        netif_freed(pstNetif); 

        if (penErr)
            *penErr = ERRROUTEADDRMATCH;

        return -1; 
    }

    /* 
    PST_NETIF pstNetif = netif_get_by_ip(unSrcAddr, TRUE);
    if (NULL == pstNetif)
    {
        if (penErr)
            *penErr = ERRNONETIFFOUND;

        return -1;
    }
    */

    return netif_ip_send(pstNetif, NULL, unSrcAddr, unDstAddr, unArpDstAddr, enProtocol, ubTTL, sBufListHead, penErr);
}

void ip_recv(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR *pubPacket, INT nPacketLen)
{
    PST_IP_HDR pstHdr = (PST_IP_HDR)pubPacket;
    UCHAR usHdrLen = pstHdr->bitHdrLen * 4;
	USHORT usIpPacketLen = htons(pstHdr->usPacketLen);
	if (nPacketLen >= (INT)usIpPacketLen)
		nPacketLen = (INT)usIpPacketLen;
	else //* 指定的报文长度与实际收到的字节数不匹配，直接丢弃该报文
		return; 

    //* 首先看看校验和是否正确
    USHORT usPktChecksum = pstHdr->usChecksum;
    pstHdr->usChecksum = 0;
    USHORT usChecksum = tcpip_checksum((USHORT *)pubPacket, usHdrLen);
    if (usPktChecksum != usChecksum)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
        pstHdr->usChecksum = usPktChecksum;
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("checksum error (%04X, %04X), and the IP packet will be dropped\r\n", usChecksum, usPktChecksum);
        printf_hex(pubPacket, nPacketLen, 48);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        return; 
    }

#if SUPPORT_ETHERNET
    //* 如果当前网卡类型是ethernet网卡，就需要看看ip地址是否匹配，只有匹配的才会处理		
    if (NIF_ETHERNET == pstNetif->enType)
    {
		// 注意，这里仅支持255.255.255.255这样的广播报文，x.x.x.255类型的广播报文不被支持
		if (pstHdr->unDstIP != 0xFFFFFFFF)
		{
			// ip地址不匹配，直接丢弃当前报文
			if (pstNetif->stIPv4.unAddr && !ethernet_ipv4_addr_matched(pstNetif, pstHdr->unDstIP))
				return;

			// 如果不是环回接口发送的报文则更新arp缓存表
			PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
			if(pubDstMacAddr != pstExtra->ubaMacAddr)
				arp_add_ethii_ipv4_ext(pstExtra->pstcbArp->staEntry, pstHdr->unSrcIP, pubDstMacAddr); 
		}        
    }	
#endif
    

    switch (pstHdr->ubProto)
    {
    case IPPROTO_ICMP: 
        icmp_recv(pstNetif, pubDstMacAddr, pubPacket, nPacketLen);
        break; 

    case IPPROTO_TCP:
        tcp_recv((in_addr_t *)&pstHdr->unSrcIP, (in_addr_t *)&pstHdr->unDstIP, pubPacket + usHdrLen, nPacketLen - usHdrLen, IPV4);
        break; 

    case IPPROTO_UDP:
        udp_recv(pstHdr->unSrcIP, pstHdr->unDstIP, pubPacket + usHdrLen, nPacketLen - usHdrLen);
        break;

    default:
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("unsupported IP upper layer protocol (%d), the packet will be dropped\r\n", (UINT)pstHdr->ubProto);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        break; 
    }
}

#if SUPPORT_IPV6
//* 计算流标签
UINT ipv6_flow_label_cal(UCHAR ubaDstAddr[16], UCHAR ubaSrcAddr[16], UCHAR ubNextHeader, USHORT usDstPort, USHORT usSrcPort)
{
	ULONGLONG ullKey; 

	if (ubaSrcAddr[16] == 0x00 || ubaSrcAddr[16] == 0xFF)
		return 0; 

	//* 单播地址则根据[RFC6437]附录A的算法描述设计实现，详见：https://www.rfc-editor.org/rfc/rfc6437#appendix-A
	if (ubaDstAddr[0] != 0xFF)
	{
		//* 存在ubaDstAddr及ubaSrcAddr两个地址8字节不对齐的可能，所以不能直接采用强制数据类型转换的方式赋值
		struct {
			ULONGLONG ullAddr0; 
			ULONGLONG ullAddr1;
		} stDstAddr, stSrcAddr; 
		memcpy(&stDstAddr, ubaDstAddr, 16); 
		memcpy(&stSrcAddr, ubaSrcAddr, 16); 
		
		ullKey = stDstAddr.ullAddr0 + stDstAddr.ullAddr1 + stSrcAddr.ullAddr0 + stSrcAddr.ullAddr1 + (ULONGLONG)ubNextHeader;				
	}
	else
	{
		//* 组播地址则生成一个随机数
		rand_any_bytes((UCHAR *)&ullKey, sizeof(ULONGLONG)); 
	}	

	USHORT usHashVal = (USHORT)hash_von_neumann(ullKey);
	usHashVal += usDstPort + usSrcPort; 
	UINT unFlowLabel = (UINT)(usHashVal << 4) & 0x000FFFFF;
	if (unFlowLabel)
		return unFlowLabel;
	else
		return 1; 
}

static INT netif_ipv6_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubaDstIpv6ToMac[16], UCHAR ubNextHeader, SHORT sBufListHead, UINT unFlowLabel, UCHAR ubHopLimit, EN_ONPSERR *penErr)
{
	INT nRtnVal;
	BOOL blNetifFreedEn = TRUE; 

	ST_IPv6_HDR stHdr; 
	stHdr.ipv6_ver = 6; 
	stHdr.ipv6_dscp = 0; 
	stHdr.ipv6_ecn = 0;	
	stHdr.ipv6_flow_label = unFlowLabel;  
	stHdr.ipv6_flag = htonl(stHdr.ipv6_flag); 
	stHdr.usPayloadLen = htons((USHORT)buf_list_get_len(sBufListHead)); 
	stHdr.ubNextHdr = ubNextHeader; 
	stHdr.ubHopLimit = ubHopLimit; 
	memcpy(stHdr.ubaSrcIpv6, ubaSrcIpv6, 16);
	memcpy(stHdr.ubaDstIpv6, ubaDstIpv6, 16); 

	//* 挂载到buf list头部
	SHORT sHdrNode;
	sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (USHORT)sizeof(ST_IPv6_HDR), penErr);
	if (sHdrNode < 0)
	{
		//* 使用计数减一，释放对网卡资源的占用
		netif_freed(pstNetif);
		return -1;
	}
	buf_list_put_head(&sBufListHead, sHdrNode); 	

#if SUPPORT_ETHERNET
	//* 如果网络接口类型为ethernet，需要先获取目标mac地址
	if (NIF_ETHERNET == pstNetif->enType)
	{
		if (pubDstMacAddr)
		{
			PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
			if (pubDstMacAddr != pstExtra->ubaMacAddr)
				nRtnVal = pstNetif->pfunSend(pstNetif, IPV6, sBufListHead, pubDstMacAddr, penErr);
			else
				nRtnVal = ethernet_loopback_put_packet(pstNetif, sBufListHead, LPPROTO_IPv6);
		}
		else
		{
			UCHAR ubaDstMac[ETH_MAC_ADDR_LEN];			          
			nRtnVal = ipv6_mac_get_ext(pstNetif, ubaSrcIpv6, ubaDstIpv6ToMac, ubaDstMac, sBufListHead, &blNetifFreedEn, penErr);
			if (!nRtnVal) //* 得到目标mac地址，直接发送该报文
			{
				PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
				if (memcmp(ubaDstMac, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN))
					nRtnVal = pstNetif->pfunSend(pstNetif, IPV6, sBufListHead, ubaDstMac, penErr);
				else
					nRtnVal = ethernet_loopback_put_packet(pstNetif, sBufListHead, LPPROTO_IPv6); 
			}
		}
	}
	else
	{ 
#endif
		//* 完成发送
		nRtnVal = pstNetif->pfunSend(pstNetif, IPV6, sBufListHead, NULL, penErr);
#if SUPPORT_ETHERNET
	}
#endif

	//* 如果不需要等待icmpv6查询结果，则立即释放对网卡的使用权
	if (blNetifFreedEn)
		netif_freed(pstNetif);

	//* 释放刚才申请的buf list节点
	buf_list_free(sHdrNode);

	return nRtnVal;
}

INT ipv6_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubNextHeader, SHORT sBufListHead, UINT unFlowLabel, EN_ONPSERR *penErr)
{
	UCHAR ubaSrcIpv6Used[16], ubaDstIpv6ToMac[16]; 
	memcpy(ubaSrcIpv6Used, ubaSrcIpv6, 16); 
	memcpy(ubaDstIpv6ToMac, ubaDstIpv6, 16); 

	UCHAR ubHopLimit = 255; 

	//* 如果未指定netif则通过路由表选择一个适合的netif
	PST_NETIF pstNetifUsed = pstNetif; 
	if (pstNetifUsed)
		netif_used(pstNetifUsed);
	else
	{		
		pstNetifUsed = route_ipv6_get_netif(ubaDstIpv6ToMac, TRUE, ubaSrcIpv6Used, ubaDstIpv6ToMac, &ubHopLimit);
		if (NULL == pstNetifUsed)
		{
			if (penErr)
				*penErr = ERRADDRESSING;

			return -1;
		}
	}

	return netif_ipv6_send(pstNetifUsed, pubDstMacAddr, ubaSrcIpv6Used, ubaDstIpv6, ubaDstIpv6ToMac, ubNextHeader, sBufListHead, unFlowLabel, ubHopLimit, penErr);
}

INT ipv6_send_ext(UCHAR ubaSrcIpv6[16], UCHAR ubaDstIpv6[16], UCHAR ubNextHeader, SHORT sBufListHead, UINT unFlowLabel, EN_ONPSERR *penErr)
{
	UCHAR /*ubaSrcIpv6Used[16], */ubaDstIpv6ToMac[16];	
	memcpy(ubaDstIpv6ToMac, ubaDstIpv6, 16);
	     
	UCHAR ubHopLimit = 255;

	//* 路由寻址
	PST_NETIF pstNetif = route_ipv6_get_netif(ubaDstIpv6ToMac, TRUE, NULL/*ubaSrcIpv6Used*/, ubaDstIpv6ToMac, &ubHopLimit);
	if (NULL == pstNetif)
	{
		if (penErr)
			*penErr = ERRADDRESSING;

		return -1;
	}    

	//* 路由寻址结果与上层调用函数给出的源地址不一致，则直接报错并抛弃该报文
	//if (memcmp(ubaSrcIpv6, ubaSrcIpv6Used, 16))
	//{
	//	netif_freed(pstNetif);

	//	if (penErr)
	//		*penErr = ERRROUTEADDRMATCH;

	//	return -1;
	//}
	     
	return netif_ipv6_send(pstNetif, NULL, ubaSrcIpv6/* && ubaSrcIpv6[0] ? ubaSrcIpv6 : ubaSrcIpv6Used*/, ubaDstIpv6, ubaDstIpv6ToMac, ubNextHeader, sBufListHead, unFlowLabel, ubHopLimit, penErr);
}

void ipv6_recv(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR *pubPacket, INT nPacketLen)
{
	if (pstNetif->stIPv6.bitCfgState < IPv6CFG_LNKADDR)
		return; 

    PST_IPv6_HDR pstHdr; 
    USHORT usPayloadLen; 
    UCHAR ubNextHdr, *pubUpperProtoPkt; 

__lblReparse: 
	pstHdr = (PST_IPv6_HDR)pubPacket;	
	usPayloadLen = htons(pstHdr->usPayloadLen); 
	if (nPacketLen < (INT)usPayloadLen) //* 指定的报文长度与实际收到的字节数不匹配，直接丢弃该报文
		return; 

#if SUPPORT_ETHERNET
	//* 如果网络接口类型为ethernet，就需要看看ipv6地址是否匹配，只有匹配的才会处理，同时顺道更新ipv6 mac地址映射缓存表
	if (NIF_ETHERNET == pstNetif->enType)
	{
		// ip地址不匹配，直接丢弃当前报文
		if (!ethernet_ipv6_addr_matched(pstNetif, pstHdr->ubaDstIpv6))
		{
			/*
			CHAR szIpv6[40];
			os_thread_mutex_lock(o_hMtxPrintf);
			printf("%s -> ", inet6_ntoa(pstHdr->ubaSrcIpv6, szIpv6));
			printf("%s\r\n", inet6_ntoa(pstHdr->ubaDstIpv6, szIpv6));
			os_thread_mutex_unlock(o_hMtxPrintf);
			*/

			return;
		}
	}
#endif

	//* 在这里目前仅处理逐跳选项头其它扩展头暂时不予理会，当需要时再分别做针对性处理	
	if (pstHdr->ubNextHdr)
	{
		ubNextHdr = pstHdr->ubNextHdr;
		pubUpperProtoPkt = pubPacket + sizeof(ST_IPv6_HDR); 
	}
	else //* 处理ipv6逐跳选项（IPv6 Hop-by-Hop Option）
	{
		PST_IPv6_EXTOPT_HDR pstExtOptHdr = (PST_IPv6_EXTOPT_HDR)pubPacket;
		USHORT usHandleOptBytes = 0, usDataBytes; 
		while (usHandleOptBytes < usPayloadLen)
		{
			usDataBytes = (USHORT)((pstExtOptHdr->ubLen + 1) * 8);
			if (IPPROTO_ICMPv6 == pstExtOptHdr->ubNextHdr || IPPROTO_TCP == pstExtOptHdr->ubNextHdr || IPPROTO_UDP == pstExtOptHdr->ubNextHdr)
			{
				ubNextHdr = pstExtOptHdr->ubNextHdr;
				pubUpperProtoPkt = (UCHAR *)pstExtOptHdr + usDataBytes;
				usPayloadLen -= usHandleOptBytes - usDataBytes; 
				break; 
			}

			usHandleOptBytes += usDataBytes; 
			pstExtOptHdr = (PST_IPv6_EXTOPT_HDR)((UCHAR *)pstExtOptHdr + usHandleOptBytes);
		}
	}	

	switch (ubNextHdr)
	{
	case IPPROTO_ICMPv6:
        pubUpperProtoPkt = icmpv6_recv(pstNetif, pubDstMacAddr, pubPacket, nPacketLen, pubUpperProtoPkt); 
        if (pubUpperProtoPkt)
        {
            nPacketLen = pubUpperProtoPkt - pubPacket; 
            pubPacket = pubUpperProtoPkt; 
            goto __lblReparse; 
        }
		break;

	case IPPROTO_TCP:
		tcp_recv(pstHdr->ubaSrcIpv6, pstHdr->ubaDstIpv6, pubUpperProtoPkt, (INT)usPayloadLen, IPV6); 
		break;

	case IPPROTO_UDP:
		ipv6_udp_recv(pstNetif, pstHdr->ubaSrcIpv6, pstHdr->ubaDstIpv6, pubUpperProtoPkt, (INT)usPayloadLen);
		break;

	default:
#if SUPPORT_PRINTF && DEBUG_LEVEL > 3
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("unsupported IPv6 upper layer protocol (%d), the packet will be dropped\r\n", (UINT)pstHdr->ubNextHdr);
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
		break;
	}
}
#endif
