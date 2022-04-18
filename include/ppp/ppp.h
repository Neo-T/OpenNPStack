/* ppp.h
 *
 * 完成ppp模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.03.21 10:19
 * 版本: 1.0
 *
 */
#ifndef PPP_H
#define PPP_H

#ifdef SYMBOL_GLOBALS
	#define PPP_EXT
#else
	#define PPP_EXT extern
#endif //* SYMBOL_GLOBALS

typedef struct _ST_DIAL_AUTH_INFO_ {
	const CHAR *pszAPN;			//* 指定拨号脚本用到的APN
	const CHAR *pszUser;		//* ppp认证用户名
	const CHAR *pszPassword;	//* ppp认证密码
} ST_DIAL_AUTH_INFO, *PST_DIAL_AUTH_INFO;

PPP_EXT BOOL ppp_init(EN_ONPSERR *penErr);
PPP_EXT void ppp_uninit(void); 
PPP_EXT void thread_ppp_handler(void *pvParam); 
PPP_EXT const CHAR *get_ppp_port_name(HTTY hTTY);
PPP_EXT const ST_DIAL_AUTH_INFO *get_ppp_dial_auth_info(HTTY hTTY);
PPP_EXT INT get_ppp_index(HTTY hTTY);
PPP_EXT void get_ppp_auth_info(HTTY hTTY, const CHAR **pszUser, const CHAR **pszPassword);
PPP_EXT INT ppp_send(HTTY hTTY, EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ONPSERR *penErr);
PPP_EXT void ppp_link_terminate(INT nPPPIdx);	//* 参数nPPPIdx指定ppp链路索引，也就是说指定终止哪一路ppp链路，索引值对应ppp.c中lr_pszaTTY数组定义的tty口建立的ppp链路
PPP_EXT void ppp_link_recreate(INT nPPPIdx);	//* 参数nPPPIdx的含义同上，只有链路当前处于TERMINATED状态才会触发重建操作，其它状态不做任何处理

#endif
