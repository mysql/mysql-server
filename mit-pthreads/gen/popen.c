#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

static pid_t *pids = NULL;
static int pids_size = 0;
static int pids_top = 0;
static pthread_mutex_t pids_lock = PTHREAD_MUTEX_INITIALIZER;

FILE *popen(const char *cmd, const char *mode)
{
    int fds[2], parent_fd, child_fd, child_target, new_size, i;
    pid_t pid, *new_pids;

    /* Verify the mode. */
    if ((*mode != 'r' && *mode != 'w') || mode[1] != 0)
	    return NULL;

    /* Generate fds, and choose the parent and child fds. */
    if (pipe(fds) < 0)
	    return NULL;
    parent_fd = (*mode == 'r') ? fds[0] : fds[1];
    child_fd = (*mode == 'r') ? fds[1] : fds[0];

    /* Ensure that there is space in the pid table. */
    pthread_mutex_lock(&pids_lock);
    if (pids_size <= parent_fd) {
	    new_size = parent_fd + 1;
	    if ((new_pids = malloc(new_size * sizeof(pid_t))) == NULL) {
    		pthread_mutex_unlock(&pids_lock);
	        close(parent_fd);
	        close(child_fd);
	        return NULL;
	    }
		if (pids) {
			memcpy(new_pids, pids, pids_size * sizeof(pid_t));
	        free(pids);
		}
		while (pids_size < new_size)
	    	new_pids[pids_size++] = -1;
		pids = new_pids;
    }
    pthread_mutex_unlock(&pids_lock);

    /* Fork off a child process. */
	switch (pid = fork()) {
	case -1: /* Failed to fork. */
		close(parent_fd);
		close(child_fd);
		return NULL;
		break;
	case 0: /* Child */
		/*
		 * Set the child fd to stdout or stdin as appropriate,
	 	 * and close the parent fd. 
		 */
		child_target = (*mode == 'r') ? STDOUT_FILENO : STDIN_FILENO;
		if (child_fd != child_target) {
	    	dup2(child_fd, child_target);
	    	close(child_fd);
		}
		close(parent_fd);

		/* Close all parent fds from previous popens(). */
		for (i = 0; i < pids_top; i++) {
		    if (pids[i] != -1)
			close(i);
		}

		execl("/bin/sh", "sh", "-c", cmd, NULL);
		exit(1);
	default:
		break;
	}

    /* Record the parent fd in the pids table. */
    pthread_mutex_lock(&pids_lock);
    pids[parent_fd] = pid;
    if (pids_top < parent_fd + 1)
	    pids_top = parent_fd + 1;
    pthread_mutex_unlock(&pids_lock);

    /* Close the child fd and return a stdio buffer for the parent fd. */
    close(child_fd);
    return fdopen(parent_fd, mode);
}

int pclose(fp)
    FILE *fp;
{
    pid_t pid, result;
    int fd, pstat;

    fd = fileno(fp);
    pthread_mutex_lock(&pids_lock);
    /* Make sure this is a popened file. */
    if ((pids_top <= fd) || ((pid = pids[fd]) == -1)) {
    	pthread_mutex_unlock(&pids_lock);
		return -1;
	}
    pids[fd] = -1;
    while (pids_top > 0 && pids[pids_top - 1] == -1)
	    pids_top--;
    pthread_mutex_unlock(&pids_lock);

    fclose(fp);

    /* Wait for the subprocess to quit. */
	return (((result = waitpid(pid, &pstat, 0)) == -1) ? -1 : pstat);
}

