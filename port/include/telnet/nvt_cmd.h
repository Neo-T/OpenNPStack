/* nvt_cmd.h
*
* ���������ն�(Network Virtual Terminal)֧�ֵ�ָ����ļ�
*
* Neo-T, ������2022.06.23 17:44
*
*/
#ifndef NVT_CMD_H
#define NVT_CMD_H

#ifdef SYMBOL_GLOBALS
#define NVT_CMD_EXT
#else
#define NVT_CMD_EXT extern
#endif //* SYMBOL_GLOBALS

NVT_CMD_EXT void nvt_cmd_register(void);  
NVT_CMD_EXT void nvt_cmd_kill(void); 

#endif
