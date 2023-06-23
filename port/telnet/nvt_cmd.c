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
#if NETTOOLS_TELNETCLT
#include "net_tools/telnet_client.h"
#endif
#include "net_tools/telnet.h"
#define SYMBOL_GLOBALS
#include "telnet/nvt_cmd.h"
#undef SYMBOL_GLOBALS

//* 在这里定义你自己要添加的nvt指令
//* ===================================================================================
#if NETTOOLS_TELNETCLT
static INT telnet(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle);
#define NVTCMD_NUM 1 //* 在这里指定你要添加的NVT指令的数量
#else
#define NVTCMD_NUM 0 //* 在这里指定你要添加的NVT指令的数量
#endif

static const ST_NVTCMD l_staNvtCmd[NVTCMD_NUM] = {
#if NETTOOLS_TELNETCLT
    { telnet, "telnet", "used to log in to remote telnet host.\r\n" },
#endif
};

#if NETTOOLS_TELNETCLT
#define NVTCMD_TELNET   0 //* "telnet"指令在l_staNvtCmd数组中的存储索引
#endif
static ST_NVTCMD_NODE l_staNvtCmdNode[NVTCMD_NUM];
//* ===================================================================================

void nvt_cmd_register(void)
{
    UCHAR i;
    for (i = 0; i < NVTCMD_NUM; i++)
    {
        nvt_cmd_add(&l_staNvtCmdNode[i], (const PST_NVTCMD)&l_staNvtCmd[i]);
    }
}

//* 杀死当前正在执行的指令，该函数用于以线程/任务方式启动的指令，当用户退出登录或者长时间没有任何操作，需要主动结束nvt结束运行时，
//* 此时如果存在正在执行的指令，nvt会先主动通知其结束运行，如果规定时间内其依然未结束运行，nvt会调用这个函数强制其结束运行释放占
//* 用的线程/任务资源
void nvt_cmd_kill(void)
{
    
}

#if NETTOOLS_TELNETCLT
static INT telnet(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle)
{
    ST_TELCLT_STARTARGS stArgs;

    if (argc != 2 && argc != 3)
    {
        nvt_output(ullNvtHandle, (const UCHAR *)"Please enter the telnet server address, usage as follows:\r\n telnet xxx.xxx.xxx.xxx [port]\r\n",
            sizeof("Please enter the telnet server address, usage as follows:\r\n telnet xxx.xxx.xxx.xxx [port]\r\n") - 1);
        nvt_cmd_exec_end(ullNvtHandle);
        return -1;
    }

    stArgs.bIsCpyEnd = FALSE;
    stArgs.ullNvtHandle = ullNvtHandle;
    stArgs.stSrvAddr.saddr_ipv4 = inet_addr(argv[1]);
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
#endif //* #if NETTOOLS_TELNETCLT
#endif //* #if NETTOOLS_TELNETSRV
