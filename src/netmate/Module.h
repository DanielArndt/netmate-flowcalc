
/*! \file   Module.h

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
    this class represents the base class for 
    packet processing modules and data export modules

    $Id: Module.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _MODULE_H_
#define _MODULE_H_


#include "stdincpp.h"
#include "Logger.h"


//! FIXME missing documentation
typedef void* libHandle_t;


/*! \short   super class for the loadable modules (for processing and export)
  
super class - stores information about an loadbale module such as 
name, libhandle and reference counter
*/

class Module
{
  private:

    Logger *log;
    int ch;

    libHandle_t libHandle;      //!< library handle from dlopen call
    string modName;             //!< short name of module (must be unique!)
    string fileName;            //!< file name of module (including path)
    string ownName;             //!< module name supplied by module itself
    int    refs;                //!< reference (link) counter
    unsigned long long calls;   //!< number of invocations of this module

  public:

    //! increase module usage counter
    int link()       
    { 
        return ++refs; 
    }

    //! decrease module usage counter
    int unlink()     
    { 
        return (refs==0) ? 0 : --refs; 
    }

    libHandle_t getLib()         
    { 
        return libHandle; 
    }

    string getModName()     
    { 
        return modName; 
    }

    string getFileName()    
    { 
        return fileName; 
    }

    string getOwnName()     
    { 
        return ownName; 
    }

    void setOwnName(string name) 
    { 
        ownName = name; 
    }

    int getRefs()        
    { 
        return refs; 
    }

    virtual int getVersion() 
    { 
        return -1; 
    }
    
    virtual string getModuleType() 
    { 
        return "basic"; 
    }

 
    /*! \short   construct and initialize a Module object

        take the library handle of an evaluation module and retrieve all the
        other information (name, uid, function list) via this handle

        \arg \c libname   - name of the evaluation module 
        \arg \c filename  - name of module including path and extension
        \arg \c libhandle - system handle for loaded library (see dlopen)
    */
    Module( string libname, string filename, libHandle_t libhandle );
    

    //! destroy a Module object, to be overloaded
    virtual ~Module();

    /*! \short  load API (block of module function references)
      
        this function queries the module for the block of module functions
        ( a struct storing function pointers to the module's methods )
    */
    void *loadAPI( string apiName );

    /*! \short  check magic number in module
        tries to read magic number from module lib file and check for correctness
        \throws Error in case magic number is not present or is wrong
    */
    void checkMagic( int magic );

    //! dump a Module object
    void dump( ostream &os );

};


//! overload for <<, so that a Module object can be thrown into an ostream
ostream& operator<< ( ostream &os, Module &obj );


#endif // _MODULE_H_
