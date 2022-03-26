/* buf_list.h
 *
 * 缓冲区链表，这是一个单向链表，由协议栈使用
 *
 * Neo-T, 创建于2022.03.16 13:50
 * 版本: 1.0
 *
 */
#ifndef BUF_LIST_H
#define BUF_LIST_H

#ifdef SYMBOL_GLOBALS
	#define BUF_LIST_EXT
#else
	#define BUF_LIST_EXT extern
#endif //* SYMBOL_GLOBALS

#define BUF_LIST_NUM	128		//* 缓冲区链表的节点数，最大不能超过2的15次方（32768）

BUF_LIST_EXT BOOL buf_list_init(EN_ERROR_CODE *penErrCode);
BUF_LIST_EXT SHORT buf_list_get(EN_ERROR_CODE *penErrCode);
BUF_LIST_EXT SHORT buf_list_get_ext(void *pvData, UINT unDataSize, EN_ERROR_CODE *penErrCode); 
BUF_LIST_EXT void buf_list_attach_data(SHORT sNode, void *pvData, UINT unDataSize);
BUF_LIST_EXT void buf_list_free(SHORT sNode);
BUF_LIST_EXT void buf_list_put_head(SHORT *psHead, SHORT sNode); 
BUF_LIST_EXT void buf_list_put_tail(SHORT sHead, SHORT sNode); 
BUF_LIST_EXT void *buf_list_get_next_node(SHORT *psNextNode, USHORT *pusDataLen); 

#endif