#include "stdincpp.h"
#include "PerfTimer.h"
#include "Error.h"

main (int argc, char **argv)
{
    PerfTimer *pt = NULL;

    try { 
	cout << "------- startup -------" << endl;

	pt = PerfTimer::getInstance();

	cout << "------- testrun -------" << endl;

	// do some testing ###

	pt->start(slot1);
	pt->start(slot2);
	pt->start(slot3);
	//    sleep(1);
	pt->stop(slot3);
	pt->stop(slot2);
	pt->stop(slot1);

	fprintf( stderr, " %llu < %llu < %llu \n",
		 pt->avg(slot3), pt->avg(slot2), pt->avg(slot1) );

	// dump PerfTimer object
	cout << *pt;

    } catch (Error &e) {
	cerr << e.getError() << endl;
	cout << "------- catched exception -------" << endl;
	cout << e.getError().c_str() << endl;
    }

    cout << "------- shutdown -------" << endl;
    delete pt;

}
