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

//* dhcp报文类型定义，具体定义参见相关rfc 2132第9.6节“DHCP Message Type”（http://mirrors.nju.edu.cn/rfc/beta/errata/rfc2132.html）
typedef enum {
    DHCPMSGTP_DISCOVER = 1, 
    DHCPMSGTP_OFFER = 2,
    DHCPMSGTP_REQUEST = 3,
    DHCPMSGTP_DECLINE = 4,
    DHCPMSGTP_ACK = 5,
    DHCPMSGTP_NAK = 6,
    DHCPMSGTP_RELEASE = 7,
    DHCPMSGTP_INFORM = 8,
} EN_DHCPMSGTYPE;

#define DHCP_OPT_REQUEST    1   //* dhcp操作码：请求
#define DHCP_OPT_REPLY      2   //* dhcp操作码：应答

#define DHCP_SRV_PORT       67  //* dhcp服务器端口
#define DHCP_CLT_PORT       68  //* dhcp客户端端口

#define DHCP_MAGIC_COOKIE       0x63825363  //* dhcp magic cookie字段值
#define DHCP_OPTIONS_LEN_MIN    60          //* dhcp选项域最小长度，选项低于该长度需要填充字符至最小长度，超过则需要确保选项字段能够16位字对齐，填充字符为00

//* dhcp协议帧头部结构体
PACKED_BEGIN
typedef struct _ST_DHCP_HDR_ {
    UCHAR ubOptCode;            //* 操作码，1: request; 2: reply
    UCHAR ubHardwareType;       //* 硬件地址类型，即mac地址类型，1代表这是这是常见
    UCHAR ubHardwareAddrLen;    //* 硬件地址长度
    UCHAR ubHops;               //* dhcp中继跳数,初始值为0，报文每经过一个dhcp中继，该字段值加1，未经过任何中继该值为0
    UINT unTransId;             //* 唯一标识本次dhcp请求的标识符，注意，本次请求所有报文标识符均一致
    USHORT usElapsedSecs;       //* 从发起请求到获取到ip地址共花费了多少秒，目前尚未使用，固定为0
    USHORT usFlags;             //* 广播应答标识位（第0位，仅该位被使用），标识dhcp服务器应答报文是采用单薄还是广播方式发送，0表示采用单薄发送方式，1表示采用广播发送方式，
                                //* 当该位置1，则客户端只接受服务器的广播应答报文，丢弃来自服务器的单播应答报文

    UINT unClientIp;            //* 客户端ip地址，如果客户机已有IP地址的话，客户机在发送请求时将自己的IP地址放在此处，反之则填0；
    UINT unYourIp;              //* 服务器分配给客户端的ip地址，由服务器把想要分配给客户端的地址填充到该字段；
    UINT unSrvIp;               //* 下一个dhcp服务器地址（用于bootstrap过程），协议栈不支持dhcp服务器中继，故这里不处理该字段，发送报文时固定填充0
    UINT unGatewayIp;           //* 网关地址，固定填0，协议栈不支持跨网段dhcp分配
    UCHAR ubaClientMacAddr[16]; //* 客户端mac地址
    CHAR szSrvName[64];         //* 服务器名称，是可选字段，由服务器在应答报文填入服务器主机名称，包含空字符结尾字符串，服务器可以选择不填（全0即可），在这里协议栈不处理也不理会该字段，发送报文时固定填充全0
    CHAR szBootFileName[128];   //* 引导文件名，协议栈不处理该字段，固定填充全0
    UINT unMagicCookie;         //* 固定为99.130.83.99（即0x63825363），网络字节序填充此字段，其作用显式地指示这是dhcp报文而不是bootp报文（dhcp基于bootp协议设计实现）
} PACKED ST_DHCP_HDR, *PST_DHCP_HDR; 
PACKED_END

//* dhcp选项结构体
//* =====================================================================================
//* 协议栈支持的dhcp选项定义
typedef enum {
    DHCPOPT_SUBNETMASK = 1,
    DHCPOPT_ROUTER = 3,
    DHCPOPT_DNS = 6,
    DHCPOPT_REQIP = 50,
    DHCPOPT_LEASETIME = 51,
    DHCPOPT_MSGTYPE = 53,
    DHCPOPT_SRVID = 54,
    DHCPOPT_REQLIST = 55, 
    DHCPOPT_RENEWALTIME = 58, 
    DHCPOPT_REBINDINGTIME = 59, 
    DHCPOPT_CLIENTID = 61,
    DHCPOPT_END = 255, 
} EN_DHCPOPTION;

PACKED_BEGIN
typedef struct _ST_DHCPOPT_HDR_ {
    UCHAR ubOption; 
    UCHAR ubLen; 
} PACKED ST_DHCPOPT_HDR, *PST_DHCPOPT_HDR;
PACKED_END

//* dhcp报文类型，具体报文类型定义参见EN_DHCPMSGTYPE宏定义
PACKED_BEGIN
typedef struct _ST_DHCPOPT_MSGTYPE_ {
    ST_DHCPOPT_HDR stHdr; 
    UCHAR ubTpVal; //* 报文类型值
} PACKED ST_DHCPOPT_MSGTYPE, *PST_DHCPOPT_MSGTYPE; 
PACKED_END

//* dhcp客户端标识，该选项携带客户端硬件地址类型及mac地址
PACKED_BEGIN
typedef struct _ST_DHCPOPT_CLTID_ {
    ST_DHCPOPT_HDR stHdr;
    UCHAR ubHardwareType; 
    UCHAR ubaMacAddr[ETH_MAC_ADDR_LEN];
} PACKED ST_DHCPOPT_CLTID, *PST_DHCPOPT_CLTID;
PACKED_END

//* dhcp服务器标识，该选项携带dhcp服务器的ip地址
PACKED_BEGIN
typedef struct _ST_DHCPOPT_SRVID_ {
    ST_DHCPOPT_HDR stHdr;
    UINT unSrvIp; 
} PACKED ST_DHCPOPT_SRVID, *PST_DHCPOPT_SRVID;
PACKED_END
//* =====================================================================================

#endif
