/* $Header$ */

/*
 * LinuxThreads specific stuff.
 */

#include	<sys/types.h>

#include	<assert.h>
#include	<limits.h>	/* PTHREAD_THREADS_MAX */
#include	<pthread.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<signal.h>
#include	<sched.h>

#include	"linuxthreads.h"

#define AT_INT(intval)	*((int32_t*)(intval))

/*
 * Internal LinuxThreads variables.
 * Official interface exposed to GDB.
 */
#if 1
extern volatile	int		__pthread_threads_debug;
extern volatile	char		__pthread_handles;
extern char			__pthread_initial_thread;
/*extern volatile	Elf32_Sym*	__pthread_manager_thread;*/
extern const int		__pthread_sizeof_handle;
extern const int		__pthread_offsetof_descr;
extern const int		__pthread_offsetof_pid;
extern volatile	int		__pthread_handles_num;
#endif /* 0 */

/*
 * Notify others.
 */
int
linuxthreads_notify_others(	const int	signotify)
{
	const pid_t			mypid = getpid();
	//const pthread_t			mytid = pthread_self();
	int				i;
	int				threadcount = 0;
	int				threads[PTHREAD_THREADS_MAX];
	int				pid;

	TRACE_FPRINTF((stderr, "theadcount:%d\n", __pthread_handles_num));
	if (__pthread_handles_num==2) {
		/* no threads beside the initial thread */
		return 0;
	}
	/*assert(maxthreads>=3);
	assert(maxthreads>=__pthread_handles_num+2);*/

	// take the initial thread with us
	pid = AT_INT(&__pthread_initial_thread + __pthread_offsetof_pid);
	if (pid!=mypid && pid!=0)
		threads[threadcount++] = pid;
	// don't know why, but always handles[0]==handles[1]
	for (i=1; i<__pthread_handles_num; ++i) {
		const int descr = AT_INT(&__pthread_handles+i*__pthread_sizeof_handle+__pthread_offsetof_descr);
		assert(descr!=0);
		pid = AT_INT(descr+__pthread_offsetof_pid);
		if (pid!=mypid && pid!=0)
			threads[threadcount++] = pid;
	}
	/* TRACE_FPRINTF((stderr, "Stopping threads...")); */
	//for (i=0; i<threadcount; ++i) {
	//	/* TRACE_FPRINTF((stderr, "%d ", threads[i])); */
	//	fflush(stdout);
	//	kill(threads[i], SIGSTOP);	/* Tell thread to stop */
	//}
	/* TRACE_FPRINTF((stderr, " done!\n")); */
	for (i=0; i<threadcount; ++i) {
		TRACE_FPRINTF((stderr, "--- NOTIFYING %d\n", threads[i]));
		kill(threads[i], signotify);	/* Tell to print stack trace */
		/* TRACE_FPRINTF((stderr, "--- WAITING FOR %d\n", threads[i])); */
		/*pause();		 Wait for confirmation. */
	}
	for (i=0; i<threadcount; ++i)
		sched_yield();
	for (i=0; i<threadcount; ++i) {
		TRACE_FPRINTF((stderr, "--- KILLING %d\n", threads[i]));
		kill(threads[i], SIGKILL);	/* Tell thread die :) */
	}
	return __pthread_handles_num;
}

