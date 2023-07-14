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

#if SUPPORT_ETHERNET && NVTCMD_IFIP_EN
#if ETH_EXTRA_IP_EN
OS_NVT_EXT BOOL os_nvt_add_ip(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask);  //* ������ӵ�ip��ַд��Ŀ��ϵͳ�ķ���ʧ�Դ洢��
OS_NVT_EXT BOOL os_nvt_del_ip(const CHAR *pszIfName, in_addr_t unIp);                          //* ��Ŀ��ϵͳ�ķ���ʧ�Դ洢����ɾ��֮ǰ��ӵ�ip��ַ
#endif //* #if ETH_EXTRA_IP_EN 
OS_NVT_EXT BOOL os_nvt_set_ip(const CHAR *pszIfName, in_addr_t unIp, in_addr_t unSubnetMask, in_addr_t unGateway); 
OS_NVT_EXT BOOL os_nvt_set_mac(const CHAR *pszIfName, const CHAR *pszMac);
OS_NVT_EXT BOOL os_nvt_set_dns(const CHAR *pszIfName, in_addr_t unPrimaryDns, in_addr_t unSecondaryDns); 
OS_NVT_EXT BOOL os_nvt_set_dhcp(const CHAR *pszIfName);   
OS_NVT_EXT void os_nvt_system_reset(void);  
#endif //* #if SUPPORT_ETHERNET && NVTCMD_IFIP_EN

#if NVTCMD_ROUTE_EN
OS_NVT_EXT BOOL os_nvt_add_route_entry(const CHAR *pszIfName, in_addr_t unDestination, in_addr_t unGenmask, in_addr_t unGateway); 
OS_NVT_EXT BOOL os_nvt_del_route_entry(in_addr_t unDestination); 
#endif //* #if NVTCMD_ROUTE_EN

#if NETTOOLS_PING && NVTCMD_PING_EN
OS_NVT_EXT UINT os_get_elapsed_millisecs(void); 
#endif //* #if NETTOOLS_PING && NVTCMD_PING_EN
#endif //* #if NETTOOLS_TELNETSRV

#endif
