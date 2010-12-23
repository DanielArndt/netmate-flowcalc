
/*! \file  bin_file.cc

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
export into bin_file (binary) file(s) 

$Id: bin_file.cc 748 2009-09-10 02:54:03Z szander $

*/

#include <pwd.h>
#include <sys/types.h>

#include "stdincpp.h"
#include "ConfigManager.h"
#include "./ExportModule.h"
#include "Rule.h"
#include "ConfigParser.h"



// module global variables (common for all rules using the bin_file export module)

static string exportDir;
static string exportFilename;

// task specific variables (one such record is stored by netmate per task
//                          using the bin_file export module)

typedef struct {
    short firstTime;  // flag: ==1 if this is the first export for a task
    short multifile;
    string exportFilename;
    uid_t exportUID;
    uid_t exportGID;
    int expFlowId;
    int expFlowStatus;
} exportRecord_t;


// FIXME how to throw exceptions from inside the shared lib?
void die(int code, char *fmt, ...)
{
    va_list           args;

    fprintf(stderr, "text_file: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(code);
}


/* -------------------- initModule -------------------- */

int initModule( ConfigManager *confMan )
{
    exportDir = confMan->getValue("ExportDir", "EXPORTER", "bin_file");
    // make sure traling / is present
    if (!exportDir.empty()) {
        if (exportDir[exportDir.length()] != '/') {
            exportDir += "/";
        }
    }
    exportFilename = confMan->getValue("ExportFilename", "EXPORTER", "bin_file");

    return 0;
}


/* -------------------- resetModule -------------------- */

int resetModule()
{
    return 0;
}


/* -------------------- destroyModule -------------------- */

int destroyModule()
{
    return 0;
}


/* -------------------- initExportRec -------------------- */

int initExportRec( configItemList_t conf, void **expRecord )
{
    exportRecord_t *rec = new exportRecord_t;

    // mark that expRecord is first use
    rec->firstTime = 1;
    rec->multifile       = (conf.getValue("Multifile") == "yes") ? 1 : 0;
    rec->exportFilename  =  conf.getValue("Filename");
    rec->expFlowId       = (conf.getValue("FlowID") == "yes") ? 1 : 0;
    rec->expFlowStatus   = (conf.getValue("ExportStatus") == "yes") ? 1 : 0; 

    // use filename from netmate config if no name in rule file
    if (rec->exportFilename.empty()) {
        rec->exportFilename = exportFilename;
    } 

    // check if we have an ExportUser specified in the config file
    string exportUserName = conf.getValue("ExportUser");
    if (exportUserName.empty()) {
        rec->exportUID = 0;
        rec->exportGID = 0;
    } else {
        struct passwd *pwEnt = getpwnam((const char *)exportUserName.c_str());
        if (pwEnt != NULL) {
            rec->exportUID = pwEnt->pw_uid;
            rec->exportGID = pwEnt->pw_gid;
        }
    }

    // store new record in location supplied by caller
    *((exportRecord_t **) expRecord) = rec;

    return 0;
}


/* -------------------- destroyExportRec -------------------- */

int destroyExportRec( void *expRecord )
{
    delete (exportRecord_t *) expRecord;
    return 0;
}


/* -------------------- timeout -------------------- */

int timeout( int id )
{
    // called by netmate if module has timers defined - in bin_file module unused!
    return 0;
}


/* -------------------- writeData (local function) -------------------- */

inline static void writeData( ofstream &ofile, DataType_e type, const char *dpos )
{
    switch (type) {
    case CHAR: 
    case INT8:
    case UINT8:	
    case INT16:
    case UINT16:
    case INT32:
    case UINT32:
    case INT64:
    case UINT64:
    case IPV4ADDR:
    case IPV6ADDR:
    case FLOAT:
    case DOUBLE:
	ofile.write(reinterpret_cast<const char *>(dpos), DataTypeSize[type]);
        break;
    case STRING:   
	{
	    // get length
	    unsigned int len = strlen((const char *)dpos);
	    ofile.write(reinterpret_cast<const char *>(dpos), len);
	}
	break;
    case BINARY:
	{
	    // get length
	    unsigned int len = *((unsigned int *)dpos);
            dpos += sizeof(unsigned int);
	    
	    if (len > 0) {
		ofile.write(reinterpret_cast<const char *>(dpos), len);
	    }
	}
	break;
    default: ;
    }
}


/* ------------------- exportMetricData (local function) ------------------- */

static int exportMetricData( string taskName, MetricData *mdata,
                             ofstream &ofile, int expFlowId, int final, int expFlowStatus )
{
    int nrows;
    DataType_e type;
    const char *str;
    int newFlow;
    unsigned long long flowId;

    mdata->initExport();

    while ((nrows = mdata->getNextList()) > 0) {
        while ((nrows = mdata->getNextFlow(&flowId, &newFlow)) > -1) {
            for (int i=0; i<nrows; i++) {

	        if (expFlowId) {
		  ofile.write((char *)&flowId, 8);
	        }
		if (expFlowStatus) {
		  ofile.write((char *)&final, 1);
		}
                
                // print flow key
                while((str = mdata->getNextFlowKey(&type)) != NULL) {
                    writeData(ofile, type, str);
                }
                
                // print flow data
                while((str = mdata->getNextFlowDataRow(&type)) != NULL) {
                    writeData(ofile, type, str);
                }
            }
        }
    }

    return 0;
}


/* -------------------- exportData (part of export API) -------------------- */

int exportData( FlowRecord *frec, void *expData ) 
{
    MetricData *mdata;
    exportRecord_t *rec;
    int result = 0;
    string filename;

#ifdef PROFILING
    unsigned long long ini = PerfTimer::readTSC();
#endif

    rec = (exportRecord_t *)expData;

    // use rule name as export file name if none was supplied explicitly
    if (rec->exportFilename.empty()) {
        filename = exportDir + frec->getRuleName();
    } else {
	if (rec->exportFilename[0] == '/') {
	    filename = rec->exportFilename;
	} else { // relative filename -> prepend export dir
	    filename = exportDir + '/' + rec->exportFilename;
	}
    }

    if (!rec->multifile) {

        // if first time here for the exported task then delete (old) file
        if (rec->firstTime == 1) {
            remove(filename.c_str());
            rec->firstTime = 0;
        }
        ofstream ofile(filename.c_str(), ios::app);
        if (!ofile) {
                die(1, "Unable to open %s", filename.c_str());
        }
        chown(filename.c_str(), rec->exportUID, rec->exportGID);
	
        // data from multiple packet proc modules can be in one FlowRecord
        while ((mdata = frec->getNextData()) != NULL) {

#ifdef DEBUG2
            cerr << "export from proc module: " << mdata->getModName() << endl;
#endif
            result += exportMetricData(frec->getRuleName(), mdata, ofile, rec->expFlowId, 
				       frec->isFinal(), rec->expFlowStatus);
        }
        ofile.close();
    
    } else {  // multifile == 1

        // data from multiple packet proc modules can be in one FlowRecord
        while ((mdata = frec->getNextData()) != NULL) {

            string filename2 = filename + mdata->getModName();
	    
            // if first time here for the exported task then delete (old) file
            if (rec->firstTime == 1) {
                remove( filename2.c_str());
                rec->firstTime = 0;
            }
            ofstream ofile(filename2.c_str(), ios::app);
            chown(filename.c_str(), rec->exportUID, rec->exportGID);
	    
#ifdef DEBUG2
            cerr << "export from proc module: " << mdata->getModName() << endl;
#endif
            result += exportMetricData(frec->getRuleName(), mdata, ofile, rec->expFlowId,
				       frec->isFinal(), rec->expFlowStatus);
            ofile.close();
        }
    }

#ifdef PROFILING
    {
        unsigned long long end = PerfTimer::readTSC();
        cerr << "bin_file::exportData took "
             << PerfTimer::ticks2ns(end - ini)/1000
             << " us" << endl; 
    }
#endif

    return result;
}	


char* getErrorMsg( int )
{
    return "not yet implemented";
}


char* getModuleInfo( int i )
{
    return "not yet implemented";
}


timers_t* getTimers()
{
    return NULL;
}

