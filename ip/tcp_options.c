#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "ip/tcp_frame.h"
#define SYMBOL_GLOBALS
#include "ip/tcp_options.h"
#undef SYMBOL_GLOBALS

//* 系统支持的TCP选项列表，在TCP连接协商时（无论是作为服务器还是客户端）SYN或SYN ACK报文携带如下选项值，
//* 如果要增加支持的选项，直接在该结构体数组增加要支持的选项即可
static void tcp_options_attach_mss(UCHAR *pubAttachAddr); 
static void tcp_options_attach_wndscale(UCHAR *pubAttachAddr); 
typedef struct _ST_TCPOPT_ATTACH_HANDLER_ {
    EN_TCPOPTTYPE enType;
    UCHAR ubLen;
    void(*pfunAttach)(UCHAR *pubAttachAddr);
} ST_TCPOPT_ATTACH_HANDLER, *PST_TCPOPT_ATTACH_HANDLER;
const static ST_TCPOPT_ATTACH_HANDLER lr_staTcpOptList[] =
{
    { TCPOPT_MSS, (UCHAR)sizeof(ST_TCPOPT_MSS), tcp_options_attach_mss },       //* 最大报文长度(MSS)
    { TCPOPT_WNDSCALE, (UCHAR)sizeof(ST_TCPOPT_WNDSCALE), tcp_options_attach_wndscale },               //* 窗口扩大因子
    { TCPOPT_SACK, (UCHAR)sizeof(ST_TCPOPT_HDR), NULL }, //* 是否支持SACK
};

static void tcp_options_attach_mss(UCHAR *pubAttachAddr)
{
    USHORT usMss = TCPRCVBUF_SIZE_DEFAULT - TCP_HDR_SIZE_MAX; 
    pubAttachAddr[0] = ((UCHAR *)&usMss)[1]; 
    pubAttachAddr[1] = ((UCHAR *)&usMss)[0];    
}

static void tcp_options_attach_wndscale(UCHAR *pubAttachAddr)
{
    *((CHAR *)pubAttachAddr) = TCP_WINDOW_SCALE; 
}

INT tcp_options_attach(UCHAR *pubAttachAddr, INT nAttachBufSize)
{
    INT i, nHasAttachBytes = 0; 
    UCHAR ubNopNum = 0; 
    PST_TCPOPT_HDR pstOptHdr;
    for (i = 0; i < (INT)(sizeof(lr_staTcpOptList) / sizeof(ST_TCPOPT_ATTACH_HANDLER)); i++)
    {
        if (i)
        {
            //* 要确保tcp选项为4字节对齐，不对齐时填充对应字节数的nop字符以强制对齐
            ubNopNum = lr_staTcpOptList[i].ubLen % 4 ? 4 - (lr_staTcpOptList[i].ubLen % 4) : 0;
        }                
        if (nHasAttachBytes + (INT)lr_staTcpOptList[i].ubLen + ubNopNum > nAttachBufSize)
            return nHasAttachBytes; 

        //* 先填充nop字符以强制4字节对齐
        if (ubNopNum)
        {
            UCHAR k = 0;
            UCHAR *pubNop = pubAttachAddr + nHasAttachBytes;
            for (; k < ubNopNum; k++)
                *(pubNop + k) = TCPOPT_NOP;
            nHasAttachBytes += (INT)ubNopNum;
        }

        //* 附着协议栈支持的tcp选项
        pstOptHdr = (PST_TCPOPT_HDR)(pubAttachAddr + nHasAttachBytes); 
        pstOptHdr->ubKind = lr_staTcpOptList[i].enType; 
        pstOptHdr->ubLen = lr_staTcpOptList[i].ubLen; 
        if (lr_staTcpOptList[i].pfunAttach)
            lr_staTcpOptList[i].pfunAttach(((UCHAR *)pstOptHdr) + sizeof(ST_TCPOPT_HDR));         

        nHasAttachBytes += (INT)lr_staTcpOptList[i].ubLen; 
    }

    return nHasAttachBytes; 
}

void tcp_options_get(UCHAR *pubOption, INT nOptionLen)
{

}
