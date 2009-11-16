/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#ifdef _WIN32
#include <process.h>
#include <sys/stat.h>
#include <io.h>
#endif
#include <BaseString.hpp>
#include <InputStream.hpp>

#include "common.hpp"
#include "CPCD.hpp"
#include <errno.h>

#ifndef _WIN32
#include <pwd.h>
#else
#include <direct.h>
#endif

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
  m_pid = bad_pid;
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
#ifdef _WIN32
    logger.critical("Process type must be 'temporary' on windows");
    exit(1);
#endif
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

  if (is_bad_pid(m_pid)) {
    //logger.critical("isRunning(%d) invalid pid: %d", m_id, m_pid);
    return false;
  }
  /* Check if there actually exists a process with such a pid */
  errno = 0;

#ifdef _WIN32
  HANDLE proc;

  if (!(proc = OpenProcess(PROCESS_QUERY_INFORMATION, 0, m_pid))) {
    logger.debug("Cannot OpenProcess on %d\n", m_pid);
    return false;
  }

  BOOL result;
  DWORD exitcode;
  if (result = GetExitCodeProcess(proc, &exitcode) && exitcode != STILL_ACTIVE) {
    CloseHandle(proc);
    return false;
  }

  CloseHandle(proc);

#else
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
#endif
  return true;
}

int
CPCD::Process::readPid() {
  if (!is_bad_pid(m_pid)) {
    logger.critical("Reading pid while having valid process (%d)", m_pid);
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
#ifdef _WIN32
inline int mkstemp(char *tmp)
{
  int fd;

  if (!_mktemp(tmp))
    return -1;
  fd = _open(tmp, _O_CREAT|_O_RDWR|_O_TEXT|_O_TRUNC, _S_IREAD|_S_IWRITE);
  return fd;
}
#endif

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

#ifdef _WIN32
  unlink(filename);
#endif

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

#ifdef _WIN32
const int S_IRUSR = _S_IREAD, S_IWUSR = _S_IWRITE;

static void
save_environment(const char *env, Vector<BaseString> &saved) {
  char **ptr;

  ptr = BaseString::argify("", env);
  if(!ptr) {
    logger.error("Could not argify new environment");
    return;
  }

  for(int i = 0; ptr[i] != NULL; i++) {
    if(!ptr[i][0]) {
      continue;
    }
    char *str1 = strdup(ptr[i]);
    char *str2;
    BaseString bs;

    *strchr(str1, '=') = 0;
    str2 = getenv(str1);
    bs.assfmt("%s=%s", str1, str2 ? str2 : "");
    saved.push_back(bs);
  }
}
#endif

void
CPCD::Process::do_exec() {
  size_t i;

#ifdef _WIN32
  Vector<BaseString> saved;
  char *cwd = 0;
  save_environment(m_env.c_str(), saved);
#endif

  setup_environment(m_env.c_str());

  char **argv = BaseString::argify(m_path.c_str(), m_args.c_str());

  if(strlen(m_cwd.c_str()) > 0) {
#ifdef _WIN32
    cwd = getcwd(0, 0);
    if(!cwd)
    {
      logger.critical("Couldn't getcwd before spawn");
    }
#endif
    int err = chdir(m_cwd.c_str());
    if(err == -1) {
      BaseString err;
      logger.error("%s: %s\n", m_cwd.c_str(), strerror(errno));
      _exit(1);
    }
  }
#ifndef _WIN32
  Vector<BaseString> ulimit;
  m_ulimit.split(ulimit);
  for(i = 0; i<ulimit.size(); i++){
    if(ulimit[i].trim().length() > 0 && set_ulimit(ulimit[i]) != 0){
      _exit(1);
    }
  }
#endif

  const char *nul = IF_WIN("nul:", "/dev/null");
  int fdnull = open(nul, O_RDWR, 0);
  if(fdnull == -1) {
    logger.error("Cannot open `%s': %s\n", nul, strerror(errno));
    _exit(1);
  }
  
  BaseString * redirects[] = { &m_stdin, &m_stdout, &m_stderr };
  int fds[3];
#ifdef _WIN32
  int std_dups[3];
#endif
  for (i = 0; i < 3; i++) {
#ifdef _WIN32
    std_dups[i] = dup(i);
#endif
    if (redirects[i]->empty()) {
#ifndef DEBUG
      dup2(fdnull, i);
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
      logger.error("Cannot redirect %ld to/from '%s' : %s\n", i,
		   redirects[i]->c_str(), strerror(errno));
      _exit(1);
    }
    dup2(f, i);
#ifdef _WIN32
    close(f);
#endif
  }

#ifndef _WIN32
  /* Close all filedescriptors */
  for(i = STDERR_FILENO+1; (int)i < getdtablesize(); i++)
    close(i);

  execv(m_path.c_str(), argv);
  /* XXX If we reach this point, an error has occurred, but it's kind of hard
   * to report it, because we've closed all files... So we should probably
   * create a new logger here */
  logger.error("Exec failed: %s\n", strerror(errno));
  /* NOTREACHED */
#else
  BaseString cmd;
  cmd.assfmt("'%s %s'", m_path.c_str(), m_args.c_str());
  const char *sh_argv[] = {"sh", "-c", cmd.c_str(), 0};
  BaseString shcmd;
  shcmd.assfmt("sh -c '%s'", cmd.c_str());
  logger.critical(shcmd.c_str());

  HANDLE proc = (HANDLE)_spawnvp(_P_NOWAIT, "sh", sh_argv);
  // go back up to original cwd
  if(chdir(cwd))
  {
    logger.critical("Couldn't go back to saved cwd after spawn()");
    logger.critical("%s", strerror(errno));
  }
  free(cwd);

  // get back to original std i/o
  for(i = 0; i < 3; i++) {
    dup2(std_dups[i], i);
    close(std_dups[i]);
  }

  for (i = 0; i < saved.size(); i++) {
    putenv(saved[i].c_str());
  }

  DWORD exitcode;
  BOOL result = GetExitCodeProcess(proc, &exitcode);
  //maybe a short running process
  if (result && exitcode != 259) {
    m_status = STOPPED;
    logger.warning("Process terminated\n");
  }

  int pid = GetProcessId(proc);
  CloseHandle(proc);
  if (!pid) {
    logger.critical("Couldn't get process ID");
  } else {
    logger.debug("new pid: %d\n", pid);
  }

  m_status = RUNNING;
  writePid(pid);
#endif

  close(fdnull);
}

#ifdef _WIN32
void sched_yield() {
  Sleep(100);
}
#endif

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
#ifndef _WIN32
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
      break;
    }
#else //_WIN32
    do_exec();
#endif
    break;
  }
#ifndef _WIN32
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
      break;
    }
    break;
  }
#endif
  default:
    logger.critical("Unknown process type");
    return -1;
  }

  while(readPid() < 0){
    sched_yield();
  }
  
  errno = 0;
  pid_t pgid = IF_WIN(-1, getpgid(pid));
  
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


#ifdef _WIN32
#include <tlhelp32.h>
/*
   fill pids with pairs:
    - all valid pids ordered as (child, parent)
*/
struct Pair
{
  pid_t child, parent;
};

typedef Vector<struct Pair> Pairs;

static void get_processes(Pairs & pairs)
{
  HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 pe = { 0 };

  pairs.clear();
  pe.dwSize = sizeof(PROCESSENTRY32);
  if(Process32First(h, &pe))
  {
    do
    {
      struct Pair pr;
      pr.child  = pe.th32ProcessID;
      pr.parent = pe.th32ParentProcessID;
      pairs.push_back(pr);
    } while(Process32Next(h, &pe));
  }
  CloseHandle(h);
}

/*
   will kill pid after killing it's children.
   last two parameters come from get_processes()
*/
static int kill_tree(pid_t pid, Pairs & pairs)
{
  size_t i, j, k, new_count;
  Vector<pid_t> parents, new_parents;

  parents.push_back(pid);
  do
  {
    for(i = 0; i < pairs.size(); i++)
    {
      for(j = 0; j < parents.size(); j++)
      {
        if(pairs[i].parent == parents[j])
        {
          bool already_a_parent = false;
          for(size_t x = 0; x < parents.size(); x++)
          {
            if(parents[x] == pairs[i].child)
            {
              already_a_parent = true;
              break;
            }
          }
          if (!already_a_parent)
          {
            new_parents.push_back(pairs[i].child);
          }
        }
      }
    }

    new_count = new_parents.size();
    for(k = 0; k < new_count; k++)
    {
      parents.push_back(new_parents[k]);
    }
    new_parents.clear();
  } while(new_count);

  logger.debug("killing processes in order: ");
  for(long i = (long)parents.size() - 1; i >= 0; i--)
  {
    logger.debug(" %d", parents[i]);
  }
  logger.debug(".\n");

  for(long i = (long)parents.size() - 1; i >= 0; i--)
  {
    pid_t pid = parents[i];
    HANDLE proc = OpenProcess(PROCESS_TERMINATE, 0, pid);
    if (!proc)
    {
      logger.info("Cannot open process %d (during a kill_tree)\n", pid);
      return 1;
    }

    BOOL tp = TerminateProcess(proc, -1);
    CloseHandle(proc);

    if(!tp)
    {
      return 2;
    }
  }
  return 0;
}

/*
  function to kill pid and children processes
*/
static int kill_process_tree(pid_t pid)
{
  Pairs pairs;

  get_processes(pairs);
  if(!pairs.size())
  {
    return 1;
  }
  return kill_tree(pid, pairs);
}
#endif

void
CPCD::Process::stop() {

  char filename[PATH_MAX*2+1];
  BaseString::snprintf(filename, sizeof(filename), "%d", m_id);
  unlink(filename);

  if (is_bad_pid(m_pid))
  {
    logger.critical("Stopping process with bogus pid: %d id: %d", 
                   m_pid, m_id);
    return;
  }

  m_status = STOPPING;

#ifndef _WIN32
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
#else
  if(isRunning())
  {
    kill_process_tree(m_pid);
  }
#endif

  m_pid = bad_pid;
  m_status = STOPPED;
}
