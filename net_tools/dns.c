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
    INT nBytesOf1stSeg;     
    UCHAR *pubDns = buddy_alloc(sizeof(ST_DNS_HDR) + strlen(pszDomainName) + 2 + 4, penErr);
    if (pubDns)
    {
        UCHAR *pubQueries = pubDns; 
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

in_addr_t dns_query(INT nClient, const CHAR *pszDomainName, EN_ONPSERR *penErr)
{
    UCHAR *pubQueryPkt = dns_queries_encap(pszDomainName, penErr); 
    if (!pubQueryPkt)
        return; 

    //* 封装头

    //* 释放刚才申请的内存
    buddy_free(pubQueryPkt); 
}
