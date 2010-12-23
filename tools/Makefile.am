
EXTRA_DIST = ports.sh protocols.sh

bin_PROGRAMS = genrules 

if ENABLE_DEBUG
  AM_CFLAGS = -g -fno-inline -DDEBUG -DPROFILING
else
  AM_CFLAGS = -O2 
endif

genrules_SOURCES = genrules.c 

