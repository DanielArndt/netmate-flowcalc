
/*! \file  ExportModuleInterface.h

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
    interface definition for netmate export modules
    (that are used by the Exporter component)

    $Id: ExportModuleInterface.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef __EXPORTMODULEINTERFACE_H
#define __EXPORTMODULEINTERFACE_H


#include "stdincpp.h"
#include "Module.h"
#include "Rule.h"
#include "ConfigManager.h"
#include "FlowRecord.h"


typedef int (*export_timeout_func_t)( int timerID );


//! short   the magic number that will be embedded into every action module
#define EXPORT_MAGIC   ('N'<<24 | 'M'<<16 | '_'<<8 | 'E')


/*! \short   initialize the export module upon loading
    \arg \c config - the current meter configuration (the module
                     may extract its own settings from the appropriate 
                     sub-section of the loaded meter configuration)
    \returns 0 - on success, <0 - else 
*/
int initModule( ConfigManager *confMan );


/*! \short   reset any static data in the export module
             (e.g. database connection, allocated output buffers)
    \returns 0 - on success, <0 - else 
*/
int resetModule();


/*! \short   unload an export module (invoked when module not used any more)
   \returns 0 - on success, <0 - else 
*/
int destroyModule();


/*! \short  initialise an export record for a new installed task
   \returns 0 - on success, <0 - else 
*/
int initExportRec( configItemList_t conf, void **expRecord );


/*! \short   get list of default timers for this proc module
    \returns   list of timer structs
*/
timers_t* getTimers();


/*! \short  cleanup an export record for a task that is being removed
   \returns 0 - on success, <0 - else 
*/
int destroyExportRec( void *expRecord );


/*! \short react on previously set timeout event
    \arg \c id - event id number as specified when setting the timeout
*/
int timeout( int id );


/*! short export collected data provided by the metric modules used by a given rule
    \arg \c frec - collection of data exported by the proc module(s) for the given rule
    \returns 0 - on success, <0 - else 
*/

int exportData( FlowRecord *frec, void *expData );


/*! \short   provide textual information about this action module

    A string is returned that describes one property (e.g.author) of the
    action module in detail. \n A list of common properties follows in the
    argument list

    \arg \c I_NAME    - name of the action module
    \arg \c I_UID     - return unique module id number (as string)
    \arg \c I_BRIEF   - brief description of the action module functionality
    \arg \c I_AUTHOR  - name/e-mail of the author of this module
    \arg \c I_CREATE  - info about module creation (usually date and similar)
    \arg \c I_DETAIL  - detailed module functionality description
    \arg \c I_PARAM   - description of parameter(s) of module
    \arg \c I_RESULT  - information about nature and format of measurement 
                        results for this module
    \arg \c I_RESERV  - reserved info entry
    \arg \c I_USER    - entry open for free use
    \arg \c I_USER+1  - entry open for free use
    \arg \c I_USER+2  - entry open for free use
    \arg \c I_USER+n  - must return NULL

    \returns - a string which contains textual information about a 
               property of this action module \n
    \returns - pointer to an empty string if no information is available \n
    \returns - NULL for index after last stored info string
*/
char* getModuleInfo( int i );


/*! \short   return error message for last failed function

    \arg \c    - error number (return value from failed function)
    \returns 0 - textual description of error for logging purposes
*/
char* getErrorMsg( int code );



/*! \short   definition of interface struct for export modules

  this structure contains pointers to all functions of an module
  which are part of the export module API. It will be automatically 
  filled for an export module upon compilation 
  (make sure to include ExportModule.h into your export module!)
*/
typedef struct {
  
    int version;
  
    int (*initModule)( ConfigManager *confMan );
    int (*destroyModule)();

    int (*resetModule)();
    int (*timeout)(int);

    int (*initExportRec)( configItemList_t conf, void **expRecord );
    timers_t* (*getTimers)();
    int (*destroyExportRec)( void *expRecord );

    int (*exportData)( FlowRecord*, void* );

    char* (*getModuleInfo)(int i);
    char* (*getErrorMsg)( int );

} ExportModuleInterface_t;


#endif /* __EXPORTMODULEINTERFACE_H */
