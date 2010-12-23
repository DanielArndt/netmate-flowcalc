
/*  \file   MeterComponent.h

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

    $Id: MeterComponent.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _METERCOMPONENT_H_
#define _METERCOMPONENT_H_


#include "stdincpp.h"
#include "Error.h"
#include "Logger.h"
#include "PerfTimer.h"
#include "CommandLineArgs.h"
#include "ConfigManager.h"
#include "Event.h"
#include "Threads.h"
#include "httpd.h"


// forward declaration
class MeterComponent;

//! make an ft_t from fd and mode
inline fd_t make_fd(int fd, int mode)
{
    fd_t _fd;

    _fd.fd = fd;
    _fd.mode = mode;

    return _fd;
}

//! reverse sort operator according to fd
struct ltfd
{
    bool operator()(const fd_t f1, const fd_t f2) const
    {
        return (f2.fd < f1.fd);
    }
};

//! file descriptor list
typedef map<fd_t, MeterComponent*,ltfd>            fdList_t;
typedef map<fd_t, MeterComponent*,ltfd>::iterator  fdListIter_t;

//! event vector list
typedef vector<Event *>            eventVec_t;
typedef vector<Event *>::iterator  eventVecIter_t;

/*!
  this class represents an abstract superclass from which all the
  components (i.e. classes) that are aggregated in the MeterCore
  must be derived
*/

class MeterComponent
{
  private:
    
    int running;
    string cname;    //!< component name
    fdList_t fdList; //!< FIXME missing documentation

    /*!
      this function is called upon the creation of
      the objects thread. Its only purpose is to call this
      objects 'main' function.
    */
    static void *thread_func(void *arg);
    
  protected:

    //! flag, is equal to 1 if component runs in separate thread
    int threaded;

#ifdef ENABLE_THREADS
    thread_t thread;  //!< the component's thread
    mutex_t maccess;  //!< maccess semaphore to block multiple crit. section
    thread_cond_t doneCond; //!< condition for signalling we are done (idle)
#endif

    Logger *log;        //!< Logger used by this component
    int ch;             //!< assigned logging channel
    PerfTimer *perf;    //!< link to PerfTimer 
    ConfigManager *cnf; //!< link to configuration manager (for cfg file query)

    //! add file descriptor to internal list
    void addFd(int fd, int mode=FD_RD)
    {
        fd_t _fd;

        _fd.fd = fd;
        _fd.mode = mode;
        fdList[_fd] = this;
    }
    
    //! remove file descriptor from internal list
    void removeFd(int fd)
    {
        fd_t _fd;

        _fd.fd = fd;
        fdList.erase(_fd);
    }
    

  public:
        
    /*!  \brief  construct a new MeterComponent base
      \arg \c theCore - a link to the MeterCore that this object is part of 
      \arg \c cname - textual name of this component (for logging purposes)
    */
    MeterComponent(ConfigManager *_cnf, string name, int thread=0 );

    //! destroy meter component, to be overloaded
    virtual ~MeterComponent();

    // thread functions

    /*! \short   start the component as thread (if threaded)

        this starts the objects thread function main 
        \throws \c Error - if a thread cannot be created
    */
    virtual void run(void);

    //! stops components thread function
    virtual void stop(void);

    //! thread main function
    virtual void main(void);

    //! merge file descriptors from internal list with list   
    void mergeFDs(fdList_t *list)
    {
        copy(fdList.begin(), fdList.end(),
             inserter(*list, list->begin()));
    }
    
    //! callback in case of a file descriptor event
    virtual int handleFDEvent(eventVec_t *e, fd_set *wset, fd_set *rset, fd_sets_t *fds)
    {
        // To be implemented in derived classes
        return 0;
    }

    //! add command line arguments
    static void add_args(CommandLineArgs *args)
    {
        // To be implemented in derived classes
    }

    //! return the config group of the component
    virtual string getConfigGroup() {
        return "";
    }

    // helper functions for obtaining config strings

    string getConfStr( string module, string name ) {
        return cnf->getValue( name, getConfigGroup(), module );
    }

    string getConfStr( string name ) {
        return cnf->getValue( name, getConfigGroup() );
    }

};


#endif // _METERCOMPONENT_H_
