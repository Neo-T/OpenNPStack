/* net_virtual_terminal.h
*
* ���������ն�(Network Virtual Terminal)ģ����ػ������ݽṹ���궨���ļ�
*
* Neo-T, ������2023.05.31 16:15
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

#define NVT_RCV_BUF_LEN_MAX  64  //* nvt�ն˽��ջ�������С������ʵ��Ӧ�ó����������ջ������Ĵ�С�����������ǿͻ�������Ŀ���ָ̨����󳤶�
#define NVT_ECHO_BUF_LEN_MAX 256 //* ���Ի�������С��������ӦΪNVT_RCV_BUF_LEN_MAX����������Ϊ�������\033[C֮��Ŀ����ַ������Σ�
#define NVT_INPUT_CACHE_SIZE 64  //* nvt�ܹ����ܵ��û����뻺���С����ͬ����Ҫ���ݵ���ָ���������󳤶ȼ������Ԥ��ָ���������е��������ָ����ٶ�С���û������ٶȵĻ���
#define TERM_NAME_MAX        16  //* �ն���������ֽ���
#define NVTCMD_ARGC_MAX      32  //* ָ������Я��������������

#define NVT_USER_NAME    "Neo-T"
#define NVT_USER_PASSWD  "1964101615"
#define NVT_PS           "onps" //* ע��������ֵʱȷ���䳤�Ȳ�Ҫ��nvt_print_login_info()��nvt_print_ps()���������еĸ�ʽ��������szFormatBuf���
//#define NVT_WELCOME_INFO "This is a telnet service provided by onps stack. After logging in, you can enter \r\nthe command \x1b[01;32m\"help\"\x1b[0m to get a detailed list of operational instructions.\r\n"
#define NVT_WELCOME_INFO "This is a telnet service provided by onps stack. In order to provide maximum\r\n" \
                         "compatibility, the server adopts a single-character communication mode. It\r\n" \
                         "supports terminal types such as ANSI, VT100, XTERM, etc. You can enter the\r\n" \
                         "command \033[01;32m\"help\"\033[0m to get a detailed list of operational instructions.\r\n" \
                         " ___  ___  ___  ___\r\n" \
                         "/ _ \\/ _ \\/ _ \\(_-<\r\n" \
                         "\\___/_//_/ .__/___/\r\n" \
                         "        /_/\r\n"                         


//* Telnetѡ��Э��״̬���壬��ȡֵ��Χ���ܳ���stNegoResults::bitXX�ֶε�λ�����ƣ������������ֵ������3����ǰλ��Ϊ2��
typedef enum {
    NNEGO_INIT = 0, 
    NNEGO_SNDREQ = 1, 
    NNEGO_AGREE = 2,
    NNEGO_DISAGREE = 3,
} EN_NVTNEGOSTATE;

//* Network Virtual Terminal
#define NVTNEGOOPT_NUM    3 //* ָ��ҪЭ�̵�ѡ����������ֵ��ΪST_SMACH_NVT::uniFlag::stb32��Э��ѡ���������ͬһѡ����ڶ���ֶζ�����ֶβ���������������ECHO����������ֶ������ﰴһ���㣩
#define NVTNEGORESULT_NUM 4 //* ָ��ҪЭ�̵�ѡ���ֶ���������ֵ��ΪST_SMACH_NVT::uniFlag::stb32��Э��ѡ���ֶε�������ע�ͺ���Э��ѡ��֮���������ֶμ���������
typedef enum { //* �����ֵȡ����ST_SMACHNVT::uniFlag::stb32::bitState�ֶε�λ��
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
            UINT bitTermType  : 2;  //* Э��ѡ��֮�ն�����ѡ��μ�telnet.h�ļ�TELNETOPT_TTYPE���壺λ0����λ��ʾ��Э����ϣ�λ1��λ0��λ��Ч����λ�����壬����λ�������ѡ���֮��ֹ����ͬ���ֶ�ֵ��0��1��3֮�У�2��Ӧ�ó��֣�
            UINT bitSrvSGA    : 2;  //* Э��ѡ��֮����GA�źţ��������ˣ���ͬ�ϣ��μ�TELNETOPT_SGA����                        
            UINT bitSrvEcho   : 2;  //* Э��ѡ��֮����������
            UINT bitCltEcho   : 2;  //* Э��ѡ��֮�ͻ��˻���
            UINT bitTryCnt    : 3;  //* ���Դ��� 
            UINT bitState     : 4;  //* NVT״̬
            UINT bitEntering  : 1;  //* �Ƿ�¼���У���ʵ�����Ƿ�δ�յ�\r\n
            UINT bitCmdExecEn : 1;  //* ָ��ִ��ʹ�� 
            UINT bitReserved  : 15; //* ����            
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

//* NVTָ��
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
    CHAR bCmdIdx; //* Telnet�ͻ���ʹ�á���������������ָ����ʷ��¼�ĵ�ǰ��ȡ����
    CHAR *pszCmdCache;    
#endif
    PST_NVTCMD_NODE pstCmdList; 
    ST_SMACHNVT stSMach;        
} STCB_NVT, *PSTCB_NVT; 

//* TCP�ͻ���
typedef struct _STCB_TELNETCLT_ {     
    SOCKET hClient;         
    STCB_NVT stcbNvt;        
    UINT unLastOperateTime; //* ����Ĳ���ʱ��        
    UCHAR bitTHRunEn  : 1;  //* �߳�������ͣ����λ
    UCHAR bitTHIsEnd  : 1;  //* �߳��Ƿ�ȫ��������     
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
