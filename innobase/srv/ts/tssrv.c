/******************************************************
Test for the database server

(c) 1995 Innobase Oy

Created 10/10/1995 Heikki Tuuri
*******************************************************/

#include "srv0srv.h"
#include "os0proc.h"
#include "ut0mem.h"


/***************************************************************************
The main function of the server. */

void
main(
/*=*/
#ifdef notdefined

	ulint	argc,	/* in: number of string arguments given on
			the command line */
	char*	argv[]
#endif
)	/* in: array of character pointers giving
			the arguments */
{
/*
	if (argc != 2) {
		printf("Error! Wrong number of command line arguments!\n");
		printf("Usage: ib <init-file-name>\n");
		os_process_exit(1);
	}
*/
	srv_boot("init.ib"/*argv[1]*/);

	os_process_exit(0);
}
