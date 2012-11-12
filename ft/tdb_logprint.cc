/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Dump the log from stdin to stdout. */

#include <ft/log_header.h>

static void newmain (int count) {
    int i;
    uint32_t version;
    int r = toku_read_and_print_logmagic(stdin, &version);
    for (i=0; i!=count; i++) {
	r = toku_logprint_one_record(stdout, stdin);
	if (r==EOF) break;
	if (r!=0) {
	    fflush(stdout);
	    fprintf(stderr, "Problem in log err=%d\n", r);
	    exit(1);
	}
    }
}

int main (int argc, char *const argv[]) {
    int count=-1;
    while (argc>1) {
	if (strcmp(argv[1], "--oldcode")==0) {
	    fprintf(stderr,"Old code no longer works.\n");
	    exit(1);
	} else {
	    count = atoi(argv[1]);
	}
	argc--; argv++;
    }
    newmain(count);
    return 0;
}

