
if ENABLE_DEBUG
  AM_CXXFLAGS = -g -O2 -D_GLIBCXX_DEBUG -DDEBUG
else
  AM_CXXFLAGS = -O2
endif


if ENABLE_TEST

  COMMON_SOURCES=../netmate/Error.cc ../netmate/Logger.cc
 
# tests

  bin_PROGRAMS = PacketQueueTest PerfTimerTest 

  PacketQueueTest_SOURCES = $(COMMON_SOURCES) ../netmate/PacketQueue.cc PacketQueueTest.cc 

  PerfTimerTest_SOURCES = $(COMMON_SOURCES) ../netmate/PerfTimer.cc PerfTimerTest.cc

# tests end

  INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/include -I$(top_srcdir)/src/lib/ctrlcomm \
    -I$(top_srcdir)/src/netmate

  LDADD = -L$(top_srcdir)/src/lib/ctrlcomm -lctrlcomm \
       @PTHREADLIB@ @DLLIB@ @PCAPLIB@ @SSLLIB@ @XMLLIB@ @MATHLIB@ 
endif
