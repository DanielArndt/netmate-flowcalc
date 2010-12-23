
/*  \file   MeterComponent.cc

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
    meter component base class, provides common functionality
    for all meter components (e.g. threads, fd handling, ...)

    $Id: MeterComponent.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "MeterComponent.h"



MeterComponent::MeterComponent(ConfigManager *_cnf, string name, int thread )
    :   running(0), cname(name), threaded(thread),  cnf(_cnf)
{
    log  = Logger::getInstance();
    ch   = log->createChannel( name );
    perf = PerfTimer::getInstance();

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexInit(&maccess);
	threadCondInit(&doneCond);
    }
#endif
}


MeterComponent::~MeterComponent()
{
#ifdef ENABLE_THREADS
    if (threaded) {
        mutexLock(&maccess);
        stop();
        mutexUnlock(&maccess);
        mutexDestroy(&maccess);
	threadCondDestroy(&doneCond);
    }
#endif
}


void MeterComponent::run(void)
{
#ifdef ENABLE_THREADS
    if (threaded && !running) {
	int res = threadCreate(&thread, thread_func, this);
	if (res != 0) {
	    throw Error("Cannot create thread within %s: %s",
			cname.c_str(), strerror(res));
	}
    
	running = 1;
    }
#endif
}


void MeterComponent::stop(void)
{
#ifdef ENABLE_THREADS
    if (threaded && running) {
	threadCancel(thread);
	threadJoin(thread);
	running = 0;
    }
#endif
}


void* MeterComponent::thread_func(void *arg)
{
#ifdef ENABLE_THREADS
    // asynch cancel
    threadSetCancelType(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    ((MeterComponent *)arg)->main();
#endif
    return NULL;
}


void MeterComponent::main(void)
{
    // nothing here
}


