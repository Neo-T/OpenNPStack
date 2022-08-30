/* onps_utils.h
 *
 * 协议栈用到的相关功能函数在此文件声明、定义
 *
 * Neo-T, 创建于2022.03.21 16:05
 * 版本: 1.0
 *
 */
#ifndef ONPS_UTILS_H
#define ONPS_UTILS_H

#ifdef SYMBOL_GLOBALS
	#define ONPS_UTILS_EXT
#else
	#define ONPS_UTILS_EXT extern
#endif //* SYMBOL_GLOBALS

 //* 大小端转换宏
#define ENDIAN_CONVERTER_LONGLONG(n)    ((((n) & 0xFF) << 56) | (((n) & 0xFF00) << 40) | (((n) & 0xFF0000) << 24) | (((n) & 0xFF000000) << 8) | (((n) & 0xFF00000000) >> 8) | (((n) & 0xFF0000000000) >> 24) | (((n) & 0xFF000000000000) >> 40) | (((n) & 0xFF00000000000000) >> 56))
#define ENDIAN_CONVERTER_UINT(n)        ((((n) & 0xFF) << 24) | (((n) & 0xFF00) << 8) | (((n) & 0xFF0000) >> 8) | (((n) & 0xFF000000) >> 24))
#define ENDIAN_CONVERTER_USHORT(n)	    ((((n) & 0xFF) << 8)  | (((n) & 0xFF00) >> 8))
#define htonll(n)   ENDIAN_CONVERTER_LONGLONG(n) 
#define htonl(n)    ENDIAN_CONVERTER_UINT(n)
#define htons(n)    ENDIAN_CONVERTER_USHORT(n)
#define ip_addressing(unDestIP, unIfIP, unGenmask) ((UINT)(unIfIP & unGenmask) == (UINT)(unDestIP & unGenmask))

#if !(defined(__linux__) || defined(__linux)) && !(defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)) && !(defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

ONPS_UTILS_EXT CHAR *mem_char(CHAR *pszMem, CHAR ch, UINT unMemSize);
ONPS_UTILS_EXT CHAR *mem_str(CHAR *pszMem, CHAR *pszStr, UINT unStrSize, UINT unMemSize);
ONPS_UTILS_EXT CHAR *strtok_safe(CHAR **ppszStart, const CHAR *pszSplitStr); 

ONPS_UTILS_EXT USHORT tcpip_checksum(USHORT *pusData, INT nDataBytes);
ONPS_UTILS_EXT USHORT tcpip_checksum_ext(SHORT sBufListHead);
ONPS_UTILS_EXT void snprintf_hex(const UCHAR *pubHexData, USHORT usHexDataLen, CHAR *pszDstBuf, UINT unDstBufSize, BOOL blIsSeparateWithSpace);

#if SUPPORT_PRINTF
ONPS_UTILS_EXT void printf_hex(const UCHAR *pubHex, USHORT usHexDataLen, UCHAR ubBytesPerLine);
ONPS_UTILS_EXT void printf_hex_ext(SHORT sBufListHead, UCHAR ubBytesPerLine);
#endif

//* 64位长整型（适用于按32位解析的场景）
typedef union _UNI_LONG_LONG_ {
    struct {
        INT l; 
        INT h; 
    } stInt64; 

    LONGLONG llVal; 
} UNI_LONG_LONG, *PUNI_LONG_LONG;

//* 单向链表节点
typedef struct _ST_SLINKEDLIST_NODE_ ST_SLINKEDLIST_NODE, *PST_SLINKEDLIST_NODE, *PST_SLINKEDLIST;
typedef struct _ST_SLINKEDLIST_NODE_ {
    PST_SLINKEDLIST_NODE pstNext;
    union {
        void *ptr; 
        INT nVal;         
        UINT unVal; 
    } uniData;  
} ST_SLINKEDLIST_NODE, *PST_SLINKEDLIST_NODE;

//* 单向链表相关操作函数
ONPS_UTILS_EXT PST_SLINKEDLIST_NODE sllist_get_node(PST_SLINKEDLIST *ppstSLList);                    //* 获取一个节点，其直接摘取链表的头部节点返回给调用者
ONPS_UTILS_EXT PST_SLINKEDLIST_NODE sllist_get_tail_node(PST_SLINKEDLIST *ppstSLList);               //* 同上，只不过这个函数将摘取链表的尾部节点给调用者
ONPS_UTILS_EXT void sllist_del_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode);      //* 从链表中摘除一个节点
ONPS_UTILS_EXT void sllist_del_node_ext(PST_SLINKEDLIST *ppstSLList, void *pvData);                  //* 从链表中摘除一个节点，与上面的函数不同的地方是入口参数pvData指向的是当前节点携带数据的首地址，首地址匹配的节点将从链表中删除
ONPS_UTILS_EXT void sllist_put_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode);      //* 归还一个节点，其直接将该节点挂接到链表头部
ONPS_UTILS_EXT void sllist_put_tail_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode); //* 同上，只不过该函数是将该节点挂接到链表尾部

ONPS_UTILS_EXT in_addr_t inet_addr(const char *pszIP);   //* 点分十进制IPv4地址转换为4字节无符号整型地址
ONPS_UTILS_EXT in_addr_t inet_addr_small(const char *pszIP); 
ONPS_UTILS_EXT char *inet_ntoa(struct in_addr stInAddr);
ONPS_UTILS_EXT char *inet_ntoa_ext(in_addr_t unAddr);
ONPS_UTILS_EXT char *inet_ntoa_safe(struct in_addr stInAddr, char *pszAddr);
ONPS_UTILS_EXT char *inet_ntoa_safe_ext(in_addr_t unAddr, char *pszAddr); 

ONPS_UTILS_EXT const CHAR *get_ip_proto_name(UCHAR ubProto); 

#if SUPPORT_ETHERNET
ONPS_UTILS_EXT BOOL ethernet_mac_matched(const UCHAR *pubaMacAddr1, const UCHAR *pubaMacAddr2); 
ONPS_UTILS_EXT BOOL is_mac_broadcast_addr(const UCHAR *pubaMacAddr); 
#endif

//* 几级域名，也就是域名分了几段，为封装dns查询报文提供数据支持，参数pnBytesOf1stSeg用于接收第一段域名的长度
//* 返回值为域名级数，其实就是这个域名分了几段
ONPS_UTILS_EXT INT get_level_of_domain_name(const CHAR *pszDomainName, INT *pnBytesOf1stSeg); 

#endif
