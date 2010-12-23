if BUILD_NMRSH

bin_PROGRAMS = nmrsh 

if ENABLE_DEBUG
  AM_CFLAGS = -g -fno-inline -D_GLIBCXX_DEBUG -DDEBUG -DPROFILING
else
  AM_CFLAGS = -O2 
endif

nmrsh_SOURCES = nmrsh.cc $(top_srcdir)/src/netmate/Error.cc \
                $(top_srcdir)/src/netmate/CommandLineArgs.cc \
                $(top_srcdir)/src/netmate/constants.cc \
                $(top_srcdir)/src/lib/getopt_long/getopt_long.c \
		$(top_srcdir)/src/lib/getpass/getpass.c

nmrsh_LDADD   = @READLINELIB@ @SSLLIB@ @MPATROLLIB@ @CURLLIB@ @XMLLIB@ @XSLTLIB@ \
                $(top_builddir)/src/lib/getopt_long/libgetopt_long.a \
		$(top_builddir)/src/lib/getpass/libgetpass.a

INCLUDES      = -I$(top_srcdir) -I$(top_srcdir)/src/include -I$(top_srcdir)/src/netmate -I$(top_srcdir)/src/lib/getopt_long -I$(top_srcdir)/src/lib/getpass

$(top_builddir)/src/lib/getopt_long/libgetopt_long.a:
	cd $(top_builddir)/src/lib/getopt_long ; $(MAKE)

$(top_builddir)/src/lib/getpass/libgetpass.a:
	cd $(top_builddir)/src/lib/getpass ; $(MAKE)

else
all:
	@echo "WARNING: not building nmrsh because libxslt and/or libcurl are missing!"
endif
