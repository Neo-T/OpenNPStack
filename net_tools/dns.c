#include "port/datatype.h"
#include "onps_errors.h"
#include "port/sys_config.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "mmu/buddy.h"
#include "onps_utils.h"
#include "onps_input.h"
#include "ip/icmp.h"
#define SYMBOL_GLOBALS
#include "net_tools/dns.h"
#undef SYMBOL_GLOBALS

//* 封装dns查询类报文
static UCHAR *dns_queries_encap(const CHAR *pszDomainName, EN_ONPSERR *penErr)
{

}

in_addr_t dns_query(const CHAR *pszDomainName, EN_ONPSERR *penErr)
{

}
