#include "port/datatype.h"
#include "port/os_datatype.h"
#include "port/os_adapter.h"
#include "port/sys_config.h"
#include "errors.h"

#define SYMBOL_GLOBALS
#include "utils.h"
#undef SYMBOL_GLOBALS

USHORT tcpip_checksum(USHORT *pusData, INT nDataBytes)
{
	ULONG ulChecksum = 0;

	while (nDataBytes > 1)
	{
		ulChecksum += *pusData++;
		nDataBytes -= sizeof(USHORT);
	}

	if (nDataBytes)
	{
		ulChecksum += *(UCHAR*)pusData;
	}
	ulChecksum = (ulChecksum >> 16) + (ulChecksum & 0xffff);
	ulChecksum += (ulChecksum >> 16);

	return (USHORT)(~ulChecksum);
}