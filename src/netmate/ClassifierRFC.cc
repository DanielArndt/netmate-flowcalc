
/*!\file   ClassifierRFC.cc

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
   Classifier based on Recursive Flow Classification (RFC)

   TODO:
    - test inc add of ranges, sets, wildcard rules (more tests)
    - test rules with masks with zero bits (more tests)
    - implement more clever chunk merging


   $Id: ClassifierRFC.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "ClassifierRFC.h"
#include "PerfTimer.h"


//#define DEBUG
//#define PROFILING

/* ------------------------- ClassifierRFC ------------------------- */

void ClassifierRFC::initData()
{
    // set chunk data to 0
    memset(&cdata, 0, sizeof(phases_t));
  
    // set up one matching rule entry with 0 rules
    rmap = NULL;
    rmap_size = 0;

    // set eqnum data to zero
    memset(&eqnums, 0, sizeof(eqNum_t));

    // set number line data to zero
    memset(&nldscs, 0, sizeof(numberLineDescrs_t));

    // reset bitmap
    bmReset(&allrules);

    // set start equiv id to zero
    for (unsigned short i = 0; i < MAX_PHASES; i++) {
        for (unsigned short j = 0; j < MAX_CHUNKS; j++) {
            eqcl[i][j].maxId = 0;
        }
    }
}


void ClassifierRFC::cleanupData()
{
    // delete chunk data
    for (unsigned short i = 0; i < cdata.phaseCount; i++) {
        for (unsigned short j = 0; j < cdata.phases[i].chunkCount; j++) {
            saveDeleteArr(cdata.phases[i].chunks[j].entries);
            for (unsigned short k = 0; k < eqcl[i][j].maxId; k++) {
                if (eqcl[i][j].eids[k].bm != NULL) {
                    saveDelete(eqcl[i][j].eids[k].bm);
                }
            }
            eqcl[i][j].freeList.clear();
            eqcl[i][j].eids.clear();
            eqcl[i][j].bms.clear();
        }
    }

    // delete final rule map
    if (rmap_size > 0) {
        saveDeleteArr(rmap);
        rmap_size = 0;
    }
}


ClassifierRFC::ClassifierRFC( ConfigManager *cnf, Sampler *sa,
			                  PacketQueue *pq, int threaded) 
    : Classifier(cnf, "ClassifierRFC", sa, pq, threaded)
{
#ifdef DEBUG
    log->dlog(ch, "ClassifierRFC constructor" );
#endif

    // initialize the data structures
    initData();

    // 2 lines -> support old g++
    auto_ptr <ClassifierStats> _stats(new ClassifierRFCStats());
    stats = _stats;
}


/* ------------------------- ~ClassifierRFC ------------------------- */

ClassifierRFC::~ClassifierRFC()
{
#ifdef DEBUG
    log->dlog(ch, "ClassifierRFC destructor" );
#endif

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexLock(&maccess);
        stop();
        mutexUnlock(&maccess);
        mutexDestroy(&maccess);
    }
#endif

    cleanupData();
}

/* ----------------------------------------------------------------------------------- */

int ClassifierRFC::classify(metaData_t* pkt)
{
    int indx;
    unsigned short val;
    unsigned short chunks, parents;

#ifdef PROFILING
    unsigned long long ti1, ti2;

    ti1 = PerfTimer::readTSC();
#endif
    
    AUTOLOCK(threaded, &maccess);

    chunks = cdata.phases[0].chunkCount;
    for (unsigned short i = 0; i < chunks; i++) {
        // get the value from the packet data
        indx = pkt->offs[nldscs[i].ref];
        if (indx < 0) {
            return 0;
        } 

        indx += nldscs[i].offs;

        if (nldscs[i].len == 1) {
            val = ((unsigned short) pkt->payload[indx]) & nldscs[i].mask;
        } else {
            // convert to host byte order (support range matches)
            val = ntohs(*((unsigned short *) &pkt->payload[indx]) & nldscs[i].mask);
        }

        // phase 0 memory lookup
        eqnums[0][i] = cdata.phases[0].chunks[i].entries[val];
    }
    
#ifdef DEBUG
    cout << "Classify: phase 0 indices" << endl;
    for (unsigned short i = 0; i < cdata.phases[0].chunkCount; i++) {
        cout << i << ": " << eqnums[0][i] << endl;
	}
    cout << endl;
#endif
    
    // phase 1-n

    for (unsigned short i = 1; i < cdata.phaseCount; i++) {        

        chunks = cdata.phases[i].chunkCount;
    	for (unsigned short j = 0; j < chunks; j++) {

    	    // get entry from first parent chunk
    	    indx = eqnums[i-1][cdata.phases[i].chunks[j].parentChunks[0]];
    	    
            parents = cdata.phases[i].chunks[j].parentCount;
            for (unsigned short k = 1; k < parents; k++) {

                // calculate index
                unsigned short pchunk = cdata.phases[i].chunks[j].parentChunks[k];
                indx = indx * eqcl[i-1][pchunk].maxId + eqnums[i-1][pchunk];
    	    }
            
    	    // phase i memory lookup
    	    eqnums[i][j] = cdata.phases[i].chunks[j].entries[indx];
    	}
    
#ifdef DEBUG
        cout << "Classify: phase " << i << " indices" << endl;
        for (unsigned short j = 0; j < cdata.phases[i].chunkCount; j++) {
            cout << j << ": " << eqnums[i][j] << endl;
        }
#endif
    }
    
    indx = eqnums[cdata.phaseCount-1][0];

    if (rmap_size > 0) {
        for (unsigned short i = 0; i < rmap[indx].ruleCount; i++) {
            pkt->match[i] = rmap[indx].rules[i];
        }
        pkt->match_cnt = rmap[indx].ruleCount;
    }
    
#ifdef PROFILING
    ti2 = PerfTimer::readTSC();
  
    cout << "Class time: " << PerfTimer::ticks2ns(1, ti2-ti1) << " ns" << endl;
#endif

    return pkt->match_cnt;
}


// check a ruleset (the filter part)
void ClassifierRFC::checkRules(ruleDB_t *rules)
{
    // accept everything without complaining
    // the RFC classifier accepts all type of filters
}


// add rules
void ClassifierRFC::addRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;

    AUTOLOCK(threaded, &maccess);
  
    if (stats->rules == 0) {   
        // go for fast initial precomputation
        addInitialRules(rules);
    } else {
        // go for slower incremental add
        for (iter = rules->begin(); iter != rules->end(); iter++) {
            addRule(*iter);
        }
    }

    stats->rules += rules->size(); // adjust number of rules currently used
}


// delete rules
void ClassifierRFC::delRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;

    AUTOLOCK(threaded, &maccess);

    for (iter = rules->begin(); iter != rules->end(); iter++) {
        delRule(*iter);
    }
}

/* ------------------- number line functions ---------------------------------------*/

// add a rule to a point on a number line
void ClassifierRFC::addRuleToPoint(point_t *p, pointType_t type, unsigned short rid)
{  
    const int initial_rule_entries = 16;

    if (p->ruleCount == p->entryCount) {       
    	// get more memory for this rule list
    	if (p->entryCount == 0) {
            // alloc initial rule entries
            p->rules = new rule_t[initial_rule_entries];
            p->entryCount = initial_rule_entries;
    	} else {
            // double the size
            rule_t *new_rules = new rule_t[p->entryCount * 2 ];
            memcpy(new_rules, p->rules, sizeof(rule_t)*p->entryCount);
            saveDeleteArr(p->rules);
            p->rules = new_rules;
            p->entryCount *= 2;
        }
    }

    // add the rule
    p->rules[p->ruleCount].type = type;
    p->rules[p->ruleCount].rid = rid;
    p->ruleCount++;
}


void ClassifierRFC::getIndex(FilterValue *v, FilterValue *m, int size, unsigned short offs,
                             unsigned short *ret)
{
    // put start point in ret[0] and end point in ret[1]
    // use host byte order otherwise range matches are a problem!
    
    if (size == 1) {
        ret[0] = (unsigned short) (v->getValue()[0] & m->getValue()[0]);
        if ((unsigned short)(m->getValue()[0]) == 0xFF) {
            ret[1] = ret[0]+1;
        } else {
            ret[1] = (unsigned short) v->getValue()[0] | ~m->getValue()[0];
        }
    } else {
        ret[0] = ntohs(*((unsigned short *) &(v->getValue()[2*offs]))) & 
          ntohs(*((unsigned short *) &(m->getValue()[2*offs])));
        if (*((unsigned short *) &m->getValue()[2*offs]) == 0xFFFF) {
            ret[1] = ret[0]+1;
        } else {
            ret[1] = ntohs(*((unsigned short *) &(v->getValue()[2*offs]))) |
              ~ ntohs(*((unsigned short *) &(m->getValue()[2*offs])));
        }
    }
}

void ClassifierRFC::projMatchRemap(unsigned short rid, unsigned short chunk_id, int chunk_size, 
                                   unsigned short ch, filter_t *f)
{
    unsigned short ind[2], ind2[2];

    switch (f->mtype) {
    case FT_EXACT:
        // change all equiv classes between start and end of the match
        getIndex(&f->value[0], &f->mask, chunk_size, ch, ind);
        remapIndex(0, chunk_id, ind[0], ind[1], rid);
        break;
    case FT_RANGE:
        // start point is index from value 1, end point is index from value 2
        getIndex(&f->value[0], &f->mask, chunk_size, ch, ind);
        getIndex(&f->value[1], &f->mask, chunk_size, ch, ind2);
        
        // only add both points if the current byte part of the range is actually different
        // otherwise single point
        if (ind != ind2) {
            remapIndex(0, chunk_id, ind[0], ind2[1], rid);
        } else {
            remapIndex(0, chunk_id, ind[0], ind[1], rid);
        }
        break;
    case FT_SET:
      {
          // like FT_EXACT (n times)
          int i = 0;
          while (f->value[i].getLen()) {
              getIndex(&f->value[i], &f->mask, chunk_size, ch, ind);
              
              remapIndex(0, chunk_id, ind[0], ind[1], rid); 			      
              
              i++;
          }
      }
      break;
    case FT_WILD:
        // add bit to all used equiv classes for that chunk
        // chunk data does not need to be changed
        for (unsigned short eq = 0; eq < (int) eqcl[0][chunk_id].maxId; eq++) {
            if (eqcl[0][chunk_id].eids[eq].refc > 0) {
                bmSet(eqcl[0][chunk_id].eids[eq].bm, rid);
            }
        }
        break;
    } 
}    

void ClassifierRFC::projMatchDirectly(unsigned short eq, unsigned short chunk_id, int chunk_size, 
                                      unsigned short ch, filter_t *f)
{
    unsigned short ind[2], ind2[2];

    // project the points directly on the chunk
    switch (f->mtype) {
    case FT_EXACT:
        // set equiv class 2 for the distinct point
        getIndex(&f->value[0], &f->mask, chunk_size, ch, ind);
        for (unsigned short i = ind[0]; i < ind[1]; i++) {
            cdata.phases[0].chunks[chunk_id].entries[i] = eq;
        }
        break;
    case FT_RANGE:
        // start point is index from value 1, end point is index from value 2
        getIndex(&f->value[0], &f->mask, chunk_size, ch, ind);
        getIndex(&f->value[1], &f->mask, chunk_size, ch, ind2);
        
        // only add both points if the current byte part of the range is actually different
        // otherwise single point
        if (ind != ind2) {
            for (unsigned short i = ind[0]; i < ind2[1]; i++) {
                cdata.phases[0].chunks[chunk_id].entries[i] = eq;
            }
        } else {
            for (unsigned short i = ind[0]; i < ind[1]; i++) {
                cdata.phases[0].chunks[chunk_id].entries[i] = eq;
            }
        }
        break;
    case FT_SET:
      {
          // like FT_EXACT (n times)
          unsigned short i = 0;
          while (f->value[i].getLen()) {
              getIndex(&f->value[i], &f->mask, chunk_size, ch, ind);
              
              for (unsigned short j = ind[0]; j < ind[1]; j++) {
                  cdata.phases[0].chunks[chunk_id].entries[j] = eq;
              }
              
              i++;
          }
      }
      break;
    case FT_WILD:
        // nothing
        break;
    }   
}

void ClassifierRFC::projMatch(unsigned short rid, unsigned short chunk_id, int chunk_size, 
                              unsigned short ch, filter_t *f)
{
    unsigned short ind[2], ind2[2];

    switch (f->mtype) {
    case FT_EXACT:
        // start point is the index, end point is the right adjacent point
        getIndex(&f->value[0], &f->mask, chunk_size, ch, ind);
        addRuleToPoint(&nlines.lines[chunk_id].points[ind[0]], RULE_START, rid);
        addRuleToPoint(&nlines.lines[chunk_id].points[ind[1]], RULE_END, rid);
        break;
    case FT_RANGE:
        // start point is index from value 1, end point is index from value 2
        getIndex(&f->value[1], &f->mask, chunk_size, ch, ind);
        getIndex(&f->value[0], &f->mask, chunk_size, ch, ind2);

        // only add both points if the current byte part of the range is actually different
        // otherwise single point
        if (ind[0] != ind2[0]) {
            addRuleToPoint(&nlines.lines[chunk_id].points[ind[0]], RULE_START, rid);
            addRuleToPoint(&nlines.lines[chunk_id].points[ind2[1]], RULE_END, rid);
        } else {
            addRuleToPoint(&nlines.lines[chunk_id].points[ind[0]], RULE_START, rid);
            addRuleToPoint(&nlines.lines[chunk_id].points[ind[1]], RULE_END, rid);
        }
        break;
    case FT_SET:
      {
          // like FT_EXACT (n times)
          unsigned short i = 0;
          while (f->value[i].getLen()) {
              getIndex(&f->value[i], &f->mask,chunk_size, ch, ind);
              
              addRuleToPoint(&nlines.lines[chunk_id].points[ind[0]], RULE_START, rid);
              addRuleToPoint(&nlines.lines[chunk_id].points[ind[1]], RULE_END, rid);
              
              i++;
          }
      }
      break;
    case FT_WILD:
    	// start point is the first point, no end
    	addRuleToPoint(&nlines.lines[chunk_id].points[0], RULE_START, rid);
    	break;
    default:
        throw Error("unknown filter type: %i", f->mtype); 
    }
}

int ClassifierRFC::findNumberLine(refer_t ref, unsigned short offs, unsigned short chunk,
                                  unsigned short chunk_size)
{
    for (unsigned short i = 0; i < nlines.lineCount; i++) {
        if ((nldscs[i].offs == offs + chunk*chunk_size) && 
            (nldscs[i].ref == ref) && 
            (nldscs[i].len == chunk_size)) {
            return i;
        }
    }

    return -1;
}

void ClassifierRFC::addNumberLineDescr(refer_t ref, unsigned short offs, unsigned short chunk,
                                       unsigned short chunk_size, FilterValue* dmask)
{
    // create new number line description
    nldscs[nlines.lineCount].offs = offs + chunk*chunk_size;
    nldscs[nlines.lineCount].len = chunk_size;
    nldscs[nlines.lineCount].ref = ref;
    if (chunk_size == 1) {
        nldscs[nlines.lineCount].mask = (unsigned short) dmask->getValue()[chunk];
    } else {
        nldscs[nlines.lineCount].mask = 
          *((unsigned short *) &dmask->getValue()[chunk*chunk_size]);
    }
}

void ClassifierRFC::addNumberLine(refer_t ref, unsigned short offs, unsigned short chunk,
                                  unsigned short chunk_size, FilterValue* dmask)
{
    int line_size = (chunk_size == 1) ? NUMBER_LINE_SIZE_8B : NUMBER_LINE_SIZE_16B;

    if (nlines.lineCount == MAX_CHUNKS) {
        throw Error("the maximum number of number lines is %d", MAX_CHUNKS);
    }
    
    addNumberLineDescr(ref, offs, chunk, chunk_size, dmask);
    
    // alloc memory
    nlines.lines[nlines.lineCount].line_size = line_size;
    nlines.lines[nlines.lineCount].points = new point_t[line_size];
    memset(nlines.lines[nlines.lineCount].points, 0, sizeof(point_t)*line_size);
    nlines.lineCount++;
}


// find equiv class (create a new in case there is no existing)
unsigned short ClassifierRFC::findEC(unsigned short phase, unsigned short chunk, bitmap_t *bmp)
{
    int eq;

    // find bitmap in equiv class list
    chunkBmListIter_t b = eqcl[phase][chunk].bms.find(bmp);

    if (b == eqcl[phase][chunk].bms.end()) {
        // add new class
        if (eqcl[phase][chunk].freeList.size() > 0) {
            // reuse id from free list
            eq = eqcl[phase][chunk].freeList.front();
            eqcl[phase][chunk].freeList.pop_front();
            // set bitmap and flags
            *eqcl[phase][chunk].eids[eq].bm = *bmp;
            eqcl[phase][chunk].eids[eq].refc = 1;
            // insert into bitmap map
            eqcl[phase][chunk].bms[eqcl[phase][chunk].eids[eq].bm] = eq;
        } else {
            bmInfo_t bmi; 

            bmi.bm = new bitmap_t;
            *bmi.bm = *bmp;
            bmi.refc = 1;
            
            // get a new id
            eq = eqcl[phase][chunk].maxId++;
            // insert
            eqcl[phase][chunk].eids.push_back(bmi);
            eqcl[phase][chunk].bms[bmi.bm] = eq;
        }
    } else {
        // use existing class
        eq = b->second;
        eqcl[phase][chunk].eids[eq].refc++;
    }	      

    return eq;
}

void ClassifierRFC::addPreAllocEC(unsigned short phase, unsigned short chunk, 
                                  unsigned short cnt)
{
    int eq;
    bitmap_t bmp;
    bmReset(&bmp);

    for (unsigned short i = 0; i < cnt; i++) {
        bmInfo_t bmi;

        bmi.bm = new bitmap_t;
        *bmi.bm = bmp;
        bmi.refc = 0;
     
        eq = eqcl[phase][chunk].maxId++;
        eqcl[phase][chunk].eids.push_back(bmi);
        // put on free list 
        eqcl[phase][chunk].freeList.push_back(eq);
    }
}

void ClassifierRFC::makeNewChunk(unsigned short phase, unsigned int size)
{
    unsigned short chunk = cdata.phases[phase].chunkCount++;
    // alloc chunk memory 
    cdata.phases[phase].chunks[chunk].entries = new unsigned short[size];
    memset(cdata.phases[phase].chunks[chunk].entries, 0, sizeof(unsigned short)*size);
    cdata.phases[phase].chunks[chunk].entryCount = size;
}

// intersect the bitmaps in phase 1-n
void ClassifierRFC::doIntersection(unsigned short phase, unsigned short chunk, 
                				   unsigned short parent, int *indx, bitmap_t *bm)
{
    int size = 0;
    unsigned short pchunk = cdata.phases[phase].chunks[chunk].parentChunks[parent];

    for (chunkEqClArrayIter_t ei = eqcl[phase-1][pchunk].eids.begin();
         ei != eqcl[phase-1][pchunk].eids.end(); ++ei) {
        bitmap_t rbmp;
        
#ifdef PREALLOC_EQUIV_CLASSES
        if (ei->refc == 0) {
            // adjust index
            size = 1;
            for (unsigned short i = parent+1; i < cdata.phases[phase].chunks[chunk].parentCount; i++) {
                size *= eqcl[phase-1][cdata.phases[phase].chunks[chunk].parentChunks[i]].maxId;
            }
            (*indx) += size;

            continue;
        }
#endif

        bmAnd(bm, ei->bm, &rbmp);
        
        // go into recursion until we reached the last parent
        if ((parent < cdata.phases[phase].chunks[chunk].parentCount-1)) {
            doIntersection(phase, chunk, parent+1, indx, &rbmp);
        } else {
            int eq = findEC(phase, chunk, &rbmp);
            cdata.phases[phase].chunks[chunk].entries[*indx] = eq;
            (*indx)++;
        }
    }
}
    
int ClassifierRFC::hasBackwardSpec(Rule *r)
{
    for (filterListIter_t fi = r->getFilter()->begin(); fi != r->getFilter()->end(); ++fi) {
        if (!fi->rname.empty()) {
            return 1;
        }
    }

    return 0;
}

/* ------------------------------------ debug ---------------------------------------------*/      

#ifdef DEBUG

void ClassifierRFC::printNumberLines()
{
    // print number lines for debug
    for (unsigned short i = 0; i < nlines.lineCount; i++) {
        // print each number line
        cout << "line: " << i << "(" << nldscs[i].offs << "," 
             << nldscs[i].len << ")" << nlines.lines[i].line_size << endl;
        for (unsigned int j = 0; j < nlines.lines[i].line_size; j++) {
            if (nlines.lines[i].points[j].ruleCount > 0) {
                cout << "point " << j << " rule# " << nlines.lines[i].points[j].ruleCount << endl;        
                for (unsigned short k = 0; k < nlines.lines[i].points[j].ruleCount; k++) {
                    cout << nlines.lines[i].points[j].rules[k].type << ":"
                         << nlines.lines[i].points[j].rules[k].rid << ",";
                }
                cout << endl;
            }
        }
        cout << endl << endl;
    }
}

void ClassifierRFC::printChunk(unsigned short phase, unsigned short chunk, int show_cdata)
{
    cout << "phase " << phase << " chunk " << chunk << ": " << eqcl[phase][chunk].maxId 
         << " equiv classes" << endl;
    // equiv classes
    for (unsigned short eq = 0; eq < (int) eqcl[phase][chunk].maxId; eq++) {
        cout << eq << ": ";
        if (eqcl[phase][chunk].eids[eq].bm != NULL) {
            bmPrint(eqcl[phase][chunk].eids[eq].bm);
            // print reference counter
            cout << "/" << eqcl[phase][chunk].eids[eq].refc;
        } else {
            cout << "-";
        }
        cout << endl;
    }
    // chunk data
    if (show_cdata) {
        for (unsigned int k = 0; k < cdata.phases[phase].chunks[chunk].entryCount; k++) {
            cout << k << " : " << cdata.phases[phase].chunks[chunk].entries[k] << endl;
        }
    }
    cout << endl << endl;
}

#endif

#ifdef PROFILING

void ClassifierRFC::printProfiling(struct timeval *t1, struct timeval *t2)
{
    cout << "Time: " << (t2->tv_sec*1e6+t2->tv_usec) - (t1->tv_sec*1e6+t1->tv_usec) 
         << "us" << endl;
    
    unsigned long mem = 0;
    for (unsigned short i = 0; i < cdata.phaseCount; i++) {
        for (unsigned short j = 0; j < cdata.phases[i].chunkCount; j++) {
            // final rule map
            if (i == cdata.phaseCount-1) {
                mem += eqcl[i][j].maxId * sizeof(matchingRules_t);
            }
            // equiv class list and revese mapping
            mem += eqcl[i][j].maxId * (sizeof(bitmap_t) + sizeof(bmInfo_t));
            mem += eqcl[i][j].bms.size() * (sizeof(bitmap_t*) + sizeof(equivID_t) + 16);
            mem += eqcl[i][j].freeList.size() * sizeof(equivID_t);
            // chunk memory
            mem += cdata.phases[i].chunks[j].entryCount * sizeof(equivID_t);
        }
    }
          
    cout << "Memory: " << mem/1024 << "kB" << endl;
}

#endif

void ClassifierRFC::genFinalMap(unsigned short phase)
{
    unsigned int size = cdata.phases[phase].chunks[0].entryCount;

    if (size != rmap_size) {
        matchingRules_t *new_rmap = new matchingRules_t[size];
        memset(new_rmap, 0, sizeof(matchingRules_t)*size);
        if (rmap != NULL) {
            memcpy(new_rmap, rmap, sizeof(matchingRules_t)*rmap_size);
            saveDeleteArr(rmap);
        }
        rmap = new_rmap;	
        rmap_size = size;
    }
        
    // loop over all equiv classes from the last chunk
    for (unsigned short eq = 0; eq < (int) eqcl[phase][0].maxId; eq++) {
        if (eqcl[phase][0].eids[eq].refc > 0) {
            bitmap_t *bm = eqcl[phase][0].eids[eq].bm;

            rmap[eq].ruleCount = 0;

            for (unsigned short i = 0; i < (bm->msb+1)*BITS_PER_ELEM; i++) {
                if (bmTest(bm, i)) {
                    // bidir support
                    if (((i%2) == 1) && (bmTest(bm, i-1))) {
                        // if this is a backward rule and if forward rule bit is set
                        // ignore because forward already matches
                    } else {
                        // use id of forward rule entry
                        rmap[eq].rules[rmap[eq].ruleCount] = i/2;
                        rmap[eq].ruleCount++; 		       
                    }
                }
                if (rmap[eq].ruleCount == MAX_RULES_MATCH) {
                    throw Error("more than %d rules match at the same time", 
                                MAX_RULES_MATCH);
                }
            }
        }
    }

#ifdef DEBUG
    cout << "final chunk" << endl;
    for (unsigned int i = 0; i < cdata.phases[phase].chunks[0].entryCount; i++) {
        unsigned short indx = cdata.phases[phase].chunks[0].entries[i];
        cout << i << ": " << indx;
        cout << " -> ";
        for (unsigned short j = 0; j < rmap[indx].ruleCount; j++) {
            cout << rmap[indx].rules[j] << " ";
        }
        cout << endl;
    }
    cout << endl;
#endif
}

/* --------------------------------------------------------------------------------------*/

// add rules (currently no rules are installed 
void ClassifierRFC::addInitialRules(ruleDB_t *rules)
{
    //! number line used index
    int nlused[MAX_CHUNKS];
    int has_bspec = 0;
#ifdef PROFILING
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);
#endif

    // assume that nlines, cdata, eqcl and bms are initialized

    if ((int)rules->size() > MAX_RULES) {
        throw Error("the maximum number of rules is %d", MAX_RULES);
    }
    
    // initialize number lines
    memset(&nlines, 0, sizeof(numberLines_t));

    // get a list of all matches of all rules uniquely identified by offset,length
    // and initialize the number lines
    for (ruleDBIter_t ri = rules->begin(); ri != rules->end(); ++ri) {
        Rule *r = (*ri);
        
        // 1. add forward part, 2. add backward part (if bidir)
        for (unsigned short backward=0; backward <= r->isBidir(); backward++) {
            // check for any filter with backward specifications
            if (backward) {
                if ((has_bspec = hasBackwardSpec(r)) == 0) {
                    // this rule has no backward spec at all -> no backward rule
                    continue;
                }
            }
            
            for (filterListIter_t fi = r->getFilter()->begin(); fi != r->getFilter()->end(); ++fi) {
                // get the relevant stuff from the match
                unsigned short len = fi->len;
                unsigned short offs;
                refer_t ref;
                // bidir support
                if (backward && !fi->rname.empty()) {
                    offs = fi->roffs;
                    ref = fi->rrefer;
                } else {
                    offs = fi->offs;
                    ref = fi->refer;
                }

                // split filter into chunks
                // matches are either 1 byte long or multiple of 2 bytes
                unsigned short chunk_count = (len/2) + (len%2);
                for (unsigned short ch = 0; ch < chunk_count; ch++) {
                    unsigned short chunk_size = (len > (2*ch + 1)) ? 2 : 1;
                    // find number line for this offset
                    if (findNumberLine(ref, offs, ch, chunk_size) < 0) {
                        addNumberLine(ref, offs, ch, chunk_size, &fi->fdmask);
                    }
                }   
            }
        }
    }

    // project each match of the rule onto the number line, marking
    // start and end points
    // an exact match is start and end point
    // a range match is start and end point
    // a set match is a number of pairs of start and end points
    // wildcards matches (point at 0)
    for (ruleDBIter_t ri = rules->begin(); ri != rules->end(); ++ri) {
        Rule *r = (*ri);
        unsigned short rid = 0;
        
        // bidir support
        for (unsigned short backward=0; backward <= r->isBidir(); backward++) {
            // clear number line used bites
            memset(nlused, 0, sizeof(nlused));

            if (backward) {
                if (!has_bspec) {
                    continue;
                }
             
                // backward id
                rid =  r->getUId() * 2 + 1;
            } else {
                // forward id
                rid =  r->getUId() * 2 ;
            }
            
            for (filterListIter_t fi = r->getFilter()->begin(); fi != r->getFilter()->end(); ++fi) {
                // get the match values
                unsigned short len = fi->len;
                unsigned short offs;
                refer_t ref;
                // bidir support
                if (backward && !fi->rname.empty()) {
                    offs = fi->roffs;
                    ref = fi->rrefer;
                } else {
                    offs = fi->offs;
                    ref = fi->refer;
                }
                     
                unsigned short chunk_count = (len/2) + (len%2);
                for (unsigned short ch = 0; ch < chunk_count; ch++) {
                    unsigned short chunk_size = (len > (2*ch + 1)) ? 2 : 1;

                    // find number line for this offset
                    int chunk_id = findNumberLine(ref, offs, ch, chunk_size);   
                    // mark number line as used
                    nlused[chunk_id] = 1;
                    // now chunk_id points to the correct number line
                    // project points from current match
                    projMatch(rid, chunk_id, chunk_size, ch, &(*fi));
                }
            }
            
            // project this rule as FT_WILD on all number lines we have no explicit matches
            for (unsigned short i = 0; i < nlines.lineCount; i++) {
                if (!nlused[i]) {
                    addRuleToPoint(&nlines.lines[i].points[0], RULE_START, rid);
                }
            }
            
            // set rule bit in all rules
            bmSet(&allrules, rid);
        }
    }
    
#ifdef DEBUG
    printNumberLines();
#endif
    
    // PHASE 0
    
    // now scan through the number line looking for distinct equivalence
    // classes
    for (unsigned short i = 0; i < nlines.lineCount; i++) {
        unsigned short eq = 0;
        bitmap_t bmp;
        bmReset(&bmp);
              
        makeNewChunk(0, nlines.lines[i].line_size);

        for (unsigned int j = 0; j < nlines.lines[i].line_size; j++) {
            unsigned short rule_cnt = nlines.lines[i].points[j].ruleCount;

            if (rule_cnt > 0) {
                for (unsigned short k = 0; k < rule_cnt; k++) {
                    switch (nlines.lines[i].points[j].rules[k].type) {
                    case RULE_START:
                        // set rule bits
                        bmSet(&bmp,nlines.lines[i].points[j].rules[k].rid);
                        break;
                    case RULE_END:
                        // unset rule bits
                        bmReset(&bmp,nlines.lines[i].points[j].rules[k].rid);
                        break;
                    }
                }            
                eq = findEC(0, i, &bmp);
            }
            // else fill in the last eq            
            cdata.phases[0].chunks[i].entries[j] = eq;
        }
        
#ifdef PREALLOC_EQUIV_CLASSES
        // create PREALLOC_CLASSES unused equiv classes per phase 0 chunk
        addPreAllocEC(0, i, PREALLOC_CLASSES);
#endif

#ifdef DEBUG
        printChunk(0, i, 0);
#endif
    }

    // free line number space
    for (unsigned short i = 0; i < nlines.lineCount; i++) {
        for (unsigned int j=0; j < nlines.lines[i].line_size; j++) {
            if (nlines.lines[i].points[j].entryCount > 0) {
                saveDeleteArr(nlines.lines[i].points[j].rules);
            }
        }
        saveDeleteArr(nlines.lines[i].points);
    }

    // PHASE 1-n
   
    // determine the number of phases from the number of phase 0 chunks
    for (unsigned short j = 2; j < MAX_PHASES; j++) {
        if ((2 << (j-1)) >= cdata.phases[0].chunkCount) {
            cdata.phaseCount = j+1;
            break;
        }
    }

    if (cdata.phaseCount > MAX_PHASES) {
        throw Error("reduction not possible in %d phases", MAX_PHASES);
    }
    
    for (unsigned short i = 1; i < cdata.phaseCount; i++) {     
        // decide which chunks to combine from previous phase
        // FIXME simply combine them in numeric order for now
        
        // determine the reduction factor
        int rfac = cdata.phases[i-1].chunkCount / cdata.phaseCount;
        if (rfac < 2) {
            rfac = 2;
        }
        // current chunk index
        int cindx = 0;
        int size = 1;
        for (unsigned short j = 0; j <= cdata.phases[i-1].chunkCount; j++) {
            // start new chunk after rfac chunks and include single last chunk
            if (((j > 0) && ((j % rfac) == 0)) || (j == cdata.phases[i-1].chunkCount)) {
                // start next chunk
                makeNewChunk(i, size);
                cindx++;
                size = 1;
            }
            cdata.phases[i].chunks[cindx].parentChunks[cdata.phases[i].chunks[cindx].parentCount++] = j;
            size *= eqcl[i-1][j].maxId;
        }

#ifdef DEBUG
        for (unsigned short j = 0; j < cdata.phases[i].chunkCount; j++) {
            cout << "create chunk " << j << " from phase " << i-1 << " chunks ";
            for (unsigned short k = 0; k < cdata.phases[i].chunks[j].parentCount; k++) {
                cout << cdata.phases[i].chunks[j].parentChunks[k] << " ";
            }
            cout << endl;
        }
        cout << endl;
#endif
	
        // now combine the chunks
	
        // loop over all target chunks
        for (unsigned short j = 0; j < cdata.phases[i].chunkCount; j++) {
            // start the recursion with a bitmap where all bits are set
            bitmap_t bmp;
            bmSet(&bmp);
            int indx = 0;
            doIntersection(i, j, 0, &indx, &bmp);
        
#ifdef PREALLOC_EQUIV_CLASSES
            // add PREALLOC_CLASSES unused classes per chunk
            addPreAllocEC(i, j, PREALLOC_CLASSES);
#endif

#ifdef DEBUG	    
            printChunk(i, j, 1);
#endif
        }
    }
    
    // generate final rule map
    genFinalMap(cdata.phaseCount-1);
    
#ifdef PROFILING
    gettimeofday(&t2, NULL);
    
    cout << "Precomputation: " << endl;
    printProfiling(&t1, &t2);
#endif
          
}

/*------------------------------------------------------------------------------*/

// remap phase 0 chunk entries
void ClassifierRFC::remapIndex(int phase, int chunk, int from, int to, int rid)
{
    unsigned short eq = 0, oeq = 0;
    unsigned short remap[65535];

    // map old class to new class
    for (unsigned short i = 0; i < eqcl[phase][chunk].maxId; i++) {
        remap[i] = i;
    }
    
    for (unsigned short i = from; i < to; i++) {
        oeq = cdata.phases[phase].chunks[chunk].entries[i];
        eq = remap[oeq];
      
        if (eq == oeq) {
            // get new equiv class
            bitmap_t bmp = *eqcl[phase][chunk].eids[oeq].bm;
            bmSet(&bmp, rid);
            
            eq = findEC(phase, chunk, &bmp);

            // insert new mapping
            remap[oeq] = eq;
        } else {
            eqcl[phase][chunk].eids[eq].refc++;
        }

        // the old bitmap may have become unused
        // a preallocated bitmap was _never_ used before
        if (eqcl[phase][chunk].eids[oeq].refc > 0) {
            eqcl[phase][chunk].eids[oeq].refc--;
            
            // if ref counter = 0 put onto free list
            if (eqcl[phase][chunk].eids[oeq].refc == 0) {
                eqcl[phase][chunk].freeList.push_back(oeq);
                // delete from bitmap map
                eqcl[phase][chunk].bms.erase(eqcl[phase][chunk].eids[oeq].bm);
            }
        }
        
        // change this entry
        cdata.phases[phase].chunks[chunk].entries[i] = eq;
            
    }
}


void ClassifierRFC::addRule(Rule *r)
{
    int nlused[MAX_CHUNKS];
    int rid = 0;
    int oldChunkCount = cdata.phases[0].chunkCount;
#ifdef PROFILING
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);
#endif    

    if (stats->rules == MAX_RULES) {
        throw Error("max rule number exceeded");
    }

    // phase 0

    // clear nlused index
    memset(nlused, 0, sizeof(nlused));

    // bidir support
    for (unsigned short backward=0; backward <= r->isBidir(); backward++) {
        if (backward) {
            if (hasBackwardSpec(r) == 0) {
                // this rule has no backward spec at all -> no backward rule
                continue;
            }
           
            rid = r->getUId() * 2 + 1; 
        } else {
            rid = r->getUId() * 2 ;
        }

        // loop over all matches
        for (filterListIter_t fi = r->getFilter()->begin(); fi != r->getFilter()->end(); ++fi) {
            // get the match values
            unsigned short len = fi->len;
            filterType_t mtype = fi->mtype;
            unsigned short offs;
            refer_t ref;
            // bidir support
            if (backward && !fi->rname.empty()) {
                offs = fi->roffs;
                ref = fi->rrefer;
            } else {
                offs = fi->offs;
                ref = fi->refer;
            }

            unsigned short chunk_count = (len/2) + (len%2);
            for (unsigned short ch = 0; ch < chunk_count; ch++) {
                unsigned short chunk_size = (len > (2*ch + 1)) ? 2 : 1;

                // find number line for this offset
                int chunk = findNumberLine(ref, offs, ch, chunk_size);
                if (chunk < 0) {
                    bitmap_t bmp = allrules;
                    int eq = 0;
                    bmInfo_t bmi = {NULL, 1};

                    // add new chunk
                    addNumberLineDescr(ref, offs, ch, chunk_size, &fi->fdmask);
                    makeNewChunk(0, (chunk_size == 1) ? NUMBER_LINE_SIZE_8B : NUMBER_LINE_SIZE_16B);
                    chunk = cdata.phases[0].chunkCount-1;

                    // mark number line as used
                    nlused[chunk] = 1;

                    if (mtype == FT_WILD) {
                        // equiv class 1 is all rules + rid
                        bmSet(&bmp, rid);
                        
                        bmi.bm = new bitmap_t;
                        *bmi.bm = bmp;
                        
                        // only one equiv class
                        eq = eqcl[0][chunk].maxId++;
                        eqcl[0][chunk].bms[bmi.bm] = eq;
                        eqcl[0][chunk].eids.push_back(bmi);
                    } else {
                        // set all existing rules as equiv class 0
                        bmi.bm = new bitmap_t;
                        *bmi.bm = bmp;
              
                        eq = eqcl[0][chunk].maxId++;
                        eqcl[0][chunk].bms[bmi.bm] = eq;
                        eqcl[0][chunk].eids.push_back(bmi);

                        // and all existing + current rule as equiv class 1
                        bmSet(&bmp, rid);

                        bmi.bm = new bitmap_t;
                        *bmi.bm = bmp;
                     
                        eq = eqcl[0][chunk].maxId++;
                        eqcl[0][chunk].bms[bmi.bm] = eq;
                        eqcl[0][chunk].eids.push_back(bmi);
                        
                        projMatchDirectly(eq, chunk, chunk_size, ch, &(*fi));
                    }

#ifdef PREALLOC_EQUIV_CLASSES
                    // add PREALLOC unused equiv classes
                    addPreAllocEC(0, chunk, PREALLOC_CLASSES);
#endif
                } else {
                    // use existing chunk

                    // mark number line as used
                    nlused[chunk] = 1;
	
                    // change equiv classes and chunk data
                    projMatchRemap(rid, chunk, chunk_size, ch, &(*fi));
                }
            }
        }

        // project this rule as FT_WILD on all chunks we have no explicit matches
        for (unsigned short i = 0; i < cdata.phases[0].chunkCount; i++) {
            if (!nlused[i]) {
                for (chunkEqClArrayIter_t ei = eqcl[0][i].eids.begin(); ei != eqcl[0][i].eids.end(); ++ei) {
                    if (ei->refc > 0) {
                        bmSet(ei->bm, rid);
                    }
                }
            }
        }
    }

#ifdef DEBUG
    for (unsigned short i = 0; i < cdata.phases[0].chunkCount; i++) {
        printChunk(0, i, 0);
    }
#endif

    // if we have new phase 0 chunks adjust the chunk merging
    // merge the new chunks into chunk 0,1,2,..,n of phase 1
    // always add to chunk witch has the smallest number of parents
    // for performance reasons it should be avoided to get new chunks on inc adds!
    unsigned short new_chunks = cdata.phases[0].chunkCount - oldChunkCount;
    if (new_chunks > 0) {
        for (unsigned short i = oldChunkCount; i < cdata.phases[0].chunkCount; i++) {
            // find chunks with smallest number of parents
            unsigned short min = 0xFFFF;
            for (unsigned short j = 0; j < cdata.phases[1].chunkCount; j++) {
                if (cdata.phases[1].chunks[j].parentCount < min) {
                    min = cdata.phases[1].chunks[j].parentCount;
                }
            }

            // add
            cdata.phases[1].chunks[ch].parentChunks[cdata.phases[1].chunks[min].parentCount++] = i;

#ifdef DEBUG
            cout << "add phase 0 chunk " << i << " as parent to chunk " << min << endl;
#endif
        }
    }

    // phase 1-n

    // update phase 1-n chunk entries and equivalence classes
   
    for (unsigned short i = 1; i < cdata.phaseCount; i++) {	
        // loop over all target chunks
        for (unsigned short j = 0; j < cdata.phases[i].chunkCount; j++) {
            // realloc chunk size if necessary
            unsigned int size = 1;
            for (unsigned short k = 0; k < cdata.phases[i].chunks[j].parentCount; k++) {
                size *= eqcl[i-1][cdata.phases[i].chunks[j].parentChunks[k]].maxId;
            }
	   
            if (size != cdata.phases[i].chunks[j].entryCount) {
                unsigned short *new_entries = new unsigned short[size];
                memset(new_entries, 0, sizeof(unsigned short)*size);
                memcpy(new_entries, cdata.phases[i].chunks[j].entries, 
                       sizeof(unsigned short)*cdata.phases[i].chunks[j].entryCount);
                saveDeleteArr(cdata.phases[i].chunks[j].entries);
                cdata.phases[i].chunks[j].entries = new_entries;
                cdata.phases[i].chunks[j].entryCount = size;
            }

            // start the recursion with a bitmap where all bits are set
            bitmap_t bmp;
            bmSet(&bmp);
            int indx = 0;
            doIntersection(i, j, 0, &indx, &bmp);
            

#ifdef DEBUG
            printChunk(i, j, 1);
#endif
        }
    }

    // add new rule in final map
    genFinalMap(cdata.phaseCount-1);

    // add to all rules
    bmSet(&allrules, rid);

#ifdef PROFILING
    gettimeofday(&t2, NULL);

    cout << "Inc Add: " << endl;
    printProfiling(&t1, &t2);
#endif       
}


void ClassifierRFC::delRule(Rule *r)
{
    int rid = 0;
#ifdef REMAP_AFTER_DELETE
    unsigned short remap[65535];
    int do_remap = 0;
#endif
#ifdef PROFILING
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);
#endif    
 
    // incremental delete is done backwards
    // first the rule is deleted from the final rule map
    // (this would be sufficient for deleting but it would cause problems for later adds)
    // then it's deleted from all equiv classes
 
    for (unsigned short backward=0; backward <= r->isBidir(); backward++) {
        if (backward) {
            // backward id
            rid =  r->getUId() * 2 + 1;
        } else {
            // remove rule from final rule map
            for (unsigned int i = 0; i < rmap_size; i++) {
                int del = 0;
                
                for (unsigned short j = 0; j < rmap[i].ruleCount; j++) {
                    if (rmap[i].rules[j] == rid) {
                        del = 1;
                    } else if (del) {
                        rmap[i].rules[j-1] = rmap[i].rules[j];
                    } 
                }
                if (del) {
                    rmap[i].ruleCount--;
                }
            }
        
            // forward id
            rid =  r->getUId() * 2 ;
        }
   
        // delete rule bit from all equiv classes and remove old bitmaps from phase 0
        for (unsigned short i = 0; i < cdata.phases[0].chunkCount; i++) {
            for (unsigned short eq = 0; eq < eqcl[0][i].maxId; eq++) {
                bmInfo_t *bi = &eqcl[0][i].eids[eq];
#ifdef REMAP_AFTER_DELETE
                remap[eq] = eq;
#endif

                if ((bi->refc > 0) && bmTest(bi->bm, rid)) {                
                    // delete bitmap from bitmap map
                    eqcl[0][i].bms.erase(bi->bm);
                
                    // reset rule bit
                    bmReset(bi->bm, rid);

                    // check resulting bitmap
                    chunkBmListIter_t b = eqcl[0][i].bms.find(bi->bm);

                    if (b == eqcl[0][i].bms.end()) {
                        // add new class
                        eqcl[0][i].bms[bi->bm] = eq;
                    } else {
#ifdef REMAP_AFTER_DELETE
                        // map to new class
                        remap[eq] = b->second;
                        do_remap = 1;
#endif
                    }
                } 
            }

#ifdef REMAP_AFTER_DELETE
            if (do_remap) {
                for (unsigned int j = 0; j < cdata.phases[0].chunks[i].entryCount; j++) {
                    unsigned short oeq = cdata.phases[0].chunks[i].entries[j];
                    unsigned short eq = remap[oeq];
                    if (eq != oeq) {
                        if (eqcl[0][i].eids[oeq].refc > 0) {
                            eqcl[0][i].eids[oeq].refc--;
                        
                            // if ref counter = 0 put onto free list
                            if (eqcl[0][i].eids[oeq].refc == 0) {
                                eqcl[0][i].freeList.push_back(oeq);
                            }
                        }
                   
                        // change
                        cdata.phases[0].chunks[i].entries[j] = eq;

                        eqcl[0][i].eids[eq].refc++;
                    }
                }
            }
#endif

#ifdef DEBUG
            printChunk(0, i, 0);
#endif

        }

        // remove rule bit in all rules
        bmReset(&allrules, rid);
    
        stats->rules--;
    }

    if (stats->rules == 0) {
        cleanupData();
        initData();
    }

#ifdef DEBUG
    cout << "final chunk" << endl;
    if (cdata.phaseCount > 0) {
        for (unsigned int i = 0; i < cdata.phases[cdata.phaseCount-1].chunks[0].entryCount; i++) {
            unsigned short indx = cdata.phases[cdata.phaseCount-1].chunks[0].entries[i];
            cout << i << ": " << indx;
            cout << " -> ";
            for (unsigned short j = 0; j < rmap[indx].ruleCount; j++) {
                cout << rmap[indx].rules[j] << " ";
            }
            cout << endl;
        }
    }
    cout << endl;
#endif

  
#ifdef PROFILING
    gettimeofday(&t2, NULL);

    cout << "Inc Delete: " << endl;
    printProfiling(&t1, &t2);
#endif  
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, ClassifierRFC &cl )
{
    cl.dump(os);
    return os;
}



