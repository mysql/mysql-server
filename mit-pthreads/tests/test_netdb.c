/* ==== test_netdb.c =========================================================
 * Copyright (c) 1995 by Greg Hudson, ghudson@.mit.edu
 *
 * Description : Test netdb calls.
 *
 *  1.00 95/01/05 ghudson
 *      -Started coding this file.
 */

#define PTHREAD_KERNEL	/* Needed for OK and NOTOK defines */
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

int debug = 0;

static int test_serv()
{
	struct servent *serv;
	char answer[1024];

	if (serv = getservbyname("telnet", "tcp"))
		printf("getservbyname -> port %d\n", ntohs(serv->s_port));
	else
		printf("getservbyname -> NULL (bad)\n");

	if (serv = getservbyname_r("telnet", "tcp", serv, answer, 1024))
		printf("getservbyname_r -> port %d\n", ntohs(serv->s_port));
	else
		printf("getservbyname_r -> NULL (bad)\n");
	return(OK);
}

static int test_host()
{
    struct hostent *host;
	struct in_addr addr;
	char answer[1024];
	int error;

	if (host = gethostbyname("maze.mit.edu")) {
		memcpy(&addr, host->h_addr, sizeof(addr));
		printf("gethostbyname -> %s\n", inet_ntoa(addr));
	} else {
		printf("gethostbyname -> NULL (bad)\n");
		host = (struct hostent *)answer;
	}

	if (host = gethostbyname_r("maze.mit.edu", host, answer, 1024, &error)) {
		memcpy(&addr, host->h_addr, sizeof(addr));
		printf("gethostbyname_r -> %s\n", inet_ntoa(addr));
	} else {
		printf("gethostbyname_r -> NULL (bad)\n");
	}
	return(OK);
}

static int test_localhost()
{
    struct hostent *host;

	if (host = gethostbyname("127.0.0.1")) {
		return(OK);
	}
	return(NOTOK);
}

/* ==========================================================================
 * usage();
 */
void usage(void)
{       
    printf("test_netdb [-d?]\n");
    errno = 0;
}

main(int argc, char **argv)
{

	/* Getopt variables. */
	extern int optind, opterr;
	extern char *optarg;
	char ch;

	while ((ch = getopt(argc, argv, "d?")) != (char)EOF) {
        switch (ch) {
        case 'd':
            debug++;
            break;
		case '?':
            usage();
            return(OK); 
        default:  
            usage();
            return(NOTOK);
        }
    }

	printf("test_netdb START\n");
	
	if (test_serv() || test_localhost() || test_host()) {
		printf("test_netdb FAILED\n");
		exit(1);
	}

	printf("test_netdb PASSED\n");
	exit(0);
}
