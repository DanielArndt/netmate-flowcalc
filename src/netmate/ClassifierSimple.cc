
/*!\file   ClassifierSimple.cc

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
    implementation for Classifier_simple

    $Id: ClassifierSimple.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "ClassifierSimple.h"


/* ------------------------- ClassifierSimple ------------------------- */

ClassifierSimple::ClassifierSimple(ConfigManager *cnf, Sampler *sa,
                                   PacketQueue *pq, int threaded) 
    : Classifier(cnf, "ClassifierSimple",sa,pq,threaded)
{
#ifdef DEBUG
    log->dlog(ch, "ClassifierSimple constructor" );
#endif

    // 2 lines -> support old g++
    auto_ptr<ClassifierStats> _stats(new ClassifierSimpleStats());
    stats = _stats;
}


/* ------------------------- ~ClassifierSimple ------------------------- */

ClassifierSimple::~ClassifierSimple()
{
#ifdef DEBUG
    log->dlog(ch, "ClassifierSimple destructor" );
#endif

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexLock(&maccess);
        stop();
        mutexUnlock(&maccess);
        mutexDestroy(&maccess);
    }
#endif
}


int ClassifierSimple::classify( metaData_t* pkt )
{
    int found;
    int last_match = -2; // first forward rule is 0
    unsigned char tmp[MAX_FILTER_LEN];

    AUTOLOCK(threaded, &maccess);

    pkt->match_cnt = 0;
    
    for (rulesIter_t r = rules.begin(); r != rules.end(); ++r) {  
        found = 1;

        for (matchListIter_t m = r->second.begin(); m != r->second.end(); ++m) {
            // if the offset is not > 0 there cant be a match
            if (pkt->offs[m->refer] < 0) {
                return 0;
            } else {
                unsigned short len = m->len;
                filterType_t type = m->type; 
                unsigned char *pval = (unsigned char *) &pkt->payload[ pkt->offs[m->refer] + m->offs ];
              
                if (type != FT_WILD) {
                  
                    // get masked pkt value ...
                    for (int i = 0; i < len; i++) {
                        tmp[i] = pval[i] & m->mask[i];
                    }	

                    // ... and compare with match value
                    if ((type == FT_EXACT) && memcmp(tmp, m->value[0], len)) {
                        found = 0;
                        break;
                    }
                    else if ((type == FT_RANGE) &&
                             ((memcmp(tmp, m->value[0], len) < 0) ||
                              (memcmp(tmp, m->value[1], len) > 0))) {
                        found = 0;
                        break;
                    }
                    else if (type == FT_SET) {
                        int c = 0;
                        int inset = 0;

                        while (c < m->cnt) {
                            if (!memcmp(tmp, m->value[c], len)) {
                                inset = 1;
                                break;
                            }
                            c++;
                        }
                        if (!inset) {
                            found = 0;
                            break;
                        }
                    } 
                }
            }
        }

        if (found) {
            if (r->first != last_match+1) {
                /// store rule id in meta-data (for use by PacketProcessor)
                if (pkt->match_cnt > MAX_RULES_MATCH) {
                    throw Error("more than %d rules match the same packet", MAX_RULES_MATCH);
                }
   
                pkt->match[pkt->match_cnt] = r->first/2;
		if ((r->first % 2) == 1) {
		  pkt->reverse = 1;
		}
                last_match = r->first;

                pkt->match_cnt++;
            }
        }
    }

    return pkt->match_cnt;
}

  
// check a ruleset (the filter part)
void ClassifierSimple::checkRules(ruleDB_t *rules)
{
    // accept everything without complaining
    // the simple classifier accepts all type of filters
}


// add rules
void ClassifierSimple::addRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;

    AUTOLOCK(threaded, &maccess);

    for (iter = rules->begin(); iter != rules->end(); iter++) {
        addRule(*iter);
        if ((*iter)->isBidir()) {
            addRule(*iter, 0);
        }
    }
}


// delete rules
void ClassifierSimple::delRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;

    AUTOLOCK(threaded, &maccess);

    for (iter = rules->begin(); iter != rules->end(); iter++) {
        delRule(*iter);
        if ((*iter)->isBidir()) {
            delRule(*iter, 0);
        }
    }
}


void ClassifierSimple::addRule(Rule *r, int forward)
{
    matchList_t matches;
    match_t m;
    filterListIter_t iter;
   
    // add rule to list

#ifdef DEBUG
    log->dlog(ch, "Add rule %d", r->getUId());
#endif

    // only add a backward rule if there are filter(s) attributes which reverse
    if (!forward) {
        int found = 0;
        for (iter = r->getFilter()->begin(); iter != r->getFilter()->end(); iter++) {
            if (!iter->rname.empty()) {
                found = 1;
                break;
            }
        }

        if (!found) {
            return;
        }
    }

    // loop over all filters
    for (iter = r->getFilter()->begin(); iter != r->getFilter()->end(); iter++) {
        if (forward || iter->rname.empty()) {
            m.refer = iter->refer;
            m.offs = iter->offs;
        } else {
            m.refer = iter->rrefer;
            m.offs = iter->roffs;
        }
        m.len = iter->len;
        m.type = iter->mtype;

        int c = 0;
        while (c < iter->cnt) {
            memcpy(m.value[c], iter->value[c].getValue(), iter->value[c].getLen());

            c++;
        }
        m.cnt = iter->cnt;
      
        // join filter and filter definition masks
        for (int i=0; i < m.len; i++) {
            m.mask[i] = iter->mask.getValue()[i] & iter->fdmask.getValue()[i];
        }

        matches.push_back(m);
    }

    if (forward) {
        // forward on even pos
        rules.insert(make_pair( r->getUId()*2, matches));     
    } else {
        // backward on odd pos (+1)
        rules.insert(make_pair( r->getUId()*2+1, matches));
    }

    stats->rules++;
}


void ClassifierSimple::delRule(Rule *r, int forward)
{
    rulesIter_t iter;
    int uid;

#ifdef DEBUG
    log->dlog(ch, "Remove rule %d", r->getUId());
#endif

    if (forward) {
        uid = r->getUId() * 2;
    } else {
        uid = r->getUId() * 2 + 1;
    }

    if ((iter = rules.find(uid)) != rules.end()) {

        // delete entry from internal rule_list
        rules.erase(iter);

        stats->rules--;
    }
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, ClassifierSimple &cl )
{
    cl.dump(os);
    return os;
}



