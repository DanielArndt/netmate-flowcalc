noinst_LIBRARIES = libhttpd.a

EXTRA_DIST = README

if ENABLE_DEBUG
  AM_CFLAGS = -g -fno-inline -DDEBUG @USE_SSL@ 
else
  AM_CFLAGS = -O2 @USE_SSL@ 
endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/include

libhttpd_a_SOURCES = httpd.h mime.c quote.c ssl.c request.c response.c httpd.c 

