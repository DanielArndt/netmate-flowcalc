noinst_LIBRARIES = libgetopt_long.a

if ENABLE_DEBUG
  AM_CFLAGS = -g -fno-inline -DDEBUG @USE_SSL@ 
else
  AM_CFLAGS = -O2 @USE_SSL@ 
endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/include

libgetopt_long_a_SOURCES = getopt_long.h getopt_long.c

