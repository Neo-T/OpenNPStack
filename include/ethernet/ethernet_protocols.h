/* ethernet_protocols.h
 *
 * ethernet协议解析相关的宏定义
 *
 * Neo-T, 创建于2022.06.14 16:39
 * 版本: 1.0
 *
 */
#ifndef ETHERNET_PROTOCOLS_H
#define ETHERNET_PROTOCOLS_H
#include "protocols.h"

//* ethernet ii协议支持的上层协议值定义
#define ETHII_IP    0x0800	//* Internet Protocol Version 4
#define ETHII_ARP   0x0806	//* Address Resolution Protocol
#define ETHII_RARP  0x0835	//* Reserver Address Resolution Protocol
#define ETHII_IPV6  0x86DD	//* Internet Protocol Version 6

#endif
