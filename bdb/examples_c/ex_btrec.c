/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_btrec.c,v 11.8 2000/05/22 15:17:03 sue Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db.h>

#ifdef HAVE_VXWORKS
#define	DATABASE	"/vxtmp/vxtmp/access.db"
#define	WORDLIST	"/vxtmp/vxtmp/wordlist"
#define	ERROR_RETURN	ERROR
#else
#define	DATABASE	"access.db"
#define	WORDLIST	"../test/wordlist"
#define	ERROR_RETURN	1
int	main __P((int, char *[]));
void	usage __P((char *));
#endif

int	ex_btrec __P((void));
void	show __P((char *, DBT *, DBT *));

#ifndef HAVE_VXWORKS
int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch;

	while ((ch = getopt(argc, argv, "")) != EOF)
		switch (ch) {
		case '?':
		default:
			usage(argv[0]);
		}
	argc -= optind;
	argv += optind;

	return (ex_btrec());
}

void
usage(progname)
	char *progname;
{
	(void)fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}
#endif

int
ex_btrec()
{
	DB *dbp;
	DBC *dbcp;
	DBT key, data;
	DB_BTREE_STAT *statp;
	FILE *fp;
	db_recno_t recno;
	u_int32_t len;
	int cnt, ret;
	char *p, *t, buf[1024], rbuf[1024];
	const char *progname = "ex_btrec";		/* Program name. */

	/* Open the word database. */
	if ((fp = fopen(WORDLIST, "r")) == NULL) {
		fprintf(stderr, "%s: open %s: %s\n",
		    progname, WORDLIST, db_strerror(errno));
		return (ERROR_RETURN);
	}

	/* Remove the previous database. */
	(void)unlink(DATABASE);

	/* Create and initialize database object, open the database. */
	if ((ret = db_create(&dbp, NULL, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_create: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}
	dbp->set_errfile(dbp, stderr);
	dbp->set_errpfx(dbp, progname);			/* 1K page sizes. */
	if ((ret = dbp->set_pagesize(dbp, 1024)) != 0) {
		dbp->err(dbp, ret, "set_pagesize");
		return (ERROR_RETURN);
	}						/* Record numbers. */
	if ((ret = dbp->set_flags(dbp, DB_RECNUM)) != 0) {
		dbp->err(dbp, ret, "set_flags: DB_RECNUM");
		return (ERROR_RETURN);
	}
	if ((ret =
	    dbp->open(dbp, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "open: %s", DATABASE);
		return (ERROR_RETURN);
	}

	/*
	 * Insert records into the database, where the key is the word
	 * preceded by its record number, and the data is the same, but
	 * in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	for (cnt = 1; cnt <= 1000; ++cnt) {
		(void)sprintf(buf, "%04d_", cnt);
		if (fgets(buf + 4, sizeof(buf) - 4, fp) == NULL)
			break;
		len = strlen(buf);
		for (t = rbuf, p = buf + (len - 2); p >= buf;)
			*t++ = *p--;
		*t++ = '\0';

		key.data = buf;
		data.data = rbuf;
		data.size = key.size = len - 1;

		if ((ret =
		    dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE)) != 0) {
			dbp->err(dbp, ret, "DB->put");
			if (ret != DB_KEYEXIST)
				goto err1;
		}
	}

	/* Close the word database. */
	(void)fclose(fp);

	/* Print out the number of records in the database. */
	if ((ret = dbp->stat(dbp, &statp, NULL, 0)) != 0) {
		dbp->err(dbp, ret, "DB->stat");
		goto err1;
	}
	printf("%s: database contains %lu records\n",
	    progname, (u_long)statp->bt_ndata);
	free(statp);

	/* Acquire a cursor for the database. */
	if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		goto err1;
	}

	/*
	 * Prompt the user for a record number, then retrieve and display
	 * that record.
	 */
	for (;;) {
		/* Get a record number. */
		printf("recno #> ");
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		recno = atoi(buf);

		/*
		 * Reset the key each time, the dbp->c_get() routine returns
		 * the key and data pair, not just the key!
		 */
		key.data = &recno;
		key.size = sizeof(recno);
		if ((ret = dbcp->c_get(dbcp, &key, &data, DB_SET_RECNO)) != 0)
			goto get_err;

		/* Display the key and data. */
		show("k/d\t", &key, &data);

		/* Move the cursor a record forward. */
		if ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) != 0)
			goto get_err;

		/* Display the key and data. */
		show("next\t", &key, &data);

		/*
		 * Retrieve the record number for the following record into
		 * local memory.
		 */
		data.data = &recno;
		data.size = sizeof(recno);
		data.ulen = sizeof(recno);
		data.flags |= DB_DBT_USERMEM;
		if ((ret = dbcp->c_get(dbcp, &key, &data, DB_GET_RECNO)) != 0) {
get_err:		dbp->err(dbp, ret, "DBcursor->get");
			if (ret != DB_NOTFOUND && ret != DB_KEYEMPTY)
				goto err2;
		} else
			printf("retrieved recno: %lu\n", (u_long)recno);

		/* Reset the data DBT. */
		memset(&data, 0, sizeof(data));
	}

	if ((ret = dbcp->c_close(dbcp)) != 0) {
		dbp->err(dbp, ret, "DBcursor->close");
		goto err1;
	}
	if ((ret = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr,
		    "%s: DB->close: %s\n", progname, db_strerror(ret));
		return (ERROR_RETURN);
	}

	return (0);

err2:	(void)dbcp->c_close(dbcp);
err1:	(void)dbp->close(dbp, 0);
	return (ret);

}

/*
 * show --
 *	Display a key/data pair.
 */
void
show(msg, key, data)
	DBT *key, *data;
	char *msg;
{
	printf("%s%.*s : %.*s\n", msg,
	    (int)key->size, (char *)key->data,
	    (int)data->size, (char *)data->data);
}
