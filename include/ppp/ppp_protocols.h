/* ppp_protocols.h
 *
 * ppp协议解析相关的宏定义：包括支持的上层协议、协议头相关的标志字符串、地址、MRU等定义
 *
 * Neo-T, 创建于2022.03.21 10:19
 * 版本: 1.0
 *
 */
#ifndef PPP_PROTOCOLS_H
#define PPP_PROTOCOLS_H
#include "protocols.h"

#define	PPP_ALLSTATIONS	0xFF	//* All-Stations broadcast address 
#define	PPP_UI			0x03	//* Unnumbered Information
#define	PPP_FLAG		0x7E	//* Flag Sequence
#define	PPP_ESCAPE		0x7D	//* Asynchronous Control Escape
#define	PPP_TRANS		0x20	//* Asynchronous transparency modifier

#define PPP_INITFCS		0xFFFF	//* Initial FCS value
#define PPP_MRU			1500	//* default MRU = max length of info field

//* PPP支持的上层协议值定义
#define PPP_IP			0x21	//* Internet Protocol
#define PPP_AT			0x29	//* AppleTalk Protocol
#define PPP_IPX			0x2B	//* IPX protocol
#define	PPP_VJC_COMP	0x2D	//* VJ compressed TCP
#define	PPP_VJC_UNCOMP	0x2F	//* VJ uncompressed TCP
#define PPP_MP			0x3D	//* Multilink protocol
#define PPP_IPV6		0x57	//* Internet Protocol Version 6
#define PPP_COMPFRAG	0xFB	//* fragment compressed below bundle
#define PPP_COMP		0xFD	//* compressed packet
#define PPP_MPLS_UC		0x0281	//* Multi Protocol Label Switching - Unicast
#define PPP_MPLS_MC		0x0283	//* Multi Protocol Label Switching - Multicast
#define PPP_IPCP		0x8021	//* IP Control Protocol
#define PPP_ATCP		0x8029	//* AppleTalk Control Protocol
#define PPP_IPXCP		0x802B	//* IPX Control Protocol
#define PPP_IPV6CP		0x8057	//* IPv6 Control Protocol
#define PPP_CCPFRAG		0x80FB	//* CCP at link level (below MP bundle)
#define PPP_CCP			0x80FD	//* Compression Control Protocol
#define PPP_MPLSCP		0x80FD	//* MPLS Control Protocol
#define PPP_LCP			0xC021	//* Link Control Protocol
#define PPP_PAP			0xC023	//* Password Authentication Protocol
#define PPP_LQR			0xC025	//* Link Quality Report protocol
#define PPP_CHAP		0xC223	//* Cryptographic Handshake Auth. Protocol
#define PPP_CBCP		0xC029	//* Callback Control Protocol

//* ppp栈支持的上层协议
typedef struct _ST_PPP_PROTOCOL_ {	
	USHORT usType; 
	void (*pfunUpper)(HTTY hTTY, UCHAR *pubPacket, INT nPacketLen, EN_ERROR_CODE *penErrCode);	
} ST_PPP_PROTOCOL, *PST_PPP_PROTOCOL;

#endif