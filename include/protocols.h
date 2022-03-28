/* protocols.h
 *
 * 协议栈支持的通讯协议类型值
 *
 * Neo-T, 创建于2022.03.26 10:00
 * 版本: 1.0
 *
 */
#ifndef PROTOCOLS_H
#define PROTOCOLS_H

//* 协议栈支持的通讯协议类型值，其用于业务逻辑实现
typedef enum {
	LCP = 0, 
	PAP, 
	CHAP, 
	IPCP, 
#if SUPPORT_IPV6
	IPV6CP, 
#endif
	IPV4, 
#if SUPPORT_IPV6
	IPV6, 
#endif

	ICMP, 
	ARP, 
	TCP, 
	UDP, 
} EN_NPSPROTOCOL; 

#endif