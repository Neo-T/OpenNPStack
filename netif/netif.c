/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"

#define SYMBOL_GLOBALS
#include "netif/netif.h"
#undef SYMBOL_GLOBALS

#if SUPPORT_IPV6
#include "ip/icmpv6.h"
#include "ip/ipv6_configure.h"
#endif


static ST_NETIF_NODE l_staNetifNode[NETIF_NUM]; 
static PST_NETIF_NODE l_pstFreeNode = NULL; 
static PST_NETIF_NODE l_pstNetifLink = NULL; 
static HMUTEX l_hMtxNetif = INVALID_HMUTEX;
BOOL netif_init(EN_ONPSERR *penErr)
{
    //* 网卡节点清零
    memset(&l_staNetifNode[0], 0, sizeof(l_staNetifNode));

    //* 初始化
    INT i;
    for (i = 1; i < NETIF_NUM; i++)
        l_staNetifNode[i - 1].pstNext = &l_staNetifNode[i];
    l_pstFreeNode = &l_staNetifNode[0];

    l_hMtxNetif = os_thread_mutex_init();
    if (INVALID_HMUTEX != l_hMtxNetif)
        return TRUE;

    if (penErr)
        *penErr = ERRMUTEXINITFAILED;
    
    return FALSE; 
}

void netif_uninit(void)
{
    l_pstNetifLink = NULL;
    l_pstFreeNode = NULL;

    if (INVALID_HMUTEX != l_hMtxNetif)
        os_thread_mutex_uninit(l_hMtxNetif);
}

static PST_NETIF_NODE get_free_node(void)
{
    PST_NETIF_NODE pstNode; 
    os_thread_mutex_lock(l_hMtxNetif);
    {
        pstNode = l_pstFreeNode;
        if (l_pstFreeNode)
            l_pstFreeNode = l_pstFreeNode->pstNext;
    }
    os_thread_mutex_unlock(l_hMtxNetif);
    
    memset(&pstNode->stIf, 0, sizeof(pstNode->stIf));
    return pstNode;  
}

static void put_free_node(PST_NETIF_NODE pstNode)
{
    os_thread_mutex_lock(l_hMtxNetif);
    {
        pstNode->pstNext = l_pstFreeNode;
        l_pstFreeNode = pstNode;
    }
    os_thread_mutex_unlock(l_hMtxNetif);    
}

PST_NETIF_NODE netif_add(EN_NETIF enType, const CHAR *pszIfName, PST_IPV4 pstIPv4, PFUN_NETIF_SEND pfunSend, void *pvExtra, EN_ONPSERR *penErr)
{
    //* 首先看看要添加的这个网络接口是否已添加到链表中，判断的依据是网络接口名称，如果存在同名网络接口则直接更新相关信息后直接返回TRUE
    os_thread_mutex_lock(l_hMtxNetif);
    {
        PST_NETIF_NODE pstNextNode = l_pstNetifLink;
        while (pstNextNode)
        {
            if (strcmp(pszIfName, pstNextNode->stIf.szName) == 0)
            {
                //* 更新网络接口相关信息
                pstNextNode->stIf.enType   = enType;
                pstNextNode->stIf.pfunSend = pfunSend;
				if(pstIPv4)
					pstNextNode->stIf.stIPv4   = *pstIPv4;
                pstNextNode->stIf.pvExtra  = pvExtra;

                os_thread_mutex_unlock(l_hMtxNetif);
                return pstNextNode;
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    //* 没有找到同名网络接口，需要添加一个新的接口到链表
    PST_NETIF_NODE pstNode = get_free_node(); 
    if (NULL == pstNode)
    {
        if (penErr)
            *penErr = ERRNONETIFNODE;

        return NULL; 
    }

    //* 保存网络接口相关信息
    pstNode->stIf.enType     = enType;  
    pstNode->stIf.pfunSend   = pfunSend; 
	if (pstIPv4)
		pstNode->stIf.stIPv4 = *pstIPv4; 
    pstNode->stIf.pvExtra    = pvExtra; 
    pstNode->stIf.bUsedCount = -1; 
    snprintf(pstNode->stIf.szName, sizeof(pstNode->stIf.szName), "%s", pszIfName);

    //* 加入链表
    os_thread_mutex_lock(l_hMtxNetif);
    {
        pstNode->pstNext = l_pstNetifLink;
        l_pstNetifLink = pstNode; 
    }
    os_thread_mutex_unlock(l_hMtxNetif); 

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
    UCHAR *pubAddr = (UCHAR *)&pstNode->stIf.stIPv4.unAddr;
#if PRINTF_THREAD_MUTEX
    os_thread_mutex_lock(o_hMtxPrintf);
#endif
    printf("<%s> added to the protocol stack\r\n", pstNode->stIf.szName);
    printf("[inet %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    pubAddr = (UCHAR *)&pstNode->stIf.stIPv4.unSubnetMask;
    printf(", netmask %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    if (NIF_PPP == pstNode->stIf.enType)
    {
        pubAddr = (UCHAR *)&pstNode->stIf.stIPv4.unGateway;
        printf(", Point to Point %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    }
    else
    {
        pubAddr = (UCHAR *)&pstNode->stIf.stIPv4.unBroadcast;
        printf(", broadcast %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
    }
    pubAddr = (UCHAR *)&pstNode->stIf.stIPv4.unPrimaryDNS;
    printf(", Primary DNS %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
    pubAddr = (UCHAR *)&pstNode->stIf.stIPv4.unSecondaryDNS;
    printf(", Secondary DNS %d.%d.%d.%d]\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
#if PRINTF_THREAD_MUTEX
    os_thread_mutex_unlock(o_hMtxPrintf);
#endif
#endif

    return pstNode; 
}

void netif_del(PST_NETIF_NODE pstNode)
{
    //* 从网卡链表删除
    os_thread_mutex_lock(l_hMtxNetif);
    {
        PST_NETIF_NODE pstNextNode = l_pstNetifLink;
        PST_NETIF_NODE pstPrevNode = NULL; 
        while (pstNextNode)
        {
            if (pstNextNode == pstNode)
            {
                if (pstPrevNode)                
                    pstPrevNode->pstNext = pstNode->pstNext; 
                else
                    l_pstNetifLink = pstNode->pstNext; 
                break;
            }
            pstPrevNode = pstNextNode; 
            pstNextNode = pstNextNode->pstNext; 
        }
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    put_free_node(pstNode); 
}

void netif_del_ext(PST_NETIF pstNetif)
{
    PST_NETIF_NODE pstNode = NULL; 

    //* 从网卡链表删除
    os_thread_mutex_lock(l_hMtxNetif);
    {
        PST_NETIF_NODE pstNextNode = l_pstNetifLink;
        PST_NETIF_NODE pstPrevNode = NULL;
        while (pstNextNode)
        {
            if (&pstNextNode->stIf == pstNetif)
            {       
                pstNode = pstNextNode;
                if (pstPrevNode)
                    pstPrevNode->pstNext = pstNode->pstNext;
                else
                    l_pstNetifLink = pstNode->pstNext;
                break;
            }
            pstPrevNode = pstNextNode;
            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    if(pstNode)
        put_free_node(pstNode);
}

PST_NETIF netif_get_first(BOOL blIsForSending)
{
    PST_NETIF_NODE pstNode = NULL;
    os_thread_mutex_lock(l_hMtxNetif);
    {
        pstNode = l_pstNetifLink;
        if (pstNode && blIsForSending)        
            pstNode->stIf.bUsedCount++;         
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    if (pstNode)
        return &pstNode->stIf;
    else
        return NULL; 
}

PST_NETIF netif_get_by_ip(UINT unNetifIp, BOOL blIsForSending)
{
    os_thread_mutex_lock(l_hMtxNetif);
    {
        PST_NETIF_NODE pstNextNode = l_pstNetifLink;
        while (pstNextNode)
        {
            //* 如果主ip地址匹配，则立即返回，否则需要看看当前网卡类型是否是ethernet，如果是就需要查找附加ip地址链表，确定其是否匹配某个附加的地址
            if (unNetifIp == pstNextNode->stIf.stIPv4.unAddr)
            {
                if(blIsForSending)
                    pstNextNode->stIf.bUsedCount++;
                os_thread_mutex_unlock(l_hMtxNetif);

                return &pstNextNode->stIf; 
            }

        #if SUPPORT_ETHERNET
            //* ethernet网卡，则需要看看附加地址链表是否有匹配的ip地址了
            if (NIF_ETHERNET == pstNextNode->stIf.enType)
            {
                PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNextNode->stIf.pvExtra;
                PST_NETIF_ETH_IP_NODE pstNextIP = pstExtra->pstIPList;
                while (pstNextIP)
                {
                    if (unNetifIp == pstNextIP->unAddr)
                    {
                        if (blIsForSending)
                            pstNextNode->stIf.bUsedCount++;
                        os_thread_mutex_unlock(l_hMtxNetif);

                        return &pstNextNode->stIf;
                    }                    

                    pstNextIP = pstNextIP->pstNext;
                }
            }
        #endif

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    return NULL; 
}

PST_NETIF netif_get_by_name(const CHAR *pszIfName)
{
    os_thread_mutex_lock(l_hMtxNetif);
    {
        PST_NETIF_NODE pstNextNode = l_pstNetifLink;
        while (pstNextNode)
        {
            if (strcmp(pszIfName, pstNextNode->stIf.szName) == 0)
            {
                os_thread_mutex_unlock(l_hMtxNetif);

                return &pstNextNode->stIf; 
            }

            pstNextNode = pstNextNode->pstNext; 
        }
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    return NULL;
}

#if SUPPORT_ETHERNET
PST_NETIF netif_eth_get_by_genmask(UINT unDstIp, in_addr_t *punSrcIp, BOOL blIsForSending)
{
    PST_NETIF pstNetif = NULL;

    os_thread_mutex_lock(l_hMtxNetif);
    {
        PST_NETIF_NODE pstNextNode = l_pstNetifLink;
        while (pstNextNode)
        {
            if (NIF_ETHERNET == pstNextNode->stIf.enType)
            {
                //* 先本地寻址，网段匹配则直接返回
                if (ip_addressing(unDstIp, pstNextNode->stIf.stIPv4.unAddr, pstNextNode->stIf.stIPv4.unSubnetMask))
                {
                    pstNetif = &pstNextNode->stIf;
                    if (punSrcIp)
                        *punSrcIp = pstNetif->stIPv4.unAddr; 

                    break;
                }

                //* 然后再遍历附加地址链表，看看是否有匹配的网段吗
                PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNextNode->stIf.pvExtra; 
                PST_NETIF_ETH_IP_NODE pstNextIP = pstExtra->pstIPList; 
                while (pstNextIP)
                {
                    if (ip_addressing(unDstIp, pstNextIP->unAddr, pstNextIP->unSubnetMask)) 
                    {
                        pstNetif = &pstNextNode->stIf; 
                        if (punSrcIp)
                            *punSrcIp = pstNextIP->unAddr;

                        break;
                    }

                    pstNextIP = pstNextIP->pstNext; 
                } 
            }            

            pstNextNode = pstNextNode->pstNext; 
        }

		if (pstNetif && blIsForSending)
			pstNetif->bUsedCount++; 
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    return pstNetif; 
}
#endif

UINT netif_get_first_ip(void)
{
    UINT unNetifIp = 0; 
    os_thread_mutex_lock(l_hMtxNetif);
    {
        if (l_pstNetifLink)
            unNetifIp = l_pstNetifLink->stIf.stIPv4.unAddr;
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    return unNetifIp; 
}

void netif_used(PST_NETIF pstNetif)
{
    os_thread_mutex_lock(l_hMtxNetif);
    {
        pstNetif->bUsedCount++;
    }
    os_thread_mutex_unlock(l_hMtxNetif);
}

void netif_freed(PST_NETIF pstNetif)
{
    os_thread_mutex_lock(l_hMtxNetif);
    {
        pstNetif->bUsedCount--;
    }
    os_thread_mutex_unlock(l_hMtxNetif);
}

BOOL netif_is_ready(const CHAR *pszIfName)
{
    os_thread_mutex_lock(l_hMtxNetif);
    {
        PST_NETIF_NODE pstNextNode = l_pstNetifLink;
        while (pstNextNode)
        {
            if (strcmp(pszIfName, pstNextNode->stIf.szName) == 0)
            {                
                os_thread_mutex_unlock(l_hMtxNetif);

                if (pstNextNode->stIf.bUsedCount < 0)
                    return FALSE;

                return TRUE; 
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    return FALSE; 
}

UINT netif_get_source_ip_by_gateway(PST_NETIF pstNetif, UINT unGateway)
{
#if SUPPORT_ETHERNET
    if (NIF_ETHERNET == pstNetif->enType)
    {
        //* 先遍历附加地址链表，网段匹配则直接返回，否则直接使用网卡的主ip地址
        PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;
        PST_NETIF_ETH_IP_NODE pstNextIP = pstExtra->pstIPList;
        while (pstNextIP)
        {
            //* 附加地址网段匹配，同样直接返回，arp寻址依然以目标地址为寻址依据
            if (ip_addressing(unGateway, pstNextIP->unAddr, pstNextIP->unSubnetMask))
                return pstNextIP->unAddr;

            pstNextIP = pstNextIP->pstNext;
        }
    }    
#endif

    return pstNetif->stIPv4.unAddr;
}

#if SUPPORT_IPV6 
//* https://www.rfc-editor.org/rfc/rfc3484#section-5
PST_NETIF netif_eth_get_by_ipv6_prefix(UCHAR ubaDestination[16], UCHAR *pubSource, BOOL blIsForSending)
{
	PST_NETIF pstNetif, pstMatchedNetif = NULL;
	UCHAR *pubNetifIpv6 = NULL; 
	UCHAR ubMatchedBitsMax = 0;

	os_thread_mutex_lock(l_hMtxNetif);
	{
		PST_NETIF_NODE pstNextNode = l_pstNetifLink;
		while (pstNextNode)
		{
			if (NIF_ETHERNET == pstNextNode->stIf.enType)
			{
				UCHAR ubMatchedBits; 

				pstNetif = &pstNextNode->stIf;

				//* 先检索生成的动态地址
				PST_IPv6_DYNADDR pstNextAddr = NULL;
				do {
					//* 采用线程安全的函数读取地址节点，如果遍历所有节点，则不需要调用netif_ipv6_dyn_addr_release()函数释放该地址节点，调用下一个地址节点时上一个节点会被释放，这里选择
					//* 处于有效生存期间的地址节点，在这里不用担心有效生存时间到期后节点资源会被立即释放，这之后还会有IPv6ADDR_INVALID_TIME时间确保资源不会被立即回收，足够循环处理结束
					pstNextAddr = netif_ipv6_dyn_addr_next_safe(pstNetif, pstNextAddr, TRUE);
					if (pstNextAddr && (pstNextAddr->bitState == IPv6ADDR_PREFERRED || pstNextAddr->bitState == IPv6ADDR_DEPRECATED))
					{
						ubMatchedBits = ipv6_prefix_matched_bits(ubaDestination, pstNextAddr->ubaVal, pstNextAddr->bitPrefixBitLen);
						if (ubMatchedBits == pstNextAddr->bitPrefixBitLen) //* 前缀完全匹配则直接返回即可
						{					
							if (pubSource)
								memcpy(pubSource, pstNextAddr->ubaVal, 16); 							

							if (blIsForSending)
								pstNetif->bUsedCount++;

							os_thread_mutex_unlock(l_hMtxNetif);

							netif_ipv6_dyn_addr_release(pstNextAddr); 

							return pstNetif;
						}
						else
						{
							if (ubMatchedBits > ubMatchedBitsMax)
							{
								ubMatchedBitsMax = ubMatchedBits;
								pubNetifIpv6 = pstNextAddr->ubaVal; 
								pstMatchedNetif = pstNetif;  
							}
						}						
					}
				} while (pstNextAddr);


				//* 再比较本地链路地址是否前缀匹配
				ubMatchedBits = ipv6_prefix_matched_bits(ubaDestination, pstNetif->stIPv6.stLnkAddr.ubaVal, 64);
				if (ubMatchedBits < 64)
				{
					if (ubMatchedBits > ubMatchedBitsMax)
					{
						ubMatchedBitsMax = ubMatchedBits;
						pubNetifIpv6 = pstNetif->stIPv6.stLnkAddr.ubaVal; 
						pstMatchedNetif = pstNetif;
					}
				}
				else
				{
					if (pubSource)
						memcpy(pubSource, pstNetif->stIPv6.stLnkAddr.ubaVal, 16); 
					 
					break; 
				}					
			}

			pstNextNode = pstNextNode->pstNext;
		}

		if (pstNetif && blIsForSending)
			pstNetif->bUsedCount++;
	}
	os_thread_mutex_unlock(l_hMtxNetif);

	return pstNetif;
}
#endif
