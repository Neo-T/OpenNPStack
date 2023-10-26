/*
 * 作者：Neo-T，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/ppp_utils.h"
#undef SYMBOL_GLOBALS

const CHAR *get_protocol_name(USHORT usProtocol)
{
	switch (usProtocol)
	{
	case PPP_LCP:
		return "LCP";

	case PPP_CCP:
		return "CCP";

	case PPP_PAP:
		return "PAP";

	case PPP_CHAP:
		return "CHAP";

	case PPP_IPCP:
		return "IPCP";

	case PPP_IP:
		return "IP";

	case PPP_IPV6:
		return "IPv6";

	case PPP_LQR:
		return "LQR";

	default:
		return "Unrecognized";
	}
}

const CHAR *get_cpcode_name(EN_CPCODE enCode)
{
	switch (enCode)
	{
	case CONFREQ:
		return "Configure Request";

	case CONFACK:
		return "Configure Ack";

	case CONFNAK:
		return "Configure Nak";

	case CONFREJ:
		return "Configure Reject";

	case TERMREQ:
		return "Terminate Request";

	case TERMACK:
		return "Terminate Ack";

	case CODEREJ:
		return "Code Reject";

	case PROTREJ:
		return "Protocol Reject";

	case ECHOREQ:
		return "Echo Request";

	case ECHOREP:
		return "Echo Reply";

	case DISCREQ:
		return "Discard Request";

	default:
		return "Unrecognized";
	}
}

const CHAR *get_chap_code_name(EN_CHAPCODE enCode)
{
	switch (enCode)
	{
	case CHALLENGE:
		return "Challenge";

	case RESPONSE:
		return "Response";

	case SUCCEEDED:
		return "Success";

	case FAILURE:
		return "Failure";

	default:
		return "Unrecognized";
	}
}

const CHAR *get_pap_code_name(EN_PAPCODE enCode)
{
	switch (enCode)
	{
	case AUTHREQ:
		return "Authentication request"; 

	case AUTHPASSED:
		return "Authentication passed"; 

	case AUTHREFUSED:
		return "Authentication refused"; 

	default:
		return "Unrecognized";
	}
}

static const USHORT l_usaFCSTbl[256] =
{
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

//* 校验域： PPP帧（ST_PPP_HDR）地址域开始一直到PPP帧数据域结束
USHORT ppp_fcs16(UCHAR *pubData, USHORT usDataLen)
{
	USHORT usFCS = PPP_INITFCS;

	while (usDataLen--)
	{
		usFCS = (usFCS >> 8) ^ l_usaFCSTbl[(usFCS ^ *pubData++) & 0xFF];
	}

	return (~usFCS);
}

USHORT ppp_fcs16_ext(SHORT sBufListHead)
{
	USHORT usFCS = PPP_INITFCS; 

	SHORT sNextNode = sBufListHead;
	UCHAR *pubData;
	USHORT usDataLen;
	BOOL blIsFirstNode = TRUE; 

__lblGetNextNode:
	pubData = (UCHAR *)buf_list_get_next_node(&sNextNode, &usDataLen); 
	if (NULL == pubData)
		return (~usFCS);

	if (blIsFirstNode)
	{
		pubData++; //* 跳过帧首标志字符
		usDataLen--;
		blIsFirstNode = FALSE;
	}
	
	while (usDataLen--)	
		usFCS = (usFCS >> 8) ^ l_usaFCSTbl[(usFCS ^ *pubData++) & 0xFF];
	goto __lblGetNextNode; 
}

UINT ppp_escape_encode(UINT unACCM, UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punEncodedBytes)
{
	UCHAR ubaACCM[ACCM_BYTES];
	UINT i;

	for (i = 0; i < ACCM_BYTES; i++)
	{
		if (unACCM & (UINT)(pow(2, i)))
			ubaACCM[i] = 1;
		else
			ubaACCM[i] = 0;
	}

	//* 开始转义，ASCII表的前0 ~ 31字符有可能作为modem控制字符，所以传输报文需要转义，另外PPP的
	//* 头部标志字符在报文的数据中出现0x7E时也要转义
	UINT unDstBufLen = *punEncodedBytes, k = 0;
	for (; i < unDataLen && k < unDstBufLen; i++)
	{
		if ((pubData[i] < ACCM_BYTES && ubaACCM[pubData[i]]) || pubData[i] == 0x7D || pubData[i] == 0x7E)
		{
			if (k + 1 < unDstBufLen)
			{
				pubDstBuf[k] = 0x7D;
				k++;
				pubDstBuf[k] = pubData[i] ^ 0x20;
				k++;
			}			
			else
				break;
		}
		else
		{
			pubDstBuf[k] = pubData[i];
			k++;
		}
	}

	*punEncodedBytes = k;
	return i;
}

void ppp_escape_encode_init(UINT unACCM, UCHAR ubaACCM[])
{
	INT i; 
	for (i = 0; i < ACCM_BYTES; i++)
	{
		if (unACCM & (UINT)(pow(2, i)))
			ubaACCM[i] = 1;
		else
			ubaACCM[i] = 0;
	}
}

UINT ppp_escape_encode_ext(UCHAR ubaACCM[], UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punEncodedBytes)
{
	//* 开始转义，ASCII表的前0 ~ 31字符有可能作为modem控制字符，所以传输报文需要转义，另外PPP的
	//* 头部标志字符在报文的数据中出现0x7E时也要转义
	UINT unDstBufLen = *punEncodedBytes, i = 0, k = 0;
	for (; i < unDataLen && k < unDstBufLen; i++)
	{
		if ((pubData[i] < ACCM_BYTES && ubaACCM[pubData[i]]) || pubData[i] == 0x7D || pubData[i] == 0x7E)
		{
			if (k + 1 < unDstBufLen)
			{
				pubDstBuf[k] = 0x7D;
				k++;
				pubDstBuf[k] = pubData[i] ^ 0x20;
				k++;
			}
			else
				break;
		}
		else
		{
			pubDstBuf[k] = pubData[i];
			k++;
		}
	}

	*punEncodedBytes = k;
	return i;
}

UINT ppp_escape_decode(UCHAR *pubData, UINT unDataLen, UCHAR *pubDstBuf, UINT *punDecodedBytes)
{
	//* 解码
	UINT unDstBufLen = *punDecodedBytes, i = 0, k = 0;
	for (; i < unDataLen && k < unDstBufLen; k++)
	{
		if (pubData[i] != 0x7D)
		{
			pubDstBuf[k] = pubData[i];
			i++;
		}
		else
		{
			pubDstBuf[k] = pubData[i + 1] ^ 0x20;
			i += 2;
		}
	}

	*punDecodedBytes = k;
	return i;
}

UINT ppp_escape_decode_ext(UCHAR *pubData, UINT unStartIdx, UINT unEndIdx, UINT unDataBufSize, UCHAR *pubDstBuf, UINT *punDecodedBytes)
{
	//* 解码
	UINT unDstBufLen = *punDecodedBytes, i = unStartIdx, k = 0;
	while (k < unDstBufLen && i != unEndIdx)
	{
		if (pubData[i] != 0x7D)
		{
			pubDstBuf[k] = pubData[i];
			i++;
			if (i >= unDataBufSize)
				i = 0; 			
		}
		else
		{
			if (i + 1 >= unDataBufSize)
			{			
				if (0 == unEndIdx)
					break;

				pubDstBuf[k] = pubData[0] ^ 0x20;
				i = 1; 
			}
			else
			{
				if (i + 1 == unEndIdx)
					break;

				pubDstBuf[k] = pubData[i + 1] ^ 0x20;
				if (i + 2 >= unDataBufSize)
					i = 0;
				else
					i += 2; 
			}
		}
		k++; 
	}

	*punDecodedBytes = k;
	return i;
}

#endif
