/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 */
#include "port/datatype.h"
#include "port/sys_config.h"
#define SYMBOL_GLOBALS
#include "onps_errors.h"
#undef SYMBOL_GLOBALS
#include "port/os_datatype.h"
#include "port/os_adapter.h"

static const ST_ONPSERR lr_staErrorList[] = {
	{ ERRNO, "no errors" },
	{ ERRNOPAGENODE, "no page nodes available"},
	{ ERRREQMEMTOOLARGE, "The requested memory is too large, please refer to the macro definition BUDDY_MEM_SIZE" },
	{ ERRNOFREEMEM, "The mmu has no memory available" }, 
	{ ERRMUTEXINITFAILED, "thread mutex initialization failed" }, 
	{ ERRNOBUFLISTNODE, "the buffer list is empty" }, 
	{ ERRSEMINITFAILED, "thread semphore initialization failed" }, 
	{ ERROPENTTY, "tty open error" }, 
	{ ERRATWRITE, "write AT command error" }, 
	{ ERRATEXEC, "the at command returned an error" }, 
	{ ERRATEXECTIMEOUT, "AT command exec timeout" }, 
	{ ERRSIMCARD, "no sim card detected" }, 
	{ ERRREGMOBILENET, "Unable to register to mobile network" }, 
    { ERRPPPIDXOVERFLOW, "ppp link index overflow" }, 
	{ ERRPPPDELIMITER, "ppp frame delimiter not found" }, 
	{ ERRTOOMANYTTYS, "too many ttys" }, 
	{ ERRTTYHANDLE, "invalid tty handle" }, 
	{ ERROSADAPTER, "os adaptation layer error" }, 
	{ ERRUNKNOWNPROTOCOL, "Unknown protocol type" }, 
	{ ERRPPPFCS, "ppp frame checksum error" }, 
	{ ERRNOIDLETIMER, "no idle timer" }, 
    { ERRNOFREEPPWANODE, "Node unavailable for ppp negotiation wait" }, 
	{ ERRPPPWALISTNOINIT, "ppp's waiting list for ack is not initialized" }, 
    { ERRNONETIFNODE, "no netif nodes available" }, 
    { ERRNONETIFFOUND, "No network interface found" }, 
    { ERRINPUTOVERFLOW, "Handle/Input overflow" }, 
    { ERRUNSUPPIPPROTO, "Unsupported ip upper layer protocol" }, 
    { ERRUNSUPPIOPT, "Unsupported control options" }, 
    { ERRIPROTOMATCH, "Protocol match error" }, 
    { ERRNOROUTENODE, "no route nodes available" }, 
    { ERRADDRESSING,"Addressing failure, default route does not exist" }, 
    { ERRADDRFAMILIES, "Unsupported address families" }, 
    { ERRSOCKETTYPE, "Unsupported socket type" }, 
    { ERRNOATTACH, "Attached data address is null" }, 
    { ERRTCSNONTCP, "Non-TCP can't get/set tcp link state" },
    { ERRTDSNONTCP, "Non-TCP can't get tcp send data state" }, 
    { ERRTCPCONNTIMEOUT, "tcp connection timeout" }, 
    { ERRTCPCONNRESET, "tcp connection reset by peer" },
    { ERRTCPCONNCLOSED, "TCP link closed" }, 
    { ERRDATAEMPTY, "tcp/udp data segment is empty" }, 
    { ERRTCPACKTIMEOUT, "tcp ack timeout" }, 
    { ERRNOTCPLINKNODE, "the tcp link list is empty" }, 
    { ERRTCPNOTCONNECTED, "tcp link not connected" }, 
    { ERRINVALIDSEM, "invalid semaphore" }, 
    { ERRSENDZEROBYTES, "0 bytes of data are sent" }, 
    { ERRPORTOCCUPIED, "Port is already occupied" }, 
    { ERRSENDADDR, "destination address is empty" }, 
    { ERRETHEXTRAEMPTY, "No eth additional info node available" }, 
    { ERRNETUNREACHABLE, "Network unreachable" }, 
    { ERRROUTEADDRMATCH, "Addressing result does not match" }, 
    { ERRNEWARPCTLBLOCK, "arp control block is empty" }, 
    { ERRUNSUPPETHIIPROTO, "unsupported ethernet ii upper layer protocol(only ipv4/arp/rarp/ipv6)" }, 
    { ERRPACKETTOOLARGE, "packet too large" }, 
    { ERRPORTEMPTY, "port number is empty" }, 
    { ERRWAITACKTIMEOUT, "Timeout waiting for the reply packet" }, 
    { ERRIPCONFLICT, "ip address conflict" }, 
    { ERRNOTBINDADDR, "The socket does not bind the address" }, 
    { ERRTCPONLY, "Only supports tcp protocol" }, 
    { ERRTCPSRVEMPTY, "tcp server resource is empty"}, 
    { ERRTCPBACKLOGEMPTY, "backlog resource for tcp server is empty" }, 
    { ERRTCPRCVQUEUEEMPTY, "recv queue resource for tcp server is empty" }, 
    { ERRTCPNOLISTEN, "tcp server does not enter the listen stage" }, 
    { ERRTCPBACKLOGFULL, "backlog resource for tcp server is full" }, 
    { ERRDNSQUERYFMT, "The format of the dns query packet is wrong" },
    { ERRDNSSRV, "dns server failure" }, 
    { ERRDNSNAME, "domain name does not exist" }, 
    { ERRDNSQUERYTYPE, "unsupported dns query type" }, 
	{ ERRDNSREFUSED, "Refused by DNS server" }, 
    { ERRDNSNOTRESOLVED, "not resolved to ip address" }, 
    { ERRNOUDPLINKNODE, "the udp link list is empty" }, 
    { ERRTCPLINKCBNULL, "the tcp link control block is NULL" }, 
#if SUPPORT_IPV6
	{ ERRNEWIPv6MACCTLBLOCK, "The control block of the ipv6 to mac address mapping table is empty" }, 
	{ ERRNOIPv6DYNADDRNODE, "the ipv6 dynamic addr list is empty" }, 
	{ ERRNOIPv6ROUTERNODE, "the ipv6 router list is empty" }, 
	{ ERRIPV4FLOWLABEL, "Ipv4 does not support flow label fields" }, 
	{ ERRNODv6CLTCBNODE, "Dhcpv6 client control block list is empty" }, 
	{ ERRROUTERINDEX, "router index overflow" }, 
#endif
	{ ERRFAMILYINCONSISTENT, "The address family of the target and source addresses is inconsistent" }, 
	{ ERRUNSUPPORTEDFAMILY, "Unsupported address family" }, 
#if NETTOOLS_TELNETSRV
    { ERRNOTELNETCLTCBNODE, "Telnet client control block list is empty" }, 
    { ERRNVTSTART, "Network virtual terminal startup failed" }, 
#endif
    { ERRNETIFNOTFOUND, "no network interface found" }, 
    { ERREXTRAIPLIMIT, "exceeded maximum number of IP addresses" }, 
    { ERREXTRAIPSAVE, "failed to write the IP address to the system memory" }, 
    { ERREXTRAIPDEL, "failed to remove IP address from system memory" }, 
    { ERRIPUPDATED, "unable to overwrite the original value in the system memory" }, 
    { ERRIPNOSTATIC, "adding, deleting, or updating IP addresses is not allowed in DHCP mode" }, 
    { ERRROUTEENTRYNOR, "failed to write to system memory when adding or deleting routing entries" }, 
    { ERRROUTEDEFAULTDEL, "the default route cannot be deleted" }, 
    { ERRROUTEENTRYNOTEXIST, "the routing entry does not exist" }, 
    { ERRUNKNOWN, "unknown error" }
}; 

const CHAR *onps_error(EN_ONPSERR enErr)
{
	UINT unIndex = (UINT)enErr;
	if (unIndex < sizeof(lr_staErrorList) / sizeof(ST_ONPSERR))
		return lr_staErrorList[unIndex].szDesc;

	return "unrecognized error code";
}
