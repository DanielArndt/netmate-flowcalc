
/*! \file   Meter.h

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
    the main class 

    $Id: Meter.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _METER_H_
#define _METER_H_


#include "stdincpp.h"
#include "Error.h"
#include "Logger.h"
#include "CommandLineArgs.h"
#include "ConfigManager.h"
#include "RuleManager.h"
#include "EventScheduler.h"
#include "Exporter.h"
#include "PacketProcessor.h"
#include "CtrlComm.h"
#include "MeterInfo.h"
#include "Sampler.h"
#include "SamplerAll.h"
#include "constants.h"

// classifier
#ifdef ENABLE_NF
#ifdef HAVE_LIBIPULOG_LIBIPULOG_H
#include "Classifier_netfilter.h"	
#endif
#else
#include "NetTapPcap.h"
#ifdef HAVE_ERF
#include "NetTapERF.h"
#endif
#include "ClassifierSimple.h"
#include "ClassifierRFC.h"
#endif

/*! \short   brief Meter class description
  
    detailed Meter class description
*/

class Meter
{
  public:

    // FIXME document!
    static int s_sigpipe[2];
 
  private:
    
    //!< start time of the Meter
    time_t startTime;

    //! config file name
    string configFileName;

    //! log file name
    string logFileName;

    auto_ptr<Logger>          log;
    auto_ptr<CommandLineArgs> args;
    auto_ptr<PerfTimer>       perf;
    auto_ptr<ConfigManager>   conf;
    auto_ptr<RuleManager>     rulm;
    auto_ptr<EventScheduler>  evnt;

    // declaration order is important as the autoptrs will be destroyed
    // in reverse order
    auto_ptr<PacketProcessor> proc;    
    auto_ptr<Exporter>        expt;
    auto_ptr<Sampler>         samp;
    auto_ptr<Classifier>      clss;
    auto_ptr<CtrlComm>        comm;

    //! logging channel number used by objects of this class
    int ch;

     // FD list (from MeterComponent.h)
    fdList_t fdList;

    // 1 if packet proc runs in a separate thread
    int pprocThread;

    // 1 if classifier runs in a separate thread
    int classThread;

    // 1 if exporter runs in a separate thread
    int expThread;

    // 1 if online packet capture
    static int onlineCap;

    // 1 if remote control interface is enabled
    static int enableCtrl;

    // list of capture file names
    list<string> capFiles;

    // signal handlers
    static void sigint_handler(int i);
    static void sigusr1_handler(int i);
    static void sigalarm_handler(int i);

    // exit function called on exit
    static void exit_fct(void);

    //! return 1 if a meter is already running on the host
    int alreadyRunning();

    //! get info string for get_info response
    string getInfo(infoType_t what, string param);

    string getHelloMsg();

    string getMeterInfo(infoList_t *i);

  public:

    /*! \short   construct and initialize a Meter object

        detailed constructor description for Meter

        \arg \c argc - number of command line arguments
        \arg \c argv - list of command line arguments
    */
    Meter(int argc, char *argv[]);


    /*! \short   destroy a Meter object
        detailed destructor description for Meter
    */
    ~Meter();

    void run();

    //! dump a Meter object
    void dump( ostream &os );

    //! handle the events
    void handleEvent(Event *e, fd_sets_t *fds);

};


//! overload for <<, so that a Meter object can be thrown into an ostream
ostream& operator<< ( ostream &os, Meter &obj );


#endif // _METER_H_
