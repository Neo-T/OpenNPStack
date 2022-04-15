#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "utils.h"

#define SYMBOL_GLOBALS
#include "netif/netif.h"
#undef SYMBOL_GLOBALS

static ST_NETIF_NODE l_staNetifNode[NETIF_NUM]; 
static PST_NETIF_NODE l_pstFreeNode = NULL; 
static PST_NETIF_NODE l_pstNetifLink = NULL; 
static HMUTEX l_hMtxNetif = INVALID_HMUTEX;
BOOL netif_init(EN_ERROR_CODE *penErrCode)
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

PST_NETIF_NODE netif_add(EN_NETIF enType, const CHAR *pszName, PST_IPV4 pstIPv4, PFUN_NETIF_SEND pfunSend, void *pvExtra, EN_ERROR_CODE *penErrCode)
{
    PST_NETIF_NODE pstNode = get_free_node(); 
    if (NULL == pstNode)
    {
        if (penErrCode)
            *penErrCode = ERRNONETIFNODE;

        return NULL; 
    }

    //* 保存网卡相关信息
    pstNode->stIf.enType   = enType;  
    pstNode->stIf.pfunSend = pfunSend; 
    pstNode->stIf.stIPv4   = *pstIPv4; 
    pstNode->stIf.pvExtra  = pvExtra; 
    snprintf(pstNode->stIf.szName, sizeof(pstNode->stIf.szName), "%s", pszName);

    //* 加入链表
    os_thread_mutex_lock(l_hMtxNetif);
    {
        pstNode->pstNext = l_pstNetifLink;
        l_pstNetifLink = pstNode; 
    }
    os_thread_mutex_unlock(l_hMtxNetif); 

#if SUPPORT_PRINTF
    UCHAR *pubAddr = (UCHAR *)&pstNode->stIf.stIPv4.unAddr;
    printf("%s added to the protocol stack\r\n", pstNode->stIf.szName);
    printf("inet %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
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
    printf(", Secondary DNS %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
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

PST_NETIF netif_get(void)
{
    PST_NETIF_NODE pstNextNode = l_pstNetifLink;
    if (pstNextNode)
        return &pstNextNode->stIf; 

    return NULL; 
}
