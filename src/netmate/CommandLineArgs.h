
/*! \file   CommandLineArgs.h

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
    parses command line args and puts values in config db

    $Id: CommandLineArgs.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _COMMANDLINEARGS_H_
#define _COMMANDLINEARGS_H_


#include "stdincpp.h"
#include "getopt_long.h"

#include "Error.h"
#include "Logger.h"



//! command line arg
typedef struct
{
    int flag;        //!< is flag
    string param;    //!< parameter(s)
    string help;     //!< help text for the argument
    string name;     //!< name of the argument
    string group;    //!< group of the argument
    string longname; //!< longname of the argument
} commandLineArg_t;

//! sort command line options alphabetically: a,A,b,B,c,D etc.
struct ltarg
{
    bool operator()(const char a1, const char a2) const
    {
        double t1 = a1, t2 = a2;

        if (t1 < 'a') {
    	    t1 += ('a'-'A') + 0.5;
    	}
        if (t2 < 'a') {
    	    t2 += ('a'-'A') + 0.5;
    	}
    	return (t1 < t2);
    }
};

//! command line argument list
typedef map<char, commandLineArg_t, ltarg>           commandLineArgList_t;
typedef map<char, commandLineArg_t, ltarg>::iterator commandLineArgListIter_t;

//! list of values for each argument
typedef map<char, string>           commandLineArgValList_t;
typedef map<char, string>::iterator commandLineArgValListIter_t;

//! from getopt.h
typedef struct option option_t;

class CommandLineArgs
{
  private:

    commandLineArgList_t    args;    //!< the command line arguments list
    commandLineArgValList_t argVals; //!< and their values
    
    /*! \short  add a possible command line option to list own known options
        \arg \c flag      1 if option if only flag (on/off) and has no parameters
        \arg \c opt       the option character
        \arg \c name      the option name
        \arg \c param     parameter(s)
        \arg \c help      a textual option description
        \arg \c group     the option group
        \arg \c longname  the long name (for --longname format) for this option
     */
    void addEntry( int flag, char opt, string name, string param, string help, string group,
        		   string longname);

  public:

    //! construct a new CommandLineArgs object
    CommandLineArgs();

    //! add a command line argument expecting a value
    void add(char opt, string name, string param="", string help="", string group="", string longname="");

    //! add a command line argument flag (yes/no)
    void addFlag(char opt, string name, string help="", string group="", string longname="");

    //! get the value for an argument
    string getArgValue(char opt);

    //! parse the command line args supplied to main
    int parseArgs(int argc, char *argv[]);

    //! get pointer to argument values
    commandLineArgValList_t *getArgVals()
    {
        return &argVals;
    }

    //! get pointer to argument definition list
    commandLineArgList_t *getArgList()
    {
        return &args;
    }

    //! get usage string (is build from options description)
    string getUsage();

};


#endif // _COMMANDLINEARGS_H_
