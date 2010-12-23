
/*! \file   Meter.cc

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

    $Id: Meter.cc 748 2009-09-10 02:54:03Z szander $
*/


#include "Meter.h"
#include "ParserFcts.h"


// globals in Meter class
int Meter::s_sigpipe[2];
int Meter::onlineCap = 1;
int Meter::enableCtrl = 0;

// global timeout flag
int g_timeout = 0;


/*
  version string embedded into executable file
  can be found via 'strings <path>/netmate | grep version'
*/
const char *NETMATE_VERSION = "NetMate version " VERSION ", (c) 2003-2004 Fraunhofer Institute FOKUS, Germany";

const char *NETMATE_OPTIONS = "compile options: "
"multi-threading support = "
#ifdef ENABLE_THREADS
"[YES]"
#else
"[NO]"
#endif
", secure sockets (SSL) support = "
#ifdef USE_SSL
"[YES]"
#else
"[NO]"
#endif
" ";


// remove newline from end of C string
static inline char *noNewline( char* s )
{
    char *p = strchr( s, '\n' );
    if (p != NULL) {
        *p = '\0';
    }
    return s;
}


/* ------------------------- Meter ------------------------- */

Meter::Meter( int argc, char *argv[])
    :  pprocThread(0), classThread(0), expThread(0)
{

    // record meter start time for later output
  startTime = ::time(NULL);

    try {

        if (pipe((int *)&s_sigpipe) < 0) {
            throw Error("failed to create signal pipe");
        }

        fdList[make_fd(s_sigpipe[0], FD_RD)] = NULL;

        // the read fd must not block
        fcntl(s_sigpipe[0], F_SETFL, O_NONBLOCK);
	
        // install signal handlers
        signal(SIGINT, sigint_handler);
        signal(SIGTERM, sigint_handler);
        signal(SIGUSR1, sigusr1_handler);
        signal(SIGALRM, sigalarm_handler);
        // FIXME sighup for log file rotation

        auto_ptr<CommandLineArgs> _args(new CommandLineArgs());
        args = _args;

        // basic args
        args->add('c', "ConfigFile", "<file>", "use alternative config file",
                  "MAIN", "configfile");
        args->add('l', "LogFile", "<file>", "use alternative log file",
                  "MAIN", "logfile");
        args->add('r', "RuleFile", "<file>", "use specified rule file",
                  "MAIN", "rulefile");
        args->addFlag('V', "RelVersion", "show version info and exit",
                      "MAIN", "version");
        args->add('D', "FilterDefFile", "<file>", "use alternative file for filter definitions",
                  "MAIN", "fdeffile");
        args->add('C', "FilterConstFile", "<file>", "use alternative file for filter constants",
                  "MAIN", "fcontfile");
#ifdef USE_SSL
        args->addFlag('x', "UseSSL", "use SSL for control communication",
                      "CONTROL", "usessl");
#endif
        args->add('P', "ControlPort", "<portnumber>", "use alternative control port",
                  "CONTROL", "cport");
#ifndef ENABLE_NF
#ifndef HAVE_LIBIPULOG_LIBIPULOG_H
        args->add('i', "NetInterface", "<iface>[,<iface2>,...]", "select network interface(s)"
                  " to capture from", "MAIN", "interface");
        args->add('f', "CaptureFile", "<file>[,<file2>,...]", "use capture file(s) to read"
                  " packets from", "MAIN", "tracefile");
        args->add('s', "SnapSize", "<size>", "specify packet snap size",
                  "CLASSIFIER", "snapsize");
        args->addFlag('p', "NoPromiscInt", "don't put interface in "
                      "promiscuous mode", "MAIN", "nopromisc");
#endif
#endif
#ifdef HAVE_ERF
	args->addFlag('L', "ERFLegacy", "read legacy ERF format", "MAIN", "erflegacy");
#endif

        // get command line arguments from components via static method
        ClassifierSimple::add_args(args.get());

        // parse command line args
        if (args->parseArgs(argc, argv)) {
            // user wanted help
            exit(0);
        }

        if (args->getArgValue('V') == "yes") {
            cout << getHelloMsg();
            exit(0);
        }

        // test before registering the exit function
        if (alreadyRunning()) {
            throw Error("already running on this machine - terminating" );
        }

        // register exit function
        atexit(exit_fct);

        auto_ptr<Logger> _log(Logger::getInstance()); 	
        log = _log;
        ch = log->createChannel("Meter");

        log->log(ch,"Initializing netmate system");
        log->log(ch,"Program executable = %s", argv[0]);
        log->log(ch,"Started at %s", noNewline(ctime(&startTime)));

        // parse config file
        configFileName = args->getArgValue('c');
        if (configFileName.empty()) { 
            // is no config file is given then use the default
            // file located in a relative location to the binary
            configFileName = DEFAULT_CONFIG_FILE;
        }

        auto_ptr<ConfigManager> _conf(new ConfigManager(configFileName, argv[0]));
        conf = _conf;

        // merge into config
        conf->mergeArgs(args->getArgList(), args->getArgVals());

        // dont need this anymore
        CommandLineArgs *a = args.release();
        saveDelete(a);

        // use logfilename (in order of precedence):
        // from command line / from config file / hardcoded default

        // query command line for log file name
        logFileName = conf->getValue("LogFile", "MAIN");

        if (logFileName.empty()) {
            logFileName = DEFAULT_LOG_FILE;
        }

        log->setDefaultLogfile(logFileName);

        cout << "netmate logfile is: " << logFileName << endl;

        // set logging vebosity level if configured
        string verbosity = conf->getValue("VerboseLevel", "MAIN");
        if (!verbosity.empty()) {
            log->setLogLevel( ParserFcts::parseInt( verbosity, -1, 4 ) );
        }

        log->log(ch,"configfilename used is: '%s'", configFileName.c_str());
#ifdef DEBUG
        log->dlog(ch,"------- startup -------" );
#endif

        // startup other core classes
        auto_ptr<PerfTimer> _perf(PerfTimer::getInstance());
        perf = _perf;
        auto_ptr<RuleManager> _rulm(new RuleManager(conf->getValue("FilterDefFile", "MAIN"),
                                                    conf->getValue("FilterConstFile", "MAIN")));
        rulm = _rulm;
        auto_ptr<EventScheduler> _evnt(new EventScheduler());
        evnt = _evnt;

        // startup meter components

#ifdef ENABLE_THREADS
        auto_ptr<Exporter> _expt(new Exporter(conf.get(), 
                                              conf->isTrue("Thread",
                                                           "EXPORTER")));
        expThread = conf->isTrue("Thread", "EXPORTER");

        auto_ptr<PacketProcessor> _proc(new PacketProcessor(conf.get(),
							    conf->isTrue("Thread",
									 "PKTPROCESSOR")));
        pprocThread = conf->isTrue("Thread", "PKTPROCESSOR");
#else
        auto_ptr<Exporter> _expt(new Exporter(conf.get(), 0));
        auto_ptr<PacketProcessor> _proc(new PacketProcessor(conf.get(), 0));
        pprocThread = 0;

        if (conf->isTrue("Thread", "EXPORTER") || conf->isTrue("Thread", "PKTPROCESSOR") ||
            conf->isTrue("Thread", "CLASSIFIER")) {
            log->wlog(ch, "Threads enabled in config file but executable is compiled without thread support");
        }
#endif

        expt = _expt;
        proc = _proc;
        expt->mergeFDs(&fdList);
        proc->mergeFDs(&fdList);

	// give packet processor handle to Exporter
	proc->setExporter(expt.get());
	
        // initialize sampler
        string smpl = conf->getValue("Sampling", "CLASSIFIER");

        if ((smpl == "All") || (smpl.empty())) {
            auto_ptr<Sampler> _samp(new SamplerAll());
            samp = _samp;
        } else {
            throw Error("Unknown sampling algorithm '%s' specified", smpl.c_str());
        }

#ifdef ENABLE_NF
#ifdef HAVE_LIBIPULOG_LIBIPULOG_H
        clss = auto_ptr<Classifier>(new ClassifierNetfilter(conf.get(), 0));
#endif
#else
        // capture file overules device setting
        string cp = conf->getValue("CaptureFile", "MAIN");
        string ni = cp.empty() ? conf->getValue("NetInterface", "MAIN") : cp;
        string sz = conf->getValue("SnapSize", "CLASSIFIER");
        onlineCap = cp.empty() ? 1 : 0;
	Timeval::setCapMode(onlineCap);
        int snapsize = sz.empty() ? DEF_SNAPSIZE : atoi(sz.c_str()); 
        int bsize = conf->getValue("RcvBufSize", "CLASSIFIER").empty() ? 65535 : 
          atoi(conf->getValue("RcvBufSize", "CLASSIFIER").c_str());
        string cl = conf->getValue("Algorithm", "CLASSIFIER");
	string tt = conf->getValue("TapType", "CLASSIFIER");

#ifdef ENABLE_THREADS
        classThread = conf->isTrue("Thread", "CLASSIFIER");
#endif

        if ((cl == "Simple") || (cl.empty())) {
            auto_ptr<Classifier> _clss(new ClassifierSimple(conf.get(),
                                                            samp.get(),
                                                            proc->getQueue(),
                                                             classThread));
            clss = _clss;
        } else if (cl == "RFC") {
            auto_ptr<Classifier> _clss(new ClassifierRFC(conf.get(),
                                                          samp.get(),
                                                          proc->getQueue(),
                                                          classThread));
            clss = _clss;
        } else {
            throw Error("Unknown classifier '%s' specified", cl.c_str());
        }
#endif

        int p1 = 0, p2 = 0, last = 1, first = 1;
        while(((p2 = ni.find(",",p1)) > 0) || last) {
            NetTap *nett;

            if (p2 <= 0) {
                p2 = ni.length();
                last = 0;
            }

            // FIXME remove leading/trailing spaces
            string dev = ni.substr(p1,p2-p1);

	    if (onlineCap || first) {
	      
	      if (tt == "erf") {
#ifdef HAVE_ERF
		if (conf->isTrue("ERFLegacy", "MAIN")) {
		  nett = new NetTapERF(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"),
				       snapsize, 1, bsize, 1);
		} else {
		  nett = new NetTapERF(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"), 
				       snapsize, 1, bsize); 
		}
#else
		throw Error("No support for ERF");
#endif
	      } else {
		/* pcap by default */
#ifdef ENABLE_THREADS
		nett = new NetTapPcap(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"), 
				      snapsize, !conf->isTrue("Thread", "CLASSIFIER"), bsize);      
#else
		nett = new NetTapPcap(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"), 
				      snapsize, 1, bsize);
#endif
	      } 

	      clss->registerTap(nett);
	      first = 0;
	    } else {
	      // store remaining capture file names
	      capFiles.push_back(dev);
	    }
        
            p1 = p2+1;
        } 


        clss->mergeFDs(&fdList);

	// setup initial rules
	string rfn = conf->getValue("RuleFile", "MAIN");
        if (!rfn.empty()) {
	    if (!onlineCap) {
	    /*
	        handle loading of rules from rule file 
		_immediately_ (synchronous call) when 
		reading from packet capture files since
		else we are missing packets at the start
		because the net tap delivers packets before
		we have installed the rules
	    */
	      AddRulesEvent e = AddRulesEvent(rfn);
	      handleEvent(&e, NULL);
	    } else {
		// for live packet capture just schedule rule addition
		evnt->addEvent(new AddRulesEvent(rfn));
	    }
        }

        // disable logger threading if not needed
        if (!classThread && !pprocThread && !expThread) {
            log->setThreaded(0);
        }

	enableCtrl = conf->isTrue("Enable", "CONTROL");
	if (!onlineCap) {
	  // always disable
	  enableCtrl = 0;
	}

	if (enableCtrl) {
	  // ctrlcomm can never be a separate thread
	  auto_ptr<CtrlComm> _comm(new CtrlComm(conf.get(), 0));
	  comm = _comm;
	  comm->mergeFDs(&fdList);
	}

    } catch (Error &e) {
        if (log.get()) {
            log->elog(ch, e);
        }  
        throw e;
    }
}


/* ------------------------- ~Meter ------------------------- */

Meter::~Meter()
{
    // objects are destroyed by their auto ptrs
}


/* -------------------- getHelloMsg -------------------- */

string Meter::getHelloMsg()
{
    ostringstream s;
    
    static char name[128] = "\0";

    if (name[0] == '\0') { // first time
        gethostname(name, sizeof(name));
    }

    s << "netmate build " << BUILD_TIME 
      << ", running at host \"" << name << "\"," << endl
      << "compile options: "
#ifndef ENABLE_THREADS
      << "_no_ "
#endif
      << "multi-threading support, "
#ifndef USE_SSL
      << "_no_ "
#endif
      << "secure sockets (SSL) support"
      << endl;
    
    return s.str();
}


/* -------------------- getInfo -------------------- */

string Meter::getInfo(infoType_t what, string param)
{  
    time_t uptime;
    ostringstream s;
    
    s << "<info name=\"" << MeterInfo::getInfoString(what) << "\" >";

    switch (what) {
    case I_METER_VERSION:
        s << getHelloMsg();
        break;
    case I_UPTIME:
      uptime = ::time(NULL) - startTime;
        s << uptime << " s, since " << noNewline(ctime(&startTime));
        break;
    case I_TASKS_STORED:
        s << rulm->getNumTasks();
        break;
    case I_FUNCTIONS_LOADED:
        s << proc->numModules();
        break;
    case I_CONFIGFILE:
        s << configFileName;
        break;
    case I_USE_SSL:
        s << (httpd_uses_ssl() ? "yes" : "no");
        break;
    case I_HELLO:
        s << getHelloMsg();
        break;
    case I_TASKLIST:
        s << CtrlComm::xmlQuote(rulm->getInfo());
        break;
    case I_MODLIST:
        s << proc->getInfo();
        break;
    case I_TASK:
        if (param.empty()) {
            throw Error("get_info: missing parameter for rule = <rulename>" );
        } else {
            int n = param.find(".");
            if (n > 0) {
                s << CtrlComm::xmlQuote(rulm->getInfo(param.substr(0,n), param.substr(n+1, param.length())));
            } else {
                s << CtrlComm::xmlQuote(rulm->getInfo(param));
            }
        }
        break;
    case I_NUMMETERINFOS:
    default:
        return string();
    }

    s << "</info>" << endl;
    
    return s.str();
}


string Meter::getMeterInfo(infoList_t *i)
{
    ostringstream s;
    infoListIter_t iter;
   
    s << "<meterinfos>\n";

    for (iter = i->begin(); iter != i->end(); iter++) {
        s << getInfo(iter->type, iter->param);
    }

    s << "</meterinfos>\n";

    return s.str();
}


/* -------------------- handleEvent -------------------- */

void Meter::handleEvent(Event *e, fd_sets_t *fds)
{
   
    switch (e->getType()) {
    case TEST:
      {

      }
      break;
    case GET_INFO:
      {
          // get info types from event
          try {
              infoList_t *i = ((GetInfoEvent *)e)->getInfos(); 
              // send meter info
              comm->sendMsg(getMeterInfo(i), ((GetInfoEvent *)e)->getReq(), fds, 0 /* do not html quote */ );
          } catch(Error &err) {
              comm->sendErrMsg(err.getError(), ((GetInfoEvent *)e)->getReq(), fds);
          }
      }
      break;
    case GET_MODINFO:
      {
          // get module information from loaded module (proc mods only now)
          try {
              string s = proc->getModuleInfoXML(((GetModInfoEvent *)e)->getModName());
              // send module info
              comm->sendMsg(s, ((GetModInfoEvent *)e)->getReq(), fds, 0);
          } catch(Error &err) {
              comm->sendErrMsg(err.getError(), ((GetModInfoEvent *)e)->getReq(), fds);
          }
      }
      break;
    case PUSH_EXPORT:
      {
          ruleDB_t     *rules = ((PushExportEvent *)e)->getRules();

          // multiple rules can export at the same time
          for (ruleDBIter_t iter = rules->begin(); iter != rules->end(); iter++) {

	      FlowRecord *rec;

              // retrieve flow data via evaluation module(s) for that rule
              rec = proc->exportRule((*iter)->getUId(), (*iter)->getRuleName());

	      // set final status
	      rec->setFinal(((PushExportEvent *)e)->isFinal());

              // schedule this data for export via export module(s) for that rule
              expt->storeData((*iter)->getUId(), ((PushExportEvent *)e)->getExpMods(), rec);   
          }
      }
      break;
    case PULL_EXPORT:
      {
          // not implemented yet
      }
      break;
    case FLOW_TIMEOUT:
      {
          int rid = ((FlowTimeoutEvent *)e)->getUId();
	  unsigned long timeout = ((FlowTimeoutEvent *)e)->getTimeout();
          time_t now = Timeval::time(NULL);
          int res;

          // retrieve timestamp of last packet for this flow
          res = proc->ruleTimeout(rid, timeout, now);

          if (res > 1) {
              // no timeout!

              // readjust exp time so that next (possible) flow expiration can
              // be detected just in time
              e->setTime(res);
          } else if (res == 0) {
              // timeout has expired ->collect data
	      FlowRecord *rec;
	
              // retrieve idle flow data via evaluation module(s) for that rule
              // and reset flow to idle in packet processor
              rec = proc->exportRule(rid, rulm->getRule(rid)->getRuleName(), now, timeout);
              
	      // final flow record
	      rec->setFinal(1);

              // export this data to via export module(s) for that rule
              expt->storeData(rid, "", rec);
          } // else (still) idle
      }
      
      break;
    case ADD_RULES:
      {
          ruleDB_t *new_rules = NULL;

          try {
              // support only XML rules from file
              new_rules = rulm->parseRules(((AddRulesEvent *)e)->getFileName());
             
              // test rule spec 
              expt->checkRules(new_rules);
              proc->checkRules(new_rules);
              clss->checkRules(new_rules);

              // no error so lets add the rules and schedule for activation
              // and removal
              rulm->addRules(new_rules, evnt.get());
              saveDelete(new_rules);

	      /*
		above 'addRules' produces an RuleActivation event.
		If rule addition shall be performed _immediately_
		(fds == NULL) then we need to execute this
		activation event _now_ and not wait for the
		EventScheduler to do this some time later.
	      */
	      if (fds == NULL ) {
		  Event *e = evnt->getNextEvent();
		  handleEvent(e, NULL);
		  saveDelete(e);
	      }

          } catch (Error &e) {
              // error in rule(s)
              if (new_rules) {
                  saveDelete(new_rules);
              }
              throw e;
          }
      }
      break;
    case ADD_RULES_CTRLCOMM:
      {
          ruleDB_t *new_rules = NULL;

          try {
              
              new_rules = rulm->parseRulesBuffer(
                ((AddRulesCtrlEvent *)e)->getBuf(),
                ((AddRulesCtrlEvent *)e)->getLen(), ((AddRulesCtrlEvent *)e)->isMAPI());

              // test rule spec 
              expt->checkRules(new_rules);
              proc->checkRules(new_rules);
              clss->checkRules(new_rules);
	  
              // no error so let's add the rules and 
              // schedule for activation and removal
              rulm->addRules(new_rules, evnt.get());
              comm->sendMsg("rule(s) added", ((AddRulesCtrlEvent *)e)->getReq(), fds);
              saveDelete(new_rules);

          } catch (Error &err) {
              // error in rule(s)
              if (new_rules) {
                  saveDelete(new_rules);
              }
              comm->sendErrMsg(err.getError(), ((AddRulesCtrlEvent *)e)->getReq(), fds); 
          }
      }
      break; 	

    case ACTIVATE_RULES:
      {
          ruleDB_t *rules = ((ActivateRulesEvent *)e)->getRules();

          expt->addRules(rules, evnt.get());
          proc->addRules(rules, evnt.get());
          clss->addRules(rules);
          // activate
          rulm->activateRules(rules, evnt.get());
      }
      break;
    case REMOVE_RULES:
      {
          ruleDB_t *rules = ((ActivateRulesEvent *)e)->getRules();
	  
          // export final result data
          for (ruleDBIter_t iter = rules->begin(); iter != rules->end(); iter++) {
              if ((*iter)->isFlagEnabled(RULE_FINAL_EXPORT)) {

		  FlowRecord *rec;

                  // retrieve flow data via evaluation module(s) for that rule
                  rec = proc->exportRule((*iter)->getUId(), (*iter)->getRuleName());

                  // export flow records directly
                  expt->exportFlowRecord(rec, "");

                  saveDelete(rec);
              }
          }
	  
          // now get rid of the expired rule
          clss->delRules(rules);
          proc->delRules(rules);
          expt->delRules(rules);
          rulm->delRules(rules, evnt.get());
      }
      break;

    case REMOVE_RULES_CTRLCOMM:
      {
          try {
              string r = ((RemoveRulesCtrlEvent *)e)->getRule();
              int n = r.find(".");
              if (n > 0) {
                  // delete 1 rule
                  Rule *rptr = rulm->getRule(r.substr(0,n), 
                                             r.substr(n+1, r.length()-n));
                  if (rptr == NULL) {
                      throw Error("no such rule");
                  }
	  
                  // export final result data
                  if (rptr->isFlagEnabled(RULE_FINAL_EXPORT)) {
		      FlowRecord *rec;

                      // retrieve flow data via evaluation module(s)
                      // for that rule
                      rec = proc->exportRule(rptr->getUId(),rptr->getRuleName());
		      
                      // export the flow record directly
                      expt->exportFlowRecord(rec, "");

                      saveDelete(rec);
                  }

                  clss->delRule(rptr);
                  proc->delRule(rptr);
                  expt->delRule(rptr);
                  rulm->delRule(rptr, evnt.get());

              } else {
                  // delete rule set
                  ruleIndex_t *rules = rulm->getRules(r);
                  if (rules == NULL) {
                      throw Error("no such rule set");
                  }

                  for (ruleIndexIter_t i = rules->begin(); i != rules->end(); i++) {
                      Rule *rptr = rulm->getRule(i->second);

                      // export final result data
                      if (rptr->isFlagEnabled(RULE_FINAL_EXPORT)) {
			  FlowRecord *rec;

                          // retrieve flow data via evaluation module(s) for that rule
                          rec = proc->exportRule(rptr->getUId(),rptr->getRuleName());
			  
                          // export this data via export module(s) for that rule
                          expt->exportFlowRecord(rec, "");

                          saveDelete(rec);
                      }
			
                      clss->delRule(rptr);
                      proc->delRule(rptr);
                      expt->delRule(rptr);
                      rulm->delRule(rptr, evnt.get());
                  }
              }

              comm->sendMsg("rule(s) deleted", ((RemoveRulesCtrlEvent *)e)->getReq(), fds);
          } catch (Error &err) {
              comm->sendErrMsg(err.getError(), ((RemoveRulesCtrlEvent *)e)->getReq(), fds);
          }
      }
      break;

    case PROC_MODULE_TIMER:

        proc->timeout(((ProcTimerEvent *)e)->getRID(), ((ProcTimerEvent *)e)->getAID(),
                      ((ProcTimerEvent *)e)->getTID());
        break;
    
    case EXPORT_MODULE_TIMER:
        
        ((ExportTimerEvent *)e)->signalTimeout();
        break;
        
    default:
        throw Error("unknown event");
    }
}


/* ----------------------- run ----------------------------- */

void Meter::run()
{
    fdListIter_t   iter;
    fd_set         rset, wset;
    fd_sets_t      fds;
    struct timeval tv;
    int            cnt = 0;
    int            stop = 0;
    eventVec_t     retEvents;
    Event         *e = NULL;

    try {
        // fill the fd set
        FD_ZERO(&fds.rset);
        FD_ZERO(&fds.wset);
        for (iter = fdList.begin(); iter != fdList.end(); iter++) {
            if ((iter->first.mode == FD_RD) || (iter->first.mode == FD_RW)) {
                FD_SET(iter->first.fd, &fds.rset);
            }
            if ((iter->first.mode == FD_WT) || (iter->first.mode == FD_RW)) {
                FD_SET(iter->first.fd, &fds.wset);
            }
        }
        fds.max = fdList.begin()->first.fd;

        // register a timer for ctrlcomm (only online capturing)
	if (enableCtrl) {
	  int t = comm->getTimeout();
	  if (t > 0) {
            evnt->addEvent(new CtrlCommTimerEvent(t, t * 1000));
	  }
	}

        // start threads (if threading is configured)
        expt->run();
        proc->run();
        clss->run();

#ifdef DEBUG
        log->dlog(ch,"------- meter is running -------");
#endif

        do {

	  if (onlineCap) {
	      // select
              rset = fds.rset;
              wset = fds.wset;
	    
	      tv = evnt->getNextEventTime();

              //cerr << "timeout: " << tv.tv_sec*1e6+tv.tv_usec << endl;

              // note: under most unix the minimal sleep time of select is
              // 10ms which means an event may be executed 10ms after expiration!
              if ((cnt = select(fds.max+1, &rset, &wset, NULL, &tv)) < 0) {
                  if (errno != EINTR) {
		    throw Error("select error: %s", strerror(errno));
                  }
              }

              // check FD events
              if (cnt > 0)  {
                if (FD_ISSET( s_sigpipe[0], &rset)) {
                    // handle sig action
                    char c;
                    
                    if (read(s_sigpipe[0], &c, 1) > 0) {
                        switch (c) {
                        case 'S':
                            stop = 1;
                            break;
                        case 'D':
                            cerr << *this;
                            break;
                        case 'A':
                            // next event
                            
                            // check Event Scheduler events
                            e = evnt->getNextEvent();
                            if (e != NULL) {
                                // FIXME hack
                                if (e->getType() == CTRLCOMM_TIMER) {
                                    comm->handleFDEvent(&retEvents, NULL, 
                                                        NULL, &fds);
                                } else {
                                    handleEvent(e, &fds);
                                }
                                // reschedule the event
                                evnt->reschedNextEvent(e);

                                e = NULL;
                            }		    
                            break;
                            
                        default:
                            throw Error("unknown signal");
                        } 
                        //} else {
                        //throw Error("sigpipe read error");
                    }
                } else {
                    // check meter components
 		    if (!classThread) {
                      clss->handleFDEvent(&retEvents, &rset, &wset, &fds);
                    }
                    if (enableCtrl) {
                      comm->handleFDEvent(&retEvents, &rset, &wset, &fds);
                    }
                 }
               } 
	     } else {
               // offline mode
	       if (!classThread) { // offline threaded is probably broken...
 		  // current time is set eccording to packet timestamp from trace
                  int ret = clss->handleFDEvent(&retEvents, NULL, NULL, NULL); 

                  if (ret < 0) {
                     cout << "End of capture file reached" << endl;
                     if (capFiles.size() > 0) {
                            cout << *clss;

                            // close current tap
                            clss->clearTaps();

                            // open new tap
                            string tt = conf->getValue("TapType", "CLASSIFIER");
                            string sz = conf->getValue("SnapSize", "CLASSIFIER");
                            int snapsize = sz.empty() ? DEF_SNAPSIZE : atoi(sz.c_str());
                            int bsize = conf->getValue("RcvBufSize", "CLASSIFIER").empty() ? 65535 :
                              atoi(conf->getValue("RcvBufSize", "CLASSIFIER").c_str());
                            string dev = capFiles.front();
                            NetTap *nett;

                            if (tt == "erf") {
#ifdef HAVE_ERF
                              if (conf->isTrue("ERFLegacy", "MAIN")) {
                                nett = new NetTapERF(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"),
                                                     snapsize, 1, bsize, 1);
                              } else {
                                nett = new NetTapERF(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"),
                                                     snapsize, 1, bsize);
                              }
#else
                              throw Error("No support for ERF");
#endif
                            } else {
                              /* pcap by default */
#ifdef ENABLE_THREADS
                              nett = new NetTapPcap(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"),
                                                    snapsize, !conf->isTrue("Thread", "CLASSIFIER"), bsize);
#else
                              nett = new NetTapPcap(dev, onlineCap, conf->isFalse("NoPromiscInt", "MAIN"),
                                                    snapsize, 1, bsize);
#endif
                            }

                            clss->registerTap(nett);

                            // remove current file from list
                            capFiles.pop_front();
                          } else {
                            stop = 1;
    	       	       }
		    }
	        }	

                // execute all due events
                evnt->getNextEventTime();
                char c;
                while (read(s_sigpipe[0], &c, 1) > 0) {
                      switch (c) {
                      case 'S':
                          stop = 1;
                          break;
                      case 'D':
                          cerr << *this;
                          break;
                      case 'A':
                          // check Event Scheduler events
                          e = evnt->getNextEvent();
                          if (e != NULL) {
			    handleEvent(e, &fds);
			    // reschedule the event
			    evnt->reschedNextEvent(e);

			    e = NULL;
                          }
                          break;

                      default:
                          throw Error("unknown signal");
                      }
                      //} else {
                      //throw Error("sigpipe read error");

                      evnt->getNextEventTime();
                }
            } 

            if (!pprocThread) {
	      proc->handleFDEvent(&retEvents, NULL,NULL, NULL);
            }
            if (!expThread) {
	      expt->handleFDEvent(&retEvents, NULL, NULL, NULL);
            }

            // schedule events
            if (retEvents.size() > 0) {
                for (eventVecIter_t iter = retEvents.begin();
                     iter != retEvents.end(); iter++) {

                    evnt->addEvent(*iter);
                }
                retEvents.clear(); 
            }
        } while (!stop);

	// wait for packet processor to handle all remaining packets (if threaded)
	proc->waitUntilDone();

 	// do export for all rules that have no export interval (for capture files and on ctrl-c)
        ruleDB_t rules = rulm->getRules();
        for (ruleDBIter_t iter = rules.begin(); iter != rules.end(); iter++) {
	     if ((*iter)->getIntervals()->empty()) {

                  FlowRecord *rec;

                  // retrieve flow data via evaluation module(s) for that rule
                  rec = proc->exportRule((*iter)->getUId(), (*iter)->getRuleName());

		  // final flow record
		  rec->setFinal(1);

                  // export flow records directly
                  expt->exportFlowRecord(rec, "");

                  saveDelete(rec);
              }
        }

	// finally do all push export events in the queue 
	while((e = evnt->getNextEvent()) != NULL) {
	  if (e->getType() == PUSH_EXPORT) {
	    // indicate this is the final record (bit of a hack)
	    ((PushExportEvent *) e)->setFinal(1);

	    handleEvent(e, &fds);

	    if (!expThread) {
	      expt->handleFDEvent(&retEvents, NULL, NULL, NULL);
	    }
	  }

	  saveDelete(e);
	}

	// wait for exporter to handle all remaining exports (if threaded)
	expt->waitUntilDone();

        log->log(ch,"meter going down on Ctrl-C" );

#ifdef DEBUG
        log->dlog(ch,"------- shutdown -------" );
#endif

	if (!onlineCap) {
	  // print the classifier stats
	  cout << *clss;
	}

    } catch (Error &err) {
        if (log.get()) { // Logger might not be available yet
            log->elog(ch, err);
        }	   

        // in case an exception happens between get and reschedule event
        if (e != NULL) {
            saveDelete(e);
        }

        throw err;
    }
}

/* ------------------------- dump ------------------------- */

void Meter::dump(ostream &os)
{
    /* FIXME to be implemented */
    os << "dump" << endl;
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<<(ostream &os, Meter &obj)
{
    obj.dump(os);
    return os;
}

/* ------------------------ signal handler ---------------------- */

void Meter::sigint_handler(int i)
{
    char c = 'S';

    write(s_sigpipe[1], &c,1);
}

void Meter::sigusr1_handler(int i)
{
    char c = 'D';
    
    write(s_sigpipe[1], &c,1);
}

void Meter::exit_fct(void)
{
    unlink(NETMATE_LOCK_FILE.c_str());
}

void Meter::sigalarm_handler(int i)
{
    g_timeout = 1;
}

/* -------------------- alreadyRunning -------------------- */

int Meter::alreadyRunning()
{
    FILE *file;
    char cmd[128];
    struct stat stats;
    int status, oldPid;

    // do we have a lock file ?
    if (stat(NETMATE_LOCK_FILE.c_str(), &stats ) == 0) { 

        // read process ID from lock file
        file = fopen(NETMATE_LOCK_FILE.c_str(), "rt" );
        if (file == NULL) {
            throw Error("cannot open old pidfile '%s' for reading: %s\n",
                        NETMATE_LOCK_FILE.c_str(), strerror(errno));
        }
        fscanf(file, "%d\n", &oldPid);
        fclose(file);

        // check if process still exists
        sprintf( cmd, "ps %d > /dev/null", oldPid );
        status = system(cmd);

        // if yes, do not start a new meter
        if (status == 0) {
            return 1;
        }

        // pid file but no meter process ->meter must have crashed
        // remove (old) pid file and proceed
        unlink(NETMATE_LOCK_FILE.c_str());
    }

    // no lock file and no running meter process
    // write new lock file and continue
    file = fopen(NETMATE_LOCK_FILE.c_str(), "wt" );
    if (file == NULL) {
        throw Error("cannot open pidfile '%s' for writing: %s\n",
                    NETMATE_LOCK_FILE.c_str(), strerror(errno));
    }
    fprintf(file, "%d\n", getpid());
    fclose(file);

    return 0;
}

/* ------------------------- main() ------------------------- */


// Log functions are not used before the logger is initialized

int main(int argc, char *argv[])
{

    try {
        // start up the netmate (this blocks until Ctrl-C !)
        cout << NETMATE_VERSION << endl;
#ifdef DEBUG
        cout << NETMATE_OPTIONS << endl;
#endif
        auto_ptr<Meter> meter(new Meter(argc, argv));
        cout << "Up and running." << endl;

        // going into main loop
        meter->run();

        // shut down the meter
        cout << "Terminating netmate." << endl;

    } catch (Error &e) {
        cerr << "Terminating netmate on error: " << e.getError() << endl;
        exit(1);
    }
}
