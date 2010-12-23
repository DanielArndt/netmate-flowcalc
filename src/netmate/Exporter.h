
/*! \file   Exporter.h

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
    export flow information 

    $Id: Exporter.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _EXPORTER_H_
#define _EXPORTER_H_


#include "stdincpp.h"
#include "MeterComponent.h"
#include "Rule.h"
#include "FlowRecord.h"
#include "ModuleLoader.h"
#include "ExportModule.h"
#include "EventScheduler.h"
#include "FlowRecordDB.h"


//! export action
typedef struct
{
    ExportModule *module;
    ExportModuleInterface_t *mapi; // module API
    void *expData;
} expAction_t;

//! list of export modules
typedef vector<expAction_t> expActionList_t;
typedef vector<expAction_t>::iterator expActionListIter_t;

typedef struct {
    //! time stamp of last packet export action taken
    //! =0 indicates the flow was exported at least once
    time_t lastExp;
    //! list of export modules to be used by this rule (task)
    expActionList_t mods;
} exportActions_t;

//! list of all export modules for all rules
typedef vector<exportActions_t>            exportActionList_t;
typedef vector<exportActions_t>::iterator  exportActionListIter_t;


/*! class that encapsulates pointers to all Export Modules and
    all stored flow data for one rule (task)
*/

class Exporter : public MeterComponent
{
  private:

    //! module loader
    ModuleLoader *loader;

    // export actions per rule 
    exportActionList_t exports;

    //! flow record database stores flow records
    FlowRecordDB *frdb;

  public:

    /*! \short construct and initialize a RecordTransmitter object

        \arg \c cnf        config manager
        \arg \c threaded   run as separate thread
        \arg \c moduleDir  action module directory
    */
    Exporter( ConfigManager *cnf, int threaded, string moduleDir = "" );
    
    //! destroy a RecordTransmitter object
    virtual ~Exporter();

    //! return the name of the configuration group queried by this class
    virtual string getConfigGroup() { return "EXPORTER"; }

    /*! \short export all data from flow record via export modules

        This method takes from the FlowRecord each MetricData entry
        (which encapsulates the data exported from one packet processing module)
        and exports this data using each of the configured export modules 
        configured by the rule identified by the ruleID

        \arg \c frec   - FlowRecord which contains export data from processing modules
        \arg \c expmod - FIXME add documentation
    */
    int exportFlowRecord( FlowRecord *frec, expnames_t expmods);

    int exportFlowRecord( FlowRecord *frec, string expmod)
      {
          expnames_t expmods;
          
          if (!expmod.empty()) {
              expmods.insert(expmod);
          }

          return exportFlowRecord(frec, expmods);
      }


    // check a ruleset (the filter part)
    virtual void checkRules(ruleDB_t *rules);

    // add rules
    virtual void addRules(ruleDB_t *rules, EventScheduler *evs);

    // delete rules
    virtual void delRules(ruleDB_t *rules);

    // check rule description (exprt part) for correctness
    int checkRule(Rule *r);

    /*! \short   add a Rule and its associated actions to rule list

        \pre     the ruleId has not yet been used in a call to addRule unless 
                 this ruleId has been freed again with delRule
        \arg \c  r - the configured rule
        \returns 0 - on success, <0 - else
    */
    int addRule( Rule *r, EventScheduler *evs );

    /*! \short   delete a Rule from PacketDispatcher's rule list

        \arg \c  ruleId  - number indicating matching rule for packet
        \returns 0 - on success, <0 - else
    */
    int delRule( Rule *r );

    //! FIXME add documentation
    virtual int handleFDEvent(eventVec_t *e, fd_set *rset, fd_set *wset, fd_sets_t *fds);


    //! the method to be started if the Exporter runs as a separate thread
    virtual void main();

    //! FIXME add documentation
    void storeData( int ruleId, expnames_t expmods, FlowRecord *fr)
    {
        frdb->storeData(ruleId, expmods, fr);
    }

    void storeData( int ruleId, string expmod, FlowRecord *fr)
    {
        expnames_t expmods;

        if (!expmod.empty()) {
            expmods.insert(expmod);
        }

        frdb->storeData(ruleId, expmods, fr);
    }

    //! dump a RecordTransmitter object
    void dump( ostream &os );

    virtual void waitUntilDone();
};


//! overload for <<, so that a RecordTransmitter object can be thrown into an ostream
ostream& operator<< ( ostream &os, Exporter &obj );


#endif // _EXPORTER_H_
