/* Copyright (c) 2009, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <ndb_global.h>
#include <ndb_version.h>

#include <algorithm>
#include <memory>

#include "angel.hpp"
#include "main.hpp"
#include "ndbd.hpp"

#include <NdbConfig.h>
#include <portlib/NdbSleep.h>
#include <portlib/ndb_daemon.h>
#include <ConfigRetriever.hpp>
#include <NdbAutoPtr.hpp>
#include <portlib/NdbDir.hpp>
#include "util/TlsKeyManager.hpp"

#include <NdbTCP.h>
#include <EventLogger.hpp>
#include "util/ndb_opts.h"

#include "../mgmapi/mgmapi_configuration.hpp"

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#endif

#define JAM_FILE_ID 333

/*
 * process_waiter class provides a check_child_exit_status method that works on
 * both Windows and POSIX systems.
 *
 * On POSIX the child process id (pid) are valid until parent has waited for
 * child or ignoring SIGCHLD.
 *
 * On Windows there is no such guarantee, instead parent should keep an open
 * process handle to child until it has fetched the exit status.
 *
 * Not this class is specifically designed to work against data nodes, the
 * class in not generally suitable for other types of processes.
 */
struct process_waiter {
#ifdef _WIN32
  using native_handle = HANDLE;
#else
  using native_handle = pid_t;
#endif
  process_waiter() : h(invalid) {}
  process_waiter(native_handle h) : h(h) {}
  process_waiter(const process_waiter &) = delete;
  process_waiter(process_waiter &&oth) : h(oth.h) { oth.h = invalid; }
  ~process_waiter() {
    assert(!valid());
    close_handle();
  }
  process_waiter &operator=(const process_waiter &) = delete;
  process_waiter &operator=(process_waiter &&oth) {
    using std::swap;
    swap(h, oth.h);
    return *this;
  }

  bool valid() const { return h != invalid; }
  /*
   * On Windows pid is a 32 bit unsigned int (DWORD).
   * On POSIX a signed int.
   * Using intmax_t that should be able to hold both and still be able to use
   * -1 as invalid pid.
   * This function simplifies printf calls there the format type can be %jd on
   * both Windows and other platforms independent on int size.
   */
  std::intmax_t get_pid_as_intmax() const;
  /*
   * check_child_exit_status returns
   *  1 - if child have stopped, stat_loc will be filled in.
   *  0 - if child is still running, call check_child_exit_status again.
   * -1 - if check_child_exit_status failed and not retryable.
   */
  int check_child_exit_status(int *stat_loc);
  /*
   * kill_child kills the child data node.
   * On POSIX it sends SIGINT signal.
   * On Windows it uses a special event that child data node has setup.
   */
  int kill_child() const;

 private:
  int close_handle();
#ifdef _WIN32
  static constexpr HANDLE invalid{INVALID_HANDLE_VALUE};
#else
  static constexpr pid_t invalid{-1};
#endif
  native_handle h{invalid};
};

inline std::intmax_t process_waiter::get_pid_as_intmax() const {
#ifdef _WIN32
  if (!valid()) return -1;
  return {GetProcessId(h)};
#else
  return h;
#endif
}

int process_waiter::check_child_exit_status(int *stat_loc) {
  assert(valid());
  if (!valid()) return -1;
#ifndef _WIN32
  pid_t p = waitpid(h, stat_loc, WNOHANG);
  if (p == 0) return 0;
  if (p != h) return -1;
#else
  DWORD exit_code;
  if (!GetExitCodeProcess(h, &exit_code)) {
    g_eventLogger->error(
        "waitpid: GetExitCodeProcess failed, pid: %jd, "
        "error: %d",
        get_pid_as_intmax(), GetLastError());
    close_handle();
    return -1;
  }

  if (exit_code == STILL_ACTIVE) {
    /* Still alive */
    return 0;
  }

  *stat_loc = exit_code;
#endif
  // wait successful, pid no longer valid
  close_handle();
  return 1;
}

int process_waiter::kill_child() const {
  assert(valid());
  if (!valid()) return -1;

#ifndef _WIN32
  return ::kill(h, SIGINT);
#else
  char shutdown_event_name[14 + 10 + 1];  // pid is DWORD
  _snprintf(shutdown_event_name, sizeof(shutdown_event_name),
            "ndbd_shutdown_%jd", get_pid_as_intmax());

  /* Open the event to signal */
  HANDLE shutdown_event;
  if ((shutdown_event =
           OpenEvent(EVENT_MODIFY_STATE, false, shutdown_event_name)) == NULL) {
    g_eventLogger->error("Failed to open shutdown_event '%s', error: %d",
                         shutdown_event_name, GetLastError());
    return -1;
  }

  int retval = 0;
  if (SetEvent(shutdown_event) == 0) {
    g_eventLogger->error("Failed to signal shutdown_event '%s', error: %d",
                         shutdown_event_name, GetLastError());
    retval = -1;
  }
  CloseHandle(shutdown_event);
  return retval;
#endif
}

int process_waiter::close_handle() {
  if (!valid()) return -1;
#ifdef _WIN32
  CloseHandle(h);
#endif
  h = invalid;
  return 0;
}

static void angel_exit(int code) { ndb_daemon_exit(code); }

static void reportShutdown(const ndb_mgm_configuration *config, NodeId nodeid,
                           int error_exit, bool restart, bool nostart,
                           bool initial, Uint32 error, Uint32 signum,
                           Uint32 sphase, ssl_ctx_st *tls, int tls_req_level) {
  // Only allow "initial" and "nostart" to be set if "restart" is set
  assert(restart || (!restart && !initial && !nostart));

  Uint32 length, theData[25];
  EventReport *rep = CAST_PTR(EventReport, &theData[0]);
  rep->eventType = 0; /* Ensure it's initialised */

  rep->setNodeId(nodeid);
  if (restart)
    theData[1] = 1 | (nostart ? 2 : 0) | (initial ? 4 : 0);
  else
    theData[1] = 0;

  if (error_exit == 0) {
    rep->setEventType(NDB_LE_NDBStopCompleted);
    theData[2] = signum;
    length = 3;
  } else {
    rep->setEventType(NDB_LE_NDBStopForced);
    theData[2] = signum;
    theData[3] = error;
    theData[4] = sphase;
    theData[5] = 0;  // extra
    length = 6;
  }

  // Log event locally
  g_eventLogger->log(rep->getEventType(), theData, length, rep->getNodeId(), 0);

  // Log event to cluster log
  ndb_mgm_configuration_iterator iter(config, CFG_SECTION_NODE);
  for (iter.first(); iter.valid(); iter.next()) {
    Uint32 type;
    if (iter.get(CFG_TYPE_OF_SECTION, &type) || type != NODE_TYPE_MGM) continue;

    Uint32 port;
    if (iter.get(CFG_MGM_PORT, &port)) continue;

    const char *hostname;
    if (iter.get(CFG_NODE_HOST, &hostname)) continue;

    BaseString connect_str;
    connect_str.assfmt("%s %d", hostname, port);

    NdbMgmHandle h = ndb_mgm_create_handle();
    if (h == 0) {
      g_eventLogger->warning(
          "Unable to report shutdown reason "
          "to '%s'(failed to create mgm handle)",
          connect_str.c_str());
      continue;
    }

    ndb_mgm_set_ssl_ctx(h, tls);
    if (ndb_mgm_set_connectstring(h, connect_str.c_str()) ||
        ndb_mgm_connect_tls(h, 1, 0, 0, tls_req_level) ||
        ndb_mgm_report_event(h, theData, length)) {
      g_eventLogger->warning(
          "Unable to report shutdown reason "
          "to '%s'(error: %s - %s)",
          connect_str.c_str(), ndb_mgm_get_latest_error_msg(h),
          ndb_mgm_get_latest_error_desc(h));
    }

    ndb_mgm_destroy_handle(&h);
  }
}

static void ignore_signals(void) {
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

  for (size_t i = 0; i < sizeof(ignore_list) / sizeof(ignore_list[0]); i++)
    signal(ignore_list[i], SIG_IGN);
}

#ifdef _WIN32
static inline int pipe(int pipefd[2]) {
  const unsigned int buffer_size = 4096;
  const int flags = 0;
  return _pipe(pipefd, buffer_size, flags);
}

static inline bool WIFEXITED(int status) { return true; }

static inline int WEXITSTATUS(int status) { return status; }

static inline bool WIFSIGNALED(int status) { return false; }

static inline int WTERMSIG(int status) { return 0; }

#endif

extern int real_main(int, char **);

static char **create_argv(const Vector<BaseString> &args) {
  char **argv = (char **)malloc(sizeof(char *) * (args.size() + 1));
  if (argv == NULL) return NULL;

  for (unsigned i = 0; i < args.size(); i++) argv[i] = strdup(args[i].c_str());
  argv[args.size()] = NULL;
  return argv;
}

static void free_argv(char **argv) {
  char **argp = argv;
  while (*argp) {
    free((void *)*argp);
    argp++;
  }
  free((void *)argv);
}

static process_waiter spawn_process(const char *progname [[maybe_unused]],
                                    const Vector<BaseString> &args) {
#ifdef _WIN32
  // Get full path name of this executeble
  char path[MAX_PATH];
  DWORD len = GetModuleFileName(nullptr, path, sizeof(path));
  if (len == 0 || len == sizeof(path)) {
    g_eventLogger->warning(
        "spawn_process: Could not extract full path, "
        "len: %u, error: %u\n",
        len, GetLastError());
    // Fall through and try with progname as it was supplied
  } else {
    progname = path;
  }
#endif

  char **argv = create_argv(args);
  if (!argv) {
    g_eventLogger->error("spawn_process: Failed to create argv, errno: %d",
                         errno);
    return {};
  }

#ifdef _WIN32

  intptr_t spawn_handle = _spawnv(P_NOWAIT, progname, argv);
  if (spawn_handle == -1) {
    g_eventLogger->error("spawn_process: Failed to spawn process, errno: %d",
                         errno);
    // Print the _spawnv arguments to aid debugging
    g_eventLogger->error(" progname: '%s'", progname);
    char **argp = argv;
    while (*argp) g_eventLogger->error("argv: '%s'", *argp++);

    free_argv(argv);
    return {};
  }
  free_argv(argv);
  return {(HANDLE)spawn_handle};
#else
  pid_t pid = fork();
  if (pid == -1) {
    g_eventLogger->error("Failed to fork, errno: %d", errno);
    free_argv(argv);
    return {};
  }

  if (pid) {
    free_argv(argv);
    // Parent
    return {pid};
  }

  // Child path (pid == 0)

  // Count number of arguments
  int argc = 0;
  while (argv[argc]) argc++;

  // Calling 'main' to start program from beginning
  // without loading (possibly new version) from disk
  (void)real_main(argc, argv);
  assert(false);  // main should never return
  exit(1);
  return {};  // Never reached
#endif
}

/*
  retry failed spawn after sleep until fork succeeds or
  max number of retries occurs
*/

static process_waiter retry_spawn_process(const char *progname,
                                          const Vector<BaseString> &args) {
  const unsigned max_retries = 10;
  unsigned retry_counter = 0;
  while (true) {
    process_waiter proc = spawn_process(progname, args);
    if (!proc.valid()) {
      if (retry_counter++ == max_retries) {
        g_eventLogger->error("Angel failed to spawn %d times, giving up",
                             retry_counter);
        angel_exit(1);
      }

      g_eventLogger->warning("Angel failed to spawn, sleep and retry");

      NdbSleep_SecSleep(1);
      continue;
    }
    return proc;
  }
}

static Uint32 stop_on_error;
static Uint32 config_max_start_fail_retries;
static Uint32 config_restart_delay_secs;

/*
  Extract the config parameters that concerns angel
*/

static bool configure(const ndb_mgm_configuration *conf, NodeId nodeid) {
  Uint32 generation = 0;
  ndb_mgm_configuration_iterator sys_iter(conf, CFG_SECTION_SYSTEM);
  if (sys_iter.get(CFG_SYS_CONFIG_GENERATION, &generation)) {
    g_eventLogger->warning(
        "Configuration didn't contain generation "
        "(likely old ndb_mgmd");
  }
  g_eventLogger->debug("Using configuration with generation %u", generation);

  ndb_mgm_configuration_iterator iter(conf, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, nodeid)) {
    g_eventLogger->error(
        "Invalid configuration fetched, could not "
        "find own node id %d",
        nodeid);
    return false;
  }

  if (iter.get(CFG_DB_STOP_ON_ERROR, &stop_on_error)) {
    g_eventLogger->error(
        "Invalid configuration fetched, could not "
        "find StopOnError");
    return false;
  }
  g_eventLogger->debug("Using StopOnError: %u", stop_on_error);

  if (iter.get(CFG_DB_MAX_START_FAIL, &config_max_start_fail_retries)) {
    /* Old Management node, use default value */
    config_max_start_fail_retries = 3;
  }

  if (iter.get(CFG_DB_START_FAIL_DELAY_SECS, &config_restart_delay_secs)) {
    /* Old Management node, use default value */
    config_restart_delay_secs = 0;
  }

  const char *datadir;
  if (iter.get(CFG_NODE_DATADIR, &datadir)) {
    g_eventLogger->error(
        "Invalid configuration fetched, could not "
        "find DataDir");
    return false;
  }
  g_eventLogger->debug("Using DataDir: %s", datadir);

  NdbConfig_SetPath(datadir);

  if (NdbDir::chdir(NdbConfig_get_path(NULL)) != 0) {
    g_eventLogger->warning("Cannot change directory to '%s', error: %d",
                           NdbConfig_get_path(NULL), errno);
    // Ignore error
  }

  return true;
}

bool stop_child = false;

void angel_run(const char *progname, const Vector<BaseString> &original_args,
               const char *connect_str, int force_nodeid,
               const char *bind_address, bool initial, bool no_start,
               bool daemon, int connnect_retries, int connect_delay,
               const char *tls_search_path, int mgm_tls_level) {
  ConfigRetriever retriever(connect_str, force_nodeid, NDB_VERSION,
                            NDB_MGM_NODE_TYPE_NDB, bind_address);
  if (retriever.hasError()) {
    g_eventLogger->error(
        "Could not initialize connection to management "
        "server, error: '%s'",
        retriever.getErrorString());
    angel_exit(1);
  }

  retriever.init_mgm_tls(tls_search_path, Node::Type::DB, mgm_tls_level);

  const int verbose = 1;
  if (retriever.do_connect(connnect_retries, connect_delay, verbose) != 0) {
    g_eventLogger->error(
        "Could not connect to management server, "
        "error: '%s'",
        retriever.getErrorString());
    angel_exit(1);
  }

  char buf[512];
  char *sockaddr_string = Ndb_combine_address_port(
      buf, sizeof(buf), retriever.get_mgmd_host(), retriever.get_mgmd_port());
  g_eventLogger->info("Angel connected to '%s'", sockaddr_string);

  /**
   * Gives information to users before allocating nodeid if invalid
   * configuration is fetched or configuration is not yet committed.
   */
  if (!retriever.getConfig(retriever.get_mgmHandle())) {
    g_eventLogger->info(
        "Could not fetch configuration/invalid "
        "configuration, message: '%s'",
        retriever.getErrorString());
  }

  const int alloc_retries = 10;
  const int alloc_delay = 3;
  const Uint32 nodeid = retriever.allocNodeId(alloc_retries, alloc_delay);
  if (nodeid == 0) {
    g_eventLogger->error("Failed to allocate nodeid, error: '%s'",
                         retriever.getErrorString());
    angel_exit(1);
  }
  g_eventLogger->info("Angel allocated nodeid: %u", nodeid);

  const ndb_mgm::config_ptr config(retriever.getConfig(nodeid));
  if (!config) {
    g_eventLogger->error(
        "Could not fetch configuration/invalid "
        "configuration, error: '%s'",
        retriever.getErrorString());
    angel_exit(1);
  }

  if (!configure(config.get(), nodeid)) {
    // Failed to configure, error already printed
    angel_exit(1);
  }

  if (daemon) {
    // Become a daemon
    char *lockfile = NdbConfig_PidFileName(nodeid);
    char *logfile = NdbConfig_StdoutFileName(nodeid);
    NdbAutoPtr<char> tmp_aptr1(lockfile), tmp_aptr2(logfile);

    if (ndb_daemonize(lockfile, logfile) != 0) {
      g_eventLogger->error("Couldn't start as daemon, error: '%s'",
                           ndb_daemon_error);
      angel_exit(1);
    }
  }

  const bool have_password_option =
      g_filesystem_password_state.have_password_option();

  // Counter for consecutive failed startups
  Uint32 failed_startups_counter = 0;
  while (true) {
    // Create pipe where ndbd process will report extra shutdown status
    int fds[2];
    if (pipe(fds)) {
      g_eventLogger->error("Failed to create pipe, errno: %d (%s)", errno,
                           strerror(errno));
      angel_exit(1);
    }

    FILE *child_info_r;
    if (!(child_info_r = fdopen(fds[0], "r"))) {
      g_eventLogger->error("Failed to open stream for pipe, errno: %d (%s)",
                           errno, strerror(errno));
      angel_exit(1);
    }

    int fs_password_fds[2];
    if (have_password_option) {
      if (pipe(fs_password_fds)) {
        g_eventLogger->error("Failed to create pipe, errno: %d (%s)", errno,
                             strerror(errno));
        angel_exit(1);
      }
      // Angel stdin is closed and attached to pipe, not strictly wanted,
      // but to make it work on windows and spawn we need to reset the
      // angels stdin since child inherits it as is.
      dup2(fs_password_fds[0], 0);
      close(fs_password_fds[0]);
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

    one_arg.assfmt("--angel-pid=%d", getpid());
    args.push_back(one_arg);

    if (have_password_option) {
      /**
       * Removes all password opts and adds a new one
       * (filesystem-password-from-stdin) to pass password to child always
       * in same way.
       * --skip-filesystem-password-from-stdin is not strictly needed,
       * it is used just to clarify the way we are passing the password.
       * Any future new filesystem-password option should also be skipped here.
       */
      args.push_back("--skip-filesystem-password");
      args.push_back("--skip-filesystem-password-from-stdin");
      args.push_back("--filesystem-password-from-stdin");
    }

    /**
     * We need to set g_is_forked=true temporarily in order to make the
     * forked child to inherit it. After fork we set g_is_forked to false
     * again in parent (angel).
     */

    g_is_forked = true;
    process_waiter child = retry_spawn_process(progname, args);
    g_is_forked = false;
    if (!child.valid()) {
      // safety, retry_spawn_process returns valid child or give up
      g_eventLogger->error("retry_spawn_process");
      angel_exit(1);
    }
    const std::intmax_t child_pid = child.get_pid_as_intmax();

    /**
     * Parent
     */
    g_eventLogger->info("Angel pid: %d started child: %jd", getpid(),
                        child_pid);

    ignore_signals();

    if (have_password_option) {
      const char *nul = IF_WIN("nul:", "/dev/null");
      int fd = open(nul, O_RDONLY, 0);
      if (fd == -1) {
        g_eventLogger->error("Failed to open %s errno: %d (%s)", nul, errno,
                             strerror(errno));
        angel_exit(1);
      }

      // angel stdin reset to /dev/null
      dup2(fd, 0);
      close(fd);

      const Uint32 password_length =
          g_filesystem_password_state.get_password_length();
      const char *state_password = g_filesystem_password_state.get_password();
      char *password = new char[password_length + 1]();
      memcpy(password, state_password, password_length);
      password[password_length] = '\n';
      if (write(fs_password_fds[1], password, password_length + 1) == -1) {
        g_eventLogger->error("Failed to write to pipe, errno: %d (%s)", errno,
                             strerror(errno));
        angel_exit(1);
      }
    }

    int status = 0, error_exit = 0;
    while (true) {
      int retval = child.check_child_exit_status(&status);
      if (retval == 1) {
        g_eventLogger->debug("Angel got child %jd", child_pid);
        break;
      }
      if (retval == -1) {
        g_eventLogger->warning("Angel failed waiting for child with pid %jd",
                               child_pid);
        break;
      }

      if (stop_child) {
        g_eventLogger->info("Angel shutting down ndbd with pid %jd", child_pid);
        child.kill_child();
      }
      NdbSleep_MilliSleep(100);
    }

    // Close the write end of pipes
    close(fds[1]);
    close(fs_password_fds[1]);

    // Read info from the child's pipe
    char buf[128];
    Uint32 child_error = 0, child_signal = 0, child_sphase = 0;
    while (fgets(buf, sizeof(buf), child_info_r)) {
      int value;
      if (sscanf(buf, "error=%d\n", &value) == 1)
        child_error = value;
      else if (sscanf(buf, "signal=%d\n", &value) == 1)
        child_signal = value;
      else if (sscanf(buf, "sphase=%d\n", &value) == 1)
        child_sphase = value;
      else if (strcmp(buf, "\n") != 0)
        g_eventLogger->info("unknown info from child: '%s'", buf);
    }
    g_eventLogger->debug("error: %u, signal: %u, sphase: %u", child_error,
                         child_signal, child_sphase);
    // Close read end of pipe in parent
    fclose(child_info_r);

    if (WIFEXITED(status)) {
      switch (WEXITSTATUS(status)) {
        case NRT_Default:
          g_eventLogger->info("Angel shutting down");
          reportShutdown(config.get(), nodeid, 0, 0, false, false, child_error,
                         child_signal, child_sphase, retriever.ssl_ctx(),
                         mgm_tls_level);
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
          error_exit = 1;
          if (stop_on_error) {
            /**
             * Error shutdown && stopOnError()
             */
            reportShutdown(config.get(), nodeid, error_exit, 0, false, false,
                           child_error, child_signal, child_sphase,
                           retriever.ssl_ctx(), mgm_tls_level);
            angel_exit(0);
          }
          [[fallthrough]];
        case NRT_DoStart_Restart:
          initial = false;
          no_start = false;
          break;
      }
    } else {
      error_exit = 1;
      if (WIFSIGNALED(status)) {
        child_signal = WTERMSIG(status);
        g_eventLogger->info("Child process terminated by signal %u",
                            child_signal);
      } else {
        child_signal = 127;
        g_eventLogger->info("Unknown exit reason. Stopped.");
      }
      if (stop_on_error) {
        /**
         * Error shutdown && stopOnError()
         */
        reportShutdown(config.get(), nodeid, error_exit, 0, false, false,
                       child_error, child_signal, child_sphase,
                       retriever.ssl_ctx(), mgm_tls_level);
        angel_exit(0);
      } else {
        // StopOnError = false, restart with safe defaults
        initial = false;   // to prevent data loss on restart
        no_start = false;  // to ensure ndbmtd comes up
        g_eventLogger->info("Angel restarting child process");
      }
    }

    // Check startup failure
    const Uint32 STARTUP_FAILURE_SPHASE = 6;
    Uint32 restart_delay_secs = 0;
    if (error_exit &&  // Only check startup failure if ndbd exited uncontrolled
        child_sphase > 0 &&  // Received valid startphase info from child
        child_sphase <= STARTUP_FAILURE_SPHASE) {
      if (++failed_startups_counter >= config_max_start_fail_retries) {
        g_eventLogger->alert(
            "Angel detected too many startup failures(%d), "
            "not restarting again",
            failed_startups_counter);
        reportShutdown(config.get(), nodeid, error_exit, 0, false, false,
                       child_error, child_signal, child_sphase,
                       retriever.ssl_ctx(), mgm_tls_level);
        angel_exit(0);
      }
      g_eventLogger->info("Angel detected startup failure, count: %u",
                          failed_startups_counter);

      restart_delay_secs = config_restart_delay_secs;
    } else {
      // Reset the counter for consecutive failed startups
      failed_startups_counter = 0;
    }

    reportShutdown(config.get(), nodeid, error_exit, 1, no_start, initial,
                   child_error, child_signal, child_sphase, retriever.ssl_ctx(),
                   mgm_tls_level);
    g_eventLogger->info(
        "Child has terminated (pid %jd). "
        "Angel restarting child process",
        child_pid);

    g_eventLogger->debug("Angel reconnecting to management server");
    (void)retriever.disconnect();

    if (restart_delay_secs > 0) {
      g_eventLogger->info("Delaying Ndb restart for %u seconds.",
                          restart_delay_secs);
      NdbSleep_SecSleep(restart_delay_secs);
    };

    const int verbose = 1;
    if (retriever.do_connect(connnect_retries, connect_delay, verbose) != 0) {
      g_eventLogger->error(
          "Could not connect to management server, "
          "error: '%s'",
          retriever.getErrorString());
      angel_exit(1);
    }
    g_eventLogger->info("Angel reconnected to '%s:%d'",
                        retriever.get_mgmd_host(), retriever.get_mgmd_port());

    // Tell retriver to allocate the same nodeid again
    retriever.setNodeId(nodeid);

    g_eventLogger->debug("Angel reallocating nodeid %d", nodeid);
    const int alloc_retries = 20;
    const int alloc_delay = 3;
    const Uint32 realloced = retriever.allocNodeId(alloc_retries, alloc_delay);
    if (realloced == 0) {
      g_eventLogger->error("Angel failed to allocate nodeid, error: '%s'",
                           retriever.getErrorString());
      angel_exit(1);
    }
    if (realloced != nodeid) {
      g_eventLogger->error("Angel failed to reallocate nodeid %d, got %d",
                           nodeid, realloced);
      angel_exit(1);
    }
    g_eventLogger->info("Angel reallocated nodeid: %u", nodeid);
  }

  abort();  // Never reached
}

/*
  Order angel to shutdown it's ndbd
*/
void angel_stop(void) { stop_child = true; }
