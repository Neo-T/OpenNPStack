/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 实现sntp协议，提供网络对时功能
 * 
 * Neo-T, 创建于2022.08.27 17:00
 *
 */
#ifndef SNTP_H
#define SNTP_H

#ifdef SYMBOL_GLOBALS
	#define SNTP_EXT
#else
	#define SNTP_EXT extern
#endif //* SYMBOL_GLOBALS

#if NETTOOLS_SNTP

#define SNTP_SRV_PORT       123         //* sntp服务器端口，这是一个固定值
#define DIFF_SEC_1900_1970  2208988800  //* 1900到1970年之间的秒数

//* dns查询报文标志字段
typedef union _UNI_SNTP_FLAG_ {
    struct {
        UCHAR mode : 3; //* 0，保留；1，主动对等体模式；2，被动对等体模式；3，客户端模式；4，服务器模式；5，广播模式；6，ntp控制报文；7，内部保留        
        UCHAR ver  : 3; //* 版本号        
        UCHAR li   : 2; //* 00，无告警；01，最后一分钟包含61秒；10，最后一分钟包含59秒；11，警告（时钟未同步）        
    } stb8;
    UCHAR ubVal;
} UNI_SNTP_FLAG, *PUNI_SNTP_FLAG; 

//* ntp数据报文格式
PACKED_BEGIN
typedef struct _ST_SNTP_DATA_ {
    UCHAR ubFlag;               //* 标志
    UCHAR ubStratum;            //* 时钟层数
    UCHAR ubPoll;               //* 轮询时间，即两个连续ntp报文之间的时间间隔，用2的幂来表示，比如值为6表示最小间隔为2^6 = 64s
    UCHAR ubPrecision;          //* 时钟精度
    UINT unRootDelay;           //* 到主参考时钟的总往返延迟时间
    UINT unRootDispersion;      //* 本地时钟相对于主参考时钟的最大误差
    UINT unRefId;               //* 特定参考时钟标识
    LONGLONG llRefTimestamp;    //* 本地时钟最后一次被设定或更新的时间，如果值为0表示本地时钟从未被同步
    LONGLONG llOrigiTimestamp;  //* NTP报文离开源端时的本地时间
    LONGLONG llRcvTimestamp;    //* NTP报文到达目的端的本地时间
    LONGLONG llTransTimestatmp; //* 目的端应答报文离开服务器端的本地时间
} PACKED ST_SNTP_DATA, *PST_SNTP_DATA; 
PACKED_END

//* 参数pszIp指定ntp服务器地址，函数指针pfunTime实现读取本地时间（unxi时间戳）可以为空，pfunSetSysTime同样是一个函数指针，用于时间同步成功后设置本地时间，
//* 参数bTimeZone用于指定时区（比如东八区则指定8，西八区则指定-8），参数penErr用于接收错误码（如果函数执行失败的话），同步成功则返回TRUE，反之FALSE
SNTP_EXT time_t sntp_update_by_ip(const CHAR *pszNtpSrvIp, time_t(*pfunTime)(void), void(*pfunSetSysTime)(time_t), CHAR bTimeZone, EN_ONPSERR *penErr); 
SNTP_EXT time_t sntp_update(in_addr_t unNtpSrvIp, time_t(*pfunTime)(void), void(*pfunSetSysTime)(time_t), CHAR bTimeZone, EN_ONPSERR *penErr);
#if NETTOOLS_DNS_CLIENT
SNTP_EXT time_t sntp_update_by_dns(const CHAR *pszDomainName, time_t(*pfunTime)(void), void(*pfunSetSysTime)(time_t), CHAR bTimeZone, EN_ONPSERR *penErr);
#endif
#endif
#endif
