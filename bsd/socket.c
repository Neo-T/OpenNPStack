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
    HSEM hSem = onps_input_get_semaphore((INT)socket);
    if (INVALID_HSEM == hSem)
        return -1; 

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
            EN_TCPCONNSTATE enState = onps_input_get_tcp_link_state((INT)socket);
            switch (enState)
            {
            case TCSCONNECTED:
                return 0; 

            case TCSRCVSYNACKTIMEOUT: 
                onps_set_last_error((INT)socket, ERRTCPCONNTIMEOUT);
                return -1; 

            case TCSRESET:
                onps_set_last_error((INT)socket, ERRTCPCONNRESET);
                return -1;

            case TCSSYNSENTFAILED:  
            case TCSSYNACKACKSENTFAILED:
                return -1; 

            default:
                onps_set_last_error((INT)socket, ERRUNKNOWN);
                return -1;
            }
        }
    }
    else
    {
        return -1; 
    }   
}

int connect(SOCKET socket, const char *srv_ip, unsigned short srv_port, int nConnTimeout)
{
    onps_set_last_error((INT)socket, ERRNO);

    switch ((EN_IPPROTO)onps_input_get_ipproto((INT)socket))
    {
    case IPPROTO_TCP: 
        return socket_tcp_connect(socket, srv_ip, srv_port, nConnTimeout);         

    default:
        onps_set_last_error((INT)socket, ERRUNSUPPIPPROTO); 
        return -1; 
    }
}

static int socket_tcp_connect_nb(SOCKET socket, const char *srv_ip, unsigned short srv_port)
{


    EN_TCPCONNSTATE enState = onps_input_get_tcp_link_state((INT)socket); 
    switch (enState)
    {
        case 

    case TCSCONNECTED:
        return 0;

    case TCSRCVSYNACKTIMEOUT:
        onps_set_last_error((INT)socket, ERRTCPCONNTIMEOUT);
        return -1;

    case TCSRESET:
        onps_set_last_error((INT)socket, ERRTCPCONNRESET);
        return -1;

    case TCSSYNSENTFAILED:
    case TCSSYNACKACKSENTFAILED:
        return -1;

    default:
        onps_set_last_error((INT)socket, ERRUNKNOWN);
        return -1;
    }

    INT nRtnVal;
    if ((nRtnVal = tcp_send_syn((INT)socket, inet_addr(srv_ip), srv_port)) > 0)
}

int connect_nb(SOCKET socket, const char *srv_ip, unsigned short srv_port)
{
    onps_set_last_error((INT)socket, ERRNO);
    switch ((EN_IPPROTO)onps_input_get_ipproto((INT)socket))
    {
    case IPPROTO_TCP:
        return socket_tcp_connect_nb(socket, srv_ip, srv_port);

    default:
        onps_set_last_error((INT)socket, ERRUNSUPPIPPROTO);
        return -1;
    }
}
