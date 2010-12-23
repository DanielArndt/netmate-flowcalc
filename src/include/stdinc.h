
/*!\file stdinc.h

    Copyright 2003-2004 Fraunhofer Institute for Open Communication Systems (FOKUS),
                        Berlin, Germany

    This file is part of Network Measurement and Accounting System (NETMATE).

    NETMATE is free software; you can redistribute it and/or modify 
    it under the terms of the GNU General Public License as published by 
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    NETMATE is distributed in the hope that it will be useful, 
    but WITHOUT ANY WARRANTY; without even the implied warranty of 
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this software; if not, write to the Free Software 
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Description:
    Include this file (stdinc.h) in every .h file.
    This .h file includes all the standard include
    files such as stdio.h, stdlib.h and others

    $Id: stdinc.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef __STDINC_H
#define __STDINC_H

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

#include <string.h>
#include <math.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>

#ifdef LINUX
#include <values.h>
#include <semaphore.h>
#endif

#ifdef HAVE_FLOAT_H
#include <float.h>
#define MINFLOAT  FLT_MIN
#define MINDOUBLE DBL_MIN
#define MAXDOUBLE DBL_MAX
#endif

#include <ctype.h>
#include <dlfcn.h>

#ifdef ENABLE_THREADS
#include <pthread.h> 
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <poll.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/wait.h>
#include <libgen.h>
#include <pcap.h>

#ifdef HAVE_ETHER_H
#include <ether.h>
#endif
#ifdef HAVE_NET_BPF_H
#include <net/bpf.h>
#endif
#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif

#ifdef HAVE_IPTABLES_H
#include <iptables.h>
#endif
#ifdef HAVE_LIBIPULOG_LIBIPULOG_H
#include "libipulog/libipulog.h"
#endif
#ifdef HAVE_LIBIPTC_LIBIPTC_H
#include "libiptc/libiptc.h"
#endif


#ifdef HRTIME_PROFILING
  #include <hrtime.h>
#endif

#ifdef ENABLE_MP
  #include "mpatrol.h"
#endif


#ifndef ULLONG_MAX
  #define ULLONG_MAX   18446744073709551615ULL
#endif
#ifndef LLONG_MAX
  #define LLONG_MAX    9223372036854775807LL
#endif
#ifndef LLONG_MIN
  #define LLONG_MIN    (-LLONG_MAX - 1LL)
#endif

#ifndef HAVE_STRTOF
#define strtof(nptr,endptr) strtod(nptr,endptr)
#endif

// global for alarm timeout (implemented in Meter.cc)
extern int g_timeout;

#endif // __STDINC_H

