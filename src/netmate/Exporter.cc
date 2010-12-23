
/*!\file   Exporter.cc

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

    $Id: Exporter.cc 748 2009-09-10 02:54:03Z szander $
*/

#include <PerfTimer.h>
#include <Exporter.h>


Exporter::Exporter(ConfigManager *cnf, int threaded, string moduleDir )
    : MeterComponent(cnf, "Exporter",threaded )
{

#ifdef DEBUG
    log->dlog(ch, "Starting");
#endif
 
    // when no moduledir is supplied in constructor then
    // use the value from the config file
    if (moduleDir.empty()) {
        moduleDir = cnf->getValue("ModuleDir", "EXPORTER");
    }

    frdb = new FlowRecordDB(threaded);
    
    loader = new ModuleLoader( cnf, moduleDir.c_str() /*basedir*/,
			       cnf->getValue("Modules","EXPORTER") /*modlist*/,
			       "Export" /*channel name prefix*/);
}


/* ------------------------- ~Exporter ------------------------- */

Exporter::~Exporter()
{
   
#ifdef DEBUG
    log->dlog(ch, "Shutdown");
#endif

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexLock(&maccess);
        stop();
        mutexUnlock(&maccess);
        mutexDestroy(&maccess);
    }
#endif

    // destroyExportRec for all rules
    for (exportActionListIter_t r = exports.begin(); r != exports.end(); r++) {
        for (expActionListIter_t i = r->mods.begin(); 
             i != r->mods.end(); i++) {
            if (i->expData != NULL) {
                i->mapi->destroyExportRec(i->expData);
            }
        }
    }

    saveDelete(frdb);

    // discard the Module Loader
    saveDelete(loader);
}


/* ------------------------- checkRules ------------------------- */

// check a ruleset (the export part)
void Exporter::checkRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;

    for (iter = rules->begin(); iter != rules->end(); iter++) {
        checkRule(*iter);
    }
}


/* ------------------------- addRules ------------------------- */

void Exporter::addRules( ruleDB_t *rules, EventScheduler *e )
{
    ruleDBIter_t iter;

    for (iter = rules->begin(); iter != rules->end(); iter++) {
        addRule(*iter, e);
    }
}


/* ------------------------- delRules ------------------------- */

void Exporter::delRules(ruleDB_t *rules)
{
    ruleDBIter_t iter;

    for (iter = rules->begin(); iter != rules->end(); iter++) {
        delRule(*iter);
    }
}


int Exporter::checkRule(Rule *r) 
{
    exportList_t     *exp;
    int               ruleId;
    expAction_t e;

    ruleId = r->getUId();
#ifdef DEBUG
    log->dlog(ch, "check Rule %s.%s", r->getSetName().c_str(), r->getRuleName().c_str());
#endif
    AUTOLOCK(threaded, &maccess);

    exp = r->getExport();

    try {
        for (exportListIter_t iter = exp->begin(); iter != exp->end(); iter++) {           
            Module *mod;
            string mname = iter->name;
            
            e.expData = NULL;
            e.module = NULL;
            
            // load an Export Module used by this rule
            mod = loader->getModule(mname.c_str());
            e.module = dynamic_cast<ExportModule*> (mod);
            
            if (e.module != NULL) { // is it an exporting kind of module

                e.mapi = e.module->getAPI();
		
                // init module, parse parameters
                e.mapi->initExportRec(iter->conf, &e.expData);
	   
                // free memory
                (e.mapi)->destroyExportRec(e.expData);
                e.expData = NULL;

                loader->releaseModule(e.module);
                e.module = NULL;
            }
        }
	
    } catch (Error &err ) {
        log->elog(ch, err);
	
        if (e.expData != NULL) {
            // free memory
            (e.mapi)->destroyExportRec(e.expData);
        }
	    
        //release packet processing modules already loaded for this rule
        if (e.module) {
            loader->releaseModule(e.module);
        }
       
        throw err;
    }
    return 0;
}


/* ------------------------- addRule ------------------------- */

int Exporter::addRule( Rule *r, EventScheduler *evs )
{
    exportActions_t   entry;
    exportList_t     *exp;
    int               ruleId;

    ruleId = r->getUId();
#ifdef DEBUG
    log->dlog(ch, "adding Rule #%d", ruleId );
#endif
    AUTOLOCK(threaded, &maccess);

    exp = r->getExport();
    entry.lastExp = 0;

    try {
	for (exportListIter_t iter = exp->begin(); iter != exp->end(); iter++) {

	    expAction_t e;
	    Module *mod;
	    string mname = iter->name;
	 
	    // load an Export Module used by this rule
	    mod = loader->getModule(mname.c_str());
	    e.module = dynamic_cast<ExportModule*> (mod);

	    if (e.module != NULL) { // is it an exporting kind of module
            
            e.mapi = e.module->getAPI();
            
            // add timer events once (only adds events on first module use)
            e.module->addTimerEvents( *evs );
            
            e.expData = NULL;
            // init module, parse parameters
            e.mapi->initExportRec(iter->conf, &e.expData);
            
            entry.mods.push_back(e);
	    }
	}
    
	// make sure the vector of rules is large enough
	if ((unsigned int)ruleId + 1 > exports.size()) {
	    exports.reserve( ruleId*2 + 1);
        exports.resize(ruleId + 1 );
	}
	// success ->enter struct into internal table
	exports[ruleId] = entry;
	
    } catch (Error &e ) {
        log->elog(ch, e);
        
        for (expActionListIter_t i = entry.mods.begin(); i != entry.mods.end(); i++) {
            // free memory
            (i->mapi)->destroyExportRec(i->expData);
            
            //release packet processing modules already loaded for this rule
            if (i->module) {
                loader->releaseModule(i->module);
            }
        }
        // empty the list itself
        entry.mods.clear();
        
        throw e;
    }
    return 0;
}


/* ------------------------- delRule ------------------------- */

int Exporter::delRule( Rule *r )
{
    exportActions_t  *entry;
    int               ruleId;

    ruleId = r->getUId();

    log->log(ch, "deleting Rule #%d", ruleId);
    
    AUTOLOCK(threaded, &maccess);
    
    entry = &exports[ruleId];
    
    // now free flow data and release used Modules
    for (expActionListIter_t i = entry->mods.begin(); i != entry->mods.end(); i++) {
        
        // dismantle export data structure with module function
        (i->mapi)->destroyExportRec(i->expData);
        
        // release modules loaded for this rule
        loader->releaseModule(i->module);
        
    }
    entry->mods.clear();

    // delete records from flow db
    frdb->delData(ruleId);

    return 0;
}


/* -------------------- exportFlowRecord -------------------- */

int Exporter::exportFlowRecord( FlowRecord *frec, expnames_t expmods )
{
    struct timeval when;

    AUTOLOCK(threaded, &maccess);

    // retrieve list of configured exporting modules
    exportActions_t *act = &exports[frec->getRuleId()];
    expActionList_t *exp = &act->mods;
    
    gettimeofday( &when, NULL );
    act->lastExp = when.tv_sec;

    // multiple export modules can be configured by one task
    for (expActionListIter_t iter2 = exp->begin();
         iter2 != exp->end(); iter2++ ) {
        
        if (expmods.empty() || expmods.find(iter2->module->getModName()) != expmods.end()) {
#ifdef DEBUG
            log->dlog(ch, "exporting flow record for rule '%s' via export module '%s'",
                      frec->getRuleName().c_str(),
                      iter2->module->getModName().c_str() );
#endif	
            // reset internal pointers
            frec->startExport();

            // give data block from processing module(s) to exp module
            iter2->mapi->exportData( frec, iter2->expData );
        }
    }

    return 0;
}


int Exporter::handleFDEvent(eventVec_t *e, fd_set *rset, fd_set *wset, fd_sets_t *fds)
{
    flowRec_t *rec;

    // get next entry from packet queue
    while ((rec = frdb->getNextRec()) != NULL) {
        if (rec->fr != NULL) {
            exportFlowRecord(rec->fr, rec->expmods);
        } 
	if (rec->fr->getDelete()) {
	  saveDelete(rec->fr);
	} else {
	  rec->fr->markForDelete();
	}
        saveDelete(rec);
	
#ifdef ENABLE_THREADS
	if (threaded && (frdb->getRecords() == 0)) {
	  threadCondSignal(&doneCond);
	}
#endif

    }

    return 0;
}


void Exporter::main()
{

    // this function will be run as a single thread inside the packet processor
    log->log(ch, "Exporter thread running");
    
    for (;;) {
        handleFDEvent(NULL, NULL,NULL, NULL);
    }
}   

void Exporter::waitUntilDone(void)
{
#ifdef ENABLE_THREADS
    AUTOLOCK(threaded, &maccess);

    if (threaded) {
      while (frdb->getRecords() > 0) {
	threadCondWait(&doneCond, &maccess);
      }
    }
#endif
}



/* ------------------------- dump ------------------------- */

void Exporter::dump( ostream &os )
{
    // FIXME to be done
    os << "Exporter dump : " << endl;
}


ostream& operator<< ( ostream &os, Exporter &obj )
{
    obj.dump(os);
    return os;
}
