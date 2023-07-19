/*
* 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
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

//* 在这里定义你自己要添加的nvt指令
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

//* 如果自定义nvt命令的名称长度超过net_virtual_terminal.c文件头部l_bNvtCmdLenMax变量定义的值，请用自定义命令的名称长度取代l_bNvtCmdLenMax变量的原值，以确保“help”命令输出格式的整齐美观
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

    {NULL, "", ""} //* 注意这个不要删除，当所有nvt命令被用户禁止时其被用于避免编译器报错
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

//* 杀死当前正在执行的指令，该函数用于以线程/任务方式启动的指令，当用户退出登录或者长时间没有任何操作，需要主动结束nvt结束运行时，
//* 此时如果存在正在执行的指令，nvt会先主动通知其结束运行，如果规定时间内其依然未结束运行，nvt会调用这个函数强制其结束运行释放占
//* 用的线程/任务资源
void nvt_cmd_kill(void)
{
    
}

//* 以线程方式运行地命令在线程结束时应显式地告知其已结束运行，因为协议栈运行的目标系统属于资源受限系统，凡是以线程运行的nvt命令在
//* 同一时刻只允许运行一个实例，这个函数确保nvt能够安全运行下一个线程实例
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

    
    /* 在这里添加线程/任务启动telnet客户端的代码
      线程/任务入口函数为telnet_clt_entry()
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

    //* 在这里添加目标系统的复位指令

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

    /* 在这里添加线程/任务启动ping探测代码
    线程/任务入口函数为nvt_cmd_ping_entry()
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
