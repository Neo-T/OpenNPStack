#define SYMBOL_GLOBALS
#include "onps.h"
#undef SYMBOL_GLOBALS

BOOL open_npstack_load(EN_ONPSERR *penErr)
{
    do {
        if (!buddy_init(penErr))
            break; 

        if (!buf_list_init(penErr))
            break; 

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

        //* 启动协议栈
        os_thread_onpstack_start(NULL);

        return TRUE; 
    } while (FALSE); 

    netif_uninit(); 
    onps_input_uninit(); 
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
    buf_list_uninit();
    buddy_uninit();    
}


