
/* \file constants.cc

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
  here all string and numeric constants for the netmate toolset are stored

  $Id: constants.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "stdincpp.h"
#include "constants.h"

using namespace std;

// RuleManager.cc
const unsigned int  DONE_LIST_SIZE = 50;
const string        FILTERVAL_FILE = DEF_SYSCONFDIR "/filterval.xml";
const string        FILTERDEF_FILE = DEF_SYSCONFDIR "/filterdef.xml";

// Meter.h
const string DEFAULT_CONFIG_FILE = DEF_SYSCONFDIR "/netmate.conf.xml";
const string NETMATE_LOCK_FILE   = DEF_STATEDIR "/run/netmate.pid";
const int    DEF_SNAPSIZE        = 65536;

// Logger.h
const string DEFAULT_LOG_FILE = DEF_STATEDIR "/log/netmate.log";

// RuleFileParser.cc
const string RULEFILE_DTD     = DEF_SYSCONFDIR "/rulefile.dtd";

// Rule.cc
const string TIME_FORMAT      = "%Y-%m-%d %T";
const string DEFAULT_SETNAME  = "default";

// FilterValParser.cc
const string FILTERVAL_DTD   = DEF_SYSCONFDIR "/filterval.dtd";

// FilterDefParser.cc
const string FILTERDEF_DTD   = DEF_SYSCONFDIR "/filterdef.dtd";

// CtrlComm.cc
const string REPLY_TEMPLATE  = DEF_SYSCONFDIR "/reply.xml";   //!< html response template
const string MAIN_PAGE_FILE  = DEF_SYSCONFDIR "/main.html";
const string XSL_PAGE_FILE   = DEF_SYSCONFDIR "/reply.xsl";
const int    EXPIRY_TIME     = 3600; //!< expiry time for web pages served from cache
const int    DEF_PORT        = 12244;         //!< default TCP port to connect to

// ConfigParser.h
const string CONFIGFILE_DTD  = DEF_SYSCONFDIR "/netmate.conf.dtd";

// nmrsh.cc
const string PROMPT     = "> ";          //!< user prompt for interactive mode
const string DEF_SERVER = "localhost";   //!< default netmate host  
const int    HISTLEN    = 200;           //!< history length (lines)
const string HISTFILE   = "/.nmrsh_hist"; //!< name of the history file
const string DEF_XSL    = DEF_SYSCONFDIR "/reply2.xsl"; //!< xsl file for decoding xml responses

// environment var which points to the home dir where the hist file is
const string HOME = "HOME";

#ifdef USE_SSL
// certificate file location (SSL)
const string CERT_FILE = DEF_SYSCONFDIR "/netmate.pem";
#endif

// help text in interactive shell
const string HELP =  "" \
"interactive commands: \n" \
"help                            displays this help text \n" \
"quit, exit, bye                 end telly program \n" \
"get_info <info_type> <param>    get meter information \n" \
"rm_task <rule_id>               remove rule(s) \n" \
"add_task <rule>                 add rule \n" \
"add_tasks <rule_file>           add rules from file \n" \
"get_modinfo <mod_name>          get meter module information \n \n" \
"any other text is sent to the server and the reply from the server is displayed.";

