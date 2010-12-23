
/*  \file   MAPIRuleParser.h

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
    parser for the old Meter API rule syntax

    $Id: MAPIRuleParser.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "MAPIRuleParser.h"


MAPIRuleParser::MAPIRuleParser(string filename)
    : fileName(filename)
{
    log = Logger::getInstance();
    ch = log->createChannel("MAPIRuleParser" );
}


MAPIRuleParser::MAPIRuleParser(char *b, int l)
    : buf(b), len(l)
{
    log = Logger::getInstance();
    ch = log->createChannel("MAPIRuleParser" );
}


string MAPIRuleParser::lookup(filterValList_t *filterVals, string fvalue, filter_t *f)
{
    filterValListIter_t iter2 = filterVals->find(fvalue);
    if ((iter2 != filterVals->end()) && (iter2->second.type == f->type)) {
        // substitute filter value
        fvalue = iter2->second.svalue;
    }

    return fvalue;
}


void MAPIRuleParser::parseFilterValue(filterValList_t *filterVals, string value, filter_t *f)
{
    int n;

    if (value == "*") {
        f->mtype = FT_WILD;
        f->cnt = 1;
    } else if ((n = value.find("-")) > 0) {
        f->mtype = FT_RANGE;
        f->value[0] = FilterValue(f->type, lookup(filterVals, value.substr(0,n),f));
        f->value[1] = FilterValue(f->type, lookup(filterVals, value.substr(n+1, value.length()-n+1),f));
        f->cnt = 2;
    } else if ((n = value.find(",")) > 0) {
        int lastn = 0;
        int c = 0;

        n = -1;
        f->mtype = FT_SET;
        while (((n = value.find(",", lastn)) > 0) && (c<(MAX_FILTER_SET_SIZE-1))) {
            f->value[c] = FilterValue(f->type, lookup(filterVals, value.substr(lastn, n-lastn),f));
            c++;
            lastn = n+1;
        }
        f->value[c] = FilterValue(f->type, lookup(filterVals, value.substr(lastn, n-lastn),f));
        f->cnt = c+1;
        if ((n > 0) && (f->cnt == MAX_FILTER_SET_SIZE)) {
            throw Error("more than %d filters specified in set", MAX_FILTER_SET_SIZE);
        }
    } else {
        f->mtype = FT_EXACT;
        f->value[0] = FilterValue(f->type, lookup(filterVals, value,f));
        f->cnt = 1;
    }
}

// FIXME to be rewritten
void MAPIRuleParser::parse(filterDefList_t *filterDefs, filterValList_t *filterVals, ruleDB_t *rules)
{
    string sname, rname;
    miscList_t miscs;
    actionList_t actions;
    exportList_t exports;
    filterList_t filters;
    istringstream in(buf);
    string args[128];
    int argc = 0;
    int ind = 0;
    string tmp;
    int n = 0, n2 = 0;
    string line;
    time_t now = time(NULL);

    // each line contains 1 rule
    while (getline(in, line)) {    
        // tokenize rule string

        // skip initial white ws
        n = line.find_first_not_of(" \t", 0);
        while ((n2 = line.find_first_of(" \t", n)) > 0) {
            if (argc >= (int) (sizeof(args)-1)) {
                throw Error("too many arguments");
            }
            args[argc] = line.substr(n,n2-n);
#ifdef DEBUG
            log->dlog(ch, "arg[%i]: %s", argc, args[argc].c_str());
#endif
            // skip additional ws
            n = line.find_first_not_of(" \t", n2);
            argc++;
        }
 
        // get last argument
        if ((n > 0) && (n < (int) line.length())) {
            if (argc >= (int) (sizeof(args)-1)) {
                throw Error("too many arguments");
            }
            args[argc] = line.substr(n,line.length()-n);
#ifdef DEBUG
            log->dlog(ch, "arg[%i]: %s", argc, args[argc].c_str());
#endif
            argc++;
        }

        if (argc == 0 ) {
            throw Error("parse error");
        }

        // parse the first argument which must be rulename and rulesetname
        n = args[0].find(".");
        if (n > 0) {
            sname = args[0].substr(0, n);
            rname = args[0].substr(n+1, tmp.length()-n);
        } else {
            sname = "0";
            rname = args[0];
        }

        // parse the rest of the args
        ind = 1;
        while (ind < argc) {
            if ((ind < argc) && (args[ind][0] == '-')) {
                switch (args[ind++][1] ) {
                case 'w':
                    // -wait is not supported
                    break;
                case 'r':
                    // filter spec
                    while ((ind < argc) && (args[ind][0] != '-')) {
                        filter_t f;
                        filterDefListIter_t iter;
                        string fvalue;
                        
                        tmp = args[ind];

                        // skip the separating commas
                        if (tmp != ",") {
                            // filter: <name >=<value>[/<mask>]
                            n = tmp.find("=");
                            if ((n > 0) && (n < (int)tmp.length()-1)) {
                                f.name = tmp.substr(0,n);
                                transform(f.name.begin(), f.name.end(), f.name.begin(), 
                                          ToLower());
                                fvalue = tmp.substr(n+1, tmp.length()-n);
                                transform(fvalue.begin(), fvalue.end(), fvalue.begin(), 
                                          ToLower());
                                
                                // lookup in filter definitions list
                                iter = filterDefs->find(f.name);
                                if (iter != filterDefs->end()) {
                                    // set according to definition
                                    f.offs = iter->second.offs;
                                    f.refer = iter->second.refer;
                                    f.len = iter->second.len;
                                    f.type = iter->second.type;
                                    f.fdmask = iter->second.mask;
				    f.fdshift = iter->second.shift;

                                    // parse and set value
                                    tmp = fvalue;
                                    n = tmp.find("/");
                                    if (n > 0) {
                                        string mask;
                                        fvalue = tmp.substr(0,n);
                                        mask = tmp.substr(n+1, tmp.length()-n);
                                        f.mask = FilterValue(f.type, mask);
                                    } else {
                                        // default mask
                                        string mask;
                                        if (f.type == "IPAddr") {
                                            mask = DEF_MASK_IP;
                                        } else if (f.type == "IP6Addr") {
                                            mask = DEF_MASK_IP6;
                                        } else { 
                                            // make default mask as wide as data
                                            mask = "0x" + string(2*f.len, 'F');
                                        }
                                        f.mask = FilterValue(f.type, mask);
                                    }
                                    
                                    parseFilterValue(filterVals, fvalue, &f);
                                    
                                    filters.push_back(f);
                                } else {
                                    throw Error("No filter definition for filter %s found", f.name.c_str());
                                }
                            } else {
                                throw Error("filter parameter parse error");
                            }
                        }
                            
                        ind++;
                    }
                    break;
                case 'a':
                  {
                      if (ind < argc) {
                          // only one action per -a parameter
                          action_t a;
                          
                          // action: <name> [<param>=<value> , ...]
                          a.name = args[ind++];
                          
                          // action parameters
                          while ((ind<argc) && (args[ind][0] != '-')) {
                              configItem_t item;
                              
                              // parse param
                              tmp = args[ind];
                              n = tmp.find("=");
                              if ((n > 0) && (n < (int)tmp.length()-1)) {
                                  item.name = tmp.substr(0,n);
                                  item.value = tmp.substr(n+1, tmp.length()-n);
                                  item.type = "String";
                                  // hack: if parameter method = <method> change name to name_<method>
                                  // and do not add this parameter
                                  if (item.name == "method") {
                                      a.name = a.name + "_" + item.value;
                                  } else {
                                      a.conf.push_back(item);
                                  }
                              } else {
                                  // else invalid parameter  
                                  throw Error("action parameter parse error");
                              }

                              ind++;
                          }
                          
                          actions.push_back(a);
                      }
                  }
                  break;
                case 'm':
                    while ((ind<argc) && (args[ind][0] != '-')) {
                        configItem_t item;

                        tmp = args[ind];
                        // skip the separating commas
                        if (tmp != ",") {
                            n = tmp.find("=");
                            if ((n > 0) && (n < (int)tmp.length()-1)) {
                                item.name = tmp.substr(0,n);
                                item.value = tmp.substr(n+1, tmp.length()-n);
                                item.type = "String";
                                if (item.name == "start") {
                                    item.name = "Start";
                                } else if (item.name == "stop") {
                                    item.name = "Stop";
                                } else if (item.name == "duration") {
                                    item.name = "Duration";
                                } else if (item.name == "interval") {
                                    item.name = "Interval";
                                } else if (item.name == "auto") {
                                    item.name = "auto"; 
                                } else if (item.name == "bidir") {
                                    item.name = "bidir"; 
                                } else {
                                    throw Error("unknown option %s", item.name.c_str());
                                }

                                miscs[item.name] = item;
                            } else {
                                throw Error("misc parse error");
                            }
                        }                       

                        ind++;
                    }
                    break;
                case 'e':
                  {
                      if (ind < argc) {
                          export_t e;
                          
                          while ((ind<argc) && (args[ind][0] != '-')) {
                              configItem_t item;
                              
                              tmp = args[ind];
                              n = tmp.find("=");
                              if ((n > 0) && (n < (int)tmp.length()-1)) {
                                  item.name = tmp.substr(0,n);
                                  item.value = tmp.substr(n+1, tmp.length()-n);
                                  item.type = "String";
                                  if (item.name == "target") {
                                      tmp = item.value;
                                      n = tmp.find(":");
                                      if ((n > 0) && (n < (int)tmp.length()-1)) {
                                          item.name = tmp.substr(0,n);
                                          if (item.name == "file") {
                                              e.name = "text_file";
                                              item.name = "Filename";
                                              item.value = tmp.substr(n+1, tmp.length()-n);
                                              e.conf.push_back(item);
                                          } else if (item.name == "ipfix") {
                                              e.name = "ipfix";
                                              n = tmp.find_first_not_of("/", n+1);
                                              n2 = tmp.find(":", n+1);
                                              if (n2 > 0) {
                                                  item.name = "Collector";
                                                  item.value = tmp.substr(n, n2-n);
                                                  e.conf.push_back(item);
                                                  item.name = "Port";
                                                  item.value = tmp.substr(n2+1, tmp.length()-n2);
                                                  e.conf.push_back(item);
                                              } else {
                                                  item.name = "Collector";
                                                  item.value = tmp.substr(n, tmp.length()-n);
                                                  e.conf.push_back(item);
                                              }
                                          }
                                          // scp not supported

                                         
                                      } else {
                                          throw Error("export target parse error");
                                      }
                                  } else {
                                      throw Error("unknown option '%s'", item.name.c_str());
                                  }

                                  exports.push_back(e);
                              } else {
                                  throw Error("export target parse error");
                              }    
                              
                              ind++;
                          } 
                      }
                  }
                  break;
                default:
                    throw Error(403, "add_task: unknown option %s", args[ind].c_str() );
                }
            }	
        }
  
#ifdef DEBUG
        // debug info
        log->dlog(ch, "rule %s.%s", sname.c_str(), rname.c_str());
        for (filterListIter_t i = filters.begin(); i != filters.end(); i++) {
            switch (i->mtype) {
            case FT_WILD:
                log->dlog(ch, " F %s&%s = *", i->name.c_str(), i->mask.getString().c_str());
                break;
            case FT_EXACT:
                log->dlog(ch, " F %s&%s = %s", i->name.c_str(), i->mask.getString().c_str(), 
                          i->value[0].getString().c_str());
                break;
            case FT_RANGE:
                log->dlog(ch, " F %s&%s = %s-%s", i->name.c_str(), i->mask.getString().c_str(), 
                          i->value[0].getString().c_str(), i->value[1].getString().c_str() );
                break;
            case FT_SET:
                string vals;
                for (int j=0; j < i->cnt; j++) {
                    vals += i->value[j].getString();
                    if (j < (i->cnt-1)) {
                        vals += ", ";
                    }
                }
                log->dlog(ch, " F %s&%s = %s", i->name.c_str(), i->mask.getString().c_str(), 
                          vals.c_str());
                break;
            }
        }
        for (actionListIter_t i = actions.begin(); i != actions.end(); i++) {
            log->dlog(ch, " A %s", i->name.c_str());
            for (configItemListIter_t j = i->conf.begin(); j != i->conf.end(); j++) {
                log->dlog(ch, "  C %s = %s", j->name.c_str(), j->value.c_str());
            }
        }
        for (exportListIter_t i = exports.begin(); i != exports.end(); i++) {
            log->dlog(ch, " E %s", i->name.c_str());
            for (configItemListIter_t j = i->conf.begin(); j != i->conf.end(); j++) {
                log->dlog(ch, "  C %s = %s", j->name.c_str(), j->value.c_str());
            }
        }
        for (miscListIter_t i = miscs.begin(); i != miscs.end(); i++) {
            log->dlog(ch, " C %s = %s", i->second.name.c_str(), i->second.value.c_str());
        }
#endif
    
        // add rule
        try {
            Rule *r = new Rule(now, sname, rname, filters, actions, exports, miscs);
            rules->push_back(r);
        } catch (Error &e) {
            log->elog(ch, e);
            throw e;
        }
    }
}
