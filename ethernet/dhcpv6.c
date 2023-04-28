/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#include "onps_errors.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "one_shot_timer.h"
#include "mmu/buf_list.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "netif/netif.h"
#include "ip/ip.h"
#include "ip/udp.h"

#if SUPPORT_IPV6
#define SYMBOL_GLOBALS
#include "ethernet/dhcpv6.h"
#undef SYMBOL_GLOBALS
#include "ip/ipv6_configure.h"

#if SUPPORT_ETHERNET
//* DHCPv6的通讯地址是固定的组播地址，参见[RFC8415] 7.1节：https://www.rfc-editor.org/rfc/rfc8415.html#section-7
static const UCHAR l_ubaDhcpv6McAddr[16] = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02 }; //* FF02::1:2，All_DHCP_Relay_Agents_and_Servers，链路本地范围内的所有DHCPv6服务器及代理

static STCB_DHCPv6_CLIENT l_stcbaDv6Clt[IPV6_ROUTER_NUM]; //* DHCPv6客户端控制块，一个路由关联一个DHCPv6服务器
static CHAR l_bFreeDv6CltList = -1; 

//* DHCPv6客户端控制块相关存储单元链表初始化，其必须在启动DHCPv6客户端之前完成该工作
void dhcpv6_client_ctl_block_init(void)
{
	CHAR i;
	for (i = 0; i < IPV6_ROUTER_NUM - 1; i++)
		l_stcbaDv6Clt[i].bNext = i + 1;
	l_stcbaDv6Clt[i].bNext = -1;
	l_bFreeDv6CltList = 0; 
}

PSTCB_DHCPv6_CLIENT dhcpv6_client_node_get(CHAR *pbNodeIdx, EN_ONPSERR *penErr)
{
	PSTCB_DHCPv6_CLIENT pstFreeNode = (PSTCB_DHCPv6_CLIENT)array_linked_list_get(&l_bFreeDv6CltList, l_stcbaDv6Clt, (UCHAR)sizeof(STCB_DHCPv6_CLIENT), offsetof(STCB_DHCPv6_CLIENT, bNext), pbNodeIdx);
	if (!pstFreeNode)
	{
		if (penErr)
			*penErr = ERRNODv6CLTCBNODE;
	}

	return pstFreeNode;
}

void dhcpv6_client_node_free(PSTCB_DHCPv6_CLIENT pstClientNode)
{
	array_linked_list_put(pstClientNode, &l_bFreeDv6CltList, l_stcbaDv6Clt, (UCHAR)sizeof(STCB_DHCPv6_CLIENT), IPV6_ROUTER_NUM, offsetof(STCB_DHCPv6_CLIENT, bNext));
}

PSTCB_DHCPv6_CLIENT dhcpv6_client_get(CHAR bClient)
{
	if (bClient >= 0 && bClient < IPV6_ROUTER_NUM)
		return &l_stcbaDv6Clt[bClient]; 
	else
		return NULL;
}

CHAR dhcpv6_client_get_index(PSTCB_DHCPv6_CLIENT pstClient)
{
	return array_linked_list_get_index(pstClient, l_stcbaDv6Clt, (UCHAR)sizeof(STCB_DHCPv6_CLIENT), IPV6_ROUTER_NUM);
}

PSTCB_DHCPv6_CLIENT dhcpv6_client_find_by_ipv6(PST_NETIF pstNetif, UCHAR ubaRouterAddr[16], PST_IPv6_ROUTER *ppstRouter)
{
	//* 查找匹配的路由器
	PST_IPv6_ROUTER pstRouter = (PST_IPv6_ROUTER)netif_ipv6_router_get_by_addr(pstNetif, ubaRouterAddr); 
	if (ppstRouter)
		*ppstRouter = pstRouter; 

	if (pstRouter)
		return dhcpv6_client_get(pstRouter->bDv6Client);
	else
		return NULL; 
}

//* 利用ethernet网卡的mac地址生成相对固定的IAID
static UINT dhcpv6_iaid_get(PST_NETIF pstNetif)
{
	UINT unIAID = 0x03; 
	((UCHAR *)&unIAID)[1] = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->ubaMacAddr[0]; 
	((UCHAR *)&unIAID)[2] = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->ubaMacAddr[1];
	((UCHAR *)&unIAID)[3] = ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->ubaMacAddr[2];

	return unIAID; 
}

static void dhcpv6_client_timeout_handler(void *pvParam)
{
	PSTCB_DHCPv6_CLIENT pstClient = (PSTCB_DHCPv6_CLIENT)pvParam; 
	PST_IPv6_ROUTER pstRouter = ipv6_router_get(pstClient->bRouter);
	if (!pstRouter)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL		
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("dhcpv6_client_timeout_handler() failed: %s\r\n", onps_error(ERRROUTERINDEX));
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif		
		return;
	}

	UINT unIntervalSecs = 1; 
	EN_ONPSERR enErr = ERRNO;  
	switch (pstClient->bitState)
	{
	case Dv6CLT_SOLICIT: 	
		if (!pstClient->stSrvId.ubSrvIdLen)
		{
			pstClient->bitOptCnt++;
			if (pstClient->bitOptCnt < 6)			
				dhcpv6_send_solicit(pstClient, pstRouter, &enErr); //* 重复发送请求报文
			else
			{
				pstRouter->bitDv6CfgState = Dv6CFG_END;
				dhcpv6_client_stop(pstClient);
				return; 
			}
		}	
		else
		{			
			pstClient->bitRcvReply = FALSE; 
			pstClient->bitOptCnt = 0; 
			pstClient->bitState = Dv6CLT_REQUEST;  
			dhcpv6_send_request(pstClient, pstRouter, DHCPv6MSGTYPE_REQUEST);
		}
		break; 

	case Dv6CLT_REQUEST: 
		if (!pstClient->bitRcvReply)
		{
			pstClient->bitOptCnt++;
			if (pstClient->bitOptCnt < 6)
				dhcpv6_send_request(pstClient, pstRouter, DHCPv6MSGTYPE_REQUEST); //* 重复发送请求报文
			else
			{
				pstRouter->bitDv6CfgState = Dv6CFG_END;
				dhcpv6_client_stop(pstClient); 
				return;
			}
		}
		else
		{
			//* 至此配置完毕
			pstRouter->bitDv6CfgState = Dv6CFG_END; 
			
			if (pstClient->bDynAddr >= 0 && pstRouter->i6r_flag_m)
			{
				//pstClient->unStartTimingCounts = os_get_system_secs(); 
				unIntervalSecs = pstClient->unT1;
				pstClient->unTransId = rand_big(); 
				pstClient->bitState = Dv6CLT_RENEW;
			}
			else
			{
				dhcpv6_client_stop(pstClient); 
				return;
			}
		}
		break; 

	case Dv6CLT_RENEW: 
		dhcpv6_send_request(pstClient, pstRouter, DHCPv6MSGTYPE_RENEW);
		unIntervalSecs = pstClient->unT2 - pstClient->unT1; 
		pstClient->bitState = Dv6CLT_REBIND;
		break;

	case Dv6CLT_REBIND:
		unIntervalSecs = os_get_system_secs() - pstClient->unStartTimingCounts; 
		if (unIntervalSecs < pstClient->unT1)
		{			
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 1     
			CHAR szIpv6[40];
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("The ip address %s of the NIC %s has been successfully renewed.\r\n", inet6_ntoa(pstClient->ubaIAAddr, szIpv6), pstRouter->pstNetif->szName);
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif

			unIntervalSecs = pstClient->unT1 - unIntervalSecs;
			pstClient->bitState = Dv6CLT_RENEW;
		}
		else
		{			
			pstClient->unTransId = rand_big(); 
			dhcpv6_send_request(pstClient, pstRouter, DHCPv6MSGTYPE_REBIND);
			unIntervalSecs = pstClient->unT1 * 2 - pstClient->unT2;
			pstClient->bitState = Dv6CLT_RELEASE;
		}
		
		break; 

	case Dv6CLT_RELEASE:
		unIntervalSecs = os_get_system_secs() - pstClient->unStartTimingCounts;
		if (unIntervalSecs < pstClient->unT1)
		{		
	#if SUPPORT_PRINTF && DEBUG_LEVEL > 1     
			CHAR szIpv6[40];
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_lock(o_hMtxPrintf);
		#endif
			printf("The ip address %s of the NIC %s has been successfully rebound.\r\n", inet6_ntoa(pstClient->ubaIAAddr, szIpv6), pstRouter->pstNetif->szName);
		#if PRINTF_THREAD_MUTEX
			os_thread_mutex_unlock(o_hMtxPrintf);
		#endif
	#endif

			unIntervalSecs = pstClient->unT1 - unIntervalSecs;
			pstClient->bitState = Dv6CLT_RENEW;
		}
		else
		{
			unIntervalSecs = 3;	

			//* 释放当前地址，重新申请地址
			pstClient->unTransId = rand_big();
			pstClient->usStatusCode = Dv6SCODE_UNASSIGNED; 						
			pstClient->bitRcvReply = FALSE; 
			pstClient->bitOptCnt = 0; 
			pstClient->bitState = Dv6CLT_RESTART; 
			dhcpv6_send_request(pstClient, pstRouter, DHCPv6MSGTYPE_RELEASE);
		}
		break; 

	case Dv6CLT_RESTART: 
		if (Dv6SCODE_SUCCESS != pstClient->usStatusCode)
		{
			pstClient->bitOptCnt++; 
			if (pstClient->bitOptCnt < 3)
			{
				unIntervalSecs = 3;				
				dhcpv6_send_request(pstClient, pstRouter, DHCPv6MSGTYPE_RELEASE); 
				break; 
			}
		}

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
		CHAR szIpv6[40];
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("The ip address %s of the NIC %s has been released.\r\n\r\n", inet6_ntoa(pstClient->ubaIAAddr, szIpv6), pstRouter->pstNetif->szName);
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif

		pstClient->stSrvId.ubSrvIdLen = 0;
		pstClient->ubaIAAddr[0] = 0;			
		pstClient->bitUnicast = 0; //* 客户端发送的报文均缺省组播发送
		pstClient->bitOptCnt = 0; 			
		pstClient->unTransId = rand_big();
		pstClient->unStartTimingCounts = os_get_system_msecs();
		pstClient->bitState = Dv6CLT_SOLICIT;
		dhcpv6_send_solicit(pstClient, pstRouter, &enErr); //* 重复发送请求报文

		break; 
	}

	//* 重启定时器
	if (!one_shot_timer_new(dhcpv6_client_timeout_handler, pstClient, unIntervalSecs))
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("Dhcpv6 client timer failed to start, %s\r\n", onps_error(ERRNOIDLETIMER));
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif	
	}

	//* 输出错误信息
#if SUPPORT_PRINTF && DEBUG_LEVEL
	if (ERRNO != enErr)
	{
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("dhcpv6_client_timeout_handler() caught an error, %s\r\n", onps_error(enErr)); 
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
	}
#endif		
}

INT dhcpv6_client_start(PST_IPv6_ROUTER pstRouter, EN_ONPSERR *penErr)
{
	PSTCB_DHCPv6_CLIENT pstClient = NULL; 

	INT nInput = onps_input_new(AF_INET6, IPPROTO_UDP, penErr); 
	if (nInput < 0)
		return nInput; 

	if (onps_input_port_used(AF_INET6, IPPROTO_UDP, DHCPv6_CLT_PORT))
	{
		if (penErr)
			*penErr = ERRPORTOCCUPIED;
		goto __lblErr;
	}

	//* 设置地址
	ST_TCPUDP_HANDLE stHandle;
	stHandle.bFamily = AF_INET6; 
	memcpy(stHandle.stSockAddr.saddr_ipv6, pstRouter->pstNetif->nif_lla_ipv6, 16);
	stHandle.stSockAddr.usPort = DHCPv6_CLT_PORT;
	if (onps_input_set(nInput, IOPT_SETTCPUDPADDR, &stHandle, penErr))
	{
		//* 申请一个客户端控制块
		CHAR bDv6Client; 
		pstClient = dhcpv6_client_node_get(&bDv6Client, penErr);
		if (!pstClient)
			goto __lblErr; 

		pstClient->nInput = nInput;  
		pstClient->stSrvId.ubSrvIdLen = 0; 
		pstClient->usStatusCode = Dv6SCODE_UNASSIGNED;
		pstClient->ubaIAAddr[0] = 0; 
		pstClient->bitState = Dv6CLT_SOLICIT; 
		pstClient->bitUnicast = 0; //* 客户端发送的报文均缺省组播发送
		pstClient->bitOptCnt = 0; 
		pstClient->bDynAddr = -1; 
		pstClient->bRouter = ipv6_router_get_index(pstRouter);
		pstRouter->bDv6Client = bDv6Client; 

		//* 发送请求报文，搜索DHCPv6服务器
		pstClient->unTransId = rand_big();
		pstClient->unStartTimingCounts = os_get_system_msecs();
		if (dhcpv6_send_solicit(pstClient, pstRouter, penErr) < 0)
			goto __lblErr; 

		//* 建立一个一秒间隔的one-shot定时器，以状态机的方式处理dhcpv6的配置、租用、续租等操作
		if (!one_shot_timer_new(dhcpv6_client_timeout_handler, pstClient, 1))					
			goto __lblErr; 		

		return nInput;
	}

__lblErr:
	if (pstClient)
		dhcpv6_client_node_free(pstClient);
	onps_input_free(nInput);
	return -1;
}

void dhcpv6_client_stop(PSTCB_DHCPv6_CLIENT pstClient)
{
	PST_IPv6_ROUTER pstRouter = ipv6_router_get(pstClient->bRouter);
	if (pstRouter)
		pstRouter->bDv6Client = -1; 

	dhcpv6_client_node_free(pstClient);
	onps_input_free(pstClient->nInput); 
}

static const UCHAR *dhcpv6_duid_ll(PST_NETIF pstNetif, UCHAR *pubDUID)
{
	PST_DUID_LL_ETH pstDUID = (PST_DUID_LL_ETH)pubDUID; 
	pstDUID->usType = htons(DUID_LL); 
	pstDUID->usHardwareType = htons(DHW_ETHERNET); 
	memcpy(pstDUID->ubaMacAddr, ((PST_NETIFEXTRA_ETH)pstNetif->pvExtra)->ubaMacAddr, ETH_MAC_ADDR_LEN); 

	return pubDUID; 
}

static INT dhcpv6_send(PSTCB_DHCPv6_CLIENT pstClient, UCHAR ubaDstAddr[16], UCHAR ubMsgType, UCHAR *pubOptions, UINT unOptionsLen, EN_ONPSERR *penErr)
{	
	PST_IPv6_ROUTER pstRouter = ipv6_router_get(pstClient->bRouter); 
	if (!pstRouter)
	{
		if (penErr)
			*penErr = ERRROUTERINDEX;
		return -1;
	}

	UCHAR ubaHdr[sizeof(UNI_DHCPv6_HDR) + sizeof(ST_DHCPv6OPT_ETIME) + sizeof(ST_DHCPv6OPT_DUID_HDR) + sizeof(ST_DUID_LL_ETH)];
	PUNI_DHCPv6_HDR puniHdr = (PUNI_DHCPv6_HDR)ubaHdr;
	PST_DHCPv6OPT_ETIME pstETime = (PST_DHCPv6OPT_ETIME)&ubaHdr[sizeof(UNI_DHCPv6_HDR)]; 
	PST_DHCPv6OPT_DUID_HDR pstDUIDHdr = (PST_DHCPv6OPT_DUID_HDR)&ubaHdr[sizeof(UNI_DHCPv6_HDR) + sizeof(ST_DHCPv6OPT_ETIME)];
	
	//* 申请一个buf list节点用于挂载数据
	SHORT sBufListHead = -1;
	SHORT sOptionsNode = buf_list_get_ext(pubOptions, unOptionsLen, penErr);
	if (sOptionsNode < 0)
		return -1;
	buf_list_put_head(&sBufListHead, sOptionsNode); 

	//* 申请一个buf list节点挂载dhcpv6头
	SHORT sDhcpv6HdrNode = buf_list_get_ext(ubaHdr, (UINT)sizeof(ubaHdr), penErr);
	if (sDhcpv6HdrNode < 0)
	{
		buf_list_free(sOptionsNode);
		return -1;
	}
	buf_list_put_head(&sBufListHead, sDhcpv6HdrNode);

	//* 填充报文头
	puniHdr->stb32.bitMsgType = ubMsgType;
	puniHdr->stb32.bitTransId = pstClient->unTransId;
	puniHdr->unVal = htonl(puniHdr->unVal); 

	//* 填充Elapsed Time Option
	pstETime->stHdr.usCode = htons(DHCPv6OPT_ETIME);
	pstETime->stHdr.usDataLen = htons(sizeof(ST_DHCPv6OPT_ETIME) - sizeof(ST_DHCPv6OPT_HDR)); 
	pstETime->usElapsedTime = pstClient->bitState < Dv6CLT_RENEW ? htons((os_get_system_msecs() - pstClient->unStartTimingCounts) / 100) : 0;

	//* 填充Client Identifier Option
	pstDUIDHdr->stHdr.usCode = htons(DHCPv6OPT_CLTID); 
	pstDUIDHdr->stHdr.usDataLen = htons(sizeof(ST_DHCPv6OPT_DUID_HDR) - sizeof(ST_DHCPv6OPT_HDR) + sizeof(ST_DUID_LL_ETH)); 
	dhcpv6_duid_ll(pstRouter->pstNetif, (UCHAR *)pstDUIDHdr + sizeof(ST_DHCPv6OPT_DUID_HDR));
	//PST_DUID_LL_ETH pstLnkLayerEth = (PST_DUID_LL_ETH)((UCHAR *)pstDUIDHdr + sizeof(ST_DHCPv6OPT_DUID_HDR));
	//pstLnkLayerEth->usHardwareType = DHW_ETHERNET; 
	//memcpy(pstLnkLayerEth->ubaMacAddr, ((PST_NETIFEXTRA_ETH)pstRouter->pstNetif->pvExtra)->ubaMacAddr, ETH_MAC_ADDR_LEN); 

	//* 发送报文
	INT nRtnVal = ipv6_udp_send_ext(pstClient->nInput, sBufListHead, ubaDstAddr, DHCPv6_SRV_PORT, pstRouter->pstNetif->nif_lla_ipv6, pstRouter->pstNetif, penErr); 
	buf_list_free(sOptionsNode);
	buf_list_free(sDhcpv6HdrNode); 

	return nRtnVal; 
}

INT dhcpv6_send_solicit(PSTCB_DHCPv6_CLIENT pstClient, PST_IPv6_ROUTER pstRouter, EN_ONPSERR *penErr)
{
	UCHAR ubaOptions[sizeof(ST_DHCPv6OPT_IANA_HDR) + sizeof(ST_DHCPv6OPT_OROSOL)];	
	PST_DHCPv6OPT_OROSOL pstOROSol; 
	UINT unOptionsLen = 0; 

	//* M标志置位则申请一个地址
	if (pstRouter->i6r_flag_m)
	{
		PST_DHCPv6OPT_IANA_HDR pstIanaHdr = (PST_DHCPv6OPT_IANA_HDR)ubaOptions;
		pstOROSol = (PST_DHCPv6OPT_OROSOL)&ubaOptions[sizeof(ST_DHCPv6OPT_IANA_HDR)]; 

		//* 填充Identity Association for Non-temporary Addresses Option
		pstIanaHdr->stHdr.usCode = htons(DHCPv6OPT_IANA);
		pstIanaHdr->stHdr.usDataLen = htons(sizeof(ST_DHCPv6OPT_IANA_HDR) - sizeof(ST_DHCPv6OPT_HDR));
		pstIanaHdr->unId = dhcpv6_iaid_get(pstRouter->pstNetif);
		pstIanaHdr->unT1 = 0;
		pstIanaHdr->unT2 = 0;

		unOptionsLen = sizeof(ST_DHCPv6OPT_IANA_HDR);
	}	

	pstOROSol = (PST_DHCPv6OPT_OROSOL)&ubaOptions[unOptionsLen];
	unOptionsLen += sizeof(ST_DHCPv6OPT_OROSOL);

	//* 填充Option Request Option for Solicit
	pstOROSol->stHdr.usCode = htons(DHCPv6OPT_ORO);
	pstOROSol->stHdr.usDataLen = htons(sizeof(ST_DHCPv6OPT_OROSOL) - sizeof(ST_DHCPv6OPT_HDR)); 
	pstOROSol->usaOptions[0] = htons(DHCPv6OPT_RDNSSRV);  	

	return dhcpv6_send(pstClient, (UCHAR *)l_ubaDhcpv6McAddr, DHCPv6MSGTYPE_SOLICIT, ubaOptions, unOptionsLen, penErr);
}

INT dhcpv6_send_request(PSTCB_DHCPv6_CLIENT pstClient, PST_IPv6_ROUTER pstRouter, USHORT usMsgType)
{
	UCHAR ubaOptions[sizeof(ST_DHCPv6OPT_DUID_HDR) + DUID_SRVID_LEN_MAX + sizeof(ST_DHCPv6OPT_IANA_HDR) + sizeof(ST_DHCPv6OPT_IAA_HDR) + sizeof(ST_DHCPv6OPT_OROREQ)];
	UINT unOptionsLen = 0;


	if (DHCPv6MSGTYPE_REBIND != usMsgType)
	{
		//* 填充Server Identifier
		PST_DHCPv6OPT_DUID_HDR pstSrvIdHdr = (PST_DHCPv6OPT_DUID_HDR)ubaOptions;
		pstSrvIdHdr->stHdr.usCode = htons(DHCPv6OPT_SRVID);
		pstSrvIdHdr->stHdr.usDataLen = htons((USHORT)pstClient->stSrvId.ubSrvIdLen);
		unOptionsLen = (UINT)sizeof(ST_DHCPv6OPT_DUID_HDR);
		memcpy(&ubaOptions[unOptionsLen], pstClient->stSrvId.ubaVal, pstClient->stSrvId.ubSrvIdLen);
		unOptionsLen += (UINT)pstClient->stSrvId.ubSrvIdLen;
	}	

	//*  M标志置位
	if (pstRouter->i6r_flag_m && pstClient->ubaIAAddr[0])
	{
		//* 填充地址头
		PST_DHCPv6OPT_IANA_HDR pstIANAHdr = (PST_DHCPv6OPT_IANA_HDR)&ubaOptions[unOptionsLen];
		pstIANAHdr->stHdr.usCode = htons(DHCPv6OPT_IANA);
		pstIANAHdr->stHdr.usDataLen = htons(sizeof(ST_DHCPv6OPT_IANA_HDR) - sizeof(ST_DHCPv6OPT_HDR) + sizeof(ST_DHCPv6OPT_IAA_HDR));
		pstIANAHdr->unId = dhcpv6_iaid_get(pstRouter->pstNetif);
		pstIANAHdr->unT1 = htonl(pstClient->unT1);
		pstIANAHdr->unT2 = htonl(pstClient->unT2);
		unOptionsLen += (UINT)sizeof(ST_DHCPv6OPT_IANA_HDR);

		//* 填充地址
		PST_DHCPv6OPT_IAA_HDR pstIAAHdr = (PST_DHCPv6OPT_IAA_HDR)&ubaOptions[unOptionsLen];
		pstIAAHdr->stHdr.usCode = htons(DHCPv6OPT_IAA);
		pstIAAHdr->stHdr.usDataLen = htons(sizeof(ST_DHCPv6OPT_IAA_HDR) - sizeof(ST_DHCPv6OPT_HDR));
		memcpy(pstIAAHdr->ubaIpv6Addr, pstClient->ubaIAAddr, 16); 
		pstIAAHdr->unPreferredLifetime = htonl(2 * pstClient->unT1);
		pstIAAHdr->unValidLifetime = htonl(4 * pstClient->unT1);
		unOptionsLen += (UINT)sizeof(ST_DHCPv6OPT_IAA_HDR);
	}

	//* 填充Option Request Option for Request
	PST_DHCPv6OPT_OROREQ pstOROReq = (PST_DHCPv6OPT_OROREQ)&ubaOptions[unOptionsLen]; 	
	pstOROReq->stHdr.usCode = htons(DHCPv6OPT_ORO);
	pstOROReq->stHdr.usDataLen = htons(sizeof(ST_DHCPv6OPT_OROSOL) - sizeof(ST_DHCPv6OPT_HDR));
	pstOROReq->usaOptions[0] = htons(DHCPv6OPT_RDNSSRV);
	unOptionsLen += (UINT)sizeof(ST_DHCPv6OPT_OROREQ);

	EN_ONPSERR enErr; 
	INT nRtnVal; 
	if(pstClient->bitUnicast)
		nRtnVal = dhcpv6_send(pstClient, pstRouter->pstNetif->nif_lla_ipv6, usMsgType, ubaOptions, unOptionsLen, &enErr);
	else
		nRtnVal = dhcpv6_send(pstClient, (UCHAR *)l_ubaDhcpv6McAddr, usMsgType, ubaOptions, unOptionsLen, &enErr);

	if (nRtnVal < 0)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL	
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("dhcpv6_send_request() failed, %s\r\n", onps_error(enErr));
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif
	}

	return nRtnVal; 
}

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
static void dhcpv6_print_status_info(USHORT usCode, UCHAR ubaSrcAddr[16])
{
	if (Dv6SCODE_SUCCESS == usCode)
		return; 

	CHAR szIpv6[40]; 

#if PRINTF_THREAD_MUTEX
	os_thread_mutex_lock(o_hMtxPrintf);
#endif
	printf("Server %s reported an error: ", inet6_ntoa(ubaSrcAddr, szIpv6)); 
#if PRINTF_THREAD_MUTEX
	os_thread_mutex_unlock(o_hMtxPrintf);
#endif

	switch (usCode)
	{
	case Dv6SCODE_UNSPECFAIL: 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("Failure with unspecified reason."); 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
		break; 

	case Dv6SCODE_NOADDRS: 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("No addresses available to assign to the IA(s)."); 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
		break; 

	case Dv6SCODE_NOBINDING: 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("Client record (binding) unavailable.");
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
		break;

	case Dv6SCODE_NOTONLINK:
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("The prefix for the address is not appropriate for the link to which the client is attached.");
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
		break;

	case DV6SCODE_USEMULTICAST: 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("Please use the All_DHCP_Relay_Agents_and_Servers multicast address."); 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
		break; 

	case Dv6SCODE_NOPREFIX: 
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("No prefixes available to assign to the IA_PD(s).");
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
		break;

	default:
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
#endif
		printf("Unassigned error.");
#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
#endif
		break; 
	}
}
#endif

static void dhcpv6_iana_handler(PST_NETIF pstNetif, PSTCB_DHCPv6_CLIENT pstClient, UCHAR ubaSrcAddr[16], UCHAR *pubIANA, USHORT usIANALen)
{
	PST_DHCPv6OPT_IANA_HDR pstIANAHdr = (PST_DHCPv6OPT_IANA_HDR)pubIANA; 

	//* IAID匹配才可
	if (pstIANAHdr->unId != dhcpv6_iaid_get(pstNetif))
		return; 

	UCHAR *pubSubOptions = pubIANA + sizeof(ST_DHCPv6OPT_IANA_HDR);
	USHORT usHandleOptBytes = 0, usDataLen; 
	while (usHandleOptBytes < usIANALen - (sizeof(ST_DHCPv6OPT_IANA_HDR) - sizeof(ST_DHCPv6OPT_HDR)))
	{		
		PST_DHCPv6OPT_IAA_HDR pstIAAHdr = (PST_DHCPv6OPT_IAA_HDR)(pubSubOptions + usHandleOptBytes);

		usDataLen = htons(pstIAAHdr->stHdr.usDataLen);
		if (DHCPv6OPT_IAA == htons(pstIAAHdr->stHdr.usCode))
		{					
			UINT unValidLifetime = htonl(pstIAAHdr->unValidLifetime);
			UINT unPreferredLifetime = htonl(pstIAAHdr->unPreferredLifetime);

			//* 有效生存期一定不能小于选用生存期
			if (unValidLifetime >= unPreferredLifetime)
			{
				USHORT usCode = Dv6SCODE_SUCCESS;

				//* 先看是否存在Status Code Option，如果存在则确定操作是否成功
				if (usDataLen >= sizeof(ST_DHCPv6OPT_IAA_HDR) - sizeof(ST_DHCPv6OPT_HDR) + sizeof(ST_DHCPv6OPT_SCODE_HDR))
				{
					PST_DHCPv6OPT_SCODE_HDR pstSCode = (PST_DHCPv6OPT_SCODE_HDR)((UCHAR *)pstIAAHdr + sizeof(ST_DHCPv6OPT_IAA_HDR));
					if (DHCPv6OPT_SCODE == htons(pstSCode->stHdr.usCode))
					{
						usCode = htons(pstSCode->usCode); 
				#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
						dhcpv6_print_status_info(usCode, ubaSrcAddr); 
				#endif
					}
				}

				//* 状态码未SUCCESS才可作为有效地址租用
				if (Dv6SCODE_SUCCESS == usCode/* && NULL == netif_ipv6_dyn_addr_get(pstNetif, pstIAAHdr->ubaIpv6Addr, TRUE)*/)
				{
					//* 保存租用时间，续租、释放都需要这个时间
					pstClient->unT1 = htonl(pstIANAHdr->unT1); 
					pstClient->unT2 = htonl(pstIANAHdr->unT2); 

					memcpy(pstClient->ubaIAAddr, pstIAAHdr->ubaIpv6Addr, 16); 
					return; 
				}
			}
			else
			{
		#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_lock(o_hMtxPrintf);
			#endif
				printf("The valid lifetime (%d) is less than the preferred lifetime (%d).\r\n", unValidLifetime, unPreferredLifetime);
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_unlock(o_hMtxPrintf);
			#endif
		#endif				
			}			
		}

		usHandleOptBytes += usDataLen + sizeof(ST_DHCPv6OPT_HDR); 
	}	
}

static void dhcpv6_add_rdnssrv(PST_IPv6_ROUTER pstRouter, UCHAR *pubOptions, USHORT usOptionsLen)
{
	//* 如果已经存在则不再处理
	if (pstRouter->staDNSSrv[0].ubaAddr[0] && pstRouter->staDNSSrv[1].ubaAddr[0])
		return; 

	USHORT usHandleAddrBytes = 0; 
	UCHAR *pubAddr = pubOptions + sizeof(ST_DHCPv6OPT_HDR);
	while (usHandleAddrBytes < usOptionsLen)
	{
		if (pstRouter->staDNSSrv[0].ubaAddr[0])
		{
			//* 不匹配，则保存之
			if (memcmp(pstRouter->staDNSSrv[0].ubaAddr, pubAddr + usHandleAddrBytes, 16))
			{
				//* 只保存两个DNS服务器
				memcpy(pstRouter->staDNSSrv[1].ubaAddr, pubAddr + usHandleAddrBytes, 16);
				pstRouter->staDNSSrv[1].unLifetime = 0xFFFFFFFF;
				return;				
			}
		}
		else
		{
			memcpy(pstRouter->staDNSSrv[0].ubaAddr, pubAddr + usHandleAddrBytes, 16);
			pstRouter->staDNSSrv[0].unLifetime = 0xFFFFFFFF;
		}		

		usHandleAddrBytes += 16; 
	}
}

/*
static BOOL dhcpv6_client_id_matched(PST_NETIF pstNetif, PST_DUID_LL_ETH pstCltId, UCHAR ubCltIdLen)
{
	ST_DUID_LL_ETH stCltId;

	// 长度相等才比较
	if (sizeof(ST_DUID_LL_ETH) == ubCltIdLen)
	{
		if (!memcmp((const UCHAR *)pstCltId, dhcpv6_duid_ll(pstNetif, (UCHAR *)&stCltId), ubCltIdLen))
			return TRUE;
	}

	return FALSE;
}
*/

static BOOL dhcpv6_is_client_id_matched(PST_NETIF pstNetif, UCHAR *pubOptions, USHORT usOptionsLen)
{
	USHORT usHandleOptBytes = 0, usDataLen;
	while (usHandleOptBytes < usOptionsLen)
	{
		PST_DHCPv6OPT_HDR pstHdr = (PST_DHCPv6OPT_HDR)(pubOptions + usHandleOptBytes);
		usDataLen = htons(pstHdr->usDataLen);
		if (DHCPv6OPT_CLTID == htons(pstHdr->usCode))
		{
			ST_DUID_LL_ETH stCltId; 
			if (sizeof(ST_DUID_LL_ETH) == usDataLen) //* 长度相等才比较，否则直接认为不匹配
			{
				if (!memcmp((const UCHAR *)pstHdr + sizeof(ST_DHCPv6OPT_HDR), dhcpv6_duid_ll(pstNetif, (UCHAR *)&stCltId), sizeof(ST_DUID_LL_ETH)))
					return TRUE;
			}

			break; 
		}

		usHandleOptBytes += usDataLen + sizeof(ST_DHCPv6OPT_HDR); 
	}

	return FALSE; 
}

static BOOL dhcpv6_is_server_id_matched(PSTCB_DHCPv6_CLIENT pstClient, UCHAR *pubOptions, USHORT usOptionsLen)
{
	USHORT usHandleOptBytes = 0, usDataLen;
	while (usHandleOptBytes < usOptionsLen)
	{
		PST_DHCPv6OPT_HDR pstHdr = (PST_DHCPv6OPT_HDR)(pubOptions + usHandleOptBytes);
		usDataLen = htons(pstHdr->usDataLen);
		if (DHCPv6OPT_SRVID == htons(pstHdr->usCode))
		{			
			if ((USHORT)pstClient->stSrvId.ubSrvIdLen == usDataLen) //* 长度相等才比较，否则直接认为不匹配
			{
				if (!memcmp((const UCHAR *)pstHdr + sizeof(ST_DHCPv6OPT_HDR), pstClient->stSrvId.ubaVal, usDataLen))
					return TRUE;
			}

			break;
		}

		usHandleOptBytes += usDataLen + sizeof(ST_DHCPv6OPT_HDR);
	}

	return FALSE;
}

static void dhcpv6_advertise_handler(PST_IPv6_ROUTER pstRouter, PSTCB_DHCPv6_CLIENT pstClient, UCHAR ubaSrcAddr[16], UCHAR *pubOptions, USHORT usOptionsLen)
{
	if (!dhcpv6_is_client_id_matched(pstRouter->pstNetif, pubOptions, usOptionsLen))
		return; 

	USHORT usHandleOptBytes = 0, usDataLen; 
	while (usHandleOptBytes < usOptionsLen)
	{
		PST_DHCPv6OPT_HDR pstHdr = (PST_DHCPv6OPT_HDR)(pubOptions + usHandleOptBytes); 
		usDataLen = htons(pstHdr->usDataLen); 
		switch (htons(pstHdr->usCode))
		{
		//case DHCPv6OPT_CLTID: 
		//	//* Client Id不匹配说明这不是给当前主机的，直接丢弃该报文
		//	if (!dhcpv6_client_id_matched(pstRouter->pstNetif, (PST_DUID_LL_ETH)(pubOptions + usHandleOptBytes + sizeof(ST_DHCPv6OPT_HDR)), (UCHAR)usDataLen))
		//		return; 			
		//	break; 

		case DHCPv6OPT_SRVID: 
			if (usDataLen <= DUID_SRVID_LEN_MAX)
			{
				pstClient->stSrvId.ubSrvIdLen = (UCHAR)usDataLen; 
				memcpy(pstClient->stSrvId.ubaVal, pubOptions + usHandleOptBytes + sizeof(ST_DHCPv6OPT_HDR), usDataLen);
			}
			else //* 大于当前存储空间就直接抛弃当前报文，理论上只有采用DUID_EN类型的客户端Id才有可能超出（可变的企业标识设置过长时）
			{
		#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
				CHAR szIpv6[40];
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_lock(o_hMtxPrintf);
			#endif
				printf("The id advertised by DHCPv6 server [%s] exceeds the onps limit of %d bytes.\r\n", inet6_ntoa(ubaSrcAddr, szIpv6), DUID_SRVID_LEN_MAX);
			#if PRINTF_THREAD_MUTEX
				os_thread_mutex_unlock(o_hMtxPrintf);
			#endif
		#endif
				return;
			}
			break; 

		case DHCPv6OPT_IANA: 
			if(pstRouter->i6r_flag_m)
				dhcpv6_iana_handler(pstRouter->pstNetif, pstClient, ubaSrcAddr, (UCHAR *)pstHdr, usDataLen);
			break; 

		//case DHCPv6OPT_RDNSSRV: 
		//	dhcpv6_add_rdnssrv(pstRouter, (UCHAR *)pstHdr, usDataLen);
		//	break; 

		case DHCPv6OPT_UNICAST: 
			//memcpy(pstClient->ubaSrvAddr, ((PST_DHCPv6OPT_UNICAST)pstHdr)->ubaSrvAddr, 16);  //* 取消，单播通讯仅支持源路由器链路本地地址，这个地址在icmpv6的RA报文中已经获取
			pstClient->bitUnicast = TRUE; 
			break; 

		default: 
			break; 
		}

		usHandleOptBytes += usDataLen + sizeof(ST_DHCPv6OPT_HDR); 
	}	
}


static void dhcpv6_reply_handler(PST_IPv6_ROUTER pstRouter, PSTCB_DHCPv6_CLIENT pstClient, UCHAR ubaSrcAddr[16], UCHAR *pubOptions, USHORT usOptionsLen)
{
	//* 服务器与客户端标识必须完全匹配才可
	if (!dhcpv6_is_client_id_matched(pstRouter->pstNetif, pubOptions, usOptionsLen) 
		|| !dhcpv6_is_server_id_matched(pstClient, pubOptions, usOptionsLen))
		return;

	USHORT usHandleOptBytes = 0, usDataLen;
	while (usHandleOptBytes < usOptionsLen)
	{
		PST_DHCPv6OPT_HDR pstHdr = (PST_DHCPv6OPT_HDR)(pubOptions + usHandleOptBytes);
		usDataLen = htons(pstHdr->usDataLen); 
		if (DHCPv6OPT_IANA == htons(pstHdr->usCode))
		{
			//* 只有M标志置位才可
			if (pstRouter->i6r_flag_m)
			{
				PST_DHCPv6OPT_IAA_HDR pstIAAHdr = (PST_DHCPv6OPT_IAA_HDR)((UCHAR *)pstHdr + sizeof(ST_DHCPv6OPT_IANA_HDR));

				UINT unValidLifetime = htonl(pstIAAHdr->unValidLifetime);
				UINT unPreferredLifetime = htonl(pstIAAHdr->unPreferredLifetime);

				if (Dv6CLT_SOLICIT < pstClient->bitState < Dv6CLT_RESTART)
				{
					PST_IPv6_DYNADDR pstDynAddr = ipv6_dyn_addr_get(pstClient->bDynAddr);
					if (!pstDynAddr)
					{
						EN_ONPSERR enErr;
						pstDynAddr = ipv6_dyn_addr_node_get(&pstClient->bDynAddr, &enErr);
						if (pstDynAddr)
						{							
							memcpy(pstDynAddr->ubaVal, pstIAAHdr->ubaIpv6Addr, 16);
							pstDynAddr->bitRouter = pstClient->bRouter;
							pstDynAddr->bitPrefixBitLen = Dv6CFGADDR_PREFIX_LEN;
							pstDynAddr->unValidLifetime = unValidLifetime ? unValidLifetime + IPv6ADDR_INVALID_TIME : IPv6ADDR_INVALID_TIME + 1; 
							pstDynAddr->unPreferredLifetime = unPreferredLifetime;
							pstDynAddr->bitState = IPv6ADDR_PREFERRED;
							netif_ipv6_dyn_addr_add(pstRouter->pstNetif, pstDynAddr);

							pstClient->unStartTimingCounts = os_get_system_secs();														
						}
						else
						{
					#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
						#if PRINTF_THREAD_MUTEX
							os_thread_mutex_lock(o_hMtxPrintf);
						#endif
							printf("dhcpv6_reply_handler() failed, %s\r\n", onps_error(enErr));
						#if PRINTF_THREAD_MUTEX
							os_thread_mutex_unlock(o_hMtxPrintf);
						#endif
					#endif
						}
					}
					else
					{
						os_critical_init();
						os_enter_critical();
						{
							memcpy(pstDynAddr->ubaVal, pstIAAHdr->ubaIpv6Addr, 16);
							pstDynAddr->unValidLifetime = unValidLifetime;
							pstDynAddr->unPreferredLifetime = unPreferredLifetime;

							if (unPreferredLifetime && pstDynAddr->bitState > IPv6ADDR_PREFERRED)
								pstDynAddr->bitState = IPv6ADDR_PREFERRED; //* 再次调整为地址“可用”状态
						}
						os_exit_critical();
												
						pstClient->unStartTimingCounts = os_get_system_secs();												
					}
				}
			}			
		}
		else if (DHCPv6OPT_RDNSSRV == htons(pstHdr->usCode))
		{
			dhcpv6_add_rdnssrv(pstRouter, (UCHAR *)pstHdr, usDataLen); 
		}
		else if (DHCPv6OPT_SCODE == htons(pstHdr->usCode))
		{
			PST_DHCPv6OPT_SCODE_HDR pstSCodeHdr = (PST_DHCPv6OPT_SCODE_HDR)pstHdr;
			pstClient->usStatusCode = htons(pstSCodeHdr->usCode); 

#if SUPPORT_PRINTF && DEBUG_LEVEL > 1
			if(Dv6SCODE_SUCCESS != pstClient->usStatusCode)
				dhcpv6_print_status_info(pstClient->usStatusCode, ubaSrcAddr);
#endif
		}
		else; 

		usHandleOptBytes += usDataLen + sizeof(ST_DHCPv6OPT_HDR); 
	}	
	
	pstClient->bitRcvReply = TRUE; 
}

void dhcpv6_recv(PST_NETIF pstNetif, UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UCHAR *pubDHCPv6, USHORT usDHCPv6Len)
{
	PST_IPv6_ROUTER pstRouter = NULL; 

	//* 确定是否存在该客户端
	PSTCB_DHCPv6_CLIENT pstClient = (PSTCB_DHCPv6_CLIENT)dhcpv6_client_find_by_ipv6(pstNetif, ubaSrcAddr, &pstRouter);
	if (NULL == pstClient)
	{
#if SUPPORT_PRINTF && DEBUG_LEVEL > 1		
		CHAR szIpv6[40]; 
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_lock(o_hMtxPrintf);
	#endif
		printf("No matching DHCPv6 client found (DHCPv6 server: %s to client: \r\n", inet6_ntoa(ubaSrcAddr, szIpv6)); 
		printf("%s)\r\n", inet6_ntoa(ubaDstAddr, szIpv6)); 
	#if PRINTF_THREAD_MUTEX
		os_thread_mutex_unlock(o_hMtxPrintf);
	#endif
#endif

		if(pstRouter)			
			netif_ipv6_router_release(pstRouter); //* 释放对路由器的占用

		return; 
	}

	PUNI_DHCPv6_HDR puniHdr = (PUNI_DHCPv6_HDR)pubDHCPv6; 
	puniHdr->unVal = htonl(puniHdr->unVal); 

	//* 事务ID匹配才处理，如果不匹配则直接丢弃当前报文
	if ((pstClient->unTransId & 0x00FFFFFF) == puniHdr->stb32.bitTransId)
	{
		switch (puniHdr->stb32.bitMsgType)
		{
		case DHCPv6MSGTYPE_ADVERTISE:
			dhcpv6_advertise_handler(pstRouter, pstClient, ubaSrcAddr, pubDHCPv6 + sizeof(UNI_DHCPv6_HDR), usDHCPv6Len - sizeof(UNI_DHCPv6_HDR));
			break;

		case DHCPv6MSGTYPE_REPLY: 
			dhcpv6_reply_handler(pstRouter, pstClient, ubaSrcAddr, pubDHCPv6 + sizeof(UNI_DHCPv6_HDR), usDHCPv6Len - sizeof(UNI_DHCPv6_HDR));
			break;

		default:
			break;
		}
	}	

	//* 释放对路由器的占用
	netif_ipv6_router_release(pstRouter); 
}

#endif
#endif
