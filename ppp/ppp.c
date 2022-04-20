#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buf_list.h"
#include "onps_utils.h"
#include "netif/netif.h"
#include "netif/route.h"
#include "ip/ip.h"

#if SUPPORT_PPP
#include "ppp/negotiation.h"
#include "ppp/lcp.h"
#include "ppp/chap.h"
#include "ppp/pap.h"
#include "ppp/ipcp.h"
#define SYMBOL_GLOBALS
#include "ppp/ppp.h"
#undef SYMBOL_GLOBALS

static BOOL l_blIsRunning = TRUE;

//* ppp层实现的ip接收函数
static void ppp_ip_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen); 

static const ST_PPP_PROTOCOL lr_staProtocol[] = { //* 这个结构体必须严格按照EN_NPSPROTOCOL类型定义的顺序指定PPP定义的协议类型
	{ PPP_LCP,  lcp_recv },
	{ PPP_PAP,  pap_recv }, 
	{ PPP_CHAP, chap_recv },
	{ PPP_IPCP, ipcp_recv }, 
#if SUPPORT_IPV6
	{ PPP_IPV6CP, NULL },
#endif
	{ PPP_IP,   ppp_ip_recv },

#if SUPPORT_IPV6
	{ PPP_IPV6, NULL }
#endif
}; 

//* 在此指定连接modem的串行口，以此作为tty终端进行ppp通讯
static const CHAR *lr_pszaTTY[PPP_NETLINK_NUM] = { "SCP3" };
static const ST_DIAL_AUTH_INFO lr_staDialAuth[PPP_NETLINK_NUM] = {
	{ "4gnet", "card", "any_char" },  /* 注意ppp账户和密码尽量控制在20个字节以内，太长需要需要修改chap.c中send_response()函数的szData数组容量及 */
									  /* pap.c中pap_send_auth_request()函数的ubaPacket数组的容量，确保其能够封装一个完整的响应报文              */
}; 
static STCB_PPP l_staPPP[PPP_NETLINK_NUM]; 
static PST_NETIF_NODE l_pstaNetif[PPP_NETLINK_NUM];
static HMUTEX l_haMtxTTY[PPP_NETLINK_NUM];
static UCHAR l_ubaaFrameBuf[PPP_NETLINK_NUM][PPP_MRU];
static UCHAR l_ubaThreadExitFlag[PPP_NETLINK_NUM];
static ST_PPPNEGORESULT l_staNegoResult[PPP_NETLINK_NUM] = {
	{
		{ 0, PPP_MRU, ACCM_INIT,{ PPP_CHAP, 0x05 /* 对于CHAP协议来说，0-4未使用，0x05代表采用MD5算法 */ }, TRUE, TRUE, FALSE, FALSE },
		{ IP_ADDR_INIT, DNS_ADDR_INIT, DNS_ADDR_INIT, IP_ADDR_INIT, MASK_INIT }, 0
	}, 

	/* 系统存在几路ppp链路，就在这里添加几路的协商初始值，如果不确定，可以直接将上面预定义的初始值直接复制过来即可 */
}; 

BOOL ppp_init(EN_ONPSERR *penErr)
{
	INT i; 

    *penErr = ERRNO; 

    //* 先赋初值（避免去初始化函数执行出错）
    for (i = 0; i < PPP_NETLINK_NUM; i++)
        l_staPPP[i].hTTY = INVALID_HTTY; 

	//* 初始化tty
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		l_staPPP[i].hTTY = tty_init(lr_pszaTTY[i], penErr);
		if (INVALID_HTTY == l_staPPP[i].hTTY)
			goto __lblEnd; 

		l_haMtxTTY[i] = os_thread_mutex_init();
        if (INVALID_HMUTEX == l_haMtxTTY[i])
        {
            *penErr = ERRMUTEXINITFAILED; 
            goto __lblEnd;
        }

		l_staPPP[i].enState = TTYINIT; 
		l_staPPP[i].pstNegoResult = &l_staNegoResult[i];
        l_pstaNetif[i] = NULL;
        l_ubaThreadExitFlag[i] = TRUE;
	}

    return TRUE; 

__lblEnd: 
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (INVALID_HTTY != l_staPPP[i].hTTY)
		{
			tty_uninit(l_staPPP[i].hTTY);

			if (INVALID_HMUTEX != l_haMtxTTY[i])
				os_thread_mutex_uninit(l_haMtxTTY[i]);
		}
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
		while (!l_ubaThreadExitFlag[i])
			os_sleep_secs(1); 

		if (INVALID_HTTY != l_staPPP[i].hTTY)
		{
			tty_uninit(l_staPPP[i].hTTY);

            if (l_pstaNetif[i])
            {
                route_del_ext(&l_pstaNetif[i]->stIf);
                netif_del(l_pstaNetif[i]);
                l_pstaNetif[i] = NULL;
            }

			if (INVALID_HMUTEX != l_haMtxTTY[i])
				os_thread_mutex_uninit(l_haMtxTTY[i]);
		}
		else
			break;
	}
}

static PSTCB_PPP get_ppp(HTTY hTTY, INT *pnPPPIndex)
{	
	if (INVALID_HTTY == hTTY)
		return NULL; 

	INT i;
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (hTTY == l_staPPP[i].hTTY)
		{
			if (pnPPPIndex)
				*pnPPPIndex = i;

			return &l_staPPP[i];
		}
	}

	return NULL; 
}

INT get_ppp_index(HTTY hTTY)
{
	if (INVALID_HTTY == hTTY)
		return -1;

	INT i;
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (hTTY == l_staPPP[i].hTTY)
			return i;
	}

	return -1;
}

const CHAR *get_ppp_port_name(HTTY hTTY)
{
	if (INVALID_HTTY == hTTY)
		return "unspecified";

	INT i;
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (hTTY == l_staPPP[i].hTTY)
			return lr_pszaTTY[i];
	}

	return "unspecified"; 
}

const ST_DIAL_AUTH_INFO *get_ppp_dial_auth_info(HTTY hTTY)
{
	if (INVALID_HTTY == hTTY)
		return NULL;

	INT i;
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (hTTY == l_staPPP[i].hTTY)
			return &lr_staDialAuth[i];
	}

	return NULL;
}

void get_ppp_auth_info(HTTY hTTY, const CHAR **pszUser, const CHAR **pszPassword)
{
	*pszUser = AUTH_USER_DEFAULT;
	*pszPassword = AUTH_PASSWORD_DEFAULT;

	INT i;
	for (i = 0; i < PPP_NETLINK_NUM; i++)
	{
		if (hTTY == l_staPPP[i].hTTY)
		{
			*pszUser = lr_staDialAuth[i].pszUser;
			*pszPassword = lr_staDialAuth[i].pszPassword;
		}
	}
}

//* ppp接收
static INT ppp_recv(INT nPPPIdx, EN_ONPSERR *penErr, INT nWaitSecs)
{
	PSTCB_PPP pstcbPPP = &l_staPPP[nPPPIdx];

	//* 读取数据
	UCHAR *pubFrameBuf = l_ubaaFrameBuf[nPPPIdx];
	INT nRcvBytes = tty_recv(pstcbPPP->hTTY, pubFrameBuf, sizeof(l_ubaaFrameBuf[nPPPIdx]), nWaitSecs, penErr);
	if (nRcvBytes > 0)
	{
		//* 验证校验和是否正确
		USHORT usFCS = ppp_fcs16(pubFrameBuf + 1, (USHORT)(nRcvBytes - 1 - sizeof(ST_PPP_TAIL)));
		PST_PPP_TAIL pstTail = (PST_PPP_TAIL)(pubFrameBuf + nRcvBytes - sizeof(ST_PPP_TAIL));
		if (usFCS != pstTail->usFCS)
		{
			if (penErr)
				*penErr = ERRPPPFCS; 
			return -1; 
		}

		//* 解析ppp帧携带的上层协议类型值
		USHORT usProtocol; 
		INT nUpperStartIdx; 
		if (pubFrameBuf[1] == PPP_ALLSTATIONS && pubFrameBuf[2] == PPP_UI) //* 地址域与控制域未被压缩，使用正常协议头
		{
			PST_PPP_HDR pstHdr = (PST_PPP_HDR)pubFrameBuf;
			usProtocol = ENDIAN_CONVERTER_USHORT(pstHdr->usProtocol);
			nUpperStartIdx = sizeof(ST_PPP_HDR); 
		}
		else
		{
			if (pubFrameBuf[1] == PPP_IP
	#if SUPPORT_IPV6
					|| pstcbNetif->ubaFrameBuf[1] ==  PPP_IPV6 //* 系统仅支持IP协议族，其它如IPX之类的协议族不提供支持
	#endif
				)
			{
				usProtocol = (USHORT)pubFrameBuf[1];
				nUpperStartIdx = 2; 
			}
			else
			{
				((UCHAR *)&usProtocol)[0] = pubFrameBuf[2];
				((UCHAR *)&usProtocol)[1] = pubFrameBuf[1];
				nUpperStartIdx = 3; 
			}
		}

		//* 遍历协议栈支持的上层协议，找出匹配的类型值并调用对应的upper函数将报文传递给该函数处理之
		INT i;
		for (i = 0; i < (INT)(sizeof(lr_staProtocol) / sizeof(ST_PPP_PROTOCOL)); i++)
		{
            if (usProtocol == lr_staProtocol[i].usType)
            {
                if (lr_staProtocol[i].pfunUpper)
                    lr_staProtocol[i].pfunUpper(pstcbPPP, pubFrameBuf + nUpperStartIdx, nRcvBytes - nUpperStartIdx - sizeof(ST_PPP_TAIL));
                break; 
            }			
		}
	}

	return nRcvBytes; 
}

//* ppp发送
INT ppp_send(HTTY hTTY, EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ONPSERR *penErr)
{
	//* 确保上层协议在ppp协议栈支持的范围之内，超出的直接抛弃当前要发送的报文并报错
	if (enProtocol >= (INT)(sizeof(lr_staProtocol) / sizeof(USHORT)))
	{
		if (penErr)
			*penErr = ERRUNKNOWNPROTOCOL;

		return -1;
	}

	//* 获取ppp接口控制块
	INT nPPPIdx; 
	PSTCB_PPP pstcbNetif = get_ppp(hTTY, &nPPPIdx);
	if (!pstcbNetif)
	{
		if(penErr)
			*penErr = ERRTTYHANDLE; 
		return -1; 
	}

	//* 填充首部数据
	UCHAR ubHead[sizeof(ST_PPP_HDR)] = { PPP_FLAG } ;
	USHORT usDataLen; 
	if (LCP != enProtocol && pstcbNetif->pstNegoResult->stLCP.blIsNegoValOfAddrCtlComp)
	{
		if (lr_staProtocol[enProtocol].usType < 0xFF && pstcbNetif->pstNegoResult->stLCP.blIsNegoValOfProtoComp)
		{
			ubHead[1] = ((UCHAR *)&lr_staProtocol[enProtocol].usType)[0];
			usDataLen = 2;
		}
		else
		{
			ubHead[1] = ((UCHAR *)&lr_staProtocol[enProtocol].usType)[1];
			ubHead[2] = ((UCHAR *)&lr_staProtocol[enProtocol].usType)[0];
			usDataLen = 3;
		}
	}
	else
	{
		PST_PPP_HDR pstHdr = (PST_PPP_HDR)ubHead;
		pstHdr->ubAddr = PPP_ALLSTATIONS; 
		pstHdr->ubCtl = PPP_UI;
		pstHdr->usProtocol = ENDIAN_CONVERTER_USHORT(lr_staProtocol[enProtocol].usType);
		usDataLen = sizeof(ST_PPP_HDR); 
	}

	//* 申请一个buf节点，将ppp帧头部数据挂载到链表头部
	SHORT sPPPHeadNode, sPPPTailNode;
	sPPPHeadNode = buf_list_get_ext(ubHead, usDataLen, penErr);
	if (sPPPHeadNode < 0)
		return -1;
	buf_list_put_head(&sBufListHead, sPPPHeadNode);

	//* 申请一个buf节点，将ppp帧尾部数据（包括校验和及尾部标志）挂载到链表尾部
	ST_PPP_TAIL stTail;
	stTail.usFCS = ppp_fcs16_ext(sBufListHead);
	stTail.ubDelimiter = PPP_FLAG; 
	sPPPTailNode = buf_list_get_ext(&stTail, sizeof(ST_PPP_TAIL), penErr);
	if (sPPPTailNode < 0)
	{
		buf_list_free(sPPPHeadNode);
		return -1;
	}
	buf_list_put_tail(sBufListHead, sPPPTailNode);

	//* 完成实际的发送
	INT nRtnVal; 
	os_thread_mutex_lock(l_haMtxTTY[nPPPIdx]);
	{
		nRtnVal = tty_send_ext(hTTY, pstcbNetif->pstNegoResult->stLCP.unACCM, sBufListHead, penErr);
	}	
	os_thread_mutex_unlock(l_haMtxTTY[nPPPIdx]);

#if SUPPORT_PRINTF
	#if DEBUG_LEVEL
        #if PRINTF_THREAD_MUTEX
        os_thread_mutex_lock(o_hMtxPrintf);
        #endif
        printf("sent %d bytes: \r\n", nRtnVal);
	    printf_hex_ext(sBufListHead, 48); 
        #if PRINTF_THREAD_MUTEX
        os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
	#endif
#endif

	//* 释放缓冲区节点
	buf_list_free(sPPPHeadNode);
	buf_list_free(sPPPTailNode);

	return nRtnVal; 
}

static INT netif_send(PST_NETIF pstIf, EN_NPSPROTOCOL enProtocol, SHORT sBufListHead, EN_ONPSERR *penErr)
{
    HTTY hTTY = *((HTTY *)pstIf->pvExtra);
    return ppp_send(hTTY, enProtocol, sBufListHead, penErr);
}

//* 将当前ppp链路作为网卡添加到协议栈
static BOOL netif_add_ppp(INT nPPPIdx, PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
    ST_IPV4 stIPv4; 
    stIPv4.unAddr = pstcbPPP->pstNegoResult->stIPCP.unAddr; 
    stIPv4.unSubnetMask = pstcbPPP->pstNegoResult->stIPCP.unSubnetMask; 
    stIPv4.unGateway = pstcbPPP->pstNegoResult->stIPCP.unPointToPointAddr; 
    stIPv4.unPrimaryDNS = pstcbPPP->pstNegoResult->stIPCP.unPrimaryDNS;
    stIPv4.unSecondaryDNS = pstcbPPP->pstNegoResult->stIPCP.unSecondaryDNS; 

    CHAR szNetIfName[NETIF_NAME_LEN]; 
    snprintf(szNetIfName, sizeof(szNetIfName), "ppp%d", nPPPIdx); 
#if SUPPORT_PRINTF
    printf("Connect: %s <--> %s\r\n", szNetIfName, lr_pszaTTY[nPPPIdx]);
#endif
    l_pstaNetif[nPPPIdx] = netif_add(NIF_PPP, szNetIfName, &stIPv4, netif_send, &pstcbPPP->hTTY, penErr); 
    if (l_pstaNetif[nPPPIdx])
    {
        if (route_add(&l_pstaNetif[nPPPIdx]->stIf, 0, 0, 0, penErr))
            return TRUE; 
        else
            return FALSE; 
    }
    else
        return FALSE; 
}

static void ppp_fsm(INT nPPPIdx, PSTCB_PPP pstcbPPP, EN_ONPSERR *penErr)
{
	INT nPacketLen;

#if SUPPORT_ECHO
	UINT unLastSndEchoReq = 0; 
#endif

	while (l_blIsRunning)
	{
		//* 已经连续多次未收到对端应答，则断开PPP链路
		if (pstcbPPP->stWaitAckList.ubTimeoutCount > WAIT_ACK_TIMEOUT_NUM)
		{
	#if SUPPORT_PRINTF	
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
			printf("No response packet received from the peer, ppp stack will redial ...\r\n");
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
	#endif
			goto __lblEnd; 
		}

		//* 接收
		nPacketLen = ppp_recv(nPPPIdx, penErr, 1);
		if (nPacketLen < 0)
		{			
	#if SUPPORT_PRINTF	
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
			printf("ppp_recv() failed, %s, ppp stack will redial ...\r\n", onps_error(*penErr));
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
	#endif
			goto __lblEnd; 
		}

		switch (pstcbPPP->enState)
		{
		case STARTNEGOTIATION:
		case NEGOTIATION: 
		case LCPCONFREQ: 
		case STARTAUTHEN:
		case AUTHENTICATE:
		case SENDIPCPCONFREQ: 
		case WAITIPCPCONFACK:
			ppp_link_establish(pstcbPPP, penErr);
			break;

		case ESTABLISHED:     
			//* 添加到网卡链表
            if (!netif_add_ppp(nPPPIdx, pstcbPPP, penErr))
            {
                pstcbPPP->enState = STACKFAULT;
                break; 
            }
			
		#if SUPPORT_ECHO
			unLastSndEchoReq = os_get_system_secs();
			pstcbPPP->enState = WAITECHOREPLY;
		#endif
			break; 

		case SENDECHOREQ:
		#if SUPPORT_ECHO
			if (lcp_send_echo_request(pstcbPPP, penErr))
			{
				unLastSndEchoReq = os_get_system_secs(); 
                pstcbPPP->enState = WAITECHOREPLY;
			}
			else
			{
		#if SUPPORT_PRINTF		
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif
				printf("lcp_send_echo_request() failed, %s\r\n", onps_error(*penErr)); 
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
		#endif
				os_thread_mutex_lock(l_haMtxTTY[nPPPIdx]);
				{                    
					pstcbPPP->enState = STACKFAULT; 
				}
				os_thread_mutex_unlock(l_haMtxTTY[nPPPIdx]);
			}
		#endif 
			break; 

		case WAITECHOREPLY:
			if (pstcbPPP->stWaitAckList.ubIsTimeout || os_get_system_secs() - unLastSndEchoReq > 60) //* 意味着没收到应答报文
				pstcbPPP->enState = SENDECHOREQ; //* 发送下一次echo request
            else; 
			break;

		case SENDTERMREQ:
			if (lcp_send_terminate_req(pstcbPPP, penErr))
				pstcbPPP->enState = WAITTERMACK;
			else
			{
		#if SUPPORT_PRINTF	
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_lock(o_hMtxPrintf);
            #endif
				printf("lcp_send_terminate_req() failed, %s\r\n", onps_error(*penErr));
            #if PRINTF_THREAD_MUTEX
                os_thread_mutex_unlock(o_hMtxPrintf);
            #endif
		#endif
				pstcbPPP->enState = STACKFAULT;
			}
			break; 

		case WAITTERMACK:
			if(pstcbPPP->stWaitAckList.ubTimeoutCount > 0)
				pstcbPPP->enState = SENDTERMREQ;
			break; 

		case AUTHENFAILED:	//* 认证超时或失败只需重建ppp链路即可
		case AUTHENTIMEOUT:
			goto __lblEnd; 

		case TERMINATED:
			lcp_end_negotiation(pstcbPPP);
			return; 

		case STACKFAULT:
		default:
	#if SUPPORT_PRINTF	
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_lock(o_hMtxPrintf);
        #endif
			printf("error: the ppp stack has a serious failure and needs to be resolved by Neo, %s\r\n", onps_error(*penErr)); 
        #if PRINTF_THREAD_MUTEX
            os_thread_mutex_unlock(o_hMtxPrintf);
        #endif
	#endif
			l_blIsRunning = FALSE; 
			break; 
		}
	}

__lblEnd: 
    //* 如已加入网卡链表则在此应先删除之，否则链表资源将被耗尽
    if (l_pstaNetif[nPPPIdx])
    {
        route_del_ext(&l_pstaNetif[nPPPIdx]->stIf);
        netif_del(l_pstaNetif[nPPPIdx]);
        l_pstaNetif[nPPPIdx] = NULL;
    }

	lcp_end_negotiation(pstcbPPP);

	os_thread_mutex_lock(l_haMtxTTY[nPPPIdx]);
	{
		pstcbPPP->enState = TTYINIT;
	}	
	os_thread_mutex_unlock(l_haMtxTTY[nPPPIdx]);
}

//* ppp协议处理器
void thread_ppp_handler(void *pvParam)
{
	INT nPPPIdx = (INT)pvParam;
	PSTCB_PPP pstcbPPP = &l_staPPP[nPPPIdx];
	EN_ONPSERR enErr;

	//* 通知ppp协议栈处理线程已经开始运行
	l_ubaThreadExitFlag[nPPPIdx] = FALSE;

	INT nTimeout = 5; 	
	while (l_blIsRunning)
	{
		if (TERMINATED == pstcbPPP->enState)
		{
			os_sleep_secs(1); 
			continue; 
		}

		if (tty_ready(pstcbPPP->hTTY, &enErr))
		{
#if SUPPORT_PRINTF
            printf("modem dial succeeded\r\n");
#endif
			pstcbPPP->enState = STARTNEGOTIATION;
			nTimeout = 5; 
            os_sleep_secs(1); //*延时1秒，确保modem已经完全就绪

			ppp_fsm(nPPPIdx, pstcbPPP, &enErr);
		}
		else
		{
	#if SUPPORT_PRINTF
			printf("tty_ready() failed, %s\r\n", onps_error(enErr));
	#endif
			os_sleep_secs(nTimeout);
			if(nTimeout < 60)
				nTimeout += 5; 			
		}
	}

	//* 通知ppp协议栈处理线程已经安全退出
	l_ubaThreadExitFlag[nPPPIdx] = TRUE;
}

void ppp_link_terminate(INT nPPPIdx)
{
	PSTCB_PPP pstcbPPP = &l_staPPP[nPPPIdx];
	os_thread_mutex_lock(l_haMtxTTY[nPPPIdx]);
	{
		if (ESTABLISHED == pstcbPPP->enState)
			pstcbPPP->enState = SENDTERMREQ;
	}
	os_thread_mutex_unlock(l_haMtxTTY[nPPPIdx]);
}

void ppp_link_recreate(INT nPPPIdx)
{
	PSTCB_PPP pstcbPPP = &l_staPPP[nPPPIdx];
	if (TERMINATED == pstcbPPP->enState)
		pstcbPPP->enState = TTYINIT;
}

static void ppp_ip_recv(PSTCB_PPP pstcbPPP, UCHAR *pubPacket, INT nPacketLen)
{
    pstcbPPP = pstcbPPP;  //* 避免编译器警告
    ip_recv(pubPacket, nPacketLen); 
}

#endif
