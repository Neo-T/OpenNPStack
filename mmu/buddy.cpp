#include "datatype.h"
#include "sys_config.h"
#include "buddy.h"

static UCHAR l_ubaMemPool[BUDDY_MEM_SIZE];
static ST_BUDDY_AREA l_staArea[BUDDY_ARER_COUNT];