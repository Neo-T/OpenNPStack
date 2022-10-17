/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 保存udp链路相关信息的辅助功能函数
 *
 * Neo-T, 创建于2022.06.01 16:59
 *
 */
#ifndef UDP_LINK_H
#define UDP_LINK_H

#ifdef SYMBOL_GLOBALS
	#define UDP_LINK_EXT
#else
	#define UDP_LINK_EXT extern
#endif //* SYMBOL_GLOBALS

typedef struct _ST_UDPLINK_ {
    CHAR bIdx;
    CHAR bNext;

    struct {
        USHORT usPort;  //* 端口
        in_addr_t unIp; //* 地址            
    } stPeerAddr;
} ST_UDPLINK, *PST_UDPLINK;

//* 到达的udp报文
typedef struct _ST_RCVED_UDP_PACKET_ ST_RCVED_UDP_PACKET, *PST_RCVED_UDP_PACKET;
typedef struct _ST_RCVED_UDP_PACKET_ {
    USHORT usLen;
    USHORT usFromPort;
    in_addr_t unFromIP;
    PST_RCVED_UDP_PACKET pstNext;
} ST_RCVED_UDP_PACKET, *PST_RCVED_UDP_PACKET;

UDP_LINK_EXT BOOL udp_link_init(EN_ONPSERR *penErr); 
UDP_LINK_EXT void udp_link_uninit(void); 
UDP_LINK_EXT PST_UDPLINK udp_link_get(EN_ONPSERR *penErr);
UDP_LINK_EXT void udp_link_free(PST_UDPLINK pstUdpLink);

#endif
