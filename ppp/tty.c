#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "ppp/chat.h"
#include "ppp/ppp_protocols.h"
#include "ppp/ppp_utils.h"

#if SUPPORT_PPP
#define SYMBOL_GLOBALS
#include "ppp/tty.h"
#undef SYMBOL_GLOBALS

//* 记录系统分配的接收缓冲区首地址
static STCB_TTY_READ l_stcbaTTYRead[TTY_NUM];

//* tty设备初始化
HTTY tty_init(const CHAR *pszTTYName, EN_ERROR_CODE *penErrCode)
{
	HTTY hTTY; 
	hTTY = os_open_tty(pszTTYName); 
	if (INVALID_HTTY == hTTY)
	{
		*penErrCode = ERROPENTTY;
		return INVALID_HTTY; 
	}

	INT i = 0;
	for (; i < TTY_NUM; i++)
	{		
		l_stcbaTTYRead[i].pubBuf = (UCHAR *)buddy_alloc(TTY_RCV_BUF_SIZE, penErrCode);
		if (NULL == l_stcbaTTYRead[i].pubBuf)
		{
			os_close_tty(hTTY); 
			hTTY = INVALID_HTTY; 
			break; 
		}

		l_stcbaTTYRead[i].hTTY = hTTY;
		l_stcbaTTYRead[i].unWriteIdx = 0; 
	}

	//* 说明前面分配内存失败了，这里需要回收之前分配的内存
	if (INVALID_HTTY == hTTY)
	{
		for (i = 0; i < TTY_NUM; i++)
		{
			if (NULL != l_stcbaTTYRead[i].pubBuf)
				buddy_free(l_stcbaTTYRead[i].pubBuf); 
			else
				break; 
		}
	}

	return hTTY; 
}

//* tty设备去初始化
void tty_uninit(HTTY hTTY)
{
	INT i; 

	if (INVALID_HTTY != hTTY)
	{
		os_close_tty(hTTY);
		hTTY = INVALID_HTTY;
	}

	for (i = 0; i < TTY_NUM; i++)
	{
		if (hTTY == l_stcbaTTYRead[i].hTTY)
		{
			if (NULL != l_stcbaTTYRead[i].pubBuf)
				buddy_free(l_stcbaTTYRead[i].pubBuf);

			break;
		}		
	}
}

BOOL tty_ready(HTTY hTTY, EN_ERROR_CODE *penErrCode)
{
	EN_ERROR_CODE enErrCode; 
	
	//* 先复位modem，以消除一切不必要的设备错误，如果不需要则port层只需实现一个无任何操作的空函数即可，或者直接注释掉这个函数的调用
	os_modem_reset(hTTY);
	do {
		if (!modem_ready(hTTY, &enErrCode))
			break; 

		if (!modem_dial(hTTY, &enErrCode))
			break; 

		return TRUE; 
	} while (FALSE);

	os_close_tty(hTTY);	
	return FALSE; 
}

INT tty_read(HTTY hTTY, UCHAR *pubReadBuf, INT nReadBufLen, EN_ERROR_CODE *penErrCode)
{
	UINT unRcvBytes; 
	UINT unStartIdx;
	BOOL blHasFoundHdrFlag = FALSE;
	UINT unStartSecs = 0;
	UINT unReadIdx = 0;
	UINT unTimeout;
	PSTCB_TTY_READ pstcbRead; 
	INT i; 

	//* 获取读取控制块相关结构
	for (i = 0; i < TTY_NUM; i++)
	{
		if (hTTY == l_stcbaTTYRead[i].hTTY)
		{
			pstcbRead = &l_stcbaTTYRead[i]; 
			break; 
		}
	}

	//* 读取数据
__lblRcv:
	if (pstcbRead->unWriteIdx > 0)
		unTimeout = 3;
	else
		unTimeout = 1;

	if (os_get_system_secs() - unStartSecs > unTimeout)
	{
		INT nRtnVal = 0;
		if (pstcbRead->unWriteIdx > 0)
		{
			*penErrCode = ERRPPPDELIMITER; 
	#if SUPPORT_PRINTF	
		#if DEBUG_LEVEL
			printf("ppp frame delimiter not found, recv %d bytes:\r\n", pstcbRead->unWriteIdx);
			print_hex(pstcbRead->pubBuf, pstcbRead->unWriteIdx, 48);
		#endif
	#endif

			nRtnVal = -1;
		}

		return nRtnVal;
	}

	unRcvBytes = os_tty_recv(hTTY, pstcbRead->pubBuf + pstcbRead->unWriteIdx, TTY_RCV_BUF_SIZE - pstcbRead->unWriteIdx); 
	if (unRcvBytes > 0)
		pstcbRead->unWriteIdx += unRcvBytes;

	for (; unReadIdx < pstcbRead->unWriteIdx; unReadIdx++)
	{
		if (pstcbRead->pubBuf[unReadIdx] == PPP_FLAG)
		{
			if (blHasFoundHdrFlag)
			{				
				UINT unPacketByes = ppp_escape_decode(pstcbRead->pubBuf + unStartIdx, unReadIdx - unStartIdx + 1, pubReadBuf, (UINT *)&nReadBufLen);
				UINT unRemainBytes = pstcbRead->unWriteIdx - unReadIdx - 1;
				if (unRemainBytes > 0)
					memmove(pstcbRead->pubBuf, pstcbRead->pubBuf + unReadIdx + 1, unRemainBytes);
				pstcbRead->unWriteIdx = unRemainBytes;

	#if SUPPORT_PRINTF	
		#if DEBUG_LEVEL				
				printf("recv %d bytes: \r\n", nPacketBufLen);
				PrintHexArray(pubPacketBuf, nPacketBufLen, 48);
		#endif	
	#endif			
				return nReadBufLen;
			}
			else
			{
				unStartIdx = unReadIdx;
				blHasFoundHdrFlag = TRUE;
			}
		}
	}

	goto __lblRcv;
}

#endif