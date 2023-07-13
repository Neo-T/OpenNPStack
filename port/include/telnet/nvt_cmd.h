/* nvt_cmd.h
*
* 网络虚拟终端(Network Virtual Terminal)支持的指令定义文件
*
* Neo-T, 创建于2022.06.23 17:44
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
NVT_CMD_EXT void nvt_cmd_thread_end(void); 

#endif
