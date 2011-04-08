EXTRA_DIST = ProcModule.h ProcModule.c

INCLUDES = -I$(top_srcdir)/src/include \
           -I$(top_srcdir)/netfilter_userspace/include \
           -I$(top_srcdir)/src/proc_modules \
           -I/usr/src/linux/include

lib_LTLIBRARIES = count.la bandwidth.la jitter.la pktid_crc32.la pktid_md5.la pktlen.la port_use.la rtploss.la rtt_ping.la show_ascii.la testmod.la netai_flowstats.la netai_flowstats_dscp.la netai_flowstats_dscp_1s.la capture.la  

if ENABLE_DEBUG
  AM_CFLAGS = -g -fPIC -DDEBUG -DPROFILING 
else
  AM_CFLAGS = -O2 -fPIC
endif

count_la_LDFLAGS = -export-dynamic -module
count_la_SOURCES = count.c
count_la_LIBADD = ProcModule.lo

bandwidth_la_LDFLAGS = -export-dynamic -module
bandwidth_la_SOURCES = bandwidth.c
bandwidth_la_LIBADD = ProcModule.lo

jitter_la_LDFLAGS = -export-dynamic -module
jitter_la_SOURCES = jitter.c
jitter_la_LIBADD = ProcModule.lo

pktid_crc32_la_LDFLAGS = -export-dynamic -module
pktid_crc32_la_SOURCES = pktid_crc32.c ip.h
pktid_crc32_la_LIBADD = ProcModule.lo

pktid_md5_la_LDFLAGS = -export-dynamic -module
pktid_md5_la_SOURCES = global.h md5.h md5c.c pktid_md5.c
pktid_md5_la_LIBADD = ProcModule.lo

pktlen_la_LDFLAGS = -export-dynamic -module
pktlen_la_SOURCES = pktlen.c
pktlen_la_LIBADD = ProcModule.lo

port_use_la_LDFLAGS = -export-dynamic -module
port_use_la_SOURCES = port_use.c
port_use_la_LIBADD = ProcModule.lo

rtploss_la_LDFLAGS = -export-dynamic -module
rtploss_la_SOURCES = rtploss.c
rtploss_la_LIBADD = ProcModule.lo

rtt_ping_la_LDFLAGS = -export-dynamic -module
rtt_ping_la_SOURCES = rtt_ping.c
rtt_ping_la_LIBADD = ProcModule.lo

show_ascii_la_LDFLAGS = -export-dynamic -module
show_ascii_la_SOURCES = show_ascii.c
show_ascii_la_LIBADD = ProcModule.lo

testmod_la_LDFLAGS = -export-dynamic -module
testmod_la_SOURCES = testmod.c
testmod_la_LIBADD = ProcModule.lo

netai_flowstats_la_LDFLAGS = -export-dynamic -module
netai_flowstats_la_SOURCES = netai_flowstats.c
netai_flowstats_la_LIBADD = ProcModule.lo

netai_flowstats_dscp_la_LDFLAGS = -export-dynamic -module
netai_flowstats_dscp_la_SOURCES = netai_flowstats_dscp.c
netai_flowstats_dscp_la_LIBADD = ProcModule.lo

netai_flowstats_dscp_1s_la_LDFLAGS = -export-dynamic -module
netai_flowstats_dscp_1s_la_SOURCES = netai_flowstats_dscp_1s.c
netai_flowstats_dscp_1s_la_LIBADD = ProcModule.lo

capture_la_LDFLAGS = -export-dynamic -module
capture_la_SOURCES = capture.c
capture_la_LIBADD = ProcModule.lo

