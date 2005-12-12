/*
 * Standalone mutex tester for Berkeley DB mutexes.
 *
 * $Id: tm.c,v 12.10 2005/10/21 17:53:04 bostic Exp $
 */
#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(MUTEX_THREAD_TEST)
#include <pthread.h>
#endif
#endif

#include "db_int.h"

#ifdef DB_WIN32
extern int getopt(int, char * const *, const char *);

typedef HANDLE os_pid_t;
typedef HANDLE os_thread_t;

#define	os_thread_create(thrp, attr, func, arg)				\
    (((*(thrp) = CreateThread(NULL, 0,					\
	(LPTHREAD_START_ROUTINE)(func), (arg), 0, NULL)) == NULL) ? -1 : 0)
#define	os_thread_join(thr, statusp)					\
    ((WaitForSingleObject((thr), INFINITE) == WAIT_OBJECT_0) &&		\
    GetExitCodeThread((thr), (LPDWORD)(statusp)) ? 0 : -1)
#define	os_thread_self() GetCurrentThreadId()

#else /* !DB_WIN32 */

typedef pid_t os_pid_t;

#ifdef MUTEX_THREAD_TEST
typedef pthread_t os_thread_t;
#endif

#define	os_thread_create(thrp, attr, func, arg)				\
    pthread_create((thrp), (attr), (func), (arg))
#define	os_thread_join(thr, statusp) pthread_join((thr), (statusp))
#define	os_thread_self() pthread_self()
#endif

#define	OS_BAD_PID (os_pid_t)-1

#define	TESTDIR		"TESTDIR"		/* Working area */
#define	MT_FILE		"TESTDIR/mutex.file"
#define	MT_FILE_QUIT	"TESTDIR/mutex.file.quit"

/*
 * The backing file layout:
 *	TM[1]			per-thread mutex array lock
 *	TM[nthreads]		per-thread mutex array
 *	TM[maxlocks]		per-lock mutex array
 */
typedef struct {
	db_mutex_t mutex;			/* Mutex. */
	u_long	   id;				/* Holder's ID. */
	u_int	   wakeme;			/* Request to awake. */
} TM;

DB_ENV	*dbenv;					/* Backing environment */
size_t	 len;					/* Backing file size. */

u_int8_t *gm_addr;				/* Global mutex */
u_int8_t *lm_addr;				/* Locker mutexes */
u_int8_t *tm_addr;				/* Thread mutexes */

#ifdef MUTEX_THREAD_TEST
os_thread_t *kidsp;				/* Locker threads */
os_thread_t  wakep;				/* Wakeup thread */
#endif

int	 maxlocks = 20;				/* -l: Backing locks. */
int	 nlocks = 10000;			/* -n: Locks per processes. */
int	 nprocs = 20;				/* -p: Processes. */
int	 nthreads = 1;				/* -t: Threads. */
int	 verbose;				/* -v: Verbosity. */

int	 locker_start(u_long);
int	 locker_wait(void);
void	 map_file(u_int8_t **, u_int8_t **, u_int8_t **, DB_FH **);
os_pid_t os_spawn(const char *, char *const[]);
int	 os_wait(os_pid_t *, int);
void	*run_lthread(void *);
void	*run_wthread(void *);
os_pid_t spawn_proc(u_long, char *, char *);
void	 tm_env_close(void);
int	 tm_env_init(void);
void	 tm_file_init(void);
void	 tm_mutex_destroy(void);
void	 tm_mutex_init(void);
void	 tm_mutex_stats(void);
void	 unmap_file(u_int8_t *, DB_FH *);
int	 usage(void);
int	 wakeup_start(u_long);
int	 wakeup_wait(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	enum {LOCKER, WAKEUP, PARENT} rtype;
	extern int optind;
	extern char *optarg;
	os_pid_t wakeup_pid, *pids;
	u_long id;
	DB_FH *fhp, *map_fhp;
	int ch, err, i;
	char *p, *tmpath, cmd[1024];

	rtype = PARENT;
	id = 0;
	tmpath = argv[0];
	while ((ch = getopt(argc, argv, "l:n:p:T:t:v")) != EOF)
		switch (ch) {
		case 'l':
			maxlocks = atoi(optarg);
			break;
		case 'n':
			nlocks = atoi(optarg);
			break;
		case 'p':
			nprocs = atoi(optarg);
			break;
		case 't':
			if ((nthreads = atoi(optarg)) == 0)
				nthreads = 1;
#if !defined(MUTEX_THREAD_TEST)
			if (nthreads != 1) {
				(void)fprintf(stderr,
    "tm: thread support not available or not compiled for this platform.\n");
				return (EXIT_FAILURE);
			}
#endif
			break;
		case 'T':
			if (!memcmp(optarg, "locker", sizeof("locker") - 1))
				rtype = LOCKER;
			else if (
			    !memcmp(optarg, "wakeup", sizeof("wakeup") - 1))
				rtype = WAKEUP;
			else
				return (usage());
			if ((p = strchr(optarg, '=')) == NULL)
				return (usage());
			id = atoi(p + 1);
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	/*
	 * If we're not running a multi-process test, we should be running
	 * a multi-thread test.
	 */
	if (nprocs == 1 && nthreads == 1) {
		fprintf(stderr,
	    "tm: running in a single process requires multiple threads\n");
		return (EXIT_FAILURE);
	}

	len = sizeof(TM) * (1 + nthreads * nprocs + maxlocks);

	/*
	 * In the multi-process test, the parent spawns processes that exec
	 * the original binary, ending up here.  Each process joins the DB
	 * environment separately and then calls the supporting function.
	 */
	if (rtype == LOCKER || rtype == WAKEUP) {
		__os_sleep(dbenv, 3, 0);	/* Let everyone catch up. */
						/* Initialize random numbers. */
		srand((u_int)time(NULL) % getpid());

		if (tm_env_init() != 0)		/* Join the environment. */
			exit(EXIT_FAILURE);
						/* Join the backing file. */
		map_file(&gm_addr, &tm_addr, &lm_addr, &map_fhp);
		if (verbose)
			printf(
	    "Backing file: global (%#lx), threads (%#lx), locks (%#lx)\n",
			    (u_long)gm_addr, (u_long)tm_addr, (u_long)lm_addr);

		if ((rtype == LOCKER ?
		    locker_start(id) : wakeup_start(id)) != 0)
			exit(EXIT_FAILURE);
		if ((rtype == LOCKER ? locker_wait() : wakeup_wait()) != 0)
			exit(EXIT_FAILURE);

		unmap_file(gm_addr, map_fhp);	/* Detach from backing file. */

		tm_env_close();			/* Detach from environment. */

		exit(EXIT_SUCCESS);
	}

	/*
	 * The following code is only executed by the original parent process.
	 *
	 * Clean up from any previous runs.
	 */
	snprintf(cmd, sizeof(cmd), "rm -rf %s", TESTDIR);
	(void)system(cmd);
	snprintf(cmd, sizeof(cmd), "mkdir %s", TESTDIR);
	(void)system(cmd);

	printf(
    "tm: %d processes, %d threads/process, %d lock requests from %d locks\n",
	    nprocs, nthreads, nlocks, maxlocks);
	printf("tm: backing file %lu bytes\n", (u_long)len);

	if (tm_env_init() != 0)		/* Create the environment. */
		exit(EXIT_FAILURE);

	tm_file_init();			/* Initialize backing file. */

					/* Map in the backing file. */
	map_file(&gm_addr, &tm_addr, &lm_addr, &map_fhp);
	if (verbose)
		printf(
	    "backing file: global (%#lx), threads (%#lx), locks (%#lx)\n",
		    (u_long)gm_addr, (u_long)tm_addr, (u_long)lm_addr);

	tm_mutex_init();		/* Initialize mutexes. */

	if (nprocs > 1) {		/* Run the multi-process test. */
		/* Allocate array of locker process IDs. */
		if ((pids = calloc(nprocs, sizeof(os_pid_t))) == NULL) {
			fprintf(stderr, "tm: %s\n", strerror(errno));
			goto fail;
		}

		/* Spawn locker processes and threads. */
		for (i = 0; i < nprocs; ++i) {
			if ((pids[i] =
			    spawn_proc(id, tmpath, "locker")) == OS_BAD_PID) {
				fprintf(stderr,
				    "tm: failed to spawn a locker\n");
				goto fail;
			}
			id += nthreads;
		}

		/* Spawn wakeup process/thread. */
		if ((wakeup_pid =
		    spawn_proc(id, tmpath, "wakeup")) == OS_BAD_PID) {
			fprintf(stderr, "tm: failed to spawn waker\n");
			goto fail;
		}
		++id;

		/* Wait for all lockers to exit. */
		if ((err = os_wait(pids, nprocs)) != 0) {
			fprintf(stderr, "locker wait failed with %d\n", err);
			goto fail;
		}

		/* Signal wakeup process to exit. */
		if ((err = __os_open(
		    dbenv, MT_FILE_QUIT, DB_OSO_CREATE, 0664, &fhp)) != 0) {
			fprintf(stderr, "tm: open %s\n", db_strerror(err));
			goto fail;
		}
		(void)__os_closehandle(dbenv, fhp);

		/* Wait for wakeup process/thread. */
		if ((err = os_wait(&wakeup_pid, 1)) != 0) {
			fprintf(stderr,
			    "%lu: exited %d\n", (u_long)wakeup_pid, err);
			goto fail;
		}
	} else {			/* Run the single-process test. */
		/* Spawn locker threads. */
		if (locker_start(0) != 0)
			goto fail;

		/* Spawn wakeup thread. */
		if (wakeup_start(nthreads) != 0)
			goto fail;

		/* Wait for all lockers to exit. */
		if (locker_wait() != 0)
			goto fail;

		/* Signal wakeup process to exit. */
		if ((err = __os_open(
		    dbenv, MT_FILE_QUIT, DB_OSO_CREATE, 0664, &fhp)) != 0) {
			fprintf(stderr, "tm: open %s\n", db_strerror(err));
			goto fail;
		}
		(void)__os_closehandle(dbenv, fhp);

		/* Wait for wakeup thread. */
		if (wakeup_wait() != 0)
			goto fail;
	}

	tm_mutex_stats();		/* Display run statistics. */
	tm_mutex_destroy();		/* Destroy mutexes. */

	unmap_file(gm_addr, map_fhp);	/* Detach from backing file. */

	tm_env_close();			/* Detach from environment. */

	printf("tm: test succeeded\n");
	return (EXIT_SUCCESS);

fail:	printf("tm: FAILED!\n");
	return (EXIT_FAILURE);
}

int
locker_start(id)
	u_long id;
{
#if defined(MUTEX_THREAD_TEST)
	int err, i;

	/*
	 * Spawn off threads.  We have nthreads all locking and going to
	 * sleep, and one other thread cycling through and waking them up.
	 */
	if ((kidsp =
	    (os_thread_t *)calloc(sizeof(os_thread_t), nthreads)) == NULL) {
		fprintf(stderr, "tm: %s\n", strerror(errno));
		return (1);
	}
	for (i = 0; i < nthreads; i++)
		if ((err = os_thread_create(
		    &kidsp[i], NULL, run_lthread, (void *)(id + i))) != 0) {
			fprintf(stderr, "tm: failed spawning thread: %s\n",
			    db_strerror(err));
			return (1);
		}
	return (0);
#else
	return (run_lthread((void *)id) == NULL ? 0 : 1);
#endif
}

int
locker_wait()
{
#if defined(MUTEX_THREAD_TEST)
	int i;
	void *retp;

	/* Wait for the threads to exit. */
	for (i = 0; i < nthreads; i++) {
		os_thread_join(kidsp[i], &retp);
		if (retp != NULL) {
			fprintf(stderr, "tm: thread exited with error\n");
			return (1);
		}
	}
	free(kidsp);
#endif
	return (0);
}

void *
run_lthread(arg)
	void *arg;
{
	TM *gp, *mp, *tp;
	u_long id, tid;
	int err, i, lock, nl;

	id = (uintptr_t)arg;
#if defined(MUTEX_THREAD_TEST)
	tid = (u_long)os_thread_self();
#else
	tid = 0;
#endif
	printf("Locker: ID %03lu (PID: %lu; TID: %lx)\n",
	    id, (u_long)getpid(), tid);

	gp = (TM *)gm_addr;
	tp = (TM *)(tm_addr + id * sizeof(TM));

	for (nl = nlocks; nl > 0;) {
		/* Select and acquire a data lock. */
		lock = rand() % maxlocks;
		mp = (TM *)(lm_addr + lock * sizeof(TM));
		if (verbose)
			printf("%03lu: lock %d (mtx: %lu)\n",
			    id, lock, (u_long)mp->mutex);

		if ((err = dbenv->mutex_lock(dbenv, mp->mutex)) != 0) {
			fprintf(stderr, "%03lu: never got lock %d: %s\n",
			    id, lock, db_strerror(err));
			return ((void *)1);
		}
		if (mp->id != 0) {
			fprintf(stderr,
			    "RACE! (%03lu granted lock %d held by %03lu)\n",
			    id, lock, mp->id);
			return ((void *)1);
		}
		mp->id = id;

		/*
		 * Pretend to do some work, periodically checking to see if
		 * we still hold the mutex.
		 */
		for (i = 0; i < 3; ++i) {
			__os_sleep(dbenv, 0, rand() % 3);
			if (mp->id != id) {
				fprintf(stderr,
				    "RACE! (%03lu stole lock %d from %03lu)\n",
				    mp->id, lock, id);
				return ((void *)1);
			}
		}

		/*
		 * Test self-blocking and unlocking by other threads/processes:
		 *
		 *	acquire the global lock
		 *	set our wakeup flag
		 *	release the global lock
		 *	acquire our per-thread lock
		 *
		 * The wakeup thread will wake us up.
		 */
		if ((err = dbenv->mutex_lock(dbenv, gp->mutex)) != 0) {
			fprintf(stderr,
			    "%03lu: global lock: %s\n", id, db_strerror(err));
			return ((void *)1);
		}
		if (tp->id != 0 && tp->id != id) {
			fprintf(stderr,
		    "%03lu: per-thread mutex isn't mine, owned by %03lu\n",
			    id, tp->id);
			return ((void *)1);
		}
		tp->id = id;
		if (verbose)
			printf("%03lu: self-blocking (mtx: %lu)\n",
			    id, (u_long)tp->mutex);
		if (tp->wakeme) {
			fprintf(stderr,
			    "%03lu: wakeup flag incorrectly set\n", id);
			return ((void *)1);
		}
		tp->wakeme = 1;
		if ((err = dbenv->mutex_unlock(dbenv, gp->mutex)) != 0) {
			fprintf(stderr,
			    "%03lu: global unlock: %s\n", id, db_strerror(err));
			return ((void *)1);
		}
		if ((err = dbenv->mutex_lock(dbenv, tp->mutex)) != 0) {
			fprintf(stderr, "%03lu: per-thread lock: %s\n",
			    id, db_strerror(err));
			return ((void *)1);
		}
		/* Time passes... */
		if (tp->wakeme) {
			fprintf(stderr, "%03lu: wakeup flag not cleared\n", id);
			return ((void *)1);
		}

		if (verbose)
			printf("%03lu: release %d (mtx: %lu)\n",
			    id, lock, (u_long)mp->mutex);

		/* Release the data lock. */
		mp->id = 0;
		if ((err = dbenv->mutex_unlock(dbenv, mp->mutex)) != 0) {
			fprintf(stderr,
			    "%03lu: lock release: %s\n", id, db_strerror(err));
			return ((void *)1);
		}

		if (--nl % 100 == 0) {
			fprintf(stderr, "%03lu: %d\n", id, nl);
			/*
			 * Windows buffers stderr and the output looks wrong
			 * without this.
			 */
			fflush(stderr);
		}
	}

	return (NULL);
}

int
wakeup_start(id)
	u_long id;
{
#if defined(MUTEX_THREAD_TEST)
	int err;

	/*
	 * Spawn off wakeup thread.
	 */
	if ((err = os_thread_create(
	    &wakep, NULL, run_wthread, (void *)id)) != 0) {
		fprintf(stderr, "tm: failed spawning wakeup thread: %s\n",
		    db_strerror(err));
		return (1);
	}
	return (0);
#else
	return (run_wthread((void *)id) == NULL ? 0 : 1);
#endif
}

int
wakeup_wait()
{
#if defined(MUTEX_THREAD_TEST)
	void *retp;

	/*
	 * A file is created when the wakeup thread is no longer needed.
	 */
	os_thread_join(wakep, &retp);
	if (retp != NULL) {
		fprintf(stderr, "tm: wakeup thread exited with error\n");
		return (1);
	}
#endif
	return (0);
}

/*
 * run_wthread --
 *	Thread to wake up other threads that are sleeping.
 */
void *
run_wthread(arg)
	void *arg;
{
	TM *gp, *tp;
	u_long id, tid;
	int check_id, err;

	id = (uintptr_t)arg;
#if defined(MUTEX_THREAD_TEST)
	tid = (u_long)os_thread_self();
#else
	tid = 0;
#endif
	printf("Wakeup: ID %03lu (PID: %lu; TID: %lx)\n",
	    id, (u_long)getpid(), tid);

	gp = (TM *)gm_addr;

	/* Loop, waking up sleepers and periodically sleeping ourselves. */
	for (check_id = 0;; ++check_id) {
		/* Check to see if the locking threads have finished. */
		if (__os_exists(MT_FILE_QUIT, NULL) == 0)
			break;

		/* Check for ID wraparound. */
		if (check_id == nthreads * nprocs)
			check_id = 0;

		/* Check for a thread that needs a wakeup. */
		tp = (TM *)(tm_addr + check_id * sizeof(TM));
		if (!tp->wakeme)
			continue;

		if (verbose) {
			printf("%03lu: wakeup thread %03lu (mtx: %lu)\n",
			    id, tp->id, (u_long)tp->mutex);
			fflush(stdout);
		}

		/* Acquire the global lock. */
		if ((err = dbenv->mutex_lock(dbenv, gp->mutex)) != 0) {
			fprintf(stderr,
			    "wakeup: global lock: %s\n", db_strerror(err));
			return ((void *)1);
		}

		tp->wakeme = 0;
		if ((err = dbenv->mutex_unlock(dbenv, tp->mutex)) != 0) {
			fprintf(stderr,
			    "wakeup: unlock: %s\n", db_strerror(err));
			return ((void *)1);
		}

		if ((err = dbenv->mutex_unlock(dbenv, gp->mutex))) {
			fprintf(stderr,
			    "wakeup: global unlock: %s\n", db_strerror(err));
			return ((void *)1);
		}

		__os_sleep(dbenv, 0, rand() % 3);
	}
	return (NULL);
}

/*
 * tm_env_init --
 *	Create the backing database environment.
 */
int
tm_env_init()
{
	u_int32_t flags;
	int ret;
	char *home;

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr, "tm: %s\n", db_strerror(ret));
		return (1);
	}
	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, "tm");

	/* Allocate enough mutexes. */
	if ((ret = dbenv->mutex_set_increment(dbenv,
	    1 + nthreads * nprocs + maxlocks)) != 0) {
		dbenv->err(dbenv, ret, "dbenv->mutex_set_increment");
		return (1);
	}

	flags = DB_CREATE;
	if (nprocs == 1) {
		home = NULL;
		flags |= DB_PRIVATE;
	} else
		home = TESTDIR;
	if (nthreads != 1)
		flags |= DB_THREAD;
	if ((ret = dbenv->open(dbenv, home, flags, 0)) != 0) {
		dbenv->err(dbenv, ret, "environment open: %s", home);
		return (1);
	}

	return (0);
}

/*
 * tm_env_close --
 *	Close the backing database environment.
 */
void
tm_env_close()
{
	(void)dbenv->close(dbenv, 0);
}

/*
 * tm_file_init --
 *	Initialize the backing file.
 */
void
tm_file_init()
{
	DB_FH *fhp;
	int err;
	size_t nwrite;

	/* Initialize the backing file. */
	if (verbose)
		printf("Create the backing file.\n");

	(void)unlink(MT_FILE);

	if ((err = __os_open(dbenv, MT_FILE,
	    DB_OSO_CREATE | DB_OSO_TRUNC, 0666, &fhp)) == -1) {
		(void)fprintf(stderr,
		    "%s: open: %s\n", MT_FILE, db_strerror(err));
		exit(EXIT_FAILURE);
	}

	if ((err = __os_seek(dbenv, fhp,
	    0, 0, len, 0, DB_OS_SEEK_SET)) != 0 ||
	    (err = __os_write(dbenv, fhp, &err, 1, &nwrite)) != 0 ||
	    nwrite != 1) {
		(void)fprintf(stderr,
		    "%s: seek/write: %s\n", MT_FILE, db_strerror(err));
		exit(EXIT_FAILURE);
	}
	(void)__os_closehandle(dbenv, fhp);
}

/*
 * tm_mutex_init --
 *	Initialize the mutexes.
 */
void
tm_mutex_init()
{
	TM *mp;
	int err, i;

	if (verbose)
		printf("Allocate the global mutex: ");
	mp = (TM *)gm_addr;
	if ((err = dbenv->mutex_alloc(dbenv, 0, &mp->mutex)) != 0) {
		fprintf(stderr,
		    "DB_ENV->mutex_alloc (global): %s\n", db_strerror(err));
		exit(EXIT_FAILURE);
	}
	if (verbose)
		printf("%lu\n", (u_long)mp->mutex);

	if (verbose)
		printf(
		    "Allocate %d per-thread, self-blocking mutexes: ",
		    nthreads * nprocs);
	for (i = 0; i < nthreads * nprocs; ++i) {
		mp = (TM *)(tm_addr + i * sizeof(TM));
		if ((err = dbenv->mutex_alloc(
		    dbenv, DB_MUTEX_SELF_BLOCK, &mp->mutex)) != 0) {
			fprintf(stderr,
			    "DB_ENV->mutex_alloc (per-thread %d): %s\n",
			    i, db_strerror(err));
			exit(EXIT_FAILURE);
		}
		if ((err = dbenv->mutex_lock(dbenv, mp->mutex)) != 0) {
			fprintf(stderr,
			    "DB_ENV->mutex_lock (per-thread %d): %s\n",
			    i, db_strerror(err));
			exit(EXIT_FAILURE);
		}
		if (verbose)
			printf("%lu ", (u_long)mp->mutex);
	}
	if (verbose)
		printf("\n");

	if (verbose)
		printf("Allocate %d per-lock mutexes: ", maxlocks);
	for (i = 0; i < maxlocks; ++i) {
		mp = (TM *)(lm_addr + i * sizeof(TM));
		if ((err = dbenv->mutex_alloc(dbenv, 0, &mp->mutex)) != 0) {
			fprintf(stderr,
			    "DB_ENV->mutex_alloc (per-lock: %d): %s\n",
			    i, db_strerror(err));
			exit(EXIT_FAILURE);
		}
		if (verbose)
			printf("%lu ", (u_long)mp->mutex);
	}
	if (verbose)
		printf("\n");
}

/*
 * tm_mutex_destroy --
 *	Destroy the mutexes.
 */
void
tm_mutex_destroy()
{
	TM *gp, *mp;
	int err, i;

	if (verbose)
		printf("Destroy the global mutex.\n");
	gp = (TM *)gm_addr;
	if ((err = dbenv->mutex_free(dbenv, gp->mutex)) != 0) {
		fprintf(stderr,
		    "DB_ENV->mutex_free (global): %s\n", db_strerror(err));
		exit(EXIT_FAILURE);
	}

	if (verbose)
		printf("Destroy the per-thread mutexes.\n");
	for (i = 0; i < nthreads * nprocs; ++i) {
		mp = (TM *)(tm_addr + i * sizeof(TM));
		if ((err = dbenv->mutex_free(dbenv, mp->mutex)) != 0) {
			fprintf(stderr,
			    "DB_ENV->mutex_free (per-thread %d): %s\n",
			    i, db_strerror(err));
			exit(EXIT_FAILURE);
		}
	}

	if (verbose)
		printf("Destroy the per-lock mutexes.\n");
	for (i = 0; i < maxlocks; ++i) {
		mp = (TM *)(lm_addr + i * sizeof(TM));
		if ((err = dbenv->mutex_free(dbenv, mp->mutex)) != 0) {
			fprintf(stderr,
			    "DB_ENV->mutex_free (per-lock: %d): %s\n",
			    i, db_strerror(err));
			exit(EXIT_FAILURE);
		}
	}

	(void)unlink(MT_FILE);
}

/*
 * tm_mutex_stats --
 *	Display mutex statistics.
 */
void
tm_mutex_stats()
{
#ifdef HAVE_STATISTICS
	TM *mp;
	int i;
	u_int32_t set_wait, set_nowait;

	printf("Per-lock mutex statistics.\n");
	for (i = 0; i < maxlocks; ++i) {
		mp = (TM *)(lm_addr + i * sizeof(TM));
		__mutex_set_wait_info(dbenv, mp->mutex, &set_wait, &set_nowait);
		printf("mutex %2d: wait: %lu; no wait %lu\n", i,
		    (u_long)set_wait, (u_long)set_nowait);
	}
#endif
}

/*
 * map_file --
 *	Map in the backing file.
 */
void
map_file(gm_addrp, tm_addrp, lm_addrp, fhpp)
	u_int8_t **gm_addrp, **tm_addrp, **lm_addrp;
	DB_FH **fhpp;
{
	void *addr;
	DB_FH *fhp;
	int err;

#ifndef MAP_FAILED
#define	MAP_FAILED	(void *)-1
#endif
#ifndef MAP_FILE
#define	MAP_FILE	0
#endif
	if ((err = __os_open(dbenv, MT_FILE, 0, 0, &fhp)) != 0) {
		fprintf(stderr, "%s: open %s\n", MT_FILE, db_strerror(err));
		exit(EXIT_FAILURE);
	}

	if ((err = __os_mapfile(dbenv, MT_FILE, fhp, len, 0, &addr)) != 0) {
		fprintf(stderr, "%s: mmap: %s\n", MT_FILE, db_strerror(err));
		exit(EXIT_FAILURE);
	}

	*gm_addrp = (u_int8_t *)addr;
	addr = (u_int8_t *)addr + sizeof(TM);
	*tm_addrp = (u_int8_t *)addr;
	addr = (u_int8_t *)addr + sizeof(TM) * (nthreads * nprocs);
	*lm_addrp = (u_int8_t *)addr;

	if (fhpp != NULL)
		*fhpp = fhp;
}

/*
 * unmap_file --
 *	Discard backing file map.
 */
void
unmap_file(addr, fhp)
	u_int8_t *addr;
	DB_FH *fhp;
{
	int err;

	if ((err = __os_unmapfile(dbenv, addr, len)) != 0) {
		fprintf(stderr, "munmap: %s\n", db_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = __os_closehandle(dbenv, fhp)) != 0) {
		fprintf(stderr, "close: %s\n", db_strerror(err));
		exit(EXIT_FAILURE);
	}
}

/*
 * usage --
 *
 */
int
usage()
{
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: tm [-v] [-l maxlocks]",
	    "[-n locks] [-p procs] [-T locker=ID|wakeup=ID] [-t threads]");
	return (EXIT_FAILURE);
}

/*
 * os_wait --
 *	Wait for an array of N procs.
 */
int
os_wait(procs, nprocs)
	os_pid_t *procs;
	int nprocs;
{
	int i, status;
#if defined(DB_WIN32)
	DWORD ret;
#endif

	status = 0;

#if defined(DB_WIN32)
	do {
		ret = WaitForMultipleObjects(nprocs, procs, FALSE, INFINITE);
		i = ret - WAIT_OBJECT_0;
		if (i < 0 || i >= nprocs)
			return (__os_get_errno());

		if ((GetExitCodeProcess(procs[i], &ret) == 0) || (ret != 0))
			return (ret);

		/* remove the process handle from the list */
		while (++i < nprocs)
			procs[i - 1] = procs[i];
	} while (--nprocs);
#elif !defined(HAVE_VXWORKS)
	do {
		if ((i = wait(&status)) == -1)
			return (__os_get_errno());

		if (WIFEXITED(status) == 0 || WEXITSTATUS(status) != 0) {
			for (i = 0; i < nprocs; i++)
				kill(procs[i], SIGKILL);
			return (WEXITSTATUS(status));
		}
	} while (--nprocs);
#endif

	return (0);
}

os_pid_t
spawn_proc(id, tmpath, typearg)
	u_long id;
	char *tmpath, *typearg;
{
	char lbuf[16], nbuf[16], pbuf[16], tbuf[16], Tbuf[256];
	char *const vbuf = verbose ?  "-v" : NULL;
	char *args[] = { NULL /* tmpath */,
	    "-l", NULL /* lbuf */, "-n", NULL /* nbuf */,
	    "-p", NULL /* pbuf */, "-t", NULL /* tbuf */,
	    "-T", NULL /* Tbuf */, NULL /* vbuf */,
	    NULL
	};

	args[0] = tmpath;
	snprintf(lbuf, sizeof(lbuf),  "%d", maxlocks);
	args[2] = lbuf;
	snprintf(nbuf, sizeof(nbuf),  "%d", nlocks);
	args[4] = nbuf;
	snprintf(pbuf, sizeof(pbuf),  "%d", nprocs);
	args[6] = pbuf;
	snprintf(tbuf, sizeof(tbuf),  "%d", nthreads);
	args[8] = tbuf;
	snprintf(Tbuf, sizeof(Tbuf),  "%s=%lu", typearg, id);
	args[10] = Tbuf;
	args[11] = vbuf;

	return (os_spawn(tmpath, args));
}

os_pid_t
os_spawn(path, argv)
	const char *path;
	char *const argv[];
{
	os_pid_t pid;
	int status;

	COMPQUIET(pid, 0);
	COMPQUIET(status, 0);

#ifdef HAVE_VXWORKS
	fprintf(stderr, "ERROR: os_spawn not supported for VxWorks.\n");
	return (OS_BAD_PID);
#elif defined(HAVE_QNX)
	/*
	 * For QNX, we cannot fork if we've ever used threads.  So
	 * we'll use their spawn function.  We use 'spawnl' which
	 * is NOT a POSIX function.
	 *
	 * The return value of spawnl is just what we want depending
	 * on the value of the 'wait' arg.
	 */
	return (spawnv(P_NOWAIT, path, argv));
#elif defined(DB_WIN32)
	return (os_pid_t)(_spawnv(P_NOWAIT, path, argv));
#else
	if ((pid = fork()) != 0) {
		if (pid == -1)
			return (OS_BAD_PID);
		return (pid);
	} else {
		execv(path, argv);
		exit(EXIT_FAILURE);
	}
#endif
}
