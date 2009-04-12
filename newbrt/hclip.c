#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// Clip horizontally (100 chars by default)
#include <stdio.h>
#include <stdlib.h>
int main (int argc, char *argv[]) {
    int targcol=100;
    int c;
    int col=0;
    if (argc>1) {
	targcol=atoi(argv[1]);
    }
    while ((c=getchar())!=EOF) {
	if (c=='\n') { col=0; putchar(c); }
	else if (col==targcol) { col++; printf("..."); }
	else if (col>targcol) { col++; }
	else { col++; putchar(c); }
    }
    return 0;
}
