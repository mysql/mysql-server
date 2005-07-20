/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_printlog.c,v 11.64 2004/06/17 17:35:17 bostic Exp $
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2004\nSleepycat Software Inc.  All rights reserved.\n";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/fop.h"
#include "dbinc/hash.h"
#include "dbinc/log.h"
#include "dbinc/qam.h"
#include "dbinc/txn.h"

int lsn_arg __P((const char *, char *, DB_LSN *));
int main __P((int, char *[]));
int open_rep_db __P((DB_ENV *, DB **, DBC **));
int print_app_record __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
int usage __P((void));
int version_check __P((const char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	const char *progname = "db_printlog";
	DB *dbp;
	DBC *dbc;
	DBT data, keydbt;
	DB_ENV	*dbenv;
	DB_LOGC *logc;
	DB_LSN key, start, stop;
	size_t dtabsize;
	u_int32_t logcflag;
	int ch, cmp, exitval, nflag, rflag, ret, repflag;
	int (**dtab) __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	char *home, *passwd;

	if ((ret = version_check(progname)) != 0)
		return (ret);

	dbp = NULL;
	dbc = NULL;
	dbenv = NULL;
	logc = NULL;
	ZERO_LSN(start);
	ZERO_LSN(stop);
	dtabsize = 0;
	exitval = nflag = rflag = repflag = 0;
	dtab = NULL;
	home = passwd = NULL;

	while ((ch = getopt(argc, argv, "b:e:h:NP:rRV")) != EOF)
		switch (ch) {
		case 'b':
			if (lsn_arg(progname, optarg, &start))
				return (usage());
			break;
		case 'e':
			if (lsn_arg(progname, optarg, &stop))
				return (usage());
			break;
		case 'h':
			home = optarg;
			break;
		case 'N':
			nflag = 1;
			break;
		case 'P':
			passwd = strdup(optarg);
			memset(optarg, 0, strlen(optarg));
			if (passwd == NULL) {
				fprintf(stderr, "%s: strdup: %s\n",
				    progname, strerror(errno));
				return (EXIT_FAILURE);
			}
			break;
		case 'r':
			rflag = 1;
			break;
		case 'R':		/* Undocumented */
			repflag = 1;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		return (usage());

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		goto shutdown;
	}

	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);

	if (nflag) {
		if ((ret = dbenv->set_flags(dbenv, DB_NOLOCKING, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOLOCKING");
			goto shutdown;
		}
		if ((ret = dbenv->set_flags(dbenv, DB_NOPANIC, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOPANIC");
			goto shutdown;
		}
	}

	if (passwd != NULL && (ret = dbenv->set_encrypt(dbenv,
	    passwd, DB_ENCRYPT_AES)) != 0) {
		dbenv->err(dbenv, ret, "set_passwd");
		goto shutdown;
	}

	/*
	 * Set up an app-specific dispatch function so that we can gracefully
	 * handle app-specific log records.
	 */
	if ((ret = dbenv->set_app_dispatch(dbenv, print_app_record)) != 0) {
		dbenv->err(dbenv, ret, "app_dispatch");
		goto shutdown;
	}

	/*
	 * An environment is required, but as all we're doing is reading log
	 * files, we create one if it doesn't already exist.  If we create
	 * it, create it private so it automatically goes away when we're done.
	 * If we are reading the replication database, do not open the env
	 * with logging, because we don't want to log the opens.
	 */
	if (repflag) {
		if ((ret = dbenv->open(dbenv, home,
		    DB_INIT_MPOOL | DB_USE_ENVIRON, 0)) != 0 &&
		    (ret == DB_VERSION_MISMATCH ||
		    (ret = dbenv->open(dbenv, home,
		    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON, 0))
		    != 0)) {
			dbenv->err(dbenv, ret, "DB_ENV->open");
			goto shutdown;
		}
	} else if ((ret = dbenv->open(dbenv, home,
	    DB_JOINENV | DB_USE_ENVIRON, 0)) != 0 &&
	    (ret == DB_VERSION_MISMATCH ||
	    (ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_LOG | DB_PRIVATE | DB_USE_ENVIRON, 0)) != 0)) {
		dbenv->err(dbenv, ret, "DB_ENV->open");
		goto shutdown;
	}

	/* Initialize print callbacks. */
	if ((ret = __bam_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __crdel_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __db_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __dbreg_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __fop_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
#ifdef HAVE_HASH
	    (ret = __ham_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
#endif
#ifdef HAVE_QUEUE
	    (ret = __qam_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
#endif
	    (ret = __txn_init_print(dbenv, &dtab, &dtabsize)) != 0) {
		dbenv->err(dbenv, ret, "callback: initialization");
		goto shutdown;
	}

	/* Allocate a log cursor. */
	if (repflag) {
		if ((ret = open_rep_db(dbenv, &dbp, &dbc)) != 0)
			goto shutdown;
	} else if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB_ENV->log_cursor");
		goto shutdown;
	}

	if (IS_ZERO_LSN(start)) {
		memset(&keydbt, 0, sizeof(keydbt));
		logcflag = rflag ? DB_PREV : DB_NEXT;
	} else {
		key = start;
		logcflag = DB_SET;
	}
	memset(&data, 0, sizeof(data));

	for (; !__db_util_interrupted(); logcflag = rflag ? DB_PREV : DB_NEXT) {
		if (repflag) {
			ret = dbc->c_get(dbc, &keydbt, &data, logcflag);
			if (ret == 0)
				key = ((REP_CONTROL *)keydbt.data)->lsn;
		} else
			ret = logc->get(logc, &key, &data, logcflag);
		if (ret != 0) {
			if (ret == DB_NOTFOUND)
				break;
			dbenv->err(dbenv,
			    ret, repflag ? "DB_LOGC->get" : "DBC->get");
			goto shutdown;
		}

		/*
		 * We may have reached the end of the range we're displaying.
		 */
		if (!IS_ZERO_LSN(stop)) {
			cmp = log_compare(&key, &stop);
			if ((rflag && cmp < 0) || (!rflag && cmp > 0))
				break;
		}

		ret = __db_dispatch(dbenv,
		    dtab, dtabsize, &data, &key, DB_TXN_PRINT, NULL);

		/*
		 * XXX
		 * Just in case the underlying routines don't flush.
		 */
		(void)fflush(stdout);

		if (ret != 0) {
			dbenv->err(dbenv, ret, "tx: dispatch");
			goto shutdown;
		}
	}

	if (0) {
shutdown:	exitval = 1;
	}
	if (logc != NULL && (ret = logc->close(logc, 0)) != 0)
		exitval = 1;

	if (dbc != NULL && (ret = dbc->c_close(dbc)) != 0)
		exitval = 1;

	if (dbp != NULL && (ret = dbp->close(dbp, 0)) != 0)
		exitval = 1;

	/*
	 * The dtab is allocated by __db_add_recovery (called by *_init_print)
	 * using the library malloc function (__os_malloc).  It thus needs to be
	 * freed using the corresponding free (__os_free).
	 */
	if (dtab != NULL)
		__os_free(dbenv, dtab);
	if (dbenv != NULL && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	if (passwd != NULL)
		free(passwd);

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

int
usage()
{
	fprintf(stderr, "usage: db_printlog %s\n",
	    "[-NrV] [-b file/offset] [-e file/offset] [-h home] [-P password]");
	return (EXIT_FAILURE);
}

int
version_check(progname)
	const char *progname;
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR || v_minor != DB_VERSION_MINOR) {
		fprintf(stderr,
	"%s: version %d.%d doesn't match library version %d.%d\n",
		    progname, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    v_major, v_minor);
		return (EXIT_FAILURE);
	}
	return (0);
}

/* Print an unknown, application-specific log record as best we can. */
int
print_app_record(dbenv, dbt, lsnp, op)
	DB_ENV *dbenv;
	DBT *dbt;
	DB_LSN *lsnp;
	db_recops op;
{
	int ch;
	u_int32_t i, rectype;

	DB_ASSERT(op == DB_TXN_PRINT);

	COMPQUIET(dbenv, NULL);
	COMPQUIET(op, DB_TXN_PRINT);

	/*
	 * Fetch the rectype, which always must be at the beginning of the
	 * record (if dispatching is to work at all).
	 */
	memcpy(&rectype, dbt->data, sizeof(rectype));

	/*
	 * Applications may wish to customize the output here based on the
	 * rectype.  We just print the entire log record in the generic
	 * mixed-hex-and-printable format we use for binary data.
	 */
	printf("[%lu][%lu]application specific record: rec: %lu\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset, (u_long)rectype);
	printf("\tdata: ");
	for (i = 0; i < dbt->size; i++) {
		ch = ((u_int8_t *)dbt->data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	printf("\n\n");

	return (0);
}

int
open_rep_db(dbenv, dbpp, dbcp)
	DB_ENV *dbenv;
	DB **dbpp;
	DBC **dbcp;
{
	int ret;

	DB *dbp;
	*dbpp = NULL;
	*dbcp = NULL;

	if ((ret = db_create(dbpp, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (ret);
	}

	dbp = *dbpp;
	if ((ret =
	    dbp->open(dbp, NULL, "__db.rep.db", NULL, DB_BTREE, 0, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->open");
		goto err;
	}

	if ((ret = dbp->cursor(dbp, NULL, dbcp, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->cursor");
		goto err;
	}

	return (0);

err:	if (*dbpp != NULL)
		(void)(*dbpp)->close(*dbpp, 0);
	return (ret);
}

/*
 * lsn_arg --
 *	Parse a LSN argument.
 */
int
lsn_arg(progname, optarg, lsnp)
	const char *progname;
	char *optarg;
	DB_LSN *lsnp;
{
	char *p;
	u_long uval;

	/*
	 * Expected format is: lsn.file/lsn.offset.
	 *
	 * Don't use getsubopt(3), some systems don't have it.
	 */
	if ((p = strchr(optarg, '/')) == NULL)
		return (1);
	*p = '\0';

	if (__db_getulong(NULL, progname, optarg, 0, 0, &uval))
		return (1);
	if (uval > UINT32_MAX)
		return (1);
	lsnp->file = uval;
	if (__db_getulong(NULL, progname, p + 1, 0, 0, &uval))
		return (1);
	if (uval > UINT32_MAX)
		return (1);
	lsnp->offset = uval;
	return (0);
}
