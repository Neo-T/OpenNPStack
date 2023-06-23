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
#if NETTOOLS_TELNETCLT
#include "net_tools/telnet_client.h"
#endif
#include "net_tools/telnet.h"
#define SYMBOL_GLOBALS
#include "telnet/nvt_cmd.h"
#undef SYMBOL_GLOBALS

//* �����ﶨ�����Լ�Ҫ��ӵ�nvtָ��
//* ===================================================================================
#if NETTOOLS_TELNETCLT
static INT telnet(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle);
#define NVTCMD_NUM 1 //* ������ָ����Ҫ��ӵ�NVTָ�������
#else
#define NVTCMD_NUM 0 //* ������ָ����Ҫ��ӵ�NVTָ�������
#endif

static const ST_NVTCMD l_staNvtCmd[NVTCMD_NUM] = {
#if NETTOOLS_TELNETCLT
    { telnet, "telnet", "used to log in to remote telnet host.\r\n" },
#endif
};

#if NETTOOLS_TELNETCLT
#define NVTCMD_TELNET   0 //* "telnet"ָ����l_staNvtCmd�����еĴ洢����
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

//* ɱ����ǰ����ִ�е�ָ��ú����������߳�/����ʽ������ָ����û��˳���¼���߳�ʱ��û���κβ�������Ҫ��������nvt��������ʱ��
//* ��ʱ�����������ִ�е�ָ�nvt��������֪ͨ��������У�����涨ʱ��������Ȼδ�������У�nvt������������ǿ������������ͷ�ռ
//* �õ��߳�/������Դ
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

    
    /* ����������߳�/��������telnet�ͻ��˵Ĵ���
      �߳�/������ں���Ϊtelnet_clt_entry()
     */

    while (!stArgs.bIsCpyEnd)
        os_sleep_ms(10);

    return 0;
}
#endif //* #if NETTOOLS_TELNETCLT
#endif //* #if NETTOOLS_TELNETSRV
