
/*  \file   RuleFileParser.cc

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
    parse rule files

    $Id: RuleFileParser.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "RuleFileParser.h"
#include "ParserFcts.h"
#include "constants.h"


RuleFileParser::RuleFileParser(string filename)
    : XMLParser(RULEFILE_DTD, filename, "RULESET")
{
    log = Logger::getInstance();
    ch = log->createChannel("RuleFileParser" );
}


RuleFileParser::RuleFileParser(char *buf, int len)
    : XMLParser(RULEFILE_DTD, buf, len, "RULESET")
{
    log = Logger::getInstance();
    ch = log->createChannel("RuleFileParser" );
}


configItem_t RuleFileParser::parsePref(xmlNodePtr cur)
{
    configItem_t item;

    item.name = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"NAME"));
    if (item.name.empty()) {
        throw Error("Rule Parser Error: missing name at line %d", XML_GET_LINE(cur));
    }
    item.value = xmlCharToString(xmlNodeListGetString(XMLDoc, cur->xmlChildrenNode, 1));
    if (item.value.empty()) {
        throw Error("Rule Parser Error: missing value at line %d", XML_GET_LINE(cur));
    }
    item.type = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"TYPE"));

    // check if item can be parsed
    try {
        ParserFcts::parseItem(item.type, item.value);
    } catch (Error &e) {    
        throw Error("Rule Parser Error: parse value error at line %d: %s", XML_GET_LINE(cur), 
                    e.getError().c_str());
    }

    return item;
}

string RuleFileParser::lookup(filterValList_t *filterVals, string fvalue, filter_t *f)
{
    filterValListIter_t iter2 = filterVals->find(fvalue);
    if (iter2 != filterVals->end()) {
        if (iter2->second.type == f->type) {
            // substitute filter value
            fvalue = iter2->second.svalue;
        } else {
            throw Error("filter value type mismatch: %s given but %s expected", 
                        iter2->second.type.c_str(), f->type.c_str());
        }
    }

    return fvalue;
}


void RuleFileParser::parseFilterValue(filterValList_t *filterVals, string value, filter_t *f)
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


void RuleFileParser::parse(filterDefList_t *filterDefs, filterValList_t *filterVals, ruleDB_t *rules)
{
    xmlNodePtr cur, cur2, cur3;
    string sname;
    miscList_t globalMiscList;
    actionList_t globalActionList;
    exportList_t globalExportList;
    time_t now = time(NULL);

    cur = xmlDocGetRootElement(XMLDoc);

    sname = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"ID"));

#ifdef DEBUG
    log->dlog(ch, "ruleset %s", sname.c_str());
#endif

    cur = cur->xmlChildrenNode;

    while (cur != NULL) {
	
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"GLOBAL")) && (cur->ns == ns)) {
            // parse global settings

            cur2 = cur->xmlChildrenNode;

            while (cur2 != NULL) {
                // get PREF
                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"PREF")) && (cur2->ns == ns)) {
                    // parse
                    configItem_t item = parsePref(cur2); 	
                    // add
                    globalMiscList[item.name] = item;
#ifdef DEBUG
                    log->dlog(ch, "C %s = %s", item.name.c_str(), item.value.c_str());
#endif
                }

                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"ACTION")) && (cur2->ns == ns)) {
                    action_t a;

                    a.name = xmlCharToString(xmlGetProp(cur2, (const xmlChar *)"NAME"));
                    if (a.name.empty()) {
                        throw Error("Rule Parser Error: missing name at line %d", XML_GET_LINE(cur2));
                    }

                    cur3 = cur2->xmlChildrenNode;

                    while (cur3 != NULL) {
                        // get action specific PREFs
                        if ((!xmlStrcmp(cur3->name, (const xmlChar *)"PREF")) && (cur3->ns == ns)) {
                            configItem_t item;
                            // parse
                            item = parsePref(cur3); 	
                            // add
                            a.conf.push_back(item);
                        }

                        cur3 = cur3->next;
                    }

                    globalActionList.push_back(a);
                }

                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"EXPORT")) && (cur2->ns == ns)) {
                    export_t e;

                    e.name = xmlCharToString(xmlGetProp(cur2, (const xmlChar *)"NAME"));
                    if (e.name.empty()) {
                        throw Error("Rule Parser Error: missing name at line %d", XML_GET_LINE(cur2));
                    }

                    cur3 = cur2->xmlChildrenNode;

                    while (cur3 != NULL) {
                        // get action specific PREFs
                        if ((!xmlStrcmp(cur3->name, (const xmlChar *)"PREF")) && (cur3->ns == ns)) {
                            configItem_t item;
                            // parse
                            item = parsePref(cur3); 	
                            // add
                            e.conf.push_back(item);
                        }

                        cur3 = cur3->next;
                    }

                    globalExportList.push_back(e);
                }
	
                cur2 = cur2->next;
            }
        }
	
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"RULE")) && (cur->ns == ns)) {
            string rname;
            filterList_t filters;
            actionList_t actions = globalActionList;
            exportList_t exports = globalExportList;
            miscList_t miscs = globalMiscList;

            rname = xmlCharToString(xmlGetProp(cur, (const xmlChar *)"ID"));

            cur2 = cur->xmlChildrenNode;

            while (cur2 != NULL) {

                // get rule specific PREFs
                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"PREF")) && (cur2->ns == ns)) {
                    // parse
                    configItem_t item = parsePref(cur2); 	
                    // add
                    miscs[item.name] = item;

                }

                // get FILTER
                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"FILTER")) && (cur2->ns == ns)) {
                    filter_t f;
                    filterDefListIter_t iter, iter2;
                    string mask;

                    f.name = xmlCharToString(xmlGetProp(cur2, (const xmlChar *)"NAME"));
                    if (f.name.empty()) {
                        throw Error("Rule Parser Error: missing name at line %d", XML_GET_LINE(cur2));
                    }
                    // use lower case internally
                    transform(f.name.begin(), f.name.end(), f.name.begin(), 
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

                        // lookup reverse attribute
                        if (!iter->second.rname.empty()) {
                            f.rname = iter->second.rname;
                            iter2 = filterDefs->find(f.rname);
                            if (iter2 != filterDefs->end()) {
                                f.roffs = iter2->second.offs;
                                f.rrefer = iter2->second.refer;
                            }
                        }

                        // lookup in filter var list
                        string fvalue = xmlCharToString(xmlNodeListGetString(XMLDoc, cur2->xmlChildrenNode, 1));
                        if (fvalue.empty()) {
                            throw Error("Rule Parser Error: missing value at line %d", XML_GET_LINE(cur2));
                        }
                        // use lower case internally
                        transform(f.name.begin(), f.name.end(), f.name.begin(), 
                                  ToLower());
                    
                        // parse and set value
                        try {
                            parseFilterValue(filterVals, fvalue, &f);
                        } catch(Error &e) {
                            throw Error("Rule Parser Error: filter value parse error at line %d: %s", 
                                        XML_GET_LINE(cur2), e.getError().c_str());
                        }

                        try {
                            // set mask
                            mask = xmlCharToString(xmlGetProp(cur2, (const xmlChar *)"MASK"));
                            // replace default mask
                            if (mask == "0xFF") {
                                if (f.type == "IPAddr") {
                                    mask = DEF_MASK_IP;
                                } else if (f.type == "IP6Addr") {
                                    mask = DEF_MASK_IP6;
                                } else {
                                    // make default mask as wide as data
                                    mask = "0x" + string(2*f.len, 'F');
                                }
                            }
                            f.mask = FilterValue(f.type, mask);
                        } catch (Error &e) {
                            throw Error("Rule Parser Error: mask parse error at line %d: %s",
                                        XML_GET_LINE(cur2), e.getError().c_str());
                        }                            

                        filters.push_back(f);
                    } else {
                        throw Error("Rule Parser Error: no filter definition found at line %d: %s", 
                                    XML_GET_LINE(cur2), f.name.c_str());
                    }
                }
        
                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"ACTION")) && (cur2->ns == ns)) {
                    action_t a;

                    a.name = xmlCharToString(xmlGetProp(cur2, (const xmlChar *)"NAME"));
                    if (a.name.empty()) {
                        throw Error("Rule Parser Error: missing name at line %d", XML_GET_LINE(cur2));
                    }

                    cur3 = cur2->xmlChildrenNode;

                    while (cur3 != NULL) {
                        // get action specific PREFs
                        if ((!xmlStrcmp(cur3->name, (const xmlChar *)"PREF")) && (cur3->ns == ns)) {
                            configItem_t item;
                            // parse
                            item = parsePref(cur3); 	
                            // add
                            a.conf.push_back(item);
                        }

                        cur3 = cur3->next;
                    }

		    // overide the global action parameter
		    for (actionListIter_t i=actions.begin(); i != actions.end(); ++i) {
		      if (i->name == a.name) {
			actions.erase(i);
			break;
		      }
		    }
                    actions.push_back(a);
                }

                if ((!xmlStrcmp(cur2->name, (const xmlChar *)"EXPORT")) && (cur2->ns == ns)) {
                    export_t e;

                    e.name = xmlCharToString(xmlGetProp(cur2, (const xmlChar *)"NAME"));
                    if (e.name.empty()) {
                        throw Error("Rule Parser Error: missing name at line %d", XML_GET_LINE(cur2));
                    }
                    
                    cur3 = cur2->xmlChildrenNode;

                    while (cur3 != NULL) {
                        // get action specific PREFs
                        if ((!xmlStrcmp(cur3->name, (const xmlChar *)"PREF")) && (cur3->ns == ns)) {
                            configItem_t item;
                            // parse
                            item = parsePref(cur3); 	
                            // add
                            e.conf.push_back(item);
                        }

                        cur3 = cur3->next;
                    }

		    // overide the global export parameter
		    for (exportListIter_t i=exports.begin(); i != exports.end(); ++i) {
		      if (i->name == e.name) {
			exports.erase(i);
			break;
		      }
		    }
                    exports.push_back(e);
                }

                cur2 = cur2->next;
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
        
        cur = cur->next;
    }
}
