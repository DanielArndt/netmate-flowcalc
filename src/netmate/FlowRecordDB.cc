
/*!\file   FlowRecordDB.cc

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
    store flow records for all flows

    $Id: FlowRecordDB.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "FlowRecordDB.h"


/* ------------------------- FlowRecordDB ------------------------- */

FlowRecordDB::FlowRecordDB( int thr) : threaded(thr)
{
    log = Logger::getInstance();
    ch = log->createChannel("FlowRecordDB");

    log->log(ch,"Starting");

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexInit(&maccess);
        threadCondInit(&emptyListCond);
    }
#endif
}


/* ------------------------- ~FlowRecordDB ------------------------- */

FlowRecordDB::~FlowRecordDB()
{
    flowRecDBIter_t iter;
    flowRecExpListIter_t iter2;

    log->dlog(ch, "shutdown");
  
#ifdef ENABLE_THREADS
    if (threaded) {
        //mutexLock(&maccess);
    }
#endif

    for (iter = db.begin(); iter != db.end(); iter++ ) {
        // delete flow record
        saveDelete(iter->second.fr);
    }

    // delete queue entries
    for (iter2 = elist.begin(); iter2 != elist.end(); iter2++) {
        saveDelete(*iter2);
    }
    
#ifdef ENABLE_THREADS
    if (threaded) {
        //mutexUnlock(&maccess);
        mutexDestroy(&maccess);
        threadCondDestroy(&emptyListCond);
    }
#endif
}


void FlowRecordDB::storeData(int ruleId, expnames_t expmods, FlowRecord *fr)
{
    flowRecDBIter_t iter;
    pair<flowRecDBIter_t,flowRecDBIter_t> res;
    flowRec_t *new_rec;
 
    // expmods can be empty indicating all rules should be exported
    if ((ruleId < 0) || (fr == NULL )) {
        if (fr != NULL) {
            saveDelete(fr);
        }
        return;
    }

#ifdef DEBUG
    string exps;
    for (expnamesIter_t i = expmods.begin(); i != expmods.end(); i++) {
        exps += *i + " ";
    }
    log->dlog(ch, "new measurement data for rule #%d (%s)", ruleId, exps.c_str());
#endif

    AUTOLOCK(threaded, &maccess);

    new_rec = new flowRec_t;
    new_rec->expmods = expmods;
    new_rec->fr = fr;

    // insert at the end of push list
    elist.push_back(new_rec);

    // lookup existing flow record
    res = db.equal_range(ruleId);
    iter = res.first;
    
    int found = 0;
    while ((iter != db.end()) && (iter != res.second)) { 
        if (iter->second.expmods == expmods) {
            // check if old flow record should be deleted
	   
            if (iter->second.fr->getDelete()) {
	       // delete straightaway
	      saveDelete(iter->second.fr);    
	    } else {
	       // mark it for later deletion
	      iter->second.fr->markForDelete();
	    }

	    // keep reference to new flow record
            iter->second.fr = fr;
            
	    found = 1;
	    break;
        }
        
        iter++;
    } 


    if (!found) {
        // insert new flow record into db
        db.insert(make_pair(ruleId, *new_rec));
    }

#ifdef ENABLE_THREADS
    // if we go from 0 to 1 flow records in the list
    if (threaded && (elist.size() == 1)) {
        threadCondSignal(&emptyListCond);
    }
#endif
  
}

void FlowRecordDB::delData(int ruleId)
{
    flowRecDBIter_t tmp, iter;
    pair<flowRecDBIter_t,flowRecDBIter_t> res;
    
    if (ruleId < 0) {
        return;
    }

    AUTOLOCK(threaded, &maccess);

    res = db.equal_range(ruleId);
    iter = res.first;

    while ((iter != db.end()) && (iter != res.second)) {
        // free flow record
        saveDelete(iter->second.fr);
        iter->second.fr = NULL;
        
        // remove entry
        tmp = iter;
        iter++;

        db.erase(tmp);
    } 
}

FlowRecord *FlowRecordDB::getData( int ruleId, string expmod )
{
    flowRecDBIter_t iter;
    pair<flowRecDBIter_t,flowRecDBIter_t> res;
    
    if ((ruleId < 0) || expmod.empty()) {
        return NULL;
    }

    AUTOLOCK(threaded, &maccess);

    res = db.equal_range(ruleId);
    iter = res.first;

    while ((iter != db.end()) && (iter != res.second)) { 
        if (iter->second.expmods.find(expmod) != iter->second.expmods.end()) {
            return iter->second.fr;
        }
        
        iter++;
    } 
    
    return NULL;
}


flowRec_t *FlowRecordDB::getNextRec()
{
    flowRec_t *tmp = NULL;

    AUTOLOCK(threaded, &maccess);

#ifdef ENABLE_THREADS
    if (threaded) {  
        while (elist.empty()) {
            threadCondWait(&emptyListCond, &maccess);
        }

        tmp = elist.front();
        elist.pop_front();
    } else {
#endif
        if (!elist.empty()) {
            tmp = elist.front();
            elist.pop_front();
        }

#ifdef ENABLE_THREADS
    }
#endif

#ifdef DEBUG
    if (tmp != NULL) {
        string exps;
        for (expnamesIter_t i = tmp->expmods.begin(); i != tmp->expmods.end(); i++) {
            exps += *i + " ";
        }
        log->dlog(ch, "get flow record for rule #%d (%s)", tmp->fr->getRuleId(), exps.c_str());
    }
#endif

    return tmp;
}


/* ------------------------- dump ------------------------- */

void FlowRecordDB::dump( ostream &os )
{
    flowRecDBIter_t iter;

    AUTOLOCK(threaded, &maccess);

    for (iter = db.begin(); iter != db.end(); iter++ ) {
        string exps;
        for (expnamesIter_t i = iter->second.expmods.begin(); i != iter->second.expmods.end(); i++) {
            exps += *i + " ";
        }
        os << exps << ":" << endl;
        os << iter->second.fr; // dump a FlowRecord object
        
    }
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, FlowRecordDB &ds )
{
    ds.dump(os);
    return os;
}
