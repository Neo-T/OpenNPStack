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

PPP_UTILS_EXT const CHAR *get_protocol_name(USHORT usProtocol);
PPP_UTILS_EXT const CHAR *get_cpcode_name(EN_CPCODE enCode); 
PPP_UTILS_EXT const CHAR *get_chap_code_Name(EN_CHAPCODE enCode);
PPP_UTILS_EXT USHORT ppp_fcs16(UCHAR *pubData, USHORT usDataLen);
PPP_UTILS_EXT USHORT ppp_escape_encode(UINT unACCM, UCHAR *pubData, USHORT usDataLen, UCHAR *pubDstBuf, USHORT *pusEncodedBytes);
PPP_UTILS_EXT USHORT ppp_escape_decode(UCHAR *pubData, USHORT usDataLen, UCHAR *pubDstBuf, USHORT *pusDecodedBytes);

#endif