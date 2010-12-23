
/*! \file  netai_socket.cc

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
    export flow information to UDP or TCP socket in ASCII comma separated values format
    first version does only allow to one specific export host and export port 
    (and NOT per rule defined export host and port) 

    $Id: netai_socket.cc 22 2006-01-24 06:13:15Z szander $

*/

#include <sys/types.h>
#include <sys/socket.h>
extern "C" {
#include <netdb.h>
}
#include "stdincpp.h"
#include "ConfigManager.h"
#include "./ExportModule.h"
#include "Rule.h"
#include "ConfigParser.h"
#include "../netmate/ProcModule.h"


// module global variables (common for all rules using the ac_udp export module) 

static string g_exportHost;
static string g_exportPort;
static string g_exportProto;
// this prevents us from exporting different rules to different export hosts
static int g_fd; 

// FIXME: need the following per rule if different export targets shall work

// buffer to be send over network
string netBuffer;
// last time data was exported
struct timeval lastExport;


// task specific variables (one such record is stored by netmate per task
//                          using the ac_file export module)

typedef struct {
    short firstTime;  // flag: ==1 if this is the first export for a task
    short multifile;
    int expFlowId;
    int expFlowStatus;
    string expHost;
    string expPort;
    string expProto;
    int sock;
} exportRecord_t;


// FIXME: how to throw exceptions from inside the shared lib?
void die(int code, char *fmt, ...)
{
    va_list           args;

    fprintf(stderr, "netai_socket: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(code);
}


void warn(int code, char *fmt, ...)
{
    va_list           args;

    fprintf(stderr, "netai_socket: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}


int initNetwork(string expHost, string expPort, string expProto, int fail)
{
  int one = 1;
  int fd = -1;
  struct addrinfo *expAddr = NULL;

  if (!expHost.empty() && !expPort.empty()) {
    // resolve hostname and set remote address
    struct addrinfo ask;
    
      memset(&ask, 0, sizeof(ask));
      ask.ai_socktype = SOCK_DGRAM;
      ask.ai_flags = 0;
      
      if (getaddrinfo(expHost.c_str(), expPort.c_str(), &ask, &expAddr) != 0) {
	if (fail) {
	  die(500, "getaddrinfo(): %s", strerror(errno));
	} else {
	  return 0;
	}
      }
      
  } else {
    if (fail) {
      die(500, "no target host and/or port specified");
    } else {
      return 0;
    }
  }

    
#ifdef DEBUG
  cout << "export to " << expHost << ":" << expPort << endl;
#endif
    
  
  // open local socket
  
  if ((expProto == "UDP") || (expProto == "udp")) {
    fd = socket(AF_INET, SOCK_DGRAM, 0);
  } else if ((expProto == "TCP") || (expProto == "tcp")) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
  } else {
    if (fail) {
      die(500, "unknown protocol %s", expProto.c_str());
    } else {
      return 0;
    }
  }

  if (fd == -1) {
    if (fail) {
      die(500, "socket(): %s", strerror(errno));
    } else {
      return 0;
    }
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
    if (fail) {
      die(500, "setsockopt(): %s", strerror(errno));
    } else {
      return 0;
    }
  }

  struct sockaddr_in addr;
  
  addr.sin_addr.s_addr = INADDR_ANY;
  //addr.sin_port = htons(atoi(expPort.c_str()));
  addr.sin_port = 0;
  addr.sin_family = AF_INET;
  memset(&(addr.sin_zero), '\0', 8);
  
  if (bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1) {
    if (fail) {
      die(500, "bind(): %s", strerror(errno));
    } else {
      return 0;
    }
  }

  if (connect(fd, (struct sockaddr *) expAddr->ai_addr, expAddr->ai_addrlen) == -1) {
    if (fail) {
      die(500, "connect(): %s", strerror(errno));
    } else {
      return 0;
    }
  }
  
  if (expAddr != NULL) {
    freeaddrinfo(expAddr);
  }

  return fd;
}

/* -------------------- initModule -------------------- */

int initModule( ConfigManager *confMan )
{
  
    g_exportHost = confMan->getValue("ExportHost", "EXPORTER", "netai_socket");
    g_exportPort = confMan->getValue("ExportPort", "EXPORTER", "netai_socket");
    g_exportProto = confMan->getValue("ExportProtocol", "EXPORTER", "netai_socket");

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
    rec->expFlowId       = (conf.getValue("FlowID") == "yes") ? 1 : 0;
    rec->expFlowStatus   = (conf.getValue("ExportStatus") == "yes") ? 1 : 0; 
    rec->expHost         = conf.getValue("ExportHost");
    rec->expPort         = conf.getValue("ExportPort");
    rec->expProto        = conf.getValue("ExportProtocol");

    if (rec->expHost.empty()) {
      rec->expHost = g_exportHost;
    }

    if (rec->expPort.empty()) {
      rec->expPort = g_exportPort;
    }

     if (rec->expProto.empty()) {
      rec->expProto = g_exportProto;
    }
    
    rec->sock = initNetwork(rec->expHost, rec->expPort, rec->expProto, 1); 
    g_fd = rec->sock;

    *((exportRecord_t **) expRecord) = rec;

    return 0;
}


/* -------------------- destroyExportRec -------------------- */

int destroyExportRec( void *expRecord )
{
    exportRecord_t *rec = (exportRecord_t *)expRecord;

    if (netBuffer.size() > 0) {
      // export whats left in the buffer

      if (send(rec->sock, (void *) netBuffer.c_str(), netBuffer.size(), 0) == -1) {
        warn(500, "send(): %s", strerror(errno));
      }
      netBuffer = "";
    }

    close(rec->sock);

    delete rec;

    return 0;
}


/* -------------------- writeData (local function) -------------------- */

inline static void writeData( ostringstream &str, DataType_e type, const char *dpos )
{
    //fprintf(stderr, "%s %x \n", ProcModule::typeLabel(type), dpos);

    switch (type) {
    case CHAR: 
        str << *dpos;
        break;
    case INT8:
        str << (short)*dpos; 
        break;
    case UINT8:	
        str << (unsigned short)*((unsigned char*)dpos); 
        break;
    case INT16:
        str << *((short*)dpos); 
        break;
    case UINT16:
        str << *((unsigned short*)dpos); 
        break;
    case INT32:
        str << *((int32_t*)dpos); 
        break;
    case UINT32:
        str << *((uint32_t*)dpos); 
        break;
    case IPV4ADDR:
      {
          char buf[64];
          str << inet_ntop(AF_INET, dpos, buf, sizeof(buf));
      }
      break;
    case IPV6ADDR:
      {
          char buf[64];
          str << inet_ntop(AF_INET6, dpos, buf, sizeof(buf));
      }
      break;
    case INT64:
        str << *((int64_t*)dpos);
        break;
    case UINT64:
        str << *((uint64_t*)dpos); 
        break;
    case FLOAT:
        str << *((float*)dpos); 
        break;
    case DOUBLE:
        str << *((double*)dpos); 
        break;
    case STRING:   
        str << dpos; 
        break;
    case LIST:
        break;
    case BINARY:
      {
	  // get length
          unsigned int len = *((unsigned int *)dpos);
          dpos += sizeof(unsigned int);

          if (len > 0) {
            str << "0x" << hex << setfill('0');

            // print binary as hex bytes
            for (unsigned int i=0; i < len; i++) {
                  str << setw(2) << (short) *((unsigned char *)dpos);
                  dpos += 1;
            }
            str << dec;
          }

      }
      break;
    default:
        break;
    }
}


/* ------------------- exportMetricData (local function) ------------------- */

static int exportMetricData( string taskName, MetricData *mdata,
                             int fd, int expFlowId, int final, int expFlowStatus, int sock )
{
    int nrows;
    DataType_e type;
    char tstamp[64];
    struct timeval tval;
    struct tm tm;
    const char *str;
    int newFlow;
    unsigned long long flowId;

    gettimeofday(&tval,NULL);
    strftime(tstamp, sizeof(tstamp), "%b %d %H:%M:%S", localtime_r((time_t *)&tval.tv_sec, &tm));
 
    mdata->initExport();

    while ((nrows = mdata->getNextList()) > -1) {
        while ((nrows = mdata->getNextFlow(&flowId, &newFlow)) > -1) {
            for (int i=0; i<nrows; i++) {
	        char c = '\0';
		ostringstream ostr;

		if (expFlowId) {
		  ostr << flowId << ",";
		}
		if (expFlowStatus) {
		  ostr << final << ",";
		}
                
                // print flow key
                while((str = mdata->getNextFlowKey(&type)) != NULL) {
		  if (c != '\0') {
		    ostr << c;
		  }
		  c = ',';
		  writeData(ostr, type, str);
                }
                
                // print flow data
                while((str = mdata->getNextFlowDataRow(&type)) != NULL) {
		  if (c != '\0') {
                    ostr << c;
		  }
		  c = ',';
		  writeData(ostr, type, str);
                }
                ostr << endl;

		// hand over to UDP socket
		if ((netBuffer.size() + ostr.tellp()) > 1470) {
		  if (send(sock, (void *) netBuffer.c_str(), netBuffer.size(), 0) == -1) {
		    warn(500, "send(): %s", strerror(errno));
		  }
		  
		  netBuffer = ostr.str();
		} else {
		  netBuffer += ostr.str();
		}
            }
        }
    }

    lastExport = tval;
    
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

#ifdef DEBUG
    cout << "netai_socket:exportData (rule='" << frec->getRuleName() << "')" << endl;
#endif

    rec = (exportRecord_t *)expData;


#ifdef DEBUG
    cout << "Export to file: " << filename << endl;
#endif

    if (!rec->multifile) {

        // if first time here for the exported task then
        // it is possible to run some special function
        if (rec->firstTime == 1) {
            // done only at first export for this task
        }
     
        // data from multiple packet proc modules can be in one FlowRecord
        while ((mdata = frec->getNextData()) != NULL) {
#ifdef DEBUG
            cout << "export from proc module: " << mdata->getModName() << endl;
#endif
            result += exportMetricData(frec->getRuleName(), mdata, rec->sock, rec->expFlowId, 
				       frec->isFinal(), rec->expFlowStatus, rec->sock);
        }
        
    
    } else {  // multifile == 1

        // data from multiple packet proc modules can be in one FlowRecord
        while ((mdata = frec->getNextData()) != NULL) {

	    
            // if first time here for the exported task then
            // it is possible to run some special function
            if (rec->firstTime == 1) {
                // done only at first export for this task
            }
         

#ifdef DEBUG	    
            cout << "export from proc module: " << mdata->getModName() << endl;
#endif
            result += exportMetricData(frec->getRuleName(), mdata, rec->sock, rec->expFlowId, 
				       frec->isFinal(), rec->expFlowStatus, rec->sock);
           
        }
    }

    rec->firstTime = 0;

#ifdef PROFILING
    {
        unsigned long long end = PerfTimer::readTSC();
        cout << "netai_socket::exportData took "
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


timers_t timers[] = { /* handle, ival_msec, flags */
    { 1, 1000, TM_RECURRING },
    TIMER_END
};


/* -------------------- timeout -------------------- */

int timeout( int id )
{
    struct timeval now;

    gettimeofday(&now, NULL);

    if ((netBuffer.size() > 0) && 
	(((double) (now.tv_sec*1e6+now.tv_usec) - (lastExport.tv_sec*1e6 + lastExport.tv_usec)) >= 1e6)) {
      // if at least 1 second has passed export whats in the buffer

      // FIXME use global fd as a bad hack, actually this will prevent having
      // different export targets -> should be a way of passing data pointer
      // with timeout function
      if (send(g_fd, (void *) netBuffer.c_str(), netBuffer.size(), 0) == -1) {
	warn(500, "send(): %s", strerror(errno));
      }
      netBuffer = "";
      lastExport = now;
    }

    return 0;
}



timers_t* getTimers()
{
    return timers;
}

