/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 错误类型定义
 *
 * Neo-T, 创建于2022.03.14 17:14
 *
 */
#ifndef ONPS_ERRORS_H
#define ONPS_ERRORS_H

#ifdef SYMBOL_GLOBALS
#define ONPS_ERRORS_EXT
#else
#define ONPS_ERRORS_EXT extern
#endif //* SYMBOL_GLOBALS

typedef enum {
    ERRNO = 0,              //* 没有发生任何错误
    ERRNOPAGENODE,          //* 无可用的内存页面节点
    ERRREQMEMTOOLARGE,      //* 申请的内存过大，超过了系统支持的最大申请配额
    ERRNOFREEMEM,           //* 系统已无可用内存
    ERRMUTEXINITFAILED,     //* 线程同步锁初始化失败
    ERRNOBUFLISTNODE,       //* 无可用缓冲区链表节点
    ERRSEMINITFAILED,       //* 信号量初始化失败
    ERROPENTTY,             //* tty终端打开失败
    ERRATWRITE,             //* AT指令写错误
    ERRATEXEC,              //* AT指令返回错误
    ERRATEXECTIMEOUT,       //* AT指令执行超时
    ERRSIMCARD,             //* 未检测到SIM卡
    ERRREGMOBILENET,        //* 没有注册到移动网络
    ERRPPPIDXOVERFLOW,      //* ppp链路索引溢出
    ERRPPPDELIMITER,        //* 未找到ppp帧定界符
    ERRTOOMANYTTYS,         //* tty数量过多
    ERRTTYHANDLE,           //* tty句柄无效
    ERROSADAPTER,           //* os适配层错误
    ERRUNKNOWNPROTOCOL,     //* 未知协议类型
    ERRPPPFCS,              //* ppp帧校验和错误                
    ERRNOIDLETIMER,         //* 没有空闲的定时器
    ERRNOFREEPPWANODE,      //* 用于ppp协商等待的节点不可用
    ERRPPPWALISTNOINIT,     //* ppp等待应答链表未初始化
    ERRNONETIFNODE,         //* 无可用的netif节点
    ERRNONETIFFOUND,        //* 未找到网络接口
    ERRINPUTOVERFLOW,       //* onps输入句柄溢出
    ERRUNSUPPIPPROTO,       //* 不被支持的IP层协议
    ERRUNSUPPIOPT,          //* 不支持的配置项
    ERRIPROTOMATCH,         //* 协议匹配错误
    ERRNOROUTENODE,         //* 无可用的路由表单元
    ERRADDRESSING,          //* 寻址失败，不存在缺省路由
    ERRADDRFAMILIES,        //* 地址族错误
    ERRSOCKETTYPE,          //* 不被支持的socket类型
    ERRNOATTACH,            //* 附加数据地址为空
    ERRTCSNONTCP,           //* 非TCP协议不能获取、设置TCP链路状态
    ERRTDSNONTCP,           //* 非TCP协议不能获取数据发送状态
    ERRTCPCONNTIMEOUT,      //* TCP连接超时
    ERRTCPCONNRESET,        //* TCP连接已被重置
    ERRTCPCONNCLOSED,       //* TCP链路已关闭
    ERRDATAEMPTY,           //* 数据段为0
    ERRTCPACKTIMEOUT,       //* TCP应答超时
    ERRNOTCPLINKNODE,       //* 无可用tcp link节点
    ERRTCPNOTCONNECTED,     //* tcp未连接
    ERRINVALIDSEM,          //* 无效的信号量
    ERRSENDZEROBYTES,       //* 发送了0字节的数据
    ERRPORTOCCUPIED,        //* 端口已被占用
    ERRSENDADDR,            //* 发送地址错误
    ERRETHEXTRAEMPTY,       //* 无可用的ethernet网卡附加信息节点
    ERRNETUNREACHABLE,      //* 网络不可达
    ERRROUTEADDRMATCH,      //* 两次寻址结果不匹配
    ERRNEWARPCTLBLOCK,      //* arp控制块已空
    ERRUNSUPPETHIIPROTO,    //* 不被支持的ethernet ii上层协议
    ERRPACKETTOOLARGE,      //* 报文太大
    ERRPORTEMPTY,           //* 端口为空
    ERRWAITACKTIMEOUT,      //* 等待应答超时
    ERRIPCONFLICT,          //* ip地址冲突
    ERRNOTBINDADDR,         //* 套接字没有绑定地址
    ERRTCPONLY,             //* 仅支持tcp协议
    ERRTCPSRVEMPTY,         //* tcp服务器资源为空
    ERRTCPBACKLOGEMPTY,     //* tcp服务器的backlog资源为空
    ERRTCPRCVQUEUEEMPTY,    //* tcp服务器的接收队列资源为空
    ERRTCPNOLISTEN,         //* tcp服务器未进入listen阶段
    ERRTCPBACKLOGFULL,      //* tcp服务器的backlog队列已满
    ERRDNSQUERYFMT,         //* dns查询报文格式错误
    ERRDNSSRV,              //* dns服务器故障
    ERRDNSNAME,             //* 域名不存在
    ERRDNSQUERYTYPE,        //* 不被支持的dns查询类型
    ERRDNSREFUSED,          //* dns服务器拒绝
    ERRDNSNOTRESOLVED,      //* 未解析到ip地址
    ERRNOUDPLINKNODE,       //* 无可用udp link节点
    ERRTCPLINKCBNULL,       //* tcp link控制块为NULL
#if SUPPORT_IPV6
	ERRNEWIPv6MACCTLBLOCK,	//* ipv6到mac地址映射表控制块已空
	ERRNOIPv6DYNADDRNODE,   //* 无可用ipv6动态地址节点
	ERRNOIPv6ROUTERNODE,    //* 无可用ipv6路由器节点
	ERRIPV4FLOWLABEL,       //* ipv4协议不支持流标签字段
	ERRNODv6CLTCBNODE,      //* 无可用dhcpv6客户端控制块节点
	ERRROUTERINDEX,         //* 路由器节点索引溢出
#endif
	ERRFAMILYINCONSISTENT,  //* 目标和源地址的地址族不一致
	ERRUNSUPPORTEDFAMILY,   //* 不支持的地址族
#if NETTOOLS_TELNETSRV
    ERRNOTELNETCLTCBNODE,   //* 无可用telnet客户端控制块节点
    ERRNVTSTART,            //* nvt启动失败
#endif
    ERRNETIFNOTFOUND,       //* 未找到网络接口
    ERREXTRAIPLIMIT,        //* 附加ip超出数量限制
    ERREXTRAIPSAVE,         //* 附加ip地址写入系统存储器失败
    ERREXTRAIPDEL,          //* 附加ip地址从系统存储器删除失败
    ERRIPUPDATED,           //* 无法覆盖系统存储器的原值
    ERRIPNOSTATIC,          //* 动态地址（dhcp）模式下不允许增加、删除、修改ip地址
    ERRROUTEENTRYNOR,       //* 增加或删除路由条目时系统存储器写入失败
    ERRROUTEDEFAULTDEL,     //* 缺省路由不能被删除
    ERRROUTEENTRYNOTEXIST,  //* 路由条目不存在
    ERRUNKNOWN,             //* 未知错误
} EN_ONPSERR;

typedef struct _ST_ONPSERR_ {
    EN_ONPSERR enCode;
    CHAR szDesc[128];
} ST_ONPSERR, *PST_ONPSERR;

ONPS_ERRORS_EXT const CHAR *onps_error(EN_ONPSERR enErr);

#endif
