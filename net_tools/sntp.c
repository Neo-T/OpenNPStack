/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "ip/udp.h"
#include "net_tools/dns.h"
#define SYMBOL_GLOBALS
#include "net_tools/sntp.h"
#undef SYMBOL_GLOBALS

#if NETTOOLS_SNTP
time_t sntp_update(in_addr_t unNtpSrvIp, time_t(*pfunTime)(void), void(*pfunSetSysTime)(time_t), CHAR bTimeZone, EN_ONPSERR *penErr)
{
    UNI_SNTP_FLAG uniFlag;
    ST_SNTP_DATA stData;
    UCHAR ubRetryNum = 0;
    INT nSndBytes, nRcvBytes;
    time_t tTimestamp = 0; 
    LONGLONG llTransTimestatmp;

    //* 新建一个udp客户端
#if SUPPORT_IPV6
	INT nClient = onps_input_new(AF_INET, IPPROTO_UDP, penErr);
#else
    INT nClient = onps_input_new(IPPROTO_UDP, penErr);
#endif
    if (nClient < 0)
        return 0;

    CHAR bRcvTimeout = 3;
    if (!onps_input_set(nClient, IOPT_SETRCVTIMEOUT, &bRcvTimeout, penErr))
        goto __lblEnd;

__lblSend:
    ubRetryNum++;
    if (ubRetryNum > 3)
    {
        if (penErr)
            *penErr = ERRWAITACKTIMEOUT;
        goto __lblEnd;
    }

    //* 填充报文    
    uniFlag.stb8.li = 0;
    uniFlag.stb8.ver = 3;
    uniFlag.stb8.mode = 3;
    stData.ubFlag = uniFlag.ubVal;
    stData.ubStratum = 0;
    stData.ubPoll = 1;              //* 2^1 = 2秒
    stData.ubPrecision = 0/*-16*/;  //* 时钟精度 2^(-16) = 152.58微秒，也可以置为0
    stData.unRootDelay = 0;
    stData.unRootDispersion = 0;
    stData.unRefId = 0;
    stData.llRefTimestamp = 0;
    stData.llOrigiTimestamp = 0;
    stData.llRcvTimestamp = 0;
    llTransTimestatmp = pfunTime ? (LONGLONG)pfunTime() : 0;
    stData.llTransTimestatmp = htonll(((LONGLONG)DIFF_SEC_1900_1970 + llTransTimestatmp) << 32);

    //* 发送对时报文    
    nSndBytes = udp_sendto(nClient, unNtpSrvIp, SNTP_SRV_PORT, (UCHAR *)&stData, sizeof(ST_SNTP_DATA));
    if (nSndBytes < 0)
    {
        onps_get_last_error(nClient, penErr);
        goto __lblEnd;
    }

    //* 等待应答    
    nRcvBytes = udp_recv_upper(nClient, (UCHAR *)&stData, sizeof(stData), NULL, NULL, bRcvTimeout);
    if (nRcvBytes > 0)
    {
        //* 非常简单的处理逻辑，就是本身单片机系统的时钟就不够精确，同时，现代网络已经足够快，所以这里直接用服务器端的应答报文离开时间作为校准时间
        LONGLONG llSrvTime = htonll(stData.llTransTimestatmp);
        PUNI_LONG_LONG puniSrvTime = (PUNI_LONG_LONG)&llSrvTime;
        tTimestamp = puniSrvTime->stInt64.h - DIFF_SEC_1900_1970 + (((INT)bTimeZone) * 3600);
        pfunSetSysTime(tTimestamp);
        
        goto __lblEnd;
    }
    else
    {
        if (nRcvBytes < 0)
            goto __lblEnd;
    }

    goto __lblSend;

__lblEnd:
    onps_input_free(nClient);
    return tTimestamp;
}

time_t sntp_update_by_ip(const CHAR *pszNtpSrvIp, time_t(*pfunTime)(void), void(*pfunSetSysTime)(time_t), CHAR bTimeZone, EN_ONPSERR *penErr)
{
    return sntp_update(inet_addr(pszNtpSrvIp), pfunTime, pfunSetSysTime, bTimeZone, penErr);
}

#if NETTOOLS_DNS_CLIENT
time_t sntp_update_by_dns(const CHAR *pszDomainName, time_t(*pfunTime)(void), void(*pfunSetSysTime)(time_t), CHAR bTimeZone, EN_ONPSERR *penErr)
{
    time_t tRtnVal = 0; 
    in_addr_t unPrimaryDNS, unSecondaryDNS;
    INT nDnsClient = dns_client_start(&unPrimaryDNS, &unSecondaryDNS, 3, penErr); 
    if (nDnsClient < 0)
        return 0; 

    //* 查询ntp服务器地址
    in_addr_t unNtpSrvIp = dns_client_query(nDnsClient, unPrimaryDNS, unSecondaryDNS, pszDomainName, penErr); 
    dns_client_end(nDnsClient);

    //* 获得服务器地址则同步时间
    if (unNtpSrvIp)    
        tRtnVal = sntp_update(unNtpSrvIp, pfunTime, pfunSetSysTime, bTimeZone, penErr);
    
    return tRtnVal;
}
#endif
#endif
