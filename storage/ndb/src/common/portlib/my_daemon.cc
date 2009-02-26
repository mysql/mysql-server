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
HANDLE    g_shutdown_evt;
#endif

static char *daemon_name;
static long daemonpid;

#define errorlen 1023
char my_daemon_error[errorlen+1];
int ERR1(const char*fmt,...) {
  va_list argptr;
  va_start(argptr, fmt);
  my_snprintf(my_daemon_error,errorlen,fmt,argptr);
  va_end(argptr);
  return 1;
}

struct MY_DAEMON g_daemon;

#ifdef _WIN32
static int init();
static int stopper(void*)
{
  WaitForSingleObject(g_shutdown_evt,INFINITE);
  return g_daemon.stop();
}
#endif

int my_daemon_run(char *name,struct MY_DAEMON*d)
{
  daemon_name= name;
  memcpy(&g_daemon,d,sizeof(g_daemon));
#ifdef _WIN32
  g_shutdown_evt=CreateEvent(0, 0, 0, 0);
  g_ntsvc.SetShutdownEvent(g_shutdown_evt);
  uintptr_t stop_thread= _beginthread((THREAD_FC)stopper,0,0);
  if(!stop_thread)
    return ERR1("couldn't start stopper thread\n");
  if(init())
    return ERR1("init failed\n");
#else /* Fork */
  pid_t n = fork();
  if(n==-1)
    return ERR1("fork failed: %s", strerror(errno));
  /* Exit if we are the parent */
  if (n != 0)
    exit(0);
  g_daemon.start(0);
#endif
  return 0;
}

#ifdef _WIN32
char *my_daemon_makecmdv(int c, const char **v)
{
  char exe[_MAX_PATH + 3], *retval, *strpos;
  int i= 0, n= 0;

  GetModuleFileName(NULL, exe, sizeof(exe));
  n= strlen(exe);
  for (i= 0; i < c; i++)
    n += strlen(v[i]) + 8;
  strpos= retval= (char*)my_malloc(n, MY_FAE);
  strpos += my_snprintf(strpos, n, "\"%s\"", exe);
  for (i= 0; i < c; i++)
    strpos += my_snprintf(strpos, n, " \"%s\"", v[i]);
  return retval;
}

static int startswith(char*s,char**set)
{
  char**item=set;
  for(;*item;item++)
    if(!strncmp(*item,s,strlen(*item)))
      return 1;
  return 0;
}

char *my_daemon_make_svc_cmd(int n, char **v, char *name)
{
  char*swi[]= {"--install","-i",0},
      *swirs[]= {"--remove","-r","--install","-i","--service","-s",0};
  if(!startswith(v[0],swi))
    return ERR1("The install option (-i) must be the first argument\n"),0;
  int i= 0;
  for(i=1;i<n;i++)
    if(startswith(v[i],swirs))
       return ERR1("The install option (-i) must be the only -i or -r"
                  " on command line\n"),0;
  size_t opt_size= strlen(name)+16;
  char*svcopt=(char*)my_malloc(opt_size, MY_FAE);
  my_snprintf(svcopt,opt_size-1,"--service=%s",name);

  size_t size=sizeof(char*)*(n+2);
  char**v1= (char**)my_malloc(size, MY_FAE);
  memset(v1,0,size);
  i=0;
  v1[i++]= svcopt;

  int j= 1;
  for(;j<n&&v[j][0]!='-';j++); // skip through to first option
  for(;j<n;j++)
    v1[i++]= v[j];
  return my_daemon_makecmdv(i, (const char**)v1);
}

//returns -1: no install/remove, 0: success, +ve error
int maybe_install_or_remove_service(int argc_,char**argv_,char*opts_remove,char*opts_install,char*default_name)
{
  if(argc_<2)
    return -1;
  char*svc=default_name,
      *r[]={"-r","--remove",0},
      *i[]={"-i","--install",0};
  if(opts_remove||startswith(argv_[1],r)) {
    if(opts_remove)
      svc=(char*)opts_remove;
    printf("Removing service \"%s\"\n",svc);
    return my_daemon_remove(svc);
  }
  if(opts_install||startswith(argv_[1],i)) {
    char *svc_cmd;
    if(opts_install)
      svc=(char*)opts_install;
    svc_cmd= my_daemon_make_svc_cmd(argc_-1, argv_+1, svc);
    if(!svc_cmd) {
      fprintf(stderr, my_daemon_error);
      return 1;
    }
    printf("Installing service \"%s\"\n",svc);
    printf("as \"%s\"\n",svc_cmd);
    return my_daemon_install(svc, svc_cmd);
  }
  return -1;
}

int my_daemon_install(const char *name, const char *cmd)
{
  SC_HANDLE svc= 0, scm= 0;

  if(!g_ntsvc.SeekStatus(name, 1))
    return ERR1("SeekStatus on %s failed\n", name);
  if(!(scm= OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE)))
    return ERR1("Failed to install the service: "
               "Could not open Service Control Manager.\n");
  if(!(svc= CreateService(scm, name, name,
                          SERVICE_ALL_ACCESS,
                          SERVICE_WIN32_OWN_PROCESS,
                          (1 ? SERVICE_AUTO_START :
                          SERVICE_DEMAND_START), SERVICE_ERROR_NORMAL,
                          cmd, 0, 0, 0, 0, 0)))
    return CloseServiceHandle(scm),
           ERR1("Failed to install the service: "
               "Couldn't create service)\n");
  printf("Service successfully installed.\n");
  CloseServiceHandle(svc);
  CloseServiceHandle(scm);
  return 0;
}
#endif
static int pidfd, logfd;
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
    if(logfd == -1)
      return ERR1("%s: open for write failed\n", logfile);
    my_dlog= fdopen(logfd, "a");
  }
  /* Check that we have write access to lock file */
  assert(pidfile != NULL);
  pidfd= open(pidfile, O_CREAT | O_RDWR, 0644);
  if(pidfd == -1)
    return ERR1("%s: open for write failed\n", pidfile);
  /* Read any old pid from lock file */
  n= read(pidfd, buf, sizeof(buf));
  if(n < 0)
    return ERR1("%s: read failed\n", pidfile);
  buf[n]= 0;
  daemonpid= atol(buf);
  if(lseek(pidfd, 0, SEEK_SET) == -1)
    return ERR1("%s: lseek failed\n", pidfile);
#ifdef __WIN__                  //TODO: add my_lockf.c with these definitions
#define lockf _locking
#define F_TLOCK _LK_NBLCK
#define F_ULOCK _LK_UNLCK
#define F_LOCK  _LK_LOCK
#endif
#ifdef F_TLOCK
  /* Test for lock before becoming daemon */
  if(lockf(pidfd, F_TLOCK, 0) == -1)
    if(errno == EACCES || errno == EAGAIN)
      return ERR1("pidfile: already locked by pid=%ld\n", daemonpid);
  if(lockf(pidfd, F_ULOCK, 0) == -1)
    return ERR1("%s: fail to unlock\n", pidfile);
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
  if(lockf(pidfd, F_LOCK, 0) == -1)
    return ERR1("%s: lock failed\n", pidfile);
#endif
#ifndef _WIN32
  /* Become process group leader */
  if(setsid()==-1)
    return ERR1("setsid failed\n");
#endif
  /* Write pid to lock file */
  if(IF_WIN(_chsize, ftruncate)(pidfd, 0) == -1)
    return ERR1("%s: ftruncate failed\n", pidfile);
  n= my_sprintf(buf, (buf, "%ld\n", daemonpid));
  if(write(pidfd, buf, n) != n)
    return ERR1("%s: write failed\n", pidfile);
  /* Do input/output redirections (assume fd 0,1,2 not in use) */
  close(0);
  const char* fname=IF_WIN("nul:", "/dev/null");
  if(open(fname, O_RDONLY)==-1)
    return ERR1("couldn't open %s\n", fname);
#ifdef _WIN32 //no stdout/stderr on windows service
  *stdout= *stderr= *my_dlog;
#else
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

static DWORD WINAPI main_function(LPVOID x)
{
  g_ntsvc.SetRunning();
  g_daemon.start(0);
  return 0;
}

static int init()
{
  return !g_ntsvc.Init(daemon_name,main_function);
}

int my_daemon_remove(const char *name)
{
  return !g_ntsvc.Remove((LPCSTR)name);
}
#endif
