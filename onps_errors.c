#include "port/datatype.h"
#define SYMBOL_GLOBALS
#include "onps_errors.h"
#undef SYMBOL_GLOBALS
#include "port/sys_config.h"
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
    { ERRUNKNOWN, "unknown error" }
}; 

const CHAR *onps_error(EN_ONPSERR enErr)
{
	UINT unIndex = (UINT)enErr;
	if (unIndex < sizeof(lr_staErrorList) / sizeof(ST_ONPSERR))
		return lr_staErrorList[unIndex].szDesc;

	return "unrecognized error code";
}
