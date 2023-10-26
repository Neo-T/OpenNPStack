/*
* 遵循Apache License 2.0开源许可协议
*
*/
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"
#include "netif/route.h"

#if NVTCMD_TELNET_EN
#include "net_tools/net_virtual_terminal.h"
#include "net_tools/telnet.h"
#include "telnet/nvt_cmd.h"

#define SYMBOL_GLOBALS
#include "net_tools/telnet_client.h"
#undef SYMBOL_GLOBALS

static void telnet_srv_data_handler(ULONGLONG ullNvtHandle, SOCKET hTelSrvSocket, UCHAR *pubSrvData, INT nSrvDataLen)
{
    UCHAR *pubNextData = pubSrvData;
    INT nHandleBytes = 0;
    INT i;

    while (nHandleBytes < nSrvDataLen)
    {
        //* 指令，非数据
        if (TELNETCMD_IAC == pubNextData[0])
        {
            PST_TELNETPKT_CMD pstCmd = (PST_TELNETPKT_CMD)pubNextData;
            if (pstCmd->ubCmd > TELNETCMD_SB)
            {
                if (TELNETOPT_TERMTYPE == pstCmd->ubNegoOption)
                {
                    if (TELNETCMD_DO == pstCmd->ubCmd)
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_WILL, TELNETOPT_TERMTYPE);
                    else if (TELNETCMD_DONT == pstCmd->ubCmd)
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_WONT, TELNETOPT_TERMTYPE); //* 告知对端，已知晓禁止激活终端类型选项
                    else;
                }
                else if (TELNETOPT_SGA == pstCmd->ubNegoOption)
                {
                    if (TELNETCMD_WILL == pstCmd->ubCmd)
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_DO, TELNETOPT_SGA);
                    else if (TELNETCMD_WONT == pstCmd->ubCmd)
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_DONT, TELNETOPT_SGA);
                    else;
                }
                else if (TELNETOPT_ECHO == pstCmd->ubNegoOption)
                {
                    if (TELNETCMD_WILL == pstCmd->ubCmd)
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_DO, TELNETOPT_ECHO);
                    else if (TELNETCMD_WONT == pstCmd->ubCmd)
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_DONT, TELNETOPT_ECHO);
                    else if (TELNETCMD_DO == pstCmd->ubCmd)
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_WILL, TELNETOPT_ECHO);
                    else
                        telnet_cmd_send(hTelSrvSocket, TELNETCMD_WONT, TELNETOPT_ECHO);

                }
                else
                    telnet_cmd_ack_default(hTelSrvSocket, pstCmd->ubCmd, pstCmd->ubNegoOption);

                pubNextData += sizeof(ST_TELNETPKT_CMD);
                nHandleBytes += sizeof(ST_TELNETPKT_CMD);
            }
            else
            {
                i = sizeof(ST_TELNETPKT_CMD);
                if (TELNETOPT_TERMTYPE == pstCmd->ubNegoOption)
                {
                    PST_TELNETPKT_SOPT_TERMTYPE pstTremType = (PST_TELNETPKT_SOPT_TERMTYPE)pubNextData;
                    if (TELNETOPT_TTCODE_SEND == pstTremType->ubCode)
                    {
                        //* 发送终端类型给服务器
                        const CHAR *pszTermType = nvt_get_term_type(ullNvtHandle);
                        telnet_report_term_type(hTelSrvSocket, pszTermType, strlen(pszTermType));

                        i = offsetof(ST_TELNETPKT_SOPT_TERMTYPE, ubEIAC);
                    }
                }

                //* 计算子选项长度            
                for (; i + nHandleBytes < nSrvDataLen; i++)
                {
                    if (pubNextData[i] == TELNETCMD_SE)
                        break;
                }

                pubNextData += i + 1;
                nHandleBytes += i + 1;
            }
        }
        else
        {
            INT nRemainBytes = pubSrvData + nSrvDataLen - pubNextData;
            for (i = 0; i < nRemainBytes; i++)
            {
                if (TELNETCMD_IAC == pubNextData[i])
                    break;
            }

            //* 非指令
            nvt_output(ullNvtHandle, pubNextData, i);

            pubNextData += i;
            nHandleBytes += i;
        }
    }
}

void telnet_clt_entry(void *pvParam)
{
    //* 必须要复制，因为这个函数被设计为单独一个线程/任务
    ST_TELCLT_STARTARGS stArgs = *((PST_TELCLT_STARTARGS)pvParam);
    ((PST_TELCLT_STARTARGS)pvParam)->bIsCpyEnd = TRUE;

    //* 输出连接提示信息
    nvt_output(stArgs.ullNvtHandle, "Connecting to telnet server ...\r\n", sizeof("Connecting to telnet server ...\r\n") - 1); 

    EN_ONPSERR enErr;                          
    SOCKET hTelSocket = tcp_srv_connect(AF_INET, (in_addr_t *)&stArgs.stSrvAddr.saddr_ipv4, stArgs.stSrvAddr.usPort, 0, 10, &enErr); 
    if (INVALID_SOCKET != hTelSocket)
    {
        EN_ONPSERR enErr;
        UCHAR *pubRcvBuf = (UCHAR *)buddy_alloc(TELNETCLT_RCVBUF_SIZE, &enErr);
        if (pubRcvBuf)
        {
            //* 非阻塞
            socket_set_rcv_timeout(hTelSocket, 0, &enErr);

            //* 进入循环状态
            INT nRcvBytes;
            BOOL blIsNotRcvedData = TRUE;
            USHORT usZeroNum = 0; 
            while (nvt_cmd_exec_enable(stArgs.ullNvtHandle))
            {
                nRcvBytes = nvt_input(stArgs.ullNvtHandle, pubRcvBuf, TELNETCLT_RCVBUF_SIZE);
                if (nRcvBytes)
                {
                    if (nRcvBytes == 1 && pubRcvBuf[0] == '\x03')                                            
                        break;                     

                    blIsNotRcvedData = FALSE;
                    usZeroNum = 0;
                    if (!tcp_send(hTelSocket, pubRcvBuf, nRcvBytes))
                        break; 
                }
                else
                    blIsNotRcvedData = TRUE;
                                    
                nRcvBytes = recv(hTelSocket, pubRcvBuf, TELNETCLT_RCVBUF_SIZE); 
                if (nRcvBytes > 0)
                {                    
                    blIsNotRcvedData = FALSE;
                    usZeroNum = 0;
                    telnet_srv_data_handler(stArgs.ullNvtHandle, hTelSocket, pubRcvBuf, nRcvBytes);
                }
                else
                {
                    if (nRcvBytes < 0)
                        break;
                }

                if (blIsNotRcvedData)
                {
                    if (usZeroNum++ > 1000)
                    {                    
                        os_sleep_ms(5);
                        usZeroNum = 0;
                    }
                }
            }

            buddy_free(pubRcvBuf);

            nvt_output(stArgs.ullNvtHandle, "\r\n", 2);
        }
        else
            nvt_output(stArgs.ullNvtHandle, "telnet failed to start, the mmu has no memory available.\r\n", 
                        sizeof("telnet failed to start, the mmu has no memory available.\r\n") - 1);
        close(hTelSocket);         
    }
    else    
        nvt_outputf(stArgs.ullNvtHandle, 128, "connect failed, %s\r\n", onps_error(enErr));     
    
    nvt_cmd_exec_end(stArgs.ullNvtHandle); 
    nvt_cmd_thread_end(); 
}

#endif

