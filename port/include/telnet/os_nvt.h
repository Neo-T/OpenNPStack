/* os_nvt.h
*
* ���������ն�(Network Virtual Terminal)����ϵͳ������ӿ��������
*
* Neo-T, ������2022.06.17 18:09
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
OS_NVT_EXT BOOL os_nvt_add_ip(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask);  //* ������ӵ�ip��ַд��Ŀ��ϵͳ�ķ���ʧ�Դ洢��
OS_NVT_EXT BOOL os_nvt_del_ip(const CHAR *pszIfName, in_addr_t unIp);                          //* ��Ŀ��ϵͳ�ķ���ʧ�Դ洢����ɾ��֮ǰ��ӵ�ip��ַ
#endif //* #if ETH_EXTRA_IP_EN 
#endif //* #if SUPPORT_ETHERNET
#endif

#endif
