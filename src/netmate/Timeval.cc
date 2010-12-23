
/*  \file   Timeval.cc

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
    functions for comparing, adding and subtracting timevals

    $Id: Timeval.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "Timeval.h"
#include "Error.h"


// global time from tcpdump file
struct timeval Timeval::g_time = {0, 0};
int Timeval::onlineCap = 1;


int Timeval::cmp(struct timeval t1, struct timeval t2)
{
    
    if (t1.tv_sec < t2.tv_sec) {
	return -1;
    } else if (t1.tv_sec > t2.tv_sec) {
	return +1;
    } else {
	if (t1.tv_usec < t2.tv_usec) {
	    return -1;
	} else if (t1.tv_usec > t2.tv_usec) {
	    return +1;
	} else {
	    return 0;
	}
    }
}

struct timeval Timeval::add(struct timeval a, struct timeval b)
{
    struct timeval rv;

    rv.tv_sec = a.tv_sec + b.tv_sec;
    rv.tv_usec = a.tv_usec + b.tv_usec;
    if (rv.tv_usec >= 1000000) { 
	rv.tv_usec -= 1000000;
	rv.tv_sec++;
    }

    return rv;
}

struct timeval Timeval::sub0(struct timeval num, struct timeval sub)
{
    struct timeval rv;

    rv.tv_sec = num.tv_sec - sub.tv_sec;
    rv.tv_usec = num.tv_usec - sub.tv_usec;
    if (rv.tv_usec < 0) { 
	rv.tv_usec += 1000000;
	rv.tv_sec--;
    }
    if (rv.tv_sec < 0) { 
	rv.tv_sec = 0;
	rv.tv_usec = 0;
    }

    return rv;
}

// function for reading the current time
// the time will either be the current system time or the time derived
// from a pcap trace file 
int Timeval::gettimeofday(struct timeval *tv, struct timezone *tz)
{
  if (!onlineCap) {
    // use timestamps from the trace
    tv->tv_sec = g_time.tv_sec;
    tv->tv_usec = g_time.tv_usec;
    return 0;
  } else {
    return ::gettimeofday(tv, tz);
  }

}
 
time_t Timeval::time(time_t *t)
{
  if (!onlineCap) {
    if (t != NULL) {
      *t = g_time.tv_sec;
    }
    return g_time.tv_sec;
  } else {
    return ::time(t);
  }
}


// set the time (used when reading the time from a pcap file)
int Timeval::settimeofday(const struct timeval *tv)
{
  if ( (tv->tv_sec*1e6 + tv->tv_usec) < (g_time.tv_sec*1e6 + g_time.tv_usec) ) {
    cerr << "Encountered a packet from the past" << endl;
    return -1;
  } 

  g_time.tv_sec = tv->tv_sec;
  g_time.tv_usec = tv->tv_usec;

  return 0;
}
