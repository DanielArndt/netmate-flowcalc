
/*! \file   ModuleLoader.h

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
    load/unload packet proc and export modules

    $Id: ModuleLoader.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _MODULELOADER_H_
#define _MODULELOADER_H_


#include "stdincpp.h"
#include "ProcModuleInterface.h"
#include "ConfigManager.h"
#include "Module.h"


/*! \short   struct that stores data for a loaded dynamic library

  this struct is used by a ModuleLoader object to store information
  about a dynamically loaded library
 */
struct ActionModule_t {
    //!< library handle from dlopen call
    void *libhandle; 
    
    //!< struct of functions pointers for library
    ProcModuleInterface_t *funcList; 

    //!< runtime type information list
    typeInfo_t *typeInfo; 

    string name; //!< name supplied by module
    int    uid;  //!< unique id number supplied by module
    int    ref;  //!< reference (link) counter
};

//! module list
typedef map<string, Module*>            moduleList_t;
typedef map<string, Module*>::iterator  moduleListIter_t;


/*! \short   load dynamic libraries and extract function pointers
  
  the ModuleLoader class allows requesting of a dynamic library that is to
  be used. That Library will be loaded and it's functions made available
  to the caller using the ModuleLoader object.
 */

class ModuleLoader
{
  private:

    //! name of dir that contains the librari(es)
    string basepath;

     //! list storing data about loaded modules
    moduleList_t modules;

    //! link to the meter's configmanager
    ConfigManager *conf;

    //! link to the globally used logger
    static Logger *s_log;
    
    //! number of ModuleLoader objects 'ever' created
    static int s_loaders;
    
    //! logging channel number used by an instance of this class
    int ch;

  public:

    /*! \short   construct and initialize a ModuleLoader object

        \arg \c basedir - directory that contains libraries that can by
                          loaded with subsequent calls to getModule()
        \arg \c modules - names of metric modules separated by " "
    */
    ModuleLoader( ConfigManager *cnf, string basedir = "./",
        		  string modules = "", string channelPrefix = "" );

    /*! \short   destroy a ModuleLoader object

        this will close all dynamic libraries used by this ModuleLoader (or at
        least decrement their reference counter if they are used otherwise)
    */
    ~ModuleLoader();


    /*! \short   load a metric module and make its functions available

        tries to open a module and return a list of functions from it. The
        library has to include 'ActionModule.h' and needs to implement all
        functions given in 'ProcModuleInterface.h'

        \arg \c modname - name of the module
        \arg \c preload - load module instantly (not only on demand) if ==1
        \returns a reference to a struct with pointers to all the functions of the module
        \throws Error in case the module is not available
    */
    Module *loadModule( string libname, int preload );


    /*! \short get the info block for a loaded metric module

        looks for a metric module with the specified name and returns
        an info block for this module 

        \arg \c libname - name of the library
        \returns a reference to a struct with pointers to all the functions of the Action Module
        \throws MyErr in case the module is not available
    */
    Module *getModule( string libname );

    //! get xml description of module
    string getModuleInfoXML( string modname );

    /*! \short   decrement reference count to a loaded dynamic lobrary

        reduce the link count of the given library by one. If the link 
        count reaches zero by doing so, the library is unloaded.

        \arg \c minfo - pointer to module
    */
    int releaseModule( Module *minfo );

    /*! \short   get the runtime type information for a module

        \arg \c  modname - name of the evaluation module
        \returns the type information for the data exported by the requested 
                 evaluation module
    */
    typeInfo_t* getTypeInfo( string modname );

    /* \short   get version number from module

      \arg \c modname - name of the evaluation module
      \returns the version number stored inside the module
    */
    int getVersion( string modname );

    /*! \short   print export data type information

      \arg \c modname - name of the proc module
     */
    void writeTypeInfo( string modname );

    /*! \short fetch magic number from module lib file
     */
    int fetchMagic( libHandle_t libHandle );

    //! FIXME missing documentation
    string getInfo();

    /*! \short   dump a ModuleLoader object

        display names of loaded libraries and the number of times 
        they have been requested
    */
    void dump( ostream &os );

    //! return number of currently loaded modules
    int numModules() 
    { 
        return modules.size(); 
    }
};


//! overload for <<, so that a ModuleLoader object can be thrown into an iostream
ostream& operator<< ( ostream &os, ModuleLoader &ml );


#endif // _MODULELOADER_H_
