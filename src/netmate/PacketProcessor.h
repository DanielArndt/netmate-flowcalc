
/*! \file   PacketProcessor.h

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
    manages and applies packet processing modules

    $Id: PacketProcessor.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _PACKETPROCESSOR_H_
#define _PACKETPROCESSOR_H_


#include "stdincpp.h"
#include "ProcModule.h"
#include "FlowRecord.h"
#include "ModuleLoader.h"
#include "Rule.h"
#include "MeterComponent.h"
#include "Error.h"
#include "Logger.h"
#include "PacketQueue.h"
#include "EventScheduler.h"
#include "FlowCreator.h"
#include "Exporter.h"



typedef struct {
    /*! time stamp of last packet seen for the packet flow of this task
         =0 indicates the flow was set to idle previously
     */
    time_t lastPkt;

    //! number of packets and bytes seen by this rule/task
    unsigned long long packets, bytes;

    // master list of action module data
    ppactionList_t actions;

     // 1 if flow autocreation enabled for rule
    int auto_flows;
     // 1 if this flow uses bidir matching (bidir autocreation flows)
    int bidir;
    int seppaths;

    int newFlow;

    Rule *rule;

    // list of flow keys
    flowKeyInfo_t *flowKeyList;
    // length of flow key in bytes
    unsigned short flowKeyLen;
    // pointer to filter list from rule description
    filterList_t *flist;

    // hash map with flows
    FlowCreator *flows;
} ruleActions_t;

//! action list for each rule
typedef vector<ruleActions_t>            ruleActionList_t;
typedef vector<ruleActions_t>::iterator  ruleActionListIter_t;


/*! \short   manage and apply Action Modules, retrieve flow data

    the PacketProcessor class allows to manage filter rules and their
    associated actions and apply those actions to incoming packets, 
    manage and retrieve flow data
*/

class PacketProcessor : public MeterComponent
{
  private:

    //! number of rules
    int numRules;

    //! associated module loader
    ModuleLoader *loader;

    //! action list for rules
    ruleActionList_t  rules;

    //! packet queue to read packets from
    PacketQueue *queue;

    //! reference to exporter
    Exporter *expt;

    //! add timer events to scheduler
    void addTimerEvents( int ruleID, int actID, ppaction_t &act, EventScheduler &es );

    void createFlowKey(unsigned char *mvalues, unsigned short len, ruleActions_t *ra);

  public:

    /*! \short   construct and initialize a PacketProcessor object

        \arg \c cnf        config manager
        \arg \c threaded   run as separate thread
        \arg \c moduleDir  action module directory
    */
    PacketProcessor(ConfigManager *cnf, int threaded, string moduleDir = "" );

    //!   destroy a PacketProcessor object, to be overloaded
    virtual ~PacketProcessor();

    //! get a link to the packet queue owned by the PacketProcessor
    PacketQueue *getQueue() { return queue; }

    //! check a ruleset (the action part)
    virtual void checkRules( ruleDB_t *rules );

    //! add rules
    virtual void addRules( ruleDB_t *rules, EventScheduler *e );

    //! delete rules
    virtual void delRules( ruleDB_t *rules );

    //! check a single rule
    int checkRule(Rule *r);

    /*! \short   add a Rule and its associated actions to rule list
        \arg \c r   pointer to rule
        \arg \c e   pointer to event scheduler (timer events)
        \returns 0 - on success, <0 - else
    */
    int addRule( Rule *r, EventScheduler *e );

    /*! \short   delete a Rule from the rule list
        \arg \c r  pointer to rule
        \returns 0 - on success, <0 - else
    */
    int delRule( Rule *r );

    /*! \short   evaluate and process packet data with Action Modules
        lookup actions and flow data associated with the flow indicated by
        RuleID and apply these actions successively upon the packet data
        \arg \c packet  - packet meta data (including matched rules)
        \returns 0 - on success, <0 - else
    */
    int processPacket(metaData_t *packet);

    //! handle file descriptor event
    virtual int handleFDEvent(eventVec_t *e, fd_set *rset, fd_set *wset, fd_sets_t *fds);

    //! thread main function
    void main();

    /*! \short   export measurement data for a given rule

        \arg \c res - location data container object of type FlowRecord which stores all the data
        \ arg \c now - if now>0 only idle flows are exported and reset
        \returns 0 - on success, <0 - else
    */
    int exportRule( FlowRecord *res, time_t now=0, unsigned long ival=0 );

    // exports rule into flow record (the above function is deprecated should not be used)
    FlowRecord *exportRule(int rid, string rname, time_t now=0, unsigned long ival=0);

    /*! \short return -1 (no packet seen), 0 (timeout), >0 (no timeout; adjust last time)

        \arg \c ruleId  - number indicating matching rule for packet
    */
    unsigned long ruleTimeout(int ruleID, unsigned long ival, time_t now);
    
    //! get information about loaded modules
    string getInfo();

    //! dump a PacketProcessor object
    void dump( ostream &os );

    //! get the number of action modules currently in use
    int numModules() 
    { 
        return loader->numModules(); 
    }

    // handle module timeouts
    void timeout(int rid, int actid, unsigned int tmID);

    //! get xml info for a specific module
    string getModuleInfoXML( string modname );

    virtual string getConfigGroup() 
    { 
        return "PKTPROCESSOR"; 
    }

    virtual void waitUntilDone();

    void setExporter(Exporter *e) 
    { 
      expt = e; 
    }
};


//! overload for <<, so that a PacketProcessor object can be thrown into an iostream
ostream& operator<< ( ostream &os, PacketProcessor &pe );


#endif // _PACKETPROCESSOR_H_
