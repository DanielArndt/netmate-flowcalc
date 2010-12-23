
/*! \file  proc_modules/ProcModule.h

    Modifications Copyright Daniel Arndt, 2010

    This version has been modified to provide additional output and 
    compatibility. For more information, please visit

    http://web.cs.dal.ca/darndt

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

    $Id: ProcModule.h 748 2009-09-10 02:54:03Z szander $

*/


#ifndef __PROCMODULE_H
#define __PROCMODULE_H


#include "ProcModuleInterface.h"



/*! functions for exporting data */
inline void STARTEXPORT(void *data);
inline void ENDEXPORT(void **exp, int *len);
inline void ADD_CHAR(char val);
inline void ADD_INT8(char val);
inline void ADD_INT16(short val);
inline void ADD_INT32(int32_t val);
inline void ADD_INT64(int64_t val);
inline void ADD_UINT8(unsigned char val);
inline void ADD_UINT16(unsigned short val);
inline void ADD_UINT32(uint32_t val);
inline void ADD_UINT64(uint64_t val);
inline void ADD_FLOAT(float val);
inline void ADD_DOUBLE(double val);
inline void ADD_IPV4ADDR(unsigned int val);
inline void ADD_LIST(unsigned int num);
inline void END_LIST();
inline void ADD_STRING(char *txt);
inline void ADD_BINARY(unsigned int size, char *src);


/*! \short   declaration of struct containing all function pointers of a module */
extern ProcModuleInterface_t func;


extern typeInfo_t exportInfo[];

#endif /* __PROCMODULE_H */

