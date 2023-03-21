/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
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
#include "ethernet/arp.h"

#define SYMBOL_GLOBALS
#include "ip/ip.h"
#undef SYMBOL_GLOBALS
#include "ethernet/ethernet.h"
#include "ip/icmp.h"
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
    IPPROTO_MAX,
    IPPROTO_TCP, 
    IPPROTO_UDP
};
static USHORT l_usIPIdentifier = 0; 

#if SUPPORT_ETHERNET
//* 当通过ethernet网卡发送ip报文时，协议栈尚未保存目标ip地址对应的mac地址时，需要先发送一组arp查询报文获取mac地址后才能发送该报文，这个定时器溢出函数即处理此项业务
void eth_arp_wait_timeout_handler(void *pvParam)
{
    PSTCB_ETH_ARP_WAIT pstcbArpWait = (PSTCB_ETH_ARP_WAIT)pvParam; 
    PST_NETIF pstNetif = pstcbArpWait->pstNetif; 
    EN_ONPSERR enErr; 
    UCHAR *pubIpPacket; 
    PST_IP_HDR pstIpHdr; 
    UCHAR ubaDstMac[ETH_MAC_ADDR_LEN];
    INT nRtnVal; 

    os_critical_init();

	//* 尚未发送
	if (!pstcbArpWait->ubSndStatus)
	{
		//* arp查询计数，如果超出限值，则不再查询直接丢弃该报文
		pstcbArpWait->ubCount++;
		if (pstcbArpWait->ubCount > 5)
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("The arp query times out and the packet will be dropped\r\n");
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
			goto __lblEnd;
		}
	}
	else //* 已经处于发送中或发送完成状态，直接跳到函数尾部
		goto __lblEnd; 

    //* 此时已经过去了1秒，看看此刻是否已经得到目标ethernet网卡的mac地址
    pubIpPacket = ((UCHAR *)pstcbArpWait) + sizeof(STCB_ETH_ARP_WAIT); 
    pstIpHdr = (PST_IP_HDR)pubIpPacket;     
    nRtnVal = arp_get_mac(pstNetif, pstIpHdr->unSrcIP, pstcbArpWait->unIpv4, ubaDstMac, &enErr);
    if (!nRtnVal) //* 存在该条目，则直接调用ethernet接口注册的发送函数即可
    {
        os_enter_critical(); 
        {
            //* 尚未发送，则首先摘除这个节点
            if (pstcbArpWait->pstNode)
            {
                PSTCB_ETHARP pstcbArp = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbArp;                
                sllist_del_node(&pstcbArp->pstSListWaitQueue, pstcbArpWait->pstNode);       //* 从队列中删除
                sllist_put_node(&pstcbArp->pstSListWaitQueueFreed, pstcbArpWait->pstNode);  //* 放入空闲资源队列
				pstcbArpWait->pstNode = NULL; //* 清空，显式地告知后续地处理代码这个节点已经被释放，2023-03-21 11:32
            }            
            else //* 已经发送，则没必要重复发送
            {
                os_exit_critical(); 
                goto __lblEnd; 
            }
        }
        os_exit_critical();         

        //* 申请一个buf list节点并将ip报文挂载到list上
        SHORT sBufListHead = -1;
        SHORT sIpPacketNode = buf_list_get_ext(pubIpPacket/*((UCHAR *)pstcbArpWait) + sizeof(STCB_ETH_ARP_WAIT)*/, (UINT)pstcbArpWait->usIpPacketLen, &enErr);
        if (sIpPacketNode < 0)
        {
            //buddy_free(pvParam);

    #if SUPPORT_PRINTF && DEBUG_LEVEL
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("eth_arp_wait_timeout_handler() failed, %s\r\n", onps_error(enErr)); 
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
            goto __lblEnd; 
        }
        buf_list_put_head(&sBufListHead, sIpPacketNode);        
        nRtnVal = pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, ubaDstMac, &enErr); //* 完成实际地发送   
        if (nRtnVal < 0)
        {
    #if SUPPORT_PRINTF && DEBUG_LEVEL
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("eth_arp_wait_timeout_handler() failed, %s\r\n", onps_error(enErr));
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
        }
        buf_list_free(sIpPacketNode); //* 释放buf list节点
    }
    else 
    {
        //* 说明还是没有得到mac地址，需要再次开启一个定时器等1秒后再发送一次试试
        if (nRtnVal > 0 && one_shot_timer_new(eth_arp_wait_timeout_handler, pstcbArpWait, 1))                   
            return;         
        else
        {
    #if SUPPORT_PRINTF && DEBUG_LEVEL
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
            printf("eth_arp_wait_timeout_handler() failed, %s\r\n", onps_error(ERRNOIDLETIMER));
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
    #endif
        }
    }

__lblEnd: 
    //* 不再占用网卡
    netif_freed(pstNetif);

    os_enter_critical();
    {
        //* 尚未发送，则首先摘除这个节点
        if (pstcbArpWait->pstNode)
        {
            PSTCB_ETHARP pstcbArp = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->pstcbArp;
            sllist_del_node(&pstcbArp->pstSListWaitQueue, pstcbArpWait->pstNode);       //* 从队列中删除
            sllist_put_node(&pstcbArp->pstSListWaitQueueFreed, pstcbArpWait->pstNode);  //* 放入空闲资源队列            
        }        
    }
    os_exit_critical();    

	//* 如果不处于发送状态则直接释放内存，否则再次开启定时器以待发送完成后释放占用的内存
	if (1 == pstcbArpWait->ubSndStatus)			
		one_shot_timer_new(eth_arp_wait_timeout_handler, pstcbArpWait, 1); 	
	else
		buddy_free(pvParam);
}
#endif

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
    stHdr.unDstIP = htonl(unDstAddr);

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
			nRtnVal = pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, pubDstMacAddr, penErr);
		}        
		else
		{
			UCHAR ubaDstMac[ETH_MAC_ADDR_LEN];            
			nRtnVal = arp_get_mac_ext(pstNetif, unSrcAddr, unArpDstAddr, ubaDstMac, sBufListHead, &blNetifFreedEn, penErr); 
			if (!nRtnVal) //* 存在该条目，则直接调用ethernet接口注册的发送函数即可
			{
				nRtnVal = pstNetif->pfunSend(pstNetif, IPV4, sBufListHead, ubaDstMac, penErr);
			}  
            /*
			else
			{
				// 说明已经成功发送了arp报文，开启一个定时器等1秒后再发送一次试试
				if (nRtnVal > 0)
				{
					UINT unIpPacketLen = buf_list_get_len(sBufListHead); // 获取报文长度
					PSTCB_ETH_ARP_WAIT pstcbArpWait = buddy_alloc(sizeof(STCB_ETH_ARP_WAIT) + unIpPacketLen, penErr); // 申请一块缓冲区用来缓存当前尚无法发送的报文，头部留出一个字节用来计数，超出累计计数限值不再发送arp报文并抛弃当前报文                
					if (pstcbArpWait)
					{
						UCHAR *pubIpPacket = ((UCHAR *)pstcbArpWait) + sizeof(STCB_ETH_ARP_WAIT);

						// 保存报文到刚申请的内存中，然后开启一个1秒定时器等待arp查询结果并在得到正确mac地址后发送ip报文
						buf_list_merge_packet(sBufListHead, pubIpPacket);

						// 计数器清零，并传递当前选择的netif
						pstcbArpWait->pstNetif = pstNetif;
						pstcbArpWait->unArpDstAddr = unArpDstAddr;
						pstcbArpWait->usIpPacketLen = (USHORT)unIpPacketLen;
						pstcbArpWait->ubCount = 0;

						// 启动一个1秒定时器，等待查询完毕
						if (one_shot_timer_new(eth_arp_wait_timeout_handler, pstcbArpWait, 1))
							blNetifFreedEn = FALSE;
						else
						{
                            // 定时器未启动，这里就要释放刚才申请的内存
                            buddy_free(pstcbArpWait);

							if (penErr)
								*penErr = ERRNOIDLETIMER;
							nRtnVal = -2;
						}
					}
					else
						nRtnVal = -1;
				}                
				else; // arp查询失败，不作任何处理
			} 
            */
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
	in_addr_t unSrcAddrUsed = unSrcAddr, unArpDstAddr = htonl(unDstAddr);

	//* 如果未指定netif则通过路由表选择一个适合的netif
	PST_NETIF pstNetifUsed = pstNetif; 
    if (pstNetifUsed)
		netif_used(pstNetifUsed);
	else
    {
        pstNetifUsed = route_get_netif(unArpDstAddr/*unDstAddr*/, TRUE, &unSrcAddrUsed, &unArpDstAddr);
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
    in_addr_t unRouteSrcAddr, unArpDstAddr = htonl(unDstAddr);

    PST_NETIF pstNetif = route_get_netif(unArpDstAddr/*unDstAddr*/, TRUE, &unRouteSrcAddr, &unArpDstAddr);
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

			// 更新arp缓存表
			PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
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
        tcp_recv(pstHdr->unSrcIP, pstHdr->unDstIP, pubPacket + usHdrLen, nPacketLen - usHdrLen);
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
static INT netif_ipv6_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, in_addr_t unSrcAddr, in_addr_t unDstAddr, in_addr_t unDstAddrUsedToGetLLA, EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ONPSERR *penErr)
{

}

INT ipv6_send(PST_NETIF pstNetif, UCHAR *pubDstMacAddr, UCHAR ubaSrcIp[16], UCHAR ubaDstIp[16], EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ONPSERR *penErr)
{

}
#endif
