
/*! \file   netmate/CtrlComm.h

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
    meter control interface

    $Id: CtrlComm.h 748 2009-09-10 02:54:03Z szander $
*/


#ifndef _CTRLCOMM_H_
#define _CTRLCOMM_H_


#include "stdincpp.h"
#include "MeterComponent.h"
#include "httpd.h"
#include "MeterInfo.h"
#include "PageRepository.h"



//! ctrlcomm flags 
const int LOG_CONNECT = 0x1;
const int LOG_COMMAND = 0x2;


//! parameter list (name value pairs) 
typedef map<string,string> paramList_t;
typedef map<string,string>::iterator paramListIter_t;

//! command and parameter contained in a request
typedef struct
{
  string comm;
  paramList_t params;
} parseReq_t;


/*! the CtrlComm class is responsible for handling network connections to the meter.
    It uses HTTP as the underlying transport protocol (HTTPD)  
*/

class CtrlComm : public MeterComponent
{
  private:

    //! the last instance of CtrlComm for use by static method s_access_check
    static CtrlComm* s_instance;

    int  portnum; //!< number of the port that will be listened on
    int flags;    //!< store configured features of CtrlComm

    configADList_t accessList; //!< the ACCESS/DENY list from the config file

    //! event returned to meter after handleFDEvent
    CtrlCommEvent *retEvent;

    //! pointer to event vector
    eventVec_t *retEventVec;

    //! repository for static preloaded response documents
    PageRepository pcache;

    //! preloaded template for replies
    string rtemplate;

    //! add to accessList all entries from list with host known by DNS
    void checkHosts( configADList_t &list, bool useIPv6 );

    //! parse an incoming request
    parseReq_t parseRequest(struct REQUEST *req);

  public:

    /*! \short   construct and initialize a CtrlComm object

        \arg \c cnf        link to config manager
        \arg \c threaded   if 1 run as separate thread
    */
    CtrlComm(ConfigManager *cnf, int threaded=0);
    

    //!  destroy a CtrlComm object
    ~CtrlComm();

    /*! \brief  callback: check if host is allowed to talk to this meter instance
        \returns 0 if allowed, -1 else
    */

    static inline int s_access_check(char *host, char *user) {
        return s_instance->accessCheck( host, user );
    }

    //! callback method: log incoming request
    static inline int s_log_request(struct REQUEST *req, time_t now)
    {
        return s_instance->logAccess(req, now );
    }

    //! callback method: log communication error
    static inline int s_log_error(int eno, int loglevel, char *txt, char *peerhost)
    {
        return s_instance->logError(eno, loglevel, txt, peerhost);
    }
    
    // callback method: parse request
    static inline int s_parse_request(struct REQUEST *req)
    {
        return s_instance->processCmd(req);
    }

    
    /*! \brief  check if host is allowed to talk to this meter instance
        \returns 0 if allowed, -1 else
    */    
    int accessCheck(char *host, char *user);
    
    //! log access
    int logAccess(struct REQUEST *req, time_t now );
    
    //! log error
    int logError(int eno, int loglevel, char *txt, char *peerhost);
    
    //! handle file descriptor event
    int handleFDEvent(eventVec_t *e, fd_set *rset, fd_set *wset, fd_sets_t *fds);
    
    //! send OK response message
    void sendMsg(string msg, struct REQUEST *req, fd_sets_t *fds, int quote=1);
    
    //! send error response message
    void sendErrMsg(string msg, struct REQUEST *req, fd_sets_t *fds);
    
    //! check whether or not a feature has been enabled on startup
    int isEnabled( int feature )
    {
        return ( flags & feature ) != 0;
    }

    //! get a timeout value ; ctrlcomm will be called in regular timeout intervals
    int getTimeout()
    {
        return httpd_get_keepalive();
    }

    /*! \short  process an incomming request

        parse request via sub function and queue event(s)

        \arg \c req   the incmoing request from HTTPD
        \returns      0 on success
    */
    int processCmd(struct REQUEST *req );

    //! add a measurement task to the currently active tasks
    char *processAddTask(parseReq_t *preq );

    //! delete a currently running measurement task
    char *processDelTask(parseReq_t *preq );

    //! return meter information (tasklist,status,modlist,metercaps)
    char *processGetInfo(parseReq_t *preq );

    //! return process module information
    char *processGetModInfo(parseReq_t *preq );

    /*! \short  convert string to new string with < > & replaced by XML representation

        \arg \c s - the string to convert
        \returns  a new string which represents s but has < > & characters replaced by &...;
    */
    static string xmlQuote(string s);

    //! dump a CtrlComm object
    void dump( ostream &os );

    //! return name of configuration group queried by this class
    virtual string getConfigGroup() { return "CONTROL"; }

};


//! overload for <<, so that a CtrlComm object can be thrown into an ostream
ostream& operator<< ( ostream &os, CtrlComm &obj );


#endif // _CTRLCOMM_H_
