
/* \file ConfigManager.cc

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
    command line and config file configuration db 

    $Id: ConfigManager.cc 748 2009-09-10 02:54:03Z szander $

*/

#include "Error.h"
#include "Logger.h"
#include "ConfigManager.h"


/* ------------------------- ConfigManager ------------------------- */

ConfigManager::ConfigManager(string filename, string binary)
{
    log = Logger::getInstance();
    ch = log->createChannel("ConfigManager");
#ifdef DEBUG
    log->dlog(ch, "Starting");
#endif

    // two separate lines -> support for old g++
    auto_ptr<ConfigParser> _parser(new ConfigParser(filename, binary));   
    parser = _parser;

    parser->parse(&list, &ad_list);
}


/* ------------------------- ~ConfigManager ------------------------- */

ConfigManager::~ConfigManager()
{
#ifdef DEBUG
    log->dlog(ch, "Shutdown");
#endif
}


/* ------------------------- reread ------------------------- */

void ConfigManager::reread(const string filename)
{
    ConfigParser *_cp = parser.release();
    saveDelete(_cp);
    
    auto_ptr<ConfigParser> cp(new ConfigParser(filename));
    parser = cp;

    parser->parse(&list, &ad_list);
}


/* ------------------------- getValue ------------------------- */

string ConfigManager::getValue(string name, string group, string module)
{
    configItemListIter_t iter;

    for (iter = list.begin(); iter != list.end(); iter++) {
        if ((name == iter->name) && (group.empty() || group == iter->group) &&
            (module.empty() || module == iter->module)) {
            return iter->value;
        }
    }
    return "";
}


configItem_t *ConfigManager::getItem(string name, string group, string module)
{
    configItemListIter_t iter;

    for (iter = list.begin(); iter != list.end(); iter++) {
        if ((name == iter->name) && (group.empty() || group == iter->group) &&
            (module.empty() || module == iter->module)) {
            return &*iter;
        }
    }
    return NULL;
}


/* -------------------- getItems -------------------- */

configItemList_t ConfigManager::getItems(string group, string module)
{
    configItemListIter_t iter;
    configItemList_t ret_list;

    for (iter = list.begin(); iter != list.end(); iter++) {
        if ((group.empty() || group == iter->group) &&
            (module.empty() || module == iter->module)) {
            ret_list.push_back(*iter);
        }
    }
    
    return ret_list;
}


/* ------------------------- isConfigured ------------------------- */

int ConfigManager::isConfigured( string name, string group, string module )
{
    return (getValue(name,group,module) != "");
}


/* ------------------------- isTrue ------------------------- */

int ConfigManager::isTrue( string name, string group, string module )
{
    string value = getValue(name,group,module);

    return (value == "1" || value == "true" || value == "yes");
}


/* ------------------------- isFalse ------------------------- */

int ConfigManager::isFalse( string name, string group, string module )
{
    string value = getValue(name,group,module);

    return (value.empty() ||
            value == "0" ||
            value == "false" ||
            value == "no");
}


/* ------------------------- setItem ------------------------- */

void ConfigManager::setItem( string name, string value, string group, string module )
{
    configItemListIter_t iter;
    configItem_t new_item;

    // lookup existing item and change it
    for (iter = list.begin(); iter != list.end(); iter++) {
        if ((name == iter->name) && (group.empty() || group == iter->group) &&
	     (module.empty() || module == iter->module)) {
            iter->value = value;
            iter->group = group;
            iter->module = module;
            return;
        }
    }

    // insert new item
    new_item.name = name;
    new_item.value = value;
    new_item.group = group;
    new_item.module = module;
    new_item.type = "String";
    list.push_back(new_item);
}


void ConfigManager::mergeArgs(commandLineArgList_t *args, commandLineArgValList_t *argVals)
{
    commandLineArgValListIter_t iter;
    commandLineArgListIter_t iter2;

    for (iter = argVals->begin(); iter != argVals->end(); iter++) {
        iter2 = args->find(iter->first);
        setItem(iter2->second.name, iter->second, iter2->second.group);
#ifdef DEBUG
        log->dlog(ch, "%s.%s = %s", iter2->second.group.c_str(), 
                  iter2->second.name.c_str(), iter->second.c_str());
#endif
    }
}


/* -------------------- getParamList -------------------- */

configParam_t *ConfigManager::getParamList( configItemList_t &list )
{
    configItemListIter_t iter;
    configParam_t *params = new configParam_t[list.size() + 1];
    int i = 0;

    for (iter = list.begin(); iter != list.end(); iter++) {
        params[i].name = (char* ) iter->name.c_str();
        params[i].value = (char *) iter->value.c_str();
        i++;
    }
    
    params[i].name = NULL;
    params[i].value = NULL;

    return params;
}


/* ------------------------- dump ------------------------- */

void ConfigManager::dump( ostream &os )
{
    configItemListIter_t iter;

    os << "ConfigManager dump" << endl << "configured items:" << endl;

    for (iter = list.begin(); iter != list.end(); iter++ )
	os << iter->group << "."
	   << iter->module << "."
	   << iter->name << " = " << iter->value << endl;
}


/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, ConfigManager &cm )
{
    cm.dump(os);
    return os;
}
