
/*! \file nmrsh.cc

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
    
    nmrsh is the netmate remote shell
    
    this tool can be used for remote control of netmate meters
    it supports all command which are part of the control interface
    

    $Id: nmrsh.cc 748 2009-09-10 02:54:03Z szander $

*/


#include "stdincpp.h"
extern "C" {
#include <readline/readline.h>
#include <readline/history.h>
#include "getpass.h"
}

// classes
#include "Error.h"
#include "CommandLineArgs.h"
#include "constants.h"

// curl includes
#include <curl/curl.h>

// xml includes
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>


/* -------------------- History -------------------- */

static int saveHistory( void )
{
    string histfile;
    char *home;

    if ((home = getenv(HOME.c_str())) != NULL ) {
        histfile = string(home) + HISTFILE;
        stifle_history(HISTLEN);

        return write_history((char *) histfile.c_str());
    }

    return -1;
}

static int initHistory(string input)
{
      string histfile;
      char  *home;
                   
      if ((home = getenv(HOME.c_str())) != NULL) {
          struct stat sbuf;
            
          histfile = string(home) + HISTFILE;
                       
          if ((stat(histfile.c_str(), &sbuf ) == 0) && (sbuf.st_size > 0)) {
              read_history((char *) histfile.c_str());

              input = history_get(history_length)->line;             
          } else {
              input = "";
          }

          return 0;
      }

      return -1;
}

/* -------------------- sig_term -------------------- */

static void sig_term(int i)
{
    if (saveHistory() < 0) {
        cerr << "Error: write history" << endl;
        exit(1);
    }
    cerr << endl;
    exit(0);
}


/* ------------------- curl callbacks ---------------- */

size_t writedata( void *ptr, size_t size, size_t nmemb, void  *stream)
{
    string *s = (string *) stream;

    s->append((char *)ptr, size*nmemb);

    return size*nmemb;
}

/* -------------------- Passwd ------------------------ */

string getpasswd(string prompt)
{
    char passwd[256];

    getpass_r("Password: ", passwd, sizeof(passwd));

    return passwd;
}


/* -------------------- Parser ------------------------ */

// parse command and generate form
int parseCommand(string input, string &action, string &postfields)
{
    stringstream input_str;
    string param;

    input_str << input;
    input_str.setf(ios::skipws);
    input_str >> action;

    if (action == "get_info") {
        action = "/get_info";
        input_str >> param;
        if (!param.empty()) {
            postfields = "IType=" + param;
        }
        if (!input_str.eof()) {
            input_str >> param;
            if (!param.empty()) {
                postfields += "&IParam=" + param;
            }
        }
    } else if (action == "rm_task") {
        action = "/rm_task";
        input_str >> param;
        if (!param.empty()) {
            postfields = "RuleID=" + param;
        }
    } else if (action == "add_task") {
        action = "/add_task";
        getline(input_str, param);
        if (!param.empty()) {
            postfields = "Rule=" + param;
        }
    } else if (action == "add_tasks") {
        action = "/add_task";
        input_str >> param;
        
        string line;
        ifstream in(param.c_str());
    
        if (!in) {
            cerr << "Error: cannot access/read file "<< param << endl;
        } else {
            param = "";
            while (getline(in, line)) {
                param += line;
            }
            
            // read rule file and add 
            if (!param.empty()) {
                postfields = "Rule=" + param;
            }
        }
    } else if (action == "get_modinfo") {
        action = "/get_modinfo";
        input_str >> param;
        if (!param.empty()) {
            postfields = "IName=" + param;
        }
    } else if (action == "help") {
        cout << HELP << endl;
        return -1;
    } else if ((action == "quit") || (action == "bye") || (action == "exit")) {
        return 1;
    } else {
        action = "";
        cout << HELP << endl;
        return -1;
    }

    return 0;
}

//! extract errno out of err string generated by curl and translate errno
//! to string by using strerror
string getErr(char *e)
{
    string err = e;

    int p = err.find_last_of(":");

    if (p > 0) {
        return err.substr(0,p) + ": " + 
          string(strerror(atoi(err.substr(p+1, err.length()-p).c_str())));
    }

    return "";
}

/* -------------------- main -------------------- */

int main( int argc, char *argv[] )
{

  int c = 0, interactive = 0;
  char cebuf[CURL_ERROR_SIZE], *ctype;
  char *post_body = NULL;
  string userpwd;
  string command;
  string cfname;
  ifstream cfname_in;
  string server = DEF_SERVER;
  string response;
  string input, input2;
  string stylesheet = DEF_XSL;
  int port = DEF_PORT;
  unsigned long rcode;
#ifdef USE_SSL
  int use_ssl = 0;
#endif
  CommandLineArgs args;
  CURL *curl;
  CURLcode res;
  xsltStylesheetPtr cur = NULL;
  xmlDocPtr doc, out;

  memset(cebuf, 0, sizeof(cebuf));

  // install signal handler
  signal( SIGTERM, sig_term );
  signal( SIGINT, sig_term );

  // set command line args
  args.add('s', "NetmateServer", "<server>", "select netmate server"
           , "MAIN", "server");
  args.add('p', "NetmatePort", "<port>", "select netmate port", "MAIN", "port");
  args.add('c', "Command", "<command>", "command which will be executed",
           "MAIN", "command");
  args.add('f', "CommandFilename", "<filename>", "select command file name",
           "MAIN", "cmdfile");
#ifdef USE_SSL
  args.addFlag('x', "UseSSL", "use SSL", "MAIN", "usessl");
#endif
  args.addFlag('V', "RelVersion", "show version info and exit",
                "MAIN", "version");
  args.add('u', "User", "<username>", "user name", "MAIN", "user");
  args.add('P', "Passwd", "<passwd>", "password", "MAIN", "password");
  args.add('t', "XSLFile", "<filename>", "select xsl stylesheet file",
           "MAIN", "stylesheet");

  try {
      // parse command line args
      if (args.parseArgs(argc, argv)) {
          exit(0);
      }
  } catch (Error &e) {
      cerr << "Error: " << e.getError() << endl;
      exit(1);
  }

  if (args.getArgValue('V') == "yes") {
      // same as netmate version
      cout << "nmrsh version: " << VERSION << endl;
      exit(0);
  }

  if (!args.getArgValue('p').empty()) {
      port = atoi(args.getArgValue('p').c_str());
  }

  if (!args.getArgValue('s').empty()) {
      server = args.getArgValue('s');
  }

  if (!args.getArgValue('c').empty()) {
      command = args.getArgValue('c');
  } else {
      c = 1;
  }

  if (!args.getArgValue('f').empty()) {
      cfname = args.getArgValue('f');
      cfname_in.open(cfname.c_str());
      if (!cfname_in) {
          cerr << "Error: cannot access/read file " << cfname;
          exit(1);
      }
  }

  if (!command.empty() && !cfname.empty()) {
      cout << "Executing command line command before command file" << endl;
  }

  if (command.empty() && cfname.empty()) {
      interactive = 1;
  }

  // initialize xml stuff
  if (!args.getArgValue('t').empty()) {
      stylesheet = args.getArgValue('t');
  }
  xmlSubstituteEntitiesDefault(1);
  xmlLoadExtDtdDefaultValue = 1;
  cur = xsltParseStylesheetFile((const xmlChar *)stylesheet.c_str());

  // initialize libcurl
  curl = curl_easy_init();

  // generate request

  if (!curl) {
      cerr << "Error: curl init error" << endl;
      exit(1);
  }

#ifdef USE_SSL
  if (args.getArgValue('x') == "yes") {
      use_ssl = 1;
      curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
      curl_easy_setopt(curl, CURLOPT_SSLCERT, CERT_FILE.c_str());
      curl_easy_setopt(curl, CURLOPT_SSLKEYPASSWD, SSL_PASSWD);
      curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
      curl_easy_setopt(curl, CURLOPT_SSLKEY, CERT_FILE.c_str());
      /* do not validate server's cert because its self signed */
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
      /* do not verify host */
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
  }
#endif

  // debug
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, cebuf);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writedata);

  if (!args.getArgValue('u').empty()) {
      // if no passwd yet ask for it
      if (args.getArgValue('P').empty()) {
	  userpwd = args.getArgValue('u') + ":" + getpasswd("Password: ");
      } else { 
          userpwd = args.getArgValue('u') + ":" + args.getArgValue('P');
      }

      curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd.c_str());
  }
  
  for(;;) {
      ostringstream url;
          
      if (!interactive) {
          // first the command on command line
          if ((c == 0) && !command.empty()) { 
              input = command;
              c++;
          }
          // then command(s) from file
          else if ((c > 0) && !cfname.empty()) {
              if (!getline(cfname_in, input)) {
                  break;
              } 
          } else {
              break;
          }
      } else {
          if (input.empty() && input2.empty()) { /* first time in getCommand? */
              if (initHistory(input) < 0) {
                  cerr << "Error: read history" << endl;
                  exit(1);
              }
          }

          char *tmp = readline((char *) PROMPT.c_str());
          input2 = tmp;
          free(tmp);

          // If the line has any text in it, save it on the history.
          // but only do so, if it is different from the previous entry 
          if (!input2.empty() && (input2 != input)) {
              add_history((char *) input2.c_str());
          }
               
          input = input2;
      }

      // parse input
      string action, post_fields;
      int ret = parseCommand(input, action, post_fields);
      
      if (ret < 0) {
          continue;
      }

      if (ret > 0) {
          break;
      }

      if (!input.empty()) {
          // build URL
#ifdef USE_SSL
          if (use_ssl) {
              url << "https://";
          } else {
#endif
              url << "http://";
#ifdef USE_SSL
          }
#endif
          url << server << ":" << port;
          url << action;
              
          //cerr << "URL: " << url.str() << endl;
         
          char *_url = strdup(url.str().c_str());
          curl_easy_setopt(curl, CURLOPT_URL, _url);
          post_body =  curl_escape(post_fields.c_str(), post_fields.length());
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);

          if (rcode == 401) {
               // last password used was wrong -> ask again
               userpwd = args.getArgValue('u') + ":" + getpasswd("Password: ");
               curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd.c_str());
          }

          res = curl_easy_perform(curl);
          // FIXME curl prob: error buf is first error while res is the
          // number for the last error
          if (res != 0) {
              cerr << "Error (" << res << "): "<< getErr(cebuf) << endl;
              goto done;
          }

          res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ctype);
          // FIXME curl prob: error buf is first error while res is the
          // number for the last error
          if (res != 0) {
              cerr << "Error (" << res << "): "<< getErr(cebuf) << endl;
              goto done;
          }

          res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &rcode);
          // FIXME curl prob: error buf is first error while res is the
          // number for the last error
          if (res != 0) {
              cerr << "Error (" << res << "): "<< getErr(cebuf) << endl;
              goto done;
          }

          //cout << "RESPONSE: " << endl;

          if (!strcmp(ctype, "text/xml")) {
              // translate
              doc = xmlParseMemory(response.c_str(), response.length());
              out = xsltApplyStylesheet(cur, doc, NULL);
              xsltSaveResultToFile(stdout, out, cur);
          
              xmlFreeDoc(out);
              xmlFreeDoc(doc);
          } else {
              // just dump
              cout << response << endl;
          }

        done:
          // clear
          response = "";
          free(_url);
#ifdef HAVE_CURL_FREE
          curl_free(post_body);
#else
          free(post_body);
#endif
      }
  }

  curl_easy_cleanup(curl);

  xsltFreeStylesheet(cur);
  xsltCleanupGlobals();
  xmlCleanupParser();

  if (!cfname.empty()) {
      cfname_in.close();
  }
  
  if (interactive) {
      if (saveHistory() < 0) {
          cerr << "Error: write history" << endl;
          exit(1);
      }
  }

  return 0;

}
