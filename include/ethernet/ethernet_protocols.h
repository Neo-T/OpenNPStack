/*
 * Copyright 2022-2024 The Onps Project Author All Rights Reserved.
 *
 * Author：Neo-T, Created in 2022.06.14 16:39
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * http://www.onps.org.cn/apache2.0.txt
 *
 * ethernet协议解析相关的宏定义
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
