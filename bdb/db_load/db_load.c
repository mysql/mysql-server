/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2000\nSleepycat Software Inc.  All rights reserved.\n";
static const char revid[] =
    "$Id: db_load.c,v 11.33 2001/01/22 17:25:07 krinsky Exp $";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_am.h"
#include "clib_ext.h"

void	badend __P((void));
void	badnum __P((void));
int	configure __P((DB *, char **, char **, int *));
int	db_init __P((char *));
int	dbt_rdump __P((DBT *));
int	dbt_rprint __P((DBT *));
int	dbt_rrecno __P((DBT *, int));
int	digitize __P((int, int *));
int	load __P((char *, DBTYPE, char **, int, u_int32_t));
int	main __P((int, char *[]));
int	rheader __P((DB *, DBTYPE *, char **, int *, int *));
void	usage __P((void));
void	version_check __P((void));

int	endodata;			/* Reached the end of a database. */
int	endofile;			/* Reached the end of the input. */
int	existed;			/* Tried to load existing key. */
u_long	lineno;				/* Input file line number. */
int	version = 1;			/* Input version. */

DB_ENV	*dbenv;
const char
	*progname = "db_load";		/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DBTYPE dbtype;
	u_int32_t db_nooverwrite;
	int ch, exitval, no_header, ret;
	char **clist, **clp, *home;

	version_check();

	home = NULL;
	db_nooverwrite = 0;
	exitval = no_header = 0;
	dbtype = DB_UNKNOWN;

	/* Allocate enough room for configuration arguments. */
	if ((clp = clist = (char **)calloc(argc + 1, sizeof(char *))) == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(ENOMEM));
		exit(1);
	}

	while ((ch = getopt(argc, argv, "c:f:h:nTt:V")) != EOF)
		switch (ch) {
		case 'c':
			*clp++ = optarg;
			break;
		case 'f':
			if (freopen(optarg, "r", stdin) == NULL) {
				fprintf(stderr, "%s: %s: reopen: %s\n",
				    progname, optarg, strerror(errno));
				exit(1);
			}
			break;
		case 'h':
			home = optarg;
			break;
		case 'n':
			db_nooverwrite = DB_NOOVERWRITE;
			break;
		case 'T':
			no_header = 1;
			break;
		case 't':
			if (strcmp(optarg, "btree") == 0) {
				dbtype = DB_BTREE;
				break;
			}
			if (strcmp(optarg, "hash") == 0) {
				dbtype = DB_HASH;
				break;
			}
			if (strcmp(optarg, "recno") == 0) {
				dbtype = DB_RECNO;
				break;
			}
			if (strcmp(optarg, "queue") == 0) {
				dbtype = DB_QUEUE;
				break;
			}
			usage();
			/* NOTREACHED */
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			exit(0);
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object initialized for error reporting, and
	 * then open it.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		goto shutdown;
	}
	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);
	if (db_init(home) != 0)
		goto shutdown;

	while (!endofile)
		if (load(argv[0],
		    dbtype, clist, no_header, db_nooverwrite) != 0)
			goto shutdown;

	if (0) {
shutdown:	exitval = 1;
	}
	if ((ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	/* Resend any caught signal. */
	__db_util_sigresend();

	/* Return 0 on success, 1 if keys existed already, and 2 on failure. */
	return (exitval == 0 ? (existed == 0 ? 0 : 1) : 2);
}

/*
 * load --
 *	Load a database.
 */
int
load(name, argtype, clist, no_header, db_nooverwrite)
	char *name, **clist;
	DBTYPE argtype;
	int no_header;
	u_int32_t db_nooverwrite;
{
	DB *dbp;
	DBT key, rkey, data, *readp, *writep;
	DBTYPE dbtype;
	DB_TXN *ctxn, *txn;
	db_recno_t recno, datarecno;
	int checkprint, hexkeys, keys, ret, rval;
	int keyflag, ascii_recno;
	char *subdb;

	endodata = 0;
	subdb = NULL;
	ctxn = txn = NULL;
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	memset(&rkey, 0, sizeof(DBT));

	/* Create the DB object. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (1);
	}

	dbtype = DB_UNKNOWN;
	keys = -1;
	hexkeys = -1;
	keyflag = -1;
	/* Read the header -- if there's no header, we expect flat text. */
	if (no_header) {
		checkprint = 1;
		dbtype = argtype;
	} else {
		if (rheader(dbp, &dbtype, &subdb, &checkprint, &keys) != 0)
			goto err;
		if (endofile)
			goto done;
	}

	/*
	 * Apply command-line configuration changes.  (We apply command-line
	 * configuration changes to all databases that are loaded, e.g., all
	 * subdatabases.)
	 */
	if (configure(dbp, clist, &subdb, &keyflag))
		goto err;

	if (keys != 1) {
		if (keyflag == 1) {
			dbp->err(dbp, EINVAL, "No keys specified in file");
			goto err;
		}
	}
	else if (keyflag == 0) {
		dbp->err(dbp, EINVAL, "Keys specified in file");
		goto err;
	}
	else
		keyflag = 1;

	if (dbtype == DB_BTREE || dbtype == DB_HASH) {
		if (keyflag == 0)
			dbp->err(dbp,
			    EINVAL, "Btree and Hash must specify keys");
		else
			keyflag = 1;
	}

	if (argtype != DB_UNKNOWN) {

		if (dbtype == DB_RECNO || dbtype == DB_QUEUE)
			if (keyflag != 1 && argtype != DB_RECNO
			     && argtype != DB_QUEUE) {
				dbenv->errx(dbenv,
			   "improper database type conversion specified");
				goto err;
			}
		dbtype = argtype;
	}

	if (dbtype == DB_UNKNOWN) {
		dbenv->errx(dbenv, "no database type specified");
		goto err;
	}

	if (keyflag == -1)
		keyflag = 0;

	/* 
	 * Recno keys have only been printed in hexadecimal starting
	 * with db_dump format version 3 (DB 3.2).  
	 *
	 * !!! 
	 * Note that version is set in rheader(), which must be called before 
	 * this assignment.
	 */
	hexkeys = (version >= 3 && keyflag == 1 && checkprint == 0);

	if (keyflag == 1 && (dbtype == DB_RECNO || dbtype == DB_QUEUE))
		ascii_recno = 1;
	else
		ascii_recno = 0;

	/* Open the DB file. */
	if ((ret = dbp->open(dbp,
	    name, subdb, dbtype, DB_CREATE, __db_omode("rwrwrw"))) != 0) {
		dbp->err(dbp, ret, "DB->open: %s", name);
		goto err;
	}

	/* Initialize the key/data pair. */
	readp = &key;
	writep = &key;
	if (dbtype == DB_RECNO || dbtype == DB_QUEUE) {
		key.size = sizeof(recno);
		if (keyflag) {
			key.data = &datarecno;
			if (checkprint) {
				readp = &rkey;
				goto key_data;
			}
		}
		else
			key.data = &recno;
	} else
key_data:	if ((readp->data =
		    (void *)malloc(readp->ulen = 1024)) == NULL) {
			dbenv->err(dbenv, ENOMEM, NULL);
			goto err;
		}
	if ((data.data = (void *)malloc(data.ulen = 1024)) == NULL) {
		dbenv->err(dbenv, ENOMEM, NULL);
		goto err;
	}

	if (TXN_ON(dbenv) && (ret = txn_begin(dbenv, NULL, &txn, 0)) != 0)
		goto err;

	/* Get each key/data pair and add them to the database. */
	for (recno = 1; !__db_util_interrupted(); ++recno) {
		if (!keyflag)
			if (checkprint) {
				if (dbt_rprint(&data))
					goto err;
			} else {
				if (dbt_rdump(&data))
					goto err;
			}
		else
			if (checkprint) {
				if (dbt_rprint(readp))
					goto err;
				if (!endodata && dbt_rprint(&data))
					goto fmt;
			} else {
				if (ascii_recno) {
					if (dbt_rrecno(readp, hexkeys))
						goto err;
				} else
					if (dbt_rdump(readp))
						goto err;
				if (!endodata && dbt_rdump(&data)) {
fmt:					dbenv->errx(dbenv,
					    "odd number of key/data pairs");
					goto err;
				}
			}
		if (endodata)
			break;
		if (readp != writep) {
			if (sscanf(readp->data, "%ud", &datarecno) != 1)
				dbenv->errx(dbenv,
				    "%s: non-integer key at line: %d",
				    name, !keyflag ? recno : recno * 2 - 1);
			if (datarecno == 0)
				dbenv->errx(dbenv, "%s: zero key at line: %d",
				    name,
				    !keyflag ? recno : recno * 2 - 1);
		}
retry:		if (txn != NULL)
			if ((ret = txn_begin(dbenv, txn, &ctxn, 0)) != 0)
				goto err;
		switch (ret =
		    dbp->put(dbp, txn, writep, &data, db_nooverwrite)) {
		case 0:
			if (ctxn != NULL) {
				if ((ret =
				    txn_commit(ctxn, DB_TXN_NOSYNC)) != 0)
					goto err;
				ctxn = NULL;
			}
			break;
		case DB_KEYEXIST:
			existed = 1;
			dbenv->errx(dbenv,
			    "%s: line %d: key already exists, not loaded:",
			    name,
			    !keyflag ? recno : recno * 2 - 1);

			(void)__db_prdbt(&key, checkprint, 0, stderr,
			    __db_verify_callback, 0, NULL);
			break;
		case DB_LOCK_DEADLOCK:
			/* If we have a child txn, retry--else it's fatal. */
			if (ctxn != NULL) {
				if ((ret = txn_abort(ctxn)) != 0)
					goto err;
				ctxn = NULL;
				goto retry;
			}
			/* FALLTHROUGH */
		default:
			dbenv->err(dbenv, ret, NULL);
			if (ctxn != NULL) {
				(void)txn_abort(ctxn);
				ctxn = NULL;
			}
			goto err;
		}
		if (ctxn != NULL) {
			if ((ret = txn_abort(ctxn)) != 0)
				goto err;
			ctxn = NULL;
		}
	}
done:	rval = 0;
	DB_ASSERT(ctxn == NULL);
	if (txn != NULL && (ret = txn_commit(txn, 0)) != 0) {
		txn = NULL;
		goto err;
	}

	if (0) {
err:		rval = 1;
		DB_ASSERT(ctxn == NULL);
		if (txn != NULL)
			(void)txn_abort(txn);
	}

	/* Close the database. */
	if ((ret = dbp->close(dbp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->close");
		rval = 1;
	}

	/* Free allocated memory. */
	if (subdb != NULL)
		free(subdb);
	if (dbtype != DB_RECNO && dbtype != DB_QUEUE)
		free(key.data);
	if (rkey.data != NULL)
		free(rkey.data);
	free(data.data);

	return (rval);
}

/*
 * db_init --
 *	Initialize the environment.
 */
int
db_init(home)
	char *home;
{
	u_int32_t flags;
	int ret;

	/* We may be loading into a live environment.  Try and join. */
	flags = DB_USE_ENVIRON |
	    DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN;
	if (dbenv->open(dbenv, home, flags, 0) == 0)
		return (0);

	/*
	 * We're trying to load a database.
	 *
	 * An environment is required because we may be trying to look at
	 * databases in directories other than the current one.  We could
	 * avoid using an environment iff the -h option wasn't specified,
	 * but that seems like more work than it's worth.
	 *
	 * No environment exists (or, at least no environment that includes
	 * an mpool region exists).  Create one, but make it private so that
	 * no files are actually created.
	 */
	LF_CLR(DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN);
	LF_SET(DB_CREATE | DB_PRIVATE);
	if ((ret = dbenv->open(dbenv, home, flags, 0)) == 0)
		return (0);

	/* An environment is required. */
	dbenv->err(dbenv, ret, "DBENV->open");
	return (1);
}

#define	FLAG(name, value, keyword, flag)				\
	if (strcmp(name, keyword) == 0) {				\
		switch (*value) {					\
		case '1':						\
			if ((ret = dbp->set_flags(dbp, flag)) != 0) {	\
				dbp->err(dbp, ret, "%s: set_flags: %s",	\
				    progname, name);			\
				return (1);				\
			}						\
			break;						\
		case '0':						\
			break;						\
		default:						\
			badnum();					\
			return (1);					\
		}							\
		continue;						\
	}
#define	NUMBER(name, value, keyword, func)				\
	if (strcmp(name, keyword) == 0) {				\
		if (__db_getlong(dbp,					\
		    NULL, value, 1, LONG_MAX, &val) != 0)		\
			return (1);					\
		if ((ret = dbp->func(dbp, val)) != 0)			\
			goto nameerr;					\
		continue;						\
	}
#define	STRING(name, value, keyword, func)				\
	if (strcmp(name, keyword) == 0) {				\
		if ((ret = dbp->func(dbp, value[0])) != 0)		\
			goto nameerr;					\
		continue;						\
	}

/*
 * configure --
 *	Handle command-line configuration options.
 */
int
configure(dbp, clp, subdbp, keysp)
	DB *dbp;
	char **clp, **subdbp;
	int *keysp;
{
	long val;
	int ret, savech;
	char *name, *value;

	for (; (name = *clp) != NULL; *--value = savech, ++clp) {
		if ((value = strchr(name, '=')) == NULL) {
			dbp->errx(dbp,
		    "command-line configuration uses name=value format");
			return (1);
		}
		savech = *value;
		*value++ = '\0';

		if (strcmp(name, "database") == 0 ||
		    strcmp(name, "subdatabase") == 0) {
			if ((*subdbp = strdup(value)) == NULL) {
				dbp->err(dbp, ENOMEM, NULL);
				return (1);
			}
			continue;
		}
		if (strcmp(name, "keys") == 0) {
			if (strcmp(value, "1") == 0)
				*keysp = 1;
			else if (strcmp(value, "0") == 0)
				*keysp = 0;
			else {
				badnum();
				return (1);
			}
			continue;
		}

#ifdef notyet
		NUMBER(name, value, "bt_maxkey", set_bt_maxkey);
#endif
		NUMBER(name, value, "bt_minkey", set_bt_minkey);
		NUMBER(name, value, "db_lorder", set_lorder);
		NUMBER(name, value, "db_pagesize", set_pagesize);
		FLAG(name, value, "duplicates", DB_DUP);
		FLAG(name, value, "dupsort", DB_DUPSORT);
		NUMBER(name, value, "h_ffactor", set_h_ffactor);
		NUMBER(name, value, "h_nelem", set_h_nelem);
		NUMBER(name, value, "re_len", set_re_len);
		STRING(name, value, "re_pad", set_re_pad);
		FLAG(name, value, "recnum", DB_RECNUM);
		FLAG(name, value, "renumber", DB_RENUMBER);

		dbp->errx(dbp,
		    "unknown command-line configuration keyword");
		return (1);
	}
	return (0);

nameerr:
	dbp->err(dbp, ret, "%s: %s=%s", progname, name, value);
	return (1);
}

/*
 * rheader --
 *	Read the header message.
 */
int
rheader(dbp, dbtypep, subdbp, checkprintp, keysp)
	DB *dbp;
	DBTYPE *dbtypep;
	char **subdbp;
	int *checkprintp, *keysp;
{
	long val;
	int first, ret;
	char *name, *value, *p, buf[128];

	*dbtypep = DB_UNKNOWN;
	*checkprintp = 0;

	for (first = 1;; first = 0) {
		++lineno;

		/* If we don't see the expected information, it's an error. */
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			if (!first || ferror(stdin))
				goto badfmt;
			endofile = 1;
			break;
		}
		if ((p = strchr(name = buf, '=')) == NULL)
			goto badfmt;
		*p++ = '\0';
		if ((p = strchr(value = p, '\n')) == NULL)
			goto badfmt;
		*p = '\0';
		if (name[0] == '\0' || value[0] == '\0')
			goto badfmt;

		if (strcmp(name, "HEADER") == 0)
			break;
		if (strcmp(name, "VERSION") == 0) {
			/*
			 * Version 1 didn't have a "VERSION" header line.  We
			 * only support versions 1, 2, and 3 of the dump format.
			 */
			version = atoi(value);

			if (version > 3) {
				dbp->errx(dbp,
				    "line %lu: VERSION %d is unsupported",
				    lineno, version);
				return (1);
			}
			continue;
		}
		if (strcmp(name, "format") == 0) {
			if (strcmp(value, "bytevalue") == 0) {
				*checkprintp = 0;
				continue;
			}
			if (strcmp(value, "print") == 0) {
				*checkprintp = 1;
				continue;
			}
			goto badfmt;
		}
		if (strcmp(name, "type") == 0) {
			if (strcmp(value, "btree") == 0) {
				*dbtypep = DB_BTREE;
				continue;
			}
			if (strcmp(value, "hash") == 0) {
				*dbtypep = DB_HASH;
				continue;
			}
			if (strcmp(value, "recno") == 0) {
				*dbtypep = DB_RECNO;
				continue;
			}
			if (strcmp(value, "queue") == 0) {
				*dbtypep = DB_QUEUE;
				continue;
			}
			dbp->errx(dbp, "line %lu: unknown type", lineno);
			return (1);
		}
		if (strcmp(name, "database") == 0 ||
		    strcmp(name, "subdatabase") == 0) {
			if ((*subdbp = strdup(value)) == NULL) {
				dbp->err(dbp, ENOMEM, NULL);
				return (1);
			}
			continue;
		}
		if (strcmp(name, "keys") == 0) {
			if (strcmp(value, "1") == 0)
				*keysp = 1;
			else if (strcmp(value, "0") == 0)
				*keysp = 0;
			else {
				badnum();
				return (1);
			}
			continue;
		}

#ifdef notyet
		NUMBER(name, value, "bt_maxkey", set_bt_maxkey);
#endif
		NUMBER(name, value, "bt_minkey", set_bt_minkey);
		NUMBER(name, value, "db_lorder", set_lorder);
		NUMBER(name, value, "db_pagesize", set_pagesize);
		FLAG(name, value, "duplicates", DB_DUP);
		FLAG(name, value, "dupsort", DB_DUPSORT);
		NUMBER(name, value, "h_ffactor", set_h_ffactor);
		NUMBER(name, value, "h_nelem", set_h_nelem);
		NUMBER(name, value, "re_len", set_re_len);
		STRING(name, value, "re_pad", set_re_pad);
		FLAG(name, value, "recnum", DB_RECNUM);
		FLAG(name, value, "renumber", DB_RENUMBER);

		dbp->errx(dbp,
		    "unknown input-file header configuration keyword");
		return (1);
	}
	return (0);

nameerr:
	dbp->err(dbp, ret, "%s: %s=%s", progname, name, value);
	return (1);

badfmt:
	dbp->errx(dbp, "line %lu: unexpected format", lineno);
	return (1);
}

/*
 * dbt_rprint --
 *	Read a printable line into a DBT structure.
 */
int
dbt_rprint(dbtp)
	DBT *dbtp;
{
	u_int32_t len;
	u_int8_t *p;
	int c1, c2, e, escape, first;
	char buf[32];

	++lineno;

	first = 1;
	e = escape = 0;
	for (p = dbtp->data, len = 0; (c1 = getchar()) != '\n';) {
		if (c1 == EOF) {
			if (len == 0) {
				endofile = endodata = 1;
				return (0);
			}
			badend();
			return (1);
		}
		if (first) {
			first = 0;
			if (version > 1) {
				if (c1 != ' ') {
					buf[0] = c1;
					if (fgets(buf + 1,
					    sizeof(buf) - 1, stdin) == NULL ||
					    strcmp(buf, "DATA=END\n") != 0) {
						badend();
						return (1);
					}
					endodata = 1;
					return (0);
				}
				continue;
			}
		}
		if (escape) {
			if (c1 != '\\') {
				if ((c2 = getchar()) == EOF) {
					badend();
					return (1);
				}
				c1 = digitize(c1, &e) << 4 | digitize(c2, &e);
				if (e)
					return (1);
			}
			escape = 0;
		} else
			if (c1 == '\\') {
				escape = 1;
				continue;
			}
		if (len >= dbtp->ulen - 10) {
			dbtp->ulen *= 2;
			if ((dbtp->data =
			    (void *)realloc(dbtp->data, dbtp->ulen)) == NULL) {
				dbenv->err(dbenv, ENOMEM, NULL);
				return (1);
			}
			p = (u_int8_t *)dbtp->data + len;
		}
		++len;
		*p++ = c1;
	}
	dbtp->size = len;

	return (0);
}

/*
 * dbt_rdump --
 *	Read a byte dump line into a DBT structure.
 */
int
dbt_rdump(dbtp)
	DBT *dbtp;
{
	u_int32_t len;
	u_int8_t *p;
	int c1, c2, e, first;
	char buf[32];

	++lineno;

	first = 1;
	e = 0;
	for (p = dbtp->data, len = 0; (c1 = getchar()) != '\n';) {
		if (c1 == EOF) {
			if (len == 0) {
				endofile = endodata = 1;
				return (0);
			}
			badend();
			return (1);
		}
		if (first) {
			first = 0;
			if (version > 1) {
				if (c1 != ' ') {
					buf[0] = c1;
					if (fgets(buf + 1,
					    sizeof(buf) - 1, stdin) == NULL ||
					    strcmp(buf, "DATA=END\n") != 0) {
						badend();
						return (1);
					}
					endodata = 1;
					return (0);
				}
				continue;
			}
		}
		if ((c2 = getchar()) == EOF) {
			badend();
			return (1);
		}
		if (len >= dbtp->ulen - 10) {
			dbtp->ulen *= 2;
			if ((dbtp->data =
			    (void *)realloc(dbtp->data, dbtp->ulen)) == NULL) {
				dbenv->err(dbenv, ENOMEM, NULL);
				return (1);
			}
			p = (u_int8_t *)dbtp->data + len;
		}
		++len;
		*p++ = digitize(c1, &e) << 4 | digitize(c2, &e);
		if (e)
			return (1);
	}
	dbtp->size = len;

	return (0);
}

/*
 * dbt_rrecno --
 *	Read a record number dump line into a DBT structure.
 */
int
dbt_rrecno(dbtp, ishex)
	DBT *dbtp;
	int ishex;
{
	char buf[32], *p, *q;

	++lineno;

	if (fgets(buf, sizeof(buf), stdin) == NULL) {
		endofile = endodata = 1;
		return (0);
	}

	if (strcmp(buf, "DATA=END\n") == 0) {
		endodata = 1;
		return (0);
	}

	if (buf[0] != ' ')
		goto bad;

	/*
	 * If we're expecting a hex key, do an in-place conversion
	 * of hex to straight ASCII before calling __db_getulong().
	 */
	if (ishex) {
		for (p = q = buf + 1; *q != '\0' && *q != '\n';) {
			/*
			 * 0-9 in hex are 0x30-0x39, so this is easy.
			 * We should alternate between 3's and [0-9], and
			 * if the [0-9] are something unexpected,
			 * __db_getulong will fail, so we only need to catch
			 * end-of-string conditions.
			 */
			if (*q++ != '3')
				goto bad;
			if (*q == '\n' || *q == '\0')
				goto bad;
			*p++ = *q++;
		}
		*p = '\0';
	}

	if (__db_getulong(NULL,
	    progname, buf + 1, 0, 0, (u_long *)dbtp->data)) {
bad:		badend();
		return (1);
	}

	dbtp->size = sizeof(db_recno_t);
	return (0);
}

/*
 * digitize --
 *	Convert a character to an integer.
 */
int
digitize(c, errorp)
	int c, *errorp;
{
	switch (c) {			/* Don't depend on ASCII ordering. */
	case '0': return (0);
	case '1': return (1);
	case '2': return (2);
	case '3': return (3);
	case '4': return (4);
	case '5': return (5);
	case '6': return (6);
	case '7': return (7);
	case '8': return (8);
	case '9': return (9);
	case 'a': return (10);
	case 'b': return (11);
	case 'c': return (12);
	case 'd': return (13);
	case 'e': return (14);
	case 'f': return (15);
	}

	dbenv->errx(dbenv, "unexpected hexadecimal value");
	*errorp = 1;

	return (0);
}

/*
 * badnum --
 *	Display the bad number message.
 */
void
badnum()
{
	dbenv->errx(dbenv,
	    "boolean name=value pairs require a value of 0 or 1");
}

/*
 * badend --
 *	Display the bad end to input message.
 */
void
badend()
{
	dbenv->errx(dbenv, "unexpected end of input data or key/data pair");
}

/*
 * usage --
 *	Display the usage message.
 */
void
usage()
{
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: db_load [-nTV]",
    "[-c name=value] [-f file] [-h home] [-t btree | hash | recno] db_file");
	exit(1);
}

void
version_check()
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR ||
	    v_minor != DB_VERSION_MINOR || v_patch != DB_VERSION_PATCH) {
		fprintf(stderr,
	"%s: version %d.%d.%d doesn't match library version %d.%d.%d\n",
		    progname, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    DB_VERSION_PATCH, v_major, v_minor, v_patch);
		exit (1);
	}
}
