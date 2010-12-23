noinst_LIBRARIES = libgetpass.a

EXTRA_DIST = COPYING

if ENABLE_DEBUG
  AM_CFLAGS = -g -fno-inline -DDEBUG @USE_SSL@ 
else
  AM_CFLAGS = -O2 @USE_SSL@ 
endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/include

libgetpass_a_SOURCES = getpass.h getpass.c 

