
/*! \file  export_modules/ExportModule.h

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
    declaration of interface for Classifier Action Modules
 
    $Id: ExportModule.h 748 2009-09-10 02:54:03Z szander $
*/


#ifndef __EXPORTMODULE_H
#define __EXPORTMODULE_H


#include "ExportModuleInterface.h"


/*! \short   declaration of struct containing all function pointers of a module */
extern ExportModuleInterface_t func;

#endif /* __EXPORTMODULE_H */
