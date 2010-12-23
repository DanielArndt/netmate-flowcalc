
/*! \file  ProcModuleInterface.h

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
    interface definition for metric computation modules
    (that are used by the PacketProcessor component)

    $Id: ProcModuleInterface.h 748 2009-09-10 02:54:03Z szander $

*/

#ifndef __PROCMODULEINTERFACE_H
#define __PROCMODULEINTERFACE_H

#include "stdinc.h"
#include "metadata.h"

// configuration parameter passed to the module
typedef struct {
    char *name; 
    char *value;
} configParam_t;


//! short   the magic number that will be embedded into every action module
#define PROC_MAGIC   ('N'<<24 | 'M'<<16 | '_'<<8 | 'P')


/*! 
    DataType_e identifiers are used within the runtime type 
    information struct inside each ProcModule
*/
enum DataType_e { 
    INVALID1 = -1, 
    EXPORTEND = 0, 
    LIST, 
    LISTEND, 
    CHAR, 
    INT8, 
    INT16, 
    INT32, 
    INT64,
    UINT8, 
    UINT16, 
    UINT32, 
    UINT64,
    STRING, 
    BINARY, 
    IPV4ADDR, 
    IPV6ADDR,
    FLOAT, 
    DOUBLE, 
    INVALID2 
};


/*! run time type information array */
typedef struct {
    enum DataType_e type;
    char *name;
} typeInfo_t;


#define LIST_END       { LISTEND, "LEnd" }
#define EXPORT_END     { EXPORTEND, "EEnd" }


/*! parameter values used for a call to 'getModuleInfo' */
enum ActionInfoNumbers_e {
    /* module function attributes */
    I_MODNAME = 0, 
    I_VERSION,
    I_CREATED,
    I_MODIFIED,
    I_BRIEF,
    I_VERBOSE,
    I_HTMLDOCS, /* new */
    I_PARAMS,
    I_RESULTS,
    /* module author attributes */
    I_AUTHOR, 
    I_AFFILI,
    I_EMAIL,
    I_HOMEPAGE,
    I_NUMINFOS
};


typedef struct {
    unsigned int id;
    unsigned int ival_msec;
    unsigned int flags;
} timers_t;


typedef enum {
    TM_NONE = 0, TM_RECURRING = 1, TM_ALIGNED = 2, TM_END = 4
    // , TM_NEXTFLAG = 8, TM_NEXTNEXTFLAG = 16, 32, 64 etc pp.
} timerFlags_e;

#define TIMER_END  { (unsigned int)-1, 0 /*ival==0 marks list end*/, TM_END }



typedef int (*proc_timeout_func_t)( int timerID, void *flowdata );



/*! \short   initialize the action module upon loading 
   \returns 0 - on success, <0 - else 
*/
int initModule();


/*! \short   cleanup action module structures before it is unloaded 
   \returns 0 - on success, <0 - else 
*/
int destroyModule();


/*! \short   return size of (initial) flow data record for this module

    A piece of memory with the number of bytes returned by this function
    will be allocated initially for a new rule that makes use of this action.
    \returns size required for storage of flow data per rule
*/
//  int getFlowRecSize();
//  deprecated!!!  removed from API 2003/04/14


/*! \short   initialize flow data record for a rule

    The freshly allocated flow data record for a measurement task (for
    one module) is initialized here (e.g. set counters to zero) and the 
    module parameter string can be parsed and checked

    \arg \c  params - module parameter text from inside '( )'
    \arg \c  flowdata  - place for action module specific data from flow table
    \returns 0 - on success (parameters are valid), <0 - else
*/
int initFlowRec( configParam_t *params, void **flowdata );


/*! \short   get list of default timers for this proc module
    \arg \c  flowdata  - place for action module specific data from flow table
    \returns   list of timer structs
*/
timers_t* getTimers( void *flowdata );


/*! \short   dismantle flow data record for a rule

    attention: do NOT free this slice of memory itself
    \arg \c  flowdata  - place of action module specific data from flow table
    \returns 0 - on success, <0 - else
*/
int destroyFlowRec( void *flowdata );


/*! \short   reset flow data record for a rule

    \arg \c  flowdata  - place of action module specific data from flow table
    \returns 0 - on success, <0 - else
*/
int resetFlowRec( void *flowdata );


/*! \short   analyse a datagram and update data in the flow table

    \arg \c  packet    - packet data
    \arg \c  meta      - meta data from Classifier, \sa metaData_t
    \arg \c  flowdata  - rule specific data from flow table
    \returns >=0 - on success, 
    \returns   1 - instant retrieval of measurement data required
    \returns  <0 - else
*/
int processPacket( char *packet, metaData_t *meta, void *flowdata );  


/*! \short   save flow- and action- specific in TLV-like format

    \arg \c  exportdata - store location of export data here
    \arg \c  len        - save length of formatted flow data here
    \arg \c  flowdata   - rule specific data from flow table
    \returns 0 - on success, <0 - else
*/
int exportData( void **exportdata, int *len, void *flowdata );


/*! \short  check module parameters for a set of rules for correctness

    This function is called by the Meter to check if the parameters supplied by 
    a set of rules, that are to be installed, are acceptable according to the
    specification of a specific module. The module is responsible for reporting
    errors, either syntactically, semantically or in the supplied values.

    \arg \c rules - a list of data sets with rules to be installed
    \returns 0 - on success (parameters are valid), <0 - else
*/
/*int checkRules( ruleDB_t *rules );*/


/*! \short   return the runtime type information structure
  
    \returns the list of type information for the data structure
             exported by this module
*/
typeInfo_t* getTypeInfo();


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
    \returns - pointer to a '\0' string if no information is available \n
    \returns - NULL for index after last stored info string
*/
char* getModuleInfo( int i );


/*! \short   this function is called if the module supports a timeout callback function every x seconds and its invokation is configured to make use of the timeout feature
 */
int timeout( int timerID, void *flowdata );


/*! \short   return error message for last failed function

    \arg \c    - error number (return value from failed function)
    \returns 0 - textual description of error for logging purposes
*/
char* getErrorMsg( int code );



/*! \short   definition of interface struct for Action Modules 

  this structure contains pointers to all functions of this module
  which are part of the Action Module API. It will be automatically 
  set for an Action Module upon compilation (don't forget to include
  ActionModule.h into every module!)
*/

typedef struct {

    int version;

    int (*initModule)();
    int (*destroyModule)();

    /*    int (*getFlowRecSize)(); -- deprecated -- */
    int (*initFlowRec)( configParam_t *params, void **flowdata );
    timers_t* (*getTimers)( void *flowdata );
    int (*destroyFlowRec)( void *flowdata );

    int (*resetFlowRec)( void *flowdata );
    int (*timeout)( int timerID, void *flowdata );

    int (*processPacket)( char *packet, metaData_t *meta, void *flowdata );
    int (*exportData)( void **exportdata, int *len, void *flowdata );

    char* (*getModuleInfo)(int i);
    char* (*getErrorMsg)( int code );

    typeInfo_t* (*getTypeInfo)();

} ProcModuleInterface_t;

#endif /* __PROCMODULEINTERFACE_H */
