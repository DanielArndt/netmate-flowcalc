
/*! \file  Threads.h

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

    definitions for OS independent wrappers for 
    OS specific thread functions and

    autolock class definition
*/

using namespace std;

#ifndef _THREADS_H_
#define _THREADS_H_

#include <pthread.h>


#ifdef ENABLE_THREADS
#ifdef HAVE_PTHREAD_H

typedef   pthread_t         thread_t;
typedef   pthread_cond_t    thread_cond_t;
typedef   pthread_mutex_t   mutex_t;

// thread functions

inline int threadCreate(thread_t *thread, void *(*thread_func)(void *), void *arg)
{
	return pthread_create(thread, NULL, thread_func, arg);
}

inline int threadCancel(thread_t thread)
{
    return pthread_cancel(thread);
}

inline int threadJoin(thread_t thread)
{
    return pthread_join(thread, NULL);
}

inline int threadSetCancelType(int type, int *oldtype)
{
    return pthread_setcanceltype(type, oldtype);
}

// mutex functions

inline int mutexInit(mutex_t *mutex)
{
	return pthread_mutex_init(mutex, NULL);
}

inline int mutexLock(mutex_t *mutex)
{
	return pthread_mutex_lock(mutex);
}

inline int mutexTrylock(mutex_t *mutex)
{
	return pthread_mutex_trylock(mutex);
}

inline int mutexUnlock(mutex_t *mutex)
{
	return pthread_mutex_unlock(mutex);
}

inline int mutexDestroy(mutex_t *mutex)
{
	return pthread_mutex_destroy(mutex);
}

// thread condition functions

inline int threadCondInit(thread_cond_t *cond)
{
	return pthread_cond_init(cond, NULL);
}

inline int threadCondDestroy(thread_cond_t *cond)
{
	return pthread_cond_destroy(cond);
}

inline int threadCondSignal(thread_cond_t *cond)
{
	return pthread_cond_signal(cond);
}

inline int threadCondWait(thread_cond_t *cond, mutex_t *mutex)
{
	return pthread_cond_wait(cond, mutex);
}

inline int threadCondTimedWait(thread_cond_t *cond, mutex_t *mutex, const struct timespec *abstime)
{
  return pthread_cond_timedwait(cond, mutex, abstime);
}

#else 

#warning  The threading code currently only works with POSIX threads (pthreads). \
          Configure without --enable-threads to get a running executable.

#endif


/*!< \brief defines an autolock class which wraps a mutex
  access semaphore. Upon destruction of an autolock
  the semaphore is unlocked again. Use as automatic
  (stack) variable
*/
struct autoLock
{
    int threaded;
    mutex_t *access;

    inline autoLock(int thr, mutex_t *mutex)
        : threaded(thr), access(mutex)
    {
        if (threaded) {
            mutexLock(access);
        }
    }

    inline ~autoLock() {
        if (threaded) {
            mutexUnlock(access);
        }
    }

    inline void unlock() {
        if (threaded) {
            mutexUnlock(access);
            threaded = 0;
        }
    }
};


#define AUTOLOCK(threaded, access)    autoLock _lock((threaded), (access));
                                        
#else // ENABLE_THREADS

#define AUTOLOCK(threaded, access)   // empty

#endif // ENABLE_THREADS


#endif // _THREADS_H_
