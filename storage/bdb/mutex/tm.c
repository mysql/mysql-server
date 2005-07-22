/*
 * Standalone mutex tester for Berkeley DB mutexes.
 */
#include "db_config.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(MUTEX_THREAD_TEST)
#include <pthread.h>
#endif

#include "db_int.h"

#ifndef	HAVE_QNX
#define	shm_open	open
#define	shm_unlink	remove
#endif

void  exec_proc(u_long, char *, char *);
void  map_file(u_int8_t **, u_int8_t **, u_int8_t **, int *);
void  tm_file_init(void);
void  run_locker(u_long);
void *run_lthread(void *);
void  run_wakeup(u_long);
void *run_wthread(void *);
void  tm_mutex_destroy(void);
void  tm_mutex_init(void);
void  tm_mutex_stats(void);
void  unmap_file(u_int8_t *, int);
int   usage(void);

#define	MT_FILE		"mutex.file"
#define	MT_FILE_QUIT	"mutex.file.quit"

DB_ENV	 dbenv;					/* Fake out DB. */
size_t	 len;					/* Backing file size. */
int	 align;					/* Mutex alignment in file. */

int	 maxlocks = 20;				/* -l: Backing locks. */
int	 nlocks = 10000;			/* -n: Locks per processes. */
int	 nprocs = 20;				/* -p: Processes. */
int	 nthreads = 1;				/* -t: Threads. */
int	 verbose;				/* -v: Verbosity. */

typedef struct {
	DB_MUTEX mutex;				/* Mutex. */
	u_long	 id;				/* Holder's ID. */
#define	MUTEX_WAKEME	0x01			/* Request to awake. */
	u_int	 flags;
} TM;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	enum {LOCKER, WAKEUP, PARENT} rtype;
	extern int optind;
	extern char *optarg;
	pid_t pid;
	u_long id;
	int ch, fd, eval, i, status;
	char *p, *tmpath;

	__os_spin(&dbenv);		/* Fake out DB. */

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
	 * The file layout:
	 *	TM[1]			per-thread mutex array lock
	 *	TM[nthreads]		per-thread mutex array
	 *	TM[maxlocks]		per-lock mutex array
	 */
	align = DB_ALIGN(sizeof(TM), MUTEX_ALIGN);
	len = align * (1 + nthreads * nprocs + maxlocks);

	switch (rtype) {
	case PARENT:
		break;
	case LOCKER:
		run_locker(id);
		return (EXIT_SUCCESS);
	case WAKEUP:
		run_wakeup(id);
		return (EXIT_SUCCESS);
	}

	printf(
    "tm: %d processes, %d threads/process, %d lock requests from %d locks\n",
	    nprocs, nthreads, nlocks, maxlocks);
	printf(
    "tm: mutex alignment %lu, structure alignment %d, backing file %lu bytes\n",
	    (u_long)MUTEX_ALIGN, align, (u_long)len);

	tm_file_init();			/* Initialize backing file. */
	tm_mutex_init();		/* Initialize file's mutexes. */

	for (i = 0; i < nprocs; ++i) {
		switch (fork()) {
		case -1:
			perror("fork");
			return (EXIT_FAILURE);
		case 0:
			exec_proc(id, tmpath, "locker");
			break;
		default:
			break;
		}
		id += nthreads;
	}

	(void)remove(MT_FILE_QUIT);

	switch (fork()) {
	case -1:
		perror("fork");
		return (EXIT_FAILURE);
	case 0:
		exec_proc(id, tmpath, "wakeup");
		break;
	default:
		break;
	}
	++id;

	/* Wait for locking threads. */
	for (i = 0, eval = EXIT_SUCCESS; i < nprocs; ++i)
		if ((pid = wait(&status)) != (pid_t)-1) {
			fprintf(stderr,
		    "%lu: exited %d\n", (u_long)pid, WEXITSTATUS(status));
			if (WEXITSTATUS(status) != 0)
				eval = EXIT_FAILURE;
		}

	/* Signal wakeup thread to exit. */
	if ((fd = open(MT_FILE_QUIT, O_WRONLY | O_CREAT, 0664)) == -1) {
		fprintf(stderr, "tm: %s\n", strerror(errno));
		status = EXIT_FAILURE;
	}
	(void)close(fd);

	/* Wait for wakeup thread. */
	if ((pid = wait(&status)) != (pid_t)-1) {
		fprintf(stderr,
	    "%lu: exited %d\n", (u_long)pid, WEXITSTATUS(status));
		if (WEXITSTATUS(status) != 0)
			eval = EXIT_FAILURE;
	}

	(void)remove(MT_FILE_QUIT);

	tm_mutex_stats();			/* Display run statistics. */
	tm_mutex_destroy();			/* Destroy region. */

	printf("tm: exit status: %s\n",
	    eval == EXIT_SUCCESS ? "success" : "failed!");
	return (eval);
}

void
exec_proc(id, tmpath, typearg)
	u_long id;
	char *tmpath, *typearg;
{
	char *argv[10], **ap, b_l[10], b_n[10], b_p[10], b_t[10], b_T[10];

	ap = &argv[0];
	*ap++ = "tm";
	sprintf(b_l, "-l%d", maxlocks);
	*ap++ = b_l;
	sprintf(b_n, "-n%d", nlocks);
	*ap++ = b_p;
	sprintf(b_p, "-p%d", nprocs);
	*ap++ = b_n;
	sprintf(b_t, "-t%d", nthreads);
	*ap++ = b_t;
	sprintf(b_T, "-T%s=%lu", typearg, id);
	*ap++ = b_T;
	if (verbose)
		*ap++ = "-v";

	*ap = NULL;
	execvp(tmpath, argv);

	fprintf(stderr, "%s: %s\n", tmpath, strerror(errno));
	exit(EXIT_FAILURE);
}

void
run_locker(id)
	u_long id;
{
#if defined(MUTEX_THREAD_TEST)
	pthread_t *kidsp;
	int i;
	void *retp;
#endif
	int status;

	__os_sleep(&dbenv, 3, 0);		/* Let everyone catch up. */

	srand((u_int)time(NULL) % getpid());	/* Initialize random numbers. */

#if defined(MUTEX_THREAD_TEST)
	/*
	 * Spawn off threads.  We have nthreads all locking and going to
	 * sleep, and one other thread cycling through and waking them up.
	 */
	if ((kidsp =
	    (pthread_t *)calloc(sizeof(pthread_t), nthreads)) == NULL) {
		fprintf(stderr, "tm: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < nthreads; i++)
		if ((errno = pthread_create(
		    &kidsp[i], NULL, run_lthread, (void *)(id + i))) != 0) {
			fprintf(stderr, "tm: failed spawning thread: %s\n",
			    strerror(errno));
			exit(EXIT_FAILURE);
		}

	/* Wait for the threads to exit. */
	status = EXIT_SUCCESS;
	for (i = 0; i < nthreads; i++) {
		pthread_join(kidsp[i], &retp);
		if (retp != NULL) {
			fprintf(stderr, "tm: thread exited with error\n");
			status = EXIT_FAILURE;
		}
	}
	free(kidsp);
#else
	status = (int)run_lthread((void *)id);
#endif
	exit(status);
}

void *
run_lthread(arg)
	void *arg;
{
	TM *gp, *mp, *tp;
	u_long id, tid;
	int fd, i, lock, nl, remap;
	u_int8_t *gm_addr, *lm_addr, *tm_addr;

	id = (int)arg;
#if defined(MUTEX_THREAD_TEST)
	tid = (u_long)pthread_self();
#else
	tid = 0;
#endif
	printf("Locker: ID %03lu (PID: %lu; TID: %lx)\n",
	    id, (u_long)getpid(), tid);

	nl = nlocks;
	for (gm_addr = NULL, gp = tp = NULL, remap = 0;;) {
		/* Map in the file as necessary. */
		if (gm_addr == NULL) {
			map_file(&gm_addr, &tm_addr, &lm_addr, &fd);
			gp = (TM *)gm_addr;
			tp = (TM *)(tm_addr + id * align);
			if (verbose)
				printf(
				    "%03lu: map threads @ %#lx; locks @ %#lx\n",
				    id, (u_long)tm_addr, (u_long)lm_addr);
			remap = (rand() % 100) + 35;
		}

		/* Select and acquire a data lock. */
		lock = rand() % maxlocks;
		mp = (TM *)(lm_addr + lock * align);
		if (verbose)
			printf("%03lu: lock %d @ %#lx\n",
			    id, lock, (u_long)&mp->mutex);

		if (__db_mutex_lock(&dbenv, &mp->mutex)) {
			fprintf(stderr, "%03lu: never got lock %d: %s\n",
			    id, lock, strerror(errno));
			return ((void *)EXIT_FAILURE);
		}
		if (mp->id != 0) {
			fprintf(stderr,
			    "RACE! (%03lu granted lock %d held by %03lu)\n",
			    id, lock, mp->id);
			return ((void *)EXIT_FAILURE);
		}
		mp->id = id;

		/*
		 * Pretend to do some work, periodically checking to see if
		 * we still hold the mutex.
		 */
		for (i = 0; i < 3; ++i) {
			__os_sleep(&dbenv, 0, rand() % 3);
			if (mp->id != id) {
				fprintf(stderr,
				    "RACE! (%03lu stole lock %d from %03lu)\n",
				    mp->id, lock, id);
				return ((void *)EXIT_FAILURE);
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
		if (__db_mutex_lock(&dbenv, &gp->mutex)) {
			fprintf(stderr,
			    "%03lu: global lock: %s\n", id, strerror(errno));
			return ((void *)EXIT_FAILURE);
		}
		if (tp->id != 0 && tp->id != id) {
			fprintf(stderr,
		    "%03lu: per-thread mutex isn't mine, owned by %03lu\n",
			    id, tp->id);
			return ((void *)EXIT_FAILURE);
		}
		tp->id = id;
		if (verbose)
			printf("%03lu: self-blocking\n", id);
		if (F_ISSET(tp, MUTEX_WAKEME)) {
			fprintf(stderr,
			    "%03lu: wakeup flag incorrectly set\n", id);
			return ((void *)EXIT_FAILURE);
		}
		F_SET(tp, MUTEX_WAKEME);
		if (__db_mutex_unlock(&dbenv, &gp->mutex)) {
			fprintf(stderr,
			    "%03lu: global unlock: %s\n", id, strerror(errno));
			return ((void *)EXIT_FAILURE);
		}
		if (__db_mutex_lock(&dbenv, &tp->mutex)) {
			fprintf(stderr, "%03lu: per-thread lock: %s\n",
			    id, strerror(errno));
			return ((void *)EXIT_FAILURE);
		}
		/* Time passes... */
		if (F_ISSET(tp, MUTEX_WAKEME)) {
			fprintf(stderr, "%03lu: wakeup flag not cleared\n", id);
			return ((void *)EXIT_FAILURE);
		}

		if (verbose)
			printf("%03lu: release %d @ %#lx\n",
			    id, lock, (u_long)&mp->mutex);

		/* Release the data lock. */
		mp->id = 0;
		if (__db_mutex_unlock(&dbenv, &mp->mutex)) {
			fprintf(stderr,
			    "%03lu: lock release: %s\n", id, strerror(errno));
			return ((void *)EXIT_FAILURE);
		}

		if (--nl % 100 == 0)
			fprintf(stderr, "%03lu: %d\n", id, nl);

		if (nl == 0 || --remap == 0) {
			if (verbose)
				printf("%03lu: re-mapping\n", id);
			unmap_file(gm_addr, fd);
			gm_addr = NULL;

			if (nl == 0)
				break;

			__os_sleep(&dbenv, 0, rand() % 500);
		}
	}

	return (NULL);
}

void
run_wakeup(id)
	u_long id;
{
#if defined(MUTEX_THREAD_TEST)
	pthread_t wakep;
	int status;
	void *retp;
#endif
	__os_sleep(&dbenv, 3, 0);		/* Let everyone catch up. */

	srand((u_int)time(NULL) % getpid());	/* Initialize random numbers. */

#if defined(MUTEX_THREAD_TEST)
	/*
	 * Spawn off wakeup thread.
	 */
	if ((errno = pthread_create(
	    &wakep, NULL, run_wthread, (void *)id)) != 0) {
		fprintf(stderr, "tm: failed spawning wakeup thread: %s\n",
		    strerror(errno));
		exit(EXIT_FAILURE);
	}

	/*
	 * run_locker will create a file when the wakeup thread is no
	 * longer needed.
	 */
	status = 0;
	pthread_join(wakep, &retp);
	if (retp != NULL) {
		fprintf(stderr, "tm: wakeup thread exited with error\n");
		status = EXIT_FAILURE;
	}

	exit(status);
#else
	exit((int)run_wthread((void *)id));
#endif
}

/*
 * run_wthread --
 *	Thread to wake up other threads that are sleeping.
 */
void *
run_wthread(arg)
	void *arg;
{
	struct stat sb;
	TM *gp, *tp;
	u_long id, tid;
	int fd, check_id;
	u_int8_t *gm_addr, *tm_addr;

	id = (int)arg;
#if defined(MUTEX_THREAD_TEST)
	tid = (u_long)pthread_self();
#else
	tid = 0;
#endif
	printf("Wakeup: ID %03lu (PID: %lu; TID: %lx)\n",
	    id, (u_long)getpid(), tid);

	arg = NULL;
	map_file(&gm_addr, &tm_addr, NULL, &fd);
	if (verbose)
		printf("%03lu: map threads @ %#lx\n", id, (u_long)tm_addr);
	gp = (TM *)gm_addr;

	/* Loop, waking up sleepers and periodically sleeping ourselves. */
	for (check_id = 0;; ++check_id) {
		/* Check to see if the locking threads have finished. */
		if (stat(MT_FILE_QUIT, &sb) == 0)
			break;

		/* Check for ID wraparound. */
		if (check_id == nthreads * nprocs)
			check_id = 0;

		/* Check for a thread that needs a wakeup. */
		tp = (TM *)(tm_addr + check_id * align);
		if (!F_ISSET(tp, MUTEX_WAKEME))
			continue;

		if (verbose)
			printf("%03lu: wakeup thread %03lu @ %#lx\n",
			    id, tp->id, (u_long)&tp->mutex);

		/* Acquire the global lock. */
		if (__db_mutex_lock(&dbenv, &gp->mutex)) {
			fprintf(stderr,
			    "wakeup: global lock: %s\n", strerror(errno));
			return ((void *)EXIT_FAILURE);
		}

		F_CLR(tp, MUTEX_WAKEME);
		if (__db_mutex_unlock(&dbenv, &tp->mutex)) {
			fprintf(stderr,
			    "wakeup: unlock: %s\n", strerror(errno));
			return ((void *)EXIT_FAILURE);
		}

		if (__db_mutex_unlock(&dbenv, &gp->mutex)) {
			fprintf(stderr,
			    "wakeup: global unlock: %s\n", strerror(errno));
			return ((void *)EXIT_FAILURE);
		}

		__os_sleep(&dbenv, 0, rand() % 3);
	}
	return (NULL);
}

/*
 * tm_file_init --
 *	Initialize the backing file.
 */
void
tm_file_init()
{
	int fd;

	/* Initialize the backing file. */
	if (verbose)
		printf("Create the backing file.\n");

	(void)shm_unlink(MT_FILE);

	if ((fd = shm_open(
	    MT_FILE, O_CREAT | O_RDWR | O_TRUNC,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1) {
		(void)fprintf(stderr,
		    "%s: open: %s\n", MT_FILE, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (lseek(fd,
	    (off_t)len, SEEK_SET) != (off_t)len || write(fd, &fd, 1) != 1) {
		(void)fprintf(stderr,
		    "%s: seek/write: %s\n", MT_FILE, strerror(errno));
		exit(EXIT_FAILURE);
	}
	(void)close(fd);
}

/*
 * tm_mutex_init --
 *	Initialize the mutexes.
 */
void
tm_mutex_init()
{
	TM *mp;
	int fd, i;
	u_int8_t *gm_addr, *lm_addr, *tm_addr;

	map_file(&gm_addr, &tm_addr, &lm_addr, &fd);
	if (verbose)
		printf("init: map threads @ %#lx; locks @ %#lx\n",
		    (u_long)tm_addr, (u_long)lm_addr);

	if (verbose)
		printf("Initialize the global mutex:\n");
	mp = (TM *)gm_addr;
	if (__db_mutex_init_int(&dbenv, &mp->mutex, 0, 0)) {
		fprintf(stderr,
		    "__db_mutex_init (global): %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (verbose)
		printf("\t@ %#lx\n", (u_long)&mp->mutex);

	if (verbose)
		printf(
		    "Initialize %d per-thread mutexes:\n", nthreads * nprocs);
	for (i = 0; i < nthreads * nprocs; ++i) {
		mp = (TM *)(tm_addr + i * align);
		if (__db_mutex_init_int(
		    &dbenv, &mp->mutex, 0, MUTEX_SELF_BLOCK)) {
			fprintf(stderr, "__db_mutex_init (per-thread %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (__db_mutex_lock(&dbenv, &mp->mutex)) {
			fprintf(stderr, "__db_mutex_lock (per-thread %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (verbose)
			printf("\t@ %#lx\n", (u_long)&mp->mutex);
	}

	if (verbose)
		printf("Initialize %d per-lock mutexes:\n", maxlocks);
	for (i = 0; i < maxlocks; ++i) {
		mp = (TM *)(lm_addr + i * align);
		if (__db_mutex_init_int(&dbenv, &mp->mutex, 0, 0)) {
			fprintf(stderr, "__db_mutex_init (per-lock: %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (verbose)
			printf("\t@ %#lx\n", (u_long)&mp->mutex);
	}

	unmap_file(gm_addr, fd);
}

/*
 * tm_mutex_destroy --
 *	Destroy the mutexes.
 */
void
tm_mutex_destroy()
{
	TM *gp, *mp;
	int fd, i;
	u_int8_t *gm_addr, *lm_addr, *tm_addr;

	map_file(&gm_addr, &tm_addr, &lm_addr, &fd);

	if (verbose)
		printf("Destroy the global mutex.\n");
	gp = (TM *)gm_addr;
	if (__db_mutex_destroy(&gp->mutex)) {
		fprintf(stderr,
		    "__db_mutex_destroy (global): %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (verbose)
		printf("Destroy the per-thread mutexes.\n");
	for (i = 0; i < nthreads * nprocs; ++i) {
		mp = (TM *)(tm_addr + i * align);
		if (__db_mutex_destroy(&mp->mutex)) {
			fprintf(stderr,
			    "__db_mutex_destroy (per-thread %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	if (verbose)
		printf("Destroy the per-lock mutexes.\n");
	for (i = 0; i < maxlocks; ++i) {
		mp = (TM *)(tm_addr + i * align);
		if (__db_mutex_destroy(&mp->mutex)) {
			fprintf(stderr,
			    "__db_mutex_destroy (per-lock: %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	unmap_file(gm_addr, fd);

	(void)shm_unlink(MT_FILE);
}

/*
 * tm_mutex_stats --
 *	Display mutex statistics.
 */
void
tm_mutex_stats()
{
	TM *mp;
	int fd, i;
	u_int8_t *gm_addr, *lm_addr;

	map_file(&gm_addr, NULL, &lm_addr, &fd);

	printf("Per-lock mutex statistics.\n");
	for (i = 0; i < maxlocks; ++i) {
		mp = (TM *)(lm_addr + i * align);
		printf("mutex %2d: wait: %lu; no wait %lu\n", i,
		    (u_long)mp->mutex.mutex_set_wait,
		    (u_long)mp->mutex.mutex_set_nowait);
	}

	unmap_file(gm_addr, fd);
}

/*
 * map_file --
 *	Map in the backing file.
 */
void
map_file(gm_addrp, tm_addrp, lm_addrp, fdp)
	u_int8_t **gm_addrp, **tm_addrp, **lm_addrp;
	int *fdp;
{
	void *addr;
	int fd;

#ifndef MAP_FAILED
#define	MAP_FAILED	(void *)-1
#endif
#ifndef MAP_FILE
#define	MAP_FILE	0
#endif
	if ((fd = shm_open(MT_FILE, O_RDWR, 0)) == -1) {
		fprintf(stderr, "%s: open %s\n", MT_FILE, strerror(errno));
		exit(EXIT_FAILURE);
	}

	addr = mmap(NULL, len,
	    PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, (off_t)0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "%s: mmap: %s\n", MT_FILE, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (gm_addrp != NULL)
		*gm_addrp = (u_int8_t *)addr;
	addr = (u_int8_t *)addr + align;
	if (tm_addrp != NULL)
		*tm_addrp = (u_int8_t *)addr;
	addr = (u_int8_t *)addr + align * (nthreads * nprocs);
	if (lm_addrp != NULL)
		*lm_addrp = (u_int8_t *)addr;

	if (fdp != NULL)
		*fdp = fd;
}

/*
 * unmap_file --
 *	Discard backing file map.
 */
void
unmap_file(addr, fd)
	u_int8_t *addr;
	int fd;
{
	if (munmap(addr, len) != 0) {
		fprintf(stderr, "munmap: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (close(fd) != 0) {
		fprintf(stderr, "close: %s\n", strerror(errno));
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
