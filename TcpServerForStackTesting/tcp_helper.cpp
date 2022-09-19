#include "stdafx.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <winsock2.h>
#define SYMBOL_GLOBALS
#include "tcp_helper.h"
#undef SYMBOL_GLOBALS

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"mswsock.lib")

typedef struct sockaddr_in ST_SOCKADDR, *PST_SOCKADDR; 

INT load_socket_lib(USHORT usVer, CHAR *pbLoadNum)
{
    INT nErrCode;
    WSADATA wsaData;

    //* 装入并初始化WinSock库
    if ((nErrCode = WSAStartup(usVer, &wsaData)) != 0)
    {
        switch (nErrCode)
        {
        case WSAVERNOTSUPPORTED:
            *pbLoadNum++;
            if (*pbLoadNum >= 3)
                printf("load WinSock lib failed, system not support the current ver!\r\n");
            else
                return load_socket_lib(wsaData.wVersion, pbLoadNum);

            break;

        case WSASYSNOTREADY:
            printf("load WinSock lib failed(WSASYSNOTREADY)!\r\n");
            break;

        case WSAEINPROGRESS:
            printf("load WinSock lib failed(WSAEINPROGRESS)!\r\n");
            break;

        case WSAEPROCLIM:
            printf("load WinSock lib failed(WSAEPROCLIM)!\r\n");
            break;

        case WSAEFAULT:
            printf("load WinSock lib failed(WSAEFAULT)!\r\n");
            break;

        default:
            break;
        }

        return 0;
    }

    if (usVer != wsaData.wVersion)
        return load_socket_lib(wsaData.wVersion, pbLoadNum); 

    return 1;
}

BOOL load_socket_lib(void)
{
    CHAR bLoadNum = 0;

    if (load_socket_lib(WSALIB_VER, &bLoadNum))
        return TRUE;

    return FALSE;
}

void unload_socket_lib(void)
{
    //* 释放WinSock库
    WSACleanup(); 
}

SOCKET start_tcp_server(USHORT usPort, UINT unListenNum)
{
    SOCKET hSocket;
    ST_SOCKADDR stSockAddr;
    INT nOn;
    INT nErrCode = 0;

    if ((hSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        return INVALID_SOCKET;

    nOn = 1;
    setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (CHAR*)&nOn, sizeof(INT));

    //* 绑定IP地址和端口号
    memset(&stSockAddr, 0, sizeof(ST_SOCKADDR));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(usPort);
    stSockAddr.sin_addr.S_un.S_addr = INADDR_ANY;/*inet_addr("192.168.0.2")*/;
    if (bind(hSocket, (struct sockaddr*)&stSockAddr, sizeof(ST_SOCKADDR)) == SOCKET_ERROR)
    {
        switch (WSAGetLastError())
        {
        case WSAEADDRINUSE:
            printf("Error:WSAEADDRINUSE\r\n");
            break;

        case WSAEFAULT:
            printf("Error:WSAEFAULT\r\n");
            break;

        case WSAEINVAL:
            printf("Error:WSAEINVAL\r\n");
            break;

        default:
            printf("Error:SOCK_ERR_BIND_OTHER\r\n");
            break;
        }

        goto __lblErr;
    }

    //* 开启监听
    if (listen(hSocket, unListenNum) == SOCKET_ERROR)
    {
        if (WSAGetLastError() == WSAENOBUFS)
        {
            printf("Error:SOCK_ERR_LISTEN_NOBUF\r\n");
        }
        else
        {
            printf("Error:SOCK_ERR_LISTEN_SOCKERR\r\n");
        }

        goto __lblErr;
    }

    //* 置为非阻塞
    //ULONG dwBlockMode;
    //dwBlockMode = 1;
    //ioctlsocket(hSocket, FIONBIO, &dwBlockMode);

    return hSocket;

__lblErr:
    //* 关闭SOCKET
    closesocket(hSocket);

    return 	INVALID_SOCKET;
}

void stop_tcp_server(SOCKET hSocket)
{
    closesocket(hSocket); 
}

SOCKET accept_client(SOCKET hSocket)
{
    SOCKET hClient;
    if ((hClient = accept(hSocket, NULL, NULL)) == INVALID_SOCKET)    
        printf("catch a error when call accept(), the error code is %d\r\n", WSAGetLastError());        

    return hClient;
}

static const USHORT l_usaCRC16[256] =
{
    0x0000,  0xC0C1,  0xC181,  0x0140,  0xC301,  0x03C0,  0x0280,  0xC241,
    0xC601,  0x06C0,  0x0780,  0xC741,  0x0500,  0xC5C1,  0xC481,  0x0440,
    0xCC01,  0x0CC0,  0x0D80,  0xCD41,  0x0F00,  0xCFC1,  0xCE81,  0x0E40,
    0x0A00,  0xCAC1,  0xCB81,  0x0B40,  0xC901,  0x09C0,  0x0880,  0xC841,
    0xD801,  0x18C0,  0x1980,  0xD941,  0x1B00,  0xDBC1,  0xDA81,  0x1A40,
    0x1E00,  0xDEC1,  0xDF81,  0x1F40,  0xDD01,  0x1DC0,  0x1C80,  0xDC41,
    0x1400,  0xD4C1,  0xD581,  0x1540,  0xD701,  0x17C0,  0x1680,  0xD641,
    0xD201,  0x12C0,  0x1380,  0xD341,  0x1100,  0xD1C1,  0xD081,  0x1040,
    0xF001,  0x30C0,  0x3180,  0xF141,  0x3300,  0xF3C1,  0xF281,  0x3240,
    0x3600,  0xF6C1,  0xF781,  0x3740,  0xF501,  0x35C0,  0x3480,  0xF441,
    0x3C00,  0xFCC1,  0xFD81,  0x3D40,  0xFF01,  0x3FC0,  0x3E80,  0xFE41,
    0xFA01,  0x3AC0,  0x3B80,  0xFB41,  0x3900,  0xF9C1,  0xF881,  0x3840,
    0x2800,  0xE8C1,  0xE981,  0x2940,  0xEB01,  0x2BC0,  0x2A80,  0xEA41,
    0xEE01,  0x2EC0,  0x2F80,  0xEF41,  0x2D00,  0xEDC1,  0xEC81,  0x2C40,
    0xE401,  0x24C0,  0x2580,  0xE541,  0x2700,  0xE7C1,  0xE681,  0x2640,
    0x2200,  0xE2C1,  0xE381,  0x2340,  0xE101,  0x21C0,  0x2080,  0xE041,
    0xA001,  0x60C0,  0x6180,  0xA141,  0x6300,  0xA3C1,  0xA281,  0x6240,
    0x6600,  0xA6C1,  0xA781,  0x6740,  0xA501,  0x65C0,  0x6480,  0xA441,
    0x6C00,  0xACC1,  0xAD81,  0x6D40,  0xAF01,  0x6FC0,  0x6E80,  0xAE41,
    0xAA01,  0x6AC0,  0x6B80,  0xAB41,  0x6900,  0xA9C1,  0xA881,  0x6840,
    0x7800,  0xB8C1,  0xB981,  0x7940,  0xBB01,  0x7BC0,  0x7A80,  0xBA41,
    0xBE01,  0x7EC0,  0x7F80,  0xBF41,  0x7D00,  0xBDC1,  0xBC81,  0x7C40,
    0xB401,  0x74C0,  0x7580,  0xB541,  0x7700,  0xB7C1,  0xB681,  0x7640,
    0x7200,  0xB2C1,  0xB381,  0x7340,  0xB101,  0x71C0,  0x7080,  0xB041,
    0x5000,  0x90C1,  0x9181,  0x5140,  0x9301,  0x53C0,  0x5280,  0x9241,
    0x9601,  0x56C0,  0x5780,  0x9741,  0x5500,  0x95C1,  0x9481,  0x5440,
    0x9C01,  0x5CC0,  0x5D80,  0x9D41,  0x5F00,  0x9FC1,  0x9E81,  0x5E40,
    0x5A00,  0x9AC1,  0x9B81,  0x5B40,  0x9901,  0x59C0,  0x5880,  0x9841,
    0x8801,  0x48C0,  0x4980,  0x8941,  0x4B00,  0x8BC1,  0x8A81,  0x4A40,
    0x4E00,  0x8EC1,  0x8F81,  0x4F40,  0x8D01,  0x4DC0,  0x4C80,  0x8C41,
    0x4400,  0x84C1,  0x8581,  0x4540,  0x8701,  0x47C0,  0x4680,  0x8641,
    0x8201,  0x42C0,  0x4380,  0x8341,  0x4100,  0x81C1,  0x8081,  0x4040,
};
USHORT crc16(const UCHAR *pubCheckData, UINT unCheckLen, USHORT usInitVal)
{
    UINT i;
    USHORT usCRC = usInitVal;
    for (i = 0; i < unCheckLen; i++)
    {
        usCRC = (usCRC >> 8) ^ l_usaCRC16[(usCRC & 0x00FF) ^ *pubCheckData];
        pubCheckData++;
    }

    return usCRC;
}

//* 初始化线程锁
void thread_lock_init(THMUTEX *pthMutex)
{
    InitializeCriticalSection(pthMutex);
}

//* 去初始化线程锁
void thread_lock_uninit(THMUTEX *pthMutex)
{
    DeleteCriticalSection(pthMutex);
}

//* 线程加锁
void thread_lock_enter(THMUTEX *pthMutex)
{
    EnterCriticalSection(pthMutex);
}

//* 线程解锁
void thread_lock_leave(THMUTEX *pthMutex)
{
    LeaveCriticalSection(pthMutex);
}

void unix_time_to_local(time_t tUnixTimestamp, CHAR *pszDatetime, UINT unDatetimeBufSize)
{
    struct tm stTime;    
    localtime_s(&stTime, (time_t*)&tUnixTimestamp);

    sprintf_s(pszDatetime, unDatetimeBufSize, "%d-%02d-%02d %02d:%02d:%02d", stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday, stTime.tm_hour, stTime.tm_min, stTime.tm_sec);
}

