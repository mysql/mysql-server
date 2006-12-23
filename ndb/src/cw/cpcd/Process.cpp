/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>

#include <BaseString.hpp>
#include <InputStream.hpp>

#include "common.hpp"
#include "CPCD.hpp"

#include <pwd.h>
#ifdef HAVE_GETRLIMIT
#include <sys/resource.h>
#endif

void
CPCD::Process::print(FILE * f){
  fprintf(f, "define process\n");
  fprintf(f, "id: %d\n",    m_id);
  fprintf(f, "name: %s\n",  m_name.c_str()  ? m_name.c_str()  : "");
  fprintf(f, "group: %s\n", m_group.c_str() ? m_group.c_str() : "");
  fprintf(f, "env: %s\n",   m_env.c_str()   ? m_env.c_str()   : "");
  fprintf(f, "path: %s\n",  m_path.c_str()  ? m_path.c_str()  : "");
  fprintf(f, "args: %s\n",  m_args.c_str()  ? m_args.c_str()  : "");
  fprintf(f, "type: %s\n",  m_type.c_str()  ? m_type.c_str()  : "");
  fprintf(f, "cwd: %s\n",   m_cwd.c_str()   ? m_cwd.c_str()   : "");
  fprintf(f, "owner: %s\n", m_owner.c_str() ? m_owner.c_str() : "");
  fprintf(f, "runas: %s\n", m_runas.c_str() ? m_runas.c_str() : "");
  fprintf(f, "stdin: %s\n", m_stdin.c_str() ? m_stdin.c_str() : "");
  fprintf(f, "stdout: %s\n", m_stdout.c_str() ? m_stdout.c_str() : "");
  fprintf(f, "stderr: %s\n", m_stderr.c_str() ? m_stderr.c_str() : "");
  fprintf(f, "ulimit: %s\n", m_ulimit.c_str() ? m_ulimit.c_str() : "");
  fprintf(f, "shutdown: %s\n", m_shutdown_options.c_str() ? 
	  m_shutdown_options.c_str() : "");
}

CPCD::Process::Process(const Properties & props, class CPCD *cpcd) {
  m_id = -1;
  m_pid = -1;
  props.get("id", (Uint32 *) &m_id);
  props.get("name", m_name);
  props.get("group", m_group);
  props.get("env", m_env);
  props.get("path", m_path);
  props.get("args", m_args);
  props.get("cwd", m_cwd);
  props.get("owner", m_owner);
  props.get("type", m_type);
  props.get("runas", m_runas);

  props.get("stdin", m_stdin);
  props.get("stdout", m_stdout);
  props.get("stderr", m_stderr);
  props.get("ulimit", m_ulimit);
  props.get("shutdown", m_shutdown_options);
  m_status = STOPPED;

  if(strcasecmp(m_type.c_str(), "temporary") == 0){
    m_processType = TEMPORARY;
  } else {
    m_processType = PERMANENT;
  }
  
  m_cpcd = cpcd;
}

void
CPCD::Process::monitor() { 
  switch(m_status) {
  case STARTING:
    break;
  case RUNNING:
    if(!isRunning()){
      m_cpcd->report(m_id, CPCEvent::ET_PROC_STATE_STOPPED);
      if(m_processType == TEMPORARY){
	m_status = STOPPED;
      } else {
	start();
      }
    }
    break;
  case STOPPED:
    assert(!isRunning());
    break;
  case STOPPING:
    break;
  }
}

bool
CPCD::Process::isRunning() {

  if(m_pid <= 1){
    //logger.critical("isRunning(%d) invalid pid: %d", m_id, m_pid);
    return false;
  }
  /* Check if there actually exists a process with such a pid */
  errno = 0;
  int s = kill((pid_t)-m_pid, 0); /* Sending "signal" 0 to a process only
				   * checkes if the process actually exists */
  if(s != 0) {
    switch(errno) {
    case EPERM:
      logger.critical("Not enough privileges to control pid %d\n", m_pid);
      break;
    case ESRCH:
      /* The pid in the file does not exist, which probably means that it
	 has died, or the file contains garbage for some other reason */
      break;
    default:
      logger.critical("Cannot not control pid %d: %s\n", m_pid, strerror(errno));
      break;
    }
    return false;
  } 
  return true;
}

int
CPCD::Process::readPid() {
  if(m_pid != -1){
    logger.critical("Reading pid while != -1(%d)", m_pid);
    return m_pid;
  }

  char filename[PATH_MAX*2+1];
  char buf[1024];
  FILE *f;

  memset(buf, 0, sizeof(buf));
  
  BaseString::snprintf(filename, sizeof(filename), "%d", m_id);
  
  f = fopen(filename, "r");
  
  if(f == NULL){
    return -1; /* File didn't exist */
  }
  
  errno = 0;
  size_t r = fread(buf, 1, sizeof(buf), f);
  fclose(f);
  if(r > 0)
    m_pid = strtol(buf, (char **)NULL, 0);
  
  if(errno == 0){
    return m_pid;
  }
  
  return -1;
}

int
CPCD::Process::writePid(int pid) {
  char tmpfilename[PATH_MAX+1+4+8];
  char filename[PATH_MAX*2+1];
  FILE *f;

  BaseString::snprintf(tmpfilename, sizeof(tmpfilename), "tmp.XXXXXX");
  BaseString::snprintf(filename, sizeof(filename), "%d", m_id);
  
  int fd = mkstemp(tmpfilename);
  if(fd < 0) {
    logger.error("Cannot open `%s': %s\n", tmpfilename, strerror(errno));
    return -1;	/* Couldn't open file */
  }

  f = fdopen(fd, "w");

  if(f == NULL) {
    logger.error("Cannot open `%s': %s\n", tmpfilename, strerror(errno));
    return -1;	/* Couldn't open file */
  }

  fprintf(f, "%d", pid);
  fclose(f);

  if(rename(tmpfilename, filename) == -1){
    logger.error("Unable to rename from %s to %s", tmpfilename, filename);
    return -1;
  }
  return 0;
}

static void
setup_environment(const char *env) {
  char **p;
  p = BaseString::argify("", env);
  for(int i = 0; p[i] != NULL; i++){
    /*int res = */ putenv(p[i]);
  }
}

static
int
set_ulimit(const BaseString & pair){
#ifdef HAVE_GETRLIMIT
  errno = 0;
  Vector<BaseString> list;
  pair.split(list, ":");
  if(list.size() != 2){
    logger.error("Unable to process ulimit: split >%s< list.size()=%d", 
		 pair.c_str(), list.size());
    return -1;
  }
  
  int res;
  rlim_t value = RLIM_INFINITY;
  if(!(list[1].trim() == "unlimited")){
    value = atoi(list[1].c_str());
  }

  struct rlimit rlp;
#define _RLIMIT_FIX(x) { res = getrlimit(x,&rlp); if(!res){ rlp.rlim_cur = value; res = setrlimit(x, &rlp); }}
  
  if(list[0].trim() == "c"){
    _RLIMIT_FIX(RLIMIT_CORE);
  } else if(list[0] == "d"){
    _RLIMIT_FIX(RLIMIT_DATA);
  } else if(list[0] == "f"){
    _RLIMIT_FIX(RLIMIT_FSIZE);
  } else if(list[0] == "n"){
    _RLIMIT_FIX(RLIMIT_NOFILE);
  } else if(list[0] == "s"){
    _RLIMIT_FIX(RLIMIT_STACK);
  } else if(list[0] == "t"){
    _RLIMIT_FIX(RLIMIT_CPU);
  } else {
    res= -11;
    errno = EINVAL;
  }
  if(res){
    logger.error("Unable to process ulimit: %s res=%d error=%d(%s)", 
		 pair.c_str(), res, errno, strerror(errno));
    return -1;
  }
#endif
  return 0;
}

void
CPCD::Process::do_exec() {
  size_t i; 
  setup_environment(m_env.c_str());

  char **argv = BaseString::argify(m_path.c_str(), m_args.c_str());

  if(strlen(m_cwd.c_str()) > 0) {
    int err = chdir(m_cwd.c_str());
    if(err == -1) {
      BaseString err;
      logger.error("%s: %s\n", m_cwd.c_str(), strerror(errno));
      _exit(1);
    }
  }

  Vector<BaseString> ulimit;
  m_ulimit.split(ulimit);
  for(i = 0; i<ulimit.size(); i++){
    if(ulimit[i].trim().length() > 0 && set_ulimit(ulimit[i]) != 0){
      _exit(1);
    }
  }

  int fd = open("/dev/null", O_RDWR, 0);
  if(fd == -1) {
    logger.error("Cannot open `/dev/null': %s\n", strerror(errno));
    _exit(1);
  }
  
  BaseString * redirects[] = { &m_stdin, &m_stdout, &m_stderr };
  int fds[3];
  for(i = 0; i<3; i++){
    if(redirects[i]->empty()){
#ifndef DEBUG
      dup2(fd, i);
#endif
      continue;
    }
    
    if((* redirects[i]) == "2>&1" && i == 2){
      dup2(fds[1], 2);
      continue;
    }
    
    /**
     * Make file
     */
    int flags = 0;
    int mode = S_IRUSR | S_IWUSR ;
    if(i == 0){
      flags |= O_RDONLY;
    } else {
      flags |= O_WRONLY | O_CREAT | O_APPEND;
    }
    int f = fds[i]= open(redirects[i]->c_str(), flags, mode);
    if(f == -1){
      logger.error("Cannot redirect %d to/from '%s' : %s\n", i, 
		   redirects[i]->c_str(), strerror(errno));
      _exit(1);
    }
    dup2(f, i);
  }

  /* Close all filedescriptors */
  for(i = STDERR_FILENO+1; (int)i < getdtablesize(); i++)
    close(i);

  execv(m_path.c_str(), argv);
  /* XXX If we reach this point, an error has occurred, but it's kind of hard
   * to report it, because we've closed all files... So we should probably
   * create a new logger here */
  logger.error("Exec failed: %s\n", strerror(errno));
  /* NOTREACHED */
}

int
CPCD::Process::start() {
  /* We need to fork() twice, so that the second child (grandchild?) can
   * become a daemon. The original child then writes the pid file,
   * so that the monitor knows the pid of the new process, and then
   * exit()s. That way, the monitor process can pickup the pid, and
   * the running process is a daemon.
   *
   * This is a bit tricky but has the following advantages:
   *  - the cpcd can die, and "reconnect" to the monitored clients
   *    without restarting them.
   *  - the cpcd does not have to wait() for the childs. init(1) will
   *    take care of that.
   */
  logger.info("Starting %d: %s", m_id, m_name.c_str());
  m_status = STARTING;
    
  int pid = -1;
  switch(m_processType){
  case TEMPORARY:{
    /**
     * Simple fork
     * don't ignore child
     */
    switch(pid = fork()) {
    case 0: /* Child */
      setsid();
      writePid(getpgrp());
      if(runas(m_runas.c_str()) == 0){
        signal(SIGCHLD, SIG_DFL);
	do_exec();
      }
      _exit(1);
      break;
    case -1: /* Error */
      logger.error("Cannot fork: %s\n", strerror(errno));
      m_status = STOPPED;
      return -1;
      break;
    default: /* Parent */
      logger.debug("Started temporary %d : pid=%d", m_id, pid);
      m_cpcd->report(m_id, CPCEvent::ET_PROC_STATE_RUNNING);
      break;
    }
    break;
  }
  case PERMANENT:{
    /**
     * PERMANENT
     */
    switch(fork()) {
    case 0: /* Child */
      signal(SIGCHLD, SIG_IGN);
      switch(pid = fork()) {
      case 0: /* Child */
	setsid();
	writePid(getpgrp());
	if(runas(m_runas.c_str()) != 0){
	  _exit(1);
	}
        signal(SIGCHLD, SIG_DFL);
	do_exec();
	_exit(1);
	/* NOTREACHED */
	break;
      case -1: /* Error */
	logger.error("Cannot fork: %s\n", strerror(errno));
	writePid(-1);
	_exit(1);
	break;
      default: /* Parent */
	logger.debug("Started permanent %d : pid=%d", m_id, pid);
	_exit(0);
	break;
      }
      break;
    case -1: /* Error */
      logger.error("Cannot fork: %s\n", strerror(errno));
      m_status = STOPPED;
      return -1;
      break;
    default: /* Parent */
      m_cpcd->report(m_id, CPCEvent::ET_PROC_STATE_RUNNING);
      break;
    }
    break;
  }
  default:
    logger.critical("Unknown process type");
    return -1;
  }

  while(readPid() < 0){
    sched_yield();
  }
  
  errno = 0;
  pid_t pgid = getpgid(pid);
  
  if(pgid != -1 && pgid != m_pid){
    logger.error("pgid and m_pid don't match: %d %d (%d)", pgid, m_pid, pid);
  }
  
  if(isRunning()){
    m_status = RUNNING;
    return 0;
  }
  m_status = STOPPED;
  return -1;
}

void
CPCD::Process::stop() {

  char filename[PATH_MAX*2+1];
  BaseString::snprintf(filename, sizeof(filename), "%d", m_id);
  unlink(filename);
  
  if(m_pid <= 1){
    logger.critical("Stopping process with bogus pid: %d id: %d", 
		    m_pid, m_id);
    return;
  }
  m_status = STOPPING;
  
  errno = 0;
  int signo= SIGTERM;
  if(m_shutdown_options == "SIGKILL")
    signo= SIGKILL;

  int ret = kill(-m_pid, signo);
  switch(ret) {
  case 0:
    logger.debug("Sent SIGTERM to pid %d", (int)-m_pid);
    break;
  default:
    logger.debug("kill pid: %d : %s", (int)-m_pid, strerror(errno));
    break;
  }
  
  if(isRunning()){
    errno = 0;
    ret = kill(-m_pid, SIGKILL);
    switch(ret) {
    case 0:
      logger.debug("Sent SIGKILL to pid %d", (int)-m_pid);
      break;
    default:
      logger.debug("kill pid: %d : %s\n", (int)-m_pid, strerror(errno));
      break;
    }
  } 
  
  m_pid = -1;
  m_status = STOPPED;
}
