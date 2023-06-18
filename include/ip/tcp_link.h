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

#if SUPPORT_SACK
#define TCPSENDTIMER_NUM    4       //* 每一路tcp链路允许挂载的定时器路数，也就是连续发送多少个报文后需要等待对端ack，这个值正好是tcp sack选项携带的最大重传块数
#define RTO                 400     //* tcp超时重传时间（Retransmission Timeout），单位：毫秒
#define RTO_MAX             6000    //* rto最大值，单位：毫秒
#endif

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
#if SUPPORT_IPV6
		union {
			UINT unVal; 
			UCHAR ubaVal[16];
		} uniIp;
#else
        UINT unIp; 
#endif
    } stAddr;

    INT nInput;

    PST_SLINKEDLIST_NODE pstNode;
} ST_TCPBACKLOG, *PST_TCPBACKLOG;

typedef struct _ST_ONESHOTTIMER_ ST_ONESHOTTIMER, *PST_ONESHOTTIMER;
typedef struct _ST_TCPUDP_HANDLE_ ST_TCPUDP_HANDLE, *PST_TCPUDP_HANDLE;
typedef struct _STCB_TCPSENDTIMER_ STCB_TCPSENDTIMER, *PSTCB_TCPSENDTIMER; 

PACKED_BEGIN
#if SUPPORT_SACK
typedef struct _ST_TCPSACK_ {
    UINT unLeft;
    UINT unRight;
} PACKED ST_TCPSACK, *PST_TCPSACK;
#endif
typedef struct _ST_TCPLINK_ {
    struct {
        UINT unSeqNum;
#if SUPPORT_SACK
        UINT unAckedSeqNum; 
        UINT unHasSndBytes;
#endif        
        USHORT usWndSize;

#if SUPPORT_SACK
        CHAR bIsZeroWnd : 1;
        CHAR bDataSendState : 3; 
        //CHAR bIsSeqNumUpdated : 1; 
#else
        CHAR bIsZeroWnd;
        CHAR bDataSendState;
#endif
        PST_TCPUDP_HANDLE pstHandle;
    } PACKED stLocal;

    struct {
        PST_ONESHOTTIMER pstTimer;
        INT nInput;
        CHAR bRcvTimeout;
        CHAR bIsAcked;
        USHORT usSendDataBytes;
    } PACKED stcbWaitAck;

    struct {
        CHAR bSackEn;       //* SACK选项使能
        CHAR bWndScale;     //* 窗口放大因子
        USHORT usMSS;       //* MSS值
        USHORT usWndSize;   //* 当前窗口大小        
#if SUPPORT_IPV6
		STP_SOCKADDR stSockAddr;
		//UINT unIpv6FlowLbl; //* ipv6流标签（Flow Label），其与源地址/端口、目的地址/端口一起唯一的标识一个通讯数据流
#else
        struct {
            USHORT usPort;  //* 端口
            in_addr_t unIp; //* 地址            
        } PACKED stSockAddr;
#endif
        UINT unSeqNum;      //* 当前序号
        //UINT unNextSeqNum;  //* 期望得到的下一组序号
        UINT unStartMSecs;  //* 延时计数
        CHAR bIsNotAcked;   //* 是否已经应答
    } PACKED stPeer;

    union {
        struct {            
            USHORT no_delay_ack : 1; //* tcp ack是否延迟一小段时间后再发送（延迟的目的是等待是否有数据一同发送到对端）
            USHORT resrved1 : 15;
        } PACKED stb16;
        USHORT usVal;
    } PACKED uniFlags;  //* tcp标志

#if SUPPORT_SACK
    struct {
        CHAR bNext; //* 链接下一个要发送数据的tcp link
        CHAR bSendPacketNum; 
        CHAR bIsPutted : 1; 
        CHAR bIsWndSizeUpdated : 1; 
        CHAR bIsZeroWnd : 1; 
        CHAR bDupAckNum; 
        UINT unWriteBytes;  
        UINT unPrevSeqNum;        
        //UINT unRetransSeqNum; 
        ST_TCPSACK staSack[TCPSENDTIMER_NUM];
        UCHAR *pubSndBuf; 
        STCB_TCPSENDTIMER *pstcbSndTimer;  
        UINT unLastSndZeroWndPktMSecs; 
        UINT unWndSize; 
    } PACKED stcbSend; //* 发送控制块
#endif

    //* 用于TCP_TYPE_RCLIENT类型的tcp链路
    PST_TCPBACKLOG pstBacklog; 
    INT nInputSrv; 

    CHAR bState;        //* 当前链路状态
    CHAR bIsPassiveFin; //* 是被动FIN操作

    CHAR bIdx;
    CHAR bNext;
} PACKED ST_TCPLINK, *PST_TCPLINK;
PACKED_END

#if SUPPORT_SACK
PACKED_BEGIN
typedef struct _STCB_TCPSENDTIMER_ {
    UINT unSendMSecs;
    UINT unLeft;
    UINT unRight;
    struct _STCB_TCPSENDTIMER_ *pstcbNextForLink;
    struct _STCB_TCPSENDTIMER_ *pstcbNext;
    PST_TCPLINK pstLink; 
    USHORT usRto;
    CHAR bIsNotSacked; 
} PACKED STCB_TCPSENDTIMER, *PSTCB_TCPSENDTIMER;
PACKED_END
#endif

//* 用于tcp服务器的input附加数据
typedef struct _ST_INPUTATTACH_TCPSRV_ {
    CHAR bIsUsed;  
    CHAR bRcvMode; 
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

#if SUPPORT_SACK
TCP_LINK_EXT void tcp_send_sem_post(void); 

//* 等待信号量到达，参数nWaitSecs指定要等待的超时时间（单位为秒）：
//* 0，一直等下去直至用户层调用了socket层的send()函数，此时返回值为0，出错了则返回返回值为-1；
//* 其它，等待指定时间，如果指定时间内信号量到达，则返回值为0，超时则返回值为1，出错则返回值为-1
TCP_LINK_EXT INT tcp_send_sem_pend(INT nWaitSecs);

//* 获取一个发送定时器
TCP_LINK_EXT PSTCB_TCPSENDTIMER tcp_send_timer_node_get(void);
TCP_LINK_EXT void tcp_send_timer_node_free(PSTCB_TCPSENDTIMER pstcbSendTimer); 
TCP_LINK_EXT void tcp_send_timer_node_free_unsafe(PSTCB_TCPSENDTIMER pstcbSendTimer); 
TCP_LINK_EXT void tcp_send_timer_node_put(PSTCB_TCPSENDTIMER pstcbSendTimer);
TCP_LINK_EXT void tcp_send_timer_node_put_unsafe(PSTCB_TCPSENDTIMER pstcbSendTimer);
TCP_LINK_EXT void tcp_send_timer_node_del(PSTCB_TCPSENDTIMER pstcbSendTimer);
TCP_LINK_EXT void tcp_send_timer_node_del_unsafe(PSTCB_TCPSENDTIMER pstcbSendTimer);
TCP_LINK_EXT void tcp_send_timer_lock(void); 
TCP_LINK_EXT void tcp_send_timer_unlock(void); 
TCP_LINK_EXT PSTCB_TCPSENDTIMER tcp_send_timer_get_next(PSTCB_TCPSENDTIMER pstcbSendTimer);
TCP_LINK_EXT void tcp_link_for_send_data_put(PST_TCPLINK pstTcpLink); 
TCP_LINK_EXT void tcp_link_for_send_data_del(PST_TCPLINK pstTcpLink); 
TCP_LINK_EXT PST_TCPLINK tcp_link_for_send_data_get_next(PST_TCPLINK pstTcpLink);
#endif

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
