/* telnet_srv.h
*
* telnet��������ص�һЩ���Լ��������ݽṹ����
*
* Neo-T, ������2022.05.30 11:56
*
*/
#ifndef TELNETSRV_H
#define TELNETSRV_H

#ifdef SYMBOL_GLOBALS
#define TELNETSRV_EXT
#else
#define TELNETSRV_EXT extern
#endif //* SYMBOL_GLOBALS

#if NETTOOLS_TELNETSRV
#include "net_virtual_terminal.h"

#define TELNETSRV_PORT 23 //* telnet�������˿�
#define TELNETCLT_NUM  6  //* ָ��ϵͳ�����telnet�ͻ�������

#define NVTNUM_MAX       2   //* ָ��nvt������������������ʵ����ָ��telnet��������ͬһʱ�̲������ӵ����������������ֵ�������ܾ�����
#define NVTCMDCACHE_EN   1   //* �Ƿ�֧������棬Ҳ����ͨ�����������л����������ָ��
#define NVTCMDCACHE_SIZE 256 //* ָ��ָ������Ĵ�С

#define TELNETCLT_INACTIVE_TIMEOUT  300 //* ����telnet�ͻ����������������κβ������������ʱ��������������Ͽ���ǰ����

TELNETSRV_EXT BOOL nvt_start(PSTCB_TELNETCLT pstcbTelnetClt);
TELNETSRV_EXT void nvt_stop(PSTCB_TELNETCLT pstcbTelnetClt);
TELNETSRV_EXT void telnet_srv_entry(void);
TELNETSRV_EXT void telnet_srv_end(void);
#endif
#endif
