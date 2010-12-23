
/*! \file ConfigManager.h

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

    $Id: ConfigManager.h 748 2009-09-10 02:54:03Z szander $
*/

#ifndef _CONFIGMANAGER_H_
#define _CONFIGMANAGER_H_


#include "stdincpp.h"
#include "libxml/parser.h"
#include "Logger.h"
#include "ConfigParser.h"
#include "CommandLineArgs.h"
#include "ProcModuleInterface.h"


/*
    provides functionality to read xml config files 
    command line args can be merged with (overrule) config file defs
    config options, groups and module names are case-sensitive
*/

class ConfigManager
{
  private:

    configItemList_t list;    //!< list of configuration items
    configADList_t   ad_list; //!< list of access allow/deny items

    int ch;      //!< logging channel number used by objects of this class
    Logger *log; //!< link to global logger object

    auto_ptr<ConfigParser> parser; //!< link to configuration parser

  public:

    /*! creates and initializes a ConfigManager
      \arg \c filename - name of config file
      \arg \c binary - location of binary file, i.e. argv[0]
    */
    ConfigManager(string filename, string binary);

    //! destroys a ConfigManager object
    ~ConfigManager();

    /*! \short   reads config strings from file into object's string table

        if used more than once, new items and values will be added
        to those already set. Old items may be overwritten.

        \arg \c fname - name of file to read configuration from
    */
    void reread(const string fname);


    /*! \short   get configured item's value

        \arg \c  name - name of the item
        \returns  the value for the queried item or "" if item is not set
    */
    string getValue(string name, string group = "", string module = "");

    /*! \short get a pointer to an item definition

        \arg \c  name - name of the item
        \returns  a pointer to the item definition or NULL if item is not set
    */
    configItem_t *getItem(string name, string group = "", string module = "");

    //! get all items for the specified module
    configItemList_t getItems(string group, string module = "");

    /*! \short   query if item is configured at all

        \arg \c  name - name of queried item
        \returns  a value !=0 if item has been configured, else 0
    */
    int isConfigured(string name, string group = "", string module = "");

    /*! \short   check if item is configured to be true

        this will lookup the given item in the list of configured items
        and return true if the item is either set to be '1', 'true' or 'yes'. 

        \arg \c   name - name of queried item
        \returns  a value !=0 if item is configured to one of '1','true','yes' or ''
    */      
    int isTrue(string name,  string group = "", string module = "");


    /*! \short   check if item is configured to be false

        this will lookup the given item in the list of configured items
        and return true if the item is either set to be '0', 'false'
        or 'no'.  Items which are configured but do not have a value are 
        considered false.

        \arg \c   name - name of queried item
        \returns  a value !=0 if item is configured to one of '0','false','no'      
    */
    int isFalse(string name, string group = "", string module = "");


    /*! \short   set configured item's value to a specified string

        \arg \c name    name of the item
        \arg \c value   string value to be set
        \arg \c group   group of the item (optional)
        \arg \c module  module of the item (optional)
    */
    void setItem(string name, string value, string group = "" , string module = "");

    
    //! get a pointer to the access list
    inline configADList_t &getAccessList() { return ad_list; }
    
    /*! merge command line args into config database
        command line args overrule existing configuration
     */
    void mergeArgs(commandLineArgList_t *args, commandLineArgValList_t *argVals);

    //! create a C-style param list for the proc modules
    static configParam_t *getParamList( configItemList_t &list );

    //! dump object (list of configured items)
    void dump( ostream &os );
};


//! overload << for use on ConfigManager objects
ostream& operator<< ( ostream &os, ConfigManager &cm );


#endif // _CONFIGMANAGER_H_
