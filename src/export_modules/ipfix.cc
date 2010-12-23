
/*! \file  ipfix.cc

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
export data via ipfix

$Id: ipfix.cc 748 2009-09-10 02:54:03Z szander $

*/

#include <sys/types.h>

#include "stdincpp.h"
#include "ConfigManager.h"
#include "./ExportModule.h"
#include "Rule.h"
#include "ConfigParser.h"
#include "ipfix.h"
#include "ipfix_fields.h"

#define MAX_CONN 16

// defaults
const int DEF_SOURCEID  = 12345;
const string DEF_CHOST  = "localhost";
const int DEF_PORT      = 6766;
const int DEF_PROTO     = IPFIX_PROTO_TCP;
const string DEF_MFILE  = DEF_SYSCONFDIR "/ipfix_mapping.txt";


// list of templates
typedef map<string, ipfix_template_t*> templList_t;
typedef map<string, ipfix_template_t*>::iterator templListIter_t; 

// map attributes by string to ipfix attribute number
typedef map<string, unsigned short> attrMap_t;
typedef map<string, unsigned short>::iterator attrMapIter_t;

// per rule data
typedef struct {
    ipfix_template_t *keyTmpl;
    templList_t metricTmplList;
    // connection
    int conn;
} exportRecord_t;

typedef struct {
    // ref counter
    int ref;
    // conn details
    string chost;
    int port, proto;
    ipfix_t *ipfixh;
} conn_t;


// globals
int sourceid, proto;
string mfile;
attrMap_t attrMap;
conn_t conns[MAX_CONN];


// FIXME how to throw exceptions from inside the shared lib?
void die(int code, char *fmt, ...)
{
    va_list           args;

    fprintf(stderr, "ipfix: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(code);
}


// parse the attribute name to ipfix number mappping
void readMap(void)
{
    int p1, p2;

    string line;
    ifstream in(mfile.c_str());
    
    if (!in) {
        die(1, "cannot access/read mapping file '%s'", mfile.c_str());
    }

    while (getline(in, line)) {
        if ((line[0] != '#') && (!line.empty())) {
            p1 = line.find_first_of(" \t");
            p2 = line.find_first_not_of(" \t", p1);
         
            if ((p1 > 0) && (p2 > 0) && (p2 >= p1)) {
                unsigned int num = atoi(line.substr(p2,line.length()-p2).c_str());

                // look if this attribute exists
                int c = 0;
                int found = 0;
                while (ipfix_fields[c].ftid != 0) {
                    if (ipfix_fields[c].ftid == (int) num) {
                        found = 1;
                        break;
                    }
                    c++;
                }

                if (!found) {
                    die(1, "no ipfix attribute with field id '%d' defined", num);
                }

                attrMap[line.substr(0, p1)] = num;
            }
        }
    }

#ifdef DEBUG2
    for (attrMapIter_t i = attrMap.begin(); i != attrMap.end(); i++) {
        cerr << i->first << "->" << i->second << endl;
    }
#endif
}

/* --------------------------- IPFIX xonnection functions ------------------- */

void openConn(int conn, string chost, int port)
{

    if (ipfix_open(&conns[conn].ipfixh, sourceid, IPFIX_VERSION_NF9) < 0) {
        die(1, "ipfix_open() failed: %s", strerror(errno));
    }
    
    if (ipfix_add_collector(conns[conn].ipfixh, (char *) chost.c_str(), port, proto) < 0) {
        die(1, "ipfix_add_collector() failed: %s", strerror(errno));
    }
    
    conns[conn].chost = chost;
    conns[conn].port = port;
    conns[conn].ref++; 
}

void closeConn(int conn)
{
  
    ipfix_close( conns[conn].ipfixh );
    conns[conn].ref = 0;
}

void closeAllConn()
{
    for (int i=0; i<MAX_CONN; i++) {
        if (conns[i].ref > 0) {
            closeConn(i);
        }
    }
}

/* -------------------- initModule -------------------- */

int initModule( ConfigManager *confMan )
{
    string sproto, chost;
    int port;

    if (!confMan->getValue("SourceID", "EXPORTER", "ipfix").empty()) {
        sourceid = atoi(confMan->getValue("SourceID", "EXPORTER", "ipfix").c_str());
    } else {
        sourceid = DEF_SOURCEID;
    }

    if (!confMan->getValue("Collector", "EXPORTER", "ipfix").empty()) {
        chost = confMan->getValue("Collector", "EXPORTER", "ipfix");
    } else {
        chost = DEF_CHOST;
    }

    if (!confMan->getValue("Port", "EXPORTER", "ipfix").empty()) {
        port = atoi(confMan->getValue("Port", "EXPORTER", "ipfix").c_str());
    } else {
        port = DEF_PORT;
    }

    sproto = confMan->getValue("Protocol", "EXPORTER", "ipfix");
    if (!sproto.empty()) {
        if (sproto == "UDP") {
            proto = IPFIX_PROTO_UDP;
        } else if (sproto == "TCP") {
            proto = IPFIX_PROTO_TCP;
        }
    } else {
        proto = DEF_PROTO;
    }

    mfile = confMan->getValue("MappingFile", "EXPORTER", "ipfix");
    if (mfile.empty()) {
        mfile = DEF_MFILE;
    }

    // read ipfix attribute map
    readMap();

    // initalize connections
    for (int i=0; i<MAX_CONN; i++) {
        conns[i].ref = 0;
    }

    // open default connection
    openConn(0, chost, port);

    return 0;
}


/* -------------------- resetModule -------------------- */

int resetModule()
{
    closeAllConn();
    
    // open default connection
    openConn(0, conns[0].chost, conns[0].port);

    return 0;
}


/* -------------------- destroyModule -------------------- */

int destroyModule()
{
    closeAllConn();

    attrMap.clear();

    return 0;
}


/* -------------------- initExportRec -------------------- */

int initExportRec( configItemList_t conf, void **expRecord )
{
    string chost;
    int port;

    // mark that expRecord is first use
    exportRecord_t *rec = new exportRecord_t;

    rec->keyTmpl = NULL;
    rec->conn = -1;

    chost = conf.getValue("Collector");
    if (chost.empty()) {
        chost = conns[0].chost;
    }

    string portstr = conf.getValue("Port");
    if (portstr.empty()) {
        port = conns[0].port;
    } else {
        port = atoi(portstr.c_str());
    }

#ifdef DEBUG
    cerr << "Trying " << chost << ":" << port << endl;
#endif

    // find and use existing connection
    for (int i=0; i<MAX_CONN; i++) {
        if ((conns[i].ref > 0) && (chost == conns[i].chost) && (port == conns[i].port)) {
            conns[i].ref++;
            rec->conn = i;
#ifdef DEBUG
            cerr << "ipfix: using connection: " << i << endl;
#endif

            break;
        }
    }

    if (rec->conn < 0) {
        // try open new connection
        for (int i=0; i<MAX_CONN; i++) {
            if (conns[i].ref == 0) {
                openConn(i, chost, port);
                rec->conn = i;
#ifdef DEBUG
                cerr << "ipfix: opening connection: " << i << endl;
#endif

                break;
            }
        }
    }

    if (rec->conn < 0) {
        die(1, "can not open any more connections");
    }

    (exportRecord_t *) (*expRecord) = rec;
    
    return 0;
}


/* -------------------- destroyExportRec -------------------- */

int destroyExportRec( void *expRecord )
{
    exportRecord_t *rec = (exportRecord_t *)expRecord;

    ipfix_release_template(rec->keyTmpl);
    for (templListIter_t i = rec->metricTmplList.begin(); i != rec->metricTmplList.end(); i++) {
        ipfix_release_template(i->second);
    }
    
    // decrement ref counter for connection
    conns[rec->conn].ref--;
    if (conns[rec->conn].ref == 0) {
#ifdef DEBUG
        cerr << "ipfix: Destroying connection: " << rec->conn << endl;
#endif

        // close
        closeConn(rec->conn);
    }
    
    delete (exportRecord_t *) expRecord;
    
    return 0;
}


/* -------------------- timeout -------------------- */

int timeout( int id )
{
    // called by netmate if module has timers defined - in text_file module unused!
    return 0;
}


/* ------------------- exportMetricData (local function) ------------------- */

static int exportMetricData(exportRecord_t *rec, string taskName, string metricName, 
                            MetricData *mdata)
{
    int nrows;
    DataType_e type;
    const char *exp_fields[64];
    int types[64];
    int lens[64];
    int task_id;
    const char *str;
    unsigned int c, list;
    int newFlow;
    unsigned long long flowId;

    mdata->initExport();

    if (rec->keyTmpl == NULL) {
        // create template for flow key          

        // first flow id and task id
        types[0] = IPFIX_FT_FLOWID;
        lens[0] = 4;

        types[1] = IPFIX_FT_TASKID;
        lens[1] = 4;

        c = 2;
        while ((str = mdata->getNextFlowKeyLabel(&type)) != NULL) {
            attrMapIter_t a = attrMap.find(str);
            if (a != attrMap.end()) {
                types[c] = a->second;
                lens[c] = DataTypeSize[type]; 
            } else {
                die(1, "missing mapping for attribute '%s'", str);
            }

            c++;
        }

        if (ipfix_get_template_array(conns[rec->conn].ipfixh, &rec->keyTmpl, c, types, lens) < 0) {
            die(1, "ipfix_get_template_array() failed to create flow key template: %s", 
                strerror(errno));
        }
    }

    list = 0;
    while ((nrows = mdata->getNextList()) > 0) {
        char buf[6];
        ipfix_template_t *templ = NULL;

        // find metric template for the metric and list
        sprintf(buf, "%d", list);
        string tmplID = metricName+string(buf);

        templListIter_t t = rec->metricTmplList.find(tmplID);
        if (t == rec->metricTmplList.end()) {
            // create template for metric and list           

            types[0] = IPFIX_FT_FLOWID;
            lens[0] = 4;

            c = 1;
            while ((str = mdata->getNextFlowDataLabel(&type)) != NULL) {
                attrMapIter_t a = attrMap.find(str);
                if (a != attrMap.end()) {
                    types[c] = a->second;
                    lens[c] = DataTypeSize[type];
                } else {
                    die(1, "ipfix: missing mapping for attribute '%s'", str);
                }  
                
                c++;
            }

            if (ipfix_get_template_array(conns[rec->conn].ipfixh, &templ, c, types, lens) < 0) {
                die(1, "ipfix_get_template_array() failed to create metric template: %s", 
                    strerror(errno));
            }
            pair<templListIter_t, bool> res = rec->metricTmplList.insert(make_pair(tmplID, templ));
        } else {
            templ = t->second;
        }

        while ((nrows = mdata->getNextFlow(&flowId, &newFlow)) > -1) {
            if (newFlow && (nrows > 0)) {
                // export flow key
                
                // first flow id
	        unsigned long truncFlowId = (unsigned long) flowId;
                exp_fields[0] = (char *) &truncFlowId;
                // second task id
                // FIXME: change to string type
                task_id = atoi(taskName.c_str());
                exp_fields[1] = (char *) &task_id;
                
                // finally all flow key values
                c = 2;
                while((str = mdata->getNextFlowKey(&type)) != NULL) {
                    if (c >= sizeof(exp_fields)/sizeof(exp_fields[0])) {
                        die(1, "only %d fields supported per IPFIX record", 
                            sizeof(exp_fields)/sizeof(exp_fields[0]));
                    }
                    exp_fields[c] = str;
                    c++;
                }
                
                if (ipfix_export_array(conns[rec->conn].ipfixh, rec->keyTmpl, c, (void**) &exp_fields) < 0) {
                    die(1, "ipfix_export() flow key failed: %s", strerror(errno));
                }
            }
            
            for (int i=0; i<nrows; i++) {
                // export flow data

                // first the flow number
                exp_fields[0] = (char *) &flowId;

                c = 1;
                while ((str = mdata->getNextFlowDataRow(&type)) != NULL) {
                    if (c >= sizeof(exp_fields)/sizeof(exp_fields[0])) {
                        die(1, "only %d fields supported per IPFIX record", 
                            sizeof(exp_fields)/sizeof(exp_fields[0]));
                    }
                    exp_fields[c] = str; 

                    c++;
                }

                if (ipfix_export_array(conns[rec->conn].ipfixh, templ, c, (void**) &exp_fields) < 0) {
                    die(1, "ipfix_export() metric data failed: %s", strerror(errno));
                }
            }
        }

        list++;
    }

    return 0;
}


/* -------------------- exportData (part of export API) -------------------- */

int exportData( FlowRecord *frec, void *expData ) 
{
    exportRecord_t *rec;
    MetricData *mdata;
    int result = 0;

#ifdef PROFILING
    unsigned long long ini = PerfTimer::readTSC();
#endif

    rec = (exportRecord_t *)expData;

#ifdef DEBUG
    cerr << "ipfix::exportData (rule='" << frec->getRuleName() << "')" << endl;
#endif

    // data from multiple packet proc modules can be in one FlowRecord
    while ((mdata = frec->getNextData()) != NULL) {
#ifdef DEBUG
        cerr << "export from proc module: " << mdata->getModName() << endl;
#endif
        result += exportMetricData(rec, frec->getRuleName(), mdata->getModName(), mdata);
    }

    ipfix_export_flush(conns[rec->conn].ipfixh);
    
#ifdef PROFILING
    {
        unsigned long long end = PerfTimer::readTSC();
        cerr << "ipfix::exportData took "
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

