/* Copyright (C) 2008 Sun Microsystems

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef _WIN32
#include <process.h>
#endif
#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#define DAEMONEXPORT
#include <my_daemon.h>
#include <NdbConfig.h>

#ifdef _WIN32
#include <nt_servc.h>
NTService g_ntsvc;
HANDLE    g_evt;
#endif

static char *daemon_name;
static long daemonpid;

#define daemon_error_len 1024
char *my_daemon_error=0;
#define check(x,retval,fmt,s) \
  {long x_=(long)(x);if(!x_){int i=0;\
   if(!my_daemon_error)my_daemon_error=(char*)malloc(1024); \
   i=my_snprintf(my_daemon_error,daemon_error_len,fmt,s);\
   my_snprintf(&my_daemon_error[i],daemon_error_len-i,": %s (errno: %d)",\
               strerror(errno),errno);\
   return retval;    }}

struct MY_DAEMON g_daemon;

#ifdef _WIN32
static int stopper(void*)
{
  WaitForSingleObject(g_evt,INFINITE);
  return g_daemon.stop();
}
#endif

int my_daemon_run(char *name,struct MY_DAEMON*d)
{
  daemon_name= name;
  memcpy(&g_daemon,d,sizeof(g_daemon));
#ifdef _WIN32
  g_evt=CreateEvent(0, 0, 0, 0);
  g_ntsvc.SetShutdownEvent(g_evt);
  uintptr_t stop_thread= _beginthread((THREAD_FC)stopper,0,0);
  if(!stop_thread)
    return 1;
  check(!init(), 1, "init failed\n", "");
#else /* Fork */
  pid_t n = fork();
  check(n!=-1,-1,"fork failed: %s", strerror(errno));
  /* Exit if we are the parent */
  if (n != 0)
    exit(0);
  g_daemon.start(0);
#endif
  return 0;
}

static int pidfd, logfd;
#ifdef _WIN32
char *my_daemon_makecmdv(int c, const char **v)
{
  char exe[_MAX_PATH + 1], *t, *u;
  int i= 0, n= 0;

  GetModuleFileName(NULL, exe, sizeof(exe));
  n= strlen(exe);
  for (i= 0; i < c; i++)
    n += strlen(v[i]) + 4;
  u= t= (char*)my_malloc(n, MY_FAE);
  u += my_snprintf(u, n, "%s", exe);
  for (i= 0; i < c; i++)
    u += my_snprintf(u, n, " %s", v[i]);
  return t;
}

char *my_daemon_make_svc_cmd(int n, char **v)
{
  check(!strcmp(v[0],"-i") || !strcmp(v[0],"--install"),
        0, "the install option (-i) must be the first argument\n", "");
  *v= "-s";
  return my_daemon_makecmdv(n, (const char**)v);
}

int my_daemon_install(const char *name, const char *cmd)
{
  SC_HANDLE svc, scm;
  const char *s= name;

  while (*s && isalnum(*s++))
    ;
  if (*s)
    return 1;
  check(g_ntsvc.SeekStatus(name, 1), 1, "SeekStatus on %s failed", name);
  check(scm= OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE), 1,
        "Failed to install the service: "
        "Could not open Service Control Manager.\n", "");
  check(svc= CreateService(scm, name, name,
                            SERVICE_ALL_ACCESS,
                            SERVICE_WIN32_OWN_PROCESS,
                            (1 ? SERVICE_AUTO_START :
                             SERVICE_DEMAND_START), SERVICE_ERROR_NORMAL,
                            cmd, 0, 0, 0, 0, 0), 1,
        "Failed to install the service: "
        "Couldn't create service)\n",
        "") printf("Service successfully installed.\n");
  CloseServiceHandle(svc);
  CloseServiceHandle(scm);
  return 0;
}
#endif

int daemon_closefiles()
{
  close(pidfd);
  fclose(my_dlog);
  return 0;
}

static const char *pidfile, *logfile;
int my_daemon_prefiles(const char *pidfil, const char *logfil)
{
  int n;
  char buf[64];

  my_dlog= 0;
  pidfile= pidfil;
  logfile= logfil;
  pidfd= logfd= -1;
  /* open log file before becoming daemon */
  if (logfile != NULL)
  {
    logfd= open(logfile, O_CREAT | O_WRONLY | O_APPEND, 0644);
    check(logfd != -1, 1, "%s: open for write failed", logfile);
    my_dlog= fdopen(logfd, "a");
  }
  /* Check that we have write access to lock file */
  assert(pidfile != NULL);
  pidfd= open(pidfile, O_CREAT | O_RDWR, 0644);
  check(pidfd != -1, 1, "%s: open for write failed", pidfile);
  /* Read any old pid from lock file */
  n= read(pidfd, buf, sizeof(buf));
  check(n >= 0, 1, "%s: read failed", pidfile);
  buf[n]= 0;
  daemonpid= atol(buf);
  check(lseek(pidfd, 0, SEEK_SET) != -1, 1, "%s: lseek failed", pidfile);
#ifdef __WIN__                  //TODO: add my_lockf.c with these definitions
#define lockf _locking
#define F_TLOCK _LK_NBLCK
#define F_ULOCK _LK_UNLCK
#define F_LOCK  _LK_LOCK
#endif
#ifdef F_TLOCK
  /* Test for lock before becoming daemon */
  if (lockf(pidfd, F_TLOCK, 0) == -1)
    check(errno != EACCES && errno != EAGAIN, 1,
          "pidfile: already locked by pid=%ld", daemonpid);
  check(lockf(pidfd, F_ULOCK, 0) != -1, 1, "%s: fail to unlock", pidfile);
#endif
  return 0;
}

int my_daemon_files()
{
  int n;
  char buf[64];
  /* Running in child process */
  daemonpid= getpid();
#ifdef F_TLOCK
  /* Lock the lock file (likely to succeed due to test above) */
  check(lockf(pidfd, F_LOCK, 0) != -1, 1, "%s: lock failed", pidfile);
#endif
  /* Become process group leader */
  IF_WIN(0, check(setsid() != -1, 1, "setsid failed%s", ""));
  /* Write pid to lock file */
  check(IF_WIN(_chsize, ftruncate) (pidfd, 0) != -1, 1,
        "%s: ftruncate failed", pidfile);
  n= my_sprintf(buf, (buf, "%ld\n", daemonpid));
  check(write(pidfd, buf, n) == n, 1, "%s: write failed", pidfile);
  /* Do input/output redirections (assume fd 0,1,2 not in use) */
  close(0);
  check(0 == open(IF_WIN("nul:", "/dev/null"), O_RDONLY), 1, "close 0%s", "");
#ifdef _WIN32
  *stdout= *stderr= *my_dlog;
#else //no stdout/stderr on windows service
  if (logfd != 0)
  {
    dup2(logfd, 1);
    dup2(logfd, 2);
    close(logfd);
    my_dlog= stdout;
  }
#endif
  return 0;
}

#ifdef _WIN32
HANDLE hExitEvent;

static int evtlog(char *s)
{
  HANDLE eventsrc;
  int n= strlen(s);
  char *msg= (char*)my_malloc(n + 1, MY_FAE);
  char *ss[]= { msg };
  my_snprintf(msg, n, "\n\n%s", s);
  if(!(eventsrc= RegisterEventSource(0, daemon_name)))
	  return 1;
  if(!(ReportEvent(eventsrc, EVENTLOG_ERROR_TYPE, 0, 0, 0, 1, 0, (LPCSTR*)ss, 0)))
	  return 1;
  if(!(DeregisterEventSource(eventsrc)))
	  return 1;
  my_free(msg,MY_FAE);
  return 0;
}

static int init()
{
  return !g_ntsvc.Init(daemon_name,g_daemon.start);
}

int my_daemon_remove(const char *name)
{
  return !g_ntsvc.Remove((LPCSTR)name);
}
#endif
