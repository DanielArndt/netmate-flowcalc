
/*!\file   CommandLineArgs.cc

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
    parses command line args

    $Id: CommandLineArgs.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "CommandLineArgs.h"


static const int INDENTATION = 14;
static const int INDENTATION2 = 25;


CommandLineArgs::CommandLineArgs()
{
    // add the default options which are handled special
    addFlag('h', "Help", "show this help and exit", "", "help");
}


void CommandLineArgs::add(char opt, string name, string param, string help, string group,
			  string longname)
{
    addEntry( 0, opt, name, param, help, group, longname );
}


void CommandLineArgs::addFlag(char opt, string name, string help, string group,
			      string longname)
{
    addEntry( 1, opt, name, "", help, group, longname );
}


void CommandLineArgs::addEntry( int flag, char opt, string name, string param, string help,
				string group, string longname)
{
    commandLineArg_t new_arg;

    if (args.find(opt) == args.end()) {
        new_arg.flag = flag;
        new_arg.name = name;
        new_arg.param = param;
        new_arg.help = help;
        new_arg.group = group;
        new_arg.longname = longname;
        args[opt] = new_arg;
    } else {
        throw Error("Redefinition of command line option!");
    }
}


string CommandLineArgs::getArgValue(char opt)
{
    commandLineArgValListIter_t iter;
    
    iter = argVals.find(opt);
    if (iter != argVals.end()) {
        return string(iter->second);
    } else {
        return string();
    }
}


int CommandLineArgs::parseArgs(int argc, char *argv[])
{
    commandLineArgListIter_t iter;
    string getopt_str;
    option_t *longopts, *lopt;
    int i;

    if (argc == 0) {
        return 0;
    }

    lopt = longopts = new option_t[args.size() + 1];

    // figure out non-options (i == 1) and print usage cause netmate
    // currently does not accept any non-options; we do this for users
    // who type a parameter value without the option...  
    getopt_str = getopt_str + "-";

    // build string and logopt array for getopt_long
    for (iter = args.begin(); iter != args.end(); iter++) {
        
        if (iter->second.flag) {
            getopt_str = getopt_str + iter->first;
        } else {
            getopt_str = getopt_str + iter->first + ":";
        }

        if (iter->second.longname != "") {
            lopt->name    = iter->second.longname.c_str();
            lopt->has_arg = (iter->second.flag == 0) ? required_argument :
              no_argument;
            lopt->flag    = NULL;
            lopt->val     = (int) iter->first;
            lopt += 1;
        }
    }

    // terminate long opts list by filling last entry with zeroes
    lopt->name    = NULL;
    lopt->has_arg = 0;
    lopt->flag    = NULL;
    lopt->val     = 0;

    while ((i = getopt_long(argc, argv, getopt_str.c_str(),
			    longopts, NULL)) != -1) {
        if ((i == 1) || (i == '?') || (i == 'h')) {
            cerr << "Usage: " << argv[0] 
                 << " [OPTIONS]" << endl
                 << getUsage();

            saveDeleteArr(longopts);

            if (i == '?') {
                throw Error("invalid command line parameter");
            } else {
                return 1;
            }
        } else {
            argVals[i] = (optarg != NULL) ? string(optarg) : string("yes");
            // FIXME only longopt not yet supported
        }
    }

    saveDeleteArr(longopts);

    return 0;
}


static string dashdash( string name ) {
    if (name != "") {
        return "--" + name;
    } else {
        return "";
    }
}


string CommandLineArgs::getUsage()
{
    ostringstream s;
    commandLineArgListIter_t iter;
    string help;

    for (iter = args.begin(); iter != args.end(); iter++) {
      s << "  -" << iter->first                       // append config option char
        << ", "
        << setfill(' ') << setw(INDENTATION)
        << setiosflags(ios::left)
        << dashdash(iter->second.longname)            // append long option name
        << setfill(' ') << setw(INDENTATION2)
        << setiosflags(ios::left)
        << iter->second.param 
        << iter->second.help << setw(0)               // append help text
	<< endl;
    }

    return s.str();
}
