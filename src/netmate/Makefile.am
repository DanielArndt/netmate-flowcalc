
bin_PROGRAMS = netmate

if ENABLE_DEBUG
  AM_CXXFLAGS = -g -fno-inline -D_GLIBCXX_DEBUG -DDEBUG -DPROFILING
else
  AM_CXXFLAGS = -O2  
endif

if ENABLE_NF
 AM_CXXFLAGS += -fpermissive -DPROGRAM_VERSION=\"@IPTABLES_VERSION@\"
endif

netmate_LDFLAGS = -export-dynamic -module 


netmate_SOURCES = Error.cc Logger.cc XMLParser.cc ConfigParser.cc ConfigManager.cc PerfTimer.cc \
       RuleIdSource.cc Rule.cc RuleManager.cc Event.cc EventScheduler.cc ProcModule.cc \
       MeterComponent.cc NetTap.cc NetTapPcap.cc Classifier.cc ClassifierSimple.cc \
       MetricData.cc FlowRecord.cc Exporter.cc Module.cc ModuleLoader.cc ExportModule.cc \
       PacketQueue.cc PacketProcessor.cc CtrlComm.cc CommandLineArgs.cc FilterValue.cc \
       FilterDefParser.cc RuleFileParser.cc FlowRecordDB.cc Bitmap.cc ClassifierRFC.cc Meter.cc \
       ExportList.cc MeterInfo.cc MAPIRuleParser.cc FilterValParser.cc ParserFcts.cc \
       Sampler.cc SamplerAll.cc Timeval.cc PageRepository.cc constants.cc \
       Error.h Logger.h XMLParser.h ConfigParser.h ConfigManager.h PerfTimer.h \
       RuleIdSource.h Rule.h RuleManager.h Event.h EventScheduler.h ProcModule.h \
       MeterComponent.h NetTap.h NetTapPcap.h Classifier.h ClassifierSimple.h \
       MetricData.h FlowRecord.h Exporter.h Module.h ModuleLoader.h ExportModule.h \
       PacketQueue.h PacketProcessor.h CtrlComm.h CommandLineArgs.h FilterValue.h \
       FilterDefParser.h RuleFileParser.h FlowRecordDB.h Bitmap.h ClassifierRFC.h Meter.h \
       ExportList.h MeterInfo.h MAPIRuleParser.h FilterValParser.h ParserFcts.h \
       Sampler.h SamplerAll.h ClassifierRFCConf.h Timeval.h PageRepository.h Threads.h \
       FlowCreator.h FlowCreator.cc 

if ENABLE_ERF
  netmate_SOURCES += NetTapERF.h NetTapERF.cc
endif

if ENABLE_NF
  netmate_SOURCES += ClassifierNetfilter.cc ClassifierNetfilter.h
endif


INCLUDES = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/lib/httpd -I$(top_srcdir)/src/lib/getopt_long

netmate_LDADD = $(top_builddir)/src/lib/httpd/libhttpd.a $(top_builddir)/src/lib/getopt_long/libgetopt_long.a \
	@PTHREADLIB@ @DLLIB@ @PCAPLIB@ @SSLLIB@ @XMLLIB@ @MATHLIB@ @MPATROLLIB@ @ERFLIB@ 

if ENABLE_NF
  INCLUDES += -I/usr/src/linux/include
  netmate_LDADD += @ULOGLIB@
endif

$(top_builddir)/src/lib/httpd/libhttpd.a:
	cd $(top_builddir)/src/lib/httpd ; $(MAKE)

$(top_builddir)/src/lib/getopt_long/libgetopt_long.a:
	cd $(top_builddir)/src/lib/getopt_long ; $(MAKE)

install-data-hook:
	$(INSTALL) -d $(DESTDIR)/$(localstatedir)/run
	$(INSTALL) -d $(DESTDIR)/$(localstatedir)/log

