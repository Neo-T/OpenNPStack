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

SOCKET socket(int family, int type, int protocol)
{
    if (AF_INET != family)
    {
#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("Unsupported address families: %d", family); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif

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

#if SUPPORT_PRINTF
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
        printf("%s", onps_error(enErr)); 
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif
        break; 
    }

    onps_set_last_error(nInput, enErr);

    return (SOCKET)nInput; 
}

void close(SOCKET socket)
{
    onps_input_free((INT)socket); 
}

static int socket_tcp_connect(SOCKET socket, const char *srv_ip, unsigned short srv_port, int nConnTimeout)
{
    EN_ONPSERR enErr;
    HSEM hSem = INVALID_HSEM;
    if (!onps_input_get((INT)socket, IOPT_GETSEM, &hSem, &enErr))
    {
        onps_set_last_error((INT)socket, enErr);
        return -1;
    }
    if (INVALID_HSEM == hSem)
    {
        onps_set_last_error((INT)socket, ERRINVALIDSEM);
        return -1;
    }

    INT nRtnVal;
    if ((nRtnVal = tcp_send_syn((INT)socket, inet_addr(srv_ip), srv_port)) > 0)
    {
        //* 超时，没有收到任何数据
        if (os_thread_sem_pend(hSem, nConnTimeout) < 0)
        {
            onps_set_last_error((INT)socket, ERRTCPCONNTIMEOUT);
            return -1;
        }
        else
        {
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

            case TLSRCVSYNACKTIMEOUT: 
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
    }
    else
        return -1;   
}

static int socket_tcp_connect_nb(SOCKET socket, const char *srv_ip, unsigned short srv_port, EN_TCPLINKSTATE enLinkState)
{
    switch (enLinkState)
    {
    case TLSINIT:
        INT nRtnVal;
        if ((nRtnVal = tcp_send_syn((INT)socket, inet_addr(srv_ip), srv_port)) > 0)
            return 1;
        else
            return -1;
        break;

    case TLSSYNSENT:
    case TLSRCVEDSYNACK:
        return 1;

    case TLSCONNECTED:
        return 0;

    case TLSRCVSYNACKTIMEOUT:
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
                    goto __lblErr;
                enLinkState = TLSINIT; 
            }
            else
                goto __lblErr;
        }

        if (nConnTimeout > 0)
            return socket_tcp_connect(socket, srv_ip, srv_port, nConnTimeout);
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

int connect(SOCKET socket, const char *srv_ip, unsigned short srv_port, int nConnTimeout)
{
    if (nConnTimeout <= 0)
        nConnTimeout = TCP_CONN_TIMEOUT; 
    return socket_connect(socket, srv_ip, srv_port, nConnTimeout);
}

int connect_nb(SOCKET socket, const char *srv_ip, unsigned short srv_port)
{
    return socket_connect(socket, srv_ip, srv_port, 0);
}
