
/*! \ file Logger.cc

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


$Id: Logger.cc 748 2009-09-10 02:54:03Z szander $

*/


#include "Error.h"
#include "Logger.h"


/*!
  minimum number of logging channels a Logger object has if it is
  constructed using the default constructor Logger().
  This number is increased dynamically at runtime when more channels are needed
*/
const int INITIAL_NB_CHANNELS = 8;

//! default number of different file descriptors usable by one logger
const int INITIAL_NB_LOGFILES = 4;


static char *LogLevel[] = { "INFO ", "CRITICAL", "ERROR", "WARN ", "DEBUG" };

Logger *Logger::s_instance = NULL;


/* -------------------- getInstance -------------------- */

Logger *Logger::getInstance()
{ 
    if (s_instance == NULL) {
        s_instance = new Logger();
    }
    return s_instance;
}


/* ------------------------- Logger ------------------------- */

Logger::Logger(const string defaultFile) :
    logLevel(4), threaded(1), canlog(0), numChannels(0), numLogfiles(0)
{
    /*
      Logger is set to not threaded from Meter.cc via setThreaded(0) if
      and only if not a single meter component is configured as threaded
    */

    unlink(defaultFile.c_str());
    //    setDefaultLogfile(defaultFile);

#ifdef ENABLE_THREADS
    if (threaded) {
        mutexInit(&maccess);
    }
#endif

    channels.resize(INITIAL_NB_CHANNELS);
    logfiles.resize(INITIAL_NB_LOGFILES);

    logfile = defaultFile;
    ch = createChannel("Logger");
    canlog = 1;
}


/* ------------------------- ~Logger ------------------------- */

Logger::~Logger()
{
    // close all open files
    for (unsigned int i = 0; i < numLogfiles; i++) {
        if (logfiles[i].file != NULL) {
            fclose(logfiles[i].file);
        }
    }
#ifdef ENABLE_THREADS
    if (threaded) {
        mutexDestroy(&maccess);
    }
#endif
}


/* -------------------- setDefaultLogfile -------------------- */

void Logger::setDefaultLogfile( string fname )
{
    // at first time new name is set rename default logs
    if (fname != logfile) {
        moveChannels(logfile, fname);
        log(ch, "logfilename used is: '%s'", fname.c_str());
    }
    logfile = fname;
}


/* ------------------------- createChannel ------------------------- */

int Logger::createChannel( const string prefix, const string filename, int newlog )
{
    unsigned int id, fi;
    FILE* fdes;
    string fname;

    try {
        AUTOLOCK(threaded, &maccess);

        // if necessary enlarge storage arrays,
        // before (possibly) inserting a new entry

        if (numChannels == channels.size()) {
            channels.resize(2 * channels.size());
        }
        if (numLogfiles == logfiles.size()) {
            logfiles.resize(2 * logfiles.size());
        }

        // use default file name if non is supplied
        if (filename != "") {
            fname = filename;
        } else {
            fname = logfile;
        }
	
        // try to find the given channel (see if prefix and logfile identical)
        for (id = 0; id < numChannels; id++) {
            if (channels[id].prefix == prefix &&
                logfiles[channels[id].file_id].fname == fname) {
                return id;
            }
        }

        // try to find a file descriptor, i.e. see if the file is already in use by Logger
        for (fi = 0; fi < numLogfiles; fi++) {
            if (logfiles[fi].fname == fname) {
                channels[numChannels].prefix = prefix;
                channels[numChannels].file_id = fi;
                return numChannels++;
            }
        }
	
        // if specified get rid of old log file first
        if (newlog == 1) {
            unlink(fname.c_str());
        }
	
        // else create new logFile and channel entry
        fdes = fopen(fname.c_str(), "a");
        if (fdes == NULL) {
            throw (Error("cannot open log file '%s' for appending", fname.c_str()));
        }
        // set to line buffered
        setvbuf(fdes, NULL, _IOLBF, 0);
	
        logfiles[numLogfiles].fname = fname;
        logfiles[numLogfiles].file = fdes;
	
        channels[numChannels].file_id = numLogfiles++;
        channels[numChannels].prefix = prefix;

    } catch (Error &e) {
        throw e;
    }
    return numChannels++;
}

int Logger::moveChannels(string ofname, string nfname, int newlog)
{
    unsigned int id, fi;
    FILE *fdes, *ofdes;

    try {
        AUTOLOCK(threaded, &maccess);

        // close and rename file
        for (fi = 0; fi < numLogfiles; fi++) {
            if (logfiles[fi].fname == ofname) {
                fclose(logfiles[fi].file);
                logfiles[fi].file = NULL;
                break;
            }
        }
	
        // move all channels over
        for (id = 0; id < numChannels; id++) {
            if (logfiles[channels[id].file_id].fname == ofname) {
                logfiles[channels[id].file_id].fname = nfname;
            }
        }

        // copy old to new (avoid rename function because it can't rename across
        // file systems

        // open old for reading
        ofdes = fopen(ofname.c_str(), "r");
        if (ofdes == NULL) {
            throw (Error("cannot open old log file '%s' for reading", ofname.c_str()));
        }

        // if start new log then delete existing file with new name
        if (newlog == 1) {
            unlink(nfname.c_str());
        }
	
        // open new file
        fdes = fopen(nfname.c_str(), "a");
        if (fdes == NULL) {
            throw (Error("cannot open log file '%s' for appending", nfname.c_str()));
        }
        // set to line buffered
        setvbuf(fdes, NULL, _IOLBF, 0);
	
        logfiles[fi].fname = nfname;
        logfiles[fi].file = fdes;
	
        // copy old stuff
        char buf[4096];
        int sz = 0;

        while ((sz = fread(buf, 1, sizeof(buf), ofdes)) > 0) {
            fwrite(buf, 1, sz, fdes);
        }

        // close and delete old
        fclose(ofdes);
        unlink(ofname.c_str());

    } catch (Error &e) {
        canlog = 0;
        throw e;
    }
    return 0;
}

/* ------------------------- log ------------------------- */


void Logger::log(int level, int channel, const char *fmt, ...)
{
    if (fmt == NULL) {
        return;
    } 

    va_list argp;
    va_start(argp, fmt);
    _write(level, channel, 1, fmt, argp);
    va_end(argp);
}


void Logger::log(int channel, const char *fmt, ...)
{
    if (fmt == NULL) {
        return;
    } 

    va_list argp;
    va_start(argp, fmt);
    _write(L_INFO, channel, 1, fmt, argp);
    va_end(argp);
}


void Logger::clog(int channel, const char *fmt, ...)
{
    if (fmt == NULL) {
        return;
    } 

    va_list argp;
    va_start(argp, fmt);
    _write(L_CRIT, channel, 1, fmt, argp);
    va_end(argp);
}


void Logger::wlog(int channel, const char *fmt, ...)
{
    if (fmt == NULL) {
        return;
    } 
 
    va_list argp;
    va_start(argp, fmt);
    _write(L_WARN, channel, 1, fmt, argp);
    va_end(argp);
}


void Logger::elog(int channel, const char *fmt, ...)
{
    if (fmt == NULL) {
        return;
    } 

    va_list argp;
    va_start(argp, fmt);
    _write(L_ERROR, channel, 1, fmt, argp);
    va_end(argp);
}


void Logger::elog(int channel, Error &e )
{
    log(L_ERROR, channel, "%s", e.getError().c_str());
}


void Logger::dlog(int channel, const char *fmt, ...)
{
#ifdef DEBUG
    if (fmt == NULL) {
        return;
    } 

    va_list argp;
    va_start(argp, fmt);
    _write(L_DEBUG, channel, 1, fmt, argp);
    va_end(argp);
#endif
}


/* ------------------------- _write ------------------------- */

void Logger::_write( int lvl, int ch, int nl, const char *fmt, va_list argp )
{
    time_t now;
    char buf[300];

    // lvl 0 is most important, >0 less impo.
    if (canlog == 0 || lvl > logLevel) { 
        return;
    } 

    try {
        AUTOLOCK(threaded, &maccess);

        FILE *file = logfiles[channels[ch].file_id].file;
        if (file == NULL) {
            throw Error("No log file for channel %d", ch);
        }

        struct tm tm;
        now = time(NULL);
        strftime(buf, sizeof(buf), "%b %d %H:%M:%S", localtime_r(&now,&tm));
        if (fprintf(file, "%s  %s  ", buf, LogLevel[lvl]) < 0) {
            throw Error("Writing log mesage (Logger::_write#strftime)");
        }

        // prefix defined? yes, then write it before fmt string
        if (channels[ch].prefix != "")
            if (fprintf(file, "[%s]  ", channels[ch].prefix.c_str()) < 0) {
                throw Error("Writing log mesage (Logger::_write#channels)");
            }
	
        // now write format string
        if (vfprintf(file, fmt, argp) < 0) {
            throw Error("Writing log mesage (Logger::_write#vfprintf)");
        }
	
        // and write NL afterwards
        if (nl) {
            fprintf(file, "\n");
        }    

    } catch (Error &e) {
        throw e;
    }
}


/* -------------------- setLogLevel -------------------- */

void Logger::setLogLevel( int level )
{
    logLevel = L_MAXLEVEL; // enable all logs for the duration of this function

    log(ch, "setting log level to %d", level);

    if (level < 0) {
        log(L_WARN, ch, "setting log level below zero disables all logging");
    } else {
        if (level > L_MAXLEVEL) {
            level = L_MAXLEVEL;
        }

        string msg = "active logs: ";

        for (int i=0; i <= level; i++) {
            msg = msg + LogLevel[i] + " ";
        }

        if (level < L_MAXLEVEL) {
            msg += ", inactive logs: ";

            for (int i = level + 1; i <= L_MAXLEVEL; i++) {
                msg = msg + LogLevel[i] + " ";
            }
        }
        log(ch, msg.c_str());
    }

    logLevel = level;
}


/* ------------------------- dump ------------------------- */

void Logger::dump(ostream &os)
{
    os << "Logger dump :" << endl;

    for (unsigned int i = 0; i<numChannels; i++) {
        os << i << "\t->\t" << "[" << channels[i].prefix << "]\t->\t\"" <<
          logfiles[channels[i].file_id].fname << "\"" << endl;
    }
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<<(ostream &os, Logger &ml)
{
    ml.dump(os);
    return os;
}
