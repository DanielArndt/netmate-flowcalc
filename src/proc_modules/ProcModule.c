
/*! \file ProcModule.c

Modifications Copyright Daniel Arndt, 2010

This version has been modified to provide additional output and 
compatibility. For more information, please visit

http://web.cs.dal.ca/darndt/projects/netmate-bundle

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
implementation of helper functions for Packet Processing Modules

$Id: ProcModule.c 748 2009-09-10 02:54:03Z szander $

*/

#include "ProcModule.h"

/*! \short   embed magic cookie number in every packet processing module
    _N_et_M_ate _P_rocessing Module */
int magic = PROC_MAGIC;


/*! \short   declaration of struct containing all function pointers of a module */
ProcModuleInterface_t func = 
{ 
    3, 
    initModule, 
    destroyModule, 
    initFlowRec, 
    getTimers, 
    destroyFlowRec,
    resetFlowRec, 
    timeout, 
    processPacket,
    exportData, 
    getModuleInfo, 
    getErrorMsg,
    getTypeInfo };


/*! \short   global state variable used within data export macros */
void *_dest;
int   _pos;
int   _listlen;
int   _listpos;


/* align macro */
#define ALIGN( var, type ) var = ( var + sizeof(type)-1 )/sizeof(type) * sizeof(type)


/*! export functions */

extern inline void STARTEXPORT(void *data)
{
    _dest = data; 
    _pos=0;   
}

extern inline void ENDEXPORT(void **exp, int *len)
{
    *exp = _dest; 
    *len = _pos;
}

extern inline void ADD_CHAR(char val)
{
    ALIGN(_pos, char);
    *((char*)(_dest+_pos)) = val;
    _pos += sizeof(val);
}

extern inline void ADD_INT8(char val)
{
    ALIGN(_pos, char);
    *((char*)(_dest+_pos)) = val;
    _pos += sizeof(val);
}
extern inline void ADD_INT16(short val)
{
    ALIGN(_pos, short);
    *((short*)(_dest+_pos))= val;
    _pos += sizeof(val);
}

extern inline void ADD_INT32(int32_t val)
{
    ALIGN(_pos, int32_t);
    *((int32_t*)(_dest+_pos))= val;
    _pos += sizeof(val);
}

extern inline void ADD_INT64(int64_t val)
{
    ALIGN( _pos, int64_t );
    /* even 64bit values only aligned to 32 bit! */
    *((int64_t*)(_dest+_pos))= val; 
    _pos += sizeof(val); 
}

extern inline void ADD_UINT8(unsigned char val)
{
    ALIGN(_pos, unsigned char);
    *((unsigned char*)(_dest+_pos)) = val;
    _pos += sizeof(val);
}

extern inline void ADD_UINT16(unsigned short val)
{
    ALIGN(_pos, unsigned short);
    *((unsigned short*)(_dest+_pos)) = val;
    _pos += sizeof(val);
}

extern inline void ADD_UINT32(uint32_t val)
{
    ALIGN(_pos, uint32_t);
    *((uint32_t*)(_dest+_pos)) = val;
    _pos += sizeof(val);
}

extern inline void ADD_UINT64(uint64_t val)
{
    ALIGN( _pos, uint64_t ); 
    /* even 64bit values only aligned to 32 bit! */
    *((uint64_t*)(_dest+_pos)) = val; 
    _pos += sizeof(val); 
}

extern inline void ADD_FLOAT(float val)
{
    ALIGN(_pos, float);
    *((float*)(_dest+_pos)) = val;
    _pos += sizeof(val);
}

extern inline void ADD_DOUBLE(double val)
{
    ALIGN(_pos, double);
    *((double*)(_dest+_pos)) = val;
    _pos += sizeof(val);
}

extern inline void ADD_IPV4ADDR(unsigned int val)
{
    /* treat as unsigned32 */
    ADD_UINT32(val);
}

extern inline void ADD_LIST(unsigned int num)
{
    ADD_UINT32(num);
    _listpos = _pos; 
    ADD_UINT32(0); 
    _listlen = _pos;
}

extern inline void END_LIST()
{
    ((int*)((char*)_dest+_listpos))[0] = _pos - _listlen;
}

extern inline void ADD_STRING(char *txt)
{
    int _slen = strlen(txt) + 1;
    memcpy(_dest+_pos, txt, _slen);
    _pos += _slen; 
}

extern inline void ADD_BINARY(unsigned int size, char *src)
{
    ADD_UINT32( size );
    memcpy(_dest+_pos, src, size);
    _pos += size;
}



typeInfo_t* getTypeInfo()
{
    return exportInfo;
}

