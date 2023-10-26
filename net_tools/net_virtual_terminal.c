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
#include "telnet/os_nvt.h"

#if SUPPORT_PPP
#include "ppp/ppp.h" 
#endif

#if NETTOOLS_TELNETSRV
#define SYMBOL_GLOBALS
#include "net_tools/net_virtual_terminal.h"
#undef SYMBOL_GLOBALS

#if NETTOOLS_SNTP && NVTCMD_NTP_EN
#include "net_tools/sntp.h" 
#endif //* #if NETTOOLS_SNTP && NVTCMD_NTP_EN

#if NETTOOLS_DNS_CLIENT && NVTCMD_NSLOOKUP_EN
#include "net_tools/dns.h" 
#endif //* #if NETTOOLS_DNS_CLIENT && NVTCMD_NSLOOKUP_EN

#include "net_tools/telnet.h"
#include "telnet/nvt_cmd.h" 

typedef struct _ST_NVTNEGOOPT_ {
    UCHAR ubOption;
    UCHAR(*pfunNegoPut)(PSTCB_NVT pstcbNvt, UCHAR **ppubFilled);
    void(*pfunNegoGet)(PSTCB_NVT pstcbNvt, UCHAR ubCmd);
} ST_NVTNEGOOPT, *PST_NVTNEGOOPT;

//* 在这里定义nvt自带的指令
//* ===================================================================================
static INT help(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle);
static INT logout(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle);

#if NVTCMD_MEMUSAGE_EN
static INT mem_usage(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle);
#endif //* #if NVTCMD_MEMUSAGE_EN

#if NVTCMD_NETIF_EN
static INT netif(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle); 
#endif //* #if NVTCMD_NETIF_EN

#if NVTCMD_IFIP_EN && SUPPORT_ETHERNET
static INT ifip(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle); 
#endif //* #if NVTCMD_IFIP_EN && SUPPORT_ETHERNET

#if NVTCMD_ROUTE_EN
static INT route(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle); 
#endif //* #if NVTCMD_ROUTE_EN

#if NETTOOLS_SNTP && NVTCMD_NTP_EN
static INT ntpdate(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle); 
#endif //* #if NETTOOLS_SNTP && NVTCMD_NTP_EN

#if NETTOOLS_DNS_CLIENT && NVTCMD_NSLOOKUP_EN
static INT nslookup(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle); 
#endif

static const ST_NVTCMD l_staNvtCmd[] = {
    { logout, "exit", "logout and return to the terminal.\r\n" },
    { logout, "logout", "same as the <exit> command, logout and return to the terminal.\r\n" },
#if NVTCMD_MEMUSAGE_EN
    { mem_usage, "memusage", "print the usage of dynamic memory in the protocol stack.\r\n" }, 
#endif //* #if NVTCMD_MEMUSAGE_EN

#if NVTCMD_NETIF_EN
    { netif, "netif", "print all network interface card information registered to the protocol stack.\r\n"}, 
#endif //* #if NVTCMD_NETIF_EN

#if NVTCMD_IFIP_EN && SUPPORT_ETHERNET
    { ifip, "ifip", "To \033[01;37madd\033[0m, \033[01;37mdel\033[0m, \033[01;37mset\033[0m IP addresses for a network interface other than a ppp network interface.\r\n" }, 
#endif //* #if NVTCMD_IFIP_EN && SUPPORT_ETHERNET

#if NVTCMD_ROUTE_EN
    { route, "route", "Print, add, delete system routing table entries.\r\n" }, 
#endif //* #if NVTCMD_ROUTE_EN

#if NETTOOLS_SNTP && NVTCMD_NTP_EN
    { ntpdate, "ntpdate", "Request NTP server to synchronize system time and achieve network time synchronization function.\r\n" }, 
#endif //* #if NETTOOLS_SNTP && NVTCMD_NTP_EN

#if NETTOOLS_DNS_CLIENT && NVTCMD_NSLOOKUP_EN
    { nslookup, "nslookup", "A lightweight domain lookup tool that we can use to obtain the IP address corresponding to a domain name.\r\n" }, 
#endif //* #if NETTOOLS_DNS_CLIENT && NVTCMD_NSLOOKUP_EN
};

static ST_NVTCMD_NODE l_staNvtCmdNode[sizeof(l_staNvtCmd) / sizeof(ST_NVTCMD)];
static PST_NVTCMD_NODE l_pstNvtCmdList = NULL; 

//* l_staNvtCmd中内嵌指令最长的那条的字节数
static CHAR l_bNvtCmdLenMax = 8; 
//* ===================================================================================

static UCHAR nego_put_term_type(PSTCB_NVT pstcbNvt, UCHAR **ppubFilled);
static UCHAR nego_put_suppress_go_ahead(PSTCB_NVT pstcbNvt, UCHAR **ppubFilled);
static UCHAR nego_put_echo(PSTCB_NVT pstcbNvt, UCHAR **ppubFilled);

static void nego_get_term_type(PSTCB_NVT pstcbNvt, UCHAR ubCmd);
static void nego_get_suppress_go_ahead(PSTCB_NVT pstcbNvt, UCHAR ubCmd);
static void nego_get_echo(PSTCB_NVT pstcbNvt, UCHAR ubCmd);

const static ST_NVTNEGOOPT l_staNvtNegoOpts[NVTNEGOOPT_NUM] = {
    { TELNETOPT_TERMTYPE, nego_put_term_type, nego_get_term_type },
    { TELNETOPT_SGA, nego_put_suppress_go_ahead, nego_get_suppress_go_ahead },
    { TELNETOPT_ECHO, nego_put_echo, nego_get_echo }
};

void nvt_embedded_cmd_loader(void)
{
    UCHAR i;
    for (i = 0; i < sizeof(l_staNvtCmd) / sizeof(ST_NVTCMD); i++)
    {
        nvt_cmd_add(&l_staNvtCmdNode[i], &l_staNvtCmd[i]);
    }
}

//* 发送协商选项
static void nvt_nego_opt_send(PSTCB_NVT pstcbNvt)
{
    UCHAR ubaSndBuf[32];
    UCHAR *pubFilled = ubaSndBuf;
    UCHAR i, ubFilledBytes = 0;

    //* 填充协商选项
    for (i = 0; i < NVTNEGOOPT_NUM; i++)
        ubFilledBytes += l_staNvtNegoOpts[i].pfunNegoPut(pstcbNvt, &pubFilled);

    //* 存在要协商的选项则下发协商选项给客户端
    if (ubFilledBytes)
    {
        SOCKET hRmtTelnetClt = *(SOCKET *)((UCHAR *)pstcbNvt - offsetof(STCB_TELNETCLT, stcbNvt));
        tcp_send(hRmtTelnetClt, ubaSndBuf, ubFilledBytes);
    }
}

#if NVTCMDCACHE_EN
static void nvt_cmd_cache_add(PSTCB_NVT pstcbNvt)
{
    INT nCmdLen = strlen(pstcbNvt->szInputCache) + 1;
    INT i, nMovedIdx = -1;
    for (i = 0; i < NVTCMDCACHE_SIZE; )
    {
        if (pstcbNvt->pszCmdCache[i])
        {
            //* 是否是已经存在的指令
            if (nMovedIdx < 0 && nCmdLen == pstcbNvt->pszCmdCache[i] && !memcmp(pstcbNvt->szInputCache, pstcbNvt->pszCmdCache + i + 1, nCmdLen - 1))
                nMovedIdx = i;

            i += pstcbNvt->pszCmdCache[i];
        }
        else //* 找到了尾部，则结束循环
            break;
    }

    //* 如果是新指令则添加，否则指令后移至尾部
    INT nCacheCmdLen = i;
    if (nMovedIdx < 0)
    {
        pstcbNvt->bCmdNum++;

        INT nRemainSpaceSize = NVTCMDCACHE_SIZE - nCacheCmdLen;
        if (nRemainSpaceSize < nCmdLen) //* 如果不够存储，删除最老的指令
        {
            CHAR nRemovedCmdNum = 0;
            for (i = 0; i < NVTCMDCACHE_SIZE; )
            {
                nRemainSpaceSize += pstcbNvt->pszCmdCache[i];
                nRemovedCmdNum++;
                if (nRemainSpaceSize < nCmdLen)
                    i += pstcbNvt->pszCmdCache[i];
                else
                {
                    nCacheCmdLen = nCacheCmdLen - (i + pstcbNvt->pszCmdCache[i]);
                    memmove(pstcbNvt->pszCmdCache, pstcbNvt->pszCmdCache + i + pstcbNvt->pszCmdCache[i], nCacheCmdLen);
                    pstcbNvt->pszCmdCache[nCacheCmdLen] = nCmdLen;
                    memcpy(pstcbNvt->pszCmdCache + nCacheCmdLen + 1, pstcbNvt->szInputCache, nCmdLen - 1);
                    nCacheCmdLen += nCmdLen;
                    if (nCacheCmdLen < NVTCMDCACHE_SIZE)
                        pstcbNvt->pszCmdCache[nCacheCmdLen] = 0; //* 结束标记
                    pstcbNvt->bCmdNum -= nRemovedCmdNum;
                    break;
                }
            }
        }
        else
        {
            pstcbNvt->pszCmdCache[nCacheCmdLen] = nCmdLen;
            memcpy(pstcbNvt->pszCmdCache + nCacheCmdLen + 1, pstcbNvt->szInputCache, nCmdLen - 1);
            nCacheCmdLen += nCmdLen;
            if (nCacheCmdLen < NVTCMDCACHE_SIZE)
                pstcbNvt->pszCmdCache[nCacheCmdLen] = 0; //* 结束标记                        
        }
    }
    else
    {
        //* 把后面的指令前移，把当前指令后移
        memmove(pstcbNvt->pszCmdCache + nMovedIdx, pstcbNvt->pszCmdCache + nMovedIdx + pstcbNvt->pszCmdCache[nMovedIdx], nCacheCmdLen - (nMovedIdx + pstcbNvt->pszCmdCache[nMovedIdx]));
        nCacheCmdLen -= nCmdLen;
        pstcbNvt->pszCmdCache[nCacheCmdLen] = nCmdLen;
        memcpy(pstcbNvt->pszCmdCache + nCacheCmdLen + 1, pstcbNvt->szInputCache, nCmdLen - 1);
    }

    pstcbNvt->bCmdIdx = pstcbNvt->bCmdNum;
}

static void nvt_cmd_cache_up(PSTCB_NVT pstcbNvt, SOCKET hRmtTelnetClt)
{
    if (pstcbNvt->bCmdIdx > 0)
    {
        pstcbNvt->bCmdIdx--;
        CHAR bCmdNum = 0, bCpyBytes;
        INT i;
        for (i = 0; i < NVTCMDCACHE_SIZE; )
        {
            if (pstcbNvt->pszCmdCache[i])
            {
                if (bCmdNum == pstcbNvt->bCmdIdx) //* 匹配
                {
                    CHAR *pszSndBuf;
                    EN_ONPSERR enErr;
                    if (NULL != (pszSndBuf = (CHAR *)buddy_alloc(NVT_ECHO_BUF_LEN_MAX, &enErr)))
                    {
                        bCpyBytes = pstcbNvt->pszCmdCache[i] - 1;
                        memcpy(pstcbNvt->szInputCache, pstcbNvt->pszCmdCache + i + 1, bCpyBytes);
                        memset(pszSndBuf, '\b', pstcbNvt->bCursorPos);
                        memcpy(pszSndBuf + pstcbNvt->bCursorPos, pstcbNvt->szInputCache, bCpyBytes);
                        memcpy(pszSndBuf + pstcbNvt->bCursorPos + bCpyBytes, "\033[K", 3);
                        tcp_send(hRmtTelnetClt, (UCHAR *)pszSndBuf, pstcbNvt->bCursorPos + bCpyBytes + 3);
                        buddy_free(pszSndBuf);
                        pstcbNvt->bInputBytes = bCpyBytes;
                        pstcbNvt->bCursorPos = bCpyBytes;
                    }
                    else
                        printf("nvt_cmd_cache_up() failed, %s\r\n", onps_error(enErr));


                    break;
                }

                i += pstcbNvt->pszCmdCache[i];
                bCmdNum++;
            }
            else //* 找到了尾部，则结束循环
                break;
        }
    }
}

static void nvt_cmd_cache_down(PSTCB_NVT pstcbNvt, SOCKET hRmtTelnetClt)
{
    if (pstcbNvt->bCmdIdx < pstcbNvt->bCmdNum - 1)
    {
        pstcbNvt->bCmdIdx++;
        CHAR bCmdNum = 0, bCpyBytes;
        INT i;
        for (i = 0; i < NVTCMDCACHE_SIZE; )
        {
            if (pstcbNvt->pszCmdCache[i])
            {
                if (bCmdNum == pstcbNvt->bCmdIdx) //* 匹配
                {
                    CHAR *pszSndBuf;
                    EN_ONPSERR enErr;
                    if (NULL != (pszSndBuf = (CHAR *)buddy_alloc(NVT_ECHO_BUF_LEN_MAX, &enErr)))
                    {
                        bCpyBytes = pstcbNvt->pszCmdCache[i] - 1;
                        memcpy(pstcbNvt->szInputCache, pstcbNvt->pszCmdCache + i + 1, bCpyBytes);
                        memset(pszSndBuf, '\b', pstcbNvt->bCursorPos);
                        memcpy(pszSndBuf + pstcbNvt->bCursorPos, pstcbNvt->szInputCache, bCpyBytes);
                        memcpy(pszSndBuf + pstcbNvt->bCursorPos + bCpyBytes, "\033[K", 3);
                        tcp_send(hRmtTelnetClt, (UCHAR *)pszSndBuf, pstcbNvt->bCursorPos + bCpyBytes + 3);
                        buddy_free(pszSndBuf);
                        pstcbNvt->bInputBytes = bCpyBytes;
                        pstcbNvt->bCursorPos = bCpyBytes;
                    }
                    else
                        printf("nvt_cmd_cache_up() failed, %s\r\n", onps_error(enErr));

                    break;
                }

                i += pstcbNvt->pszCmdCache[i];
                bCmdNum++;
            }
            else //* 找到了尾部，则结束循环
                break;
        }
    }
}
#endif

static void nvt_char_handler(PSTCB_NVT pstcbNvt, SOCKET hRmtTelnetClt, CHAR ch, BOOL blIsCtlChar)
{
    CHAR *pszSndBuf = NULL;
    EN_ONPSERR enErr;

    if (blIsCtlChar)
    {
        switch (ch)
        {
        case 'C':
            if (pstcbNvt->bCursorPos < pstcbNvt->bInputBytes)
            {
                if (3 == pstcbNvt->stSMach.nvt_srv_echo && SMACHNVT_PASSWD != pstcbNvt->stSMach.nvt_state)
                    tcp_send(hRmtTelnetClt, "\033[C", 3);
                pstcbNvt->bCursorPos++;
            }

            break;

        case 'D':
            if (pstcbNvt->bCursorPos > 0)
            {
                if (3 == pstcbNvt->stSMach.nvt_srv_echo && SMACHNVT_PASSWD != pstcbNvt->stSMach.nvt_state)
                    tcp_send(hRmtTelnetClt, "\b", 1);
                pstcbNvt->bCursorPos--;
            }
            break;

        case 'H':
            if (NULL != (pszSndBuf = (CHAR *)buddy_alloc(NVT_ECHO_BUF_LEN_MAX, &enErr)))
            {
                memset(pszSndBuf, '\b', pstcbNvt->bCursorPos);
                tcp_send(hRmtTelnetClt, (UCHAR *)pszSndBuf, pstcbNvt->bCursorPos);
                buddy_free(pszSndBuf);
                pstcbNvt->bCursorPos = 0;
            }
            else
                printf("nvt_char_handler() failed, %s\r\n", onps_error(enErr));
            break;

        case 'F':
            if (NULL != (pszSndBuf = (CHAR *)buddy_alloc(NVT_ECHO_BUF_LEN_MAX, &enErr)))
            {
                INT i;
                for (i = 0; i < (pstcbNvt->bInputBytes - pstcbNvt->bCursorPos) * 3; )
                {
                    pszSndBuf[i++] = '\033';
                    pszSndBuf[i++] = '[';
                    pszSndBuf[i++] = 'C';
                }
                tcp_send(hRmtTelnetClt, (UCHAR *)pszSndBuf, i);
                buddy_free(pszSndBuf);
                pstcbNvt->bCursorPos = pstcbNvt->bInputBytes;
            }
            else
                printf("nvt_char_handler() failed, %s\r\n", onps_error(enErr));
            break;
#if NVTCMDCACHE_EN
        case 'A':
            nvt_cmd_cache_up(pstcbNvt, hRmtTelnetClt);
            break;

        case 'B':
            nvt_cmd_cache_down(pstcbNvt, hRmtTelnetClt);
            break;
#endif
        }
    }
    else
    {
        if (0x1F < ch && ch < 0x7F)
        {
            if (pstcbNvt->bInputBytes > NVT_RCV_BUF_LEN_MAX - 2)
                return;

            pstcbNvt->stSMach.nvt_entering = 1;
            if (pstcbNvt->bCursorPos < pstcbNvt->bInputBytes)
            {
                //* 留出“空穴”，以便新字符占位
                CHAR bCpyBytes = pstcbNvt->bInputBytes - pstcbNvt->bCursorPos;
                memmove(&pstcbNvt->szInputCache[pstcbNvt->bCursorPos + 1], &pstcbNvt->szInputCache[pstcbNvt->bCursorPos], bCpyBytes);

                //* 插入新字符
                pstcbNvt->szInputCache[pstcbNvt->bCursorPos] = ch;

                //* 回显
                if (3 == pstcbNvt->stSMach.nvt_srv_echo && SMACHNVT_PASSWD != pstcbNvt->stSMach.nvt_state)
                {
                    if (NULL != (pszSndBuf = (CHAR *)buddy_alloc(NVT_ECHO_BUF_LEN_MAX, &enErr)))
                    {
                        memcpy(pszSndBuf, &pstcbNvt->szInputCache[pstcbNvt->bCursorPos], bCpyBytes + 1);
                        memset(pszSndBuf + bCpyBytes + 1, '\b', bCpyBytes);
                        tcp_send(hRmtTelnetClt, (UCHAR *)pszSndBuf, (bCpyBytes << 1) + 1);
                        buddy_free(pszSndBuf);
                    }
                    else
                        printf("nvt_char_handler() failed, %s\r\n", onps_error(enErr));
                }
            }
            else
            {
                pstcbNvt->szInputCache[pstcbNvt->bCursorPos] = ch;

                if (3 == pstcbNvt->stSMach.nvt_srv_echo && SMACHNVT_PASSWD != pstcbNvt->stSMach.nvt_state)
                    tcp_send(hRmtTelnetClt, (UCHAR *)&ch, 1);
            }
            pstcbNvt->bInputBytes++;
            pstcbNvt->bCursorPos++;
        }
        else
        {
            switch (ch)
            {
            case '\r':
                pstcbNvt->bCursorPos = 0;
                pstcbNvt->stSMach.nvt_entering = 0;
                pstcbNvt->szInputCache[pstcbNvt->bInputBytes++] = 0;
#if NVTCMDCACHE_EN
                if (SMACHNVT_INTERACTIVE == pstcbNvt->stSMach.nvt_state)
                    nvt_cmd_cache_add(pstcbNvt);
#endif
                break;

            case '\n':
            case '\0':
                tcp_send(hRmtTelnetClt, "\r\n", 2);
                break;

            case '\b':
            case '\177': //* ASCII删除符delete     
                if (pstcbNvt->bCursorPos > 0)
                {
                    CHAR bCpyBytes = pstcbNvt->bInputBytes - pstcbNvt->bCursorPos;
                    memmove(&pstcbNvt->szInputCache[pstcbNvt->bCursorPos - 1], &pstcbNvt->szInputCache[pstcbNvt->bCursorPos], bCpyBytes);
                    pstcbNvt->bInputBytes--;
                    pstcbNvt->szInputCache[pstcbNvt->bInputBytes] = '\0';
                    pstcbNvt->bCursorPos--;
                    if (3 == pstcbNvt->stSMach.nvt_srv_echo && SMACHNVT_PASSWD != pstcbNvt->stSMach.nvt_state)
                    {
                        if (NULL != (pszSndBuf = (CHAR *)buddy_alloc(NVT_ECHO_BUF_LEN_MAX, &enErr)))
                        {
                            sprintf(pszSndBuf, "\b%s\033[K", &pstcbNvt->szInputCache[pstcbNvt->bCursorPos]);
                            memset(pszSndBuf + bCpyBytes + 4, '\b', bCpyBytes);
                            tcp_send(hRmtTelnetClt, (UCHAR *)pszSndBuf, (bCpyBytes << 1) + 4);
                            buddy_free(pszSndBuf);
                        }
                        else
                            printf("nvt_char_handler() failed, %s\r\n", onps_error(enErr));
                    }
                }
                break;
            }
        }
    }
}

//* 客户端上报数据、指令处理函数
static void nvt_rcv_handler(PSTCB_NVT pstcbNvt, UCHAR *pubTelnetPkt, INT nPktLen)
{
    SOCKET hRmtTelnetClt = *(SOCKET *)((UCHAR *)pstcbNvt - offsetof(STCB_TELNETCLT, stcbNvt));
    UCHAR *pubNextData = pubTelnetPkt;
    INT nHandleBytes = 0;
    UCHAR i;

    while (nHandleBytes < nPktLen)
    {
        //* 指令，非数据
        if (TELNETCMD_IAC == pubNextData[0])
        {
            PST_TELNETPKT_CMD pstCmd = (PST_TELNETPKT_CMD)pubNextData;
            if (pstCmd->ubCmd > TELNETCMD_SB)
            {
                for (i = 0; i < NVTNEGOOPT_NUM; i++)
                {
                    if (l_staNvtNegoOpts[i].ubOption == pstCmd->ubNegoOption)
                    {
                        l_staNvtNegoOpts[i].pfunNegoGet(pstcbNvt, pstCmd->ubCmd);
                        break;
                    }
                }

                //* 对端发送了一个协议栈不支持的选项，则一律禁止
                if (i == NVTNEGOOPT_NUM)
                {
                    if (pstCmd->ubNegoOption != pstcbNvt->stSMach.ubLastAckOption)
                    {
                        telnet_cmd_ack_default(hRmtTelnetClt, pstCmd->ubCmd, pstCmd->ubNegoOption);
                        pstcbNvt->stSMach.ubLastAckOption = pstCmd->ubNegoOption;
                    }
                }

                pubNextData += sizeof(ST_TELNETPKT_CMD);
                nHandleBytes += sizeof(ST_TELNETPKT_CMD);
            }
            else
            {
                i = sizeof(ST_TELNETPKT_CMD);
                if (TELNETOPT_TERMTYPE == pstCmd->ubNegoOption)
                {
                    PST_TELNETPKT_SOPT_TERMTYPE pstTremType = (PST_TELNETPKT_SOPT_TERMTYPE)pubNextData;
                    if (TELNETOPT_TTCODE_IS == pstTremType->ubCode)
                    {
                        UCHAR *pubTermName = pubNextData + offsetof(ST_TELNETPKT_SOPT_TERMTYPE, ubEIAC);
                        for (i = 0; i < TERM_NAME_MAX - 1 && pubTermName[i] != TELNETCMD_IAC; i++)
                            pstcbNvt->stSMach.szTermName[i] = pubTermName[i];
                        pstcbNvt->stSMach.szTermName[i] = 0;
                        i += offsetof(ST_TELNETPKT_SOPT_TERMTYPE, ubEIAC);
                    }
                }

                //* 计算子选项长度            
                for (; i + nHandleBytes < nPktLen; i++)
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
            SOCKET hRmtTelnetClt = *(SOCKET *)((UCHAR *)pstcbNvt - offsetof(STCB_TELNETCLT, stcbNvt));

            //* 缓存用户输入的数据
            CHAR bRemainBytes = pubTelnetPkt + nPktLen - pubNextData;
            for (i = 0; i < bRemainBytes/* && pstcbNvt->bInputBytes < NVT_INPUT_CACHE_SIZE*/; )
            {
                if (TELNETCMD_IAC != pubNextData[i])
                {
                    if ('\033' != pubNextData[i])
                    {
                        nvt_char_handler(pstcbNvt, hRmtTelnetClt, pubNextData[i], FALSE);
                        i++;
                    }
                    else
                    {
                        if (i + 3 < bRemainBytes && '~' == pubNextData[i + 3])
                        {
                            if ('1' == pubNextData[i + 2])
                            {
                                nvt_char_handler(pstcbNvt, hRmtTelnetClt, 'H', TRUE);
                                i += 4;
                            }
                            else
                            {
                                nvt_char_handler(pstcbNvt, hRmtTelnetClt, 'F', TRUE);
                                i += 4;
                            }
                        }
                        else
                        {         
                            if ('[' == pubNextData[i + 1])
                            {
                                nvt_char_handler(pstcbNvt, hRmtTelnetClt, pubNextData[i + 2], TRUE);
                                i += 3;
                            }
                            else
                            {
                                nvt_char_handler(pstcbNvt, hRmtTelnetClt, pubNextData[i + 1], TRUE);
                                i += 2;
                            }
                        }
                    }

                }
                else
                    break;
            }

            pubNextData += i;
            nHandleBytes += i;
        }
    }
}

static CHAR telnet_login_handler(CHAR *pszInputData, CHAR *pbInputDataBytes, CHAR *pszUserInfo, UCHAR ubUserInfoLen)
{
    CHAR i;
    for (i = 0; i < *pbInputDataBytes; i++)
    {
        if ('\0' == pszInputData[i])
        {
            *pbInputDataBytes -= i + 1;
            if (i == ubUserInfoLen)
                return (strcmp(pszInputData, pszUserInfo) ? -1 : 1);
            else
                return -1;
        }
    }

    return 0;
}

static void nvt_printf(SOCKET hRmtTelnetClt, INT nFormatBufSize, const CHAR *pszInfo, ...)
{
    EN_ONPSERR enErr;
    CHAR *pszFormatBuf = (CHAR *)buddy_alloc(nFormatBufSize, &enErr);
    if (pszFormatBuf)
    {
        va_list pvaArgList;
        va_start(pvaArgList, pszInfo);
        vsnprintf(pszFormatBuf, nFormatBufSize, pszInfo, pvaArgList);
        va_end(pvaArgList);

        tcp_send(hRmtTelnetClt, (UCHAR *)pszFormatBuf, strlen(pszFormatBuf));

        buddy_free(pszFormatBuf);
    }
    else
        tcp_send(hRmtTelnetClt, "nvt_printf() failed, dynamic memory is empty.", sizeof("nvt_printf() failed, dynamic memory is empty.") - 1);
}

//* 输出onps标
static void nvt_print_logo(SOCKET hRmtTelnetClt)
{
    nvt_printf(hRmtTelnetClt, 512, "\033[01;32m       ______   .__   __. .______     _______.\r\n" \
                                   "      /  __  \\  |  \\ |  | |   _  \\   /       |\r\n" \
                                   "     |  |  |  | |   \\|  | |  |_)  | |   (----`\r\n" \
                                   "     |  |  |  | |  . `  | |   ___/   \\   \\    \r\n" \
                                   "     |  `--'  | |  |\\   | |  |   .----)   |   \r\n" \
                                   "      \\______/  |__| \\__| | _|   |_______/    \033[0m\r\n");

    /*
    send(hRmtTelnetClt, "\033[01;32m       ______   .__   __. .______     _______.\r\n", sizeof("\033[01;31m       ______   .__   __. .______     _______.\r\n") - 1, 1);    
    send(hRmtTelnetClt, "      /  __  \\  |  \\ |  | |   _  \\   /       |\r\n", sizeof("      /  __  \\  |  \\ |  | |   _  \\   /       |\r\n") - 1, 1);
    send(hRmtTelnetClt, "     |  |  |  | |   \\|  | |  |_)  | |   (----`\r\n", sizeof("     |  |  |  | |   \\|  | |  |_)  | |   (----`\r\n") - 1, 1);
    send(hRmtTelnetClt, "     |  |  |  | |  . `  | |   ___/   \\   \\    \r\n", sizeof("     |  |  |  | |  . `  | |   ___/   \\   \\    \r\n") - 1, 1);
    send(hRmtTelnetClt, "     |  `--'  | |  |\\   | |  |   .----)   |   \r\n", sizeof("     |  `--'  | |  |\\   | |  |   .----)   |   \r\n") - 1, 1);
    send(hRmtTelnetClt, "      \\______/  |__| \\__| | _|   |_______/    \033[0m\r\n", sizeof("      \\______/  |__| \\__| | _|   |_______/    \033[0m\r\n") - 1, 1);
    */
}

//* 输出登录信息
static void nvt_print_login_info(SOCKET hRmtTelnetClt)
{
    nvt_printf(hRmtTelnetClt, 128, "\033[01;32m%s\033[0m#\033[01;31mlogin\033[0m: ", NVT_PS);

    /*
    CHAR szFormatBuf[20];
    sprintf(szFormatBuf, "\033[01;32m%s\033[0m", NVT_PS);
    send(hRmtTelnetClt, (UCHAR *)szFormatBuf, strlen(szFormatBuf), 1);
    send(hRmtTelnetClt, "#\033[01;31mlogin\033[0m: ", sizeof("#\033[01;31mlogin\033[0m: ") - 1, 1);
    */
}

//* 输出密码录入提示
static void nvt_print_passwd_info(SOCKET hRmtTelnetClt)
{
    nvt_printf(hRmtTelnetClt, 128, "\033[01;32m%s\033[0m#\033[01;31mpassword\033[0m: ", NVT_PS);

    /*
    CHAR szFormatBuf[48];
    sprintf(szFormatBuf, "\033[01;32m%s\033[0m#\033[01;31mpassword\033[0m: ", NVT_PS);
    send(hRmtTelnetClt, (UCHAR *)szFormatBuf, strlen(szFormatBuf), 1);
    */
}

//* 输出命令行前缀
static void nvt_print_ps(SOCKET hRmtTelnetClt)
{
    nvt_printf(hRmtTelnetClt, 128, "\033[01;32m%s@%s\033[0m# ", NVT_USER_NAME, NVT_PS);

    /*
    CHAR szFormatBuf[32];
    sprintf(szFormatBuf, "\033[01;32m%s@%s\033[0m# ", NVT_USER_NAME, NVT_PS);
    send(hRmtTelnetClt, (UCHAR *)szFormatBuf, strlen(szFormatBuf), 1);
    */
}

static void telnet_cmd_handler(PSTCB_NVT pstcbNvt, SOCKET hRmtTelnetClt)
{
    CHAR i;
    for (i = 0; i < pstcbNvt->bInputBytes; i++)
    {
        if ('\0' == pstcbNvt->szInputCache[i])
        {
            UCHAR ubLen = strlen(pstcbNvt->szInputCache);
            if (ubLen)
            {
                //* 解析输入数据转换成指令执行需要的格式
                CHAR *pszaArg[NVTCMD_ARGC_MAX];
                CHAR bArgCnt = 0;
                CHAR *pszStart = pstcbNvt->szInputCache;
                CHAR *pszArg;
                do {
                    if (NULL != (pszArg = strtok_safe(&pszStart, " ")))
                    {
                        pszaArg[bArgCnt] = pszArg;
                        bArgCnt++;
                    }
                    else
                        break;
                } while (TRUE);

                if (strcmp(pszaArg[0], "help"))
                {
                    PST_NVTCMD_NODE pstNextNvtCmd = l_pstNvtCmdList;                    
                    while (pstNextNvtCmd)
                    {
                        if (!strcmp(pszaArg[0], pstNextNvtCmd->pstNvtCmd->pszCmdName))
                        {
                            pstcbNvt->stSMach.nvt_state = SMACHNVT_CMDEXECING;
                            pstcbNvt->stSMach.nvt_cmd_exec_en = TRUE;
                            pstcbNvt->bInputBytes = 0;
                            pstcbNvt->bCursorPos = 0;
                            pstNextNvtCmd->pstNvtCmd->pfun_cmd_entry(bArgCnt, pszaArg, (ULONGLONG)pstcbNvt);
                            return; 
                        }

                        pstNextNvtCmd = pstNextNvtCmd->pstNextCmd;
                    }

                    nvt_printf(hRmtTelnetClt, 128, "Command not supported by terminal: %s\r\n", pszaArg[0]);

                    /*
                    send(hRmtTelnetClt, (UCHAR *)"Command not supported by terminal: ", sizeof("Command not supported by terminal: ") - 1, 1);
                    send(hRmtTelnetClt, (UCHAR *)pszaArg[0], strlen(pszaArg[0]), 1);
                    send(hRmtTelnetClt, "\r\n", sizeof("\r\n") - 1, 1);
                    */
                    pstcbNvt->stSMach.nvt_state = SMACHNVT_CMDEXECEND; 
                }
                else
                {
                    CHAR szRmtTelnetClt[8];
                    sprintf(szRmtTelnetClt, "%d", (INT)hRmtTelnetClt);
                    pszaArg[1] = szRmtTelnetClt;
                    bArgCnt = 2;
                    help(bArgCnt, pszaArg, (ULONGLONG)pstcbNvt);
                }
            }
            else
                pstcbNvt->stSMach.nvt_state = SMACHNVT_CMDEXECEND; 

            pstcbNvt->bInputBytes -= i + 1;
            pstcbNvt->bCursorPos = 0;
            if (pstcbNvt->bInputBytes) //* 把剩余的用户数据前移到头部
                memmove(pstcbNvt->szInputCache, &pstcbNvt->szInputCache[i + 1], pstcbNvt->bInputBytes);
        }
    }
}

void thread_nvt_handler(void *pvParam)
{
    PSTCB_NVT pstcbNvt = (PSTCB_NVT)pvParam;
    PSTCB_TELNETCLT pstcbTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)pvParam - offsetof(STCB_TELNETCLT, stcbNvt));

    //* 状态机初始化    
    pstcbNvt->stSMach.uniFlag.unVal = 0;
    pstcbNvt->stSMach.ubLastAckOption = 0;
    pstcbNvt->stSMach.szTermName[0] = 0;
    pstcbNvt->stSMach.nvt_state = SMACHNVT_NEGO;

#if NVTCMDCACHE_EN
    EN_ONPSERR enErr;
    pstcbNvt->pszCmdCache = (CHAR *)buddy_alloc(NVTCMDCACHE_SIZE, &enErr);
    if (pstcbNvt->pszCmdCache)
    {
        pstcbNvt->bCmdNum = 0;
        pstcbNvt->bCmdIdx = 0;
        memset(pstcbNvt->pszCmdCache, 0, NVTCMDCACHE_SIZE);
    }
    else
        tcp_send(pstcbTelnetClt->hClient, "Failed to allocate memory for command cache, you will not be able to browse the command input history by using the up and down arrow keys.\r\n",
                sizeof("Failed to allocate memory for command cache, you will not be able to browse the command input history by using the up and down arrow keys.\r\n") - 1);

#endif


    //* 计算用于判断协商是否结束的掩码。每个协商选项字段的位0用于标记当前协商是否已结束，掩码与之进行“与”操作结果等于掩码则意味着协商结束
    //* 在设计上有意让ST_SMACHNVT::uniFlag::stb32中的各协商选项字段的位0组成了一个等比数列：2^0 + 2^2 + 2^4 +...+2^2n，其和Sn = (4^n - 1) / 3，Sn即为掩码值
    UINT unNegoEndMask = (pow(4, NVTNEGORESULT_NUM) - 1) / 3;

    UCHAR ubaRcvBuf[NVT_RCV_BUF_LEN_MAX];
    INT nRcvBytes;
    pstcbNvt->bCursorPos = 0;
    pstcbNvt->bInputBytes = 0;
    while (pstcbTelnetClt->bitTHRunEn)
    {
        switch (pstcbNvt->stSMach.nvt_state)
        {
        case SMACHNVT_NEGO:
            if (unNegoEndMask != (unNegoEndMask & pstcbNvt->stSMach.uniFlag.unVal))
            {
                if (os_get_system_secs() - pstcbTelnetClt->unLastOperateTime > 3)
                {
                    if (pstcbNvt->stSMach.nvt_try_cnt < 15)
                    {
                        nvt_nego_opt_send(pstcbNvt);
                        pstcbTelnetClt->unLastOperateTime = os_get_system_secs();
                        pstcbNvt->stSMach.nvt_try_cnt++;
                    }
                    else //* 一直未协商通过，则结束当前nvt的运行                                 
                    {
                        pstcbTelnetClt->bitTHRunEn = FALSE;
                        continue;
                    }
                }
            }
            else
            {
                pstcbNvt->stSMach.nvt_state = SMACHNVT_GETTERMNAME;
                pstcbNvt->stSMach.nvt_try_cnt = 0;
            }
            break;

        case SMACHNVT_GETTERMNAME:
            if (3 == pstcbNvt->stSMach.nvt_term_type) //* 已协商通过，对端同意激活该选项，则在这里就要获取对端的终端类型了
            {
                if (!pstcbNvt->stSMach.szTermName[0])
                {
                    if (pstcbNvt->stSMach.nvt_try_cnt < 6)
                    {
                        telnet_req_term_type(pstcbTelnetClt->hClient);
                        break;
                    }
                    else
                    {
                        pstcbTelnetClt->bitTHRunEn = FALSE;
                        continue;
                    }
                }
            }

            //* 输出协商配置结果信息
            nvt_print_logo(pstcbTelnetClt->hClient);
            nvt_printf(pstcbTelnetClt->hClient, 512, "=====================================================\r\n" \
                                                      "| Welcome to Zion, Your terminal type is \033[01;32m%s\033[0m\r\n" \
                                                      "| * Character operation mode\r\n| * Remote echo %s\r\n| * Local echo %s\r\n" \
                                                      "| * Case sensitive mode\r\n=====================================================\r\n", 
                pstcbNvt->stSMach.szTermName, pstcbNvt->stSMach.nvt_srv_echo == 3 ? "\033[01;32mOn\033[0m" : "\033[01;31mOff\033[0m", pstcbNvt->stSMach.nvt_clt_echo == 3 ? "\033[01;32mOn" : "\033[01;31mOff\033[0m");

            /*
            send(pstcbTelnetClt->hClient, "=====================================================\r\n", sizeof("=====================================================\r\n") - 1, 1);
            send(pstcbTelnetClt->hClient, "| Welcome to Zion, Your terminal type is ", sizeof("| Welcome to Zion, Your terminal type is ") - 1, 1);
            sprintf((CHAR *)ubaRcvBuf, "\033[01;32m%s\033[0m\r\n", pstcbNvt->stSMach.szTermName);
            send(pstcbTelnetClt->hClient, ubaRcvBuf, strlen((const char *)ubaRcvBuf), 1);
            send(pstcbTelnetClt->hClient, "| * Character operation mode\r\n| * Remote echo ", sizeof("| * Character operation mode\r\n| * Remote echo ") - 1, 1);
            sprintf((CHAR *)ubaRcvBuf, "%s\r\n| * Local echo %s\r\n", pstcbNvt->stSMach.nvt_srv_echo == 3 ? "\033[01;32mOn\033[0m" : "\033[01;31mOff\033[0m", pstcbNvt->stSMach.nvt_clt_echo == 3 ? "\033[01;32mOn" : "\033[01;31mOff\033[0m");
            send(pstcbTelnetClt->hClient, ubaRcvBuf, strlen((const char *)ubaRcvBuf), 1);
            send(pstcbTelnetClt->hClient, "| * Case sensitive mode\r\n=====================================================\r\n", sizeof("| * Case sensitive mode\r\n=====================================================\r\n") - 1, 1);
            */

            //* 输出登录信息
            nvt_print_login_info(pstcbTelnetClt->hClient);

            //* 迁移到“登录”阶段之前初始化用户输入相关的变量
            pstcbNvt->bInputBytes = 0;
            pstcbNvt->stSMach.nvt_try_cnt = 0;
            pstcbNvt->stSMach.nvt_state = SMACHNVT_LOGIN;
            break;

        case SMACHNVT_LOGIN:
            nRcvBytes = telnet_login_handler(pstcbNvt->szInputCache, &pstcbNvt->bInputBytes, NVT_USER_NAME, sizeof(NVT_USER_NAME) - 1);
            if (nRcvBytes)
            {
                if (nRcvBytes > 0)
                {
                    //sprintf((CHAR *)ubaRcvBuf, "\x1b[01;32m%s\x1b[0m#\x1b[01;31mpassword\x1b[0m: ", NVT_PS);
                    //send(hRmtTelnetClt, (const char *)ubaRcvBuf, strlen((const char *)ubaRcvBuf), 0);                    
                    pstcbNvt->stSMach.nvt_try_cnt = 0;
                    nvt_print_passwd_info(pstcbTelnetClt->hClient);
                    pstcbNvt->stSMach.nvt_state = SMACHNVT_PASSWD;
                }
                else
                {
                    pstcbNvt->stSMach.nvt_try_cnt++;
                    if (pstcbNvt->stSMach.nvt_try_cnt < 6)
                    {
                        nvt_printf(pstcbTelnetClt->hClient, 128, "Username is incorrect, please enter again. You have \033[01;31m%d\033[0m attempts left.\r\n", 6 - pstcbNvt->stSMach.nvt_try_cnt);

                        /*
                        send(pstcbTelnetClt->hClient, "Username is incorrect, please enter again. You have ", sizeof("Username is incorrect, please enter again. You have ") - 1, 1);
                        sprintf((CHAR *)ubaRcvBuf, "\033[01;31m%d\033[0m attempts left.\r\n", 6 - pstcbNvt->stSMach.nvt_try_cnt);
                        send(pstcbTelnetClt->hClient, ubaRcvBuf, strlen((const char *)ubaRcvBuf), 1);
                        */

                        nvt_print_login_info(pstcbTelnetClt->hClient); //* 用户名错误，再次输出登录信息
                    }
                    else
                    {
                        pstcbTelnetClt->bitTHRunEn = FALSE;
                        continue;
                    }
                }
            }
            break;

        case SMACHNVT_PASSWD:
            nRcvBytes = telnet_login_handler(pstcbNvt->szInputCache, &pstcbNvt->bInputBytes, NVT_USER_PASSWD, sizeof(NVT_USER_PASSWD) - 1);
            if (nRcvBytes)
            {
                if (nRcvBytes > 0)
                {
                    nvt_printf(pstcbTelnetClt->hClient, 1024, "\r\nWelcome %s to my world, We're in everything you see.\r\n%s* onps stack version: %s\r\n*   Official website: \033[01;34m%s\033[0m\r\n\r\n", NVT_USER_NAME, NVT_WELCOME_INFO, ONPS_VER, ONPS_OFFICIAL_WEB);

                    /*
                    sprintf((CHAR *)ubaRcvBuf, "\r\nWelcome %s to my world, We're in everything you see.\r\n", NVT_USER_NAME);
                    send(pstcbTelnetClt->hClient, ubaRcvBuf, strlen((const char *)ubaRcvBuf), 1);
                    send(pstcbTelnetClt->hClient, NVT_WELCOME_INFO, sizeof(NVT_WELCOME_INFO) - 1, 1);

                    sprintf((CHAR *)ubaRcvBuf, "* onps stack version: %s\r\n", ONPS_VER);
                    send(pstcbTelnetClt->hClient, ubaRcvBuf, strlen((const char *)ubaRcvBuf), 1);

                    send(pstcbTelnetClt->hClient, "*   Official website:", sizeof("*   Official website:") - 1, 1);
                    sprintf((CHAR *)ubaRcvBuf, " \033[01;34m%s\033[0m\r\n\r\n", ONPS_OFFICIAL_WEB);
                    send(pstcbTelnetClt->hClient, ubaRcvBuf, strlen((const char *)ubaRcvBuf), 1);
                    */

                    nvt_print_ps(pstcbTelnetClt->hClient);
                    pstcbNvt->stSMach.nvt_state = SMACHNVT_INTERACTIVE;
                }
                else
                {
                    pstcbNvt->stSMach.nvt_try_cnt++;
                    if (pstcbNvt->stSMach.nvt_try_cnt < 6)
                    {
                        nvt_printf(pstcbTelnetClt->hClient, 128, "Password is incorrect, please enter again. You have \033[01;31m%d\033[0m attempts left.\r\n", 6 - pstcbNvt->stSMach.nvt_try_cnt);

                        /*
                        send(pstcbTelnetClt->hClient, "Password is incorrect, please enter again. You have ", sizeof("Password is incorrect, please enter again. You have ") - 1, 1);
                        sprintf((CHAR *)ubaRcvBuf, "\033[01;31m%d\033[0m attempts left.\r\n", 6 - pstcbNvt->stSMach.nvt_try_cnt);
                        send(pstcbTelnetClt->hClient, ubaRcvBuf, strlen((const char *)ubaRcvBuf), 1);
                        */
                        nvt_print_passwd_info(pstcbTelnetClt->hClient);
                    }
                    else
                    {
                        pstcbTelnetClt->bitTHRunEn = FALSE;
                        continue;
                    }
                }
            }
            break;

        case SMACHNVT_INTERACTIVE:
            telnet_cmd_handler(pstcbNvt, pstcbTelnetClt->hClient);
            break;

        case SMACHNVT_CMDEXECEND:
            if (pstcbTelnetClt->unLastOperateTime)
            {
                nvt_print_ps(pstcbTelnetClt->hClient);
                pstcbNvt->stSMach.nvt_state = SMACHNVT_INTERACTIVE;
            }

            break;
        }

        //* 接收
        nRcvBytes = recv(pstcbTelnetClt->hClient, ubaRcvBuf, sizeof(ubaRcvBuf)); 
        if (nRcvBytes > 0)
        {
            if (SMACHNVT_CMDEXECING != pstcbNvt->stSMach.nvt_state)
                nvt_rcv_handler(pstcbNvt, ubaRcvBuf, nRcvBytes);
            else
            {
                os_critical_init();
                os_enter_critical();
                {
                    CHAR bCpyBytes = NVT_INPUT_CACHE_SIZE - pstcbNvt->bInputBytes;
                    bCpyBytes = nRcvBytes < bCpyBytes ? nRcvBytes : bCpyBytes;
                    memcpy(&pstcbNvt->szInputCache[pstcbNvt->bInputBytes], ubaRcvBuf, bCpyBytes);
                    pstcbNvt->bInputBytes += bCpyBytes;
                }
                os_exit_critical();
            }

            pstcbTelnetClt->unLastOperateTime = os_get_system_secs();
        }
        else
        {
            //* 如果返回值小于0，表明当前tcp链路出现了故障，则立即结束当前nvt的运行
            if (nRcvBytes < 0)
            {
                pstcbTelnetClt->bitTHRunEn = FALSE;
        #if SUPPORT_PRINTF
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif	
                printf("recv() failed in file %s, %s\r\n", __FILE__, onps_get_last_error(pstcbTelnetClt->hClient, NULL)); 
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
        #endif
                
                break;
            }

            os_sleep_ms(10);
        }
    }

#if NVTCMDCACHE_EN    
    if (pstcbNvt->pszCmdCache)
        buddy_free(pstcbNvt->pszCmdCache);
#endif

    //* 假如当前还有正在执行中的指令，结束nvt之前就需要先通知其停止运行并等待一小段时间使其能够有充足的时间安全结束
    if (SMACHNVT_CMDEXECING == pstcbNvt->stSMach.nvt_state)
    {
        pstcbNvt->stSMach.nvt_cmd_exec_en = FALSE;
        INT nTimeout = 0;
        while (SMACHNVT_CMDEXECING == pstcbNvt->stSMach.nvt_state && nTimeout++ < 6 * 100)
            os_sleep_ms(10);

        if (SMACHNVT_CMDEXECING == pstcbNvt->stSMach.nvt_state)
            nvt_cmd_kill();
    }

    //* 当前nvt已结束运行，需要显式地通知Telnet服务器清除该客户端以释放占用的相关资源    
    pstcbTelnetClt->unLastOperateTime = 0;
    pstcbTelnetClt->bitTHIsEnd = TRUE;
}

static UCHAR nego_put_option(UCHAR **ppubFilled, CHAR bNegoEnd, UCHAR ubCmd, UCHAR ubOption)
{
    //* 协商完毕则不再填充
    if (bNegoEnd)
        return 0;

    PST_TELNETPKT_CMD pstCmd = (PST_TELNETPKT_CMD)*ppubFilled;
    pstCmd->ubIAC = TELNETCMD_IAC;
    pstCmd->ubCmd = ubCmd;
    pstCmd->ubNegoOption = ubOption;

    *ppubFilled = (*ppubFilled) + sizeof(ST_TELNETPKT_CMD);
    return sizeof(ST_TELNETPKT_CMD);
}

static UCHAR nego_put_term_type(PSTCB_NVT pstcbNvt, UCHAR **ppubFilled)
{
    return nego_put_option(ppubFilled, pstcbNvt->stSMach.nvt_term_type, TELNETCMD_DO, TELNETOPT_TERMTYPE);
}

static UCHAR nego_put_suppress_go_ahead(PSTCB_NVT pstcbNvt, UCHAR **ppubFilled)
{
    return nego_put_option(ppubFilled, pstcbNvt->stSMach.nvt_srv_sga, TELNETCMD_WILL, TELNETOPT_SGA);
}

static UCHAR nego_put_echo(PSTCB_NVT pstcbNvt, UCHAR **ppubFilled)
{
    UCHAR ubFilledBytes = nego_put_option(ppubFilled, pstcbNvt->stSMach.nvt_srv_echo, TELNETCMD_WILL, TELNETOPT_ECHO);
    ubFilledBytes += nego_put_option(ppubFilled, pstcbNvt->stSMach.nvt_clt_echo, TELNETCMD_DO, TELNETOPT_ECHO); //* 一定是DO，然后由对端自行决定是否激活客户端的本地回显，最终由服务器拒绝激活才可顺利协商
    return ubFilledBytes;
}

static void nego_get_term_type(PSTCB_NVT pstcbNvt, UCHAR ubCmd)
{
    SOCKET hRmtTelnetClt = *(SOCKET *)((UCHAR *)pstcbNvt - offsetof(STCB_TELNETCLT, stcbNvt)); 

    if (TELNETCMD_WILL == ubCmd)
    {
        pstcbNvt->stSMach.nvt_term_type = 3; //* 对端同意激活终端类型选项 
        telnet_req_term_type(hRmtTelnetClt); //* 请求对端的终端类型
    }
    else if (TELNETCMD_WONT == ubCmd)
    {
        pstcbNvt->stSMach.nvt_term_type = 1; //* 对端不同意激活终端类型选项        
        telnet_cmd_send(hRmtTelnetClt, TELNETCMD_DONT, TELNETOPT_TERMTYPE); //* 告知对端，已知晓禁止激活终端类型选项 
    }
}

static void nego_get_suppress_go_ahead(PSTCB_NVT pstcbNvt, UCHAR ubCmd)
{
    SOCKET hRmtTelnetClt = *(SOCKET *)((UCHAR *)pstcbNvt - offsetof(STCB_TELNETCLT, stcbNvt));

    if (TELNETCMD_DO == ubCmd)
        pstcbNvt->stSMach.nvt_srv_sga = 3;
    else if (TELNETCMD_DONT == ubCmd)
        tcp_send(hRmtTelnetClt, "The client refused the Suppress Go Ahead option on the server and the negotiation was aborted", sizeof("The client refused the Suppress Go Ahead option on the server and the negotiation was aborted") - 1);
    else if (TELNETCMD_WILL == ubCmd)
        telnet_cmd_send(hRmtTelnetClt, TELNETCMD_DO, TELNETOPT_SGA);
    else if (TELNETCMD_WONT == ubCmd)
        telnet_cmd_send(hRmtTelnetClt, TELNETCMD_DONT, TELNETOPT_SGA);
}

static void nego_get_echo(PSTCB_NVT pstcbNvt, UCHAR ubCmd)
{
    SOCKET hRmtTelnetClt = *(SOCKET *)((UCHAR *)pstcbNvt - offsetof(STCB_TELNETCLT, stcbNvt));

    if (TELNETCMD_DO == ubCmd)
        pstcbNvt->stSMach.nvt_srv_echo = 3;
    else if (TELNETCMD_DONT == ubCmd)
    {
        pstcbNvt->stSMach.nvt_srv_echo = 1;
        telnet_cmd_send(hRmtTelnetClt, TELNETCMD_WONT, TELNETOPT_ECHO);
    }
    else if (TELNETCMD_WILL == ubCmd)
        telnet_cmd_send(hRmtTelnetClt, TELNETCMD_DONT, TELNETOPT_ECHO);
    else if (TELNETCMD_WONT == ubCmd)
    {
        if (1 != pstcbNvt->stSMach.nvt_clt_echo)
        {
            pstcbNvt->stSMach.nvt_clt_echo = 1;
            telnet_cmd_send(hRmtTelnetClt, TELNETCMD_DONT, TELNETOPT_ECHO);
        }
    }
}

void nvt_cmd_add(PST_NVTCMD_NODE pstCmdNode, const ST_NVTCMD *pstCmd)
{
    os_critical_init();

    os_enter_critical();
    {
        pstCmdNode->pstNvtCmd = pstCmd;
        pstCmdNode->pstNextCmd = l_pstNvtCmdList;
        l_pstNvtCmdList = pstCmdNode;

        CHAR bCmdLen = strlen(pstCmd->pszCmdName);
        if (l_bNvtCmdLenMax < bCmdLen)
            l_bNvtCmdLenMax = bCmdLen;
    }
    os_exit_critical();
}

void nvt_cmd_exec_end(ULONGLONG ullNvtHandle)
{
    PSTCB_NVT pstcbNvt = (PSTCB_NVT)ullNvtHandle;
    pstcbNvt->stSMach.nvt_state = SMACHNVT_CMDEXECEND;
}

BOOL nvt_cmd_exec_enable(ULONGLONG ullNvtHandle)
{
    PSTCB_NVT pstcbNvt = (PSTCB_NVT)ullNvtHandle;
    return pstcbNvt->stSMach.nvt_cmd_exec_en;
}

static INT help(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{    
    SOCKET hRmtTelnetClt = atoi(argv[1]);
    PST_NVTCMD_NODE pstNextNvtCmd = l_pstNvtCmdList;
    while (pstNextNvtCmd)
    {        
        nvt_printf(hRmtTelnetClt, 256, "\033[01;33m%*s\033[0m %s", l_bNvtCmdLenMax, pstNextNvtCmd->pstNvtCmd->pszCmdName, pstNextNvtCmd->pstNvtCmd->pszReadme);
        pstNextNvtCmd = pstNextNvtCmd->pstNextCmd;
    }

    nvt_cmd_exec_end(ullNvtHandle);

    return 0;
}

static INT logout(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt));
    tcp_send(pstTelnetClt->hClient, "You have logged out.\r\n", sizeof("You have logged out.\r\n") - 1);
    pstTelnetClt->bitTHRunEn = FALSE;

    nvt_cmd_exec_end(ullNvtHandle);

    return 0;
}

#if NVTCMD_MEMUSAGE_EN
static INT mem_usage(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt));
    UINT unTotalBytes, unUsedBytes, unMaxFreedPageSize, unMinFreedPageSize;
    FLOAT flUsage = buddy_usage_details(&unTotalBytes, &unUsedBytes, &unMaxFreedPageSize, &unMinFreedPageSize) * 100.0; 

    nvt_printf(pstTelnetClt->hClient, 256, "     Current memory usage: %0.1f%%\r\n"
                                           "Total dynamic memory size: %d KBytes\r\n"
                                           "Current memory usage size: %0.1f KBytes\r\n"
                                           "Max free memory page size: %d Bytes\r\n"
                                           "Min free memory page size: %d Bytes\r\n" , flUsage, unTotalBytes / 1024, (FLOAT)unUsedBytes / 1024.0, unMaxFreedPageSize, unMinFreedPageSize);

    nvt_cmd_exec_end(ullNvtHandle);

    return 0;
}
#endif //* #if NVTCMD_MEMUSAGE_EN

#if NVTCMD_NETIF_EN
static INT netif(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt));

    EN_ONPSERR enErr; 
    CHAR *pszFormatBuf = (CHAR *)buddy_alloc(1024, &enErr);
    if (pszFormatBuf)
    {
        const ST_NETIF *pstNetif = NULL; 
        do {
            if (NULL != (pstNetif = netif_get_next(pstNetif)))
            {
        #if SUPPORT_PPP
                if (pstNetif->enType == NIF_PPP)
                {
                    sprintf(pszFormatBuf, "Network adapter <ppp, point-to-point protocol> : %s <--> %s\r\n", pstNetif->szName, get_ppp_port_name(*((HTTY *)pstNetif->pvExtra))); 
                    UCHAR *pubAddr = (UCHAR *)&pstNetif->stIPv4.unAddr;
                    sprintf(pszFormatBuf + strlen(pszFormatBuf), "\r\n\r\n    IPv4 : %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
                    pubAddr = (UCHAR *)&pstNetif->stIPv4.unSubnetMask;
                    sprintf(pszFormatBuf + strlen(pszFormatBuf), " netmask %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
                    pubAddr = (UCHAR *)&pstNetif->stIPv4.unGateway;
                    printf(", Point to Point %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
                    if (pstNetif->stIPv4.unPrimaryDNS)
                    {
                        pubAddr = (UCHAR *)&pstNetif->stIPv4.unPrimaryDNS;
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), "     Dns : %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);

                        if (pstNetif->stIPv4.unSecondaryDNS)
                        {
                            pubAddr = (UCHAR *)&pstNetif->stIPv4.unSecondaryDNS;
                            sprintf(pszFormatBuf + strlen(pszFormatBuf), "       : %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
                        }
                    }

                    tcp_send(pstTelnetClt->hClient, (UCHAR *)pszFormatBuf, strlen(pszFormatBuf));                
                    continue;                 
                }
        #endif

        #if SUPPORT_ETHERNET
                if (pstNetif->enType == NIF_ETHERNET)
                {
                    PST_NETIFEXTRA_ETH pstExtra = (PST_NETIFEXTRA_ETH)pstNetif->pvExtra;                    
                    sprintf(pszFormatBuf, "Network adapter <ethernet> : %s\r\n     Mac : ", pstNetif->szName);  
                    netif_eth_mac_to_ascii(pstExtra->ubaMacAddr, pszFormatBuf + strlen(pszFormatBuf)); 

                    UCHAR *pubAddr = (UCHAR *)&pstNetif->stIPv4.unAddr;
                    sprintf(pszFormatBuf + strlen(pszFormatBuf), "\r\n\r\n    IPv4 : %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
                    pubAddr = (UCHAR *)&pstNetif->stIPv4.unSubnetMask; 
                    sprintf(pszFormatBuf + strlen(pszFormatBuf), " netmask %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
                    pubAddr = (UCHAR *)&pstNetif->stIPv4.unBroadcast; 
                    sprintf(pszFormatBuf + strlen(pszFormatBuf), " broadcast %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
                    if (pstNetif->stIPv4.unGateway)
                    {
                        pubAddr = (UCHAR *)&pstNetif->stIPv4.unGateway;
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), " gateway %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
                    }                    

                    if (pstExtra->bIsStaticAddr)
                    {
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), " static\r\n"); 
                        
                #if ETH_EXTRA_IP_EN
                        UINT unNextIp = 0, unSubnetMask; 
                        do {
                            if (0 != (unNextIp = netif_eth_get_next_ip(pstNetif, &unSubnetMask, unNextIp)))
                            {
                                pubAddr = (UCHAR *)&unNextIp; 
                                sprintf(pszFormatBuf + strlen(pszFormatBuf), "         : %d.%d.%d.%d", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
                                pubAddr = (UCHAR *)&unSubnetMask;
                                sprintf(pszFormatBuf + strlen(pszFormatBuf), " netmask %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 
                            }
                        } while (unNextIp); 
                #endif //* #if ETH_EXTRA_IP_EN
                    }
                    else                    
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), " dhcp\r\n"); 

                    if (pstNetif->stIPv4.unPrimaryDNS)
                    {
                        pubAddr = (UCHAR *)&pstNetif->stIPv4.unPrimaryDNS; 
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), "     Dns : %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]); 

                        if (pstNetif->stIPv4.unSecondaryDNS)
                        {
                            pubAddr = (UCHAR *)&pstNetif->stIPv4.unSecondaryDNS; 
                            sprintf(pszFormatBuf + strlen(pszFormatBuf), "         : %d.%d.%d.%d\r\n", pubAddr[0], pubAddr[1], pubAddr[2], pubAddr[3]);
                        }
                    }    

            #if SUPPORT_IPV6
                    sprintf(pszFormatBuf + strlen(pszFormatBuf), "\r\n    IPv6 : "); 
                    inet6_ntoa(pstNetif->nif_lla_ipv6, pszFormatBuf + strlen(pszFormatBuf)); 
                    sprintf(pszFormatBuf + strlen(pszFormatBuf), " (Link-local address)\r\n"); 
                    
                    EN_IPv6ADDRSTATE enState; 
                    UINT unValidLifetime; 
                    UCHAR ubaIpv6[16], ubaPriDns[16], ubaSecDns[16]; 
                    ubaIpv6[0] = 0; 
                    while (netif_eth_get_next_ipv6(pstNetif, ubaIpv6, &enState, &unValidLifetime))
                    {
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), "         : ");
                        inet6_ntoa(ubaIpv6, pszFormatBuf + strlen(pszFormatBuf)); 
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), " %s (Valid lifetime %d secs)\r\n", ipv6_addr_state(enState), unValidLifetime); 
                    }
                    
                    CHAR bRouterPrf; 
                    USHORT usMtu; 
                    ubaIpv6[0] = 0;
                    while (netif_eth_get_next_ipv6_router(pstNetif, ubaIpv6, &bRouterPrf, &usMtu, (USHORT *)&unValidLifetime, ubaPriDns, ubaSecDns))
                    {
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), "  Router : "); 
                        inet6_ntoa(ubaIpv6, pszFormatBuf + strlen(pszFormatBuf)); 
                        sprintf(pszFormatBuf + strlen(pszFormatBuf), " (%s, MTU %d, Lifetime %d)\r\n", i6r_prf_desc(bRouterPrf), usMtu, (USHORT)unValidLifetime); 
                        if (ubaPriDns[0])
                        {
                            sprintf(pszFormatBuf + strlen(pszFormatBuf), "     Dns : "); 
                            inet6_ntoa(ubaPriDns, pszFormatBuf + strlen(pszFormatBuf)); 
                            sprintf(pszFormatBuf + strlen(pszFormatBuf), "\r\n%s", ubaSecDns[0] ? "         : " : "");  
                            if (ubaSecDns[0])
                            {
                                inet6_ntoa(ubaSecDns, pszFormatBuf + strlen(pszFormatBuf));
                                sprintf(pszFormatBuf + strlen(pszFormatBuf), "\r\n"); 
                            }
                        }
                    }
            #endif //* #if SUPPORT_IPV6

                    tcp_send(pstTelnetClt->hClient, (UCHAR *)pszFormatBuf, strlen(pszFormatBuf)); 
                    continue; 
                }
        #endif  //* #if SUPPORT_ETHERNET
            }
        } while (pstNetif);

        buddy_free(pszFormatBuf); 
    }
    else
        tcp_send(pstTelnetClt->hClient, "Failed to execute \033[01;33mnetif\033[0m command, dynamic memory is empty.\r\n", sizeof("Failed to execute \033[01;33mnetif\033[0m command, dynamic memory is empty.\r\n") - 1);
    
    nvt_cmd_exec_end(ullNvtHandle); 

    return 0; 
}
#endif //* #if NVTCMD_NETIF_EN

#if NVTCMD_IFIP_EN && SUPPORT_ETHERNET
#define NVTHELP_IFIP_USAGE_ADD "Add an IP address, for example: \033[01;37mifip add <if name, like eth0, etc> <ip> <subnet mask>\033[0m\r\n"
#define NVTHELP_IFIP_USAGE_DEL "Delete an IP address, for example: \033[01;37mifip del <if name> <ip>\033[0m\r\n"
#define NVTHELP_IFIP_USAGE_SET "Modify Ethernet MAC address, for example: \033[01;37mifip set mac <if name> <mac, like 4E-65-6F-XX-XX-XX, etc>\033[0m\r\n" \
                               "Modify IP address, note that if it is currently a dynamic address, this command will change it to a static address, for example: \033[01;37mifip set ip <if name> <ip> <subnet mask> <gateway>\033[0m\r\n" \
                               "Modify DNS address, for example: \033[01;37mifip set dns <if name> <primary dns addr> [secondary dns addr, if exists]\033[0m\r\n" \
                               "Modify to dhcp, for example: \033[01;37mifip set dhcp <if name>\033[0m\r\n"

static INT ifip(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    EN_ONPSERR enErr; 
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt)); 
    if (argc > 1)
    {        
        if (!strcmp("add", argv[1]))
        {
        #if ETH_EXTRA_IP_EN
            if (argc == 5)
            {
                in_addr_t unIp = inet_addr(argv[3]); 
                in_addr_t unSubnetMask = inet_addr(argv[4]); 
                if (netif_eth_add_ip_by_if_name(argv[2], unIp, unSubnetMask, &enErr)) 
                {
                    if (os_nvt_add_ip(argv[2], unIp, unSubnetMask))
                    {
                        nvt_printf(pstTelnetClt->hClient, 256, "Successfully added IP address %s, netmask %s.\r\n", argv[3], argv[4]);
                        goto __lblEnd; 
                    }
                    else
                    {
                        netif_eth_del_ip_by_if_name(argv[2], unIp, NULL); 
                        enErr = ERREXTRAIPSAVE; 
                    }
                }
                
                goto __lblErr; 
            }
            else
            {
                tcp_send(pstTelnetClt->hClient, NVTHELP_IFIP_USAGE_ADD, sizeof(NVTHELP_IFIP_USAGE_ADD) - 1); 
                goto __lblEnd; 
            }
        #else
            tcp_send(pstTelnetClt->hClient, "The protocol stack does not support adding multiple IP addresses.\r\n", sizeof("The protocol stack does not support adding multiple IP addresses.\r\n") - 1);
        #endif //* #if ETH_EXTRA_IP_EN
        }
        else if (!strcmp("del", argv[1]))
        {
        #if ETH_EXTRA_IP_EN
            if (argc == 4)
            {
                in_addr_t unIp = inet_addr(argv[3]); 
                if (os_nvt_del_ip(argv[2], unIp))
                {
                    nvt_printf(pstTelnetClt->hClient, 256, "Successfully deleted IP address %s. the system will be restarted immediately...\r\n", argv[3]); 

                    os_sleep_secs(3);
                    close(pstTelnetClt->hClient);
                    os_nvt_system_reset();
                    goto __lblEnd;
                }
                else
                    enErr = ERREXTRAIPDEL;                

                goto __lblErr;
            }
            else
            {
                tcp_send(pstTelnetClt->hClient, NVTHELP_IFIP_USAGE_DEL, sizeof(NVTHELP_IFIP_USAGE_DEL) - 1);
                goto __lblEnd; 
            }
        #else
            tcp_send(pstTelnetClt->hClient, "The protocol stack does not support an IP address deletion command.\r\n", sizeof("The protocol stack does not support an IP address deletion command.\r\n") - 1);
        #endif
        }
        else if (!strcmp("set", argv[1]))
        {
            if (argc > 3)
            {
                if (!strcmp("ip", argv[2]))
                {
                    if (argc == 7)
                    {
                        in_addr_t unIp = inet_addr(argv[4]); 
                        in_addr_t unSubnetMask = inet_addr(argv[5]); 
                        in_addr_t unGateway = inet_addr(argv[6]);                         
                        if (os_nvt_set_ip(argv[3], unIp, unSubnetMask, unGateway))
                        {
                            nvt_printf(pstTelnetClt->hClient, 256, "Successfully updated IP address, the system will be restarted immediately...\r\n");

                            os_sleep_secs(3);
                            close(pstTelnetClt->hClient);
                            os_nvt_system_reset();

                            goto __lblEnd;
                        }
                        else
                            enErr = ERRIPUPDATED;                        

                        goto __lblErr; 
                    }
                    else
                    {
                        tcp_send(pstTelnetClt->hClient, "Parameter error, usage is as follows: \033[01;37mifip set ip <if name> <ip> <subnet mask> <gateway>\033[0m\r\n", 
                                    sizeof("Parameter error, usage is as follows: \033[01;37mifip set ip <if name> <ip> <subnet mask> <gateway>\033[0m\r\n") - 1); 
                        goto __lblEnd;
                    }
                }
                else if (!strcmp("mac", argv[2]))
                {
                    if (argc == 5)
                    {
                        if (netif_eth_set_mac_by_if_name(argv[3], argv[4], &enErr))
                        {
                            if (os_nvt_set_mac(argv[3], argv[4]))
                            {
                                tcp_send(pstTelnetClt->hClient, "MAC address updated successfully.\r\n", sizeof("MAC address updated successfully.\r\n") - 1); 
                                goto __lblEnd;
                            }
                            else
                                enErr = ERRIPUPDATED; 
                        }
                        
                        goto __lblErr;
                    }
                    else
                    {
                        tcp_send(pstTelnetClt->hClient, "Parameter error, usage is as follows: \033[01;37mifip set mac <if name> <mac, like 4E-65-6F-XX-XX-XX, etc>\033[0m\r\n",
                            sizeof("Parameter error, usage is as follows: \033[01;37mifip set mac <if name> <mac, like 4E-65-6F-XX-XX-XX, etc>\033[0m\r\n") - 1);
                        goto __lblEnd;
                    }
                }
                else if (!strcmp("dns", argv[2]))
                {
                    if (argc == 5 || argc == 6)
                    {
                        in_addr_t unPrimaryDns = inet_addr(argv[4]);
                        in_addr_t unSecondaryDns = (argc == 6) ? inet_addr(argv[5]) : 0; 
                        if (netif_eth_set_dns_by_if_name(argv[3], unPrimaryDns, unSecondaryDns, &enErr)) 
                        {
                            if (os_nvt_set_dns(argv[3], unPrimaryDns, unSecondaryDns)) 
                            {
                                tcp_send(pstTelnetClt->hClient, "Dns address updated successfully.\r\n", sizeof("Dns address updated successfully.\r\n") - 1);
                                goto __lblEnd;
                            }
                            else
                                enErr = ERRIPUPDATED;
                        }

                        goto __lblErr; 
                    }
                    else
                    {
                        tcp_send(pstTelnetClt->hClient, "Parameter error, usage is as follows: \033[01;37mifip set dns <if name> <primary dns addr> [secondary dns addr, if exists]\033[0m\r\n",
                            sizeof("Parameter error, usage is as follows: \033[01;37mifip set dns <if name> <primary dns addr> [secondary dns addr, if exists]\033[0m\r\n") - 1);
                        goto __lblEnd;
                    }
                }
                else if (!strcmp("dhcp", argv[2]))
                {
                    if (argc == 4)
                    {
                        enErr = ERRNO; 
                        BOOL blRtnVal = netif_eth_is_static_addr(argv[3], &enErr);
                        if (ERRNO == enErr)
                        {
                            if (blRtnVal)
                            {     
                                if (os_nvt_set_dhcp(argv[3]))
                                {
                                    tcp_send(pstTelnetClt->hClient, "Successfully modified to dynamic address mode. System will immediately boot to reconfigure IP address ...\r\n", sizeof("Successfully modified to dynamic address mode. System will immediately boot to reconfigure IP address ...\r\n") - 1);

                                    os_sleep_secs(3);
                                    close(pstTelnetClt->hClient);
                                    os_nvt_system_reset();

                                    goto __lblEnd; 
                                }
                                else
                                    enErr = ERRIPUPDATED;
                            }
                            else
                            {
                                tcp_send(pstTelnetClt->hClient, "DHCP client is already enabled, no need to set it again.\r\n", sizeof("DHCP client is already enabled, no need to set it again.\r\n") - 1);
                                goto __lblEnd; 
                            }
                        }

                        goto __lblErr; 
                    }
                    else
                    {
                        tcp_send(pstTelnetClt->hClient, "Parameter error, usage is as follows: \033[01;37mifip set dhcp <if name>\033[0m\r\n",
                            sizeof("Parameter error, usage is as follows: \033[01;37mifip set dhcp <if name>\033[0m\r\n") - 1);
                        goto __lblEnd;
                    }
                }
                else
                {
                    nvt_printf(pstTelnetClt->hClient, 256, "Unsupported set option: \033[01;37m%s\033[0m, Please choose from \033[01;37mip\033[0m, \033[01;37mmac\033[0m, \033[01;37mdns\033[0m, \033[01;37mdhcp\033[0m.\r\n", argv[2]);
                    goto __lblEnd;
                }
            }
            else
            {
                nvt_printf(pstTelnetClt->hClient, 1024, "Parameter error, usage is as follows: \r\n%s", NVTHELP_IFIP_USAGE_SET, sizeof(NVTHELP_IFIP_USAGE_SET) - 1);
                goto __lblEnd;
            }
        }
        else
        {
            nvt_printf(pstTelnetClt->hClient, 256, "Unsupported operation option: \033[01;37m%s\033[0m, Please choose from \033[01;37madd\033[0m, \033[01;37mdel\033[0m, \033[01;37mset\033[0m.\r\n", argv[1]); 
            goto __lblEnd;
        }
    }
    else
    {
        nvt_printf(pstTelnetClt->hClient, 1024, "%s%s%s", NVTHELP_IFIP_USAGE_ADD, NVTHELP_IFIP_USAGE_DEL, NVTHELP_IFIP_USAGE_SET); 
        goto __lblEnd;
    }

__lblErr: 
    nvt_printf(pstTelnetClt->hClient, 256, "The \033[01;37mifip\033[0m command failed, %s.\r\n", onps_error(enErr));

__lblEnd: 
    nvt_cmd_exec_end(ullNvtHandle);

    return 0; 
}
#endif //* #if NVTCMD_IFIP_EN && SUPPORT_ETHERNET

#if NVTCMD_ROUTE_EN
#define NVTHELP_ROUTE_USAGE_PRINT "Print routing table information, for example: \033[01;37mroute print\033[0m\r\n"
#define NVTHELP_ROUTE_USAGE_ADD   "Add a new routing entry, for example: \033[01;37mroute add <if name> <destination addr\033[0m, \033[01;37m0.0.0.0\033[0m is the default route> \033[01;37m<genmask> <gateway>\033[0m\r\n" 
#define NVTHELP_ROUTE_USAGE_DEL   "Delete routing table entry, default route cannot be deleted, for example: \033[01;37mroute del <destination addr>\033[0m\r\n"
#define ROUTE_TBL_TITLE "Destination addr      Genmask          Gateway      Interface name\r\n"
static INT route(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    EN_ONPSERR enErr; 
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt)); 

    if (argc == 2 && !strcmp("print", argv[1]))
    {
        tcp_send(pstTelnetClt->hClient, ROUTE_TBL_TITLE, sizeof(ROUTE_TBL_TITLE) - 1); 

        const ST_ROUTE *pstRoute = NULL;
        do {
            CHAR szAddr0[16], szAddr1[16], szAddr2[16]; 
            if (NULL != (pstRoute = route_get_next(pstRoute)))                            
                nvt_printf(pstTelnetClt->hClient, 128, "%*s  %*s  %*s   %s\r\n", 15, pstRoute->unDestination ? inet_ntoa_safe_ext(pstRoute->unDestination, szAddr0) : "*", 15,
                            inet_ntoa_safe_ext(pstRoute->unGenmask, szAddr1), 15, inet_ntoa_safe_ext(pstRoute->unGateway, szAddr2), pstRoute->pstNetif->szName);
        } while (pstRoute);        

        goto __lblEnd;
    }
    else if (argc == 6 && !strcmp("add", argv[1]))
    {
        PST_NETIF pstNetif = netif_get_by_name(argv[2]); 
        if (pstNetif)
        {
            in_addr_t unDestination = inet_addr(argv[3]);
            in_addr_t unGenmask = inet_addr(argv[4]);            
            in_addr_t unGateway = inet_addr(argv[5]); 
            if (route_add(pstNetif, unDestination, unGateway, unGenmask, &enErr))
            {
                if (os_nvt_add_route_entry(argv[2], unDestination, unGenmask, unGateway))
                {
                    tcp_send(pstTelnetClt->hClient, "New routing entry added successfully.\r\n", sizeof("New routing entry added successfully.\r\n") - 1); 
                    goto __lblEnd;
                }
                else
                    enErr = ERRROUTEENTRYNOR;
            }
        }
        else
            enErr = ERRNETIFNOTFOUND; 

        goto __lblErr; 
    }
    else if (argc == 3 && !strcmp("del", argv[1]))
    {
        in_addr_t unDestination = inet_addr(argv[2]); 
        if (unDestination)
        {
            if (route_del(unDestination, &enErr))
            {
                if (os_nvt_del_route_entry(unDestination))
                {
                    nvt_printf(pstTelnetClt->hClient, 128, "Routing entry with destination address of %s deleted successfully.\r\n", argv[2]);
                    goto __lblEnd;
                }
                else
                    enErr = ERRROUTEENTRYNOR;
            }            
        }
        else
        {
            tcp_send(pstTelnetClt->hClient, "The default route cannot be deleted, but you can modify it by adding a routing entry with a destination address of 0.0.0.0.\r\n", 
                        sizeof("The default route cannot be deleted, but you can modify it by adding a routing entry with a destination address of 0.0.0.0.\r\n") - 1); 
            goto __lblEnd;
        }

        goto __lblErr;
    }
    else
    {
        nvt_printf(pstTelnetClt->hClient, 512, "%s%s%s", NVTHELP_ROUTE_USAGE_PRINT, NVTHELP_ROUTE_USAGE_ADD, NVTHELP_ROUTE_USAGE_DEL);
        goto __lblEnd; 
    }

__lblErr:
    nvt_printf(pstTelnetClt->hClient, 256, "The \033[01;37mifip\033[0m command failed, %s.\r\n", onps_error(enErr));

__lblEnd: 
    nvt_cmd_exec_end(ullNvtHandle);

    return 0; 
}
#endif //* #if NVTCMD_ROUTE_EN

#if NETTOOLS_SNTP && NVTCMD_NTP_EN
#define NVTHELP_NTPDATE_USAGE   "Usage as follows:\r\n  \033[01;37mntpdate <ntp server ip addr> [time zone\033[0m, such as GMT+8, enter \033[01;37m8\033[0m (\033[01;37mdefault\033[0m)\033[01;37m]\033[0m\r\n  \033[01;37mntpdate <ntp server domain name> [time zone]\033[0m\r\n" 
#define NVTHELP_NTPDATE_DISDNS  "Due to the fact that the protocol stack configuration item NETTOOLS_DNS_CLIENT is not set, DNS query functionality is not supported. Please use the IP address directly.\r\n"
static INT ntpdate(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt)); 
    if (argc == 2 || argc == 3)
    {
        EN_ONPSERR enErr = ERRNO; 
        CHAR bTimeZone;
        if (argc == 3)
        {
            bTimeZone = atoi(argv[2]);
            if (!(bTimeZone && bTimeZone > -13 && bTimeZone < 13))
                bTimeZone = 8;
        }
        else
            bTimeZone = 8;

        //* 同步时间
        time_t tNtpTime; 
        if (argv[1][0] > 0x29 && argv[1][0] < 0x40) //* ip地址方式        
            tNtpTime = sntp_update_by_ip(argv[1], NULL, os_nvt_set_system_time, bTimeZone, &enErr);                     
        else
        {
    #if NETTOOLS_DNS_CLIENT
            tNtpTime = sntp_update_by_dns(argv[1], NULL, os_nvt_set_system_time, bTimeZone, &enErr);
    #else
            tNtpTime = 0; 
            tcp_send(pstTelnetClt->hClient, NVTHELP_NTPDATE_DISDNS, sizeof(NVTHELP_NTPDATE_DISDNS) - 1);
    #endif
        }

        if (tNtpTime)
        {
            struct tm stTime;
            memcpy(&stTime, localtime((time_t*)&tNtpTime), sizeof(struct tm));
            nvt_printf(pstTelnetClt->hClient, 64, "The time is %d-%02d-%02d %02d:%02d:%02d.\r\n", stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday, stTime.tm_hour, stTime.tm_min, stTime.tm_sec);
        }
        else
        {
            if(ERRNO != enErr)
                nvt_printf(pstTelnetClt->hClient, 256, "Network time synchronization failed, %s.\r\n", onps_error(enErr)); 
        }
    }
    else
        tcp_send(pstTelnetClt->hClient, NVTHELP_NTPDATE_USAGE, sizeof(NVTHELP_NTPDATE_USAGE) - 1);

    nvt_cmd_exec_end(ullNvtHandle);

    return 0;
}
#endif //* #if NETTOOLS_SNTP && NVTCMD_NTP_EN

#if NETTOOLS_DNS_CLIENT && NVTCMD_NSLOOKUP_EN
#define NVTHELP_NSLOOKUP_USAGE   "Usage as follows:\r\n  \033[01;37mnslookup <domain name>\033[0m\r\n" 
static INT nslookup(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt)); 
    if (argc == 2)
    {
        EN_ONPSERR enErr; 
        in_addr_t unPrimaryDNS, unSecondaryDNS;
        INT nDnsClient = dns_client_start(&unPrimaryDNS, &unSecondaryDNS, 3, &enErr); 
        if (nDnsClient >= 0)
        {
            in_addr_t unIp = dns_client_query(nDnsClient, unPrimaryDNS, unSecondaryDNS, argv[1], &enErr);
            if (unIp)
            {
                CHAR szAddr[20];                
                nvt_printf(pstTelnetClt->hClient, 256, "Domain name: \033[01;37m%s\033[0m\r\n    Ip addr: \033[01;37m%s\033[0m\r\n", argv[1], inet_ntoa_safe_ext(unIp, szAddr)); 
            }
            else
                nvt_printf(pstTelnetClt->hClient, 128, "Domain name lookup failed, %s.\r\n", onps_error(enErr)); 

            dns_client_end(nDnsClient);
        }
        else
            nvt_printf(pstTelnetClt->hClient, 128, "Domain name lookup failed, %s.\r\n", onps_error(enErr)); 
    }
    else
        tcp_send(pstTelnetClt->hClient, NVTHELP_NSLOOKUP_USAGE, sizeof(NVTHELP_NSLOOKUP_USAGE) - 1);

    nvt_cmd_exec_end(ullNvtHandle);

    return 0;
}
#endif //* #if NETTOOLS_DNS_CLIENT && NVTCMD_NSLOOKUP_EN

void nvt_output(ULONGLONG ullNvtHandle, UCHAR *pubData, INT nDataLen)
{
    PSTCB_TELNETCLT pstcbTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt));
    if(tcp_send(pstcbTelnetClt->hClient, pubData, nDataLen))
        pstcbTelnetClt->unLastOperateTime = os_get_system_secs();
}

void nvt_outputf(ULONGLONG ullNvtHandle, INT nFormatBufSize, const CHAR *pszInfo, ...)
{
    PSTCB_TELNETCLT pstcbTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt));

    EN_ONPSERR enErr; 
    CHAR *pszFormatBuf = (CHAR *)buddy_alloc(nFormatBufSize, &enErr);
    if (pszFormatBuf)
    {
        va_list pvaArgList;
        va_start(pvaArgList, pszInfo); 
        vsnprintf(pszFormatBuf, nFormatBufSize, pszInfo, pvaArgList);
        va_end(pvaArgList); 

        if(tcp_send(pstcbTelnetClt->hClient, (UCHAR *)pszFormatBuf, strlen(pszFormatBuf)))
            pstcbTelnetClt->unLastOperateTime = os_get_system_secs(); 

        buddy_free(pszFormatBuf); 
    }
    else
        tcp_send(pstcbTelnetClt->hClient, "nvt_outputf() failed, dynamic memory is empty.\r\n", sizeof("nvt_printf() failed, dynamic memory is empty.\r\n") - 1);
}

INT nvt_input(ULONGLONG ullNvtHandle, UCHAR *pubInputBuf, INT nInputBufLen)
{
    PSTCB_NVT pstcbNvt = (PSTCB_NVT)ullNvtHandle;
    CHAR bCpyBytes = 0;

    os_critical_init();
    os_enter_critical();
    {
        if (pstcbNvt->bInputBytes)
        {
            bCpyBytes = pstcbNvt->bInputBytes < nInputBufLen ? pstcbNvt->bInputBytes : nInputBufLen;
            memcpy(pubInputBuf, pstcbNvt->szInputCache, bCpyBytes);
            if (bCpyBytes < pstcbNvt->bInputBytes)
            {
                pstcbNvt->bInputBytes -= bCpyBytes;
                memmove(pstcbNvt->szInputCache, &pstcbNvt->szInputCache[bCpyBytes], pstcbNvt->bInputBytes);
            }
            else
                pstcbNvt->bInputBytes -= bCpyBytes;
        }
    }
    os_exit_critical();

    return (INT)bCpyBytes;
}

void nvt_close(ULONGLONG ullNvtHandle)
{
    PSTCB_TELNETCLT pstTelnetClt = (PSTCB_TELNETCLT)((UCHAR *)ullNvtHandle - offsetof(STCB_TELNETCLT, stcbNvt));     
    close(pstTelnetClt->hClient);
}

const CHAR *nvt_get_term_type(ULONGLONG ullNvtHandle)
{
    PSTCB_NVT pstcbNvt = (PSTCB_NVT)ullNvtHandle;
    return pstcbNvt->stSMach.szTermName;
}
#endif
