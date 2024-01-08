/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.08.23 09:15
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 提供dns功能实现相关的操作函数
 *
 */
#ifndef DNS_H
#define DNS_H

#ifdef SYMBOL_GLOBALS
	#define DNS_EXT
#else
	#define DNS_EXT extern
#endif //* SYMBOL_GLOBALS

#if NETTOOLS_DNS_CLIENT

#define DNS_SRV_PORT        53  //* dns服务器端口，这是一个固定值
#define DNS_RCV_BUF_SIZE    384 //* 注意如果要查比较长的域名时请把该值调整为最大值512即可

//* dns查询报文标志字段
typedef union _UNI_DNS_FLAG_ {
    struct {
        USHORT reply_code    : 4; //* 响应码，表示响应的差错状态。0，无差错；1,格式错误；2，服务器自身原因无法处理这个请求；3，域名不存在；4，查询类型不支持；5，服务器拒绝；6~15，保留 
        USHORT resrved       : 3;
        USHORT recur_avail   : 1; //* 可用递归，响应报文有效。为 1 时，表示服务器支持递归查询，反之则不支持
        USHORT recur_desired : 1; //* 期望递归，查询报文设置，响应报文返回。为1，告诉名称服务器必须处理这个查询得到结果，这被称作递归查；0，且被请求的名称服务器没有一个授权回答，它将返回一个能解答该查询的其他名称服务器列表，这种方式被称为迭代查询
        USHORT truncated     : 1; //* 表示是否被截断。值为 1 时，表示响应已超过 512 字节并已被截断，只返回前 512 个字节。
        USHORT auth          : 1; //* 授权应答，响应报文有效。值为 1 时，表示名称服务器是权威服务器；值为 0 时，表示不是权威服务器
        USHORT opcode        : 4; //* 操作码，查询及响应报文均有效。0，标准查询；1，反向查询；2，服务器状态查询；其它值保留
        USHORT qr            : 1; //* 查询/响应标识。0，查询；1，响应        
    } stb16;
    USHORT usVal;
} UNI_DNS_FLAG, *PUNI_DNS_FLAG; 

//* dns报文头部结构体
PACKED_BEGIN
typedef struct _ST_DNS_HDR_ {
    USHORT usTransId; 
    USHORT usFlag; 
    USHORT usQuestions; 
    USHORT usAnswerRRs; 
    USHORT usAuthorityRRs; 
    USHORT usAdditionalRRs; 
} PACKED ST_DNS_HDR, *PST_DNS_HDR; 
PACKED_END

//* dns应答报文携带的Answers字段头部结构
PACKED_BEGIN
typedef struct _ST_DNS_ANSWER_HDR_ {
    USHORT usOffset;
    USHORT usType;
    USHORT usClass;
    UINT unTTL;
    USHORT usDataLen;    
} PACKED ST_DNS_ANSWER_HDR, *PST_DNS_ANSWER_HDR;
PACKED_END

//* dns应答报文携带的Answers字段头部结构
PACKED_BEGIN
typedef struct _ST_DNS_ANSWER_HDR_NONAME_ {    
    USHORT usType;
    USHORT usClass;
    UINT unTTL;
    USHORT usDataLen;
} PACKED ST_DNS_ANSWER_HDR_NONAME, *PST_DNS_ANSWER_HDR_NONAME; 
PACKED_END

//* 开启dns查询，返回值为dns客户端句柄，利用该句柄可以实现多次不同域名的dns查询
DNS_EXT INT dns_client_start(in_addr_t *punPrimaryDNS, in_addr_t *punSecondaryDNS, CHAR bRcvTimeout, EN_ONPSERR *penErr);
DNS_EXT void dns_client_end(INT nClient); 

//* 实现dns查询，参数pszDomainName指定要查询的域名，返回值为查询结果，如果地址为0，具体错误信息从penErr获得
DNS_EXT in_addr_t dns_client_query(INT nClient, in_addr_t unPrimaryDNS, in_addr_t unSecondaryDNS, const CHAR *pszDomainName, EN_ONPSERR *penErr);
#endif
#endif
