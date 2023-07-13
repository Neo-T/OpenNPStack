/* net_virtual_terminal.h
*
* 网络虚拟终端(Network Virtual Terminal)模块相关基础数据结构及宏定义文件
*
* Neo-T, 创建于2023.05.31 16:15
*
*/
#ifndef NVT_H
#define NVT_H

#ifdef SYMBOL_GLOBALS
#define NVT_EXT
#else
#define NVT_EXT extern
#endif //* SYMBOL_GLOBALS

#include "bsd/socket.h" 

#define NVT_RCV_BUF_LEN_MAX  64  //* nvt终端接收缓冲区大小，根据实际应用场景调整接收缓冲区的大小，调整依据是客户端输入的控制台指令最大长度
#define NVT_ECHO_BUF_LEN_MAX 256 //* 回显缓冲区大小，其至少应为NVT_RCV_BUF_LEN_MAX的三倍（因为存在填充\033[C之类的控制字符的情形）
#define NVT_INPUT_CACHE_SIZE 64  //* nvt能够接受的用户输入缓存大小，其同样需要根据单个指令输入的最大长度及允许的预存指令数量进行调整（如果指令处理速度小于用户输入速度的话）
#define TERM_NAME_MAX        16  //* 终端名称最大字节数
#define NVTCMD_ARGC_MAX      32  //* 指令允许携带的最大参数数量

#define NVT_USER_NAME    "Neo-T"
#define NVT_USER_PASSWD  "1964101615"
#define NVT_PS           "onps" //* 注意调整这个值时确保其长度不要让nvt_print_login_info()、nvt_print_ps()两个函数中的格式化缓冲区szFormatBuf溢出
//#define NVT_WELCOME_INFO "This is a telnet service provided by onps stack. After logging in, you can enter \r\nthe command \x1b[01;32m\"help\"\x1b[0m to get a detailed list of operational instructions.\r\n"
#define NVT_WELCOME_INFO "This is a telnet service provided by onps stack. In order to provide maximum\r\n" \
                         "compatibility, the server adopts a single-character communication mode. It\r\n" \
                         "supports terminal types such as ANSI, VT100, XTERM, etc. You can enter the\r\n" \
                         "command \033[01;32m\"help\"\033[0m to get a detailed list of operational instructions.\r\n" \
                         " ___  ___  ___  ___\r\n" \
                         "/ _ \\/ _ \\/ _ \\(_-<\r\n" \
                         "\\___/_//_/ .__/___/\r\n" \
                         "        /_/\r\n"                         


//* Telnet选项协商状态定义，其取值范围不能超过stNegoResults::bitXX字段的位宽限制，在这里就是其值最大就是3（当前位宽为2）
typedef enum {
    NNEGO_INIT = 0, 
    NNEGO_SNDREQ = 1, 
    NNEGO_AGREE = 2,
    NNEGO_DISAGREE = 3,
} EN_NVTNEGOSTATE;

//* Network Virtual Terminal
#define NVTNEGOOPT_NUM    3 //* 指定要协商的选项数量，其值即为ST_SMACH_NVT::uniFlag::stb32中协商选项的数量（同一选项存在多个字段多出的字段不计入总数，比如ECHO项存在两个字段在这里按一个算）
#define NVTNEGORESULT_NUM 4 //* 指定要协商的选项字段数量，其值即为ST_SMACH_NVT::uniFlag::stb32中协商选项字段的数量（注释含“协商选项之……”的字段计入总数）
typedef enum { //* 其最大值取决于ST_SMACHNVT::uniFlag::stb32::bitState字段的位宽
    SMACHNVT_NEGO = 0, 
    SMACHNVT_GETTERMNAME = 1, 
    SMACHNVT_LOGIN = 2, 
    SMACHNVT_PASSWD = 3,
    SMACHNVT_INTERACTIVE = 4, 
    SMACHNVT_CMDEXECING = 5, 
    SMACHNVT_CMDEXECEND = 6 
} EN_NVTSTATE; 
typedef struct _ST_SMACHNVT_ {
    union {
        struct {
            UINT bitTermType  : 2;  //* 协商选项之终端类型选项，参见telnet.h文件TELNETOPT_TTYPE定义：位0，置位表示已协商完毕；位1，位0置位有效，复位无意义，其置位代表激活该选项，反之禁止，下同（字段值在0、1、3之中，2不应该出现）
            UINT bitSrvSGA    : 2;  //* 协商选项之抑制GA信号（服务器端），同上，参见TELNETOPT_SGA定义                        
            UINT bitSrvEcho   : 2;  //* 协商选项之服务器回显
            UINT bitCltEcho   : 2;  //* 协商选项之客户端回显
            UINT bitTryCnt    : 3;  //* 重试次数 
            UINT bitState     : 4;  //* NVT状态
            UINT bitEntering  : 1;  //* 是否录入中，其实就是是否还未收到\r\n
            UINT bitCmdExecEn : 1;  //* 指令执行使能 
            UINT bitReserved  : 15; //* 保留            
        } stb32;
        UINT unVal;
    } uniFlag;
    CHAR szTermName[TERM_NAME_MAX]; 
    UCHAR ubLastAckOption; 
} ST_SMACHNVT, *PST_SMACHNVT; 
#define nvt_state uniFlag.stb32.bitState
#define nvt_cmd_exec_en uniFlag.stb32.bitCmdExecEn
#define nvt_try_cnt uniFlag.stb32.bitTryCnt
#define nvt_term_type uniFlag.stb32.bitTermType
#define nvt_srv_sga uniFlag.stb32.bitSrvSGA
#define nvt_srv_echo uniFlag.stb32.bitSrvEcho
#define nvt_clt_echo uniFlag.stb32.bitCltEcho
#define nvt_entering uniFlag.stb32.bitEntering

//* NVT指令
typedef struct _ST_NVTCMD_ {
    INT(*pfun_cmd_entry)(CHAR argc, CHAR* argv[], ULONGLONG ullNvtHandle);
    CHAR *pszCmdName;
    CHAR *pszReadme; 
} ST_NVTCMD, *PST_NVTCMD;
typedef struct _ST_NVTCMD_NODE_ {
    const ST_NVTCMD *pstNvtCmd; 
    struct _ST_NVTCMD_NODE_ *pstNextCmd; 
} ST_NVTCMD_NODE, *PST_NVTCMD_NODE;

typedef struct _STCB_NVT_ {
    CHAR szInputCache[NVT_INPUT_CACHE_SIZE]; 
    CHAR bInputBytes; 
    CHAR bCursorPos; 
#if NVTCMDCACHE_EN
    CHAR bCmdNum; 
    CHAR bCmdIdx; //* Telnet客户端使用“↑↓”查阅输入指令历史记录的当前读取索引
    CHAR *pszCmdCache;    
#endif
    PST_NVTCMD_NODE pstCmdList; 
    ST_SMACHNVT stSMach;        
} STCB_NVT, *PSTCB_NVT; 

//* TCP客户端
typedef struct _STCB_TELNETCLT_ {     
    SOCKET hClient;         
    STCB_NVT stcbNvt;        
    UINT unLastOperateTime; //* 最近的操作时间        
    UCHAR bitTHRunEn  : 1;  //* 线程运行启停控制位
    UCHAR bitTHIsEnd  : 1;  //* 线程是否安全结束运行     
    UCHAR bitReserved : 6;
    struct _STCB_TELNETCLT_ *pstcbNext; 
} STCB_TELNETCLT, *PSTCB_TELNETCLT; 

NVT_EXT void thread_nvt_handler(void *pvParam); 
NVT_EXT void nvt_embedded_cmd_loader(void); 
NVT_EXT void nvt_cmd_add(PST_NVTCMD_NODE pstCmdNode, const ST_NVTCMD *pstCmd); 
NVT_EXT void nvt_cmd_exec_end(ULONGLONG ullNvtHandle);
NVT_EXT BOOL nvt_cmd_exec_enable(ULONGLONG ullNvtHandle);
NVT_EXT void nvt_output(ULONGLONG ullNvtHandle, UCHAR *pubData, INT nDataLen);
NVT_EXT void nvt_outputf(ULONGLONG ullNvtHandle, INT nFormatBufSize, const CHAR *pszInfo, ...);
NVT_EXT INT nvt_input(ULONGLONG ullNvtHandle, UCHAR *pubInputBuf, INT nInputBufLen);
NVT_EXT const CHAR *nvt_get_term_type(ULONGLONG ullNvtHandle);

#endif
