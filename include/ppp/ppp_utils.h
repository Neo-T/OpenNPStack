/* ppp_utils.h
 *
 * 完成ppp模块相关宏定义、接口函数、结构体定义等工作
 *
 * Neo-T, 创建于2022.03.21 10:19
 * 版本: 1.0
 *
 */
#ifndef PPP_UTILS_H
#define PPP_UTILS_H

#ifdef SYMBOL_GLOBALS
	#define PPP_UTILS_EXT
#else
	#define PPP_UTILS_EXT extern
#endif //* SYMBOL_GLOBALS
#include "one_shot_timer.h"
#include "ppp_frame.h"

typedef struct _ST_PPPWAITACKNODE_ { //* 等待应答的PPP报文链表节点
	UCHAR ubIsUsed;	
	struct {
		USHORT usProtocol;
		UCHAR ubCode;
		UCHAR ubIdentifier;
	} stPacket;	
	PST_ONESHOTTIMER pstTimer;
} ST_PPPWAITACKNODE, *PST_PPPWAITACKNODE;

PPP_UTILS_EXT const CHAR *get_protocol_name(USHORT usProtocol);
PPP_UTILS_EXT const CHAR *get_cpcode_name(EN_CPCODE enCode); 
PPP_UTILS_EXT const CHAR *get_chap_code_name(EN_CHAPCODE enCode);
PPP_UTILS_EXT USHORT ppp_fcs16(UCHAR *pubData, USHORT usDataLen);
PPP_UTILS_EXT USHORT ppp_fcs16_ext(SHORT sBufListHead); 
PPP_UTILS_EXT UINT ppp_escape_encode(UINT unACCM, UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punEncodedBytes);
PPP_UTILS_EXT void ppp_escape_encode_init(UINT unACCM, UCHAR ubaACCM[]); 
PPP_UTILS_EXT UINT ppp_escape_encode_ext(UCHAR ubaACCM[], UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punEncodedBytes); 
PPP_UTILS_EXT UINT ppp_escape_decode(UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punDecodedBytes);
PPP_UTILS_EXT UINT ppp_escape_decode_ext(UCHAR *pubData, UINT unStartIdx, UINT unEndIdx, UINT unDataBufSize, UCHAR *pubDstBuf, UINT *punDecodedBytes);
PPP_UTILS_EXT BOOL ppp_wait_ack_node_new(PFUN_ONESHOTTIMEOUT_HANDLER pfunTimeoutHandler, USHORT usProtocol, UCHAR ubCode, UCHAR ubIdentifier, INT nTimerCount, EN_ERROR_CODE *penErrCode);

#endif