// TcpServerForStackTesting.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <Tlhelp32.h>
#include <Wtsapi32.h>
#include <Userenv.h>
#include <winternl.h>
#include <thread>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "tcp_helper.h" 

using namespace std;

#define SRV_PORT            6410    //* 服务器端口
#define LISTEN_NUM          10      //* 最大监听数
#define RCV_BUF_SIZE        2048    //* 接收缓冲区容量
#define PKT_DATA_LEN_MAX    1200    //* 报文携带的数据最大长度，凡是超过这个长度的报文都将被丢弃

static SOCKET l_hSocketSrv;
static SOCKET l_hSocketMax; 
static THMUTEX l_thLockClients; 
static CHAR l_bLinkIdx = 0;

//* TCP客户端
typedef struct _ST_TCPCLIENT_ {
    SOCKET hClient;    
    time_t tPrevActiveTime;    
    time_t tTimestampToAck; 
    struct {
        UINT unReadIdx;
        UINT unWriteIdx; 
        UINT unPktStartIdx;         
        CHAR bParsingState;
        UCHAR ubaRcvBuf[RCV_BUF_SIZE];
    } stcbRcv;
    CHAR bClientIdx; 
    CHAR bLinkIdx;    
    BOOL blTHIsRunning; 
    thread objTHSender;
} ST_TCPCLIENT, *PST_TCPCLIENT;
unordered_map<SOCKET, ST_TCPCLIENT> l_umstClients;

static BOOL l_blIsRunning = TRUE;
BOOL WINAPI ConsoleCtrlHandler(DWORD dwEvent)
{
    l_blIsRunning = FALSE;    
    return TRUE;
}
static BOOL init(void)
{
    thread_lock_init(&l_thLockClients);

    if (!load_socket_lib())
    {
        thread_lock_uninit(&l_thLockClients);
        return FALSE;
    }

    if (INVALID_SOCKET != (l_hSocketSrv = start_tcp_server(SRV_PORT, LISTEN_NUM)))
    {        
        l_hSocketMax = l_hSocketSrv; 
        return TRUE;
    }

    thread_lock_uninit(&l_thLockClients);
    unload_socket_lib(); 
    return FALSE; 
}

static void uninit(void)
{
    unordered_map<SOCKET, ST_TCPCLIENT>::iterator iter = l_umstClients.begin(); 
    for (; iter != l_umstClients.end(); iter++)
    {
        closesocket(iter->second.hClient); 
        if (iter->second.objTHSender.joinable())
            iter->second.objTHSender.join();        
    }

    if (INVALID_SOCKET != l_hSocketSrv)
        stop_tcp_server(l_hSocketSrv); 
    unload_socket_lib(); 
}

//* 处理客户端异常
void ClearClient(PST_TCPCLIENT pstClient, fd_set *pfdsRead, fd_set *pfdsException, unordered_map<SOCKET, ST_TCPCLIENT>::iterator *pIter)
{                
    SOCKET hClient = pstClient->hClient;
    FD_CLR(pstClient->hClient, pfdsRead);
    FD_CLR(pstClient->hClient, pfdsException);            

    (*pIter)->second.blTHIsRunning = FALSE;
    if ((*pIter)->second.objTHSender.joinable())
        (*pIter)->second.objTHSender.join();        
    *pIter = l_umstClients.erase(*pIter);

    //* 确定当前SOCKET是不是最大值,如果是则需要重新比较当前所有已连接客户端以获取最新的最大值
    if (hClient == l_hSocketMax)
    {
        unordered_map<SOCKET, ST_TCPCLIENT>::iterator iter = l_umstClients.begin();
        l_hSocketMax = l_hSocketSrv;            
        for (; iter != l_umstClients.end(); iter++)
            l_hSocketMax = l_hSocketMax < iter->second.hClient ? iter->second.hClient : l_hSocketMax; 
    }

    closesocket(hClient);    
}

static BOOL SendCtlCmd(PST_TCPCLIENT pstClient, UCHAR *pubPacket, USHORT usDataLen, UINT unSeqNum)
{
    PST_COMMUPKT_HDR pstHdr = (PST_COMMUPKT_HDR)pubPacket;
    pstHdr->bFlag = (CHAR)PKT_FLAG; 
    pstHdr->bCmd = 0x01; 
    pstHdr->bLinkIdx = pstClient->bLinkIdx;
	pstHdr->unSeqNum = unSeqNum; 
    pstHdr->unTimestamp = (UINT)time(NULL); 
    pstHdr->usDataLen = usDataLen; 
    pstHdr->usChechsum = 0; 
    pstHdr->usChechsum = crc16(pubPacket + sizeof(ST_COMMUPKT_HDR::bFlag), sizeof(ST_COMMUPKT_HDR) - sizeof(ST_COMMUPKT_HDR::bFlag) + usDataLen, 0xFFFF);
    pubPacket[sizeof(ST_COMMUPKT_HDR) + usDataLen] = PKT_FLAG;

    //* 日志输出
    CHAR szPktTime[24] = { 0 };
    unix_time_to_local((time_t)pstHdr->unTimestamp, szPktTime, sizeof(szPktTime));     
    //printf("%d#%s#>sent control command to peer, cmd = 0x01, ClientID = %d, the data length is %d bytes\r\n",  pstClient->bLinkIdx, szPktTime, pstClient->bClientIdx, pstHdr->usDataLen);

    //* 更新需要等待的报文标识
    pstClient->tTimestampToAck = (time_t)pstHdr->unTimestamp;     

    //* 发送并等待接收对端的应答
    INT nSndTotalBytes;
    INT nPacketLen = sizeof(ST_COMMUPKT_HDR) + usDataLen + sizeof(ST_COMMUPKT_HDR::bFlag);
    time_t tStartSecs; 
    INT nTryNum = 0; 
__lblSend: 
    nTryNum++; 
    if (nTryNum > 3)
    {
        printf("Did not receive the reply packet from the peer\r\n"); 
        return FALSE; 
    }
    printf("%d#%s#>sent control command to peer, cmd = 0x01, ClientID = %d, the data length is %d bytes\r\n", pstClient->bLinkIdx, szPktTime, pstClient->bClientIdx, pstHdr->usDataLen);

    nSndTotalBytes = 0;
    while (nSndTotalBytes < nPacketLen)
    {
        INT nSndBytes = send(pstClient->hClient, (const char *)pubPacket + nSndTotalBytes, nPacketLen - nSndTotalBytes, 0);
        if (nSndBytes > 0)
            nSndTotalBytes += nSndBytes;
        else
        {
            if (nSndBytes < 0)
            {
                printf("send() failed, the error code is %d\r\n", WSAGetLastError()); 
                return FALSE; 
            }
        }
    }

    //* 开始接收
    tStartSecs = time(NULL); 

__lblRecv:
    if (time(NULL) - tStartSecs > 6)
        goto __lblSend; 

    if (pstClient->tTimestampToAck)
    {
        Sleep(10);
        goto __lblRecv;
    }
    else            
        return TRUE;    
}

static void THSender(SOCKET hClient, fd_set *pfdsRead, fd_set *pfdsException)
{
    unordered_map<SOCKET, ST_TCPCLIENT>::iterator iter = l_umstClients.find(hClient); 
    if (iter == l_umstClients.end())
    {
        printf("Client not found, thread THSender failed to start\r\n");
        return; 
    }

    //* 填充要下发的数据
    CHAR szPacket[sizeof(ST_COMMUPKT_HDR) + 1200];
    CHAR szData[94];
    for (INT i = 33; i < 127; i++)
        szData[i - 33] = (CHAR)i;

    INT nHasCpyBytes = sizeof(ST_COMMUPKT_HDR);
    for (INT i = 0; i < 12; i++)
    {
        memcpy(&szPacket[nHasCpyBytes], szData, sizeof(szData));
        nHasCpyBytes += sizeof(szData);
    } 

    //* 以系统时间戳为种子
    srand(time(NULL));    

    //* 开始随机下发数据，模拟服务器下发控制指令的操作
    PST_TCPCLIENT pstClient = &iter->second; 
	UINT unSeqNum = 0;
    time_t tInterval;
    time_t tLastSendSecs = 0;    
    while (l_blIsRunning && pstClient->blTHIsRunning)
    {
    #if 1
        tInterval = 120 - (time_t)rand() % 31; 
        if (time(NULL) - tLastSendSecs > tInterval)
        {
            if (SendCtlCmd(pstClient, (UCHAR *)szPacket, nHasCpyBytes - sizeof(ST_COMMUPKT_HDR), unSeqNum++))
                tLastSendSecs = time(NULL); 
            else
                break; 
        }        
    #endif
        Sleep(1000);
    }

    printf("A client exception is caught, the client will be removed\r\n");
    pstClient->tPrevActiveTime = 0; 
}

static void HandleRead(PST_TCPCLIENT pstClient)
{    
    UINT unRemainBytes = RCV_BUF_SIZE - pstClient->stcbRcv.unWriteIdx;
    INT nRcvBytes = recv(pstClient->hClient, (char *)pstClient->stcbRcv.ubaRcvBuf + pstClient->stcbRcv.unWriteIdx, unRemainBytes, 0);
    if (nRcvBytes > 0)
    {        
        PST_COMMUPKT_HDR pstHdr;
        pstClient->stcbRcv.unWriteIdx += (UINT)nRcvBytes; 
        for (; pstClient->stcbRcv.unReadIdx < pstClient->stcbRcv.unWriteIdx; )
        {
            UCHAR ch = pstClient->stcbRcv.ubaRcvBuf[pstClient->stcbRcv.unReadIdx]; 
            switch (pstClient->stcbRcv.bParsingState)
            {
            case 0: //* 找到头部标志
                if (ch == PKT_FLAG)
                {
                    pstClient->stcbRcv.unPktStartIdx = pstClient->stcbRcv.unReadIdx;
                    pstClient->stcbRcv.bParsingState = 1;
                }
                else
                {
                    pstClient->stcbRcv.unReadIdx++;
                    break;
                }

            case 1: //* 获取整个头部数据
                if (pstClient->stcbRcv.unWriteIdx - pstClient->stcbRcv.unPktStartIdx < sizeof(ST_COMMUPKT_HDR))
                    return; 
                else
                {
                    pstHdr = (PST_COMMUPKT_HDR)(pstClient->stcbRcv.ubaRcvBuf + pstClient->stcbRcv.unPktStartIdx);
                    if (pstHdr->usDataLen < PKT_DATA_LEN_MAX) //* 只有小于系统允许的最大数据长度才是合法报文
                        pstClient->stcbRcv.bParsingState = 2;
                    else
                    {                 
                        //* 不合法，重新查找
                        pstClient->stcbRcv.unReadIdx = pstClient->stcbRcv.unPktStartIdx + 1; 
                        pstClient->stcbRcv.bParsingState = 0;
                        break; 
                    }
                }

            case 2: //* 获取完整报文
                pstHdr = (PST_COMMUPKT_HDR)(pstClient->stcbRcv.ubaRcvBuf + pstClient->stcbRcv.unPktStartIdx);
                if (pstClient->stcbRcv.unWriteIdx - pstClient->stcbRcv.unPktStartIdx < sizeof(ST_COMMUPKT_HDR) + (UINT)pstHdr->usDataLen + 1)
                    return; 
                else
                {
                    pstClient->stcbRcv.unReadIdx = pstClient->stcbRcv.unPktStartIdx + sizeof(ST_COMMUPKT_HDR) + (UINT)pstHdr->usDataLen;

                    //* 尾部标识必须匹配
                    ch = pstClient->stcbRcv.ubaRcvBuf[pstClient->stcbRcv.unReadIdx];
                    if (ch == PKT_FLAG)
                    {
                        //* 判断校验和是否正确
                        USHORT usPktChecksum = pstHdr->usChechsum;
                        pstHdr->usChechsum = 0;
                        USHORT usChecksum = crc16(&pstClient->stcbRcv.ubaRcvBuf[pstClient->stcbRcv.unPktStartIdx + sizeof(ST_COMMUPKT_HDR::bFlag)], sizeof(ST_COMMUPKT_HDR) - sizeof(ST_COMMUPKT_HDR::bFlag) + (UINT)pstHdr->usDataLen, 0xFFFF);
                        if (usChecksum == usPktChecksum)
                        {
                            pstClient->tPrevActiveTime = time(NULL);  //* 记录最后一组报文到达时间，告知主线程这个客户端上报的最后一组报文是在什么时间

                            CHAR szPktTime[24] = { 0 }; 
                            unix_time_to_local((time_t)pstHdr->unTimestamp - 8 * 3600, szPktTime, sizeof(szPktTime));

                            //* 处理收到的报文，首先看看这是不是上传的数据报文
                            if (pstHdr->bCmd == 0)
                            {
                                UCHAR ubaSndBuf[sizeof(ST_COMMUPKT_ACK)];
                                PST_COMMUPKT_ACK pstAck = (PST_COMMUPKT_ACK)ubaSndBuf;
                                pstAck->stHdr.bFlag = (CHAR)PKT_FLAG;
                                pstAck->stHdr.bCmd = 0x00; 
                                pstAck->stHdr.bLinkIdx = pstClient->bLinkIdx;
								pstAck->stHdr.unSeqNum = pstHdr->unSeqNum; 
                                pstAck->stHdr.unTimestamp = (UINT)time(NULL); 
                                pstAck->stHdr.usDataLen = sizeof(UINT) + sizeof(CHAR); 
                                pstAck->stHdr.usChechsum = 0; 
                                pstAck->unTimestamp = pstHdr->unTimestamp; 
                                pstAck->bLinkIdx = pstHdr->bLinkIdx; 
                                pstAck->bTail = (CHAR)PKT_FLAG; 
                                pstAck->stHdr.usChechsum = crc16(&ubaSndBuf[sizeof(ST_COMMUPKT_HDR::bFlag)], sizeof(ST_COMMUPKT_ACK) - 2 * sizeof(ST_COMMUPKT_HDR::bFlag), 0xFFFF);
                                pstClient->bClientIdx = pstHdr->bLinkIdx; 
                                printf("%d#%s#>recved the uploaded packet, cmd = 0x%02X, ClientID = %d, SeqNum = %d, the data length is %d bytes\r\n", pstClient->bLinkIdx, szPktTime, pstHdr->bCmd, pstHdr->bLinkIdx, pstHdr->unSeqNum, pstHdr->usDataLen);
                                send(pstClient->hClient, (const char *)ubaSndBuf, sizeof(ST_COMMUPKT_ACK), 0);
                            }
                            else if (pstHdr->bCmd == 1) //* 这是控制指令的应答报文
                            {
                                PST_COMMUPKT_ACK pstAck = (PST_COMMUPKT_ACK)pstHdr;
                                if (pstAck->unTimestamp == (UINT)pstClient->tTimestampToAck && pstAck->bLinkIdx == pstClient->bLinkIdx)
                                {
                                    pstClient->tTimestampToAck = 0;
                                    printf("%d#%s#>recved acknowledge packet, AckedLinkIdx = %d, ClientID = %d, AckedTimestamp <", pstClient->bLinkIdx, szPktTime, pstAck->bLinkIdx, pstHdr->bLinkIdx);
                                    unix_time_to_local((time_t)pstAck->unTimestamp, szPktTime, sizeof(szPktTime));
                                    printf("%s>\r\n", szPktTime);
                                }
                            }
                            else; 

                            //* 搬运剩余的字节
                            UINT unRemainBytes = pstClient->stcbRcv.unWriteIdx - pstClient->stcbRcv.unReadIdx - 1;
                            if (pstClient->stcbRcv.unReadIdx < pstClient->stcbRcv.unWriteIdx) 
                                memmove(pstClient->stcbRcv.ubaRcvBuf, pstClient->stcbRcv.ubaRcvBuf + pstClient->stcbRcv.unReadIdx + 1, unRemainBytes);
                            pstClient->stcbRcv.unWriteIdx = unRemainBytes;

                            //* 开始截取下一个报文
                            pstClient->stcbRcv.unReadIdx = 0;
                        }
                        else
                        {
                            //* 不合法，从第一个标识后的字符开始重新查找
                            pstClient->stcbRcv.unReadIdx = pstClient->stcbRcv.unPktStartIdx + 1;
                        }
                    }
                    else
                    {
                        //* 不合法，从第一个标识后的字符开始重新查找
                        pstClient->stcbRcv.unReadIdx = pstClient->stcbRcv.unPktStartIdx + 1;
                    }                    
                }

                pstClient->stcbRcv.bParsingState = 0;
                break; 
            }
        }

        pstClient->tPrevActiveTime = time(NULL);  //* 收到数据了
    }
}

static BOOL HandleAccept(fd_set *pfdsRead, fd_set *pfdsException)
{    
    SOCKET hClient;

    if ((hClient = accept_client(l_hSocketSrv)) == INVALID_SOCKET)
        return FALSE; 

    //* 如果当前接受的客户端连接句柄已经是最大连接描述符,说明当前进程出现了错误,需要关闭当前所有客户端以释放掉当前占用的句柄    
    if(hClient >= ((sizeof(fd_set) * 8) - 1))
    {
    	printf("The process has up to fd_set max, the process will be close all client to correct the problem.\r\n");
    	return FALSE;
    }

    //* 更逊最大文件集
    l_hSocketMax = ((hClient > l_hSocketMax) ? hClient : l_hSocketMax); 

    //* 加入到测试文件集
    FD_SET(hClient, pfdsRead);
    FD_SET(hClient, pfdsException);

    //* 添加到客户端列表中
    l_umstClients.emplace(hClient, ST_TCPCLIENT{ hClient, time(NULL), 0, { 0, 0, 0, 0 }, -1 });
    auto atoPair = l_umstClients.emplace(hClient, ST_TCPCLIENT{ hClient, time(NULL), 0,{ 0, 0, 0, 0, NULL }, -1 });
    atoPair.first->second.bLinkIdx = l_bLinkIdx++; 
    atoPair.first->second.blTHIsRunning = TRUE; 
    atoPair.first->second.objTHSender = thread(THSender, hClient, pfdsRead, pfdsException); 

    return TRUE; 
}

//* 清除不活跃的客户端以释放宝贵的客户端缓冲区
void ScanInactiveClients(fd_set *pfdsRead, fd_set *pfdsException)
{
    thread_lock_enter(&l_thLockClients);
    {
        unordered_map<SOCKET, ST_TCPCLIENT>::iterator iter = l_umstClients.begin();
        for (; iter != l_umstClients.end();)
        {            
            if (abs(time(NULL) - iter->second.tPrevActiveTime) > 30)            
                ClearClient(&iter->second, pfdsRead, pfdsException, &iter);             
            else
                iter++; 
        }
    }
    thread_lock_leave(&l_thLockClients);        
}

int main()
{    
    //* 捕获控制台的CTRL+C输入
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleCtrlHandler, TRUE);

    if (!init())
        return -1;     

    //* 使用select模型，将相关句柄加入描述符集
    fd_set fdsRead, fdsTstRead, fdsException, fdsTstException;    
    FD_ZERO(&fdsRead);
    FD_SET(l_hSocketSrv, &fdsRead);
    FD_ZERO(&fdsException);
    FD_SET(l_hSocketSrv, &fdsException);    

    //* 开始循环，等待新的客户端进入
    struct timeval stTimeout;
    INT nRtnVal;
    while (l_blIsRunning)
    {        
        thread_lock_enter(&l_thLockClients);
        {                        
            //* 由于select描述符集会导致所有已加入的描述符集被清零,因此实际调用select之前应先建立一套描述符集副本,select使用此副本即可避免该问题
            fdsTstRead = fdsRead;
            fdsTstException = fdsException;

            stTimeout.tv_sec = 1;
            stTimeout.tv_usec = 0;
            nRtnVal = select(l_hSocketMax + 1, &fdsTstRead, NULL, &fdsTstException, &stTimeout);
            if (nRtnVal > 0)
            {                                
                //* 首先处理监听端口的相关连接请求和异常事件			
                if (FD_ISSET(l_hSocketSrv, &fdsTstRead))
                {                    
                    if (!HandleAccept(&fdsRead, &fdsException))                    
                        l_blIsRunning = FALSE;
                }
                //* 捕捉到异常
                else if (FD_ISSET(l_hSocketSrv, &fdsTstException))
                {                    
                    printf("The process catch a select exception at listen port, the process will be exit!\r\n"); 
                    l_blIsRunning = FALSE;
                }
                else
                {
                    unordered_map<SOCKET, ST_TCPCLIENT>::iterator iter = l_umstClients.begin();
                    for (; iter != l_umstClients.end(); )
                    {
                        //* 如果是收到报文
                        if (FD_ISSET(iter->second.hClient, &fdsTstRead))		//* 读处理
                        {
                            HandleRead(&iter->second);
                        }

                        if (FD_ISSET(iter->second.hClient, &fdsTstException))	//* 异常处理
                        {                            
                            ClearClient(&iter->second, &fdsRead, &fdsException, &iter);
                            continue; 
                        }

                        iter++; 
                    }
                }
            }
            else
            {                
                if (nRtnVal < 0)
                {
                    printf("The process catch a select error (%d), the process will be exit!\r\n", WSAGetLastError());
                    l_blIsRunning = FALSE;
                }
            }
        }
        thread_lock_leave(&l_thLockClients);        

        ScanInactiveClients(&fdsRead, &fdsException);
    }    

    uninit(); 

    return 0;
}

