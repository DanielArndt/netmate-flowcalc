/* 

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
  small program to generate artificial rule files

  $Id: genrules.c,v 1.1.1.1 2004/12/23 05:52:37 s_zander Exp $

*/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

void usage() 
{
    printf("Usage: \n");
    printf("  -n   number of rules to generate\n");
    printf("  -f   file name\n");
}

int getRandByte()
{
    return (int) (drand48() * 255);
}

int getRandWord()
{
	return (int) (drand48() * 65535);	
}

int main(int argc, char *argv[])
{
    int i;
    int n = 0;
    char *fname = NULL;
    FILE *f;

    srand48(time(NULL));

    while ((i = getopt(argc, argv, "n:f:h?")) != -1) {
	if ((i != '?') && (i != 'h')) {
	    switch(i) {
	    case 'n':
		n = atoi(optarg);
		break;
	    case 'f':
		fname = strdup(optarg);
		break;
	    default:
		break;
	    }
	} else {
	    usage();
	}
    }

    if (fname == NULL) {
      usage();	
      exit(1);
    }	

    f = fopen(fname, "w");

    fprintf(f, "<!DOCTYPE RULESET SYSTEM \"rulefile.dtd\">\n");
    fprintf(f, "<RULESET ID=\"1\">\n");
    fprintf(f, "<GLOBAL>\n");
    fprintf(f, "<PREF NAME=\"Duration\">1000</PREF>\n");
    fprintf(f, "<PREF NAME=\"Interval\">1</PREF>\n");
    fprintf(f, "<ACTION NAME=\"count\"></ACTION>\n");
    fprintf(f, "<EXPORT NAME=\"text_file\">\n");
    fprintf(f, "<PREF NAME=\"Filename\">/tmp/netmate-results</PREF>\n");
    fprintf(f, "</EXPORT>\n");
    fprintf(f, "</GLOBAL>\n");

    // print rules

    for (i=1; i<n; i++) {
	fprintf(f, "<RULE ID=\"%d\">\n", i);
	fprintf(f, "<FILTER NAME=\"SrcIP\">%d.%d.%d.%d</FILTER>\n", getRandByte(), 
		getRandByte(), getRandByte(), getRandByte());
	fprintf(f, "<FILTER NAME=\"DstIP\">%d.%d.%d.%d</FILTER>\n", getRandByte(), 
		getRandByte(), getRandByte(), getRandByte());
	fprintf(f, "<FILTER NAME=\"Proto\">%d</FILTER>\n", getRandByte());
	fprintf(f, "<FILTER NAME=\"SrcPort\">%d</FILTER>\n", getRandWord());
	fprintf(f, "<FILTER NAME=\"DstPort\">%d</FILTER>\n", getRandWord());
	fprintf(f, "</RULE>\n");
    }

    // add a rule later
    fprintf(f, "<RULE ID=\"%d\">\n", n);
    fprintf(f, "<FILTER NAME=\"SrcIP\">192.168.60.1</FILTER>\n");
    fprintf(f, "<FILTER NAME=\"DstIP\">192.168.60.3</FILTER>\n");
    fprintf(f, "<FILTER NAME=\"Proto\">udp</FILTER>\n");
    fprintf(f, "<FILTER NAME=\"SrcPort\">1024</FILTER>\n");
    fprintf(f, "<FILTER NAME=\"DstPort\">1025</FILTER>\n");
    fprintf(f, "</RULE>\n");

    fprintf(f, "</RULESET>");

    fclose(f);

    return 0;
}
