/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_hotbackup.c,v 1.16 2005/10/27 01:25:54 mjc Exp $
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2005\nSleepycat Software Inc.  All rights reserved.\n";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "dbinc/log.h"

enum which_open { OPEN_ORIGINAL, OPEN_HOT_BACKUP };

int backup_dir_clean __P((DB_ENV *, char *, int *, int, int));
int data_copy __P((DB_ENV *, char *, char *, char *, int));
int env_init __P((DB_ENV **, char *, char *, char *, enum which_open));
int main __P((int, char *[]));
int read_data_dir __P((DB_ENV *, char *, char *, int));
int read_log_dir __P((DB_ENV *, char *, char *, int *, int, int));
int usage __P((void));
int version_check __P((void));

const char *progname;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	time_t now;
	DB_ENV *dbenv;
	u_int data_cnt, data_next;
	int ch, checkpoint, copy_min, exitval, remove_max, ret, update, verbose;
	char *backup_dir, **data_dir, **dir, *home, *log_dir, *passwd;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		++progname;

	if ((ret = version_check()) != 0)
		return (ret);

	checkpoint = data_cnt = data_next = exitval = update = verbose = 0;
	data_dir = NULL;
	backup_dir = home = log_dir = passwd = NULL;
	copy_min = remove_max = 0;
	while ((ch = getopt(argc, argv, "b:cd:h:l:P:uVv")) != EOF)
		switch (ch) {
		case 'b':
			backup_dir = optarg;
			break;
		case 'c':
			checkpoint = 1;
			break;
		case 'd':
			/*
			 * User can specify a list of directories -- keep an
			 * array, leaving room for the trailing NULL.
			 */
			if (data_dir == NULL || data_next >= data_cnt - 2) {
				data_cnt = data_cnt == 0 ? 20 : data_cnt * 2;
				if ((data_dir = realloc(data_dir,
				    data_cnt * sizeof(*data_dir))) == NULL) {
					fprintf(stderr, "%s: %s\n",
					    progname, strerror(errno));
					return (EXIT_FAILURE);
				}
			}
			data_dir[data_next++] = optarg;
			break;
		case 'h':
			home = optarg;
			break;
		case 'l':
			log_dir = optarg;
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
		case 'u':
			update = 1;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		return (usage());

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * The home directory defaults to the environment variable DB_HOME.
	 * The log directory defaults to the home directory.
	 *
	 * We require a source database environment directory and a target
	 * backup directory.
	 */
	if (home == NULL)
		home = getenv("DB_HOME");
	if (home == NULL) {
		fprintf(stderr,
		    "%s: no source database environment specified\n", progname);
		return (usage());
	}
	if (log_dir == NULL)
		log_dir = home;
	if (backup_dir == NULL) {
		fprintf(stderr,
		    "%s: no target backup directory specified\n", progname);
		return (usage());
	}

	/* NULL-terminate any list of data directories. */
	if (data_dir != NULL)
		data_dir[data_next] = NULL;

	if (verbose) {
		(void)time(&now);
		printf("%s: hot backup started at %s", progname, ctime(&now));
	}

	/* Open the source environment. */
	if ((ret = env_init(&dbenv, home, log_dir, passwd, OPEN_ORIGINAL)) != 0)
		goto shutdown;

	/*
	 * If the -c option is specified, checkpoint the source home
	 * database environment, and remove any unnecessary log files.
	 */
	if (checkpoint) {
		if (verbose)
			printf("%s: %s: force checkpoint\n", progname, home);
		if ((ret =
		    dbenv->txn_checkpoint(dbenv, 0, 0, DB_FORCE)) != 0) {
			dbenv->err(dbenv, ret, "DB_ENV->txn_checkpoint");
			goto shutdown;
		}
		if (!update) {
			if (verbose)
				printf("%s: %s: remove unnecessary log files\n",
				    progname, home);
			if ((ret = dbenv->log_archive(dbenv,
			     NULL, DB_ARCH_REMOVE)) != 0) {
				dbenv->err(dbenv, ret, "DB_ENV->log_archive");
				goto shutdown;
			}
		}
	}

	/*
	 * If the target directory for the backup does not exist, create it
	 * with mode read-write-execute for the owner.  Ignore errors here,
	 * it's simpler and more portable to just always try the create.  If
	 * there's a problem, we'll fail with reasonable errors later.
	 */
	(void)__os_mkdir(NULL, backup_dir, __db_omode("rwx------"));

	/*
	 * If the target directory for the backup does exist and the -u option
	 * was specified, all log files in the target directory are removed;
	 * if the -u option was not specified, all files in the target directory
	 * are removed.
	 */
	if ((ret = backup_dir_clean(
	    dbenv, backup_dir, &remove_max, update, verbose)) != 0)
		goto shutdown;

	/*
	 * If the -u option was not specified, copy all database files found in
	 * the database environment home directory, or any directory specified
	 * using the -d option, into the target directory for the backup.
	 */
	if (!update) {
		if (read_data_dir(dbenv, backup_dir, home, verbose) != 0)
			goto shutdown;
		if (data_dir != NULL)
			for (dir = &data_dir[0]; *dir != NULL; ++dir)
				if (read_data_dir(
				    dbenv, backup_dir, *dir, verbose) != 0)
					goto shutdown;
	}

	/*
	 * Copy all log files found in the directory specified by the -l option
	 * (or in the database environment home directory, if no -l option was
	 * specified), into the target directory for the backup.
	 *
	 * The log directory defaults to the home directory.
	 */
	if (read_log_dir(dbenv,
	     backup_dir, log_dir, &copy_min, update, verbose) != 0)
		goto shutdown;

	/*
	 * If we're updating a snapshot, the lowest-numbered log file copied
	 * into the backup directory should be less than, or equal to, the
	 * highest-numbered log file removed from the backup directory during
	 * cleanup.
	 */
	if (update && remove_max < copy_min &&
	     !(remove_max == 0 && copy_min == 1)) {
		fprintf(stderr,
		    "%s: the largest log file removed (%d) must be greater\n",
		    progname, remove_max);
		fprintf(stderr,
		    "%s: than or equal the smallest log file copied (%d)\n",
		    progname, copy_min);
		goto shutdown;
	}

	/* Close the source environment. */
	if ((ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
		dbenv = NULL;
		goto shutdown;
	}
	/* Perform catastrophic recovery on the hot backup. */
	if (verbose)
		printf("%s: %s: run catastrophic recovery\n",
		    progname, backup_dir);
	if ((ret = env_init(
	    &dbenv, backup_dir, NULL, passwd, OPEN_HOT_BACKUP)) != 0)
		goto shutdown;

	/*
	 * Remove any unnecessary log files from the hot backup.
	 */
	if (verbose)
		printf("%s: %s: remove unnecessary log files\n",
		    progname, backup_dir);
	if ((ret =
	    dbenv->log_archive(dbenv, NULL, DB_ARCH_REMOVE)) != 0) {
		dbenv->err(dbenv, ret, "DB_ENV->log_archive");
		goto shutdown;
	}

	if (0) {
shutdown:	exitval = 1;
	}
	if (dbenv != NULL && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	if (data_dir != NULL)
		free(data_dir);
	if (passwd != NULL)
		free(passwd);

	if (exitval == 0) {
		if (verbose) {
			(void)time(&now);
			printf("%s: hot backup completed at %s",
			    progname, ctime(&now));
		}
	} else {
		fprintf(stderr, "%s: HOT BACKUP FAILED!\n", progname);
	}

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval == 0 ? EXIT_SUCCESS : EXIT_FAILURE);

}

/*
 * env_init --
 *	Open a database environment.
 */
int
env_init(dbenvp, home, log_dir, passwd, which)
	DB_ENV **dbenvp;
	char *home, *log_dir, *passwd;
	enum which_open which;
{
	DB_ENV *dbenv;
	int ret;

	*dbenvp = NULL;

	/*
	 * Create an environment object and initialize it for error reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		return (1);
	}

	dbenv->set_errfile(dbenv, stderr);
	setbuf(stderr, NULL);
	dbenv->set_errpfx(dbenv, progname);
	setvbuf(stdout, NULL, _IOLBF, 0);

	/*
	 * If a log directory has been specified, and it's not the same as the
	 * home directory, set it for the environment.
	 */
	if (log_dir != NULL && log_dir != home &&
	    (ret = dbenv->set_lg_dir(dbenv, log_dir)) != 0) {
		dbenv->err(dbenv, ret, "DB_ENV->set_lg_dir: %s", log_dir);
		return (1);
	}

	/* Optionally set the password. */
	if (passwd != NULL &&
	    (ret = dbenv->set_encrypt(dbenv, passwd, DB_ENCRYPT_AES)) != 0) {
		dbenv->err(dbenv, ret, "DB_ENV->set_encrypt");
		return (1);
	}

	switch (which) {
	case OPEN_ORIGINAL:
		/*
		 * Opening the database environment we're trying to back up.
		 * We try to attach to a pre-existing environment; if that
		 * fails, we create a private environment and try again.
		 */
		if ((ret = dbenv->open(dbenv, home, DB_USE_ENVIRON, 0)) != 0 &&
		    (ret == DB_VERSION_MISMATCH ||
		    (ret = dbenv->open(dbenv, home, DB_CREATE |
		    DB_INIT_LOG | DB_INIT_TXN | DB_PRIVATE | DB_USE_ENVIRON,
		    0)) != 0)) {
			dbenv->err(dbenv, ret, "DB_ENV->open: %s", home);
			return (1);
		}
		break;
	case OPEN_HOT_BACKUP:
		/*
		 * Opening the backup copy of the database environment.  We
		 * better be the only user, we're running recovery.
		 */
		if ((ret = dbenv->open(dbenv, home, DB_CREATE |
		    DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_PRIVATE |
		    DB_RECOVER_FATAL | DB_USE_ENVIRON, 0)) != 0) {
			dbenv->err(dbenv, ret, "DB_ENV->open: %s", home);
			return (1);
		}
		break;
	}

	*dbenvp = dbenv;
	return (0);
}

/*
 * backup_dir_clean --
 *	Clean out the backup directory.
 */
int
backup_dir_clean(dbenv, backup_dir, remove_maxp, update, verbose)
	DB_ENV *dbenv;
	char *backup_dir;
	int *remove_maxp, update, verbose;
{
	int cnt, fcnt, ret, v;
	char **names;
	char buf[2048];			/* MAXPATHLEN is too hard to find. */

	/* Get a list of file names. */
	if ((ret = __os_dirlist(dbenv, backup_dir, &names, &fcnt)) != 0) {
		dbenv->err(dbenv, ret, "%s: directory read", backup_dir);
		return (1);
	}
	for (cnt = fcnt; --cnt >= 0;) {
		/*
		 * Skip ".", ".." and log files (if update wasn't specified).
		 */
		if (!strcmp(names[cnt], ".") || !strcmp(names[cnt], ".."))
			continue;
		if (strncmp(names[cnt], LFPREFIX, sizeof(LFPREFIX) - 1)) {
			if (update)
				continue;
		} else {
			/* Track the highest-numbered log file removed. */
			v = atoi(names[cnt] + sizeof(LFPREFIX) - 1);
			if (*remove_maxp < v)
				*remove_maxp = v;
		}
		if ((size_t)snprintf(buf, sizeof(buf),
		    "%s/%s", backup_dir, names[cnt]) == sizeof(buf)) {
			dbenv->err(dbenv, ret,
			    "%s/%s: path too long", backup_dir, names[cnt]);
			return (1);
		}
		if (verbose)
			printf("%s: removing %s\n", progname, buf);
		if ((ret = remove(buf)) != 0) {
			dbenv->err(dbenv, ret, "%s: remove", buf);
			return (1);
		}
	}

	__os_dirfree(dbenv, names, fcnt);

	if (verbose && *remove_maxp != 0)
		printf("%s: highest numbered log file removed: %d\n",
		    progname, *remove_maxp);

	return (0);
}

/*
 * read_data_dir --
 *	Read a directory looking for databases to copy.
 */
int
read_data_dir(dbenv, backup_dir, dir, verbose)
	DB_ENV *dbenv;
	char *backup_dir, *dir;
	int verbose;
{
	int cnt, fcnt, ret;
	char **names;
	char buf[2048];			/* MAXPATHLEN is too hard to find. */

	/* Get a list of file names. */
	if ((ret = __os_dirlist(dbenv, dir, &names, &fcnt)) != 0) {
		dbenv->err(dbenv, ret, "%s: directory read", dir);
		return (1);
	}
	for (cnt = fcnt; --cnt >= 0;) {
		/*
		 * Skip ".", ".." and files in DB's name space (but not Queue
		 * extent files, we need them).
		 */
		if (!strcmp(names[cnt], ".") || !strcmp(names[cnt], ".."))
			continue;
		if (!strncmp(names[cnt], LFPREFIX, sizeof(LFPREFIX) - 1))
			continue;
		if (!strncmp(names[cnt],
		    DB_REGION_PREFIX, sizeof(DB_REGION_PREFIX) - 1))
			continue;

		/* Build a path name to the source. */
		if ((size_t)snprintf(buf, sizeof(buf),
		    "%s/%s", dir, names[cnt]) == sizeof(buf)) {
			dbenv->errx(dbenv,
			    "%s/%s: path too long", dir, names[cnt]);
			return (1);
		}

		/* Copy the file. */
		if ((ret = data_copy(
		    dbenv, buf, backup_dir, names[cnt], verbose)) != 0)
			return (1);
	}

	__os_dirfree(dbenv, names, fcnt);

	return (0);
}

/*
 * read_log_dir --
 *	Read a directory looking for log files to copy.
 */
int
read_log_dir(dbenv, backup_dir, log_dir, copy_minp, update, verbose)
	DB_ENV *dbenv;
	char *backup_dir, *log_dir;
	int *copy_minp, update, verbose;
{
	int aflag, ret, v;
	char **begin, **names;
	char from[2048], to[2048];	/* MAXPATHLEN is too hard to find. */

again:	aflag = DB_ARCH_LOG;

	/*
	 * If this is an update and we are deleting files, first process
	 * those files that can be removed, then repeat with the rest.
	 */
	if (update)
		aflag = 0;
	/* Get a list of file names to be copied. */
	if ((ret = dbenv->log_archive(dbenv, &names, aflag)) != 0) {
		dbenv->err(dbenv, ret, "%s: log_archive", log_dir);
		return (1);
	}
	if (names == NULL)
		goto done;
	begin = names;
	for (; *names != NULL; names++) {
		/* Track the lowest-numbered log file copied. */
		v = atoi(*names + sizeof(LFPREFIX) - 1);
		if (*copy_minp == 0 || *copy_minp > v)
			*copy_minp = v;

		/* Build a path name to the source. */
		if ((size_t)snprintf(from, sizeof(from),
		    "%s/%s", log_dir, *names) == sizeof(from)) {
			dbenv->errx(dbenv,
			    "%s/%s: path too long", log_dir, *names);
			return (1);
		}

		/*
		 * If we're going to remove the file, attempt to rename the
		 * instead of copying and then removing.  The likely failure
		 * is EXDEV (source and destination are on different volumes).
		 * Fall back to a copy, regardless of the error.  We don't
		 * worry about partial contents, the copy truncates the file
		 * on open.
		 */
		if (update) {
			if ((size_t)snprintf(to, sizeof(to),
			    "%s/%s", backup_dir, *names) == sizeof(to)) {
				dbenv->errx(dbenv,
				    "%s/%s: path too long", backup_dir, *names);
				return (1);
			}
			if (rename(from, to) == 0) {
				if (verbose)
					printf("%s: moving %s to %s\n",
					   progname, from, to);
				continue;
			}
		}

		/* Copy the file. */
		if ((ret = data_copy(dbenv,
		    from, backup_dir, *names, verbose)) != 0)
			return (1);

		if (update) {
			if (verbose)
				printf("%s: removing %s\n", progname, from);
			if ((ret = __os_unlink(dbenv, from)) != 0) {
				dbenv->err(dbenv, ret,
				     "unlink of %s failed", from);
				return (1);
			}
		}

	}

	free(begin);
done:	if (update) {
		update = 0;
		goto again;
	}

	if (verbose && *copy_minp != 0)
		printf("%s: lowest numbered log file copied: %d\n",
		    progname, *copy_minp);

	return (0);
}

/*
 * data_copy --
 *	Copy a file into the backup directory.
 */
int
data_copy(dbenv, from, to_dir, to_file, verbose)
	DB_ENV *dbenv;
	char *from, *to_dir, *to_file;
	int verbose;
{
	ssize_t nr, nw;
	size_t offset;
	int ret, rfd, wfd;
	char *buf, *taddr;

	ret = 0;
	rfd = wfd = -1;

	if (verbose)
		printf("%s: copying %s to %s/%s\n",
		    progname, from, to_dir, to_file);

	/*
	 * We MUST copy multiples of the page size, atomically, to ensure a
	 * database page is not updated by another thread of control during
	 * the copy.
	 *
	 * !!!
	 * The current maximum page size for Berkeley DB is 64KB; we will have
	 * to increase this value if the maximum page size is ever more than a
	 * megabyte
	 */
	if ((buf = malloc(MEGABYTE)) == NULL) {
		dbenv->err(dbenv,
		    errno, "%lu buffer allocation", (u_long)MEGABYTE);
		return (1);
	}

	/* Open the input file. */
	if ((rfd = open(from, O_RDONLY, 0)) == -1) {
		dbenv->err(dbenv, errno, "%s", from);
		goto err;
	}

	/* Open the output file. */
	if ((u_int32_t)snprintf(
	    buf, MEGABYTE, "%s/%s", to_dir, to_file) == MEGABYTE) {
		dbenv->errx(dbenv, "%s/%s: path too long", to_dir, to_file);
		goto err;
	}
	if ((wfd = open(
	    buf, O_CREAT | O_TRUNC | O_WRONLY, __db_omode(OWNER_RW))) == -1)
		goto err;

	/* Copy the data. */
	while ((nr = read(rfd, buf, MEGABYTE)) > 0)
		for (taddr = buf, offset = 0;
		    offset < (size_t)nr; taddr += nw, offset += (size_t)nw) {
			RETRY_CHK(((nw = write(wfd,
			    taddr, (u_int)(nr - offset))) < 0 ? 1 : 0), ret);
			if (ret != 0)
				break;
		}
	if (nr == -1) {
		dbenv->err(dbenv, errno, "%s: read", from);
		goto err;
	}

	if (ret != 0) {
		dbenv->err(dbenv, errno, "%s: write %s/%s", to_dir, to_file);
		goto err;
	}

	if (0) {
err:		ret = 1;
	}
	if (buf != NULL)
		free(buf);

	if (rfd != -1)
		(void)close(rfd);

	/* We may be running on a remote filesystem; force the flush. */
	if (wfd != -1 && (fsync(wfd) != 0 || close(wfd) != 0)) {
		dbenv->err(dbenv,
		    errno, "%s: fsync %s/%s", to_dir, to_file);
		ret = 1;
	}
	return (ret);
}

int
usage()
{
	(void)fprintf(stderr, "usage: %s [-cuVv]\n\t%s\n", progname,
    "[-d data_dir ...] [-h home] [-l log_dir] [-P password] -b backup_dir");
	return (EXIT_FAILURE);
}

int
version_check()
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
