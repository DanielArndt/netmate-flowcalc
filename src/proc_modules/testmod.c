
/*  testmod.c

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
dummy evaluation module for testing purposes


$Id: testmod.c 748 2009-09-10 02:54:03Z szander $

*/

#include "config.h"
#include <stdio.h>
#include "ProcModule.h"

/*
  define here the timers which such a module is using on default
*/

timers_t timers[] = { /* handle, ival_msec, flags */
    { 1, 1000, 0 },
    { 2, 2000, TM_RECURRING },
    { 3, 3000, TM_ALIGNED },
    { 4, 4000, TM_RECURRING | TM_ALIGNED },
    { 5, 5000, TM_NONE /*==0*/ },
    TIMER_END
};


/*
  define here the runtime type information structure for this module
*/
typeInfo_t exportInfo[] = {
    { CHAR, "a_char"        },
    { INT8, "int8_1"      },
    { INT8, "int8_2"      },
    { INT16, "int16"  },
    { INT32, "int32" },
    { INT64, "int64" },

    { LIST, "list" },
    { UINT16, "col1"  },
    { UINT16, "col2"  },
    { UINT32, "col3"  },
    { UINT8, "col4"  },
    LIST_END,

    { UINT8, "uint8"  },
    { UINT16, "uint16"  },
    { UINT32, "uint32" },
    { UINT64, "uint64" },
    { STRING, "string"   },
    { BINARY, "a_binary"   },

    { LIST, "list2" },
    { UINT16, "an_id" },
    { STRING, "a_text" },
    LIST_END,

    EXPORT_END };

int initModule()
{
#ifdef DEBUG
    fprintf( stderr, "testmod : initModule\n" );
#endif
    return 0;
}

int destroyModule()
{
#ifdef DEBUG
    fprintf( stderr, "testmod : destroyModule\n" );
#endif
    return 0;
}

int processPacket( char *packet, metaData_t *meta, void *flowdata )
{
#ifdef DEBUG
    fprintf( stderr, "testmod : processPacket (l = %d)\n", meta->cap_len );
#endif

    return 0;
}

int initFlowRec( configParam_t *params, void **flowdata )
{
    int *data;

#ifdef DEBUG
    fprintf( stderr, "testmod : initFlowRec\n" );
    fprintf( stderr, "testmod : module parameters:\n" );

    /* show parameters */
    while (params->name != NULL ) {
        fprintf( stderr, "'%s' = '%s'\n",
                 params->name, params->value );
        params++;
    }
#endif

    /* data = malloc( sizeof(struct accData_t) ); */
    data = (int*) malloc( 10 * sizeof(int) );

    if (data == NULL ) {
        return -1;
    }

    data[0] = 3;
    data[1] = time(NULL);

    *flowdata = data;

    return 0;
}

int resetFlowRec( void *flowdata )
{
#ifdef DEBUG
    fprintf( stderr, "testmod : resetFlowRec\n" );
#endif

    return 0;
}

int destroyFlowRec( void *flowdata )
{
#ifdef DEBUG
    fprintf( stderr, "testmod : destroyFlowRec\n" );
#endif

    free( flowdata );
    return 0;
}

int exportData( void **exp, int *len, void *flowdata )
{
    static unsigned char export[2000];
    char string[] = "test", bin[] = "1234", *txt[] = {"abcd","efgh","ijkl"};
    int *num,i;

#ifdef DEBUG
    fprintf( stderr, "testmod : exportData\n" );
#endif

    memset( export, 0, 1000);

    num = (int*)flowdata;
    if (*num < 6 ) {
        *num += 1;
    }

    STARTEXPORT( export );
    ADD_CHAR(    'a'  );
    ADD_INT8(    -1  );
    ADD_INT8(    -125  );
    ADD_INT16(   -16000  );
    ADD_INT32(   -2000100200  );
    ADD_INT64(   -10200300400LL  );

    ADD_LIST( *num );
    for( i = 0; i<*num; i++ ) {
        ADD_UINT16(  i  );
        ADD_UINT16(  2*i  );
        ADD_UINT32(  10*i  );
        ADD_UINT8( 42 );
    }
    END_LIST();
    
    ADD_UINT8(   250  );
    ADD_UINT16(  32000  );
    ADD_UINT32(  4000100200U  );
    ADD_UINT64(  10200300400ULL  );
    ADD_STRING(  string  );
    ADD_BINARY( 5, bin );

    ADD_LIST( 3 );
    for( i = 0; i<3; i++ ) {
        ADD_UINT16( i+1 );
        ADD_STRING( txt[i] );
    }
    END_LIST();

    ENDEXPORT( exp, len );

#ifdef DEBUG
    fprintf( stderr, "testmod : exportData done.\n" );
#endif

    return 0;
}

int dumpData( char *string, void *flowdata )
{
#ifdef DEBUG
    fprintf( stderr, "testmod : dumpData\n" );
#endif

    return 0;
}

char* getModuleInfo(int i)
{
#ifdef DEBUG
    fprintf( stderr, "testmod : getModuleInfo(%d)\n",i );
#endif

    switch(i) {
    case I_MODNAME:   return "testmod";
    case I_BRIEF:  return "module for testing";
    case I_AUTHOR: return "Carsten Schmoll";
    case I_CREATED: return "2001/05/08";
    case I_VERBOSE: return "work in progress";
    case I_PARAMS:  return "none";
    case I_RESULTS: return "work in progress";
    default: return NULL;
    }
}


char* getErrorMsg( int code )
{
#ifdef DEBUG
    fprintf( stderr, "testmod : getErrorMsg\n" );
#endif

    return NULL;
}


int timeout( int timerID, void *flowdata )
{
#ifdef DEBUG
    fprintf( stderr, "testmod_module: timeout (timer id = %d,"
             " time since start = %ld seconds)\n\n",
             timerID, time(NULL) - ((int*)flowdata )[1] );
#endif
    return 0;
}




timers_t* getTimers( void *flowData )
{
    return timers;
}
