/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 保存tcp链路相关信息的辅助功能函数
 *
 * Neo-T, 创建于2022.04.28 14:28
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

    //* 以下为TCP Client的状态定义
    TLSSYNSENT,             //* SYN请求已发送
    TLSRCVEDSYNACK,         //* 收到SYN ACK    
    TLSSYNACKACKSENTFAILED, //* 给服务器发送SYN ACK的ACK报文失败
    TLSCONNECTED,           //* 已连接

    //* 以下为连接本地tcp服务器的远端tcp客户端相关链路状态定义
    TLSRCVEDSYN,            //* 收到客户端发送的syn连接请求报文
    TLSSYNACKSENT,          //* 服务器已发送syn ack报文给客户端
    //TLSSYNACKACKTIMEOUT,    //* 等待客户端回馈的syn ack报文的ack报文超时
    
    TLSRESET,               //* 连接被重置    

    //* 链路关闭状态定义
    TLSFINWAIT1,            //* FIN请求已发送，等待对端回馈应答    
    TLSFINWAIT2,            //* 收到对端回馈的FIN ACK报文    
    TLSCLOSING,             //* 收到对端发送的FIN时，链路关闭状态正处于TLSFINWAIT1态，尚未进入TLSFINWAIT2    
    TLSTIMEWAIT,            //* 收到对端的FIN报文
    TLSCLOSED,              //* 彻底关闭

    //* 以下为Socket被用于TCP Server时的状态定义
    //TLSSRVSTARTED,  //* TCP Server已启动
    //TLSSRVDOWN,     //* TCP Server已关闭
} EN_TCPLINKSTATE;

typedef enum {
    TDSSENDRDY = 0, 
    TDSSENDING, 
    TDSACKRCVED, 
    TDSTIMEOUT, 
    TDSLINKRESET, 
    TDSLINKCLOSED
} EN_TCPDATASNDSTATE; 

//* 记录到达的tcp服务器连接请求信息的结构体
typedef struct _ST_TCPBACKLOG__ {
    struct {
        USHORT usPort;
        UINT unIp;
    } stAdrr;

    INT nInput;

    PST_SLINKEDLIST_NODE pstNode;
} ST_TCPBACKLOG, *PST_TCPBACKLOG;

typedef struct _ST_ONESHOTTIMER_ ST_ONESHOTTIMER, *PST_ONESHOTTIMER;
typedef struct _ST_TCPUDP_HANDLE_ ST_TCPUDP_HANDLE, *PST_TCPUDP_HANDLE;
typedef struct _ST_TCPLINK_ {
    struct {
        UINT unSeqNum;
        UINT unAckNum;
        USHORT usWndSize;
        CHAR bIsZeroWnd;
        CHAR bDataSendState;
        PST_TCPUDP_HANDLE pstAddr;
    } stLocal;

    struct {
        PST_ONESHOTTIMER pstTimer;
        INT nInput;
        CHAR bRcvTimeout;
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
        UINT unStartMSecs;  //* 延时计数
        CHAR bIsNotAcked;   //* 是否已经应答
    } stPeer;

    union {
        struct {            
            USHORT no_delay_ack : 1;            
            USHORT resrved1 : 15;
        } stb16;
        USHORT usVal;
    } uniFlags; 

    //* 用于TCP_TYPE_RCLIENT类型的tcp链路
    PST_TCPBACKLOG pstBacklog; 
    INT nInputSrv; 

    CHAR bState;        //* 当前链路状态
    CHAR bIsPassiveFin; //* 是被动FIN操作

    CHAR bIdx;
    CHAR bNext;
} ST_TCPLINK, *PST_TCPLINK;

//* 用于tcp服务器的input附加数据
typedef struct _ST_INPUTATTACH_TCPSRV_ {
    CHAR bIsUsed;    
    USHORT usBacklogNum; 
    USHORT usBacklogCnt;      
    HSEM hSemAccept; 
    PST_SLINKEDLIST pstSListBacklog; 
    PST_SLINKEDLIST pstSListRcvQueue; 
} ST_INPUTATTACH_TCPSRV, *PST_INPUTATTACH_TCPSRV;

//* tcp服务器数据接收队列节点定义
typedef ST_SLINKEDLIST_NODE ST_TCPSRV_RCVQUEUE_NODE, *PST_TCPSRV_RCVQUEUE_NODE; 

TCP_LINK_EXT BOOL tcp_link_init(EN_ONPSERR *penErr); 
TCP_LINK_EXT void tcp_link_uninit(void); 
TCP_LINK_EXT PST_TCPLINK tcp_link_get(EN_ONPSERR *penErr);
TCP_LINK_EXT void tcp_link_free(PST_TCPLINK pstTcpLink); 
TCP_LINK_EXT void tcp_link_list_used_put(PST_TCPLINK pstTcpLink);
TCP_LINK_EXT PST_TCPLINK tcp_link_list_used_get_next(PST_TCPLINK pstTcpLink);
TCP_LINK_EXT void tcp_link_lock(void); 
TCP_LINK_EXT void tcp_link_unlock(void);

#if SUPPORT_ETHERNET
TCP_LINK_EXT PST_INPUTATTACH_TCPSRV tcpsrv_input_attach_get(EN_ONPSERR *penErr);
TCP_LINK_EXT void tcpsrv_input_attach_free(PST_INPUTATTACH_TCPSRV pstAttach); 
TCP_LINK_EXT PST_TCPBACKLOG tcp_backlog_freed_get(EN_ONPSERR *penErr);
TCP_LINK_EXT PST_TCPBACKLOG tcp_backlog_get(PST_SLINKEDLIST *ppstSListBacklog, USHORT *pusBacklogCnt);
TCP_LINK_EXT void tcp_backlog_put(PST_SLINKEDLIST *ppstSListBacklog, PST_TCPBACKLOG pstBacklog, USHORT *pusBacklogCnt);
TCP_LINK_EXT void tcp_backlog_free(PST_TCPBACKLOG pstBacklog);

TCP_LINK_EXT PST_TCPSRV_RCVQUEUE_NODE tcpsrv_recv_queue_freed_get(EN_ONPSERR *penErr);
TCP_LINK_EXT PST_TCPSRV_RCVQUEUE_NODE tcpsrv_recv_queue_get(PST_SLINKEDLIST *ppstSListRcvQueue);
TCP_LINK_EXT void tcpsrv_recv_queue_put(PST_SLINKEDLIST *ppstSListRcvQueue, PST_TCPSRV_RCVQUEUE_NODE pstNode, INT nInput);
TCP_LINK_EXT void tcpsrv_recv_queue_free(PST_TCPSRV_RCVQUEUE_NODE pstNode); 
TCP_LINK_EXT INT tcpsrv_recv_queue_count(PST_SLINKEDLIST *ppstSListRcvQueue); 
#endif

#endif
