
/*! \file   Event.h

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
    event classes for all events handled by meter

    $Id: Event.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _EVENT_H_
#define _EVENT_H_


#include "stdincpp.h"
#include "RuleFileParser.h"
#include "MeterInfo.h"
#include "ProcModule.h"
#include "ProcModuleInterface.h"
#include "ExportModuleInterface.h"


//! event numbers
typedef enum 
{
      PUSH_EXPORT = 0,
      PULL_EXPORT,
      FLOW_TIMEOUT,
      REMOVE_RULES,
      ADD_RULES,
      ACTIVATE_RULES,
      GET_INFO,
      GET_MODINFO,
      TEST,
      REMOVE_RULES_CTRLCOMM,
      ADD_RULES_CTRLCOMM,
      PROC_MODULE_TIMER,
      EXPORT_MODULE_TIMER,
      CTRLCOMM_TIMER,
} event_t;

//! event names for dump method
const string eventNames[] = 
{
      "Push-export",
      "Pull-export",
      "Flow-timeout",
      "Remove-rules",
      "Add-rules",
      "Activate-rules",
      "Get-info",
      "Get-module-info",
      "Test",
      "Remove-rules-ctrlcomm",
      "Add-rules-ctrlcomm",
      "Proc-module-timer",
      "Export-module-timer",
      "Ctrlcomm-timer",
};

/* ------------------------- Event class ------------------------- */

/*! \short   basic event element that is the base class of all 
             other events that can be stored in the event queue 
             of EventScheduler
*/

class Event {
      
  private:

    //! type of the event   
    event_t type;    
    
    //!< time for next processing of event [sec since Epoch]
    struct timeval when;
    
    //!< interval between two event processings [msec]
    unsigned long interval;
    
    //! align events on time boundaries
    void doAlign();
    
  public:
  
    /*! \short  create an event at an absolute time
        \arg \c type  type of the event
        \arg \c time  absolute timestamp when the event is due
        \arg \c ival  interval (in ms) if the event is recurrent
        \arg \c align  align the event on the next ival
    */
    Event(event_t type, struct timeval time, unsigned long ival=0, 
	  int align=0);
    
    /*! \short  create an event relative to current time
        \arg \c type  type of the event
        \arg \c offs_sec  sec offset from now when the event is due
        \arg \c offs_used  usec offset from now when the event is due
        \arg \c ival  interval (in ms) if the event is recurrent
        \arg \c align  align the event on the next ival
    */
    Event(event_t type, time_t offs_sec, time_t offs_usec = 0, 
    	  unsigned long ival=0, int align=0);
    
    /*! \short  create an event at the current time (now)
        \arg \c type  type of the event
        \arg \c ival  interval (in ms) if the event is recurrent
        \arg \c align  align the event on the next ival
    */
    Event(event_t type, unsigned long ival=0, int align=0);
    
    virtual ~Event() {}
    
    //! get event type
    event_t getType() 
    { 
        return type;
    }

    int isType( event_t atype )
    { 
        return (type == atype);
    }

    //! get expiry time
    struct timeval getTime()                        
    {
        return when;
    }
    
    //! get interval
    unsigned long getIval()                        
    {
        return interval;
    }
    
    //! set interval
    void setInterval( unsigned long ival )        
    {
        interval = ival;
    }
    
    //! set expiry time
    void setTime( struct timeval newTime )         
    {
        when = newTime;
    }
    
    //! set expiry time (full sec)
    void setTime(time_t newTimeSec)
    { 
        when.tv_sec = newTimeSec;
        when.tv_usec = 0;
    }
    
    //! get next expiry time (recurrent events)
    void advance();
    
    virtual void dump( ostream &os );
    
    //! delete rules stored in this event
    virtual int deleteRule(int uid) 
    {
	return 0;
    }
};


//! base class for all ctrlcomm events, contains pointer to request
class CtrlCommEvent: public Event
{
  private:  
    struct REQUEST *req;

  public:
    //! ctrlcomm events always expire now
    CtrlCommEvent(event_t type, unsigned long ival=0, int align=0)
      : Event(type, ival, align) {}

    virtual ~CtrlCommEvent() {}

    //! get request pointer
    struct REQUEST *getReq()
    { 
	return req;
    }
    
    //! set request pointer
    void setReq(struct REQUEST *r)
    {
	req = r;
    }
};


//! test event
class TestEvent : public Event
{
  private:
   
  public:

    TestEvent(struct timeval time, unsigned long ival=0)
      : Event(TEST, time, ival) {}

    TestEvent(time_t offs_sec, time_t offs_usec, unsigned long ival=0)
      : Event(TEST, offs_sec, offs_usec, ival) {}

    TestEvent(unsigned long ival=0)
      : Event(TEST, ival) {}
};

/* --------------------------------- events ------------------------------ */


class AddRulesEvent : public Event
{
  private:
    string fileName;

  public:

    AddRulesEvent(string fname, int mapi=0) 
      : Event(ADD_RULES), fileName(fname) 
    {
        
    }

    string getFileName()
    {
        return fileName;
    }
};


class ActivateRulesEvent : public Event
{
  private:
    ruleDB_t rules;

  public:

    ActivateRulesEvent(struct timeval time, ruleDB_t &r) 
      : Event(ACTIVATE_RULES, time), rules(r) {}

     ActivateRulesEvent(time_t offs_sec, ruleDB_t &r) 
      : Event(ACTIVATE_RULES, offs_sec), rules(r) {}

     ActivateRulesEvent(ruleDB_t &r) 
      : Event(ACTIVATE_RULES), rules(r) {}

     ruleDB_t *getRules()
     {
         return &rules;
     }

     int deleteRule(int uid)
     {
         int ret = 0;
         ruleDBIter_t iter;
           
         for (iter=rules.begin(); iter != rules.end(); iter++) {
             if ((*iter)->getUId() == uid) {
                 rules.erase(iter);
                 ret++;
                 break;
             }   
         }
         
         if (rules.empty()) {
             return ++ret;
         }
         
         return ret;
     }
};


class RemoveRulesEvent : public Event
{
  private:
    ruleDB_t rules;

  public:

    RemoveRulesEvent(struct timeval time, ruleDB_t &r) 
      : Event(REMOVE_RULES, time), rules(r) {}

    RemoveRulesEvent(time_t offs_sec, ruleDB_t &r) 
      : Event(REMOVE_RULES, offs_sec), rules(r) {}
    
    RemoveRulesEvent(ruleDB_t &r) 
      : Event(REMOVE_RULES), rules(r) {}

    ruleDB_t *getRules()
    {
        return &rules;
    }
    
    int deleteRule(int uid)
    {
        int ret = 0;
        ruleDBIter_t iter;
        
        for (iter=rules.begin(); iter != rules.end(); iter++) {
            if ((*iter)->getUId() == uid) {
                rules.erase(iter);
                ret++;
                break;
            }   
        }
          
        if (rules.empty()) {
            return ++ret;
        }
          
        return ret;
    }
};


class PushExportEvent : public Event
{
  private:
    ruleDB_t rules;
    expnames_t exps;
    int final;

  public:

    PushExportEvent(struct timeval time, ruleDB_t &r, expnames_t e, unsigned long ival=0, int align=0) 
      : Event(PUSH_EXPORT, time, ival, align), rules(r), exps(e), final(0) {}

    PushExportEvent(time_t offs_sec, ruleDB_t &r, expnames_t e, unsigned long ival=0, int align=0) 
      : Event(PUSH_EXPORT, offs_sec, 0, ival, align), rules(r), exps(e), final(0) {}

    PushExportEvent(ruleDB_t &r, expnames_t e, unsigned long ival=0, int align=0) 
      : Event(PUSH_EXPORT, ival, align), rules(r), exps(e), final(0) {}

    ruleDB_t *getRules()
    {
        return &rules;
    }
    
    expnames_t getExpMods()
    {
        return exps;
    }

    int deleteRule(int uid)
    {
        int ret = 0;
        ruleDBIter_t iter;
           
        for (iter=rules.begin(); iter != rules.end(); iter++) {
            if ((*iter)->getUId() == uid) {
                rules.erase(iter);
                ret++;
                break;
            }   
        }
           
        if (rules.empty()) {
            return ++ret;
        }
        
        return ret;
    }

    void setFinal(int f)
    {
      final = f;
    }

    int isFinal() 
    {
      return final;
    }
};

class FlowTimeoutEvent : public Event
{
  private:
    int rid;
    unsigned long timeout;

  public:
    
    FlowTimeoutEvent(struct timeval time, int r, unsigned long to, unsigned long ival=0, int align=0) 
      : Event(FLOW_TIMEOUT, time, ival, align), rid(r), timeout(to) {}
      
      FlowTimeoutEvent(time_t offs_sec, int r, unsigned long to, unsigned long ival=0, int align=0) 
	: Event(FLOW_TIMEOUT, offs_sec,0,ival,align), rid(r), timeout(to) {}

	FlowTimeoutEvent(int r, unsigned long to, unsigned long ival=0, int align=0) 
	  : Event(FLOW_TIMEOUT,ival,align), rid(r), timeout(to) {}

    int getUId()
    {
        return rid;
    }
    
    unsigned long getTimeout()
    {
      return timeout;
    }
    
    int deleteRule(int uid)
    {
        int ret = 0;
        
        if (uid == rid) {
            ret = 2;
        }
        
        return ret;
    }
};


class ProcTimerEvent : public Event
{
private:

 public:
    int rid;
    int actid;
    unsigned int tmID;
    
public:

    ProcTimerEvent( int ruleID, int actID, timers_t *timer ) :
      Event( PROC_MODULE_TIMER, timer->ival_msec/1000, timer->ival_msec%1000,
             ((timer->flags & TM_RECURRING) ? timer->ival_msec : 0),
             timer->flags & TM_ALIGNED),
      rid(ruleID), 
      actid(actID),
      tmID(timer->id)
      {}
    
    int getRID() 
      { 
          return rid;
      }

    int getAID()
      {
          return actid;
      }

    unsigned int getTID()
      {
          return tmID;
      }
    
    int deleteRule(int uid)
    {
        int ret = 0;
        
        if (uid == rid) {
            ret = 2;
        }
        return ret;
    }
};


class ExportTimerEvent : public Event
{
private:

    unsigned int tmID;
    export_timeout_func_t tmFunc;
    
public:

    ExportTimerEvent( export_timeout_func_t timeout, timers_t *timer ) :
      Event( EXPORT_MODULE_TIMER,
             timer->ival_msec/1000, timer->ival_msec%1000,
             ((timer->flags & TM_RECURRING) ? timer->ival_msec : 0),
             timer->flags & TM_ALIGNED),
      tmID(timer->id),
      tmFunc(timeout)
      {}
    
    int signalTimeout() { return tmFunc( tmID ); }
};


class CtrlCommTimerEvent : public Event
{
    
  public:
    
    CtrlCommTimerEvent(time_t offs_sec, unsigned long ival=0, int align=0) 
      : Event(CTRLCOMM_TIMER, offs_sec,0,ival,align) {}
};


/* ------------------------------- ctrlcomm events ------------------------ */

class GetInfoEvent : public CtrlCommEvent
{
  private:
    infoList_t *infos;
   
  public:

    GetInfoEvent(infoList_t *i)
      : CtrlCommEvent(GET_INFO), infos(i) {}

    ~GetInfoEvent()
    {
        saveDelete(infos);
    }

    infoList_t *getInfos()
    {
        return infos;
    }
};


class GetModInfoEvent : public CtrlCommEvent
{
  private:

    string modname;
    
  public:

    GetModInfoEvent( string modulename )
      : CtrlCommEvent(GET_MODINFO), modname(modulename) {}

    ~GetModInfoEvent() {}

    string getModName()
    {
        return modname;
    }
};


//! add rules flags
const int ADD_RULES_MAPI   = 0x1;


class AddRulesCtrlEvent : public CtrlCommEvent
{
  private:
    int type;
    char *buf;
    int len;

  public:

    AddRulesCtrlEvent(char *b, int l, int mapi=0)
      : CtrlCommEvent(ADD_RULES_CTRLCOMM), type(0), len(l) 
    {
        buf = new char[len+1];
        memcpy(buf, b, len+1);
          
        if (mapi) {
            type |= ADD_RULES_MAPI;
        }
    }

    ~AddRulesCtrlEvent()
    {
        saveDeleteArr(buf);
    }

    int isMAPI()
    {
        return (type & ADD_RULES_MAPI);
    }

    char *getBuf()
    {
        return buf;
    }

    int getLen()
    {
        return len;
    }
};


class RemoveRulesCtrlEvent : public CtrlCommEvent
{
  private:
    string rule;

  public:
    
    RemoveRulesCtrlEvent(string r) 
      : CtrlCommEvent(REMOVE_RULES_CTRLCOMM), rule(r) {}

    string getRule()
    {
        return rule;
    }
};

/*  FIXME support or delete!
class PullExportEvent : public CtrlCommEvent
{
  private:
  

  public:

    PullExportEvent() 
      : CtrlCommEvent(PULL_EXPORT) {}

};
*/


//! overload for << so that an Event object can be thrown into an ostream
ostream& operator<< ( ostream &os, Event &ev );


#endif // _EVENT_H_
