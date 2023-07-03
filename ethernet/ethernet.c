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
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "ip/ip.h"

#if SUPPORT_ETHERNET
#if SUPPORT_IPV6
#include "ip/icmpv6.h"
#include "ethernet/dhcpv6.h"
#include "ip/ipv6_configure.h"
#endif

#include "ethernet/arp.h" 
#include "ethernet/ethernet_frame.h"
#define SYMBOL_GLOBALS
#include "ethernet/ethernet.h"
#undef SYMBOL_GLOBALS

//* 保存ethernet网卡附加信息的静态存储时期变量，系统存在几个ethernet网卡，这里就会申请几个数组单元
static ST_NETIFEXTRA_ETH l_staExtraOfEth[ETHERNET_NUM]; 

#if SUPPORT_IPV6
static const UCHAR l_ubaInputMcAddrs[] = { IPv6MCA_NETIFNODES, IPv6MCA_ALLNODES, IPv6MCA_SOLNODE }; //* 以太网接口允许通讯的组播地址白名单，这里如果添加新的名单，需要修改ethernet_ipv6_addr_matched()函数增加新名单的处理代码
#endif

void ethernet_init(void)
{
    arp_init(); 

#if SUPPORT_IPV6
	ipv6_cfg_init();	
	dhcpv6_client_ctl_block_init(); 
	ipv6_mac_mapping_tbl_init(); 
#endif

    memset(l_staExtraOfEth, 0, sizeof(l_staExtraOfEth)); 
}

//* 添加ethernet网卡
PST_NETIF ethernet_add(const CHAR *pszIfName, const UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN], PST_IPV4 pstIPv4, PFUN_EMAC_SEND pfunEmacSend, void(*pfunStartTHEmacRecv)(void *pvParam), PST_NETIF *ppstNetif, EN_ONPSERR *penErr)
{
    PST_NETIF pstNetif = NULL; 
    PST_NETIFEXTRA_ETH pstExtra = NULL; 
    INT i; 
    os_critical_init(); 
    os_enter_critical();
    {
        for (i = 0; i < ETHERNET_NUM; i++)
        {
            if (!l_staExtraOfEth[i].bIsUsed)
            {
                pstExtra = &l_staExtraOfEth[i]; 
                pstExtra->pstRcvedPacketList = NULL; 
                pstExtra->bIsUsed = TRUE;                  
                break; 
            }
        }
    }
    os_exit_critical();    

	if (penErr)
		*penErr = ERRNO;

    if (!pstExtra)
    {
        if (penErr)
            *penErr = ERRETHEXTRAEMPTY;
        return NULL;
    }

    //* 建立信号量
    pstExtra->hSem = os_thread_sem_init(0, 100000); 
    if (INVALID_HSEM == pstExtra->hSem)
    {
        pstExtra->bIsUsed = FALSE; //* 归还

        if (penErr)
            *penErr = ERRSEMINITFAILED;
        return NULL;
    }

    //* 申请一个arp控制块
    PSTCB_ETHARP pstcbArp = arp_ctl_block_new(); 
    if (!pstcbArp)
    {
        os_thread_sem_uninit(pstExtra->hSem);	//* 归还占用的信号量资源
        pstExtra->bIsUsed = FALSE;				//* 归还占用的附加信息存储节点
        if (penErr)
            *penErr = ERRNEWARPCTLBLOCK;
        return NULL;
    }

#if SUPPORT_IPV6
	//* 申请一个
	PSTCB_ETHIPv6MAC pstcbIpv6Mac = ipv6_mac_ctl_block_new(); 
	if (!pstcbIpv6Mac)
	{
		arp_ctl_block_free(pstcbArp);			//* 释放刚才申请的arp控制块
		os_thread_sem_uninit(pstExtra->hSem);	//* 归还占用的信号量资源
		pstExtra->bIsUsed = FALSE;				//* 归还刚刚占用的附加信息节点，不需要关中断进行保护，获取节点的时候需要
		if (penErr)
			*penErr = ERRNEWIPv6MACCTLBLOCK; 
		return NULL; 
	}
#endif

    PST_NETIF_NODE pstIfNode = netif_add(NIF_ETHERNET, pszIfName, pstIPv4, ethernet_ii_send, pstExtra, penErr);
    if (pstIfNode)
    {
        pstNetif = &pstIfNode->stIf;

#if ETH_EXTRA_IP_EN
        memset(pstExtra->staExtraIp, 0, sizeof(pstExtra->staExtraIp)); 
#endif //* #if ETH_EXTRA_IP_EN
        pstExtra->pstcbArp = pstcbArp; 
#if SUPPORT_IPV6
		//* 以太网接收启动之前必须先初始化配置状态，这样可确保网卡开启地址自动配置后才开始处理到达的ipv6报文，这之前的将直接丢掉
		//* 之所以在这里而不是在netif_add()函数内部初始这个状态值，原因是不同类型的网卡可能存在初始值值不同的情形，比如ppp网卡
		pstNetif->stIPv6.bitCfgState = IPv6CFG_INIT; 
		pstExtra->pstcbIpv6Mac = pstcbIpv6Mac; 
#endif
        pstExtra->pfunEmacSend = pfunEmacSend; 
        memcpy(pstExtra->ubaMacAddr, ubaMacAddr, ETH_MAC_ADDR_LEN);
        if (pstIPv4 && pstIPv4->unAddr) //* 地址不为0则为静态地址，需要将其添加到路由表中
        {
            pstExtra->bIsStaticAddr = TRUE;

            //* 添加到路由表，使其成为缺省路由
            if (route_add(pstNetif, 0, pstIPv4->unGateway, pstIPv4->unSubnetMask, penErr))
            {
                //* 启动接收任务
                *ppstNetif = pstNetif; 
                pfunStartTHEmacRecv(ppstNetif);
            }
            else
            {
                netif_del(pstIfNode);
                pstNetif = NULL; 
                
		#if SUPPORT_IPV6
				ipv6_mac_ctl_block_free(pstcbIpv6Mac);	//* 释放前面刚刚申请的ipv6到mac地址映射表控制块
		#endif
				arp_ctl_block_free(pstcbArp);				//* 释放前面刚才申请的arp控制块
                os_thread_sem_uninit(pstExtra->hSem);		//* 归还占用的信号量资源                				
                pstExtra->bIsUsed = FALSE;					//* 归还刚刚占用的附加信息节点，不需要关中断进行保护，获取节点的时候需要

				goto __lblEnd; 
            }
        }
		else
		{
			pstExtra->bIsStaticAddr = FALSE;

			//* 启动接收任务
			*ppstNetif = pstNetif;
			pfunStartTHEmacRecv(ppstNetif);
		}

#if SUPPORT_IPV6
		//* 开启自动配置
		if (!ipv6_cfg_start(pstNetif, penErr))
		{
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("icmpv6_start_config() failed, %s\r\n", onps_error(*penErr));
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif
		}
#endif
    }
    else
    {
#if SUPPORT_IPV6
		ipv6_mac_ctl_block_free(pstcbIpv6Mac);	//* 释放前面刚刚申请的ipv6到mac地址映射表控制块
#endif
		arp_ctl_block_free(pstcbArp);			//* 释放前面刚才申请的arp控制块
		os_thread_sem_uninit(pstExtra->hSem);	//* 归还占用的信号量资源                				
		pstExtra->bIsUsed = FALSE;				//* 归还刚刚占用的附加信息节点，不需要关中断进行保护，获取节点的时候需要
    }

__lblEnd: 
    return pstNetif;
}

//* 删除ethernet网卡，注意这个函数为阻塞型，如果协议栈支持ipv6的选项被打开的话
void ethernet_del(PST_NETIF *ppstNetif)
{
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)(*ppstNetif)->pvExtra; 

    os_thread_sem_uninit(pstExtra->hSem);    
    arp_ctl_block_free(pstExtra->pstcbArp); 
    pstExtra->bIsUsed = FALSE; 

    //* 先从路由表删除
    route_del_ext(*ppstNetif);

#if SUPPORT_IPV6	
	//* 首先等待Ipv6动态地址及缺省路由器的生存期计时器正常启动，因为资源的回收工作由这个计时器完成，否则会造成资源泄露
	while ((*ppstNetif)->stIPv6.bitSvvTimerState == IPv6SVVTMR_INVALID)
		os_sleep_secs(1);

	//* 通知生存计时器回收资源并结束运行
	(*ppstNetif)->stIPv6.bitSvvTimerState = IPv6SVVTMR_STOP;

	//* 等待生存计时器回收完全部资源后才可以删除网卡
	while (TRUE)
	{
		if ((*ppstNetif)->stIPv6.bitSvvTimerState == IPv6SVVTMR_RELEASED)
			break; 
		os_sleep_secs(1);
	}		
#endif

    //* 再从网卡链表删除
    netif_del_ext(*ppstNetif); 

    *ppstNetif = NULL; 
}

//* 通过ethernet网卡进行发送
INT ethernet_ii_send(PST_NETIF pstNetif, UCHAR ubProtocol, SHORT sBufListHead, void *pvExtraParam, EN_ONPSERR *penErr)
{
    ST_ETHERNET_II_HDR stEthIIHdr; 
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

    //* 填充源与目标mac地址
    memcpy(stEthIIHdr.ubaDstMacAddr, (const UCHAR *)pvExtraParam, ETH_MAC_ADDR_LEN); 
    memcpy(stEthIIHdr.ubaSrcMacAddr, pstExtra->ubaMacAddr, ETH_MAC_ADDR_LEN); 

    //* 转换成ethernte ii定义的协议类型值
    switch((EN_NPSPROTOCOL)ubProtocol)
    {
    case IPV4: 
        stEthIIHdr.usProtoType = htons(ETHII_IP);
        break; 

    case ARP: 
        stEthIIHdr.usProtoType = htons(ETHII_ARP);
        break; 

#if SUPPORT_IPV6
	case IPV6:
		stEthIIHdr.usProtoType = htons(ETHII_IPV6);
		break;
#endif

    default: 
        if (penErr)
            *penErr = ERRUNSUPPETHIIPROTO; 
        return -1; 
    }

    //* ethernet ii协议头挂载到链表头部
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stEthIIHdr, (USHORT)sizeof(ST_ETHERNET_II_HDR), penErr);
    if (sHdrNode < 0)
    {
        //* 使用计数减一，释放对网卡资源的占用
        //netif_freed(pstNetif);
        return -1;
    }
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 发送数据
    INT nRtnVal = pstExtra->pfunEmacSend(sBufListHead, penErr); 

#if SUPPORT_PRINTF && DEBUG_LEVEL == 3
	#if PRINTF_THREAD_MUTEX
	os_thread_mutex_lock(o_hMtxPrintf);
	#endif
	printf("sent %d bytes: \r\n", nRtnVal);
	printf_hex_ext(sBufListHead, 48);
	#if PRINTF_THREAD_MUTEX
	os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif

    //* 释放刚才申请的buf list节点
    buf_list_free(sHdrNode); 

    return nRtnVal; 
}

//* 处理ethernet网卡收到的ethernet ii协议帧
void ethernet_ii_recv(PST_NETIF pstNetif, UCHAR *pubPacket, INT nPacketLen)
{
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;

    if (nPacketLen < (INT)sizeof(ST_ETHERNET_II_HDR))
        return; 
	
	PST_LOOPBACK_HDR pstLoopbackHdr = (PST_LOOPBACK_HDR)pubPacket; 
	if (LPPROTO_IP == pstLoopbackHdr->unProtoType && (0x40 == (pubPacket[sizeof(ST_LOOPBACK_HDR)] & 0xF0)))
	{
		ip_recv(pstNetif, pstExtra->ubaMacAddr, pubPacket + sizeof(ST_LOOPBACK_HDR), nPacketLen);
		return; 
	}
#if SUPPORT_IPV6
	else if (LPPROTO_IPv6 == pstLoopbackHdr->unProtoType && (0x60 == (pubPacket[sizeof(ST_LOOPBACK_HDR)] & 0xF0)))
	{
		ipv6_recv(pstNetif, pstExtra->ubaMacAddr, pubPacket + sizeof(ST_LOOPBACK_HDR), nPacketLen); 
		return; 
	}
#endif
	else; 


    PST_ETHERNET_II_HDR pstHdr = (PST_ETHERNET_II_HDR)pubPacket; 

    //* 既不是广播地址，也不匹配本ethernet网卡mac地址，则直接丢弃该报文
    if (!is_mac_broadcast_addr(pstHdr->ubaDstMacAddr) && !ethernet_mac_matched(pstHdr->ubaDstMacAddr, pstExtra->ubaMacAddr))
        return; 

#if SUPPORT_PRINTF && DEBUG_LEVEL == 4	        	        
		printf("recv %d bytes: \r\n", nPacketLen);
		printf_hex(pubPacket, nPacketLen, 48);	
#endif

    //* 根据ethernet ii帧携带的协议类型分别处理之
    USHORT usProtocolType = htons(pstHdr->usProtoType);
    switch (usProtocolType)
    {
    case ETHII_IP: 	        
        ip_recv(pstNetif, pstHdr->ubaSrcMacAddr, pubPacket + sizeof(ST_ETHERNET_II_HDR), nPacketLen - (INT)sizeof(ST_ETHERNET_II_HDR));
        break; 

    case ETHII_ARP: 		
        arp_recv_from_ethii(pstNetif, pubPacket + sizeof(ST_ETHERNET_II_HDR), nPacketLen - (INT)sizeof(ST_ETHERNET_II_HDR)); 		
        break; 

    case ETHII_IPV6: 		
#if SUPPORT_IPV6
		ipv6_recv(pstNetif, pstHdr->ubaSrcMacAddr, pubPacket + sizeof(ST_ETHERNET_II_HDR), nPacketLen - (INT)sizeof(ST_ETHERNET_II_HDR));
#endif
        break; 

    default: 
#if SUPPORT_PRINTF && DEBUG_LEVEL
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("error: Unsupported ethernet ii protocol type (%04X), the packet will be dropped\r\n", usProtocolType);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        break; 
    }
}

//* 参数unTargetIpAddr指定的ip地址是否与ethernet网卡的ip地址匹配
BOOL ethernet_ipv4_addr_matched(PST_NETIF pstNetif, in_addr_t unTargetIpAddr)
{
	if (0x0100007F == unTargetIpAddr)
		return TRUE; 

    if (unTargetIpAddr == pstNetif->stIPv4.unAddr)
        return TRUE; 

#if ETH_EXTRA_IP_EN
    //* 看看附加ip地址列表有匹配的吗    
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
    CHAR i;     
    for (i = 0; i < ETH_EXTRA_IP_NUM; i++)
    {
        if (pstExtra->staExtraIp[i].unAddr)
        {
            if (unTargetIpAddr == pstExtra->staExtraIp[i].unAddr)
                return TRUE;
        }
        else
            break; 
    }
#endif //* #if ETH_EXTRA_IP_EN

    return FALSE; 
}

#if SUPPORT_IPV6
//* 判断目标地址是否与网络接口支持的邻居节点请求组播地址是否匹配
static BOOL ipv6_sol_mc_addr_matched(PST_NETIF pstNetif, UCHAR ubaTargetIpv6[16])
{
	UCHAR ubaSolMcAddr[16];
	PST_IPv6_DYNADDR pstNextAddr = NULL;
	do {
		//* 采用线程安全的函数读取地址节点，直至调用netif_ipv6_dyn_addr_release()函数之前，该节点占用的资源均不会被协议栈回收，即使生存时间到期
		pstNextAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextAddr, TRUE);
		if (pstNextAddr && (pstNextAddr->bitState == IPv6ADDR_PREFERRED || pstNextAddr->bitState == IPv6ADDR_DEPRECATED))
		{
			if (!memcmp(ubaTargetIpv6, ipv6_sol_mc_addr(pstNextAddr->ubaVal, ubaSolMcAddr), 16)) 
			{
				//* 处理完毕释放当前地址节点，其实就是引用计数减一
				netif_ipv6_dyn_addr_release(pstNextAddr);
				return TRUE;
			}
		}
	} while (pstNextAddr);


	if (pstNetif->stIPv6.stLnkAddr.bitState == IPv6ADDR_PREFERRED && !memcmp(ubaTargetIpv6, ipv6_sol_mc_addr(pstNetif->nif_lla_ipv6, ubaSolMcAddr), 16))
		return TRUE;

	return FALSE;
}

BOOL ethernet_ipv6_addr_matched(PST_NETIF pstNetif, UCHAR ubaTargetIpv6[16])
{
	//* 看看是否是单播地址
	if (ubaTargetIpv6[0] != 0xFF)
	{	
		if (!memcmp(ubaTargetIpv6, ipv6_get_loopback_addr(), 16))
			return TRUE;

		//* 到达的报文不需要判断地址当前状态，即使处于“IPv6ADDR_DEPRECATED”（弃用）状态也可以接受到达的报文
		PST_IPv6_DYNADDR pstNextAddr = NULL; 
		do {
			//* 采用线程安全的函数读取地址节点，直至调用netif_ipv6_dyn_addr_release()函数之前，该节点占用的资源均不会被协议栈回收，即使生存时间到期
			pstNextAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextAddr, TRUE);
			if (pstNextAddr && (pstNextAddr->bitState == IPv6ADDR_PREFERRED || pstNextAddr->bitState == IPv6ADDR_DEPRECATED))
			{
				if (!memcmp(ubaTargetIpv6, pstNextAddr->ubaVal, 16))
				{
					//* 处理完毕释放当前地址节点，其实就是引用计数减一
					netif_ipv6_dyn_addr_release(pstNextAddr); 

					return TRUE;
				}
			}
		} while (pstNextAddr);  

		//* 链路本地地址是否匹配
		if (pstNetif->stIPv6.stLnkAddr.bitState == IPv6ADDR_PREFERRED && !memcmp(ubaTargetIpv6, pstNetif->nif_lla_ipv6, 16))
			return TRUE;
	}
	else //* 组播地址，需要逐个判断组播地址是否匹配
	{
		UCHAR i; 
		for (i = 0; i < sizeof(l_ubaInputMcAddrs); i++)
		{
			switch ((EN_IPv6MCADDR_TYPE)l_ubaInputMcAddrs[i])
			{
			case IPv6MCA_NETIFNODES: 
				if (!memcmp(ubaTargetIpv6, ipv6_mc_addr(IPv6MCA_NETIFNODES), 16))
					return TRUE;
				break; 

			case IPv6MCA_ALLNODES:
				if (!memcmp(ubaTargetIpv6, ipv6_mc_addr(IPv6MCA_ALLNODES), 16))
					return TRUE;
				break; 

			case IPv6MCA_SOLNODE:
				if (ipv6_sol_mc_addr_matched(pstNetif, ubaTargetIpv6))
					return TRUE; 
				break; 
			}
		}
	}

	return FALSE; 
}
#endif

void thread_ethernet_ii_recv(void *pvParam)
{
    PST_NETIF *ppstNetif = (PST_NETIF *)pvParam; 
    PST_NETIF pstNetif; 
    PST_NETIFEXTRA_ETH pstExtra; 
    PST_SLINKEDLIST_NODE pstNode; 
    INT nRtnVal; 

    os_critical_init(); 

    while (TRUE)
    {
        if (NULL == *ppstNetif)
        {
            os_sleep_secs(1);
            continue; 
        }

        pstNetif = *ppstNetif; 
        pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
        nRtnVal = os_thread_sem_pend(pstExtra->hSem, 1);
        if (nRtnVal)
        {
            if (nRtnVal < 0)
            {
        #if SUPPORT_PRINTF && DEBUG_LEVEL
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif
                printf("os_thread_sem_pend() failed, the NIC name is %s, %s\r\n", (*ppstNetif)->szName, onps_error(ERRINVALIDSEM));
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
        #endif
                os_sleep_secs(1);
            }
            continue;
        }

__lblGetPacket:
        //* 取出数据，需要先关中断
        os_enter_critical();         
        pstNode = sllist_get_node(&pstExtra->pstRcvedPacketList);         
        os_exit_critical(); 

        if (pstNode)
        {
            //* 向上传递
            ethernet_ii_recv(pstNetif, ((UCHAR *)pstNode) + sizeof(ST_SLINKEDLIST_NODE), (INT)pstNode->uniData.unVal); 
            //* 释放该节点
            buddy_free(pstNode); 

            goto __lblGetPacket; 
        }   
    }
}

//* 这个函数在ethernet网卡接收中断中使用，所以不需要加临界保护，相反，读取函数一定要加关中断，避免读写冲突
void ethernet_put_packet(PST_NETIF pstNetif, PST_SLINKEDLIST_NODE pstNode)
{
    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 
	
    sllist_put_tail_node(&pstExtra->pstRcvedPacketList, pstNode); 	
	os_thread_sem_post(pstExtra->hSem); 
}

INT ethernet_loopback_put_packet(PST_NETIF pstNetif, SHORT sBufListHead, UINT unLoopProtocol)
{
	UINT unPacketLen = buf_list_get_len(sBufListHead);

	EN_ONPSERR enErr; 
	UCHAR *pubPacket = (UCHAR *)buddy_alloc(sizeof(ST_SLINKEDLIST_NODE) + sizeof(ST_LOOPBACK_HDR) + unPacketLen, &enErr);
	if (pubPacket)
	{
		//* 搬运数据到接收链表
		PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra; 
		PST_SLINKEDLIST_NODE pstNode = (PST_SLINKEDLIST_NODE)pubPacket;
		pstNode->uniData.unVal = unPacketLen;
		buf_list_merge_packet(sBufListHead, pubPacket + sizeof(ST_SLINKEDLIST_NODE) + +sizeof(ST_LOOPBACK_HDR));

		PST_LOOPBACK_HDR pstLoopbackHdr = (PST_LOOPBACK_HDR)(pubPacket + sizeof(ST_SLINKEDLIST_NODE)); 
		pstLoopbackHdr->unProtoType = unLoopProtocol;

		os_critical_init();
		os_enter_critical(); 
		sllist_put_tail_node(&pstExtra->pstRcvedPacketList, pstNode);		
		os_exit_critical();

		os_thread_sem_post(pstExtra->hSem); 		

		return (INT)unPacketLen; 
	}
	else
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("ethernet_loopback_put_packet() failed, %s\r\n", onps_error(enErr));
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
	}

	return -1; 
}

#endif
