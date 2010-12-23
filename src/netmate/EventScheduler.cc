
/*!\file   EventScheduler.cc

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
    event scheduling

    $Id: EventScheduler.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "Error.h"
#include "EventScheduler.h"
#include "Meter.h"
#include "Timeval.h"


// for reading from trace files that have time gaps: make sure that
// event is not in the past
// this will cause all events to be 'suspended' during the time gap
#define TIME_GAP_HACK 0


// min timeout for select() in us (10ms minimum on current UNIX!)
const int MIN_TIMEOUT = 10000;


/* ------------------------- EventScheduler ------------------------- */

EventScheduler::EventScheduler() 
{

    log = Logger::getInstance();
    ch = log->createChannel("EventScheduler");
#ifdef DEBUG
    log->dlog(ch, "Starting");
#endif
}


/* ------------------------- ~EventScheduler ------------------------- */

EventScheduler::~EventScheduler()
{
    eventListIter_t iter;

#ifdef DEBUG
    log->dlog(ch, "Shutdown");
#endif

    // free all stored events
    for (iter = events.begin(); iter != events.end(); iter++) {
        saveDelete(iter->second);
    }
}


/* ------------------------- addEvent ------------------------- */

void EventScheduler::addEvent(Event *ev)
{
#ifdef DEBUG
    log->dlog(ch,"new event %s", eventNames[ev->getType()].c_str());
#endif
   
    events.insert(make_pair(ev->getTime(),ev));
}


/*! deleting is costly but it should occur much less than exporting
    data or flow timeouts assuming the lifetimes of rules are reasonably large
*/
void EventScheduler::delRuleEvents(int uid)
{
    int ret = 0;
    eventListIter_t iter, tmp;

    // search linearly through list for rule with given ID and delete entries
    iter = events.begin();
    while (iter != events.end()) {
        tmp = iter;
        iter++;
        
        ret = tmp->second->deleteRule(uid);
        if (ret == 1) {
            // ret = 1 means rule was present in event but other rules are still in
            // the event
#ifdef DEBUG
            log->dlog(ch,"remove rule %d from event %s", uid, 
                      eventNames[tmp->second->getType()].c_str());
#endif
        } else if (ret == 2) {
            // ret=2 means the event is now empty and therefore can be deleted
#ifdef DEBUG
            log->dlog(ch,"remove event %s", eventNames[tmp->second->getType()].c_str());
#endif
           
            saveDelete(tmp->second);
            events.erase(tmp);
        } 
    }
}


Event *EventScheduler::getNextEvent()
{
    Event *ev;
    
    if (events.begin() != events.end()) {
        ev = events.begin()->second;
        // dequeue event
        events.erase(events.begin());
        // the receiver is responsible for
        // returning or freeing the event
        return ev;
    } else {
        return NULL;
    }
}


void EventScheduler::reschedNextEvent(Event *ev)
{
    assert(ev != NULL);
   
    if (ev->getIval() > 0) {

#ifdef DEBUG
        log->dlog(ch,"requeue event %s", eventNames[ev->getType()].c_str());
#endif	
        // recurring event so calculate next expiry time
        ev->advance();

#ifdef TIME_GAP_HACK
	struct timeval now;
	Timeval::gettimeofday(&now, NULL);
	struct timeval d = Timeval::sub0(now, ev->getTime());
	if (d.tv_sec > 0) {
	  ev->setTime(now);
	}
#endif

        // and requeue it
        events.insert(make_pair(ev->getTime(),ev));
    } else {
#ifdef DEBUG
        log->dlog(ch,"remove event %s", eventNames[ev->getType()].c_str());
#endif
        saveDelete(ev);
    } 
}


struct timeval EventScheduler::getNextEventTime()
{
    struct timeval rv = {0, MIN_TIMEOUT};
    struct timeval now;
    char c = 'A';

    if (events.begin() != events.end()) {
        Event *ev = events.begin()->second;
	Timeval::gettimeofday(&now, NULL);

        rv = Timeval::sub0(ev->getTime(), now);
        // be 100us fuzzy
        if ((rv.tv_sec == 0) && (rv.tv_usec<100)) {
#ifdef DEBUG
            log->dlog(ch,"expired event %s", eventNames[ev->getType()].c_str());
#endif
            write(Meter::s_sigpipe[1], &c, 1);
        }
    } 
    return rv;
}


/* ------------------------- dump ------------------------- */

void EventScheduler::dump(ostream &os)
{
    struct timeval now;
    eventListIter_t iter;
    
    gettimeofday(&now, NULL);
    
    os << "EventScheduler dump : \n";
    
    // output all scheduled Events to ostream
    for (iter = events.begin(); iter != events.end(); iter++) {
        struct timeval rv = Timeval::sub0(iter->first, now);
        os << "at t = " << rv.tv_sec * 1e6 + rv.tv_usec << " -> " 
           << eventNames[iter->second->getType()] << endl;
    }
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< (ostream &os, EventScheduler &dc)
{
    dc.dump(os);
    return os;
}
