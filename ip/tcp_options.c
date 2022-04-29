#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#define SYMBOL_GLOBALS
#include "ip/tcp_options.h"
#undef SYMBOL_GLOBALS

//* 系统支持的TCP选项列表，在TCP连接协商时（无论是作为服务器还是客户端）SYN或SYN ACK报文携带如下选项值，
//* 如果要增加支持的选项，直接在该结构体数组增加要支持的选项即可
const static struct {
    EN_TCPOPTTYPE enType; 
    CHAR bLen; 
} lr_staTcpOptList[] =
{
    { TCPOPT_MSS, 4 },          //* 最大报文长度(MSS)
    { TCPOPT_WNDSCALE, 3 },     //* 窗口扩大因子
    { TCPOPT_SACK, 2 },         //* 是否支持SACK
    { TCPOPT_TIMESTAMP, 10 }    //* 时间戳
};

typedef struct _ST_TCPOPT_HANDLER_ {
    UCHAR ubKind; 
    UCHAR ubLen; 
    INT(*pfunPut)(UCHAR ubKind, UCHAR ubLen, UCHAR *pubData); 
    void(*pfunGet)(void); 
} ST_TCPOPT_HANDLER, *PST_TCPOPT_HANDLER;