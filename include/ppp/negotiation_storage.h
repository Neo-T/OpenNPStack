/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.03.25 10:26
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * 用于保存链路协商结果的存储类结构体及其相关辅助宏定义
 *
 */
#ifndef NEGOTIATION_STORAGE_H
#define NEGOTIATION_STORAGE_H

#ifdef SYMBOL_GLOBALS
	#define NEGOTIATION_STORAGE_EXT
#else
	#define NEGOTIATION_STORAGE_EXT extern
#endif //* SYMBOL_GLOBALS
#include "ppp_utils.h"
#include "wait_ack_list.h"

//* LCP相关配置项初始值
#define ACCM_INIT		0	//* ACCM初始值，缺省0~31全部不进行转义

 //* IPCP相关配置项初始值
#define IP_ADDR_INIT	0
#define MASK_INIT		0xFFFFFFFF //* 子网掩码强制为255.255.255.255
#define DNS_ADDR_INIT	0

 //* ppp链路工作状态
typedef enum {
	TTYINIT = 0,		//* tty终端初始化
	STARTNEGOTIATION,	//* 开启链路协商
	NEGOTIATION,		//* 协商
	LCPCONFREQ,			//* LCP配置协商
	STARTAUTHEN,		//* 开始认证
	AUTHENTICATE,		//* 认证中
	AUTHENFAILED,		//* 认证失败
	AUTHENTIMEOUT,		//* 认证超时，一直未收到对端下发的challenge报文
	SENDIPCPCONFREQ,	//* 发送IP层配置协商请求报文
	WAITIPCPCONFACK,	//* 等待IP层配置协商请求应答报文
	ESTABLISHED,		//* 链路已建立
	SENDECHOREQ,		//* 发送echo，链路保持
	WAITECHOREPLY,		//* 等待echo应答
	SENDTERMREQ,		//* 发送终止链路请求
	WAITTERMACK,		//* 等待终止请求应答
	TERMINATED, 		//* 链路已终止	
	STACKFAULT,			//* 协议栈严重故障阶段（软件BUG导致）
} EN_PPP_LINK_STATE;

//* 记录协商结果
typedef struct _ST_PPPNEGORESULT_ {
	struct {
		UINT unMagicNum;
		USHORT usMRU;
		UINT unACCM;
		struct {
			USHORT usType;
			UCHAR ubaData[16];
		} stAuth;
		BOOL blIsProtoComp;
		BOOL blIsAddrCtlComp;
		BOOL blIsNegoValOfProtoComp;
		BOOL blIsNegoValOfAddrCtlComp;
	} stLCP;
	struct {
		UINT unAddr;
		UINT unPrimaryDNS;
		UINT unSecondaryDNS;
		UINT unPointToPointAddr;
		UINT unSubnetMask;
	} stIPCP;
	UCHAR ubIdentifier; 
	UINT unLastRcvedSecs; 
} ST_PPPNEGORESULT, *PST_PPPNEGORESULT;

//* PPP接口控制块
typedef struct _STCB_PPP_ {
	HTTY hTTY;	
	EN_PPP_LINK_STATE enState;
	PST_PPPNEGORESULT pstNegoResult;
	ST_PPPWAITACKLIST stWaitAckList;	
} STCB_PPP, *PSTCB_PPP; 

//* LCP/NCP协议配置请求项处理器相关宏、处理函数及结构体定义
typedef INT(*PFUN_PUTREQITEM)(UCHAR *pubFilled, PST_PPPNEGORESULT pstNegoResult);
typedef INT(*PFUN_GETREQVAL)(UCHAR *pubItem, UCHAR *pubVal, PST_PPPNEGORESULT pstNegoResult);
typedef struct _ST_LNCP_CONFREQ_ITEM_ {
	UCHAR ubType;
	const CHAR *pszName; 
	BOOL blIsNegoRequired;		//* 是否需要协商，生成初始配置请求报文时需要
	PFUN_PUTREQITEM pfunPut;	//* 填充请求内容到缓冲区，包括请求类型、长度及数据（如果需要携带数据的话）
	PFUN_GETREQVAL pfunGet;		//* 从收到的配置请求报文中读取协商值    
} ST_LNCP_CONFREQ_ITEM, *PST_LNCP_CONFREQ_ITEM;

//* LCP/NCP协商处理器，其针对报文代码域携带的值分别进行特定处理，在这里定义处理器相关的基础数据结构、宏、处理函数等定义
typedef void(*PFUN_LNCPNEGOHANDLER)(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
typedef struct _ST_LCPNEGOHANDLER_ {
	EN_CPCODE enCode;
	PFUN_LNCPNEGOHANDLER pfunHandler;
} ST_LNCPNEGOHANDLER, *PST_LNCPNEGOHANDLER; 

//* ppp栈支持的上层协议
typedef struct _ST_PPP_PROTOCOL_ {
	USHORT usType;
	void(*pfunUpper)(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen);
} ST_PPP_PROTOCOL, *PST_PPP_PROTOCOL;

#endif
