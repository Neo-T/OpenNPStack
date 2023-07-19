/*
* ��Ȩ����onpsջ�����Ŷӣ���ѭApache License 2.0��Դ���Э��
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

#if NETTOOLS_TELNETSRV
#include "net_tools/net_virtual_terminal.h"
#if NVTCMD_TELNET_EN
#include "net_tools/telnet_client.h"
#endif //* #if NVTCMD_TELNET_EN
#include "net_tools/telnet.h"
#define SYMBOL_GLOBALS
#include "telnet/nvt_cmd.h"
#undef SYMBOL_GLOBALS

//* �����ﶨ�����Լ�Ҫ��ӵ�nvtָ��
//* ===================================================================================
#if NVTCMD_TELNET_EN
static INT telnet(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle); 
#endif //* #if NVTCMD_TELNET_EN

#if NETTOOLS_PING && NVTCMD_PING_EN
#include "net_tools/ping.h"
static INT nvt_cmd_ping(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle); 
#endif //* #if NETTOOLS_PING && NVTCMD_PING_EN

#if NVTCMD_RESET_EN
static INT reset(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle);
#endif //* #if NVTCMD_RESET_EN

//* ����Զ���nvt��������Ƴ��ȳ���net_virtual_terminal.c�ļ�ͷ��l_bNvtCmdLenMax���������ֵ�������Զ�����������Ƴ���ȡ��l_bNvtCmdLenMax������ԭֵ����ȷ����help�����������ʽ����������
static const ST_NVTCMD l_staNvtCmd[] = {    
#if NVTCMD_TELNET_EN
    { telnet, "telnet", "used to log in to remote telnet host.\r\n" },     
#endif //* #if NVTCMD_TELNET_EN

#if NETTOOLS_PING && NVTCMD_PING_EN
    { nvt_cmd_ping, "ping", "A lightweight ping testing tool that supports IPv4 and IPv6 address probing.\r\n" },
#endif //* #if NETTOOLS_PING && NVTCMD_PING_EN

#if NVTCMD_RESET_EN
    { reset, "reset", "system reset.\r\n" }, 
#endif //* #if NVTCMD_RESET_EN

    {NULL, "", ""} //* ע�������Ҫɾ����������nvt����û���ֹʱ�䱻���ڱ������������
};

static ST_NVTCMD_NODE l_staNvtCmdNode[sizeof(l_staNvtCmd) / sizeof(ST_NVTCMD)]; 
//* ===================================================================================

void nvt_cmd_register(void)
{
    UCHAR i;
    for (i = 0; i < sizeof(l_staNvtCmd) / sizeof(ST_NVTCMD); i++)
    {
        if(l_staNvtCmd[i].pfun_cmd_entry)
            nvt_cmd_add(&l_staNvtCmdNode[i], &l_staNvtCmd[i]); 
    }
}

//* ɱ����ǰ����ִ�е�ָ��ú����������߳�/����ʽ������ָ����û��˳���¼���߳�ʱ��û���κβ�������Ҫ��������nvt��������ʱ��
//* ��ʱ�����������ִ�е�ָ�nvt��������֪ͨ��������У�����涨ʱ��������Ȼδ�������У�nvt������������ǿ������������ͷ�ռ
//* �õ��߳�/������Դ
void nvt_cmd_kill(void)
{
    
}

//* ���̷߳�ʽ���е��������߳̽���ʱӦ��ʽ�ظ�֪���ѽ������У���ΪЭ��ջ���е�Ŀ��ϵͳ������Դ����ϵͳ���������߳����е�nvt������
//* ͬһʱ��ֻ��������һ��ʵ�����������ȷ��nvt�ܹ���ȫ������һ���߳�ʵ��
void nvt_cmd_thread_end(void)
{

}

#if NVTCMD_TELNET_EN
#define NVTHELP_TELNET_USAGE       "Please enter the telnet server address, usage as follows:\r\n \033[01;37mtelnet xxx.xxx.xxx.xxx [port]\033[0m\r\n"
#define NVTHELP_TELNET_LOGIN_LOCAL "Due to resource constraints, the telnet command is prohibited from logging in to its own server.\r\n"
static INT telnet(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    ST_TELCLT_STARTARGS stArgs;

    if (argc != 2 && argc != 3)
    {
        nvt_output(ullNvtHandle, NVTHELP_TELNET_USAGE, sizeof(NVTHELP_TELNET_USAGE) - 1);
        nvt_cmd_exec_end(ullNvtHandle);
        return -1;
    }

    if (is_local_ip(inet_addr_small(argv[1])))
    {
        nvt_output(ullNvtHandle, NVTHELP_TELNET_LOGIN_LOCAL, sizeof(NVTHELP_TELNET_LOGIN_LOCAL) - 1);
        nvt_cmd_exec_end(ullNvtHandle);
        return -1;
    }

    stArgs.bIsCpyEnd = FALSE;
    stArgs.ullNvtHandle = ullNvtHandle;
    stArgs.stSrvAddr.saddr_ipv4 = inet_addr_small(argv[1]);
    if (argc == 3)
        stArgs.stSrvAddr.usPort = atoi(argv[2]);
    else
        stArgs.stSrvAddr.usPort = 23;

    
    /* ����������߳�/��������telnet�ͻ��˵Ĵ���
      �߳�/������ں���Ϊtelnet_clt_entry()
     */

    while (!stArgs.bIsCpyEnd)
        os_sleep_ms(10);

    return 0;
}
#endif //* #if NVTCMD_TELNET_EN

#if NVTCMD_RESET_EN
static INT reset(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    nvt_output(ullNvtHandle, "The system will be reset ...", sizeof("The system will be reset ...") - 1); 
    os_sleep_secs(3); 
    nvt_close(ullNvtHandle); 

    //* ���������Ŀ��ϵͳ�ĸ�λָ��

    return 0; 
}
#endif //* #if NVTCMD_RESET_EN

#if NETTOOLS_PING && NVTCMD_PING_EN
#if SUPPORT_IPV6
#define NVTHELP_PING_USAGE "Usage as follows:\r\n  \033[01;37mping [4] xxx.xxx.xxx.xxx\033[0m\r\n  \033[01;37mping 6 xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx\033[0m\r\n"
#else //* #if SUPPORT_IPV6
#define NVTHELP_PING_USAGE "Usage as follows:\r\n  \033[01;37mping xxx.xxx.xxx.xxx\033[0m\r\n"
#endif //* #if SUPPORT_IPV6
static INT nvt_cmd_ping(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    ST_PING_STARTARGS stArgs; 
#if SUPPORT_IPV6
    if (argc != 2 && argc != 3)
#else
    if (argc != 2)
#endif
        goto __lblHelp; 
    else
    {         
#if SUPPORT_IPV6
        if (argc == 3)
        {
            if (strlen(argv[1]) == 1)
            {
                if ('4' == argv[1][0])                
                    stArgs.nFamily = AF_INET;                                  
                else if('6' == argv[1][0])
                    stArgs.nFamily = AF_INET6;
                else
                    goto __lblHelp; 

                snprintf(stArgs.szDstIp, sizeof(stArgs.szDstIp), "%s", argv[2]); 
            }
            else            
                goto __lblHelp; 
        }   
        else
        {
            stArgs.nFamily = AF_INET;
            snprintf(stArgs.szDstIp, sizeof(stArgs.szDstIp), "%s", argv[1]);
        }
#else
        stArgs.nFamily = AF_INET; 
        snprintf(stArgs.szDstIp, sizeof(stArgs.szDstIp), "%s", argv[1]); 
#endif
    }

    stArgs.bIsCpyEnd = FALSE;
    stArgs.ullNvtHandle = ullNvtHandle; 

    /* ����������߳�/��������ping̽�����
    �߳�/������ں���Ϊnvt_cmd_ping_entry()
    */

    while (!stArgs.bIsCpyEnd)
        os_sleep_ms(10);

    return 0; 

__lblHelp: 
    nvt_output(ullNvtHandle, NVTHELP_PING_USAGE, sizeof(NVTHELP_PING_USAGE) - 1);
    nvt_cmd_exec_end(ullNvtHandle);
    return -1;
}
#endif //* #if NETTOOLS_PING && NVTCMD_PING_EN
#endif //* #if NETTOOLS_TELNETSRV
