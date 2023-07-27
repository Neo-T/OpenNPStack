/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 系统配置头文件，用户可根据实际情况对协议栈进行裁剪、参数配置等工作
 *
 * Neo-T, 创建于2022.03.11 14:45
 *
 */
#ifndef SYS_CONFIG_H
#define SYS_CONFIG_H

#define SOCKET_NUM_MAX   16 //* 系统支持的最大SOCKET数量，如实际应用中超过这个数量则会导致用户层业务逻辑无法全部正常运行（icmp/tcp/udp业务均受此影响），其值应大于等于TCP_LINK_NUM_MAX值
#define IP_TTL_DEFAULT   64 //* 缺省TTL值

 //* 系统支持哪些功能模块由此配置
 //* ===============================================================================================
#define SUPPORT_IPV6	1   //* 是否支持IPv6：1，支持；0，不支持
#define SUPPORT_PRINTF	1	//* 是否支持调用printf()输出相关调试或系统信息
#if SUPPORT_PRINTF
    #define PRINTF_THREAD_MUTEX 1   //* 是否支持使用printf线程互斥锁，确保不同线程的调试输出信息不被互相干扰，值为1则支持互斥锁
	#define DEBUG_LEVEL         4	//* 共5个调试级别：
                                    //* 0 输出协议栈底层严重错误
                                    //* 1 输出所有系统错误（包括0级错误）
                                    //* 2 输出协议栈重要的配置、运行信息，同时包括0、1级信息
                                    //* 3 输出网卡的原始通讯通讯报文（ppp为收发，ethnernet为发送），以及0、1、2级信息
                                    //* 4 输出ethernet网卡接收的原始通讯报文，被协议栈丢弃的非法（校验和错误、通讯链路不存在等原因）通讯报文，以及0、1、2、3级信息（除ethernet发送的原始报文）
                                    
#endif

#define SUPPORT_PPP		1	//* 是否支持ppp模块：1，支持；0，不支持，如果选择支持，则系统会将ppp模块代码加入到协议栈中
#if SUPPORT_PPP
	#define APN_DEFAULT				"4gnet"		//* 根据实际情况在这里设置缺省APN
	#define AUTH_USER_DEFAULT		"card"		//* ppp认证缺省用户名
	#define AUTH_PASSWORD_DEFAULT	"any_char"	//* ppp认证缺省口令

	#define PPP_NETLINK_NUM			1	//* 协议栈加载几路ppp链路（系统存在几个modem这里就指定几就行）
	#define	SUPPORT_ECHO			1	//* 对端是否支持echo链路探测
	#define WAIT_ACK_TIMEOUT_NUM	5	//* 在这里指定连续几次接收不到对端的应答报文就进入协议栈故障处理流程（STACKFAULT），这意味着当前链路已经因严重故障终止了
#else
    #define PPP_NETLINK_NUM			0
#endif

#define SUPPORT_ETHERNET    1   //* 是否支持ethernet：1，支持；0，不支持
#if SUPPORT_ETHERNET    
    #define ETHERNET_NUM        1   //* 要添加几个ethernet网卡（实际存在几个就添加几个）    
    #define ARPENTRY_NUM        32  //* arp条目缓存表的大小（不要超过127），只要不小于局域网内目标通讯节点的个数即可确保arp寻址次数为1，否则就会出现频繁寻址的可能，当然这也不会妨碍正常通讯逻辑，只不过这会降低通讯效率    
	#if SUPPORT_IPV6
		#define IPV6TOMAC_ENTRY_NUM	8	//* Ipv6到以太网mac地址映射缓存表的大小（不要超过127），这个配置项指定缓存条目的数量，同样确保其不小于Ipv6通讯节点数量即可避免重复寻址的问题
		#define IPV6_CFG_ADDR_NUM	8	//* 指定所有以太网卡能够自动/手动配置的最大地址数量（不要超过128），超过这个数量将无法为网卡配置新的地址，如果目标网络环境地址数量确定建议将该值调整到合适的值以节省内存
		#define IPV6_ROUTER_NUM		4	//* 指定所有以太网卡能够添加的路由器最大数量（最多8个），请根据目标网络实际情况调整该值以最大限度节省内存使用
	#endif
    
    #define ETH_EXTRA_IP_EN  1 //* 是否允许添加多个ip地址
    #if ETH_EXTRA_IP_EN
        #define ETH_EXTRA_IP_NUM 2 //* 允许添加的ip地址数量
    #endif    
#else
    #define ETHERNET_NUM    0
#endif

#define NETIF_NUM		(PPP_NETLINK_NUM + ETHERNET_NUM)    //* 系统支持的网卡数量
#define ROUTE_ITEM_NUM	8									//* Ipv4系统路由表数量
 //* ===============================================================================================

//* ip支持的上层协议相关配置项
//* ===============================================================================================
#define SUPPORT_SACK    1		//* 系统是否支持sack项

#define ICMPRCVBUF_SIZE 128     //* icmp发送echo请求报文时指定的接收缓冲区的大小，注意，如果要发送较大的ping包就必须指定较大的接收缓冲区

#define TCPRCVBUF_SIZE  2048    //* tcp层接收缓冲区大小，大小应是2^n次幂才能最大限度不浪费budyy模块分配的内存
#if SUPPORT_SACK
#define TCPSNDBUF_SIZE  4096    //* tcp层发送缓冲区大小，同接收缓冲区，大小应是2^n次幂才能最大限度不浪费budyy模块分配的内存
#endif

#define TCPUDP_PORT_START       20000   //* TCP/UDP协议动态分配的起始端口号

#define TCP_WINDOW_SCALE        0       //* 窗口扩大因子缺省值
#define TCP_CONN_TIMEOUT        30      //* 缺省TCP连接超时时间
#define TCP_ACK_TIMEOUT         3       //* 缺省TCP应答超时时间
#define TCP_LINK_NUM_MAX        16      //* 系统支持最多建立多少路TCP链路（涵盖所有TCP客户端 + TCP服务器的并发连接数），超过这个数量将无法建立新的tcp链路，另外这个值最大为127，超过则系统无法正常运行
#define TCP_ACK_DELAY_MSECS     100     //* 延迟多少毫秒发送ack报文，这个值最小40毫秒，最大200毫秒

#if SUPPORT_ETHERNET
    #define TCPSRV_BACKLOG_NUM_MAX  10      //* tcp服务器支持的最大请求队列数量，任意时刻所有已开启的tcp服务器的请求连接队列数量之和应小于该值，否则将会出现拒绝连接的情况
    #define TCPSRV_NUM_MAX          0       //* 系统能够同时建立的tcp服务器数量
    #define TCPSRV_RECV_QUEUE_NUM   64      //* tcp服务器接收队列大小，所有已开启的tcp服务器共享该队列资源，如果单位时间内到达所有已开启tcp服务器的报文数量较大，应将该值调大
#endif

#define UDP_LINK_NUM_MAX 4  //* 调用connect()函数连接对端udp服务器的最大数量（一旦调用connect()函数，收到的非服务器报文将被直接丢弃）
//* ===============================================================================================

//* 网络工具配置项
//* ===============================================================================================
#define NETTOOLS_PING       1 //* 使能或禁止ping，置位使能，复位禁止，下同
#define NETTOOLS_DNS_CLIENT 1 //* 使能或禁止dns查询客户端
#define NETTOOLS_SNTP       1 //* 使能或禁止sntp客户端
#define NETTOOLS_TELNETSRV  1 //* 使能或禁止telnet服务端，其值必须为0或1（禁止/使能），因为其还被用于tcp服务器资源分配统计（TCPSRV_NUM_MAX + NETTOOLS_TELNETSRV）

#if NETTOOLS_TELNETSRV
#define NVTCMD_MEMUSAGE_EN 1 //* 使能或禁止nvt命令：memusage
#define NVTCMD_NETIF_EN    1 //* 使能或禁止nvt命令：netif
#define NVTCMD_IFIP_EN     1 //* 使能或禁止nvt命令：ifip
#define NVTCMD_ROUTE_EN    1 //* 使能或禁止nvt命令：route
#define NVTCMD_TELNET_EN   1 //* 使能或禁止nvt命令：telnet

#if NETTOOLS_SNTP
#define NVTCMD_NTP_EN      1 //* 使能或禁止nvt命令：ntp，其必须先使能NETTOOLS_SNTP
#endif //* #if NETTOOLS_SNTP

#if NETTOOLS_DNS_CLIENT
#define NVTCMD_NSLOOKUP_EN      1 //* 使能或禁止nvt命令：nslookup，其必须先使能NETTOOLS_DNS_CLIENT
#endif //* #if NETTOOLS_DNS_CLIENT

#if NETTOOLS_PING
#define NVTCMD_PING_EN     1 //* 使能或禁止nvt命令：ping，使能ping命令时应同时使能NETTOOLS_PING
#endif //* #if NETTOOLS_PING

#define NVTCMD_RESET_EN    1 //* 使能或禁止nvt命令：reset

#if NVTCMD_TELNET_EN
//* telnet客户端接收缓冲区大小，注意关闭TCP SACK选项时，设置的接收缓冲区大小一旦超过过tcp mtu（一般
//* 为1460字节），就必须在用户层限定单个发包的大小不能超过tcp mtu，否则会丢失数据
#define TELNETCLT_RCVBUF_SIZE 1024
#endif //* #if NVTCMD_TELNET_EN

#define NVTNUM_MAX          2   //* 指定nvt并发工作的数量，其实就是指定telnet服务器在同一时刻并发连接的数量，超过这个数值服务器拒绝连接
#define NVTCMDCACHE_EN      1   //* 是否支持命令缓存，也就是通过“↑↓”切换曾经输入的指令
#define NVTCMDCACHE_SIZE    256 //* 指定指令缓存区的大小

#if SUPPORT_IPV6
#define TELNETSRV_SUPPORT_IPv6 1 //* telnet服务器使能或禁止ipv6支持，与NETTOOLS_TELNETSRV同，其值必须为0或1（禁止/使能）
#endif //* #if SUPPORT_IPV6
#endif //* #if NETTOOLS_TELNETSRV
//* ===============================================================================================

//* 内存管理单元(mmu)相关配置项，其直接影响协议栈能分配多少个socket给用户使用
//* ===============================================================================================
#define BUF_LIST_NUM	 80     //* 缓存链表的节点数，最大不能超过2的15次方（32768）
#define BUDDY_PAGE_SIZE  32     //* 系统能够分配的最小页面大小，其值必须是2的整数次幂
#define BUDDY_ARER_COUNT 9      //* 指定buddy算法管理的内存块数组单元数量

#define BUDDY_MEM_SIZE   8192   //* buddy算法管理的内存总大小，其值由BUDDY_PAGE_SIZE、BUDDY_ARER_COUNT两个宏计算得到：
                                //* 32 * (2 ^ (9 - 1))，即BUDDY_MEM_SIZE = BUDDY_PAGE_SIZE * (2 ^ (BUDDY_ARER_COUNT - 1))
                                //* 之所以在此定义好要管理的内存大小，原因是buddy管理的内存其实就是一块提前分配好的静态存储
                                //* 时期的字节型一维数组，以确保协议栈不占用宝贵的堆空间
//* ===============================================================================================

#endif
