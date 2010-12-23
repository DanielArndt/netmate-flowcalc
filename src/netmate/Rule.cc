
/*!\file   Rule.cc

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

    $Id: Rule.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "Rule.h"
#include "Error.h"
#include "ParserFcts.h"
#include "constants.h"

/* ------------------------- Rule ------------------------- */

Rule::Rule(time_t now, string sname, string rname, filterList_t &f, 
           actionList_t &a, exportList_t &e, miscList_t &m)
  : uid(-1), state(RS_NEW), ruleName(rname), setName(sname), flags(0), bidir(0), seppaths(0),
      filterList(f), actionList(a), exportList(e), miscList(m)
{
    unsigned long duration;

    log = Logger::getInstance();
    ch = log->createChannel("Rule");

#ifdef DEBUG
    log->dlog(ch, "Rule constructor");
#endif    

    try {
	
        parseRuleName(rname);
	
        if (rname.empty()) {
            // we tolerate an empty sname but not an empty rname
            throw Error("missing rule identifier value in rule description");
        }
        if (sname.empty()) {
            sname = DEFAULT_SETNAME;
        }

        if (filterList.empty()) {
            throw Error("no filters specified", sname.c_str(), rname.c_str());
        }

        if (actionList.empty()) {
            throw Error("no actions specified", sname.c_str(), rname.c_str());
        }

        if (exportList.empty()) {
            throw Error("no exports specified", sname.c_str(), rname.c_str());
        }

        /* time stuff */
        start = now;
        // stop = 0 indicates infinite running time
        stop = 0;
        // duration = 0 indicates no duration set
        duration = 0;
	    
        // get the configured values
        string sstart = getMiscVal("Start");
        string sstop = getMiscVal("Stop");
        string sduration = getMiscVal("Duration");
        string sinterval = getMiscVal("Interval");
        string salign = getMiscVal("Align");
        string sftimeout = getMiscVal("FlowTimeout");
	    
        if (!sstart.empty() && !sstop.empty() && !sduration.empty()) {
            throw Error(409, "illegal to specify: start+stop+duration time");
        }
	
        if (!sstart.empty()) {
            start = parseTime(sstart);
            if(start == 0) {
                throw Error(410, "invalid start time %s", sstart.c_str());
            }
        }

        if (!sstop.empty()) {
            stop = parseTime(sstop);
            if(stop == 0) {
                throw Error(411, "invalid stop time %s", sstop.c_str());
            }
        }
	
        if (!sduration.empty()) {
            duration = ParserFcts::parseULong(sduration);
        }

        if (duration) {
            if (stop) {
                // stop + duration specified
                start = stop - duration;
            } else {
                // stop [+ start] specified
                stop = start + duration;
            }
        }
	
        // now start has a defined value, while stop may still be zero 
        // indicating an infinite rule
	    
        // do we have a stop time defined that is in the past ?
        if ((stop != 0) && (stop <= now)) {
            throw Error(300, "task running time is already over");
        }
	
        if (start < now) {
            // start late tasks immediately
            start = now;
        }

        // parse flow timeout if set
        if (!sftimeout.empty()) {
            if ((sftimeout != "no") && (sftimeout != "false") && (sftimeout != "0")) {
                flags |= RULE_FLOW_TIMEOUT;
                if ((sftimeout != "yes") && (sftimeout != "true")) {
                    flowTimeout = ParserFcts::parseInt(sftimeout);
                }
            } else {
                flowTimeout = 0;
            }
        }

        // set sinterval for all export modules which have no export interval
        // if sinterval is set
        for (exportListIter_t it = exportList.begin(); it != exportList.end(); it++) {
            // get export module params
            string einterval = it->conf.getValue("Interval");
            string ealign = it->conf.getValue("Align");

            int interval = 0;
            if (!einterval.empty()) {
                interval = ParserFcts::parseInt(einterval);
            } else if (!sinterval.empty()) {
                interval = ParserFcts::parseInt(sinterval);
            }
            int align = (!ealign.empty() || !salign.empty()) ? 1 : 0;

	    if (interval > 0) {
            	// add to intervals list
            	interval_t ientry;

            	ientry.interval = interval;
            	ientry.align = align;

            	intervals[ientry].insert(it->name);
	    }
        }

        // always trigger final export at the end of rule lifetime; a rule will die if 
	// stop time is reached or terminated through an external event
	flags |= RULE_FINAL_EXPORT;

        string bidir_str = getMiscVal("bidir");
        if (!bidir_str.empty()) {
            bidir = 1;
        }

	string seppaths_str = getMiscVal("sep_paths");
	if (!seppaths_str.empty()) {
	  seppaths = 1;
	}

        if (!getMiscVal("auto").empty()) {
            flags |=  RULE_AUTO_FLOWS;
        }

    } catch (Error &e) {    
        state = RS_ERROR;
        throw Error("rule %s.%s: %s", sname.c_str(), rname.c_str(), e.getError().c_str());
    }
}


/* ------------------------- ~Rule ------------------------- */

Rule::~Rule()
{
#ifdef DEBUG
    log->dlog(ch, "Rule destructor");
#endif    

}

/* functions for accessing the templates */

string Rule::getMiscVal(string name)
{
    miscListIter_t iter;

    iter = miscList.find(name);
    if (iter != miscList.end()) {
        return iter->second.value;
    } else {
        return "";
    }
}


void Rule::parseRuleName(string rname)
{
    int n;

    if (rname.empty()) {
        throw Error("malformed rule identifier %s, "
                    "use <identifier> or <source>.<identifier> ",
                    rname.c_str());
    }

    if ((n = rname.find(".")) > 0) {
        source = rname.substr(0,n);
        id = rname.substr(n+1, rname.length()-n);
    } else {
        // no dot so everything is recognized as id
        id = rname;
    }

}


/* ------------------------- parseTime ------------------------- */

time_t Rule::parseTime(string timestr)
{
    struct tm  t;
  
    if (timestr[0] == '+') {
        // relative time in secs to start
        try {
	    struct tm tm;
            int secs = ParserFcts::parseInt(timestr.substr(1,timestr.length()));
            time_t start = time(NULL) + secs;
            return mktime(localtime_r(&start,&tm));
        } catch (Error &e) {
            throw Error("Incorrect relative time value '%s'", timestr.c_str());
        }
    } else {
        // absolute time
        if (timestr.empty() || (strptime(timestr.c_str(), TIME_FORMAT.c_str(), &t) == NULL)) {
            return 0;
        }
    }
    return mktime(&t);
}


/* ------------------------- getActions ------------------------- */

actionList_t *Rule::getActions()
{
    return &actionList;
}


/* ------------------------- getFilter ------------------------- */

filterList_t *Rule::getFilter()
{
    return &filterList;
}

exportList_t *Rule::getExport()
{
    return &exportList;
}

/* ------------------------- getMisc ------------------------- */

miscList_t *Rule::getMisc()
{
    return &miscList;
}


/* ------------------------- dump ------------------------- */

void Rule::dump( ostream &os )
{
    os << "Rule dump :" << endl;
    os << getInfo() << endl;
  
}


/* ------------------------- getInfo ------------------------- */

string Rule::getInfo(void)
{
    ostringstream s;

    s << getSetName() << "." << getRuleName() << " ";

    switch (getState()) {
    case RS_NEW:
        s << "new";
        break;
    case RS_VALID:
        s << "validated";
        break;
    case RS_SCHEDULED:
        s << "scheduled";
        break;
    case RS_ACTIVE:
        s << "active";
        break;
    case RS_DONE:
        s << "done";
        break;
    case RS_ERROR:
        s << "error";
        break;
    default:
        s << "unknown";
    }

    s << ": ";

    filterListIter_t i = filterList.begin();
    while (i != filterList.end()) {
        s << i->name << "&" << i->mask.getString() << " = ";
        switch (i->mtype) {
        case FT_WILD:
            s << "*";
            break;
        case FT_EXACT:
            s << i->value[0].getString();
            break;
        case FT_RANGE:
           s << i->value[0].getString();
           s << "-";
           s << i->value[1].getString();
           break;
        case FT_SET:
            for (int j=0; j < i->cnt; j++) {
                s << i->value[j].getString();
                if (j < (i->cnt-1)) {
                    s << ", ";
                }
            }
            break;
        }

        i++;
        
        if (i != filterList.end()) {
            s << ", ";
        }
    }

    s << " | ";

    actionListIter_t ai = actionList.begin();
    while (ai != actionList.end()) {
        s << ai->name;

        ai++;

        if (ai != actionList.end()) {
            s << ", ";
        }
    }

    s << " | ";

    exportListIter_t ei = exportList.begin();
    while (ei != exportList.end()) {
        s << ei->name;

        ei++;

        if (ei != exportList.end()) {
            s << ", ";
        }
    }

    s << " | ";

    miscListIter_t mi = miscList.begin();
    while (mi != miscList.end()) {
        s << mi->second.name << " = " << mi->second.value;

        mi++;

        if (mi != miscList.end()) {
            s << ", ";
        }
    }

    s << endl;

    return s.str();
}

flowKeyInfo_t *Rule::getFlowKeyList()
{
    flowKeyInfo_t *fkinfo;
    unsigned short c = 0;

    fkinfo = new flowKeyInfo_t[filterList.size()+2];

    for (filterListIter_t i=filterList.begin(); i != filterList.end(); ++i) {

        if (i->type == "SInt8") {
            fkinfo[c].type = INT8;
        } else if (i->type == "UInt8") {
            fkinfo[c].type = UINT8;
        } else if (i->type == "SInt16") {
            fkinfo[c].type = INT16;
        } if (i->type == "UInt16") {
            fkinfo[c].type = UINT16;
        } if (i->type == "SInt32") {
            fkinfo[c].type = INT32;
        } if (i->type == "UInt32") {
            fkinfo[c].type = UINT32;
        } if (i->type == "SInt64") {
            fkinfo[c].type = INT64;
        } if (i->type == "UInt64") {
            fkinfo[c].type = UINT64;
        } if (i->type == "String") {
            fkinfo[c].type = STRING;
        } if (i->type == "Binary") {
            fkinfo[c].type = BINARY;
        } if (i->type == "IPAddr") {
            fkinfo[c].type = IPV4ADDR;
        } if (i->type == "IP6Addr") {
            fkinfo[c].type = IPV6ADDR;
        } 

        fkinfo[c].name = (char *) i->name.c_str();
        fkinfo[c].len = i->len;

        c++;
    }

    // add a direction attribute if forward and backward path shall be separated
    if (isBidir() && sepPaths()) {
        fkinfo[c].type = UINT8;
	fkinfo[c].name = "direction";
	fkinfo[c].len = 1;
	c++;
    }

    fkinfo[c].type = EXPORTEND;
    fkinfo[c].name = "EEnd";

    return fkinfo;
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, Rule &ri )
{
    ri.dump(os);
    return os;
}





