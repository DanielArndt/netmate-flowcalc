
/*!\file   netmate/ExportModule.cc

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
    container class for export module information

    $Id: ExportModule.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "ExportModule.h"


/* ----- init static class members ----- */

Logger *ExportModule::s_log = NULL;
int     ExportModule::s_ch = 0;


/* ------------------------- ExportModule ------------------------- */

ExportModule::ExportModule( ConfigManager *conf, string libName,
			    string libFileName, libHandle_t libhandle ) : 
    Module( libName, libFileName, libhandle ), timersActive(0)
{
    int res;

    if (s_log == NULL ) {
        s_log = Logger::getInstance();
        s_ch = s_log->createChannel("ExportModule");
    }

#ifdef DEBUG
    s_log->dlog(s_ch, "Creating" );
#endif

    checkMagic(EXPORT_MAGIC);

    funcList = (ExportModuleInterface_t *) loadAPI( "func" );

    setOwnName(libName);

    res = funcList->initModule(conf);
    if (res != 0 ) {
        s_log->elog(s_ch, "initialization of module '%s' failed: %s", 
                    libName.c_str(), funcList->getErrorMsg(res) );
    }
}


/* ------------------------- ~ExportModule ------------------------- */

ExportModule::~ExportModule()
{
    funcList->destroyModule();
#ifdef DEBUG
    s_log->dlog(s_ch, "Destroyed" );
#endif
}


/* -------------------- addTimerEvents -------------------- */

void ExportModule::addTimerEvents( EventScheduler &evs )
{
    if (timersActive != 0) {
        return;
    }

    ExportModuleInterface_t *mapi = getAPI();
    timers_t *timers = mapi->getTimers();
    
    if (timers != NULL) {
        while (timers->flags != TM_END) {
            evs.addEvent(new ExportTimerEvent(mapi->timeout, timers++));
        }
    }
    timersActive = 1;
}


/* ------------------------- dump ------------------------- */

void ExportModule::dump( ostream &os )
{
    Module::dump(os);
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, ExportModule &obj )
{
    obj.dump(os);
    return os;
}

