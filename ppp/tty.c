#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "ppp/chat.h"
#include "ppp/ppp_protocols.h"
#include "ppp/ppp_utils.h"
#include "onps_utils.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/tty.h"
#undef SYMBOL_GLOBALS

//* 记录系统分配的接收缓冲区首地址
static STCB_TTYIO l_stcbaIO[PPP_NETLINK_NUM];
static UINT l_unTTYIdx = 0; 

//* tty设备初始化
HTTY tty_init(const CHAR *pszTTYName, EN_ONPSERR *penErr)
{
	if (l_unTTYIdx >= PPP_NETLINK_NUM)
	{
		*penErr = ERRTOOMANYTTYS; 
		return INVALID_HTTY; 
	}

	HTTY hTTY; 
	hTTY = os_open_tty(pszTTYName); 
	if (INVALID_HTTY == hTTY)
	{
		*penErr = ERROPENTTY;
		return INVALID_HTTY; 
	}	

	l_stcbaIO[l_unTTYIdx].hTTY = hTTY;
	l_stcbaIO[l_unTTYIdx].stRecv.nWriteIdx = 0;
	l_unTTYIdx++; 

	return hTTY; 
}

//* tty设备去初始化
void tty_uninit(HTTY hTTY)
{
	if (INVALID_HTTY != hTTY)
		os_close_tty(hTTY);
}

BOOL tty_ready(HTTY hTTY, EN_ONPSERR *penErr)
{	
    //* 先复位modem，以消除一切不必要的设备错误，如果不需要则port层只需实现一个无任何操作的空函数即可，或者直接注释掉这个函数的调用
	os_modem_reset(hTTY);
	do {
		if (!modem_ready(hTTY, penErr))
			break; 

		if (!modem_dial(hTTY, penErr))
			break; 

		return TRUE; 
	} while (FALSE);

	os_close_tty(hTTY);	
	return FALSE; 
}

static PSTCB_TTYIO get_io_control_block(HTTY hTTY, EN_ONPSERR *penErr)
{
	INT i; 
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (hTTY == l_stcbaIO[i].hTTY)		
			return &l_stcbaIO[i];		
	}

	if (penErr)
		*penErr = ERRTTYHANDLE;

#if SUPPORT_PRINTF	
	#if DEBUG_LEVEL
		printf("<%d> get_io_control_block() failed, %s\r\n", hTTY, onps_error(ERRTTYHANDLE));
	#endif
#endif

	return NULL; 
}

INT tty_recv(HTTY hTTY, UCHAR *pubRecvBuf, INT nRecvBufLen, INT nWaitSecs, EN_ONPSERR *penErr)
{
	INT nRcvBytes; 
	INT nStartIdx;
	BOOL blHasFoundHdrFlag = FALSE;
	UINT unStartSecs = os_get_system_secs(); 
	INT nReadIdx = 0;
	UINT unTimeout;
	PSTCB_TTYIO pstcbIO = get_io_control_block(hTTY, penErr);	
	if (NULL == pstcbIO)
		return -1; 

	//* 读取数据
__lblRcv:
	if (pstcbIO->stRecv.nWriteIdx > 0)
		unTimeout = 3;
	else
		unTimeout = 1;

	if (os_get_system_secs() - unStartSecs > unTimeout)
	{
		INT nRtnVal = 0;
		if (pstcbIO->stRecv.nWriteIdx > 0)
		{			
	#if SUPPORT_PRINTF	
		#if DEBUG_LEVEL
			printf("ppp frame delimiter not found, recv %d bytes:\r\n", pstcbIO->stRecv.nWriteIdx); 
			printf_hex(pstcbIO->stRecv.ubaBuf, pstcbIO->stRecv.nWriteIdx, 48);
		#endif
	#endif
			if (penErr)
				*penErr = ERRPPPDELIMITER; 

			nRtnVal = -1;
		}

		return nRtnVal;
	}

	nRcvBytes = os_tty_recv(hTTY, pstcbIO->stRecv.ubaBuf + pstcbIO->stRecv.nWriteIdx, (INT)(TTY_RCV_BUF_SIZE - pstcbIO->stRecv.nWriteIdx), nWaitSecs);
	if (nRcvBytes > 0)
		pstcbIO->stRecv.nWriteIdx += nRcvBytes;

	for (; nReadIdx < pstcbIO->stRecv.nWriteIdx; nReadIdx++)
	{
		if (pstcbIO->stRecv.ubaBuf[nReadIdx] == PPP_FLAG)
		{
			if (blHasFoundHdrFlag)
			{				
				UINT unPacketByes = ppp_escape_decode(pstcbIO->stRecv.ubaBuf + nStartIdx, (UINT)(nReadIdx - nStartIdx + 1), pubRecvBuf, (UINT *)&nRecvBufLen);
				UINT unRemainBytes = pstcbIO->stRecv.nWriteIdx - nReadIdx - 1;
				if (unRemainBytes > 0)
					memmove(pstcbIO->stRecv.ubaBuf, pstcbIO->stRecv.ubaBuf + nReadIdx + 1, unRemainBytes);
				pstcbIO->stRecv.nWriteIdx = unRemainBytes;

	#if SUPPORT_PRINTF	
		#if DEBUG_LEVEL				
				printf("recv %d bytes: \r\n", nRecvBufLen);
				printf_hex(pubRecvBuf, nRecvBufLen, 48);
		#endif	
	#endif			
				return nRecvBufLen; 
			}
			else
			{
				nStartIdx = nReadIdx;
				blHasFoundHdrFlag = TRUE;
			}
		}
	}

	goto __lblRcv;
}

INT tty_send(HTTY hTTY, UINT unACCM, UCHAR *pubData, INT nDataLen, EN_ONPSERR *penErr)
{
	UCHAR ubaACCM[ACCM_BYTES];

	//* 获取tty设备的IO控制块
	PSTCB_TTYIO pstcbIO = get_io_control_block(hTTY, penErr);
	if (NULL == pstcbIO)
		return -1;

	//* 准备转义
	ppp_escape_encode_init(unACCM, ubaACCM);

	//* 开始转义
	UINT unEscapedTotalBytes = 0; 
	UINT unEscapedBytes, unEncodedBytes;
	BOOL blIsSendTailFlag = FALSE; 
	pstcbIO->ubaSendBuf[0] = PPP_FLAG; 

__lblEscape:
	if (unEscapedTotalBytes >= (UINT)nDataLen)
		return nDataLen; 
	
	unEncodedBytes = unEscapedTotalBytes ? TTY_SEND_BUF_SIZE : TTY_SEND_BUF_SIZE - 1; 
	unEscapedBytes = ppp_escape_encode_ext(ubaACCM, pubData + unEscapedTotalBytes, ((UINT)nDataLen) - unEscapedTotalBytes, &pstcbIO->ubaSendBuf[TTY_SEND_BUF_SIZE - unEncodedBytes], &unEncodedBytes); 	
	if (!unEscapedTotalBytes) //* 第一段报文，携带了ppp帧定界符
		unEncodedBytes += 1;
	unEscapedTotalBytes += unEscapedBytes;
	if (unEscapedTotalBytes >= (UINT)nDataLen) //* 最后一段报文，同样需要携带ppp帧定界符
	{
		if (unEncodedBytes < TTY_SEND_BUF_SIZE)
		{
			pstcbIO->ubaSendBuf[unEncodedBytes] = PPP_FLAG;
			unEncodedBytes += 1; 
		}
		else
			blIsSendTailFlag = TRUE;
	}

__lblSend: 
	if (os_tty_send(hTTY, pstcbIO->ubaSendBuf, (INT)unEncodedBytes) < 0)
	{
		if (penErr)
			*penErr = ERROSADAPTER;

#if SUPPORT_PRINTF	
	#if DEBUG_LEVEL
		printf("<%d> os_tty_send() failed, %s\r\n", hTTY, onps_error(ERROSADAPTER));
	#endif
#endif
		return -1;
	}

	if (blIsSendTailFlag)
	{
		pstcbIO->ubaSendBuf[0] = PPP_FLAG;
		unEncodedBytes = 1;
		blIsSendTailFlag = FALSE;
		goto __lblSend;
	}
	else
		goto __lblEscape; 
}

INT tty_send_ext(HTTY hTTY, UINT unACCM, SHORT sBufListHead, EN_ONPSERR *penErr)
{
	UCHAR ubaACCM[ACCM_BYTES];

	//* 获取tty设备的IO控制块
	PSTCB_TTYIO pstcbIO = get_io_control_block(hTTY, penErr);
	if (NULL == pstcbIO)
		return -1;

	//* 准备转义
	ppp_escape_encode_init(unACCM, ubaACCM);	

	//* 开始转义
	INT nDataLen = 1;
	UINT unEscapedTotalBytes, unEncodedBytes;
	SHORT sNextNode = sBufListHead;
	UCHAR *pubData;
	USHORT usDataLen;	
	pstcbIO->ubaSendBuf[0] = PPP_FLAG;	//* 链表第一个节点的首字符一定是帧首定界符

__lblGetNextNode: 
	if (NULL == (pubData = (UCHAR *)buf_list_get_next_node(&sNextNode, &usDataLen)))
		return nDataLen; 	
	if (nDataLen != 1) //* 不是第一个节点就需要判断是不是尾部节点
	{
		if (sNextNode < 0) //* 这就是尾部节点了
		{
			//* 把尾部的定界符去掉，不要对它转义
			usDataLen--;
			nDataLen += 1; 
		}
	}
	else //* 第一个节点
	{
		//* 跳过ppp帧首定界符的转义
		pubData++;
		usDataLen--;
	}

	unEscapedTotalBytes = 0;
__lblEscape: 
	if (unEscapedTotalBytes >= (UINT)usDataLen)
	{		
		nDataLen += (INT)usDataLen;
		goto __lblGetNextNode; 
	}

	unEncodedBytes = (nDataLen != 1) ? TTY_SEND_BUF_SIZE : TTY_SEND_BUF_SIZE - 1;
	unEscapedTotalBytes += ppp_escape_encode_ext(ubaACCM, pubData + unEscapedTotalBytes, ((UINT)usDataLen) - unEscapedTotalBytes, &pstcbIO->ubaSendBuf[TTY_SEND_BUF_SIZE - unEncodedBytes], &unEncodedBytes);
	if (sNextNode < 0) //* 这是最后一个携带校验和的buf节点，则需要在尾部再挂载一个ppp帧定界符	
		pstcbIO->ubaSendBuf[unEncodedBytes] = PPP_FLAG;		

	//* 发送转义后的ppp帧
	if (os_tty_send(hTTY, pstcbIO->ubaSendBuf, (nDataLen == 1 || sNextNode < 0) ? (INT)unEncodedBytes + 1 : (INT)unEncodedBytes) < 0)
	{
		if (penErr)
			*penErr = ERROSADAPTER;

#if SUPPORT_PRINTF	
	#if DEBUG_LEVEL
		printf("<%d> os_tty_send() failed, %s\r\n", hTTY, onps_error(ERROSADAPTER));
	#endif
#endif
		return -1;
	}

	goto __lblEscape;
}

#endif
