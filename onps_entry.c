#define SYMBOL_GLOBALS
#include "onps.h"
#undef SYMBOL_GLOBALS

BOOL open_npstack_load(EN_ERROR_CODE *penErrCode)
{
    do {
        if (!buddy_init(penErrCode))
            break; 

        if (!buf_list_init(penErrCode))
            break; 

        if (!one_shot_timer_init(penErrCode))
            break; 

        if (!netif_init(penErrCode))
            break; 

#if SUPPORT_PPP
        if (!ppp_init(penErrCode))
            break; 
#endif

        //* 启动协议栈
        os_thread_onpstack_start(NULL);

        return TRUE; 
    } while (FALSE); 

    netif_uninit(); 
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
    netif_uninit();
    one_shot_timer_uninit();
    buf_list_uninit();
    buddy_uninit();    
}


