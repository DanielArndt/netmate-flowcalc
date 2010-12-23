
/*! \file   Rule.h

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
    container class for rule

    $Id: Rule.h 748 2009-09-10 02:54:03Z szander $
*/


#ifndef _RULE_H_
#define _RULE_H_


#include "stdincpp.h"
#include "Logger.h"
#include "PerfTimer.h"
#include "ConfigParser.h"
#include "FilterValue.h"
#include "FilterDefParser.h"
#include "ProcModuleInterface.h"

// FIXME document!
const int MAX_FILTER_SET_SIZE = 16;

// list of defined flags for use within a Rule object
typedef enum
{
  // define if a final export is done upon rule stop
  RULE_FINAL_EXPORT   =  0x01,
  RULE_FLOW_TIMEOUT   =  0x02,
  // auto creation of flows (based on wildcard attributes)
  RULE_AUTO_FLOWS     =  0x04
} ruleFlags_t;

//! rule states during lifecycle
typedef enum
{
    RS_NEW = 0,
    RS_VALID,
    RS_SCHEDULED,
    RS_ACTIVE,
    RS_DONE,
    RS_ERROR
} ruleState_t;

//! match/filter types
typedef enum
{
    FT_EXACT =0,
    FT_RANGE,
    FT_SET,
    FT_WILD
} filterType_t;

//! definition of a filter
typedef struct
{
    string name;
    string rname;
    string type;
    filterType_t mtype;
    refer_t refer;
    refer_t rrefer;
    unsigned short offs;
    unsigned short roffs;
    unsigned short len;
    //! number of values
    unsigned short cnt;

    //! mask from filter definition
    FilterValue fdmask;
    //! position of lowest set bit in fdmask (only set when len == 1)
    unsigned char fdshift;
    //! mask for the filter value
    FilterValue mask;
    //! value definition:
    //! EXACT -> value[0]
    //! RANGE -> min in value[0], max in value[1]
    //! SET -> value[0-n] where value.len>0
    //! WILD -> no value
    FilterValue value[MAX_FILTER_SET_SIZE];
} filter_t;

//! FIXME document!
typedef struct
{
    string name;
    configItemList_t conf;
} action_t;

//! FIXME document!
typedef struct
{
    string name;
    configItemList_t conf;
} export_t;

//! filter list (only push_back & sequential access)
typedef list<filter_t>            filterList_t;
typedef list<filter_t>::iterator  filterListIter_t;

//! action list (only push_back & sequential access)
typedef list<action_t>            actionList_t;
typedef list<action_t>::iterator  actionListIter_t;

//! export list (only push_back & sequential access)
typedef list<export_t>            exportList_t;
typedef list<export_t>::iterator  exportListIter_t;

//! misc list (random access based on name required)
typedef map<string,configItem_t>            miscList_t;
typedef map<string,configItem_t>::iterator  miscListIter_t;

//! export interval definition
typedef struct 
{
    //! export interval
    unsigned long interval;
    //! align yes/no
    int align;
} interval_t;

//! export module names
typedef set<string> expnames_t;
typedef set<string>::iterator expnamesIter_t;

//! compare two interval structs
struct lttint
{
    bool operator()(const interval_t i1, const interval_t i2) const
    {
      if  ((i1.interval < i2.interval) ||
           (i1.align < i2.align)) {
          return 1;
      } else {
          return 0;
      }
    }
};

//! contains export modules indexed by export interval
typedef map<interval_t, expnames_t, lttint>            intervalList_t;
typedef map<interval_t, expnames_t, lttint>::iterator  intervalListIter_t;

typedef struct {
    enum DataType_e type;
    char *name;
    unsigned short len;
} flowKeyInfo_t;

//! parse and store a complete rule description

class Rule
{
  private:

    Logger *log; //!< link to global logger object
    int ch;      //!< logging channel number used by objects of this class
    
    //! define the rules running time properties
    time_t start;
    time_t stop;

    //! define the export intervals for each export module
    intervalList_t intervals;

    //! define the flow timeout
    unsigned long flowTimeout;

    //! unique ruleID of this Rule instance (has to be provided)
    int uid;

    //! state of this rule
    ruleState_t state;

    //! name of the rule by convention this must be either: <name> or <source>.<id>
    string ruleName;

    //! parts of rule name for efficiency
    string source;
    string id;

    //! name of the rule set this rule belongs to
    string setName;

    //! stores flags that indicate if a certain yes/no option is enabled
    unsigned long flags;

    //! rule bidirectional or unidirectional
    int bidir;

    //! separate forward and backward paths (only useful for auto+bidir rules)
    int seppaths;

    //! list of filters
    filterList_t filterList;

    //! list of actions
    actionList_t actionList;

    //! list of exports
    exportList_t exportList;

    //! list of misc stuff (start, stop, duration etc.)
    miscList_t miscList;

    /*! \short   parse identifier format 'sourcename.rulename'

        recognizes dor (.) in task identifier and saves sourcename and 
        rulename to the new malloced strings source and rname
    */
    void parseRuleName(string rname);

    //! parse time string
    time_t parseTime(string timestr);

    //! get a value by name from the misc rule attriutes
    string getMiscVal(string name);


  public:
    
    void setState(ruleState_t s) 
    { 
        state = s;
    }

    ruleState_t getState()
    {
        return state;
    }

    int isFlagEnabled(ruleFlags_t f) 
    { 
        return (flags & f);
    }

    int getUId() 
    { 
        return uid;
    }
    
    void setUId(int nuid)
    {
        uid = nuid;
    }
    
    string getSetName()
    {
        return setName;
    }

    string getRuleName()
    {
        return ruleName;
    }
    
    string getRuleSource()
    {
        return source;
    }
    
    string getRuleId()
    {
        return id;
    }

    time_t getStart()
    {
        return start;
    }
    
    time_t getStop()
    {
        return stop;
    }
    
    intervalList_t *getIntervals()
    {
        return &intervals;
    }
    
    int isBidir()
    {
        return bidir;
    }
    
    int sepPaths()
    {
      return seppaths;
    }
    
    void setBidir()
    {
        bidir = 1;
    }
    
    unsigned long getFlowTimeout()
    {
        return flowTimeout;
    }
    
    /*! \short   construct and initialize a Rule object
        \arg \c now   current timestamp
        \arg \c sname   rule set name
        \arg \c s  rname  rule name
        \arg \c f  list of filters
        \arg \c a  list of actions
        \arg \c e  list of exports
        \arg \c m  list of misc parameters
    */
    Rule(time_t now, string sname, string rname, filterList_t &f, actionList_t &a,
    	 exportList_t &e, miscList_t &m);

    //! destroy a Rule object
    ~Rule();
   
    /*! \short   get names and values (parameters) of configured actions
        \returns a pointer (link) to a list that contains the configured actions for this rule
    */
    actionList_t *getActions();

    /*! \short   get names and values (parameters) of configured filter rule

        \returns a pointer (link) to a ParameterSet object that contains the 
                 configured filter rule description for this rule
    */
    filterList_t *getFilter();

    //! returns list of export configuration parameters
    exportList_t *getExport();

    /*! \short   get names and values (parameters) of misc. attributes

        \returns a pointer (link) to a ParameterSet object that contains the 
                 miscanellenous attributes of a configured filter rule
    */
    miscList_t *getMisc();

    // get an export list for the flow key fields
    flowKeyInfo_t *getFlowKeyList();

    //! dump a Rule object
    void dump( ostream &os );

    //! get rule info string
    string getInfo(void);
};


//! overload for <<, so that a Rule object can be thrown into an iostream
ostream& operator<< ( ostream &os, Rule &ri );


#endif // _RULEINFO_H_
