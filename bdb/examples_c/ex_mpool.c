/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_mpool.c,v 11.13 2000/10/27 20:32:00 dda Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db.h>

int	init __P((char *, int, int, char *));
int	run __P((int, int, int, int, char *));
int	run_mpool __P((int, int, int, int, char *));
#ifdef HAVE_VXWORKS
int	ex_mpool __P((void));
#define	MPOOL	"/vxtmp/vxtmp/mpool"			/* File. */
#define	ERROR_RETURN	ERROR
#define	VXSHM_KEY	12
#else
int	main __P((int, char *[]));
void	usage __P((char *));
#define	MPOOL	"mpool"					/* File. */
#define	ERROR_RETURN	1
#endif

#ifndef HAVE_VXWORKS
int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int cachesize, ch, hits, npages, pagesize;
	char *progname;

	cachesize = 20 * 1024;
	hits = 1000;
	npages = 50;
	pagesize = 1024;
	progname = argv[0];
	while ((ch = getopt(argc, argv, "c:h:n:p:")) != EOF)
		switch (ch) {
		case 'c':
			if ((cachesize = atoi(optarg)) < 20 * 1024)
				usage(progname);
			break;
		case 'h':
			if ((hits = atoi(optarg)) <= 0)
				usage(progname);
			break;
		case 'n':
			if ((npages = atoi(optarg)) <= 0)
				usage(progname);
			break;
		case 'p':
			if ((pagesize = atoi(optarg)) <= 0)
				usage(progname);
			break;
		case '?':
		default:
			usage(progname);
		}
	argc -= optind;
	argv += optind;

	return (run_mpool(pagesize, cachesize, hits, npages, progname));
}

void
usage(progname)
	char *progname;
{
	(void)fprintf(stderr,
	    "usage: %s [-c cachesize] [-h hits] [-n npages] [-p pagesize]\n",
	    progname);
	exit(1);
}
#else
int
ex_mpool()
{
	char *progname = "ex_mpool";			/* Program name. */
	int cachesize, ch, hits, npages, pagesize;

	cachesize = 20 * 1024;
	hits = 1000;
	npages = 50;
	pagesize = 1024;

	return (run_mpool(pagesize, cachesize, hits, npages, progname));
}
#endif

int
run_mpool(pagesize, cachesize, hits, npages, progname)
	int pagesize, cachesize, hits, npages;
	char *progname;
{
	int ret;

	/* Initialize the file. */
	if ((ret = init(MPOOL, pagesize, npages, progname)) != 0)
		return (ret);

	/* Get the pages. */
	if ((ret = run(hits, cachesize, pagesize, npages, progname)) != 0)
		return (ret);

	return (0);
}

/*
 * init --
 *	Create a backing file.
 */
int
init(file, pagesize, npages, progname)
	char *file, *progname;
	int pagesize, npages;
{
	int cnt, flags, fd;
	char *p;

	/*
	 * Create a file with the right number of pages, and store a page
	 * number on each page.
	 */
	flags = O_CREAT | O_RDWR | O_TRUNC;
#ifdef DB_WIN32
	flags |= O_BINARY;
#endif
	if ((fd = open(file, flags, 0666)) < 0) {
		fprintf(stderr,
		    "%s: %s: %s\n", progname, file, strerror(errno));
		return (ERROR_RETURN);
	}
	if ((p = (char *)malloc(pagesize)) == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(ENOMEM));
		return (ERROR_RETURN);
	}

	/* The pages are numbered from 0. */
	for (cnt = 0; cnt <= npages; ++cnt) {
		*(int *)p = cnt;
		if (write(fd, p, pagesize) != pagesize) {
			fprintf(stderr,
			    "%s: %s: %s\n", progname, file, strerror(errno));
			return (ERROR_RETURN);
		}
	}

	(void)close(fd);
	free(p);
	return (0);
}

/*
 * run --
 *	Get a set of pages.
 */
int
run(hits, cachesize, pagesize, npages, progname)
	int hits, cachesize, pagesize, npages;
	char *progname;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *dbmfp;
	db_pgno_t pageno;
	int cnt, ret;
	void *p;

	printf("%s: cachesize: %d; pagesize: %d; N pages: %d\n",
	    progname, cachesize, pagesize, npages);

	/*
	 * Open a memory pool, specify a cachesize, output error messages
	 * to stderr.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);
#ifdef HAVE_VXWORKS
	if ((ret = dbenv->set_shm_key(dbenv, VXSHM_KEY)) != 0) {
		dbenv->err(dbenv, ret, "set_shm_key");
		return (ERROR_RETURN);
	}
#endif

	/* Set the cachesize. */
	if ((ret = dbenv->set_cachesize(dbenv, 0, cachesize, 0)) != 0) {
		dbenv->err(dbenv, ret, "set_cachesize");
		goto err1;
	}

	/* Open the environment. */
	if ((ret = dbenv->open(
	    dbenv, NULL, DB_CREATE | DB_INIT_MPOOL, 0)) != 0) {
		dbenv->err(dbenv, ret, "open");
		goto err1;
	}

	/* Open the file in the environment. */
	if ((ret =
	    memp_fopen(dbenv, MPOOL, 0, 0, pagesize, NULL, &dbmfp)) != 0) {
		dbenv->err(dbenv, ret, "memp_fopen: %s", MPOOL);
		goto err1;
	}

	printf("retrieve %d random pages... ", hits);

	srand((u_int)time(NULL));
	for (cnt = 0; cnt < hits; ++cnt) {
		pageno = (rand() % npages) + 1;
		if ((ret = memp_fget(dbmfp, &pageno, 0, &p)) != 0) {
			dbenv->err(dbenv, ret,
			    "unable to retrieve page %lu", (u_long)pageno);
			goto err2;
		}
		if (*(db_pgno_t *)p != pageno) {
			dbenv->errx(dbenv,
			    "wrong page retrieved (%lu != %d)",
			    (u_long)pageno, *(int *)p);
			goto err2;
		}
		if ((ret = memp_fput(dbmfp, p, 0)) != 0) {
			dbenv->err(dbenv, ret,
			    "unable to return page %lu", (u_long)pageno);
			goto err2;
		}
	}

	printf("successful.\n");

	/* Close the file. */
	if ((ret = memp_fclose(dbmfp)) != 0) {
		dbenv->err(dbenv, ret, "memp_fclose");
		goto err1;
	}

	/* Close the pool. */
	if ((ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
	return (0);

err2:	(void)memp_fclose(dbmfp);
err1:	(void)dbenv->close(dbenv, 0);
	return (ERROR_RETURN);
}
