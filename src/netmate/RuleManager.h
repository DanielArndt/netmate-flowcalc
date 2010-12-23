
/*! \file   RuleManager.h

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
    rule db

    $Id: RuleManager.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _RULEMANAGER_H_
#define _RULEMANAGER_H_


#include "stdincpp.h"
#include "Logger.h"
#include "Error.h"
#include "RuleIdSource.h"
#include "Rule.h"
#include "FilterDefParser.h"
#include "FilterValParser.h"
#include "RuleFileParser.h"
#include "MAPIRuleParser.h"
#include "EventScheduler.h"


// default flow idle timeout
const time_t FLOW_IDLE_TIMEOUT = 30;


// RuleDB definition is currently in RuleFileParser.h

// index by set id and name
typedef map<string, int>            ruleIndex_t;
typedef map<string, int>::iterator  ruleIndexIter_t;
typedef map<string, ruleIndex_t>            ruleSetIndex_t;
typedef map<string, ruleIndex_t>::iterator  ruleSetIndexIter_t;

//! list of done rules
typedef list<Rule*>            ruleDone_t;
typedef list<Rule*>::iterator  ruleDoneIter_t;

//! index rules by time
typedef map<time_t, ruleDB_t>            ruleTimeIndex_t;
typedef map<time_t, ruleDB_t>::iterator  ruleTimeIndexIter_t;


typedef struct 
{
    interval_t i;
    expnames_t e;
} expdef_t;

//! compare two export definition structs
struct lttexp
{
    bool operator()(const expdef_t e1, const expdef_t e2) const
    {
      if  ((e1.i.interval < e2.i.interval) ||
           (e1.i.align < e2.i.align) ||
           (e1.e < e2.e)) {
          return 1;
      } else {
          return 0;
      }
    }
};


//! rules indexed by interval and export modules struct
typedef map<expdef_t, ruleDB_t, lttexp>            ruleIntervalsIndex_t;
typedef map<expdef_t, ruleDB_t, lttexp>::iterator  ruleIntervalsIndexIter_t;

/*! \short   manage adding/deleting of complete rule descriptions
  
  the RuleManager class allows to add and remove rules in the Meters
  core system. Rule data are a set of ascii strings that are parsed
  and syntax checked by the RuleManager and then their respective
  settings are used to configure the other MeterCore components. The
  rule will then be stored in the ruleDatabase inside the RuleManager
*/

class RuleManager
{
  private:

    Logger *log;
    int ch; //!< logging channel number used by objects of this class

    //!< number of tasks in the database
    int tasks;

    //! index to rules via setID and name
    ruleSetIndex_t ruleSetIndex;

    //! stores all rules indexed by setID, ruleID
    ruleDB_t  ruleDB;

    //! list with rules done
    ruleDone_t ruleDone;

    //! filter definitions
    filterDefList_t filterDefs;

    //! filter values
    filterValList_t filterVals;

    // pool of unique rule ids
    RuleIdSource idSource;

    // name of filter def and filter vals files
    string filterDefFileName, filterValFileName;

    //! load filter definitions
    void loadFilterDefs(string fname);

    //! load filter value definitions
    void loadFilterVals(string fname);

    /* \short add the rule name to teh list of finished rules/tasks

       \arg \c rulename - name of the finished rule (source.name)
    */
    void storeRuleAsDone(Rule *r);

  public:

    int getNumTasks() 
    { 
        return tasks; 
    }

    filterDefList_t *getFilterDefs()
    { 
        return &filterDefs; 
    }

    string getInfo(int uid)
    {
        return getInfo(getRule(uid)); 
    }

    /*! \short   construct and initialize a RuleManager object
        \arg \c fdname  filter definition file name
        \arg \c fvname  filter value definition name
     */
    RuleManager(string fdname, string fvname);

    //! destroy a RuleManager object
    ~RuleManager();

     /*! \short   lookup the rule info data for a given ruleId or name

        lookup the database of installed filter rules for a specific rule
        and return a link (pointer) to the Rule data associated with it
        do not store this pointer, its contents will be destroyed upon rule deletion. 
        do not free this pointer as it is a link to the Rule and not a copy.
    
        \arg \c ruleId - unique rule id
    */
    Rule *getRule(int uid);

    //! get rule rname from ruleset sname 
    Rule *getRule(string sname, string rname);

    //! get all rules in ruleset with name sname 
    ruleIndex_t *getRules(string sname);

    //! get all rules
    ruleDB_t getRules();

    //! parse XML rules from file 
    ruleDB_t *parseRules(string fname);

    //! parse XML or Meter API rules from buffer
    ruleDB_t *parseRulesBuffer(char *buf, int len, int mapi);
   
    /*! \short   add a filter rule description 

        adding new rules to the metering system will parse and syntax
        check the given rule specifications, lookup the rule database for
        already installed rules and store the rule into the database 

        \throws an Error exception if the given rule description is not
        syntactically correct or does not contain the mandatory fields
        or if a rule with the given identification is already present in the ruleDatabase
    */
    void addRules(ruleDB_t *rules, EventScheduler *e);

    //! add a single rule
    void addRule(Rule *r);

    //! activate/execute rules
    void activateRules(ruleDB_t *rules, EventScheduler *e);

    /*! \short   delete a filter rule description 

        deletion of a filter rule will parse and syntax check the
        identification string, test the presence of the given filter
        rule in the ruleDatabase, remove the rule from the ruleDatabase

        \throws an Error exception if the given rule identification is not
        syntactically correct or does not contain the mandatory fields  or 
        if a rule with the given identification is currently not present in the ruleDatabase
    */
    void delRule(int uid, EventScheduler *e);
    void delRule(string rname, string sname, EventScheduler *e);
    void delRules(string sname, EventScheduler *e);
    void delRule(Rule *r, EventScheduler *e);
    void delRules(ruleDB_t *rules, EventScheduler *e);
   
    /*! \short   get information from the rule manager

        these functions can be used to get information for a single rule,
        or a set of rules or all rules
    */
    string getInfo(void);
    string getInfo(Rule *r);
    string getInfo(string sname, string rname);
    string getInfo(string sname);

    //! dump a RuleManager object
    void dump( ostream &os );
};


//! overload for <<, so that a RuleManager object can be thrown into an iostream
ostream& operator<< ( ostream &os, RuleManager &rm );


#endif // _RULEMANAGER_H_
