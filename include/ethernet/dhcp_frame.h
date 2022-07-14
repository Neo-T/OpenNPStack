/* dhcp_frame.h
 *
 * dhcp（Dynamic Host Configuration Protocol）协议帧结构定义
 *
 * Neo-T, 创建于2022.07.14 10:11
 * 版本: 1.0
 *
 */
#ifndef DHCP_FRAME_H
#define DHCP_FRAME_H

//* dhcp操作码定义，具体定义参见相关rfc2132第9.6节“DHCP Message Type”（http://mirrors.nju.edu.cn/rfc/beta/errata/rfc2132.html）
typedef enum {
    DHCPOPCODE_DISCOVER = 1, 
    DHCPOPCODE_OFFER = 2, 
    DHCPOPCODE_REQUEST = 3, 
    DHCPOPCODE_DECLINE = 4,
    DHCPOPCODE_ACK = 5, 
    DHCPOPCODE_NAK = 6, 
    DHCPOPCODE_RELEASE = 7,
    DHCPOPCODE_INFORM = 8,
} EN_DHCPOPCODE;

//* dhcp协议帧头部结构体
PACKED_BEGIN
typedef struct _ST_DHCP_HDR_ {
    USHORT usHardwareType;      //* 硬件类型
    USHORT usProtoType;         //* 协议类型
    UCHAR ubHardwareAddrLen;    //* 硬件地址长度
    UCHAR ubProtoAddrLen;       //* 协议地址长度
    USHORT usOptCode;           //* 操作码
} PACKED ST_DHCP_HDR, *PST_DHCP_HDR; 
PACKED_END

PACKED_BEGIN
typedef struct _ST_ETHIIARP_IPV4_ {
    ST_ARP_HDR stHdr; 
    UCHAR ubaSrcMacAddr[ETH_MAC_ADDR_LEN];
    UINT unSrcIPAddr; 
    UCHAR ubaDstMacAddr[ETH_MAC_ADDR_LEN];
    UINT unDstIPAddr; 
    UCHAR ubaPadding[18];  //* 填充字符，以满足ethernet帧数据域最小长度为46字节的要求
} PACKED ST_ETHIIARP_IPV4, *PST_ETHIIARP_IPV4;
PACKED_END

#endif
