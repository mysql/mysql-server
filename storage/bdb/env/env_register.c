/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_register.c,v 1.15 2005/10/07 20:21:27 ubell Exp $
 */
#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"

#define	REGISTER_FILE	"__db.register"

#define	PID_EMPTY	"X%23lu\n"	/* An unused PID entry. */
#define	PID_FMT		"%24lu\n"	/* File PID format. */
#define	PID_ISEMPTY(p)	(p[0] == 'X')
#define	PID_LEN		25		/* Length of PID line. */

#define	REGISTRY_LOCK(dbenv, pos, nowait)				\
	__os_fdlock(dbenv, (dbenv)->registry, (off_t)(pos), 1, nowait)
#define	REGISTRY_UNLOCK(dbenv, pos)					\
	__os_fdlock(dbenv, (dbenv)->registry, (off_t)(pos), 0, 0)
#define	REGISTRY_EXCL_LOCK(dbenv, nowait)				\
	REGISTRY_LOCK(dbenv, 1, nowait)
#define	REGISTRY_EXCL_UNLOCK(dbenv)					\
	REGISTRY_UNLOCK(dbenv, 1)

static  int __envreg_add __P((DB_ENV *, int *));

/*
 * Support for portable, multi-process database environment locking, based on
 * the Subversion SR (#11511).
 *
 * The registry feature is configured by specifying the DB_REGISTER flag to the
 * DbEnv.open method.  If DB_REGISTER is specified, DB opens the registry file
 * in the database environment home directory.  The registry file is formatted
 * as follows:
 *
 *	                    12345		# process ID slot 1
 *	X		# empty slot
 *	                    12346		# process ID slot 2
 *	X		# empty slot
 *	                    12347		# process ID slot 3
 *	                    12348		# process ID slot 4
 *	X                   12349		# empty slot
 *	X		# empty slot
 *
 * All lines are fixed-length.  All lines are process ID slots.  Empty slots
 * are marked with leading non-digit characters.
 *
 * To modify the file, you get an exclusive lock on the first byte of the file.
 *
 * While holding any DbEnv handle, each process has an exclusive lock on the
 * first byte of a process ID slot.  There is a restriction on having more
 * than one DbEnv handle open at a time, because Berkeley DB uses per-process
 * locking to implement this feature, that is, a process may never have more
 * than a single slot locked.
 *
 * This work requires that if a process dies or the system crashes, locks held
 * by the dying processes will be dropped.  (We can't use system shared
 * memory-backed or filesystem-backed locks because they're persistent when a
 * process dies.)  On POSIX systems, we use fcntl(2) locks; on Win32 we have
 * LockFileEx/UnlockFile, except for Win/9X and Win/ME which have to loop on
 * Lockfile/UnlockFile.
 *
 * We could implement the same solution with flock locking instead of fcntl,
 * but flock would require a separate file for each process of control (and
 * probably each DbEnv handle) in the database environment, which is fairly
 * ugly.
 *
 * Whenever a process opens a new DbEnv handle, it walks the registry file and
 * verifies it CANNOT acquire the lock for any non-empty slot.  If a lock for
 * a non-empty slot is available, we know a process died holding an open handle,
 * and recovery needs to be run.
 *
 * There can still be processes running in the environment when we recover it,
 * and, in fact, there can still be processes running in the old environment
 * after we're up and running in a new one.  This is safe because performing
 * recovery panics (and removes) the existing environment, so the window of
 * vulnerability is small.  Further, we check the panic flag in the DB API
 * methods, when waking from spinning on a mutex, and whenever we're about to
 * write to disk).  The only window of corruption is if the write check of the
 * panic were to complete, the region subsequently be recovered, and then the
 * write continues.  That's very, very unlikely to happen.  This vulnerability
 * already exists in Berkeley DB, too, the registry code doesn't make it any
 * worse than it already is.
 */
/*
 * __envreg_register --
 *	Register a DB_ENV handle.
 *
 * PUBLIC: int __envreg_register __P((DB_ENV *, const char *, int *));
 */
int
__envreg_register(dbenv, db_home, need_recoveryp)
	DB_ENV *dbenv;
	const char *db_home;
	int *need_recoveryp;
{
	pid_t pid;
	db_threadid_t tid;
	u_int32_t bytes, mbytes;
	int ret;
	char path[MAXPATHLEN];

	*need_recoveryp = 0;
	dbenv->thread_id(dbenv, &pid, &tid);

	if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
		__db_msg(dbenv, "%lu: register environment", (u_long)pid);

	/* Build the path name and open the registry file. */
	(void)snprintf(path, sizeof(path), "%s/%s", db_home, REGISTER_FILE);
	if ((ret = __os_open(dbenv, path,
	    DB_OSO_CREATE, __db_omode("rw-rw----"), &dbenv->registry)) != 0)
		goto err;

	/*
	 * Wait for an exclusive lock on the file.
	 *
	 * !!!
	 * We're locking bytes that don't yet exist, but that's OK as far as
	 * I know.
	 */
	if ((ret = REGISTRY_EXCL_LOCK(dbenv, 0)) != 0)
		goto err;

	/*
	 * If the file size is 0, initialize the file.
	 *
	 * Run recovery if we create the file, that means we can clean up the
	 * system by removing the registry file and restarting the application.
	 */
	if ((ret = __os_ioinfo(
	    dbenv, path, dbenv->registry, &mbytes, &bytes, NULL)) != 0)
		goto err;
	if (mbytes == 0 && bytes == 0) {
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
			__db_msg(dbenv,
			    "%lu: creating %s", (u_long)pid, path);
		*need_recoveryp = 1;
	}

	/* Register this process. */
	if ((ret = __envreg_add(dbenv, need_recoveryp)) != 0)
		goto err;

	/*
	 * Release our exclusive lock if we don't need to run recovery.  If
	 * we need to run recovery, DB_ENV->open will call back into register
	 * code once recovery has completed.
	 */
	if (*need_recoveryp == 0 && (ret = REGISTRY_EXCL_UNLOCK(dbenv)) != 0)
		goto err;

	if (0) {
err:		*need_recoveryp = 0;

		/*
		 * !!!
		 * Closing the file handle must release all of our locks.
		 */
		(void)__os_closehandle(dbenv, dbenv->registry);
		dbenv->registry = NULL;
	}

	return (ret);
}

/*
 * __envreg_add --
 *	Add the process' pid to the register.
 */
static int
__envreg_add(dbenv, need_recoveryp)
	DB_ENV *dbenv;
	int *need_recoveryp;
{
	pid_t pid;
	db_threadid_t tid;
	off_t end, pos;
	size_t nr, nw;
	u_int lcnt;
	u_int32_t bytes, mbytes;
	int need_recovery, ret;
	char *p, buf[256], pid_buf[256];

	need_recovery = 0;
	COMPQUIET(p, NULL);

	/* Get a copy of our process ID. */
	dbenv->thread_id(dbenv, &pid, &tid);
	snprintf(pid_buf, sizeof(pid_buf), PID_FMT, (u_long)pid);

	if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
		__db_msg(dbenv, "===== %lu: before add", (u_long)pid);

	/*
	 * Read the file.  Skip empty slots, and check that a lock is held
	 * for any allocated slots.  An allocated slot which we can lock
	 * indicates a process died holding a handle and recovery needs to
	 * be run.
	 */
	for (lcnt = 0;; ++lcnt) {
		if ((ret = __os_read(
		    dbenv, dbenv->registry, buf, PID_LEN, &nr)) != 0)
			return (ret);
		if (nr == 0)
			break;
		if (nr != PID_LEN)
			goto corrupt;

		if (FLD_ISSET(
		    dbenv->verbose, DB_VERB_REGISTER) && PID_ISEMPTY(buf)) {
			__db_msg(dbenv, "%02u: EMPTY", lcnt);
			continue;
		}

		/*
		 * !!!
		 * DB_REGISTER is implemented using per-process locking, only
		 * a single DB_ENV handle may be open per process.  Enforce
		 * that restriction.
		 */
		if (memcmp(buf, pid_buf, PID_LEN) == 0) {
			__db_err(dbenv,
	"DB_REGISTER limits each process to a single open DB_ENV handle");
			return (EINVAL);
		}

		if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER)) {
			for (p = buf; *p == ' ';)
				++p;
			buf[nr - 1] = '\0';
		}

		pos = (off_t)lcnt * PID_LEN;
		if (REGISTRY_LOCK(dbenv, pos, 1) == 0) {
			if ((ret = REGISTRY_UNLOCK(dbenv, pos)) != 0)
				return (ret);

			if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
				__db_msg(dbenv, "%02u: %s: FAILED", lcnt, p);

			need_recovery = 1;
			break;
		} else
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
				__db_msg(dbenv, "%02u: %s: LOCKED", lcnt, p);
	}

	/*
	 * If we have to perform recovery...
	 *
	 * Mark all slots empty.  Registry ignores empty slots we can't lock,
	 * so it doesn't matter if any of the processes are in the middle of
	 * exiting Berkeley DB -- they'll discard their lock when they exit.
	 */
	if (need_recovery) {
		/* Figure out how big the file is. */
		if ((ret = __os_ioinfo(
		    dbenv, NULL, dbenv->registry, &mbytes, &bytes, NULL)) != 0)
			return (ret);
		end = (off_t)mbytes * MEGABYTE + bytes;

		/* Confirm the file is of a reasonable size. */
		DB_ASSERT(end % PID_LEN == 0);

		/*
		 * Seek to the beginning of the file and overwrite slots to
		 * the end of the file.
		 */
		if ((ret = __os_seek(
		    dbenv, dbenv->registry, 0, 0, 0, 0, DB_OS_SEEK_SET)) != 0)
			return (ret);
		snprintf(buf, sizeof(buf), PID_EMPTY, (u_long)0);
		for (lcnt = (u_int)end / PID_LEN; lcnt > 0; --lcnt)
			if ((ret = __os_write(
			    dbenv, dbenv->registry, buf, PID_LEN, &nw)) != 0 ||
			    nw != PID_LEN)
				goto corrupt;
	}

	/*
	 * Seek to the first process slot and add ourselves to the first empty
	 * slot we can lock.
	 */
	if ((ret = __os_seek(
	    dbenv, dbenv->registry, 0, 0, 0, 0, DB_OS_SEEK_SET)) != 0)
		return (ret);
	for (lcnt = 0;; ++lcnt) {
		if ((ret = __os_read(
		    dbenv, dbenv->registry, buf, PID_LEN, &nr)) != 0)
			return (ret);
		if (nr == PID_LEN && !PID_ISEMPTY(buf))
			continue;
		pos = (off_t)lcnt * PID_LEN;
		if (REGISTRY_LOCK(dbenv, pos, 1) == 0) {
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
				__db_msg(dbenv,
				    "%lu: locking slot %02u at offset %lu",
				    (u_long)pid, lcnt, (u_long)pos);

			if ((ret = __os_seek(dbenv, dbenv->registry,
			    0, 0, (u_int32_t)pos, 0, DB_OS_SEEK_SET)) != 0 ||
			    (ret = __os_write(dbenv,
			    dbenv->registry, pid_buf, PID_LEN, &nw)) != 0 ||
			    nw != PID_LEN)
				return (ret);
			dbenv->registry_off = (u_int32_t)pos;
			break;
		}
	}

	if (need_recovery)
		*need_recoveryp = 1;

	if (0) {
corrupt:	__db_err(dbenv, "%s: file contents corrupted", REGISTER_FILE);
		return (ret == 0 ? EACCES : ret);
	}

	return (ret);
}

/*
 * __envreg_unregister --
 *	Unregister a DB_ENV handle.
 *
 * PUBLIC: int __envreg_unregister __P((DB_ENV *, int));
 */
int
__envreg_unregister(dbenv, recovery_failed)
	DB_ENV *dbenv;
	int recovery_failed;
{
	size_t nw;
	int ret, t_ret;
	char buf[256];

	ret = 0;

	/*
	 * If recovery failed, we want to drop our locks and return, but still
	 * make sure any subsequent process doesn't decide everything is just
	 * fine and try to get into the database environment.  In the case of
	 * an error, discard our locks, but leave our slot filled-in.
	 */
	if (recovery_failed)
		goto err;

	/*
	 * Why isn't an exclusive lock necessary to discard a DB_ENV handle?
	 *
	 * We mark our process ID slot empty before we discard the process slot
	 * lock, and threads of control reviewing the register file ignore any
	 * slots which they can't lock.
	 */
	snprintf(buf, sizeof(buf), PID_EMPTY, (u_long)0);
	if ((ret = __os_seek(dbenv, dbenv->registry,
	    0, 0, dbenv->registry_off, 0, DB_OS_SEEK_SET)) != 0 ||
	    (ret = __os_write(
	    dbenv, dbenv->registry, buf, PID_LEN, &nw)) != 0 ||
	    nw != PID_LEN)
		goto err;

	/*
	 * !!!
	 * This code assumes that closing the file descriptor discards all
	 * held locks.
	 *
	 * !!!
	 * There is an ordering problem here -- in the case of a process that
	 * failed in recovery, we're unlocking both the exclusive lock and our
	 * slot lock.  If the OS unlocked the exclusive lock and then allowed
	 * another thread of control to acquire the exclusive lock before also
	 * also releasing our slot lock, we could race.  That can't happen, I
	 * don't think.
	 */
err:	if ((t_ret =
	    __os_closehandle(dbenv, dbenv->registry)) != 0 && ret == 0)
		ret = t_ret;

	dbenv->registry = NULL;
	return (ret);
}

/*
 * __envreg_xunlock --
 *	Discard the exclusive lock held by the DB_ENV handle.
 *
 * PUBLIC: int __envreg_xunlock __P((DB_ENV *));
 */
int
__envreg_xunlock(dbenv)
	DB_ENV *dbenv;
{
	pid_t pid;
	db_threadid_t tid;
	int ret;

	dbenv->thread_id(dbenv, &pid, &tid);

	if (FLD_ISSET(dbenv->verbose, DB_VERB_REGISTER))
		__db_msg(dbenv,
		    "%lu: recovery completed, unlocking", (u_long)pid);

	if ((ret = REGISTRY_EXCL_UNLOCK(dbenv)) == 0)
		return (ret);

	__db_err(dbenv,
	    "%s: exclusive file unlock: %s", REGISTER_FILE, db_strerror(ret));
	return (__db_panic(dbenv, ret));
}
