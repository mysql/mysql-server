#ifndef _SYS_WAIT_H_
#define _SYS_WAIT_H_

#define WNOHANG		1	/* dont hang in wait */
#define WUNTRACED	2	/* tell about stopped, untraced children */

pid_t wait 				__P_((int *));
pid_t waitpid 				__P_((pid_t, int *, int));

#define _W_INT(i)       (i)
#define WCOREFLAG       0200

#define _WSTATUS(x)     (_W_INT(x) & 0177)
#define _WSTOPPED       0177            /* _WSTATUS if process is stopped */
#define WIFSTOPPED(x)   (_WSTATUS(x) == _WSTOPPED)
#define WSTOPSIG(x)     (_W_INT(x) >> 8)
#define WIFSIGNALED(x)  (_WSTATUS(x) != _WSTOPPED && _WSTATUS(x) != 0)
#define WTERMSIG(x)     (_WSTATUS(x))
#define WIFEXITED(x)    (_WSTATUS(x) == 0)
#define WEXITSTATUS(x)  (_W_INT(x) >> 8)

#endif /* _SYS_WAIT_H_ */
