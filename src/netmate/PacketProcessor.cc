
/*!\file   PacketProcessor.cc

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

    $Id: PacketProcessor.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "PacketProcessor.h"
#include "Module.h"
#include "ParserFcts.h"


//!\short  default number of packet buffers to reserve if none is configured in the meter config file (item: PacketQueueBuffers)
static const int  DEF_PACKET_BUFFERS = 2000;


/* ------------------------- PacketProcessor ------------------------- */

PacketProcessor::PacketProcessor(ConfigManager *cnf, int threaded, string moduleDir ) 
    : MeterComponent(cnf, "PacketProcessor", threaded),
      numRules(0), expt(NULL)
{
    string txt;
    
#ifdef DEBUG
    log->dlog(ch,"Starting");
#endif

    if (moduleDir.empty()) {
        if ((txt = cnf->getValue("ModuleDir", "PKTPROCESSOR")) != "") {
            moduleDir = txt;
        }
    }

    if ((txt = cnf->getValue("PacketQueueBuffers",  "PKTPROCESSOR")) != "") {
        queue = new PacketQueue(ParserFcts::parseULong(txt, 0), threaded);
    } else {
        queue = new PacketQueue(DEF_PACKET_BUFFERS, threaded);
    }

    try {
        loader = new ModuleLoader(cnf, moduleDir.c_str() /*module (lib) basedir*/,
                                  cnf->getValue("Modules", "PKTPROCESSOR"),/*modlist*/
                                  "Proc" /*channel name prefix*/);
    } catch (Error &e) {
        saveDelete(queue);
        throw e;
    }
}


/* ------------------------- ~PacketProcessor ------------------------- */

PacketProcessor::~PacketProcessor()
{

#ifdef DEBUG
    log->dlog(ch,"Shutdown");
#endif

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexLock(&maccess);
        stop();
        mutexUnlock(&maccess);
        mutexDestroy(&maccess);
    }
#endif

    // destroyFlowRecord for all rules
    for (ruleActionListIter_t r = rules.begin(); r != rules.end(); r++) {
        for (ppactionListIter_t i = r->actions.begin(); 
             i != r->actions.end(); i++) {
            if (i->flowData != NULL) {
                i->mapi->destroyFlowRec(i->flowData);
            }
            saveDeleteArr(i->params);
        }
        
        if (r->flowKeyLen > 0) {
            saveDeleteArr(r->flowKeyList);
        }

        if (r->auto_flows) {
            for (flowListIter_t i = r->flows->getFlows()->begin(); 
                 i != r->flows->getFlows()->end(); ++i) {
                for (ppactionListIter_t j = i->second.actions.begin(); j != i->second.actions.end(); j++) {
                    j->mapi->destroyFlowRec(j->flowData);
                    j->flowData = NULL; 
                    j->params = NULL;
                    
                    // FIXME disable timers
                }
            }

            saveDelete(r->flows);
        }
    }

    // discard the Module Loader
    saveDelete(loader);

    // destroy the packet queue
    saveDelete(queue);
}


// check a ruleset (the filter part)
void PacketProcessor::checkRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;
    
    for (iter = rules->begin(); iter != rules->end(); iter++) {
        checkRule(*iter);
    }
}


// add rules
void PacketProcessor::addRules( ruleDB_t *rules, EventScheduler *e )
{
    ruleDBIter_t iter;
   
    for (iter = rules->begin(); iter != rules->end(); iter++) {
        addRule(*iter, e);
    }
}


// delete rules
void PacketProcessor::delRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;

    for (iter = rules->begin(); iter != rules->end(); iter++) {
        delRule(*iter);
    }
}


int PacketProcessor::checkRule(Rule *r)
{
    int ruleId;
    actionList_t *actions;
    ppaction_t a;

    ruleId  = r->getUId();
    actions = r->getActions();

#ifdef DEBUG
    log->dlog(ch, "checking Rule %s.%s", r->getSetName().c_str(), r->getRuleName().c_str());
#endif  

    try {
        AUTOLOCK(threaded, &maccess);

        for (actionListIter_t iter = actions->begin(); iter != actions->end(); iter++) {
            Module *mod;
            string mname = iter->name;
       
            a.flowData = NULL;
            a.module = NULL;
            a.params = NULL;

            // load Action Module used by this rule
            mod = loader->getModule(mname.c_str());
            a.module = dynamic_cast<ProcModule*> (mod);

            if (a.module != NULL) { // is it a processing kind of module

                a.mapi = a.module->getAPI();

                // init module
                a.params = ConfigManager::getParamList(iter->conf);
                int ret = (a.mapi)->initFlowRec(a.params, &a.flowData);

                if (ret < 0) {
                    throw Error("Invalid parameters for module %s", mname.c_str());
                }

                saveDeleteArr(a.params);
                a.params = NULL;

                // free memory
                (a.mapi)->destroyFlowRec(a.flowData);
                a.flowData = NULL;

                //release packet processing modules already loaded for this rule
                loader->releaseModule(a.module);
                a.module = NULL;
            }
        }
    	
    } catch (Error &e) { 
        log->elog(ch, e);

        if (a.params != NULL) {
            saveDeleteArr(a.params);
        }

        // free memory
        if (a.flowData != NULL) {
            (a.mapi)->destroyFlowRec(a.flowData);
        }
            
        //release packet processing modules already loaded for this rule
        if (a.module) {
            loader->releaseModule(a.module);
        }
       
        throw e;
    }
    return 0;
}


/* ------------------------- addRule ------------------------- */

int PacketProcessor::addRule( Rule *r, EventScheduler *e )
{
    int ruleId;
    ruleActions_t entry;
    actionList_t *actions;

    ruleId  = r->getUId();
    actions = r->getActions();

#ifdef DEBUG
    log->dlog(ch, "adding Rule #%d", ruleId);
#endif  

    AUTOLOCK(threaded, &maccess);  

    entry.lastPkt = 0;
    entry.packets = 0;
    entry.bytes = 0;
    entry.flowKeyLen = 0;
    entry.flowKeyList = r->getFlowKeyList();
    entry.flist = r->getFilter();
    entry.auto_flows = r->isFlagEnabled(RULE_AUTO_FLOWS);
    entry.bidir = r->isBidir();
    entry.seppaths = r->sepPaths();
    entry.newFlow = 1;
    entry.rule = r;
    entry.flows = NULL;
    if (entry.auto_flows) {
        entry.flows = new FlowCreator();
    }

    try {

        int cnt = 0;
        for (actionListIter_t iter = actions->begin(); iter != actions->end(); iter++) {
            ppaction_t a;
            Module *mod;
            string mname = iter->name;
            	    
            // load Action Module used by this rule
            mod = loader->getModule(mname.c_str());
            a.module = dynamic_cast<ProcModule*> (mod);

            if (a.module != NULL) { // is it a processing kind of module

                a.mapi = a.module->getAPI();

                // init module
                a.params = ConfigManager::getParamList(iter->conf);
                a.flowData = NULL;
                int ret = (a.mapi)->initFlowRec(a.params, &a.flowData);

                // if packet proc modules requires bidir matching
                // then set rule to bidir
                // FIXME not a well defined method...
                if (ret == 1) {
                    r->setBidir();
                }

                // init timers
                addTimerEvents(ruleId, cnt, a, *e);
	
                entry.actions.push_back(a);
            }

            cnt++;
        }
    
        // make sure the vector of rules is large enough
        if ((unsigned int)ruleId + 1 > rules.size()) {
            rules.reserve(ruleId*2 + 1);
            rules.resize(ruleId + 1 );
        }
        // success ->enter struct into internal table
        rules[ruleId] = entry;
	
    } catch (Error &e) { 
        log->elog(ch, e);
	
        for (ppactionListIter_t i = entry.actions.begin(); 
             i != entry.actions.end(); i++) {

            saveDelete(i->params);

            (i->mapi)->destroyFlowRec(i->flowData);
	    
            //release packet processing modules already loaded for this rule
            if (i->module) {
                loader->releaseModule(i->module);
            }
        }
        // empty the list itself
        entry.actions.clear();

        throw e;
    }
    return 0;
}


/* ------------------------- delRule ------------------------- */

int PacketProcessor::delRule( Rule *r )
{
    ruleActions_t *ra;
    int ruleId = r->getUId();

#ifdef DEBUG
    log->dlog(ch, "deleting Rule #%d", ruleId);
#endif

    AUTOLOCK(threaded, &maccess);

    ra = &rules[ruleId];

    if (ra->auto_flows) {
        for (flowListIter_t i = ra->flows->getFlows()->begin(); 
             i != ra->flows->getFlows()->end(); ++i) {
            for (ppactionListIter_t j = i->second.actions.begin(); j != i->second.actions.end(); j++) {
                j->mapi->destroyFlowRec(j->flowData);
                j->flowData = NULL; 
                j->params = NULL;

                // FIXME disable timers
            }
        }

        saveDelete(ra->flows);
    }


    // now free flow data and release used Modules
    for (ppactionListIter_t i = ra->actions.begin(); i != ra->actions.end(); i++) {

        // dismantle flow data structure with module function
        i->mapi->destroyFlowRec(i->flowData);
        i->flowData = NULL;
        
        if (i->params != NULL) {
            saveDeleteArr(i->params);
            i->params = NULL;
        }

        // release modules loaded for this rule
        loader->releaseModule(i->module);
        
        // FIXME disable timers
    }
    
    ra->actions.clear();
    ra->lastPkt = 0;
    ra->flowKeyLen = 0;
    saveDeleteArr(ra->flowKeyList);
    ra->flist = NULL;
    ra->auto_flows = 0;
    ra->bidir = 0;
    ra->seppaths = 0;
    ra->newFlow = 0;
    ra->rule = NULL;
    if (ra->flows != NULL) {
        saveDelete(ra->flows);
    }

    return 0;
}


/* ------------------------- processPacket ------------------------- */


int PacketProcessor::processPacket(metaData_t *meta)
{

#ifdef PROFILING
    unsigned long long ini, end;
#endif
    ruleActions_t *ra;
    ppactionList_t *acts;

    AUTOLOCK(threaded, &maccess);

    // loop over all the rules matched
    for (int i = 0; i<meta->match_cnt; i++) {
        int ruleId = meta->match[i];

        ra = &rules[ruleId];

        // make sure the rule still exists
        if (!ra->actions.empty()) {
	    flowInfo_t *flow = NULL;
            //log->dlog(ch,"processing packet for Rule #%d", ruleId);

            // account number of packets and bytes for this task
            ra->packets++;
            ra->bytes += meta->cap_len;

            if (ra->auto_flows) {
	        int mval_len = 0, rmval_len = 0;
                unsigned char mvalues[1024];
		unsigned char rmvalues[1024];
		unsigned char tmp[MAX_FILTER_LEN];

                // get matching attributes from packet
                for (filterListIter_t fi = ra->flist->begin(); fi != ra->flist->end(); ++fi) {

                    unsigned char *pval = (unsigned char *) &meta->payload[ meta->offs[fi->refer] + fi->offs ]; 

                    // get masked pkt value
                    for (int i = 0; i < fi->len; i++) {
		        tmp[i] = pval[i] & fi->fdmask.getValue()[i] & fi->mask.getValue()[i];
                    }

		    // if pkt value was only one byte then shift the value 
		    // downwards according to filter def mask (e.g. 0x02 -> 1bit, 0x10 -> 4bits)
		    if (fi->len == 1 && fi->fdshift != 0) {
			tmp[0] = tmp[0] >> fi->fdshift;
		    }

		    // FIXME the following 'switch' is terribly inefficient
		    if (fi->type == "String") {
		       memcpy(&mvalues[mval_len], tmp, fi->len);
		       mval_len += fi->len;
		       // insert final terminator
		       mvalues[mval_len] = '\0';
		       mval_len++;
		    } else if (fi->type == "Binary") {
		      // insert length first
		      *((unsigned int *) &mvalues[mval_len]) = fi->len;
		      mval_len += sizeof(unsigned int);
		      memcpy(&mvalues[mval_len], tmp, fi->len);
		      mval_len += fi->len;
		    } else {
		      memcpy(&mvalues[mval_len], tmp, fi->len);
		      mval_len += fi->len;
		    }
		    
		    if (ra->bidir) {

		      if (!fi->rname.empty()) {
			// get reverse value
			unsigned char *pval = (unsigned char *) &meta->payload[ meta->offs[fi->rrefer] + fi->roffs ]; 
			// get masked pkt value
			for (int i = 0; i < fi->len; i++) {
			  tmp[i] = pval[i] & fi->fdmask.getValue()[i] & fi->mask.getValue()[i];
			}	
		      } 

		      // FIXME the following 'switch' is terribly inefficient
		      if (fi->type == "String") {
			memcpy(&rmvalues[rmval_len], tmp, fi->len);
			rmval_len += fi->len;
			// insert final terminator
			rmvalues[rmval_len] = '\0';
			rmval_len++;
		      } else if (fi->type == "Binary") {
			// insert length first
			*((unsigned int *) &rmvalues[rmval_len]) = fi->len;
			rmval_len += sizeof(unsigned int);
			memcpy(&rmvalues[rmval_len], tmp, fi->len);
			rmval_len += fi->len;
		      } else {
			memcpy(&rmvalues[rmval_len], tmp, fi->len);
			rmval_len += fi->len;
		      }
		    }
                }

		if (ra->seppaths) {
		  mvalues[mval_len] = 1;
		  mval_len++;
		  rmvalues[rmval_len] = 1;
		  rmval_len++;
		}

		flowInfo_t *fi = ra->flows->getFlow(mvalues, mval_len);

		if ((fi == NULL) && ra->bidir) {
		  // try reverse match
		  fi = ra->flows->getFlow(rmvalues, rmval_len);

		  if (fi != NULL) {
		    // set backward indication
		    meta->reverse = 1;

		    if (ra->seppaths) {
		      // generate extra flow entry for reverse path
		      rmvalues[rmval_len-1] = 2;
		      fi = ra->flows->getFlow(rmvalues, rmval_len);
		      memcpy(mvalues, rmvalues, rmval_len);
		      mval_len = rmval_len;
		    }
		  } else {
		    // this is the first packet of a new flow
		    if (meta->reverse) {
		      // according to the classifier its in reverse direction -> swap directions
		      memcpy(mvalues, rmvalues, rmval_len);
		      mval_len = rmval_len;
                    }
#ifdef SWAP_HACK
		    else if ((meta->layers[2] == T_UDP) || (meta->layers[2] == T_TCP)) {
		      // use heuristic: swap direction if udp/tcp source port is well-known but dst port is not
		      unsigned short src_port = ntohs(*((unsigned short *) &meta->payload[meta->offs[2]]));
		      unsigned short dst_port = ntohs(*((unsigned short *) &meta->payload[meta->offs[2]+2]));
		      if ( (src_port < 1024) && (dst_port >= 1024) ) { 
			memcpy(mvalues, rmvalues, rmval_len);
			mval_len = rmval_len;
			// and change direction to backward!
			meta->reverse = 1;
		      }
		    }
#endif
		  } 
		}
		
		if (fi == NULL) {
		  // add new flow
		  fi = ra->flows->addFlow(mvalues, mval_len);
                  
		  // initialize proc modules
		  for (ppactionListIter_t i = ra->actions.begin(); i != ra->actions.end(); i++) {
		    ppaction_t a;
		    
		    a.mapi = i->mapi;
		    a.module = i->module;
		    a.params = i->params;
		    
		    a.flowData = NULL;
		    (a.mapi)->initFlowRec(a.params, &a.flowData);
		    fi->actions.push_back(a);
		  }
		} 

                ra->flows->setLastTime(fi, meta->tv_sec + 1);

                ra->flowKeyLen = mval_len;

                acts = &fi->actions;

		flow = fi;
            } else {
                acts = &ra->actions;
            }
	    
	    int reset = 0;

            // apply all registered evaluation modules for the specified rule
            for (ppactionListIter_t i = acts->begin(); i != acts->end(); i++) {
#ifdef PROFILING
                ini = PerfTimer::readTSC();
#endif

                int doExport = (i->mapi)->processPacket((char *)meta->payload, meta, i->flowData);

		// flow can trigger its immediate export
		if (doExport == 1) {
		  int            size = 0;
		  unsigned char *data = NULL;
		  FlowRecord *frec = new FlowRecord(ruleId, ra->rule->getRuleName(), 1);
		  MetricData *md = new MetricData(i->module->getModName(), i->module->getExportLists(), 
						  ra->flowKeyList, 0, NULL, 0, NULL);
#ifdef DEBUG
		  log->dlog(ch, "querying processing module '%s' for rule %i", 
			    i->module->getModName().c_str(), ruleId);
#endif              
		  
		  // fetch export data from processing module
		  i->mapi->exportData((void**)&data, &size, i->flowData);
		  
		  if (ra->auto_flows) {
		    assert(flow != NULL);
		    md->addFlowData(size, data, ra->flowKeyLen, flow->keyData, 
				    flow->newFlow, flow->flowId);
		  } else {
		    md->addFlowData(size, data, ra->flowKeyLen, NULL, 0, 0);
		  }
		  
		  // reset module data
		  i->mapi->destroyFlowRec(i->flowData);
		  
		  // add data to flow record
		  frec->addData(md);
		  
		  // get all export modules names (FIXME put this list together before)
		  exportList_t *exps = ra->rule->getExport();
		  expnames_t enames; 
		  for (exportListIter_t e = exps->begin(); e != exps->end(); e++) {
		    enames.insert(e->name);
		  }
		  // give to Exporter
		  if (expt != NULL) {
		    expt->storeData(ruleId, enames, frec);
		  }

		  // flows that export here are reset after all action modules are
		  // executed
		  reset = 1;
		}
		
#ifdef PROFILING
                end = PerfTimer::readTSC();	      
                perf->account(MS_MODULE, end - ini);
#endif
            }

	    // #if 0
	    if (reset == 1) {
	      if (ra->auto_flows) {
		// delete flow
		assert(flow != NULL);
		flow->newFlow = 0;
		ra->flows->deleteFlow(flow);
	      } else {
		ra->newFlow = 0;
	      }
	    }
	    // #endif
	    
            // save time for last packet of this flow (for idle flow detection)
            ra->lastPkt = meta->tv_sec + 1; // (rounded up)

        }
    }

#ifdef PROFILING
    {
        static int n = 0;
        if (++n == 100000) {
            cerr << "process packet in " << perf->latest(MS_MODULE) << "(" <<
              perf->avg(MS_MODULE) << ") ns " << endl;
            n = 0;
        }
    }
#endif
    return 0;
}


int PacketProcessor::handleFDEvent(eventVec_t *e, fd_set *rset, fd_set *wset, fd_sets_t *fds)
{
    metaData_t *meta;

    // get next entry from packet queue
    meta = queue->readBuffer(threaded);
    if (meta) {
        processPacket(meta);
        queue->releaseBuffer();
	// restart waiting meter
#if ENABLE_THREADS
	if (threaded && (queue->getUsedBuffers() == 0)) {
	  threadCondSignal(&doneCond);
	}
#endif
	return 1;
    }

    return 0;
}

void PacketProcessor::main()
{

    // this function will be run as a single thread inside the packet processor
    log->log(ch, "PacketProcessor thread running");
    
    for (;;) {
        handleFDEvent(NULL, NULL,NULL, NULL);
    }
}       

void PacketProcessor::waitUntilDone(void)
{
#ifdef ENABLE_THREADS
    AUTOLOCK(threaded, &maccess);

    if (threaded) {
      while (queue->getUsedBuffers() > 0) {
        threadCondWait(&doneCond, &maccess);
      }
    }
#endif
}

// This functions triggers the export of all rules into the flow record
// if now is > 0 only idle flows are exported and their flow data reset
// assert: if now > 0 than at least one flow has timeout
int PacketProcessor::exportRule( FlowRecord *frec, time_t now, unsigned long ival)
{
    int            size = 0;
    unsigned char *data = NULL;
    ruleActions_t *ra;

    AUTOLOCK(threaded, &maccess);

    ra = &rules[frec->getRuleId()];

    if (ra->auto_flows) {
        int cnt = 0, flow = 0;
        MetricData *md[ra->actions.size()];

         // fetch flow data from registered packet processing modules for this rule
        for (ppactionListIter_t i = ra->actions.begin(); i != ra->actions.end(); i++) {
            md[cnt] = new MetricData(i->module->getModName(), i->module->getExportLists(), 
                                     ra->flowKeyList, 0, NULL, 0, NULL);
            cnt++;
        }

        flowListIter_t tmp;
        flowListIter_t f = ra->flows->getFlows()->begin();
        while (f != ra->flows->getFlows()->end()) {
            cnt = 0;
            tmp = f;
            f++;
	  
	    if ((now == 0) || ((time_t)(tmp->second.lastPkt + ival) <= now)) { 
	      for (ppactionListIter_t j = tmp->second.actions.begin(); j != tmp->second.actions.end(); ++j) {
#ifdef DEBUG
                log->dlog(ch, "querying processing module '%s' for rule %i and flow %i", 
                          j->module->getModName().c_str(), frec->getRuleId(), flow);
#endif              
		
                // fetch export data from processing module
                j->mapi->exportData((void* *)&data, &size, j->flowData);
		
                md[cnt]->addFlowData(size, data, ra->flowKeyLen, tmp->second.keyData, 
                                     tmp->second.newFlow, tmp->second.flowId);
                tmp->second.newFlow = 0;

                if (now > 0) {
		  j->mapi->destroyFlowRec(j->flowData);
                }
 
                cnt++;
	      }

	      if (now > 0) {
                // delete flow
                ra->flows->deleteFlow(tmp);
	      }
	    }

            flow++;
        }

        cnt = 0;
        for (ppactionListIter_t i = ra->actions.begin(); i != ra->actions.end(); i++) {
            frec->addData(md[cnt]);
            cnt++;
        }
    } else {
        MetricData *md;

        // fetch flow data from registered packet processing modules for this rule
        for (ppactionListIter_t i = ra->actions.begin(); i != ra->actions.end(); i++) {

#ifdef DEBUG
            log->dlog(ch, "querying processing module '%s' for rule %i", 
                      i->module->getModName().c_str(), frec->getRuleId());
#endif
      
            // fetch export data from processing module
            i->mapi->exportData((void* *)&data, &size, i->flowData);
            
            // store export data into flow record container object
            md = new MetricData(i->module->getModName(), i->module->getExportLists(), 
                                NULL, size, data, 0, NULL, ra->newFlow);

            // if requested: reset intermediate flow data using processing module
            // and reset flow to idle status 
            if (now > 0) {
                i->mapi->resetFlowRec(i->flowData);
            }

            frec->addData(md);
        }  

        ra->newFlow = 0;
    }

    if ((time_t)(ra->lastPkt + ival) <= now) {
        ra->lastPkt = 0;
        ra->flowKeyLen = 0;
    }

    return 0;
}


FlowRecord *PacketProcessor::exportRule(int rid, string rname, time_t now, unsigned long ival)
{
  FlowRecord *f = new FlowRecord(rid, rname);

  exportRule(f, now, ival);

  return f;
}

/* ------------------------- ruleTimeout ------------------------- */

// return 0 (if timeout), 1 (stays idle), >1 (active and no timeout yet)
unsigned long PacketProcessor::ruleTimeout(int ruleID, unsigned long ival, time_t now)
{
    AUTOLOCK(threaded, &maccess);

    time_t last = rules[ruleID].lastPkt;

    if (last > 0) {
        ruleActions_t *ra = &rules[ruleID];

        if (ra->auto_flows) {
            // has any of the auto flows expired? (this is only accurate to +-1s)
            flowListIter_t f = ra->flows->getFlows()->begin(); 
            while (f != ra->flows->getFlows()->end()) {
                if ((time_t)(f->second.lastPkt + ival) <= now) { 
		  // expired -> export
#ifdef DEBUG
		  log->dlog(ch,"auto flow idle, export: YES");
#endif
                  return 0;
                }

		f++;
            }		    
        } else {
	  // check if timeout hasn't expired for the rule
	  if ((time_t)(last + ival) > now) {
            
#ifdef DEBUG
            log->dlog(ch,"flow idle for %d seconds, export: NO",
                      (int)(now-last));
#endif		
	    
            return last;
	  } else { 
	    
#ifdef DEBUG
            log->dlog(ch, "flow idle for %d seconds, export: YES",
                      (int)(now-last));
#endif		
	    
            return 0;
	  }        
	}
    }

    return 1;
}

string PacketProcessor::getInfo()
{
    ostringstream s;

    AUTOLOCK(threaded, &maccess);

    /* FIXME delete? 
     * 
     * uncomment to print out which task uses what modules

    for (unsigned int j = 0; j<rules.size(); j++) {
	
	if (rules[ j ].actions.size()>0) {
	    
    s << "rule with id #" << j << " uses " 
    << rules[ j ].actions.size() << " actions :";
	    
    s << " ";
    for (ppactionListIter_t i = rules[j].actions.begin(); i != rules[j].actions.end(); i++) {
    s << (i->module)->getModName();
    s << ",";
    }
    s << endl;
	}
    }
    */

    s << loader->getInfo();  // get the list of loaded modules

    return s.str();
}



/* -------------------- addTimerEvents -------------------- */

void PacketProcessor::addTimerEvents( int ruleID, int actID,
                                      ppaction_t &act, EventScheduler &es )
{
    timers_t *timers = (act.mapi)->getTimers(act.flowData);

    if (timers != NULL) {
        while (timers->flags != TM_END) {
            es.addEvent(new ProcTimerEvent(ruleID, actID, timers++));
        }
    }
}

// handle module timeouts
void PacketProcessor::timeout(int rid, int actid, unsigned int tmID)
{
    ppaction_t *a;
    ruleActions_t *ra;

    AUTOLOCK(threaded, &maccess);

    ra = &rules[rid];

    if (!ra->auto_flows) {
        a = &ra->actions[actid];
        a->mapi->timeout(tmID, a->flowData);
    } else {       
        for (flowListIter_t f = ra->flows->getFlows()->begin(); f != ra->flows->getFlows()->end(); ++f) {
            a = &f->second.actions[actid];
            a->mapi->timeout(tmID, a->flowData);
        }
    }
}

/* -------------------- getModuleInfoXML -------------------- */

string PacketProcessor::getModuleInfoXML( string modname )
{
    AUTOLOCK(threaded, &maccess);
    return loader->getModuleInfoXML( modname );
}


/* ------------------------- dump ------------------------- */

void PacketProcessor::dump( ostream &os )
{

    os << "PacketProcessor dump :" << endl;
    os << getInfo() << endl;

}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, PacketProcessor &pe )
{
    pe.dump(os);
    return os;
}
