#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "ip/tcp_link.h"
#include "ip/tcp_frame.h"
#define SYMBOL_GLOBALS
#include "ip/tcp.h"
#undef SYMBOL_GLOBALS

//* 获取指定TCP选项类型的值，参数cType指明要读取哪种类型，pszOption、cOptionLen参数指向要解析
//* 的选项数据及其长度，cOptionLen则是选项数据的长度。返回值为对应选项类型的存储位置，否则返回NULL。
const ST_TCPOPT_HDR lr_staTcpOpts[] =
{
    { 0, 1 },   //* 选项表结束
    { 1, 1 },   //* 无操作
    { 2, 4 },   //* 最大报文长度(MSS)
    { 3, 3 },   //* 窗口扩大因子
    { 4, 2 },   //* 是否支持SACK
    { 8, 10 }   //* 时间戳
};

static INT tcp_send_packet(in_addr_t unSrvAddr, USHORT usSrvPort)
{

}

INT tcp_send_syn(INT nInput, in_addr_t unSrvAddr, USHORT usSrvPort)
{


    return -1; 
}

