
/*! \file  netai_arff.cc

    Modifications Copyright Daniel Arndt, 2010

    This version has been modified to provide additional output and 
    compatibility. For more information, please visit

    http://web.cs.dal.ca/~darndt

    Copyright 2005-2006 Swinburne University of Technology    

    This file is part of Network Traffic based Application Identification (netAI).

    netAI is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    netAI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this software; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 
    Description:
    export into ARFF file suitable for importing into WEKA 

    $Id: netai_arff.cc 97 2006-05-16 23:54:35Z szander $

*/

#include <pwd.h>
#include <sys/types.h>

#include "stdincpp.h"
#include "ConfigManager.h"
#include "./ExportModule.h"
#include "Rule.h"
#include "ConfigParser.h"
#include "../netmate/ProcModule.h"



// module global variables (common for all rules using the export module)

static string exportDir;
static string exportFilename;

// task specific variables (one such record is stored by netmate per task
//                          using the export module)

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

    fprintf(stderr, "netai_arff: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(code);
}

/* -------------------- initModule -------------------- */

int initModule( ConfigManager *confMan )
{
    exportDir = confMan->getValue("ExportDir", "EXPORTER", "netai_arff");
    // make sure traling / is present
    if (!exportDir.empty()) {
        if (exportDir[exportDir.length()] != '/') {
            exportDir += "/";
        }
    }
    exportFilename = confMan->getValue("ExportFilename", "EXPORTER", "netai_arff");

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
    // mark that expRecord is first use
    exportRecord_t *rec = new exportRecord_t;

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
    // called by netmate if module has timers defined - in module unused!
    return 0;
}


/* -------------------- writeData (local function) -------------------- */

inline static void writeData( ofstream &ofile, DataType_e type, const char *dpos )
{
    //fprintf(stderr, "%s %x \n", ProcModule::typeLabel(type), dpos);

    switch (type) {
    case CHAR: 
        ofile << *dpos;
        break;
    case INT8:
        ofile << (short)*dpos; 
        break;
    case UINT8:	
        ofile << (unsigned short)*((unsigned char*)dpos); 
        break;
    case INT16:
        ofile << *((short*)dpos); 
        break;
    case UINT16:
        ofile << *((unsigned short*)dpos); 
        break;
    case INT32:
        ofile << *((int32_t*)dpos); 
        break;
    case UINT32:
        ofile << *((uint32_t*)dpos); 
        break;
    case IPV4ADDR:
      {
          char buf[64];
          ofile << inet_ntop(AF_INET, dpos, buf, sizeof(buf));
      }
      break;
    case IPV6ADDR:
      {
          char buf[64];
          ofile << inet_ntop(AF_INET6, dpos, buf, sizeof(buf));
      }
      break;
    case INT64:
        ofile << *((int64_t*)dpos);
        break;
    case UINT64:
        ofile << *((uint64_t*)dpos); 
        break;
    case FLOAT:
        ofile << *((float*)dpos); 
        break;
    case DOUBLE:
        ofile << *((double*)dpos); 
        break;
    case STRING:   
        ofile << dpos; 
        break;
    case LIST:
        break;
    case BINARY:
      {
	  // get length
          unsigned int len = *((unsigned int *)dpos);
          dpos += sizeof(unsigned int);

          if (len > 0) {
            ofile << "0x" << hex << setfill('0');

            // print binary as hex bytes
            for (unsigned int i=0; i < len; i++) {
                  ofile << setw(2) << (short) *((unsigned char *)dpos);
                  dpos += 1;
            }
            ofile << dec;
          }
      }
      break;
    default:
        break;
    }
}


/* ------------------- exportMetricData (local function) ------------------- */

static string getType(DataType_e type)
{
    string typeStr;

    switch (type) {
    case CHAR:
    case STRING:
    case BINARY:
    case IPV4ADDR:
    case IPV6ADDR:
        typeStr = "STRING";
        break;
    case INT8:
    case UINT8:
    case INT16:
    case UINT16:
    case INT32:
    case UINT32:
    case INT64:
    case UINT64:
    case FLOAT:
    case DOUBLE:
        typeStr = "NUMERIC";
        break;
    default:
	typeStr = "STRING";
    }

    return typeStr;
}

static int writeArffHeader( string taskName, MetricData *mdata,
                             ofstream &ofile)
{
   int nrows;
   DataType_e type;
   const char *str;
   struct timeval tval;
   struct tm tm;
   char tstamp[64];

   gettimeofday(&tval,NULL);
   strftime(tstamp, sizeof(tstamp), "%Y-%m-%d-%H:%M:%S", localtime_r((time_t *)&tval.tv_sec, &tm));
 
   ofile << "@RELATION <netmate-" << tstamp << "-" << taskName << ">" << endl;
   ofile << endl;

   mdata->initExport();

   while ((nrows = mdata->getNextList()) > -1) {
        // print flow key names
        while ((str = mdata->getNextFlowKeyLabel(&type)) != NULL) {
            ofile << "@ATTRIBUTE " << str << " " << getType(type) << endl;
        }

        // print flow data field names
        while ((str = mdata->getNextFlowDataLabel(&type)) != NULL) {
            ofile << "@ATTRIBUTE " << str << " " << getType(type) << endl;
        }

        break;
   }
   
   ofile << endl;
   ofile << "% you need to add a nominal class attribute!" << endl;
   ofile << "% @ATTRIBUTE class {class0,class1}" << endl;
   ofile << endl;
   ofile << "@DATA" << endl << endl;

   return 0;
}

static int exportMetricData( string taskName, MetricData *mdata,
                             ofstream &ofile, int expFlowId, int final, int expFlowStatus )
{
    int nrows;
    DataType_e type;
    const char *str;
    int newFlow;
    unsigned long long flowId;

    mdata->initExport();

    while ((nrows = mdata->getNextList()) > -1) {
        while ((nrows = mdata->getNextFlow(&flowId, &newFlow)) > -1) {
            for (int i=0; i<nrows; i++) {
	        char c = '\0';

		if (expFlowId) {
		  ofile << flowId << ",";
		}
		if (expFlowStatus) {
		  ofile << final << ",";
		}
                
                // print flow key
                while((str = mdata->getNextFlowKey(&type)) != NULL) {
		  if (c != '\0') {
		    ofile << c;
		  }
		  c = ',';
		  writeData(ofile, type, str);
                }
                
                // print flow data
                while((str = mdata->getNextFlowDataRow(&type)) != NULL) {
		  if (c != '\0') {
                    ofile << c;
		  }
		  c = ',';
		  writeData(ofile, type, str);
                }
                ofile << endl;
            }
        }
    }
    return 0;
}


void createDir(string filename)
{  
    int successMkdir = 0;
    int i = 0;

    // create all directories above the file
    while ((i = filename.find('/', i+1)) != -1) {
        struct stat statbuf;
        if (!(stat(filename.substr(0, i).c_str(), &statbuf) == 0 &&
              (S_ISDIR(statbuf.st_mode)))) {
            successMkdir = mkdir(filename.substr(0, i).c_str(), 0777);
            if (successMkdir != 0) {
                die(1, "Unable to make directory for %s: %s", filename.c_str(), 
                    strerror(errno));
            }
        }
    }
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

#ifdef DEBUG
    cerr << "netai_arff::exportData (rule='" << frec->getRuleName() << "')" << endl;
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

#ifdef DEBUG
    cerr << "Export to file: " << filename << endl;
#endif

    if (!rec->multifile) {

        createDir(filename);
        ofstream ofile(filename.c_str(), ios::app);
        if (!ofile) {
		die(1, "Unable to open %s", filename.c_str());
        } 
        chown( filename.c_str(), rec->exportUID, rec->exportGID );

        // data from multiple packet proc modules can be in one FlowRecord
        while ((mdata = frec->getNextData()) != NULL) {
#ifdef DEBUG
	  cerr << "export from proc module: " << mdata->getModName() << endl;
#endif
          // if first time here for the exported task then
          // it is possible to run some special function
          if (rec->firstTime == 1) {
            // write arff header
            writeArffHeader(frec->getRuleName(), mdata, ofile);
            rec->firstTime = 0;
          }

          result += exportMetricData(frec->getRuleName(), mdata, ofile, rec->expFlowId, frec->isFinal(),
				       rec->expFlowStatus);
        }
        ofile.close();
    
    } else {  // multifile == 1

        // data from multiple packet proc modules can be in one FlowRecord
        while ((mdata = frec->getNextData()) != NULL) {

            string filename2 = filename + mdata->getModName();
	    
            createDir(filename2);
            ofstream ofile(filename2.c_str(), ios::app);
            chown( filename.c_str(), rec->exportUID, rec->exportGID );

#ifdef DEBUG	    
	    cerr << "export from proc module: " << mdata->getModName() << endl;
#endif
 
            // if first time here for the exported task then
            // it is possible to run some special function
            if (rec->firstTime == 1) {
              // write arff header
              writeArffHeader(frec->getRuleName(), mdata, ofile);
              rec->firstTime = 0;
            }

            result += exportMetricData(frec->getRuleName(), mdata, ofile, rec->expFlowId, frec->isFinal(),
				       rec->expFlowStatus);
            ofile.close();
        }
    }

    rec->firstTime = 0;

#ifdef PROFILING
    {
        unsigned long long end = PerfTimer::readTSC();
        cerr << "netai_arff::exportData took "
             << PerfTimer::ticks2ns(end - ini)/1000
             << " us" << endl; 
    }
#endif

    return result;
}	


char* getErrorMsg( int )
{
    return "200";
}


char* getModuleInfo( int i )
{
    return "201";
}


timers_t* getTimers()
{
    return NULL;
}

