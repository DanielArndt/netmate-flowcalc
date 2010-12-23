
/*! \file  testexp.cc

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
    used for testing export module functions

    $Id: testexp.cc 748 2009-09-10 02:54:03Z szander $

*/


#include "stdincpp.h"
#include "ConfigManager.h"
#include "./ExportModule.h"
#include "Rule.h"
#include "ConfigParser.h"


/*
  define here the timers which such a module is using on default
*/

timers_t timers[] = { /* handle, ival_msec, flags */
    { 1, 2000, 0 },
    { 2, 4000, TM_RECURRING },
    { 3, 6000, TM_ALIGNED },
    { 4, 8000, TM_RECURRING | TM_ALIGNED },
    { 5, 10000, TM_NONE /*==0*/ },
    TIMER_END
};


static int first = 0;


int initModule( ConfigManager *confMan )
{
    return 0;
}


int resetModule()
{
    return 0;
}


int destroyModule()
{
    return 0;
}


int initExportRec( configItemList_t conf, void **expRecord )
{
    fprintf(stderr, "testexp : initExportRec\n");
    first = time(NULL);
    return 0;
}


int destroyExportRec( void *expRecord )
{
    fprintf( stderr, "testexp : destroyFlowRec\n" );
    return 0;
}


int timeout( int timerID )
{
    fprintf( stderr, "testexp_module: timeout (timer id = %d,"
	     " time sice start = %ld seconds)\n\n",
	     timerID, time(NULL) - first );
    return 0;
}


int exportMetricData( MetricData *mdata )
{
    cout << "testexp::exportData (mod='" << mdata->getModName() << "')" <<endl;
    return 0;
}


int exportData( FlowRecord *frec, void *expData ) 
{
    MetricData *mdata;

    // data from multiple packet processing modules can be in one FlowRecord
    while ( (mdata = frec->getNextData()) != NULL ) {

        exportMetricData( mdata );
    }

    return 0;
}	



char* getErrorMsg( int )
{
    return "200";
}


char* getModuleInfo(int i)
{
    return "200";
}


timers_t* getTimers()
{
    return timers;
}

