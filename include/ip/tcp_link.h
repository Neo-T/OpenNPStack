/* tcp_link.h
 *
 * 保存tcp链路相关信息的辅助功能函数
 *
 * Neo-T, 创建于2022.04.28 14:28
 * 版本: 1.0
 *
 */
#ifndef TCP_LINK_H
#define TCP_LINK_H

#ifdef SYMBOL_GLOBALS
	#define TCP_LINK_EXT
#else
	#define TCP_LINK_EXT extern
#endif //* SYMBOL_GLOBALS

typedef enum {
    TLSINVALID, //* TCP链路无效（尚未申请一个有效的tcp link节点）
    TLSINIT,    //* TCP链路初始状态

    //* 以下为Socket被用于TCP Client时的状态定义
    TLSSYNSENT,             //* 发送SYN请求
    TLSRCVEDSYNACK,         //* 收到SYN ACK
    TLSRCVSYNACKTIMEOUT,    //* 等待接收SYN ACK报文超时
    TLSSYNACKACKSENTFAILED, //* 给服务器发送SYN ACK的ACK报文失败
    TLSCONNECTED,           //* 已连接
    TLSRESET,               //* 连接被重置
    TLSFINSENT,             //* FIN已发送
    TLSRCVEDFINACK,         //* 收到FIN ACK
    TLSCLOSED,              //* 已关闭    

    //* 以下为Socket被用于TCP Server时的状态定义
    TLSSRVSTARTED,  //* TCP Server已启动
    TLSSRVDOWN,     //* TCP Server已关闭
} EN_TCPLINKSTATE;

typedef struct _ST_TCPLINK_ {
    CHAR bState;    //* 当前链路状态       
    CHAR bSackEn;   //* SACK选项使能
    USHORT usMSS;   //* MSS值
    UINT unSeqNum;
    UINT unAckNum; 
    USHORT usWndSize; 
    struct {
        in_addr_t unIP;
        USHORT usPort;
    } stPeerAddr;
    CHAR bIdx; 
    CHAR bNext; 
} ST_TCPLINK, *PST_TCPLINK;

TCP_LINK_EXT BOOL tcp_link_init(EN_ONPSERR *penErr); 
TCP_LINK_EXT void tcp_link_uninit(void); 
TCP_LINK_EXT PST_TCPLINK tcp_link_get(EN_ONPSERR *penErr);
TCP_LINK_EXT void tcp_link_free(PST_TCPLINK pstTcpLink);

#endif
