/* buddy.h
 *
 * 閲囩敤Buddy绠楁硶瀹炵幇鐨勫唴瀛樼鐞嗗崟鍏冩ā鍧楃敤鍒扮殑鐩稿叧鏁版嵁缁撴瀯鍙婂畯瀹氫箟鏂囦欢
 *
 * Neo-T, 鍒涘缓浜?022.03.11 14:28
 * 鐗堟湰: 1.0
 *
 */
#ifndef BUDDY_H
#define BUDDY_H

#ifdef SYMBOL_GLOBALS
	#define BUDDY_EXT
#else
	#define BUDDY_EXT extern
#endif //* SYMBOL_GLOBALS

typedef struct _ST_BUDDY_PAGE_ { //* 鍏锋湁鐩稿悓椤甸潰澶у皬鐨勯〉闈㈤摼琛ㄨ妭鐐圭殑鍩烘湰鏁版嵁缁撴瀯
    struct _ST_BUDDY_PAGE_ *pstNext;     
    UCHAR *pubStart;     
} ST_BUDDY_PAGE, *PST_BUDDY_PAGE; 

typedef struct _ST_BUDDY_AREA_ { //* 鍏锋湁鐩稿悓椤甸潰澶у皬鐨勯〉鍧楁暟缁勫崟鍏冪殑鍩烘湰鏁版嵁缁撴瀯
    PST_BUDDY_PAGE pstNext; 
    UINT unPageSize; 
    UCHAR ubMap;  //* 鏍囪瘑涓や釜浼欎即椤甸潰鐨勪娇鐢ㄦ儏鍐碉細0锛屼袱涓潎绌洪棽鎴栧潎鍦ㄤ娇鐢紱1锛屼竴涓湪浣跨敤鍙︿竴涓┖闂?
} ST_BUDDY_AREA, *PST_BUDDY_AREA; 

BUDDY_EXT void buddy_init(void); 
BUDDY_EXT void buddy_alloc(UINT unSize);
#endif