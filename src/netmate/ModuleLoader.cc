
/*!\file   ModuleLoader.cc

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

    $Id: ModuleLoader.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "Error.h"
#include "Logger.h"
#include "ModuleLoader.h"
#include "Module.h"
#include "ProcModule.h"
#include "ProcModuleInterface.h"
#include "ExportModule.h"
#include "ExportModuleInterface.h"


/* ------------------------- init static ------------------------- */

Logger *ModuleLoader::s_log = NULL;
int     ModuleLoader::s_loaders = 0;


/* ------------------------- ModuleLoader ------------------------- */

ModuleLoader::ModuleLoader( ConfigManager *cnf, string basedir,
                            string modules, string channelPrefix )
    : basepath(basedir), conf(cnf)

{
    struct stat statbuf;

    s_log = Logger::getInstance();
    ch = s_log->createChannel(channelPrefix + "ModuleLoader");
#ifdef DEBUG
    s_log->dlog(ch, "Creating");
#endif
 
    // add '/' if dir does not end in one already
    if (basepath[basepath.size()-1] != '/') {
        basepath += "/";
    }
    s_log->log(ch, "using moduledir: %s", basepath.c_str());

    // test if specified dir does exist
    if (stat(basepath.c_str(), &statbuf) < 0) {
        throw Error("ModuleLoader: invalid basepath '%s': %s", 
                    basepath.c_str(), strerror(errno));
    }

    if (modules != "") {
        int n = 0, n2 = 0;
 
#ifdef DEBUG
        s_log->dlog(ch, "specified modules to be preloaded: '%s'",modules.c_str());
#endif	

        while ((n2 = modules.find_first_of(" \t", n)) > 0) {
            string mod = modules.substr(n, n2-n);
#ifdef DEBUG
            s_log->log(ch, "going to preload module: '%s'", mod.c_str());
#endif	
            loadModule(mod.c_str(), 1);

            // skip additional ws
            n = modules.find_first_not_of(" \t", n2);
        }
        if ((n > 0) && (n < (int) modules.length())) {
            string mod = modules.substr(n, modules.length()-n);
#ifdef DEBUG
            s_log->log(ch, "going to preload module: '%s'", mod.c_str());
#endif	
            loadModule(mod.c_str(), 1);
        }
    }
}


/* ------------------------- ~ModuleLoader ------------------------- */

ModuleLoader::~ModuleLoader()
{
    moduleListIter_t iter;

#ifdef DEBUG
    s_log->dlog(ch, "Shutdown");
#endif

    // close each dymamic library we have opened so far
    for (iter = modules.begin(); iter != modules.end(); iter++) {	
        s_log->log(ch, "unloading module '%s'", (iter->first).c_str());
        saveDelete(iter->second); // deleting a Module
    }

}


/* ------------------------- getTypeInfo ------------------------- */

typeInfo_t* ModuleLoader::getTypeInfo( string modname )
{
    // search for module in list of already loaded modules
    moduleListIter_t iter = modules.find(modname);

    if (iter == modules.end()) {
        return NULL;
    }

    // typeinfo information only exists for packet processing modules
    ProcModule *pmod = dynamic_cast<ProcModule*>(iter->second);

    if (pmod == NULL) {
        return NULL;
    }

    return pmod->getTypeInfo();
}

/* ------------------------- getVersion ------------------------- */

int ModuleLoader::getVersion( string modname )
{
    moduleListIter_t iter;
    
    // search for module in list of already loaded modules
    if ((iter = modules.find(modname)) != modules.end()) {
        return (iter->second)->getVersion();
    } else {
        return -1;
    }
}


/* ------------------------- getModule ------------------------- */

Module *ModuleLoader::getModule( string modname )
{
    moduleListIter_t iter;

    if (modname.empty()) {
        throw Error("no module name specified");
    }
    
    // search for library in list of already used libraries
    if ((iter = modules.find(modname)) != modules.end()) {
        (iter->second)->link(); // increment reference count
        return iter->second; // return Module object
    }

    // no such module in the list of currently available modules ->try to load it
    return loadModule(modname, 0);
}


/* -------------------- getModuleInfoXML -------------------- */

string ModuleLoader::getModuleInfoXML( string modname )
{
    Module *mod = getModule( modname );

    if (mod == NULL) {
        return "";
    }
    
    ProcModule *pmod = dynamic_cast<ProcModule*>(mod);
    
    if (pmod == NULL) {
        return "";
    }
    
    return pmod->getModuleInfoXML();
}


/* ------------------------- loadModule ------------------------- */

Module *ModuleLoader::loadModule( string libname, int preload )
{
    Module *module = NULL;
    string filename, path, ext;
    libHandle_t libhandle = NULL;
    
    if (libname.empty()) {
        return NULL;
    }
    
    try {
#ifdef DEBUG
        s_log->log(ch, "trying to load module '%s'", libname.c_str());
#endif
        // if libname has relative path (or none) then use basepath
        if (libname[0] != '/') {
            path = basepath;
        }

        // use '.so' as postfix if it is not yet there
        if (libname.substr(libname.size()-3,3) != ".so") {
            ext = ".so";
        }
	
        // construct filename of module including path and extension
        filename = path + libname + ext;
	
        // try to load the library module
        libhandle = dlopen(filename.c_str(), RTLD_LAZY);
        if (libhandle == NULL) {
            // try to load without .so extension (cater for libtool bug)
            filename = path + libname;
            libhandle = dlopen(filename.c_str(), RTLD_LAZY);
            if (libhandle == NULL) {
                throw Error("cannot load module '%s': %s", 
                            libname.c_str(), (char *) dlerror());
            }
        }
        
        // dlopen succeeded, now check what module we have there 
        // everything went fine up to now -> create new module info entry
        {
            int magic = fetchMagic(libhandle);
            
            if (magic == PROC_MAGIC) {
                module = new ProcModule(libname, filename, libhandle);
            } else if (magic == EXPORT_MAGIC) {
                module = new ExportModule(conf, libname, filename, 
                                          libhandle);
            } else {
                throw Error("unsupported module type (unknown magic number)");
            }
        }
	
        module->link();             // increase sue counter in this module
        modules[libname] = module;  // and finally store the module
        
        s_log->log(ch, "loaded %s module '%s'",
		   module->getModuleType().c_str(), libname.c_str());
        
        return module;
        
    } catch (Error &e) {
        if (module) {
            saveDelete(module);
        }
        if (libhandle) {
            dlclose(libhandle);
        }
        throw e;
    }
}


/* ------------------------- releaseModule ------------------------- */

int ModuleLoader::releaseModule( Module *minfo )
{
    moduleListIter_t iter;
    
    iter = modules.find(minfo->getModName());

    if (iter != modules.end()) {
        if (iter->second->unlink() == 0) {
            s_log->log(ch, "unloading unused module '%s'",
                       (iter->first).c_str());
            saveDelete(iter->second);
            modules.erase(iter);
        }
    }
 
    return 0;
}


/* ------------------------- writeTypeInfo ------------------------- */

void ModuleLoader::writeTypeInfo( string modname )
{
    // search for module in list of already loaded modules
    moduleListIter_t iter = modules.find(modname);
    if (iter == modules.end()) {
        return;
    }

    // check if it's a packet processing module
    ProcModule *pmod = dynamic_cast<ProcModule*>(iter->second);
    if (pmod == NULL) {
        return;
    }
    
    cout << pmod->getTypeInfoText() << endl;
}


/* ------------------------- dump ------------------------- */

void ModuleLoader::dump( ostream &os )
{

    os << "ModuleLoader dump : " << endl;
    os << getInfo();
}

string ModuleLoader::getInfo()
{
    ostringstream s;
    moduleListIter_t iter;

    for (iter = modules.begin(); iter != modules.end(); iter++) {
        s << *(iter->second);
    }

    return s.str();
}


/* ------------------------- fetchMagic ------------------------- */

int ModuleLoader::fetchMagic( libHandle_t libHandle )
{
    // test for magic number in loaded module
    int *magic = (int *)dlsym(libHandle, "magic");

    if (magic == NULL) {
        throw Error("invalid module - no magic number present");
    } else {
        return *magic;
    }
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, ModuleLoader &ml )
{
    ml.dump(os);
    return os;
}
