from building import *

cwd = GetCurrentDir()
path = [cwd + '/include']
path += [cwd + '/port/include']
src  = ['one_shot_timer.c', 'onps_entry.c', 'onps_errors.c', 'onps_input.c', 'onps_utils.c']

src += ['bsd/socket.c', 'ethernet/arp.c', 'ethernet/ethernet.c', 'ip/icmp.c', 'ip/ip.c', 'ip/tcp.c', 'ip/tcp_link.c', 'ip/tcp_options.c', 'ip/udp.c', 'ip/udp_link.c']
src += ['mmu/buddy.c', 'mmu/buf_list.c', 'netif/netif.c', 'netif/route.c']
src += ['port/os_adapter.c']
src += ['port/include/port/datatype.h', 'port/include/port/os_datatype.h', 'port/include/port/os_adapter.h', 'port/include/port/sys_config.h']

group = DefineGroup('onps', src, depend = ['PKG_USING_ONPS'], CPPPATH = path)

Return('group')
