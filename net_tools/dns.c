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

INT dns_client_start(in_addr_t *punPrimaryDNS, in_addr_t *punSecondaryDNS, CHAR bRcvTimeout, EN_ONPSERR *penErr)
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

    if (!onps_input_set(nClient, IOPT_SETRCVTIMEOUT, &bRcvTimeout, penErr))
    {
        onps_input_free(nClient); 
        return -1; 
    }

    *punPrimaryDNS = htonl(pstNetif->stIPv4.unPrimaryDNS); 
    *punSecondaryDNS = htonl(pstNetif->stIPv4.unSecondaryDNS); 

    return nClient; 
}

void dns_client_end(INT nClient)
{
    onps_input_free(nClient); 
}

//* 封装dns查询类报文
static void dns_queries_encap(UCHAR *pubQueries, const CHAR *pszDomainName, EN_ONPSERR *penErr)
{    
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
}

//* 查询dns
in_addr_t dns_client_query(INT nClient, in_addr_t unPrimaryDNS, in_addr_t unSecondaryDNS, const CHAR *pszDomainName, EN_ONPSERR *penErr)
{    
    UCHAR ubRetryNum = 0; 
    USHORT usTransId; 
    in_addr_t unRtnAddr = 0; 
    INT nSndBytes, nRcvBytes;
    PST_DNS_HDR pstHdr; 
    UNI_DNS_FLAG uniFlag; 
    in_addr_t unDnsSrvIp = /*inet_addr("192.168.3.210")*/unPrimaryDNS;

    //* 获取接收等待时长
    CHAR bRcvTimeout;
    if (!onps_input_get(nClient, IOPT_GETRCVTIMEOUT, &bRcvTimeout, penErr))            
        return 0; 

    //* 申请一块内存，然后根据协议封装必要的字段  
    UCHAR *pubDnsQueryPkt = buddy_alloc(DNS_RCV_BUF_SIZE/*sizeof(ST_DNS_HDR) + strlen(pszDomainName) + 2 + 4*/, penErr);
    if (pubDnsQueryPkt)    
        pstHdr = (PST_DNS_HDR)pubDnsQueryPkt;     
    else
        return 0;     

__lblSend: 
    ubRetryNum++; 
    if (ubRetryNum > 6)
    {
        if (penErr)
            *penErr = ERRWAITACKTIMEOUT; 
        goto __lblEnd; 
    }

    //* 封装    
    dns_queries_encap(pubDnsQueryPkt + sizeof(ST_DNS_HDR), pszDomainName, penErr);

    //* 封装头
    usTransId = (USHORT)(rand() % (0xFFFF + 1)); 
    pstHdr->usTransId = usTransId;
    uniFlag.usVal = 0; 
    uniFlag.stb16.recur_desired = 1;  //* 递归查询
    pstHdr->usFlag = htons(uniFlag.usVal); 
    pstHdr->usQuestions = htons(1); 
    pstHdr->usAnswerRRs = 0; 
    pstHdr->usAuthorityRRs = 0; 
    pstHdr->usAdditionalRRs = 0;     

    //* 发送查询报文，2为单个域名首部长度字节+单个域名尾部结束字节，4为紧跟域名的查询类型与地址类型字段，这两个字段各占两个字节
    nSndBytes = udp_sendto(nClient, unDnsSrvIp, DNS_SRV_PORT, pubDnsQueryPkt, sizeof(ST_DNS_HDR) + strlen(pszDomainName) + 2 + 4);
    if (nSndBytes < 0)
    {
        onps_get_last_error(nClient, penErr); 
        goto __lblEnd; 
    } 

    //* 等待应答报文
    nRcvBytes = udp_recv_upper(nClient, pubDnsQueryPkt, DNS_RCV_BUF_SIZE, NULL, NULL, bRcvTimeout); 
    if (nRcvBytes > 0)
    {
        //* 事务Id匹配且为响应报文则说明是针对当前查询的应答
        uniFlag.usVal = pstHdr->usFlag; 
        if (usTransId == pstHdr->usTransId && uniFlag.stb16.qr)
        {
            //* 响应码无差错，则报文有效，可以解析地址了
            if (!uniFlag.stb16.reply_code)
            {

            }
            else
            {
                if (penErr)
                {
                    switch (uniFlag.stb16.reply_code)
                    {
                    case 1:
                        *penErr = ERRDNSQUERYFMT; 
                        break; 

                    case 2: 
                        *penErr = ERRDNSSRV; 
                        break; 

                    case 3:
                        *penErr = ERRDNSNAME; 
                        break; 

                    case 4:
                        *penErr = ERRDNSQUERYTYPE; 
                        break; 

                    case 5:
                        *penErr = ERRDNSREFUSED; 
                        break; 
                    }
                }                
            }
        }
    }
    else
    {
        if (!nRcvBytes)
        {
            if (unDnsSrvIp == unPrimaryDNS)
                unDnsSrvIp = unSecondaryDNS; 
            else
                unDnsSrvIp = unPrimaryDNS; 

            goto __lblSend; 
        }
    }

__lblEnd: 
    //* 释放刚才申请的内存
    buddy_free(pubDnsQueryPkt); 

    return unRtnAddr;
}
