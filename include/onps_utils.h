/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 协议栈用到的相关功能函数在此文件声明、定义
 *
 * Neo-T, 创建于2022.03.21 16:05
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
#define broadcast_addr(unIpAddr, unSubnetMask) (unIpAddr | (~unSubnetMask))

//* 判断某个无符号整型数是否在指定范围内
#define uint_is_within_range(num, left, right) ((INT)((num - left) | (right - num)) >= 0)

//* tcp序号比较宏
#define uint_before(seq1, seq2) (((INT)((seq1) - (seq2))) < 0)
#define uint_after(seq2, seq1) uint_before(seq1, seq2)

#if !(defined(__linux__) || defined(__linux)) && !(defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)) && !(defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#else
#include <stddef.h>
#endif

ONPS_UTILS_EXT CHAR *mem_char(CHAR *pszMem, CHAR ch, UINT unMemSize);
ONPS_UTILS_EXT CHAR *mem_str(CHAR *pszMem, CHAR *pszStr, UINT unStrSize, UINT unMemSize);
ONPS_UTILS_EXT CHAR *strtok_safe(CHAR **ppszStart, const CHAR *pszSplitStr); 
ONPS_UTILS_EXT UINT rand_big(void); 
ONPS_UTILS_EXT UCHAR *rand_any_bytes(UCHAR *pubRandSeq, UINT unSeqLen); 

#define hex_to_char(val, is_uppercase) (val += val < 10 ? '0' : (is_uppercase ? 'A' - 10 : 'a' - 10))
#define hex_to_char_no_lz(prev_val, next_val, is_uppercase) (prev_val = next_val + (next_val < 10 ? '0' : (is_uppercase ? 'A' - 10 : 'a' - 10)))
ONPS_UTILS_EXT const CHAR *hex_to_str_8(UCHAR ubVal, CHAR szDst[3], BOOL blIsUppercase); //* 完整转换，如0x0A->0A
ONPS_UTILS_EXT const CHAR *hex_to_str_no_lz_8(UCHAR ubVal, CHAR szDst[3], BOOL blIsUppercase, CHAR *pbBytes); //* 不带前导0的转换函数，如0x0A->A
ONPS_UTILS_EXT const CHAR *hex_to_str_16(USHORT usVal, CHAR szDst[5], BOOL blIsUppercase, BOOL blIsBigEndian); //* 完整转换，如0x0F09->0F09
ONPS_UTILS_EXT const CHAR *hex_to_str_no_lz_16(USHORT usVal, CHAR szDst[5], BOOL blIsUppercase, BOOL blIsBigEndian, CHAR *pbBytes);//* 不带前导零的转换，如0x0F09->F09
ONPS_UTILS_EXT CHAR ascii_to_hex_4(CHAR ch); //* 如ascii的字符F转为->0xF
ONPS_UTILS_EXT USHORT ascii_to_hex_16(const CHAR *pszAscii); //* 如16位短整型ascii字符串F09转为0x0F09

ONPS_UTILS_EXT USHORT tcpip_checksum(USHORT *pusData, INT nDataBytes);
ONPS_UTILS_EXT USHORT tcpip_checksum_ext(SHORT sBufListHead); 
ONPS_UTILS_EXT USHORT tcpip_checksum_ipv4(in_addr_t unSrcAddr, in_addr_t unDstAddr, USHORT usPayloadLen, UCHAR ubProto, SHORT sBufListHead, EN_ONPSERR *penErr); 
ONPS_UTILS_EXT USHORT tcpip_checksum_ipv4_ext(in_addr_t unSrcAddr, in_addr_t unDstAddr, UCHAR ubProto, UCHAR *pubPayload, USHORT usPayloadLen, EN_ONPSERR *penErr);
#if SUPPORT_IPV6
ONPS_UTILS_EXT USHORT tcpip_checksum_ipv6(UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UINT unPayloadLen, UCHAR ubProto, SHORT sBufListHead, EN_ONPSERR *penErr);
ONPS_UTILS_EXT USHORT tcpip_checksum_ipv6_ext(UCHAR ubaSrcAddr[16], UCHAR ubaDstAddr[16], UCHAR ubProto, UCHAR *pubPayload, UINT unPayloadLen, EN_ONPSERR *penErr); 
#endif
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

typedef enum {
    AND,    //* 与  
    OR,     //* 或
    LT,     //* 小于
    LE,     //* 小于等于
    EQ,     //* 等于
    NE,     //* 不等于
    GT,     //* 大于
    GE      //* 大于等于
} EN_OPTTYPE;

//* 单向链表相关操作函数
ONPS_UTILS_EXT PST_SLINKEDLIST_NODE sllist_get_node(PST_SLINKEDLIST *ppstSLList);                    //* 获取一个节点，其直接摘取链表的头部节点返回给调用者
ONPS_UTILS_EXT PST_SLINKEDLIST_NODE sllist_get_tail_node(PST_SLINKEDLIST *ppstSLList);               //* 同上，只不过这个函数将摘取链表的尾部节点给调用者
ONPS_UTILS_EXT void sllist_del_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode);      //* 从链表中摘除一个节点
ONPS_UTILS_EXT void sllist_del_node_ext(PST_SLINKEDLIST *ppstSLList, void *pvData);                  //* 从链表中摘除一个节点，与上面的函数不同的地方是入口参数pvData指向的是当前节点携带数据的首地址，首地址匹配的节点将从链表中删除
ONPS_UTILS_EXT void sllist_put_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode);      //* 归还一个节点，其直接将该节点挂接到链表头部
ONPS_UTILS_EXT void sllist_put_tail_node(PST_SLINKEDLIST *ppstSLList, PST_SLINKEDLIST_NODE pstNode); //* 同上，只不过该函数是将该节点挂接到链表尾部

#define INVALID_ARRAYLNKLIST_UNIT -1 //* 无效的数组型链表单元
ONPS_UTILS_EXT CHAR array_linked_list_get_index(void *pvUnit, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum);
ONPS_UTILS_EXT void *array_linked_list_get(CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit, CHAR *pbUnitIdx); 
ONPS_UTILS_EXT void *array_linked_list_get_safe(CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit, CHAR *pbUnitIdx);
ONPS_UTILS_EXT void array_linked_list_put(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit);
ONPS_UTILS_EXT void array_linked_list_put_safe(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit);
ONPS_UTILS_EXT void array_linked_list_put_tail(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit);
ONPS_UTILS_EXT void array_linked_list_put_tail_safe(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bUnitNum, CHAR bOffsetNextUnit);
ONPS_UTILS_EXT void array_linked_list_del(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit);
ONPS_UTILS_EXT void array_linked_list_del_safe(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit);
ONPS_UTILS_EXT void *array_linked_list_next(CHAR *pbNextUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit, CHAR *pbUnitIdx); 
ONPS_UTILS_EXT void *array_linked_list_next_ext(void *pvUnit, CHAR *pbListHead, void *pvArray, UCHAR ubUnitSize, CHAR bOffsetNextUnit);

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

//* 返回两个8位字节匹配的位数，从左侧开始比较（高到低）
ONPS_UTILS_EXT INT bit8_matched_from_left(UCHAR ch1, UCHAR ch2, UCHAR ubCmpBits); 

//* 利用冯.诺伊曼算法生成近似均匀分布的哈希值，其输出并不在16位结果值生成后就立即结束，而是64位数据耗尽后结束，其生成结果最大为32位值
ONPS_UTILS_EXT UINT hash_von_neumann(ULONGLONG ullKey);

#if SUPPORT_IPV6
ONPS_UTILS_EXT const CHAR *inet6_ntoa(const UCHAR ubaIpv6[16], CHAR szIpv6[40]); 
ONPS_UTILS_EXT const UCHAR *inet6_aton(const CHAR *pszIpv6, UCHAR ubaIpv6[16]); 
ONPS_UTILS_EXT INT ipv6_addr_cmp(const UCHAR *pubAddr1, const UCHAR *pubAddr2, UCHAR ubBitsToCompare); //* 参数ubBitsToCompare指定要比较的数据位数，最长即为ipv6地址的128位长，返回值为0则相等，1则前者大于后者，-1则后者大于前者
ONPS_UTILS_EXT INT ipv6_prefix_matched_bits(const UCHAR ubaAddr1[16], const UCHAR ubaAddr2[16], UCHAR ubPrefixBitsLen); //* 给出两个地址前缀匹配的数据位数，返回值为匹配的位数，0为不匹配
#endif

#endif
