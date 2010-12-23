
/*! \file   netmate/ExportModule.h

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

    $Id: ExportModule.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _EXPORTMODULE_H_
#define _EXPORTMODULE_H_


#include "stdincpp.h"
#include "Logger.h"
#include "Module.h"
#include "ConfigManager.h"
#include "ExportModuleInterface.h"
#include "EventScheduler.h"


/*! \short   container class that stores information about an export module
  
    container class - stores information about an export module such as 
                      name, libhandle and reference counter
*/

class ExportModule : public Module
{
  private:

    static Logger *s_log; //!< link to the global Logger instance
    static int    s_ch;   //!< logging channel number for ExportModule class

    //!< struct of functions pointers for dynamically loaded library
    ExportModuleInterface_t *funcList; 

    //!< number of active timers
    unsigned int timersActive;

  public:

    //! return block of function pointers (API) for a module
    inline ExportModuleInterface_t *getAPI()  
    { 
        return funcList; 
    }

    //! return a module's version
    virtual int getVersion()     
    { 
        return funcList->version; 
    }

    
    virtual string getModuleType() 
    { 
        return "data export"; 
    }


    /*! \short   construct and initialize an ExportModule object

        take the library handle of an export module and retrieve all the
        other information (name, uid, function list) via this handle

        \arg \c conf - the global available configuration manager
        \arg \c libname - name of the export module 
        \arg \c filename - name of module including path and extension
        \arg \c libhandle - system handle for loaded library (see dlopen)
    */
    ExportModule( ConfigManager *conf, string libName,
        		  string libFileName, libHandle_t libhandle );

    //! destroy an ExportModule object
    ~ExportModule();
    
    /*! \short  add timers of this module as new events to the event 
                scheduler component (but only once per module!)
        \arg \c evs - the meter's event scheduler component
    */
    void addTimerEvents( EventScheduler &evs );
    
    
    //! dump an ExportModule object
    void dump( ostream &os );

};


//! overload for <<, so that an ExportModule object can be thrown into an ostream
ostream& operator<< ( ostream &os, ExportModule &obj );


#endif // _EXPORTMODULE_H_
