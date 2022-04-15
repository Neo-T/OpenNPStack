#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "utils.h"
#define SYMBOL_GLOBALS
#include "ip/icmp.h"
#undef SYMBOL_GLOBALS
#include "ip/icmp_frame.h"

INT icmp_send(UINT unDstAddr, EN_ICMPTYPE enType, UCHAR ubCode, const UCHAR *pubData, UINT unDataSize, EN_ERROR_CODE *penErrCode)
{
    //* 申请一个buf list节点
    SHORT sBufListHead = -1;
    SHORT sDataNode = buf_list_get_ext(pubData, (USHORT)unDataSize, penErrCode);
    if (sDataNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sDataNode);

    //* 填充头部字段
    ST_ICMP_HDR stHdr; 
    stHdr.ubType = (UCHAR)enType; 
    stHdr.ubCode = ubCode;   
    stHdr.usChecksum = 0;  //* 该字段必须为0，因为校验和计算范围覆盖该字段，其初始值必须为0才可
    SHORT sHdrNode;
    sHdrNode = buf_list_get_ext((UCHAR *)&stHdr, (USHORT)sizeof(ST_ICMP_HDR), penErrCode);
    if (sHdrNode < 0)
        return -1;
    buf_list_put_head(&sBufListHead, sHdrNode);

    //* 计算校验和
    stHdr.usChecksum = tcpip_checksum_ext(sBufListHead); 

    //* 完成发送
    INT nRtnVal = ip_send(unDstAddr, sBufListHead, penErrCode); 

    //* 释放刚才申请的buf list节点
    buf_list_free(sDataNode);
    buf_list_free(sHdrNode);
}