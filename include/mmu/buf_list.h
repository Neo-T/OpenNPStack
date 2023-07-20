/*
 * 版权属于onps栈开发团队，遵循Apache License 2.0开源许可协议
 *
 * 缓冲区链表，这是一个单向链表，由协议栈使用，用于实现发送时零拷贝设计
 *
 * Neo-T, 创建于2022.03.16 13:50
 *
 */
#ifndef BUF_LIST_H
#define BUF_LIST_H

#ifdef SYMBOL_GLOBALS
	#define BUF_LIST_EXT
#else
	#define BUF_LIST_EXT extern
#endif //* SYMBOL_GLOBALS

BUF_LIST_EXT BOOL buf_list_init(EN_ONPSERR *penErr);
BUF_LIST_EXT void buf_list_uninit(void); 
BUF_LIST_EXT SHORT buf_list_get(EN_ONPSERR *penErr);
BUF_LIST_EXT SHORT buf_list_get_ext(void *pvData, UINT unDataSize, EN_ONPSERR *penErr); 
BUF_LIST_EXT void buf_list_attach_data(SHORT sNode, void *pvData, UINT unDataSize);
BUF_LIST_EXT void buf_list_free(SHORT sNode);
BUF_LIST_EXT void buf_list_free_head(SHORT *psHead, SHORT sNode);
BUF_LIST_EXT void buf_list_put_head(SHORT *psHead, SHORT sNode); 
BUF_LIST_EXT void buf_list_put_tail(SHORT sHead, SHORT sNode); 
BUF_LIST_EXT void *buf_list_get_next_node(SHORT *psNextNode, USHORT *pusDataLen); 
BUF_LIST_EXT UINT buf_list_get_len(SHORT sBufListHead); 
BUF_LIST_EXT void buf_list_merge_packet(SHORT sBufListHead, UCHAR *pubPacket); 

#endif
