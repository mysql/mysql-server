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

#if defined(HAVE_MUTEX_PTHREADS) || defined(BUILD_PTHREADS_ANYWAY)
#include <pthread.h>
#endif

#include "db_int.h"

void  exec_proc();
void  tm_file_init();
void  map_file();
void  run_proc();
void *run_thread();
void *run_thread_wake();
void  tm_mutex_destroy();
void  tm_mutex_init();
void  tm_mutex_stats();
void  unmap_file();

#define	MUTEX_WAKEME	0x80			/* Wake-me flag. */

DB_ENV	 dbenv;					/* Fake out DB. */
size_t	 len;					/* Backing file size. */
int	 align;					/* Mutex alignment in file. */
int	 quit;					/* End-of-test flag. */
char	*file = "mutex.file";			/* Backing file. */

int	 maxlocks = 20;				/* -l: Backing locks. */
int	 nlocks = 10000;			/* -n: Locks per processes. */
int	 nprocs = 20;				/* -p: Processes. */
int	 child;					/* -s: Slave. */
int	 nthreads = 1;				/* -t: Threads. */
int	 verbose;				/* -v: Verbosity. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	pid_t pid;
	int ch, eval, i, status;
	char *tmpath;

	tmpath = argv[0];
	while ((ch = getopt(argc, argv, "l:n:p:st:v")) != EOF)
		switch(ch) {
		case 'l':
			maxlocks = atoi(optarg);
			break;
		case 'n':
			nlocks = atoi(optarg);
			break;
		case 'p':
			nprocs = atoi(optarg);
			break;
		case 's':
			child = 1;
			break;
		case 't':
			nthreads = atoi(optarg);
#if !defined(HAVE_MUTEX_PTHREADS) && !defined(BUILD_PTHREADS_ANYWAY)
			if (nthreads != 1) {
				(void)fprintf(stderr,
	"tm: pthreads not available or not compiled for this platform.\n");
				return (EXIT_FAILURE);
			}
#endif
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			(void)fprintf(stderr,
    "usage: tm [-v] [-l maxlocks] [-n locks] [-p procs] [-t threads]\n");
			return (EXIT_FAILURE);
		}
	argc -= optind;
	argv += optind;

	/*
	 * The file layout:
	 *	DB_MUTEX[1]		per-thread mutex array lock
	 *	DB_MUTEX[nthreads]	per-thread mutex array
	 *	DB_MUTEX[maxlocks]	per-lock mutex array
	 *	u_long[maxlocks][2]	per-lock ID array
	 */
	align = ALIGN(sizeof(DB_MUTEX) * 2, MUTEX_ALIGN);
	len =
	    align * (1 + nthreads +  maxlocks) + sizeof(u_long) * maxlocks * 2;
	printf(
    "mutex alignment %d, structure alignment %d, backing file %lu bytes\n",
	    MUTEX_ALIGN, align, (u_long)len);

	if (child) {
		run_proc();
		return (EXIT_SUCCESS);
	}

	tm_file_init();
	tm_mutex_init();

	printf(
	    "%d proc, %d threads/proc, %d lock requests from %d locks:\n",
	    nprocs, nthreads, nlocks, maxlocks);
	for (i = 0; i < nprocs; ++i)
		switch (fork()) {
		case -1:
			perror("fork");
			return (EXIT_FAILURE);
		case 0:
			exec_proc(tmpath);
			break;
		default:
			break;
		}

	eval = EXIT_SUCCESS;
	while ((pid = wait(&status)) != (pid_t)-1) {
		fprintf(stderr,
		    "%lu: exited %d\n", (u_long)pid, WEXITSTATUS(status));
		if (WEXITSTATUS(status) != 0)
			eval = EXIT_FAILURE;
	}

	tm_mutex_stats();
	tm_mutex_destroy();

	printf("tm: exit status: %s\n",
	    eval == EXIT_SUCCESS ? "success" : "failed!");
	return (eval);
}

void
exec_proc(tmpath)
	char *tmpath;
{
	char *argv[10], **ap, b_l[10], b_n[10], b_t[10];

	ap = &argv[0];
	*ap++ = "tm";
	sprintf(b_l, "-l%d", maxlocks);
	*ap++ = b_l;
	sprintf(b_n, "-n%d", nlocks);
	*ap++ = b_n;
	*ap++ = "-s";
	sprintf(b_t, "-t%d", nthreads);
	*ap++ = b_t;
	if (verbose)
		*ap++ = "-v";

	*ap = NULL;
	execvp(tmpath, argv);

	fprintf(stderr, "%s: %s\n", tmpath, strerror(errno));
	exit(EXIT_FAILURE);
}

void
run_proc()
{
#if defined(HAVE_MUTEX_PTHREADS) || defined(BUILD_PTHREADS_ANYWAY)
	pthread_t *kidsp, wakep;
	int i, status;
	void *retp;
#endif
	__os_sleep(&dbenv, 3, 0);		/* Let everyone catch up. */

	srand((u_int)time(NULL) / getpid());	/* Initialize random numbers. */

	if (nthreads == 1)			/* Simple case. */
		exit((int)run_thread((void *)0));

#if defined(HAVE_MUTEX_PTHREADS) || defined(BUILD_PTHREADS_ANYWAY)
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
		    &kidsp[i], NULL, run_thread, (void *)i)) != 0) {
			fprintf(stderr, "tm: failed spawning thread %d: %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}

	if ((errno = pthread_create(
	    &wakep, NULL, run_thread_wake, (void *)0)) != 0) {
		fprintf(stderr, "tm: failed spawning wakeup thread: %s\n",
		    strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Wait for the threads to exit. */
	status = 0;
	for (i = 0; i < nthreads; i++) {
		pthread_join(kidsp[i], &retp);
		if (retp != NULL) {
			fprintf(stderr,
			    "tm: thread %d exited with error\n", i);
			status = EXIT_FAILURE;
		}
	}
	free(kidsp);

	/* Signal wakeup thread to stop. */
	quit = 1;
	pthread_join(wakep, &retp);
	if (retp != NULL) {
		fprintf(stderr, "tm: wakeup thread exited with error\n");
		status = EXIT_FAILURE;
	}

	exit(status);
#endif
}

void *
run_thread(arg)
	void *arg;
{
	DB_MUTEX *gm_addr, *lm_addr, *tm_addr, *mp;
	u_long gid1, gid2, *id_addr;
	int fd, i, lock, id, nl, remap;

	/* Set local and global per-thread ID. */
	id = (int)arg;
	gid1 = (u_long)getpid();
#if defined(HAVE_MUTEX_PTHREADS) || defined(BUILD_PTHREADS_ANYWAY)
	gid2 = (u_long)pthread_self();
#else
	gid2 = 0;
#endif
	printf("\tPID: %lu; TID: %lx; ID: %d\n", gid1, gid2, id);

	nl = nlocks;
	for (gm_addr = NULL, remap = 0;;) {
		/* Map in the file as necessary. */
		if (gm_addr == NULL) {
			map_file(&gm_addr, &tm_addr, &lm_addr, &id_addr, &fd);
			remap = (rand() % 100) + 35;
		}

		/* Select and acquire a data lock. */
		lock = rand() % maxlocks;
		mp = (DB_MUTEX *)((u_int8_t *)lm_addr + lock * align);
		if (verbose)
			printf("%lu/%lx: %03d\n", gid1, gid2, lock);

		if (__db_mutex_lock(&dbenv, mp)) {
			fprintf(stderr,
			    "%lu/%lx: never got lock\n", gid1, gid2);
			return ((void *)EXIT_FAILURE);
		}
		if (id_addr[lock * 2] != 0) {
			fprintf(stderr,
			    "RACE! (%lu/%lx granted lock %d held by %lu/%lx)\n",
			    gid1, gid2,
			    lock, id_addr[lock * 2], id_addr[lock * 2 + 1]);
			return ((void *)EXIT_FAILURE);
		}
		id_addr[lock * 2] = gid1;
		id_addr[lock * 2 + 1] = gid2;

		/*
		 * Pretend to do some work, periodically checking to see if
		 * we still hold the mutex.
		 */
		for (i = 0; i < 3; ++i) {
			__os_sleep(&dbenv, 0, rand() % 3);
			if (id_addr[lock * 2] != gid1 ||
			    id_addr[lock * 2 + 1] != gid2) {
				fprintf(stderr,
			    "RACE! (%lu/%lx stole lock %d from %lu/%lx)\n",
				    id_addr[lock * 2],
				    id_addr[lock * 2 + 1], lock, gid1, gid2);
				return ((void *)EXIT_FAILURE);
			}
		}

#if defined(HAVE_MUTEX_PTHREADS) || defined(BUILD_PTHREADS_ANYWAY)
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
		if (__db_mutex_lock(&dbenv, gm_addr)) {
			fprintf(stderr, "%lu/%lx: global lock\n", gid1, gid2);
			return ((void *)EXIT_FAILURE);
		}
		mp = (DB_MUTEX *)((u_int8_t *)tm_addr + id * align);
		F_SET(mp, MUTEX_WAKEME);
		if (__db_mutex_unlock(&dbenv, gm_addr)) {
			fprintf(stderr,
			    "%lu/%lx: per-thread wakeup failed\n", gid1, gid2);
			return ((void *)EXIT_FAILURE);
		}
		if (__db_mutex_lock(&dbenv, mp)) {
			fprintf(stderr,
			    "%lu/%lx: per-thread lock\n", gid1, gid2);
			return ((void *)EXIT_FAILURE);
		}
		/* Time passes... */
		if (F_ISSET(mp, MUTEX_WAKEME)) {
			fprintf(stderr, "%lu/%lx: %03d wakeup flag still set\n",
			    gid1, gid2, id);
			return ((void *)EXIT_FAILURE);
		}
#endif

		/* Release the data lock. */
		id_addr[lock * 2] = id_addr[lock * 2 + 1] = 0;
		mp = (DB_MUTEX *)((u_int8_t *)lm_addr + lock * align);
		if (__db_mutex_unlock(&dbenv, mp)) {
			fprintf(stderr, "%lu/%lx: wakeup failed\n", gid1, gid2);
			return ((void *)EXIT_FAILURE);
		}

		if (--nl % 100 == 0)
			fprintf(stderr, "%lu/%lx: %d\n", gid1, gid2, nl);

		if (nl == 0 || --remap == 0) {
			unmap_file((void *)gm_addr, fd);
			gm_addr = NULL;

			if (nl == 0)
				break;

			__os_sleep(&dbenv, rand() % 3, 0);
		}
	}

	return (NULL);
}

#if defined(HAVE_MUTEX_PTHREADS) || defined(BUILD_PTHREADS_ANYWAY)
/*
 * run_thread_wake --
 *	Thread to wake up other threads that are sleeping.
 */
void *
run_thread_wake(arg)
	void *arg;
{
	DB_MUTEX *gm_addr, *tm_addr, *mp;
	int fd, id;

	arg = NULL;
	map_file(&gm_addr, &tm_addr, NULL, NULL, &fd);

	/* Loop, waking up sleepers and periodically sleeping ourselves. */
	while (!quit) {
		id = 0;

		/* Acquire the global lock. */
retry:		if (__db_mutex_lock(&dbenv, gm_addr)) {
			fprintf(stderr, "wt: global lock failed\n");
			return ((void *)EXIT_FAILURE);
		}

next:		mp = (DB_MUTEX *)((u_int8_t *)tm_addr + id * align);
		if (F_ISSET(mp, MUTEX_WAKEME)) {
			F_CLR(mp, MUTEX_WAKEME);
			if (__db_mutex_unlock(&dbenv, mp)) {
				fprintf(stderr, "wt: wakeup failed\n");
				return ((void *)EXIT_FAILURE);
			}
		}

		if (++id < nthreads && id % 3 != 0)
			goto next;

		if (__db_mutex_unlock(&dbenv, gm_addr)) {
			fprintf(stderr, "wt: global unlock failed\n");
			return ((void *)EXIT_FAILURE);
		}

		__os_sleep(&dbenv, 0, 500);

		if (id < nthreads)
			goto retry;
	}
	return (NULL);
}
#endif

/*
 * tm_file_init --
 *	Initialize the backing file.
 */
void
tm_file_init()
{
	int fd;


	/* Initialize the backing file. */
	printf("Create the backing file...\n");
#ifdef	HAVE_QNX
	(void)shm_unlink(file);
	if ((fd = shm_open(file, O_CREAT | O_RDWR | O_TRUNC,
#else
	(void)remove(file);
	if ((fd = open(file, O_CREAT | O_RDWR | O_TRUNC,
#endif

	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1) {
		(void)fprintf(stderr, "%s: open: %s\n", file, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (lseek(fd, (off_t)len, SEEK_SET) != len || write(fd, &fd, 1) != 1) {
		(void)fprintf(stderr,
		    "%s: seek/write: %s\n", file, strerror(errno));
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
	DB_MUTEX *gm_addr, *lm_addr, *mp, *tm_addr;
	int fd, i;

	map_file(&gm_addr, &tm_addr, &lm_addr, NULL, &fd);

	printf("Initialize the global mutex...\n");
	if (__db_mutex_init_int(&dbenv, gm_addr, 0, 0)) {
		fprintf(stderr,
		    "__db_mutex_init (global): %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	printf("Initialize the per-thread mutexes...\n");
	for (i = 1, mp = tm_addr;
	    i <= nthreads; ++i, mp = (DB_MUTEX *)((u_int8_t *)mp + align)) {
		if (__db_mutex_init_int(&dbenv, mp, 0, MUTEX_SELF_BLOCK)) {
			fprintf(stderr, "__db_mutex_init (per-thread %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (__db_mutex_lock(&dbenv, mp)) {
			fprintf(stderr,
			    "__db_mutex_init (per-thread %d) lock: %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	printf("Initialize the per-lock mutexes...\n");
	for (i = 1, mp = lm_addr;
	    i <= maxlocks; ++i, mp = (DB_MUTEX *)((u_int8_t *)mp + align))
		if (__db_mutex_init_int(&dbenv, mp, 0, 0)) {
			fprintf(stderr, "__db_mutex_init (per-lock: %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}

	unmap_file((void *)gm_addr, fd);
}

/*
 * tm_mutex_destroy --
 *	Destroy the mutexes.
 */
void
tm_mutex_destroy()
{
	DB_MUTEX *gm_addr, *lm_addr, *mp, *tm_addr;
	int fd, i;

	map_file(&gm_addr, &tm_addr, &lm_addr, NULL, &fd);

	printf("Destroy the global mutex...\n");
	if (__db_mutex_destroy(gm_addr)) {
		fprintf(stderr,
		    "__db_mutex_destroy (global): %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	printf("Destroy the per-thread mutexes...\n");
	for (i = 1, mp = tm_addr;
	    i <= nthreads; ++i, mp = (DB_MUTEX *)((u_int8_t *)mp + align)) {
		if (__db_mutex_destroy(mp)) {
			fprintf(stderr,
			    "__db_mutex_destroy (per-thread %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	printf("Destroy the per-lock mutexes...\n");
	for (i = 1, mp = lm_addr;
	    i <= maxlocks; ++i, mp = (DB_MUTEX *)((u_int8_t *)mp + align))
		if (__db_mutex_destroy(mp)) {
			fprintf(stderr,
			    "__db_mutex_destroy (per-lock: %d): %s\n",
			    i, strerror(errno));
			exit(EXIT_FAILURE);
		}

	unmap_file((void *)gm_addr, fd);
#ifdef HAVE_QNX
	(void)shm_unlink(file);
#endif
}

/*
 * tm_mutex_stats --
 *	Display mutex statistics.
 */
void
tm_mutex_stats()
{
	DB_MUTEX *gm_addr, *lm_addr, *mp;
	int fd, i;

	map_file(&gm_addr, NULL, &lm_addr, NULL, &fd);

	printf("Per-lock mutex statistics...\n");
	for (i = 1, mp = lm_addr;
	    i <= maxlocks; ++i, mp = (DB_MUTEX *)((u_int8_t *)mp + align))
		printf("mutex %2d: wait: %lu; no wait %lu\n", i,
		    (u_long)mp->mutex_set_wait, (u_long)mp->mutex_set_nowait);

	unmap_file((void *)gm_addr, fd);
}

/*
 * map_file --
 *	Map in the backing file.
 */
void
map_file(gm_addrp, tm_addrp, lm_addrp, id_addrp, fdp)
	DB_MUTEX **gm_addrp, **tm_addrp, **lm_addrp;
	u_long **id_addrp;
	int *fdp;
{
	void *maddr;
	int fd;

#ifndef MAP_FAILED
#define	MAP_FAILED	(void *)-1
#endif
#ifndef MAP_FILE
#define	MAP_FILE	0
#endif
#ifdef	HAVE_QNX
	if ((fd = shm_open(file, O_RDWR, 0)) == -1) {
#else
	if ((fd = open(file, O_RDWR, 0)) == -1) {
#endif
		fprintf(stderr, "%s: open %s\n", file, strerror(errno));
		exit(EXIT_FAILURE);
	}

	maddr = mmap(NULL, len,
	    PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, (off_t)0);
	if (maddr == MAP_FAILED) {
		fprintf(stderr, "%s: mmap: %s\n", file, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (gm_addrp != NULL)
		*gm_addrp = (DB_MUTEX *)maddr;
	maddr = (u_int8_t *)maddr + align;
	if (tm_addrp != NULL)
		*tm_addrp = (DB_MUTEX *)maddr;
	maddr = (u_int8_t *)maddr + align * nthreads;
	if (lm_addrp != NULL)
		*lm_addrp = (DB_MUTEX *)maddr;
	maddr = (u_int8_t *)maddr + align * maxlocks;
	if (id_addrp != NULL)
		*id_addrp = (u_long *)maddr;
	if (fdp != NULL)
		*fdp = fd;
}

/*
 * unmap_file --
 *	Discard backing file map.
 */
void
unmap_file(maddr, fd)
	void *maddr;
	int fd;
{
	if (munmap(maddr, len) != 0) {
		fprintf(stderr, "munmap: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (close(fd) != 0) {
		fprintf(stderr, "close: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}
