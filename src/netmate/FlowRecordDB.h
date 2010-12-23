
/*! \file   FlowRecordDB.h

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

    $Id: FlowRecordDB.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _FLOWRECORDDB_H_
#define _FLOWRECORDDB_H_


#include "stdincpp.h"
#include "Logger.h"
#include "FlowRecord.h"
#include "Threads.h"
#include "Rule.h"


//! extended flow record
typedef struct 
{
    //! export module name
    expnames_t expmods;
    FlowRecord *fr;
} flowRec_t;

//! flow record queue (push based export)
typedef deque<flowRec_t*>            flowRecExpList_t;
typedef deque<flowRec_t*>::iterator  flowRecExpListIter_t;

//! flow record DB index by rule id (pull based export)
typedef multimap<int, flowRec_t>            flowRecDB_t;
typedef multimap<int, flowRec_t>::iterator  flowRecDBIter_t;

/*! \short   save measurement results and store these until retrieval
  
    The FlowRecordDB class will receive flow records from the packet
    processor and store them into a queue and a map. The queue is used
    for push based export (also called FIFO). The map is used for pull
    based export (also called DB).
*/

class FlowRecordDB
{
  private:

    Logger *log; //!< link to global logger
    int ch;      //!< logging channel used by objects of this class

    int threaded; //!< flag, indicates whether db runs as a separate class

    //! flow record db
    flowRecDB_t db;

    //! flow record FIFO
    flowRecExpList_t elist;
    
#ifdef ENABLE_THREADS
    mutex_t maccess;
    thread_cond_t  emptyListCond;
#endif

  public:

    //! construct and initialize a FlowRecordDB object
    FlowRecordDB( int threaded );

    //! destroy a FlowRecordDB object
    ~FlowRecordDB();

    /*! \short   store measurement data into repository
        \arg \c ruleId  unique rule id
        \arg \c expmod  export module name
        \arg \c fr  pointer to the flow record
    */
    void storeData( int ruleId, expnames_t expmods, FlowRecord *fr );

    /*! \short   get flow record from DB
        \arg \c ruleId  unique rule id
        \arg \c expmod  export module name
        FIXME need expmod as parameter?
    */
    FlowRecord *getData( int ruleId, string expmod );

    //! delete all flow records of rule
    void delData(int ruleID);

    //! get next flow record from the FIFO
    flowRec_t *getNextRec();

    //! dump a FlowRecordDB object
    void dump( ostream &os );

    //! get number of queued records
    int getRecords()
    {
        return elist.size();
    }
};


//! overload for <<, so that a FlowRecordDB object can be thrown into an iostream
ostream& operator<< ( ostream &os, FlowRecordDB &ds );


#endif // _DATASTORAGE_H_
