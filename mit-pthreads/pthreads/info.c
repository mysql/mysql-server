/* hello */

#include <stdio.h>
#include <pthread.h>
#include <signal.h>

static const char *const state_names[] = {
#define __pthread_defstate(S,NAME) NAME,
#include "pthread/state.def"
#undef __pthread_defstate
	0
};

void (*dump_thread_info_fn) (struct pthread *, FILE *);

static void
dump_thread_info (thread, file)
     struct pthread *thread;
     FILE *file;
{
	/* machdep */
	/* attr */
	/* signals */
	/* wakeup_time */
	/* join */
	fprintf (file, "    thread @%p prio %3d %s", thread,
			 thread->pthread_priority, state_names[(int) thread->state]);
	switch (thread->state) {
	  case PS_FDLR_WAIT:
		fprintf (file, " fd %d[%d]", thread->data.fd.fd,
				 thread->data.fd.branch);
		fprintf (file, " owner %pr/%pw",
				 fd_table[thread->data.fd.fd]->r_owner,
				 fd_table[thread->data.fd.fd]->w_owner);
		break;
	}
	/* show where the signal handler gets run */
	if (thread == pthread_run)
		fprintf (file, "\t\t[ME!]");
	fprintf (file, "\n");
	if (dump_thread_info_fn)
		(*dump_thread_info_fn) (thread, file);
}

static void
pthread_dump_info_to_file (file)
     FILE *file;
{
	pthread_t t;
	for (t = pthread_link_list; t; t = t->pll)
		dump_thread_info (t, file);
}

void
pthread_dump_info ()
{
	if (ftrylockfile (stderr) != 0)
		return;
	fprintf (stderr, "process id %ld:\n", (long) getpid ());
	pthread_dump_info_to_file (stderr);
	funlockfile (stderr);
}

#ifdef SIGINFO
static void
sig_handler (sig)
     int sig;
{
	pthread_dump_info ();
}

void
pthread_setup_siginfo ()
{
	(void) signal (SIGINFO, sig_handler);
}
#endif
