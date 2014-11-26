/* Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */


#include <ndb_global.h>
#include <ndb_version.h>

#include "angel.hpp"
#include "ndbd.hpp"

#include <NdbConfig.h>
#include <NdbAutoPtr.hpp>
#include <portlib/ndb_daemon.h>
#include <portlib/NdbSleep.h>
#include <portlib/NdbDir.hpp>

#include <ConfigRetriever.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

static void
angel_exit(int code)
{
  ndb_daemon_exit(code);
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
  EventReport *rep= CAST_PTR(EventReport, &theData[0]);
  rep->eventType = 0; /* Ensure it's initialised */

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
#ifdef _WIN32
    SIGTERM,
#else
    SIGQUIT,
#endif
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
#ifdef _WIN32
    SIGINT,
#else
    SIGPIPE,
#endif
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

static const int WNOHANG = 37;

static inline
pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
  /* Only support waitpid(,,WNOHANG) */
  assert(options == WNOHANG);
  assert(stat_loc);

  HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if (handle == NULL)
  {
    g_eventLogger->error("waitpid: Could not open handle for pid %d, "
                         "error: %d", pid, GetLastError());
    return -1;
  }

  DWORD exit_code;
  if (!GetExitCodeProcess(handle, &exit_code))
  {
    g_eventLogger->error("waitpid: GetExitCodeProcess failed, pid: %d, "
                         "error: %d", pid, GetLastError());
    CloseHandle(handle);
    return -1;
  }
  CloseHandle(handle);

  if (exit_code == STILL_ACTIVE)
  {
    /* Still alive */
    return 0;
  }

  *stat_loc = exit_code;

  return pid;
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

static int
kill(pid_t pid, int sig)
{
  int retry_open_event = 10;

  char shutdown_event_name[32];
  _snprintf(shutdown_event_name, sizeof(shutdown_event_name),
            "ndbd_shutdown_%d", pid);

  /* Open the event to signal */
  HANDLE shutdown_event;
  while ((shutdown_event =
          OpenEvent(EVENT_MODIFY_STATE, FALSE, shutdown_event_name)) == NULL)
  {
     /*
      Check if the process is alive, otherwise there is really
      no sense to retry the open of the event
     */
    DWORD exit_code;
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                                  FALSE, pid);
    if (!process)
    {
      /* Already died */
      return -1;
    }

    if (!GetExitCodeProcess(process,&exit_code))
    {
      g_eventLogger->error("GetExitCodeProcess failed, pid: %d, error: %d",
                           pid, GetLastError());
      CloseHandle(process);
      return -1;
    }
    CloseHandle(process);

    if (exit_code != STILL_ACTIVE)
    {
      /* Already died */
      return -1;
    }

    if (retry_open_event--)
      Sleep(100);
    else
    {
      g_eventLogger->error("Failed to open shutdown_event '%s', error: %d",
                            shutdown_event_name, GetLastError());
      return -1;
    }
  }

  if (SetEvent(shutdown_event) == 0)
  {
    g_eventLogger->error("Failed to signal shutdown_event '%s', error: %d",
                         shutdown_event_name, GetLastError());
  }
  CloseHandle(shutdown_event);
  return pid;
}
#endif

#define JAM_FILE_ID 333


extern int real_main(int, char**);


static
char** create_argv(const Vector<BaseString>& args)
{
  char **argv = (char **)malloc(sizeof(char*) * (args.size() + 1));
  if(argv == NULL)
    return NULL;

  for(unsigned i = 0; i < args.size(); i++)
    argv[i] = strdup(args[i].c_str());
  argv[args.size()] = NULL;
  return argv;
}


static
void free_argv(char** argv)
{
  char** argp = argv;
  while(*argp)
  {
    free((void*)*argp);
    argp++;
  }
  free((void*)argv);
}


static pid_t
spawn_process(const char* progname, const Vector<BaseString>& args)
{
#ifdef _WIN32
  // Get full path name of this executeble
  char path[MAX_PATH];
  DWORD len = GetModuleFileName(NULL, path, sizeof(path));
  if (len == 0 || len == sizeof(path))
  {
    g_eventLogger->warning("spawn_process: Could not extract full path, "
                           "len: %u, error: %u\n",
                           len, GetLastError());
    // Fall through and try with progname as it was supplied
  }
  else
  {
    progname = path;
  }
#endif

  char** argv = create_argv(args);
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
    // Print the _spawnv arguments to aid debugging
    g_eventLogger->error(" progname: '%s'", progname);
    char** argp = argv;
    while(*argp)
      g_eventLogger->error("argv: '%s'", *argp++);

    free_argv(argv);
    return -1;
  }
  free_argv(argv);

  // Convert the handle returned from spawnv_ to a pid
  DWORD pid = GetProcessId((HANDLE)spawn_handle);
  if (pid == 0)
  {
    g_eventLogger->error("spawn_process: Failed to convert handle %d "
                         "to pid, error: %d", spawn_handle, GetLastError());
    CloseHandle((HANDLE)spawn_handle);
    return -1;
  }
  CloseHandle((HANDLE)spawn_handle);
  return pid;
#else
  pid_t pid = fork();
  if (pid == -1)
  {
    g_eventLogger->error("Failed to fork, errno: %d", errno);
    free_argv(argv);
    return -1;
  }

  if (pid)
  {
    free_argv(argv);
    // Parent
    return pid;
  }

  // Count number of arguments
  int argc = 0;
  while(argv[argc])
    argc++;

  // Calling 'main' to start program from beginning
  // without loading (possibly new version) from disk
  (void)real_main(argc, argv);
  assert(false); // main should never return
  exit(1);
  return -1; // Never reached
#endif
}

/*
  retry failed spawn after sleep until fork suceeds or
  max number of retries occurs
*/

static pid_t
retry_spawn_process(const char* progname, const Vector<BaseString>& args)
{
  const unsigned max_retries = 10;
  unsigned retry_counter = 0;
  while(true)
  {
    pid_t pid = spawn_process(progname, args);
    if (pid == -1)
    {
      if (retry_counter++ == max_retries)
      {
        g_eventLogger->error("Angel failed to spawn %d times, giving up",
                             retry_counter);
        angel_exit(1);
      }

      g_eventLogger->warning("Angel failed to spawn, sleep and retry");

      NdbSleep_SecSleep(1);
      continue;
    }
    return pid;
  }
}

static Uint32 stop_on_error;
static Uint32 config_max_start_fail_retries;
static Uint32 config_restart_delay_secs; 


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
  g_eventLogger->debug("Using configuration with generation %u", generation);

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

  if (iter.get(CFG_DB_MAX_START_FAIL, &config_max_start_fail_retries))
  {
    /* Old Management node, use default value */
    config_max_start_fail_retries = 3;
  }

  if (iter.get(CFG_DB_START_FAIL_DELAY_SECS, &config_restart_delay_secs))
  {
    /* Old Management node, use default value */
    config_restart_delay_secs = 0;
  }
  
  const char * datadir;
  if (iter.get(CFG_NODE_DATADIR, &datadir))
  {
    g_eventLogger->error("Invalid configuration fetched, could not "
                         "find DataDir");
    return false;
  }
  g_eventLogger->debug("Using DataDir: %s", datadir);

  NdbConfig_SetPath(datadir);

  if (NdbDir::chdir(NdbConfig_get_path(NULL)) != 0)
  {
    g_eventLogger->warning("Cannot change directory to '%s', error: %d",
                           NdbConfig_get_path(NULL), errno);
    // Ignore error
  }

  return true;
}

bool stop_child = false;

void
angel_run(const char* progname,
          const Vector<BaseString>& original_args,
          const char* connect_str,
          int force_nodeid,
          const char* bind_address,
          bool initial,
          bool no_start,
          bool daemon,
          int connnect_retries,
          int connect_delay)
{
  ConfigRetriever retriever(connect_str,
                            force_nodeid,
                            NDB_VERSION,
                            NDB_MGM_NODE_TYPE_NDB,
                            bind_address);
  if (retriever.hasError())
  {
    g_eventLogger->error("Could not initialize connection to management "
                         "server, error: '%s'", retriever.getErrorString());
    angel_exit(1);
  }

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

  const int alloc_retries = 10;
  const int alloc_delay = 3;
  const Uint32 nodeid = retriever.allocNodeId(alloc_retries, alloc_delay);
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

    if (ndb_daemonize(lockfile, logfile) != 0)
    {
      g_eventLogger->error("Couldn't start as daemon, error: '%s'",
                           ndb_daemon_error);
      angel_exit(1);
    }
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
    BaseString one_arg;
    Vector<BaseString> args;
    args = original_args;

    // Pass fd number of the pipe which ndbd should use
    // for sending extra status to angel
    one_arg.assfmt("--report-fd=%d", fds[1]);
    args.push_back(one_arg);

    // The nodeid which has been allocated by angel
    one_arg.assfmt("--allocated-nodeid=%d", nodeid);
    args.push_back(one_arg);

    one_arg.assfmt("--initial=%d", initial);
    args.push_back(one_arg);

    one_arg.assfmt("--nostart=%d", no_start);
    args.push_back(one_arg);

    pid_t child = retry_spawn_process(progname, args);
    if (child <= 0)
    {
      // safety, retry_spawn_process returns valid child or give up
      g_eventLogger->error("retry_spawn_process, child: %d", child);
      angel_exit(1);
    }

    /**
     * Parent
     */
    g_eventLogger->info("Angel pid: %d started child: %d",
                        getpid(), child);

    ignore_signals();

    int status=0, error_exit=0;
    while(true)
    {
      pid_t ret_pid = waitpid(child, &status, WNOHANG);
      if (ret_pid == child)
      {
        g_eventLogger->debug("Angel got child %d", child);
        break;
      }
      if (ret_pid > 0)
      {
        g_eventLogger->warning("Angel got unexpected pid %d "
                               "when waiting for %d",
                               ret_pid, child);
      }

      if (stop_child)
      {
        g_eventLogger->info("Angel shutting down ndbd with pid %d", child);
        kill(child, SIGINT);
       }
      NdbSleep_MilliSleep(100);
    }

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
    Uint32 restart_delay_secs = 0;
    if (error_exit && // Only check startup failure if ndbd exited uncontrolled
        child_sphase <= STARTUP_FAILURE_SPHASE)
    {
      if (++failed_startups_counter >= config_max_start_fail_retries)
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
      
      restart_delay_secs = config_restart_delay_secs;
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

    g_eventLogger->debug("Angel reconnecting to management server");
    (void)retriever.disconnect();

    if (restart_delay_secs > 0)
    {
      g_eventLogger->info("Delaying Ndb restart for %u seconds.",
                          restart_delay_secs);
      NdbSleep_SecSleep(restart_delay_secs);
    };

    const int verbose = 1;
    if (retriever.do_connect(connnect_retries, connect_delay, verbose) != 0)
    {
      g_eventLogger->error("Could not connect to management server, "
                           "error: '%s'", retriever.getErrorString());
      angel_exit(1);
    }
    g_eventLogger->info("Angel reconnected to '%s:%d'",
                        retriever.get_mgmd_host(),
                        retriever.get_mgmd_port());

    // Tell retriver to allocate the same nodeid again
    retriever.setNodeId(nodeid);

    g_eventLogger->debug("Angel reallocating nodeid %d", nodeid);
    const int alloc_retries = 20;
    const int alloc_delay = 3;
    const Uint32 realloced = retriever.allocNodeId(alloc_retries, alloc_delay);
    if (realloced == 0)
    {
      g_eventLogger->error("Angel failed to allocate nodeid, error: '%s'",
                           retriever.getErrorString());
      angel_exit(1);
    }
    if (realloced != nodeid)
    {
      g_eventLogger->error("Angel failed to reallocate nodeid %d, got %d",
                           nodeid, realloced);
      angel_exit(1);
    }
    g_eventLogger->info("Angel reallocated nodeid: %u", nodeid);

  }

  abort(); // Never reached
}


/*
  Order angel to shutdown it's ndbd
*/
void angel_stop(void)
{
  stop_child = true;
}
