#include "port/datatype.h"
#include "errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#include "ppp/wait_ack_list.h"
#define SYMBOL_GLOBALS
#include "ppp/ppp.h"
#undef SYMBOL_GLOBALS

static BOOL l_blIsRunning = TRUE;

//* 这个结构体必须严格按照EN_NPSPROTOCOL类型定义的顺序指定PPP定义的协议类型
static const ST_PPP_PROTOCOL lr_staProtocol[] = {
	{ PPP_LCP,  NULL }, 
	{ PPP_PAP,  NULL }, 
	{ PPP_CHAP, NULL },
	{ PPP_IPCP, NULL }, 
#if SUPPORT_IPV6
	{ PPP_IPV6CP, NULL },
#endif
	{ PPP_IP,   NULL }, 

#if SUPPORT_IPV6
	{ PPP_IPV6, NULL }
#endif
}; 

//* 在此指定连接modem的串行口，以此作为tty终端进行ppp通讯
static const CHAR *l_pszaTTY[PPP_NETLINK_NUM] = { "SCP3" };
static STCB_NETIFPPP l_staNetifPPP[PPP_NETLINK_NUM]; 
static ST_PPPNEGORESULT l_staNegoResult[PPP_NETLINK_NUM] = {
	{
		{ 0, PPP_MRU, ACCM_INIT,{ PPP_CHAP, 0x05 /* 对于CHAP协议来说，0-4未使用，0x05代表采用MD5算法 */ }, TRUE, TRUE, FALSE, FALSE },
		{ IP_ADDR_INIT, DNS_ADDR_INIT, DNS_ADDR_INIT, IP_ADDR_INIT, MASK_INIT }, 0
	}, 

	/* 系统存在几路ppp链路，就在这里添加几路的协商初始值，如果不确定，可以直接将上面预定义的初始值直接复制过来即可 */
}; 

//* 启动ppp处理线程，需要根据目标系统实际情况编写该函数，其实现的功能为启动ppp链路的主处理线程，该线程的入口函数为thread_ppp_handler()
static void thread_ppp_handler_start(INT nPPPIdx)
{
	//* 在此按照顺序建立工作线程，入口函数thread_ppp_handler()，线程入口参数为该ppp链路在l_staTTY数组的存储单元索引值
	//* 其直接强行进行数据类型转换即可，即作为线程入口参数时直接以如下形式传递：
	//* (void *)nPPPIdx
	//* 不要传递参数地址，即(void *)&nPPPIdx，这种方式时错误的
	//* 用户自定义代码
}

BOOL ppp_init(EN_ERROR_CODE *penErrCode)
{
	INT i; 

	//* 初始化tty
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		l_staNetifPPP[i].hTTY = tty_init(l_pszaTTY[i], penErrCode);
		if (INVALID_HTTY == l_staNetifPPP[i].hTTY)
			goto __lblEnd; 

		l_staNetifPPP[i].enState = TTYINIT; 
		l_staNetifPPP[i].pstNegoResult = &l_staNegoResult[i];
	}

	//* 启动ppp处理线程
	for (i = 0; i < PPP_NETLINK_NUM; i++)	
		thread_ppp_handler_start(i);	

__lblEnd: 
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (INVALID_HTTY != l_staNetifPPP[i].hTTY)		
			tty_uninit(l_staNetifPPP[i].hTTY);		
		else
			break; 
	}

	return FALSE; 
}

void ppp_uninit(void)
{
	l_blIsRunning = FALSE;

	INT i; 
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		while (l_staNetifPPP[i].blIsThreadExit)
			os_sleep_secs(1); 

		if (INVALID_HTTY != l_staNetifPPP[i].hTTY)		
			tty_uninit(l_staNetifPPP[i].hTTY);		
		else
			break;
	}
}

static PSTCB_NETIFPPP get_netif_ppp(HTTY hTTY)
{	
	if (INVALID_HTTY == hTTY)
		return NULL; 

	INT i;
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (hTTY == l_staNetifPPP[i].hTTY)
			return &l_staNetifPPP[i]; 
	}

	return NULL; 
}

//* ppp协议处理器
void thread_ppp_handler(void *pvParam)
{
	INT nPPPIdx = (INT)pvParam; 
	PSTCB_NETIFPPP pstcbPPP = &l_staNetifPPP[nPPPIdx];	
	EN_ERROR_CODE enErrCode; 

	//* 通知ppp协议栈处理线程已经开始运行
	l_staNetifPPP[nPPPIdx].blIsThreadExit = FALSE;

	INT nPacketLen; 
	while (l_blIsRunning)
	{
		nPacketLen = ppp_recv(pstcbPPP->hTTY, &enErrCode);
#if SUPPORT_PRINTF	
		if (nPacketLen < 0)
			printf("ppp_recv() failed, %s\r\n", error(enErrCode));
#endif

		//*  状态机
		switch (pstcbPPP->enState)
		{
		case TTYINIT:
		case STARTNEGOTIATION:
		case NEGOTIATION: 
			ppp_link_establish(pstcbPPP, &l_blIsRunning, &enErrCode); 
			break; 

		case ESTABLISHED: 
			break; 
		}
	}

	//* 通知ppp协议栈处理线程已经安全退出
	l_staNetifPPP[nPPPIdx].blIsThreadExit = TRUE;
}

//* ppp接收
INT ppp_recv(HTTY hTTY, EN_ERROR_CODE *penErrCode)
{
	PSTCB_NETIFPPP pstcbNetif = get_netif_ppp(hTTY);
	if (!pstcbNetif)
	{
		if (penErrCode)
			*penErrCode = ERRTTYHANDLE;
		return -1;
	}

	//* 读取数据
	INT nRcvBytes = tty_recv(hTTY, pstcbNetif->ubaFrameBuf, sizeof(pstcbNetif->ubaFrameBuf), penErrCode);
	if (nRcvBytes > 0)
	{
		//* 验证校验和是否正确
		USHORT usFCS = ppp_fcs16(pstcbNetif->ubaFrameBuf + 1, (USHORT)(nRcvBytes - 1 - sizeof(ST_PPP_TAIL))); 
		PST_PPP_TAIL pstTail = (PST_PPP_TAIL)&pstcbNetif->ubaFrameBuf[nRcvBytes - sizeof(ST_PPP_TAIL)];
		if (usFCS != pstTail->usFCS)
		{
			if (penErrCode)
				*penErrCode = ERRPPPFCS; 
			return -1; 
		}

		//* 解析ppp帧携带的上层协议类型值
		USHORT usProtocol; 
		INT nUpperStartIdx; 
		if (pstcbNetif->ubaFrameBuf[1] == PPP_ALLSTATIONS && pstcbNetif->ubaFrameBuf[2] == PPP_UI) //* 地址域与控制域未被压缩，使用正常协议头
		{
			PST_PPP_HDR pstHdr = (PST_PPP_HDR)pstcbNetif->ubaFrameBuf;
			usProtocol = pstHdr->usProtocol; 
			nUpperStartIdx = sizeof(ST_PPP_HDR); 
		}
		else
		{
			if (pstcbNetif->ubaFrameBuf[1] == PPP_IP 
	#if SUPPORT_IPV6
					|| pstcbNetif->ubaFrameBuf[1] ==  PPP_IPV6 //* 系统仅支持IP协议族，其它如IPX之类的协议族不提供支持
	#endif
				)
			{
				usProtocol = (USHORT)pstcbNetif->ubaFrameBuf[1]; 
				nUpperStartIdx = 2; 
			}
			else
			{
				((UCHAR *)&usProtocol)[0] = pstcbNetif->ubaFrameBuf[2];
				((UCHAR *)&usProtocol)[1] = pstcbNetif->ubaFrameBuf[1];
				nUpperStartIdx = 3; 
			}
		}

		//* 遍历协议栈支持的上层协议，找出匹配的类型值并调用对应的upper函数将报文传递给该函数处理之
		INT i;
		for (i = 0; i < (INT)(sizeof(lr_staProtocol) / sizeof(ST_PPP_PROTOCOL)); i++)
		{
			if (lr_staProtocol[i].pfunUpper)
				lr_staProtocol[i].pfunUpper(hTTY, pstcbNetif->ubaFrameBuf[nUpperStartIdx], nRcvBytes - nUpperStartIdx - sizeof(ST_PPP_TAIL), penErrCode); 
		}
	}

	return nRcvBytes; 
}

//* ppp发送
INT ppp_send(HTTY hTTY, EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ERROR_CODE *penErrCode)
{
	//* 确保上层协议在ppp协议栈支持的范围之内，超出的直接抛弃当前要发送的报文并报错
	if (enProtocol >= (INT)(sizeof(lr_staProtocol) / sizeof(USHORT)))
	{
		if (penErrCode)
			*penErrCode = ERRUNKNOWNPROTOCOL;

		return -1;
	}

	//* 获取ppp接口控制块
	PSTCB_NETIFPPP pstcbNetif = get_netif_ppp(hTTY); 
	if (!pstcbNetif)
	{
		if(penErrCode)
			*penErrCode = ERRTTYHANDLE; 
		return -1; 
	}

	//* 填充首部数据
	UCHAR ubHead[sizeof(ST_PPP_HDR)] = { PPP_FLAG } ;
	USHORT usHeadDataLen; 
	if (LCP != enProtocol && pstcbNetif->pstNegoResult->stLCP.blIsNegoValOfAddrCtlComp)
	{
		if (enProtocol < 0xFF && pstcbNetif->pstNegoResult->stLCP.blIsNegoValOfProtoComp)
		{
			ubHead[1] = ((UCHAR *)&lr_staProtocol[enProtocol].usType)[0];
			usHeadDataLen = 2;
		}
		else
		{
			ubHead[1] = ((UCHAR *)&lr_staProtocol[enProtocol].usType)[1];
			ubHead[2] = ((UCHAR *)&lr_staProtocol[enProtocol].usType)[0];
			usHeadDataLen = 3;
		}
	}
	else
	{
		PST_PPP_HDR pstHdr = (PST_PPP_HDR)ubHead;
		pstHdr->ubAddr = PPP_ALLSTATIONS; 
		pstHdr->ubCtl = PPP_UI;
		pstHdr->usProtocol = USHORT_EDGE_CONVERT(lr_staProtocol[enProtocol].usType);
		usHeadDataLen = sizeof(ST_PPP_HDR); 
	}

	//* 申请一个buf节点，将ppp帧头部数据挂载到链表头部
	SHORT sPPPHeadNode, sPPPTailNode;
	sPPPHeadNode = buf_list_get_ext(ubHead, usHeadDataLen, penErrCode);
	if (sPPPHeadNode < 0)
		return -1;
	buf_list_put_head(&sBufListHead, sPPPHeadNode);

	//* 申请一个buf节点，将ppp帧尾部数据（包括校验和及尾部标志）挂载到链表尾部
	ST_PPP_TAIL stTail;
	stTail.usFCS = ppp_fcs16_ext(sBufListHead);
	stTail.ubDelimiter = PPP_FLAG; 
	sPPPTailNode = buf_list_get_ext(&stTail, sizeof(ST_PPP_TAIL), penErrCode);
	if (sPPPTailNode < 0)
	{
		buf_list_free(sPPPHeadNode);
		return -1;
	}
	buf_list_put_tail(sBufListHead, sPPPTailNode);

	//* 完成实际的发送
	INT nRtnVal = tty_send_ext(hTTY, pstcbNetif->pstNegoResult->stLCP.unACCM, sBufListHead, penErrCode);

	//* 释放缓冲区节点
	buf_list_free(sPPPHeadNode);
	buf_list_free(sPPPTailNode);

	return nRtnVal; 
}

#endif