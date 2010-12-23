EXTRA_DIST = ExportModule.h ExportModule.cc

INCLUDES = -I$(top_srcdir)/src/include \
           -I$(top_srcdir)/netfilter_userspace/include \
           -I$(top_srcdir)/src/netmate \
           -I$(top_srcdir)/src/lib/getopt_long \
           -I$(top_srcdir)/src/lib/libipfix \
           -I/usr/src/linux/include

lib_LTLIBRARIES = testexp.la text_file.la bin_file.la ctext_file.la ac_file.la netai_socket.la netai_arff.la 
if ENABLE_IPFIX
  lib_LTLIBRARIES += ipfix.la
endif

if ENABLE_DEBUG
  AM_CXXFLAGS = -g -fPIC -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GLIBCXX_DEBUG -DDEBUG -DPROFILING
else
  AM_CXXFLAGS = -O2 -fPIC -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
endif

testexp_la_LDFLAGS = -export-dynamic -module
testexp_la_SOURCES = testexp.cc
testexp_la_LIBADD = ExportModule.lo

text_file_la_LDFLAGS = -export-dynamic -module
text_file_la_SOURCES = text_file.cc
text_file_la_LIBADD = ExportModule.lo

bin_file_la_LDFLAGS = -export-dynamic -module
bin_file_la_SOURCES = bin_file.cc
bin_file_la_LIBADD = ExportModule.lo

ctext_file_la_LDFLAGS = -export-dynamic -module
ctext_file_la_SOURCES = ctext_file.cc
ctext_file_la_LIBADD = ExportModule.lo

ac_file_la_LDFLAGS = -export-dynamic -module
ac_file_la_SOURCES = ac_file.cc
ac_file_la_LIBADD = ExportModule.lo

netai_socket_la_LDFLAGS = -export-dynamic -module
netai_socket_la_SOURCES = netai_socket.cc
netai_socket_la_LIBADD = ExportModule.lo

netai_arff_la_LDFLAGS = -export-dynamic -module
netai_arff_la_SOURCES = netai_arff.cc
netai_arff_la_LIBADD = ExportModule.lo



ipfix_la_LDFLAGS = -export-dynamic -module
ipfix_la_SOURCES = ipfix.cc
ipfix_la_LIBADD = ExportModule.lo @IPFIXLIB@ 
