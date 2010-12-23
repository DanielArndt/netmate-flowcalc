
#include "Error.h"
#include "Logger.h"
#include "PacketQueue.h"

#define LOG logger->log

#define BUFFERS 10000
#define MINLEN 32
#define MAXLEN 1500
#define LENSTEP 20
#define RUNS 100000


void writeData( char *buf, int lin, char in )
{
    char *pos = buf;

    //    fprintf( stdout, "writing test data to %d, len = %d, c = %d \n",
    //     (int) buf, lin, in );
    
    *((int*)pos)++ = lin;
    *pos++         = in;
    
    for( ; pos < buf+lin-5 ; pos++ ) {
	*pos = in++;
    }
}
		

int checkData( char *buf )
{
    char *pos = buf;
    int len = *((int*)pos)++;
    char c  = *pos++;

    //fprintf( stdout, "reading test data from %d, len = %d, c = %d \n",
    //     (int) buf, len, c );

    for( ; pos < buf+len-5; pos++ ) {
	if( *pos != c++ ) {
	    return -1;
	}
    }

    return 0;
}



int main(int argc, char** argv)
{
    Logger *logger = NULL;
    int res, len;
    unsigned char in, out;
    int lin, lout;
    PacketQueue *pq;
    char *buf; 
    //    metaData_t *meta;
    
    try{
	logger = Logger::getInstance();
	logger->setDefaultLogfile( "PacketQueueTest.log" );
	LOG( 0, "------- starting test -------" );

	// logger->log( L_INFO, 0, "hallo %d", 222 );

	in = out = 0;
	lin = lout = MINLEN;

	pq = new PacketQueue( BUFFERS );

	for (int i=0; i<RUNS; i++) {

	    for (int j=0; j<2; j++ ) {

		res = pq->getBufferSpace( &buf );

		if (res != 0) {
		    //		    printf( "getBufferSpace failed.\n" );
		    printf( "." );
		    break;
		}

		writeData( buf, lin, in++ );
		
		res = pq->setBufferOccupied( lin );
		
		if (res != 0) {
		    //		    printf( "setBufferOccupied failed.\n" );
		    printf( "#" );
		    break;
		}
		
		lin += LENSTEP;
		if (lin > MAXLEN ) {
		    lin = MINLEN;
		}
	    }

	    res = pq->readBuffer( &buf, &len );

	    if (res != 0) {
		//		printf( "readBuffer failed.\n" );
		printf( "1" );
		break;
	    }

	    // check buffer contents
	    if( checkData( buf ) == -1 ) {
		// printf( "data verify error" );
		printf( "!" );
	    }

	    res = pq->releaseBuffer();

	    if (res != 0) {
		//		printf( "releaseBuffer failed.\n" );
		printf( "2" );
		break;
	    }

	    // printf( "\n" );
	    // sleep(2);
	}

	delete pq;

    } catch( Error &e ) {
	cerr << e.getError() << "\n";
	LOG( 0, "------- catched exception -------" );
	LOG( 0, e.getError().c_str() );
    }

    LOG( 0, "------- shutdown -------" );
    delete logger;

    return 0;
}
