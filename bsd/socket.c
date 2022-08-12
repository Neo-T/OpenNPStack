#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "one_shot_timer.h" 
#include "onps_utils.h"
#define SYMBOL_GLOBALS
#include "bsd/socket.h"
#undef SYMBOL_GLOBALS
#include "ip/tcp.h"
#include "ip/udp.h"

SOCKET socket(INT family, INT type, INT protocol, EN_ONPSERR *penErr)
{
    if (AF_INET != family)
    {
        if (penErr)
            *penErr = ERRADDRFAMILIES;
        return INVALID_SOCKET; 
    }

    EN_ONPSERR enErr; 
    INT nInput; 
    switch (type)
    {
    case SOCK_STREAM: 
        nInput = onps_input_new(IPPROTO_TCP, &enErr);
        break; 

    case SOCK_DGRAM: 
        nInput = onps_input_new(IPPROTO_UDP, &enErr);
        break; 

    default: 
        nInput = INVALID_SOCKET; 
        enErr = ERRSOCKETTYPE; 
        break; 
    }

    if (nInput < 0)
    {
        if (penErr)
            *penErr = enErr; 
    }

    return (SOCKET)nInput; 
}

void close(SOCKET socket)
{
    EN_ONPSERR enErr;
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        return; 

    EN_TCPLINKSTATE enLinkState;
    if (!onps_input_get((INT)socket, IOPT_GETTCPLINKSTATE, &enLinkState, &enErr))
        return;     

    if (enProto == IPPROTO_TCP)
    {
        if (TLSCONNECTED == enLinkState)
        {
            tcp_disconnect((INT)socket);
            return; 
        }

        if (TLSFINWAIT1 == enLinkState 
            || TLSFINWAIT2 == enLinkState 
            || TLSCLOSING == enLinkState 
            || TLSTIMEWAIT == enLinkState
            || TLSCLOSED == enLinkState) 
            return; 
    }    
    onps_input_free((INT)socket); 
}

static int socket_tcp_connect(SOCKET socket, HSEM hSem, const char *srv_ip, unsigned short srv_port, int nConnTimeout)
{    
    if (tcp_send_syn((INT)socket, inet_addr(srv_ip), srv_port, nConnTimeout) > 0)
    {    
__lblWait: 
        //* 等待信号到达：超时或者收到syn ack同时本地回馈的syn ack的ack发送成功
        if (os_thread_sem_pend(hSem, 0) < 0)
        {
            onps_set_last_error((INT)socket, ERRINVALIDSEM);
            return -1;
        }

        EN_ONPSERR enErr;
        EN_TCPLINKSTATE enLinkState;
        if (!onps_input_get((INT)socket, IOPT_GETTCPLINKSTATE, &enLinkState, &enErr))
        {
            onps_set_last_error((INT)socket, enErr);
            return -1;
        }

        if (TLSSYNSENT == enLinkState)
            goto __lblWait; 

        switch (enLinkState)
        {
        case TLSCONNECTED:
            return 0;

        case TLSACKTIMEOUT:
            onps_set_last_error((INT)socket, ERRTCPCONNTIMEOUT);
            return -1;

        case TLSRESET:
            onps_set_last_error((INT)socket, ERRTCPCONNRESET);
            return -1;

        case TLSSYNACKACKSENTFAILED:
            return -1;

        default:            
            onps_set_last_error((INT)socket, ERRUNKNOWN);
            return -1;
        }
    }
    else
        return -1;   
}

static int socket_tcp_connect_nb(SOCKET socket, const char *srv_ip, unsigned short srv_port, EN_TCPLINKSTATE enLinkState)
{
    switch (enLinkState)
    {
    case TLSINIT:
        if (tcp_send_syn((INT)socket, inet_addr(srv_ip), srv_port, 0) > 0)
            return 1;
        else
            return -1;

    case TLSSYNSENT:
    case TLSRCVEDSYNACK:
        return 1;

    case TLSCONNECTED:
        return 0;

    case TLSACKTIMEOUT:
        onps_set_last_error((INT)socket, ERRTCPCONNTIMEOUT);
        return -1;

    case TLSRESET:
        onps_set_last_error((INT)socket, ERRTCPCONNRESET);
        return -1;

    case TLSSYNACKACKSENTFAILED:
        return -1;

    default:
        onps_set_last_error((INT)socket, ERRUNKNOWN);
        return -1;
    }
}

static int socket_connect(SOCKET socket, const char *srv_ip, unsigned short srv_port, int nConnTimeout)
{
    EN_ONPSERR enErr;
    EN_IPPROTO enProto;    
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        goto __lblErr;    

    onps_set_last_error((INT)socket, ERRNO);

    if (enProto == IPPROTO_TCP)
    {        
        //* 获取当前链路状态
        EN_TCPLINKSTATE enLinkState;
        if (!onps_input_get((INT)socket, IOPT_GETTCPLINKSTATE, &enLinkState, &enErr))
            goto __lblErr;

        //* 无效，意味着当前TCP连接链路尚未申请一个tcp link节点，需要在这里申请
        if (TLSINVALID == enLinkState)
        {
            PST_TCPLINK pstLink = tcp_link_get(&enErr); 
            if(pstLink)
            {                 
                if (!onps_input_set((INT)socket, IOPT_SETATTACH, pstLink, &enErr))
                {
                    tcp_link_free(pstLink);
                    goto __lblErr;
                }
                enLinkState = TLSINIT; 
            }
            else
                goto __lblErr;
        }

        if (nConnTimeout > 0)
        {
            HSEM hSem = INVALID_HSEM;
            if (!onps_input_get((INT)socket, IOPT_GETSEM, &hSem, &enErr))
                goto __lblErr;
            if (INVALID_HSEM == hSem)
            {
                enErr = ERRINVALIDSEM;
                goto __lblErr;
            }

            return socket_tcp_connect(socket, hSem, srv_ip, srv_port, nConnTimeout);
        }
        else
            return socket_tcp_connect_nb(socket, srv_ip, srv_port, enLinkState);
    }
    else if (enProto == IPPROTO_UDP)
    {
        //* 说明要绑定一个具体的目标服务器，所以这里需要记录下目标服务器的地址
        PST_UDPLINK pstLink = udp_link_get(&enErr); 
        if (pstLink)
        {
            pstLink->stPeerAddr.unIp = (UINT)inet_addr(srv_ip); 
            pstLink->stPeerAddr.usPort = srv_port; 

            //* 附加到input
            if (!onps_input_set((INT)socket, IOPT_SETATTACH, pstLink, &enErr))
            {
                udp_link_free(pstLink);
                goto __lblErr;
            }
        }
        else
            goto __lblErr;
        return 0; 
    }
    else    
        enErr = ERRUNSUPPIPPROTO; 

__lblErr:
    onps_set_last_error((INT)socket, enErr);
    return -1;
}

INT connect(SOCKET socket, const CHAR *srv_ip, USHORT srv_port, INT nConnTimeout)
{
    if (nConnTimeout <= 0)
        nConnTimeout = TCP_CONN_TIMEOUT; 
    return socket_connect(socket, srv_ip, srv_port, nConnTimeout);
}

INT connect_nb(SOCKET socket, const CHAR *srv_ip, USHORT srv_port)
{
    return socket_connect(socket, srv_ip, srv_port, 0);
}

static INT socket_tcp_send(SOCKET socket, HSEM hSem, UCHAR *pubData, INT nDataLen, INT nWaitAckTimeout)
{    
    //* 发送数据	
    INT nRtnVal = tcp_send_data((INT)socket, pubData, nDataLen, nWaitAckTimeout);
    if (nRtnVal < 0)    
        return -1;    
    
__lblWaitAck:     
    //* 等待信号量到达：定时器报超时或者ack到达    
    if (os_thread_sem_pend(hSem, 0) < 0)
    {
        onps_set_last_error((INT)socket, ERRINVALIDSEM); 
        return -1; 
    }	
    
    //* 信号量到达，根据实际处理结果返回不同值
    EN_ONPSERR enErr;
    EN_TCPDATASNDSTATE enSndState;
    if (!onps_input_get((INT)socket, IOPT_GETTCPDATASNDSTATE, &enSndState, &enErr))
    {
        onps_set_last_error((INT)socket, enErr);
        return -1;
    }
    
    switch (enSndState)
    {  
    case TDSSENDING: 
        goto __lblWaitAck; 

    case TDSACKRCVED:
        return nRtnVal; 

    case TDSTIMEOUT:		
        onps_set_last_error((INT)socket, ERRTCPACKTIMEOUT);
        return -1; 

    case TDSLINKRESET: 
        onps_set_last_error((INT)socket, ERRTCPCONNRESET);
        return -1;

    case TDSLINKCLOSED:
        onps_set_last_error((INT)socket, ERRTCPCONNCLOSED);
        return -1;

    default:              
        onps_set_last_error((INT)socket, ERRUNKNOWN);
        return -1;
    }
}

static INT socket_tcp_send_nb(SOCKET socket, UCHAR *pubData, INT nDataLen, EN_TCPDATASNDSTATE enSndState)
{    
    if (TDSSENDING != enSndState)
    {
        INT nRtnVal = tcp_send_data((INT)socket, pubData, nDataLen, 0); 
        if (nRtnVal < 0)
            return -1;
        else
            return 0; 
    }
    else
    {
        EN_ONPSERR enErr; 
        USHORT usLastSndBytes; 
        switch (enSndState)
        {
        case TDSACKRCVED:
            if (onps_input_get((INT)socket, IOPT_GETLASTSNDBYTES, &usLastSndBytes, &enErr))            
                return (INT)usLastSndBytes;             
            else
            {
                onps_set_last_error((INT)socket, enErr);
                return -1;
            }            

        case TDSTIMEOUT:
            onps_set_last_error((INT)socket, ERRTCPACKTIMEOUT);
            return -1;

        case TDSLINKRESET:
            onps_set_last_error((INT)socket, ERRTCPCONNRESET);
            return -1;

        case TDSLINKCLOSED:
            onps_set_last_error((INT)socket, ERRTCPCONNCLOSED);
            return -1;

        default:
            return 0; 
        }
    }
}

static int socket_send(SOCKET socket, UCHAR *pubData, INT nDataLen, int nWaitAckTimeout)
{
    //* 空数据没必要发送，这里并不返回-1以显式地告诉用户，仅记录这个错误即可，用户可以主动获取这个错误
    if (NULL == pubData || !nDataLen)
    {
        onps_set_last_error((INT)socket, ERRDATAEMPTY);
        return 0;
    }    

    //* 确定这是系统支持的协议才可
    EN_ONPSERR enErr;
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        goto __lblErr;

    //* 错误清0
    onps_set_last_error((INT)socket, ERRNO); 

    //* 完成实际的发送
    if (enProto == IPPROTO_TCP)
    {
        //* 获取当前链路状态
        EN_TCPLINKSTATE enLinkState;
        if (!onps_input_get((INT)socket, IOPT_GETTCPLINKSTATE, &enLinkState, &enErr))
            goto __lblErr;         
        if (TLSCONNECTED == enLinkState)
        {          
            //* 获取当前数据发送状态，如果上一条数据尚未发送结束则暂不发送当前这条数据，因为在资源受限系统中内存有限，无法在协议栈底层实现数据重传，所以必须在用户层来控制
            EN_TCPDATASNDSTATE enSndState;
            if (!onps_input_get((INT)socket, IOPT_GETTCPDATASNDSTATE, &enSndState, &enErr))
            {
                onps_set_last_error((INT)socket, enErr);
                return -1;
            }            

            if (nWaitAckTimeout > 0)
            {
                //* 如果当前尚未就绪，则直接返回0，告知调用者需要继续重复调用该函数以在状态就绪后可以立即发送数据
                if (TDSSENDING == enSndState)
                    return 0;

                HSEM hSem = INVALID_HSEM;
                if (!onps_input_get((INT)socket, IOPT_GETSEM, &hSem, &enErr))
                    goto __lblErr;
                if (INVALID_HSEM == hSem)
                {
                    enErr = ERRINVALIDSEM;
                    goto __lblErr;
                }

                return socket_tcp_send(socket, hSem, pubData, nDataLen, nWaitAckTimeout);
            }
            else
                return socket_tcp_send_nb(socket, pubData, nDataLen, enSndState);
        }
        else
        {
            enErr = ERRTCPNOTCONNECTED;
            goto __lblErr; 
        }
    }
    else if (enProto == IPPROTO_UDP)
    {
        return udp_send((INT)socket, pubData, nDataLen); 
    }
    else
        enErr = ERRUNSUPPIPPROTO;

__lblErr:
    onps_set_last_error((INT)socket, enErr);
    return -1;
}

INT send(SOCKET socket, UCHAR *pubData, INT nDataLen, INT nWaitAckTimeout)
{
    if (nWaitAckTimeout <= 0)
        nWaitAckTimeout = TCP_ACK_TIMEOUT;
    return socket_send(socket, pubData, nDataLen, nWaitAckTimeout); 
}

INT send_nb(SOCKET socket, UCHAR *pubData, INT nDataLen)
{
    return socket_send(socket, pubData, nDataLen, 0);
}

INT sendto(SOCKET socket, const CHAR *srv_ip, USHORT srv_port, UCHAR *pubData, INT nDataLen)
{
    //* 空数据没必要发送，这里并不返回-1以显式地告诉用户，仅记录这个错误即可，用户可以主动获取这个错误
    if (NULL == pubData || !nDataLen)
    {
        onps_set_last_error((INT)socket, ERRDATAEMPTY);
        return 0;
    }

    //* 确定这是系统支持的协议才可
    EN_ONPSERR enErr;
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        goto __lblErr;

    //* 错误清0
    onps_set_last_error((INT)socket, ERRNO);

    //* 只有udp协议才支持指定目标地址的发送操作
    if (IPPROTO_UDP == enProto)
    {
        return udp_sendto((INT)socket, inet_addr(srv_ip), srv_port, pubData, nDataLen);  
    }
    else
        enErr = ERRIPROTOMATCH;

__lblErr:
    onps_set_last_error((INT)socket, enErr);
    return -1;
}

BOOL socket_set_rcv_timeout(SOCKET socket, CHAR bRcvTimeout, EN_ONPSERR *penErr)
{    
    return onps_input_set((INT)socket, IOPT_SETRCVTIMEOUT, &bRcvTimeout, penErr); 
}

INT recv(SOCKET socket, UCHAR *pubDataBuf, INT nDataBufSize)
{
    EN_ONPSERR enErr;

    //* 获取当前socket绑定的协议类型
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        goto __lblErr;

    //* 仅支持tcp和udp，其它不支持
    if (enProto != IPPROTO_UDP && enProto != IPPROTO_TCP)
    {
        enErr = ERRUNSUPPIPPROTO;
        goto __lblErr;
    }

    //* 获取接收等待时长
    CHAR bRcvTimeout;
    if (!onps_input_get((INT)socket, IOPT_GETRCVTIMEOUT, &bRcvTimeout, &enErr))
    {
        onps_set_last_error((INT)socket, enErr);
        return -1;
    }

    //* 错误清0
    onps_set_last_error((INT)socket, ERRNO);

    //* 读取收到的数据
    if (enProto == IPPROTO_TCP)
        return tcp_recv_upper((INT)socket, pubDataBuf, nDataBufSize, bRcvTimeout);
    else
    {
        return udp_recv_upper((INT)socket, pubDataBuf, nDataBufSize, NULL, NULL, bRcvTimeout);
    }

__lblErr:
    onps_set_last_error((INT)socket, enErr);
    return -1;            
}

INT recvfrom(SOCKET socket, UCHAR *pubDataBuf, INT nDataBufSize, in_addr_t *punFromIP, USHORT *pusFromPort)
{
    EN_ONPSERR enErr;

    //* 获取当前socket绑定的协议类型
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        goto __lblErr;

    //* 仅支持tcp和udp，其它不支持
    if (enProto != IPPROTO_UDP)
    {
        enErr = ERRUNSUPPIPPROTO;
        goto __lblErr;
    }

    //* 获取接收等待时长
    CHAR bRcvTimeout;
    if (!onps_input_get((INT)socket, IOPT_GETRCVTIMEOUT, &bRcvTimeout, &enErr))
    {
        onps_set_last_error((INT)socket, enErr);
        return -1;
    }

    //* 错误清0
    onps_set_last_error((INT)socket, ERRNO);

    return udp_recv_upper((INT)socket, pubDataBuf, nDataBufSize, punFromIP, pusFromPort, bRcvTimeout);

__lblErr:
    onps_set_last_error((INT)socket, enErr);
    return -1;
}

INT is_tcp_connected(SOCKET socket, EN_ONPSERR *penErr)
{
    //* 获取当前链路状态
    EN_TCPLINKSTATE enLinkState;
    if (!onps_input_get((INT)socket, IOPT_GETTCPLINKSTATE, &enLinkState, penErr))
        return -1; 

    if (TLSCONNECTED == enLinkState)
        return 1; 

    return 0; 
}

INT bind(SOCKET socket, const CHAR *pszNetifIp, USHORT usPort)
{
    EN_ONPSERR enErr;
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        goto __lblErr;    

    //* 首先看看指定的端口是否已被使用
    if (onps_input_port_used(enProto, usPort))
    {
        enErr = ERRPORTOCCUPIED; 
        goto __lblErr; 
    }

    //* 设置地址
    ST_TCPUDP_HANDLE stHandle;     
    if (pszNetifIp) 
        stHandle.unNetifIp = (UINT)inet_addr(pszNetifIp);
    else
        stHandle.unNetifIp = 0; 
    stHandle.usPort = usPort;  

    //* 绑定地址和端口且是tcp协议，就需要显式地指定这个input是一个tcp服务器类型
    if(IPPROTO_TCP == enProto)
        stHandle.bType = TCP_TYPE_SERVER;
    
    //* 更新句柄
    if (onps_input_set((INT)socket, IOPT_SETTCPUDPADDR, &stHandle, &enErr))
        return TRUE; 

__lblErr:
    onps_set_last_error((INT)socket, enErr);
    return -1;      
}

INT listen(SOCKET socket, INT backlog)
{
    EN_ONPSERR enErr;
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, &enErr))
        goto __lblErr; 

    //* 只有tcp服务器才能调用这个函数，其它都不可以
    if (enProto == IPPROTO_TCP)
    {
        PST_TCPUDP_HANDLE pstHandle;
        if (!onps_input_get((INT)socket, IOPT_GETTCPUDPADDR, &pstHandle, &enErr))
            goto __lblErr; 

        //* 已经绑定地址和端口才可，否则没法成为服务器，如果进入该分支，则意味着用户没调用bind()函数
        if (TCP_TYPE_SERVER != pstHandle->bType)
        {
            enErr = ERRNOTBINDADDR; 
            goto __lblErr; 
        }

        //* 获取附加段数据，看看监听是否已启动
        PST_INPUTATTACH_TCPSRV pstAttach;
        if (!onps_input_get((INT)socket, IOPT_GETATTACH, &pstAttach, &enErr))                    
            goto __lblErr; 

        //* 为空则意味着当前服务尚未进入监听阶段
        if (!pstAttach)
        {
            pstAttach = tcpsrv_input_attach_get(&enErr);
            if (pstAttach)
            {
                if (!onps_input_set((INT)socket, IOPT_SETATTACH, pstAttach, &enErr))
                {
                    tcpsrv_input_attach_free(pstAttach); 
                    goto __lblErr;
                }
            }
            else
                goto __lblErr; 
        }
    }
    else
    {
        enErr = ERRTCPONLY;
        goto __lblErr;
    }

    return 0; 

__lblErr: 
    onps_set_last_error((INT)socket, enErr);
    return -1;
}

SOCKET accept(SOCKET socket, in_addr_t *punCltIP, USHORT *pusCltPort, INT nWaitSecs, EN_ONPSERR *penErr)
{
    if (penErr)
        *penErr = ERRNO; 

    INT nInputClient;     
    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, penErr))
        goto __lblErr; 

    //* 只有tcp服务器才能调用这个函数，其它都不可以
    if (enProto == IPPROTO_TCP)
    {
        PST_TCPUDP_HANDLE pstHandle;
        if (!onps_input_get((INT)socket, IOPT_GETTCPUDPADDR, &pstHandle, penErr))
            goto __lblErr;

        //* 已经绑定地址和端口才可，否则没法成为服务器，如果进入该分支，则意味着用户没调用bind()函数
        if (TCP_TYPE_SERVER != pstHandle->bType)
        {
            if (penErr)
                *penErr = ERRNOTBINDADDR;
            goto __lblErr;
        }

        //* 获取附加段数据，看看监听是否已启动
        PST_INPUTATTACH_TCPSRV pstAttach;
        if (!onps_input_get((INT)socket, IOPT_GETATTACH, &pstAttach, penErr))
            goto __lblErr;

        //* 为空则意味着当前服务尚未进入监听阶段，不能调用这个函数
        if (!pstAttach)
        {
            if (penErr)
                *penErr = ERRTCPNOLISTEN;
            goto __lblErr; 
        }

        //* 是否需要等待指定时间直至信号到达或超时
        if (nWaitSecs)
        {
            if (nWaitSecs < 0)
                nWaitSecs = 0; 

            INT nRtnVal = os_thread_sem_pend(pstAttach->hSemAccept, nWaitSecs); 
            if (nRtnVal < 0) //* 等待期间发生错误
            {
                if (penErr)
                    *penErr = ERRINVALIDSEM;
                goto __lblErr;
            }

            if (nRtnVal) //* 超时，未收到信号                            
                goto __lblErr;             
        }
        
        //* 取出一个连接请求
        PST_TCPBACKLOG pstBacklog = tcp_backlog_get(&pstAttach->pstSListBacklog); 
        if (pstBacklog)
        {
            //* 只要是正常完成三次握手的连接请求，协议栈底层就会投递一个semaphore，所以如果用户选择了不等待（即参数nWaitSecs为0），这里就必须pend一次以消除这个到达的semaphore
            if (0 == nWaitSecs)
                os_thread_sem_pend(pstAttach->hSemAccept, 1);

            //* 取出input节点句柄，然后释放当前占用的backlog节点资源
            nInputClient = pstBacklog->nInput;
            if (punCltIP)
                *punCltIP = pstBacklog->stAdrr.unIp; 
            if (pusCltPort)
                *pusCltPort = pstBacklog->stAdrr.usPort; 
            tcp_backlog_free(pstBacklog);

            return (SOCKET)nInputClient;
        }
    }
    else
    {
        if (penErr)
            *penErr = ERRTCPONLY;
    }

__lblErr:    
    return INVALID_SOCKET; 
}

SOCKET tcpsrv_recv_poll(SOCKET socket, INT nWaitSecs, EN_ONPSERR *penErr)
{    
    if (penErr)
        *penErr = ERRNO;

    EN_IPPROTO enProto;
    if (!onps_input_get((INT)socket, IOPT_GETIPPROTO, &enProto, penErr))
        goto __lblErr;

    //* 只有tcp服务器才能调用这个函数，其它都不可以
    if (enProto != IPPROTO_TCP)
    {
        if (penErr)
            *penErr = ERRTCPONLY;
        goto __lblErr;
    }
    
    PST_TCPUDP_HANDLE pstHandle;
    if (!onps_input_get((INT)socket, IOPT_GETTCPUDPADDR, &pstHandle, penErr))
        goto __lblErr;

    //* 已经绑定地址和端口才可，否则没法成为服务器，如果进入该分支，则意味着用户没调用bind()函数
    if (TCP_TYPE_SERVER != pstHandle->bType)
    {
        penErr = ERRNOTBINDADDR;
        goto __lblErr;
    }

    //* 获取附加段数据，看看监听是否已启动
    PST_INPUTATTACH_TCPSRV pstAttach;
    if (!onps_input_get((INT)socket, IOPT_GETATTACH, &pstAttach, penErr))
        goto __lblErr;

    //* 为空则意味着当前服务尚未进入监听阶段，不能调用这个函数
    if (!pstAttach)
    {
        penErr = ERRTCPNOLISTEN;
        goto __lblErr;
    }

    //* 需要等待一小段时间或直至数据到达
    if (nWaitSecs)
    {
        if (nWaitSecs < 0)
            nWaitSecs = 0; 
        INT nRtnVal = onps_input_sem_pend_uncond((INT)socket, nWaitSecs, penErr); 
        if (nRtnVal < 0)
            goto __lblErr; 

        if (nRtnVal) //* 超时，未收到信号
            goto __lblErr; 
    }

    //* 获取接收队列
    PST_TCPSRV_RCVQUEUE_NODE pstNode = tcpsrv_recv_queue_get(&pstAttach->pstSListRcvQueue); 
    if (pstNode)
    {
        INT nClientInput = pstNode->uniData.nVal; 
        tcpsrv_recv_queue_free(pstNode); 
        return (SOCKET)nClientInput; 
    }

__lblErr: 
    return INVALID_SOCKET; 
}
