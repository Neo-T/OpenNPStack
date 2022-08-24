#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "ip/udp.h"
#define SYMBOL_GLOBALS
#include "net_tools/dns.h"
#undef SYMBOL_GLOBALS

INT dns_client_start(in_addr_t *punPrimaryDNS, in_addr_t *punSecondaryDNS, EN_ONPSERR *penErr)
{
    INT nClient = onps_input_new(IPPROTO_UDP, penErr);
    if (nClient < 0)
        return nClient; 

    //* 获取缺省网卡列表
    PST_NETIF pstNetif = route_get_default(); 
    if (!pstNetif)
    {
        if (penErr)
            *penErr = ERRADDRESSING;

        onps_input_free(nClient); 
        return -1; 
    }

    *punPrimaryDNS = pstNetif->stIPv4.unPrimaryDNS; 
    *punSecondaryDNS = pstNetif->stIPv4.unSecondaryDNS; 

    return nClient; 
}

void dns_client_end(INT nClient)
{
    onps_input_free(nClient); 
}

//* 封装dns查询类报文
static UCHAR *dns_queries_encap(const CHAR *pszDomainName, EN_ONPSERR *penErr)
{
    //* 申请一块内存，然后根据协议封装必要的字段  
    UCHAR *pubDns = buddy_alloc(sizeof(ST_DNS_HDR) + strlen(pszDomainName) + 2 + 4, penErr);
    if (pubDns)
    {
        UCHAR *pubQueries = pubDns + sizeof(ST_DNS_HDR);
        INT i, k, n = 0, nWriteIdx = 0;
        INT nDomainNameLen = strlen(pszDomainName);
        for (i = 0, k = 1; i < nDomainNameLen; i++, k++)
        {
            if (pszDomainName[i] == '.')
            {
                pubQueries[nWriteIdx] = n;
                nWriteIdx = i + 1;
                n = 0;
            }
            else
            {
                n++;
                pubQueries[k] = pszDomainName[i];
            }
        }

        if (i > nWriteIdx)
            pubQueries[nWriteIdx] = n;
        pubQueries[i + 1] = 0x00;

        //* 查询类型（ipv4地址）
        pubQueries[i + 2] = 0x00; 
        pubQueries[i + 3] = 0x01; 

        //* 地址类型（互联网地址）
        pubQueries[i + 4] = 0x00;
        pubQueries[i + 5] = 0x01; 

        return pubDns; 
    }

    return NULL; 
}

in_addr_t dns_client_query(INT nClient, in_addr_t unPrimaryDNS, in_addr_t unSecondaryDNS, const CHAR *pszDomainName, EN_ONPSERR *penErr)
{
    in_addr_t unDnsSrvIp = inet_addr("192.168.3.210")/*unPrimaryDNS*/;
    UCHAR *pubDnsQueryPkt = dns_queries_encap(pszDomainName, penErr); 
    if (!pubDnsQueryPkt)
        return 0; 

    //* 封装头    
    PST_DNS_HDR pstHdr = (PST_DNS_HDR)pubDnsQueryPkt; 
    pstHdr->usTransId = (USHORT)(rand() % (0xFFFF + 1)); 
    UNI_DNS_FLAG uniFlag; 
    uniFlag.usVal = 0; 
    uniFlag.stb16.recur_desired = 1;  //* 递归查询
    pstHdr->usFlag = htons(uniFlag.usVal); 
    pstHdr->usQuestions = htons(1); 
    pstHdr->usAnswerRRs = 0; 
    pstHdr->usAuthorityRRs = 0; 
    pstHdr->usAdditionalRRs = 0; 

    //* 发送查询报文，2为单个域名首部长度字节+单个域名尾部结束字节，4为紧跟域名的查询类型与地址类型字段，这两个字段各占两个字节
    INT nSendBytes = udp_sendto(nClient, unDnsSrvIp, DNS_SRV_PORT, pubDnsQueryPkt, sizeof(ST_DNS_HDR) + strlen(pszDomainName) + 2 + 4);     
    if (nSendBytes < 0)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("udp_sendto() failed, %s\r\n", onps_get_last_error(nClient, NULL));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
    }

    //* 释放刚才申请的内存
    buddy_free(pubDnsQueryPkt); 

    return 0; 
}
