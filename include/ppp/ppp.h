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

PPP_EXT BOOL ppp_init(EN_ERROR_CODE *penErrCode);
PPP_EXT void ppp_uninit(void); 
PPP_EXT void thread_ppp_handler(void *pvParam); 
PPP_EXT const CHAR *get_ppp_port_name(HTTY hTTY);
PPP_EXT const PST_DIAL_AUTH_INFO get_ppp_dial_auth_info(HTTY hTTY);
PPP_EXT INT ppp_send(HTTY hTTY, EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ERROR_CODE *penErrCode);

#endif