/* os_nvt.h
*
* 网络虚拟终端(Network Virtual Terminal)操作系统层适配接口相关声明
*
* Neo-T, 创建于2022.06.17 18:09
*
*/
#ifndef OS_NVT_H
#define OS_NVT_H

#ifdef SYMBOL_GLOBALS
#define OS_NVT_EXT
#else
#define OS_NVT_EXT extern
#endif //* SYMBOL_GLOBALS

#if NETTOOLS_TELNETSRV
OS_NVT_EXT void os_nvt_init(void);
OS_NVT_EXT void os_nvt_uninit(void);
OS_NVT_EXT BOOL os_nvt_start(void *pvParam);
OS_NVT_EXT void os_nvt_stop(void *pvParam);
#if SUPPORT_ETHERNET
#if ETH_EXTRA_IP_EN
OS_NVT_EXT BOOL os_nvt_add_ip(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask);  //* 将新添加的ip地址写入目标系统的非易失性存储器
OS_NVT_EXT BOOL os_nvt_del_ip(const CHAR *pszIfName, in_addr_t unIp);                          //* 从目标系统的非易失性存储器中删除之前添加的ip地址
#endif //* #if ETH_EXTRA_IP_EN 
#endif //* #if SUPPORT_ETHERNET
#endif

#endif
