
/*! \file   netmate/ProcModule.h

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
    container class for packet proc module

    $Id: ProcModule.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _PROCMODULE_H_
#define _PROCMODULE_H_


#include "stdincpp.h"
#include "ProcModuleInterface.h"
#include "PerfTimer.h"
#include "Logger.h"
#include "Module.h"
#include "ExportList.h"



/*! \short   container class that stores information about an evaluation module
  
    container class - stores information about an evaluation module such as 
    name, uid, function list, libhandle and reference counter
*/

class ProcModule : public Module
{
  private:

    //! static module info string, generated once upon object costruction
    string moduleInfoXML;  

    static Logger *s_log; //!< link to the global Logger instance
    static int    s_ch;   //!< logging channel number for ExportModule class

    static string INFOLABEL[]; // FIXME check and document

    //! struct of functions pointers for library
    ProcModuleInterface_t *funcList;

    //!< runtime type information list
    typeInfo_t *typeInfo;

    //!< parsed typeInfo_t structure as ExportList object
    ExportList *expList;

    /*! \short   convert export attribute list into internal format

        parses the attrib list supplied by the module and stores and internal
        representation of that structure for later use during daat export

        \pre the module has been loaded and typeInfo_t* typeInfo has a valid value
    */
    void parseAttribList();

  public:

    typeInfo_t *getTypeInfo() 
    { 
        return typeInfo; 
    }

    ProcModuleInterface_t *getAPI()  
    { 
        return funcList; 
    }

    virtual string getModuleType() 
    { 
        return "packet processing"; 
    }

    virtual int getVersion()     
    { 
        return funcList->version; 
    }

    char* getModuleInfo( int i ) 
    { 
        return funcList->getModuleInfo(i); 
    }

    inline ExportList *getExportLists() 
    { 
        return expList; 
    }

    /*! \short   construct and initialize a ProcModule object

        take the library handle of an evaluation module and retrieve all the
        other information (name, uid, function list) via this handle

        \arg \c libname - name of the evaluation module 
        \arg \c filename - name of module including path and extension
        \arg \c libhandle - system handle for loaded library (see dlopen)
    */
    ProcModule( string libname, string libfile, libHandle_t libhandle );

    //! destroy a ProcModule object
    ~ProcModule();

    /*! \short   generate textual module description in XML format from 
                 text data stored in module itself, writes moduleInfoXML
    */
    string makeModuleInfoXML();

    //! return textual module description in XML format
    string getModuleInfoXML() 
    { 
        return moduleInfoXML; 
    }

    //!  return a textual representation of this modules export info structure 
    string getTypeInfoText();

    //!  get text label for type property i
    static string infoLabel( int i )
    {
        if (i < 0 || i >= I_NUMINFOS) {
            return "";
        } else {
            return INFOLABEL[i];
        }
    }

    //! get text label for type property i
    static string typeLabel( int i );


    //! dump a ProcModule object
    void dump( ostream &os );

};


//! overload for <<, so that a ProcModule object can be thrown into an ostream
ostream& operator<< ( ostream &os, ProcModule &obj );


#endif // _PROCMODULE_H_
