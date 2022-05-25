#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#define SYMBOL_GLOBALS
#include "bsd/socket.h"
#undef SYMBOL_GLOBALS
#include "ip/tcp.h"

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
    }
    onps_input_free((INT)socket); 
}

static int socket_tcp_connect(SOCKET socket, HSEM hSem, const char *srv_ip, unsigned short srv_port, int nConnTimeout)
{    
    if (tcp_send_syn((INT)socket, inet_addr(srv_ip), srv_port, nConnTimeout) > 0)
    {
        //* 等待信号到达：超时或者收到syn ack同时本地回馈的syn ack的ack发送成功
        os_thread_sem_pend(hSem, 0); 

        EN_ONPSERR enErr;
        EN_TCPLINKSTATE enLinkState;
        if (!onps_input_get((INT)socket, IOPT_GETTCPLINKSTATE, &enLinkState, &enErr))
        {
            onps_set_last_error((INT)socket, enErr);
            return -1;
        }

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
    os_thread_sem_pend(hSem, 0);     
    
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

int socket_send(SOCKET socket, UCHAR *pubData, INT nDataLen, int nWaitAckTimeout)
{
    //* 空数据没必要发送，这里并不返回-1以显式地告诉用户，仅记录这个错误即可，用户可以主动获取这个错误
    if (NULL == pubData || !nDataLen)
    {
        onps_set_last_error((INT)socket, ERRTCPDATAEMPTY);
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
        return nDataLen;
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
        return 0; 
    }

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
