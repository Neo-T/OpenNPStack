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
    *penErr = ERRNO; 
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
    l_stcbaIO[l_unTTYIdx].stRecv.bErrCount = 0; 
	//l_stcbaIO[l_unTTYIdx].stRecv.nWriteIdx = 0;
	l_unTTYIdx++; 

	return hTTY; 
}

//* tty设备去初始化
void tty_uninit(HTTY hTTY)
{
	if (INVALID_HTTY != hTTY)
		os_close_tty(hTTY);
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

#if SUPPORT_PRINTF && DEBUG_LEVEL	
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif
		printf("<%d> get_io_control_block() failed, %s\r\n", hTTY, onps_error(ERRTTYHANDLE));
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif	
#endif

	return NULL; 
}

BOOL tty_ready(HTTY hTTY, EN_ONPSERR *penErr)
{	
    //* 确保modem复位时能够重新接收数据
    PSTCB_TTYIO pstcbIO = get_io_control_block(hTTY, penErr);
    if (NULL == pstcbIO)
        return FALSE;
    pstcbIO->stRecv.nWriteIdx = 0;
    pstcbIO->stRecv.nReadIdx = 0; 
    pstcbIO->stRecv.bState = 0; 

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

INT tty_recv(INT nPPPIdx, HTTY hTTY, UCHAR *pubRecvBuf, INT nRecvBufLen, void(*pfunPacketHandler)(INT, UCHAR *, INT), INT nWaitSecs, EN_ONPSERR *penErr)
{
    CHAR bIsExempt = FALSE;
    INT nRtnVal = 0; 

    PSTCB_TTYIO pstcbIO = get_io_control_block(hTTY, penErr);
    if (NULL == pstcbIO)
        return -1;

    //* 读取新到达的数据，只要重新进入该函数，说明收到的数据已经处理完毕，需要新的血液到达来重新激活处理过程    
    INT nRcvBytes = os_tty_recv(hTTY, pubRecvBuf + pstcbIO->stRecv.nWriteIdx, (nRecvBufLen - pstcbIO->stRecv.nWriteIdx), nWaitSecs);
    if (nRcvBytes > 0)
    {
#if SUPPORT_PRINTF && DEBUG_LEVEL	        
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
    #endif        
        printf("recv %d bytes: \r\n", nRcvBytes);
        printf_hex(pubRecvBuf + pstcbIO->stRecv.nWriteIdx, nRcvBytes, 48);
    #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif        

        //* 更新写指针
        pstcbIO->stRecv.nWriteIdx += nRcvBytes; 

        //* 解析收到数据，处理逻辑如下：
        //* 1) 先查找ppp报文头标识，找到一对该标识，一头一尾，中间为携带的ppp报文；
        //* 2) 为节省内存就地转义，不再使用暂存缓冲区；
        //* 3) 计算校验和，正确，调用回调函数pfunPacketHandler处理之，回到第一步继续解析剩余的数据；
        //* 4) 失败，pstcbIO->stRecv.nReadIdx调整到ppp尾部标识处，使其组为新一组ppp报文的起始标识；
        //* 5) 使用memmove()函数将缓冲区中剩余数据全部移动到缓冲区前部，以方便下一组报文的处理；
        //* 6) 回到第1步重新开始解析流程；
        //* 7) nReadIdx与nWriteIdx相等则解析完毕，函数返回；                 
        while (pstcbIO->stRecv.nReadIdx < pstcbIO->stRecv.nWriteIdx)
        {
            if (!pstcbIO->stRecv.bState) //* 尚未找到第一个标识
            {                                
                UCHAR *pubStart = memchr(pubRecvBuf, (int)PPP_FLAG, pstcbIO->stRecv.nWriteIdx);                
                if (pubStart)
                {
                    pstcbIO->stRecv.nReadIdx = pubStart - pubRecvBuf; 
                    pstcbIO->stRecv.bState = 1;
                }
                else                
                    goto __lblErr;                
            }
            else 
            {
                //* 找尾部标识                
                UCHAR *pubEnd = memchr(pubRecvBuf + pstcbIO->stRecv.nReadIdx + 1, (int)PPP_FLAG, pstcbIO->stRecv.nWriteIdx - (pstcbIO->stRecv.nReadIdx + 1));                
                if (pubEnd)
                {
                    //* 先计算剩余数据长度，这个数值一定大于0，因为两个标识都找到了，所以剩余字节数至少为1
                    UINT unRemainBytes = pstcbIO->stRecv.nWriteIdx - (pubEnd - pubRecvBuf);

                    //* 计算原始报文的长度
                    UINT unRawPacketBytes = (UINT)(pubEnd - (pubRecvBuf + pstcbIO->stRecv.nReadIdx) + 1);

                    //* 原始报文长度太短，有可能是前一个报文没收全仅收到了尾部标识，紧接着tty收到的是下一个报文的头部标识，两个紧挨着就会出现这种情况
                    if (unRawPacketBytes < sizeof(ST_PPP_HDR) + sizeof(ST_PPP_TAIL) + 1)
                    {                               
                        memmove(pubRecvBuf, pubEnd, unRemainBytes); 
                        pstcbIO->stRecv.nWriteIdx = unRemainBytes; 
                        pstcbIO->stRecv.nReadIdx = 0;   //* 首部即是ppp帧开始位置
                        pstcbIO->stRecv.bState = 1;     //* 只需再找到一个尾部标识即可
                        bIsExempt = TRUE;
                        continue;
                    }

                    //* 就地转义，转义后的报文直接从接收缓冲区的起始地址开始存放
                    UINT unDecodedBytes = nRecvBufLen; 
                    ppp_escape_decode(pubRecvBuf + pstcbIO->stRecv.nReadIdx, (UINT)unRawPacketBytes, pubRecvBuf, &unDecodedBytes);                                       

                    //* 报文校验                    
                    USHORT usFCS = ppp_fcs16(pubRecvBuf + 1, (USHORT)(unDecodedBytes - 1 - sizeof(ST_PPP_TAIL)));
                    PST_PPP_TAIL pstTail = (PST_PPP_TAIL)(pubRecvBuf + unDecodedBytes - sizeof(ST_PPP_TAIL)); 
                    if (usFCS == pstTail->usFCS)
                    {  
                        //* 传递给ppp层，处理之
                        pfunPacketHandler(nPPPIdx, pubRecvBuf, (INT)unDecodedBytes);
                        pstcbIO->stRecv.bErrCount = 0; 

                        //* 处理完毕，看看是否还存在剩余数据，如果存在则将剩余数据转移到接收缓冲区首部                        
                        unRemainBytes -= 1; 
                        if (unRemainBytes)                        
                            memmove(pubRecvBuf, pubEnd + 1/* 跳过尾部标识 */, unRemainBytes);
                        pstcbIO->stRecv.nWriteIdx = unRemainBytes;
                        pstcbIO->stRecv.nReadIdx = 0;   //* 首部即是ppp帧开始位置
                        pstcbIO->stRecv.bState = 0;     //* 开始截取下一个ppp帧
                        nRtnVal = 1;
                    }
                    else
                    {                        
                        if (penErr)
                            *penErr = ERRPPPFCS;

                        //* 校验和不对，尾部标识转成首部标识，继续解析下一组报文                       
                        memmove(pubRecvBuf, pubEnd + 1, unRemainBytes);
                        pstcbIO->stRecv.nWriteIdx = unRemainBytes;
                        pstcbIO->stRecv.nReadIdx = 0;   //* 首部即是ppp帧开始位置
                        pstcbIO->stRecv.bState = 1;     //* 只需再找到一个尾部标识即可
                        bIsExempt = TRUE;
                        continue;
                    }
                }
                else
                    goto __lblErr; 
            }
        }
    }

    return nRtnVal;

__lblErr: 
    if (bIsExempt)
        return 0; 

    //* 没找到，说明当前接收到的数据不完整，存在tty接收过速的问题，直接丢弃当前的数据，一切归零，并记录这个错误
    pstcbIO->stRecv.nReadIdx = pstcbIO->stRecv.nWriteIdx = 0;
    pstcbIO->stRecv.bErrCount++;
    if (pstcbIO->stRecv.bErrCount < 6)
    {
        pstcbIO->stRecv.bErrCount = 0; 
        pstcbIO->stRecv.bState = 0;
        return 0;
    }
 
#if SUPPORT_PRINTF && DEBUG_LEVEL    
    #if PRINTF_THREAD_MUTEX
    os_thread_mutex_lock(o_hMtxPrintf);
    #endif
    printf("#%d# ppp frame delimiter not found, recv %d bytes:\r\n", (INT)pstcbIO->stRecv.bState, pstcbIO->stRecv.nWriteIdx); 
    printf_hex(pubRecvBuf, pstcbIO->stRecv.nWriteIdx, 48);
    #if PRINTF_THREAD_MUTEX
    os_thread_mutex_unlock(o_hMtxPrintf);
    #endif
#endif

    pstcbIO->stRecv.bState = 0;

    if (penErr)
        *penErr = ERRPPPDELIMITER;

    return -1; 
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
        #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
        #endif
		printf("<%d> os_tty_send() failed, %s\r\n", hTTY, onps_error(ERROSADAPTER));
        #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
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
        #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
        #endif
		printf("<%d> os_tty_send() failed, %s\r\n", hTTY, onps_error(ERROSADAPTER));
        #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
	#endif
#endif
		return -1;
	}  

	goto __lblEscape;
}

#endif
