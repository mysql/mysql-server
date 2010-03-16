/* Copyright (C) 2003-2008 MySQL AB, 2009 Sun Microsystems, Inc.

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
#include <ndb_version.h>
#include <signal.h>

#include "angel.hpp"

#include <NdbConfig.h>
#include <NdbAutoPtr.hpp>
#include <NdbDaemon.h>

#include <ConfigRetriever.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

static void
angel_exit(int code)
{
#ifdef HAVE_gcov
  exit(code);
#else
  _exit(code);
#endif
}

#include "../mgmapi/mgmapi_configuration.hpp"

static void
reportShutdown(const ndb_mgm_configuration* config,
               NodeId nodeid, int error_exit,
               bool restart, bool nostart, bool initial,
               Uint32 error, Uint32 signum, Uint32 sphase)
{
  // Only allow "initial" and "nostart" to be set if "restart" is set
  assert(restart ||
         (!restart && !initial && !nostart));

  Uint32 length, theData[25];
  EventReport *rep=(EventReport *) theData;

  rep->setNodeId(nodeid);
  if (restart)
    theData[1]=1 |
      (nostart ? 2 : 0) |
      (initial ? 4 : 0);
  else
    theData[1]=0;

  if (error_exit == 0)
  {
    rep->setEventType(NDB_LE_NDBStopCompleted);
    theData[2]=signum;
    length=3;
  } else
  {
    rep->setEventType(NDB_LE_NDBStopForced);
    theData[2]=signum;
    theData[3]=error;
    theData[4]=sphase;
    theData[5]=0; // extra
    length=6;
  }

  // Log event locally
  g_eventLogger->log(rep->getEventType(), theData, length,
                     rep->getNodeId(), 0);

  // Log event to cluster log
  ndb_mgm_configuration_iterator iter(*config, CFG_SECTION_NODE);
  for (iter.first(); iter.valid(); iter.next())
  {
    Uint32 type;
    if (iter.get(CFG_TYPE_OF_SECTION, &type) ||
       type != NODE_TYPE_MGM)
      continue;

    Uint32 port;
    if (iter.get(CFG_MGM_PORT, &port))
      continue;

    const char* hostname;
    if (iter.get(CFG_NODE_HOST, &hostname))
      continue;

    BaseString connect_str;
    connect_str.assfmt("%s:%d", hostname, port);


    NdbMgmHandle h = ndb_mgm_create_handle();
    if (h == 0)
    {
      g_eventLogger->warning("Unable to report shutdown reason "
                             "to '%s'(failed to create mgm handle)",
                             connect_str.c_str());
      continue;
    }

    if (ndb_mgm_set_connectstring(h, connect_str.c_str()) ||
        ndb_mgm_connect(h, 1, 0, 0) ||
        ndb_mgm_report_event(h, theData, length))
    {
      g_eventLogger->warning("Unable to report shutdown reason "
                             "to '%s'(error: %s - %s)",
                             connect_str.c_str(),
                             ndb_mgm_get_latest_error_msg(h),
                             ndb_mgm_get_latest_error_desc(h));
    }

    ndb_mgm_destroy_handle(&h);
  }
}


static void
ignore_signals(void)
{
  static const int ignore_list[] = {
#ifdef SIGBREAK
    SIGBREAK,
#endif
#ifdef SIGHUP
    SIGHUP,
#endif
    SIGINT,
#if defined SIGPWR
    SIGPWR,
#elif defined SIGINFO
    SIGINFO,
#endif
    SIGQUIT,
    SIGTERM,
#ifdef SIGTSTP
    SIGTSTP,
#endif
#ifdef SIGTTIN
    SIGTTIN,
#endif
#ifdef SIGTTOU
    SIGTTOU,
#endif
    SIGABRT,
#ifdef SIGALRM
    SIGALRM,
#endif
#ifdef SIGBUS
    SIGBUS,
#endif
    SIGFPE,
    SIGILL,
#ifdef SIGIO
    SIGIO,
#endif
#ifdef SIGPOLL
    SIGPOLL,
#endif
    SIGSEGV,
    SIGPIPE,
#ifdef SIGTRAP
    SIGTRAP
#endif
  };

  for(size_t i = 0; i < sizeof(ignore_list)/sizeof(ignore_list[0]); i++)
    signal(ignore_list[i], SIG_IGN);
}

#ifdef _WIN32
static inline
int pipe(int pipefd[2]){
  const unsigned int buffer_size = 4096;
  const int flags = 0;
  return _pipe(pipefd, buffer_size, flags);
}

#undef getpid
#include <process.h>

typedef DWORD pid_t;

static inline
pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
  HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (handle == NULL)
  {
    g_eventLogger->error("waitpid: Could not open handle for pid %d, "
                         "error: %d", pid, GetLastError());
    return -1;
  }
  return _cwait(stat_loc, (intptr_t)handle, 0);
}

static inline
bool WIFEXITED(int status)
{
  return true;
}

static inline
int WEXITSTATUS(int status)
{
  return status;
}

static inline
bool WIFSIGNALED(int status)
{
  return false;
}

static inline
int WTERMSIG(int status)
{
  return 0;
}
#endif

extern int main(int, char**);

static pid_t
spawn_process(const char* progname, const BaseString& args)
{
  char** argv = BaseString::argify(progname, args.c_str());
  if (!argv)
  {
    g_eventLogger->error("spawn_process: Failed to create argv, errno: %d",
                         errno);
    return -1;
  }

#ifdef _WIN32

  intptr_t spawn_handle = _spawnv(P_NOWAIT, progname, argv);
  if (spawn_handle == -1)
  {
    g_eventLogger->error("spawn_process: Failed to spawn process, errno: %d",
                         errno);
    return -1;
  }

  // Convert the handle returned from spawnv_ to a pid
  DWORD pid = GetProcessId((HANDLE)spawn_handle);
  if (pid == 0)
  {
    g_eventLogger->error("spawn_process: Failed to convert handle %d "
                         "to pid, error: %d", spawn_handle, GetLastError());
    return -1;
  }
  return pid;
#else
  pid_t pid = fork();
  if (pid == -1)
  {
    g_eventLogger->error("Failed to fork, errno: %d", errno);
    return -1;
  }

  if (pid)
  {
    // Parent
    return pid;
  }

  // Conunt number of arguments
  int argc = 0;
  while(argv[argc])
    argc++;

#ifdef VM_TRACE
#define NDB_USE_FORK_AND_EXEC
#endif
#ifdef NDB_USE_FORK_AND_EXEC
  // Start program from beginning using execv which is
  // as close as possible to the Windows implementation
  if (execv(argv[0], argv) == -1)
    g_eventLogger->error("Failed to execv, errno: %d", errno);
#else
  // Calling 'main' to start program from beginning
  // without loading (possibly new version) from disk
  (void)main(argc, argv);
  assert(false); // main should never return
#endif
  exit(1);
  return -1; // Never reached
#endif
}

static Uint32 stop_on_error;


/*
  Extract the config parameters that concerns angel
*/

static bool
configure(const ndb_mgm_configuration* conf, NodeId nodeid)
{
  Uint32 generation = 0;
  ndb_mgm_configuration_iterator sys_iter(*conf, CFG_SECTION_SYSTEM);
  if (sys_iter.get(CFG_SYS_CONFIG_GENERATION, &generation))
  {
    g_eventLogger->warning("Configuration didn't contain generation "
                           "(likely old ndb_mgmd");
  }
  g_eventLogger->info("Using configuration with generation %u", generation);

  ndb_mgm_configuration_iterator iter(*conf, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, nodeid))
  {
    g_eventLogger->error("Invalid configuration fetched, could not "
                         "find own node id %d", nodeid);
    return false;
  }

  if (iter.get(CFG_DB_STOP_ON_ERROR, &stop_on_error))
  {
    g_eventLogger->error("Invalid configuration fetched, could not "
                         "find StopOnError");
    return false;
  }
  g_eventLogger->debug("Using StopOnError: %u", stop_on_error);

  const char * datadir;
  if (iter.get(CFG_NODE_DATADIR, &datadir))
  {
    g_eventLogger->error("Invalid configuration fetched, could not "
                         "find DataDir");
    return false;
  }
  g_eventLogger->debug("Using DataDir: %s", datadir);

  NdbConfig_SetPath(datadir);

  my_setwd(NdbConfig_get_path(0), MYF(0));

  return true;
}


// Temporary duplicate define
enum NdbRestartType {
  NRT_Default               = 0,
  NRT_NoStart_Restart       = 1, // -n
  NRT_DoStart_Restart       = 2, //
  NRT_NoStart_InitialStart  = 3, // -n -i
  NRT_DoStart_InitialStart  = 4  // -i
};


void
angel_run(const BaseString& original_args,
          const char* connect_str,
          const char* bind_address,
          bool initial,
          bool no_start,
          bool daemon)
{
  ConfigRetriever retriever(connect_str,
                            NDB_VERSION,
                            NDB_MGM_NODE_TYPE_NDB,
                            bind_address);
  if (retriever.hasError())
  {
    g_eventLogger->error("Could not initialize connection to management "
                         "server, error: '%s'", retriever.getErrorString());
    angel_exit(1);
  }

  const int connnect_retries = 12;
  const int connect_delay = 5;
  const int verbose = 1;
  if (retriever.do_connect(connnect_retries, connect_delay, verbose) != 0)
  {
    g_eventLogger->error("Could not connect to management server, "
                         "error: '%s'", retriever.getErrorString());
    angel_exit(1);
  }
  g_eventLogger->info("Angel connected to '%s:%d'",
                      retriever.get_mgmd_host(),
                      retriever.get_mgmd_port());

  const int alloc_retries = 2;
  const int alloc_delay = 3;
  Uint32 nodeid = retriever.allocNodeId(alloc_retries, alloc_delay);
  if (nodeid == 0)
  {
    g_eventLogger->error("Failed to allocate nodeid, error: '%s'",
                         retriever.getErrorString());
    angel_exit(1);
  }
  g_eventLogger->info("Angel allocated nodeid: %u", nodeid);

  ndb_mgm_configuration * config = retriever.getConfig(nodeid);
  NdbAutoPtr<ndb_mgm_configuration> config_autoptr(config);
  if (config == 0)
  {
    g_eventLogger->error("Could not fetch configuration/invalid "
                         "configuration, error: '%s'",
                         retriever.getErrorString());
    angel_exit(1);
  }

  if (!configure(config, nodeid))
  {
    // Failed to configure, error already printed
    angel_exit(1);
  }

  if (daemon)
  {
    // Become a daemon
    char *lockfile = NdbConfig_PidFileName(nodeid);
    char *logfile = NdbConfig_StdoutFileName(nodeid);
    NdbAutoPtr<char> tmp_aptr1(lockfile), tmp_aptr2(logfile);

#ifndef NDB_WIN32
    if (NdbDaemon_Make(lockfile, logfile, 0) == -1)
    {
      ndbout << "Cannot become daemon: " << NdbDaemon_ErrorText << endl;
      angel_exit(1);
    }
#endif
  }

  // Counter for consecutive failed startups
  Uint32 failed_startups_counter = 0;
  while (true)
  {

    // Create pipe where ndbd process will report extra shutdown status
    int fds[2];
    if (pipe(fds))
    {
      g_eventLogger->error("Failed to create pipe, errno: %d (%s)",
                           errno, strerror(errno));
      angel_exit(1);
    }

    FILE *child_info_r;
    if (!(child_info_r = fdopen(fds[0], "r")))
    {
      g_eventLogger->error("Failed to open stream for pipe, errno: %d (%s)",
                           errno, strerror(errno));
      angel_exit(1);
    }

    // Build the args used to start ndbd by appending
    // the arguments that may have changed at the end
    // of original argument list
    BaseString args(original_args);

    // Pass fd number of the pipe which ndbd should use
    // for sending extra status to angel
    args.appfmt(" --report-fd=%d", fds[1]);

    // The nodeid which has been allocated by angel
    args.appfmt(" --allocated-nodeid=%d", nodeid);

    args.appfmt(" --initial=%d", initial);
    args.appfmt(" --nostart=%d", no_start);

    pid_t child = spawn_process(my_progname, args);
    if (child == -1)
      angel_exit(1);

    /**
     * Parent
     */
    g_eventLogger->info("Angel pid: %d started child: %d",
                        getpid(), child);

    ignore_signals();

    int status=0, error_exit=0;
    while (waitpid(child, &status, 0) != child);

    g_eventLogger->debug("Angel got child %d", child);

    // Close the write end of pipe
    close(fds[1]);

    // Read info from the child's pipe
    char buf[128];
    Uint32 child_error = 0, child_signal = 0, child_sphase = 0;
    while (fgets(buf, sizeof (buf), child_info_r))
    {
      int value;
      if (sscanf(buf, "error=%d\n", &value) == 1)
        child_error = value;
      else if (sscanf(buf, "signal=%d\n", &value) == 1)
        child_signal = value;
      else if (sscanf(buf, "sphase=%d\n", &value) == 1)
        child_sphase = value;
      else if (strcmp(buf, "\n") != 0)
        fprintf(stderr, "unknown info from child: '%s'\n", buf);
    }
    g_eventLogger->debug("error: %u, signal: %u, sphase: %u",
                         child_error, child_signal, child_sphase);
    // Close read end of pipe in parent
    fclose(child_info_r);

    if (WIFEXITED(status))
    {
      switch (WEXITSTATUS(status)) {
      case NRT_Default:
        g_eventLogger->info("Angel shutting down");
        reportShutdown(config, nodeid, 0, 0, false, false,
                       child_error, child_signal, child_sphase);
        angel_exit(0);
        break;
      case NRT_NoStart_Restart:
        initial = false;
        no_start = true;
        break;
      case NRT_NoStart_InitialStart:
        initial = true;
        no_start = true;
        break;
      case NRT_DoStart_InitialStart:
        initial = true;
        no_start = false;
        break;
      default:
        error_exit=1;
        if (stop_on_error)
        {
          /**
           * Error shutdown && stopOnError()
           */
          reportShutdown(config, nodeid,
                         error_exit, 0, false, false,
                         child_error, child_signal, child_sphase);
          angel_exit(0);
        }
        // Fall-through
      case NRT_DoStart_Restart:
        initial = false;
        no_start = false;
        break;
      }
    } else
    {
      error_exit=1;
      if (WIFSIGNALED(status))
      {
        child_signal = WTERMSIG(status);
      }
      else
      {
        child_signal = 127;
        g_eventLogger->info("Unknown exit reason. Stopped.");
      }
      if (stop_on_error)
      {
        /**
         * Error shutdown && stopOnError()
         */
        reportShutdown(config, nodeid,
                       error_exit, 0, false, false,
                       child_error, child_signal, child_sphase);
        angel_exit(0);
      }
    }

    // Check startup failure
    const Uint32 STARTUP_FAILURE_SPHASE = 6;
    const Uint32 MAX_FAILED_STARTUPS = 3;
    if (child_sphase <= STARTUP_FAILURE_SPHASE)
    {
      if (++failed_startups_counter >= MAX_FAILED_STARTUPS)
      {
        g_eventLogger->alert("Angel detected too many startup failures(%d), "
                             "not restarting again", failed_startups_counter);
        reportShutdown(config, nodeid,
                       error_exit, 0, false, false,
                       child_error, child_signal, child_sphase);
        angel_exit(0);
      }
      g_eventLogger->info("Angel detected startup failure, count: %u",
                          failed_startups_counter);
    }
    else
    {
      // Reset the counter for consecutive failed startups
      failed_startups_counter = 0;
    }

    reportShutdown(config, nodeid,
                   error_exit, 1,
                   no_start,
                   initial,
                   child_error, child_signal, child_sphase);
    g_eventLogger->info("Ndb has terminated (pid %d) restarting", child);
  }

  abort(); // Never reached
}
