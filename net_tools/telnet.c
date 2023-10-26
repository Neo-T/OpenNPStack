/*
* ���ߣ�Neo-T����ѭApache License 2.0��Դ���Э��
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
#include "bsd/socket.h" 

#if NETTOOLS_TELNETSRV
#define SYMBOL_GLOBALS
#include "net_tools/telnet.h"
#undef SYMBOL_GLOBALS

void telnet_cmd_send(SOCKET hSocket, UCHAR ubCmd, UCHAR ubOption)
{
    ST_TELNETPKT_CMD stCmd;
    stCmd.ubIAC = TELNETCMD_IAC;
    stCmd.ubCmd = ubCmd;
    stCmd.ubNegoOption = ubOption;
    send(hSocket, (UCHAR *)&stCmd, sizeof(ST_TELNETPKT_CMD), 1);
}

//* Э��ջ��֧�ֵ�Э��ѡ��һ�ɽ�ֹ����ú����������ԭ�����Ӧ��
void telnet_cmd_ack_default(SOCKET hSocket, UCHAR ubCmd, UCHAR ubOption)
{
    if (TELNETCMD_WILL == ubCmd)
        telnet_cmd_send(hSocket, TELNETCMD_DONT, ubOption);
    else if (TELNETCMD_WONT == ubCmd)
        telnet_cmd_send(hSocket, TELNETCMD_DONT, ubOption);
    else if (TELNETCMD_DO == ubCmd)
        telnet_cmd_send(hSocket, TELNETCMD_WONT, ubOption);
    else if (TELNETCMD_DONT == ubCmd)
        telnet_cmd_send(hSocket, TELNETCMD_WONT, ubOption);
    else;
}

void telnet_req_term_type(SOCKET hSocket)
{
    //* ��������Ҫ��Զ��ϱ��ն�����
    ST_TELNETPKT_SOPT_TERMTYPE stSubOptTermType;
    stSubOptTermType.ubSIAC = TELNETCMD_IAC;
    stSubOptTermType.ubSB = TELNETCMD_SB;
    stSubOptTermType.ubOption = TELNETOPT_TERMTYPE;
    stSubOptTermType.ubCode = TELNETOPT_TTCODE_SEND;
    stSubOptTermType.ubEIAC = TELNETCMD_IAC;
    stSubOptTermType.ubSE = TELNETCMD_SE;
    send(hSocket, (UCHAR *)&stSubOptTermType, sizeof(ST_TELNETPKT_SOPT_TERMTYPE), 1);
}

void telnet_report_term_type(SOCKET hSocket, const CHAR *pszTermType, INT nTremTypeLen)
{
    UCHAR ubaSndBuf[sizeof(ST_TELNETPKT_SOPT_TERMTYPE) + TERM_NAME_MAX];
    PST_TELNETPKT_SOPT_TERMTYPE pstTermType = (PST_TELNETPKT_SOPT_TERMTYPE)ubaSndBuf;
    pstTermType->ubSIAC = TELNETCMD_IAC;
    pstTermType->ubSB = TELNETCMD_SB;
    pstTermType->ubOption = TELNETOPT_TERMTYPE;
    pstTermType->ubCode = TELNETOPT_TTCODE_IS;
    memcpy(&ubaSndBuf[offsetof(ST_TELNETPKT_SOPT_TERMTYPE, ubEIAC)], pszTermType, nTremTypeLen);
    ubaSndBuf[offsetof(ST_TELNETPKT_SOPT_TERMTYPE, ubEIAC) + nTremTypeLen] = TELNETCMD_IAC;
    ubaSndBuf[offsetof(ST_TELNETPKT_SOPT_TERMTYPE, ubEIAC) + nTremTypeLen + 1] = TELNETCMD_SE;
    send(hSocket, (UCHAR *)ubaSndBuf, sizeof(ST_TELNETPKT_SOPT_TERMTYPE) + nTremTypeLen, 1);
}
#endif
