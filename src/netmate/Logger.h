
/*! \file Logger.h

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
    generate log file messages

    $Id: Logger.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _LOGGER_H_
#define _LOGGER_H_


#include "stdincpp.h"
#include "Error.h"
#include "Threads.h"
#include "constants.h"


//! symbolic ids for logging levels
typedef enum {
    L_INFO  = 0,    //!<  log level id for logging of [INFO]     messages
    L_CRIT  = 1,    //!<  log level id for logging of [CRITICAL] messages
    L_ERROR = 2,    //!<  log level id for logging of [ERROR]    messages
    L_WARN  = 3,    //!<  log level id for logging of [WARN]     messages
    L_DEBUG = 4,    //!<  log level id for logging of [DEBUG]    messages
    L_MAXLEVEL = 4  //!<  maximal log level id for logging of messages
} logLevel_t;


/*! \brief provides logging of text messages to logging channels (files)

    the Logger class is used to hold a number of FILE* for
    files in which logging of messages is to be done \n
    so you only need to have one global Logger object lying around \n
    \n
    usage: \n
    create a Logger object \n
    assign a logging channel (number) to a file (filename) \n
    log messages to that channel (fprintf like syntax)
*/

class Logger
{
  private:

    //! link to the globally available Logger instance
    static Logger *s_instance;

    //! private subclass representing one logging channel inside the Logger object
    class Channel
    {
      public:
        string prefix;  //! prefix string to write before each log text
	    int    file_id; //! id (index) for the used file
	
    	Channel(string str = "") : prefix(str), file_id(0) {}
    };
    
    
    class LogFile
    {
      public:
    	string fname;
	    FILE*  file;
	
    	LogFile( string name = "" ) : fname(name), file(NULL) {}
    };


  private:
    
    int ch;       //!< the Loggers own logging channel
    int logLevel; //!< the configured logging level

    /*! flag, indicates whether some meter component runs as a separate thread.
        If yes, then the Logger uses semaphores to be multi-threading-save
     */
    int threaded; 

    /*!  flag, set to 0 upon Logger startup if logging is not possible
         (e.g. log file cannot be written), else == 1
    */
    int canlog;

    unsigned int numChannels; //!< number of used channels
    vector<Channel> channels; //!< array with channel prefix and file id info

    unsigned int numLogfiles; //!< number of used file descriptors
    vector<LogFile> logfiles; //!< array with log file name and FILE pointers

    string logfile; //!< logger class default logfile name

#ifdef ENABLE_THREADS
    mutex_t maccess;  //!< mutex semaphore for thread safety blocking
#endif

private:

    void _write( int level, int channel, int newline, const char *fmt, va_list argp );

    /*! \short   constructor - creates and initializes the one Logger object
        \throws Error in case default log file cannot be created/written
    */
    Logger(const string logFile = DEFAULT_LOG_FILE);


public:

    /*! \short   set the log file which is used if no log file in
                 explicitly given when opening a new channel
    */
    void setDefaultLogfile( string fname );


    /*! \short   destroys a message logger object,
                 closes all open message channels (files)
    */
    ~Logger();

    //! set value of threading flag (see above)
    inline void setThreaded( int thr ) {
    	threaded = thr;
    }

    //! \short  flush all logging channels (does not close files)
    void flush();

    //! set the maximum logging level, higher levels will not be shown
    void setLogLevel( int level );

    //! get access to the one and only Logger instance
    static Logger *getInstance();

    /*! \short   assign a channel number to a file

        the file will be opened for appending text and each 
        message logged to that channel will be written to that file \n
        the file will be opened for appending in line buffered mode
      
        \arg \c prefix  - text to prepend to each message logged
                          via this channel
        \arg \c name    - name of file to appends messages to (optional),
                          if unspecified default log file is used
        \arg \c newlog  - remove log file first if =1, else append to file
    */
    int createChannel( const string prefix = string(""), 
                       const string name = string(""), int newlog = 1 );

    //! rename (move) the currently used default logfile
    int moveChannels(string ofname, string nfname, int newlog = 1);

    /*! \short   logs a textual message to a channel

        the given text will be written to the file associated with the 
        specified channel prefixed with that channels prefix string \n
        arguments after the format string are treated in a printf manner \n
        a newline written after the format string

        \arg \c channel - number of logging channel
        \arg \c fmt     - printf like format string
        \arg \c ...     - additional arguments for printing

        \throws Error , getError() can result in:
        - "illegal channel number"
        - "channel not yet assigned"
        - "error writing to file"
    */
    void log(  int level, int channel, const char *fmt, ... );
    void log(  int channel, const char *fmt, ... );
    void clog( int channel, const char *fmt, ... );
    void wlog( int channel, const char *fmt, ... );
    void elog( int channel, const char *fmt, ... );
    void dlog( int channel, const char *fmt, ... );
    void elog( int channel, Error &e );

    //! \short   dump Logger object
    void dump( ostream &os );

};


//! overload << for use on Logger objects
ostream& operator<< ( ostream &os, Logger &ml );


#endif // _LOGGER_H_
