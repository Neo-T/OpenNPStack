#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"

#define SYMBOL_GLOBALS
#include "netif/netif.h"
#undef SYMBOL_GLOBALS

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
    pstNode->stIf.stIPv4     = *pstIPv4; 
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

#if SUPPORT_PRINTF
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
            if (unNetifIp == pstNextNode->stIf.stIPv4.unAddr)
            {
                if(blIsForSending)
                    pstNextNode->stIf.bUsedCount++;
                os_thread_mutex_unlock(l_hMtxNetif);

                return &pstNextNode->stIf; 
            }

            pstNextNode = pstNextNode->pstNext;
        }
    }
    os_thread_mutex_unlock(l_hMtxNetif);

    return NULL; 
}

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

void netif_used_count_decrement(PST_NETIF pstNetif)
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
