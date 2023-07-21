/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#define SYMBOL_GLOBALS
#include "onps.h"
#undef SYMBOL_GLOBALS

HMUTEX o_hMtxPrintf = INVALID_HMUTEX;
BOOL open_npstack_load(EN_ONPSERR *penErr)
{
    //* lcp魔术字生成及tcp/udp端口号动态分配都需要随机数生成函数，所以协议栈启动时先设置个种子，确保栈每次启动时生成不同的随机数
    srand(os_get_system_msecs());

    do {
        if (!buddy_init(penErr))
            break; 

        if (!buf_list_init(penErr))
            break; 

#if SUPPORT_PRINTF && PRINTF_THREAD_MUTEX
        o_hMtxPrintf = os_thread_mutex_init();
        if (INVALID_HMUTEX == o_hMtxPrintf)
        {
            *penErr = ERRMUTEXINITFAILED;
            break;
        }
#endif

        if (!one_shot_timer_init(penErr))
            break; 

        if (!onps_input_init(penErr))
            break; 

        if (!netif_init(penErr))
            break;

        if (!route_table_init(penErr))
            break; 

#if SUPPORT_PPP
        if (!ppp_init(penErr))
            break; 
#endif

#if SUPPORT_ETHERNET
        ethernet_init(); 
#endif

        //* 启动协议栈
        os_thread_onpstack_start(NULL);

        return TRUE; 
    } while (FALSE); 

    netif_uninit(); 
    onps_input_uninit(); 

    if (INVALID_HMUTEX != o_hMtxPrintf)
        os_thread_mutex_uninit(o_hMtxPrintf);

    buf_list_uninit(); 
    buddy_uninit(); 
    one_shot_timer_uninit();     

    return FALSE; 
}

void open_npstack_unload(void)
{ 
#if SUPPORT_PPP
    ppp_uninit();
#endif
    route_table_uninit(); 
    netif_uninit();    
    one_shot_timer_uninit();
    onps_input_uninit();

    if (INVALID_HMUTEX != o_hMtxPrintf)
        os_thread_mutex_uninit(o_hMtxPrintf);

    buf_list_uninit();
    buddy_uninit();    
}


