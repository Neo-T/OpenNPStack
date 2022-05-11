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
    TLSINVALID,     //* TCP链路无效（尚未申请一个有效的tcp link节点）
    TLSINIT,        //* TCP链路初始状态
    TLSACKTIMEOUT,  //* 等待接收ACK报文超时

    //* 以下为Socket被用于TCP Client时的状态定义
    TLSSYNSENT,             //* 发送SYN请求
    TLSRCVEDSYNACK,         //* 收到SYN ACK    
    TLSSYNACKACKSENTFAILED, //* 给服务器发送SYN ACK的ACK报文失败
    TLSCONNECTED,           //* 已连接
    TLSRESET,               //* 连接被重置    
    TLSCLOSED,              //* 已关闭    

    //* 以下为Socket被用于TCP Server时的状态定义
    TLSSRVSTARTED,  //* TCP Server已启动
    TLSSRVDOWN,     //* TCP Server已关闭
} EN_TCPLINKSTATE;

typedef enum {
    TDSSENDRDY = 0, 
    TDSSENDING, 
    TDSACKRCVED, 
    TDSTIMEOUT, 
    TDSLINKRESET, 
    TDSLINKCLOSED
} EN_TCPDATASNDSTATE;

typedef struct _ST_ONESHOTTIMER_ ST_ONESHOTTIMER, *PST_ONESHOTTIMER; 
typedef struct _ST_TCPUDP_HANDLE_ ST_TCPUDP_HANDLE, *PST_TCPUDP_HANDLE; 
typedef struct _ST_TCPLINK_ {
    struct {
        UINT unSeqNum;  
        UINT unAckNum; 
        USHORT usWndSize;
        CHAR bDataSendState;         
        PST_TCPUDP_HANDLE pstAddr; 
    } stLocal;

    struct {        
        PST_ONESHOTTIMER pstTimer;
        HSEM hSem;
        CHAR bIsAcked; 
        USHORT usSendDataBytes; 
    } stcbWaitAck;        

    struct {
        CHAR bSackEn;       //* SACK选项使能
        CHAR bWndScale;     //* 窗口放大因子
        USHORT usMSS;       //* MSS值
        USHORT usWndSize;   //* 当前窗口大小        
        struct {
            USHORT usPort;  //* 端口
            in_addr_t unIp; //* 地址            
        } stAddr;
        UINT unSeqNum;      //* 当前序号
    } stPeer;

    CHAR bState;    //* 当前链路状态    
    
    CHAR bIdx; 
    CHAR bNext; 
} ST_TCPLINK, *PST_TCPLINK;

TCP_LINK_EXT BOOL tcp_link_init(EN_ONPSERR *penErr); 
TCP_LINK_EXT void tcp_link_uninit(void); 
TCP_LINK_EXT PST_TCPLINK tcp_link_get(EN_ONPSERR *penErr);
TCP_LINK_EXT void tcp_link_free(PST_TCPLINK pstTcpLink);

#endif
