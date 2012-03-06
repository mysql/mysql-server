/*
   Copyright (C) 2003-2007 MySQL AB, 2009, 2010 Sun Microsystems, Inc.
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

#ifndef CPCD_HPP
#define CPCD_HPP

#include <Vector.hpp>
#include <Properties.hpp>
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbCondition.h>
#include <BaseString.hpp>

#ifdef _WIN32
typedef DWORD pid_t;
#endif
const pid_t bad_pid = -1;

inline bool is_bad_pid(pid_t pid)
{
#ifdef _WIN32
  return pid == bad_pid;
#else
  return pid <= 1;
#endif
}

#define CPCD_DEFAULT_PROC_FILE    	"ndb_cpcd.conf"
#define CPCD_DEFAULT_TCP_PORT		1234
#define CPCD_DEFAULT_POLLING_INTERVAL	5 /* seconds */
#ifndef _WIN32
#define CPCD_DEFAULT_WORK_DIR		"/var/run/ndb_cpcd"
#define CPCD_DEFAULT_CONFIG_FILE        "/etc/ndb_cpcd.conf"

#else
#define CPCD_DEFAULT_WORK_DIR		"c:\\ndb_cpcd"
#define CPCD_DEFAULT_CONFIG_FILE        "c:\\ndb_cpcd\\ndb_cpcd.conf"
#endif
enum ProcessStatus {
  STOPPED  = 0,
  STARTING = 1,
  RUNNING  = 2,
  STOPPING = 3
};

enum ProcessType {
  PERMANENT = 0,
  TEMPORARY = 1
};

/**
 *  @brief Error codes for CPCD requests
 */
enum RequestStatusCode {
  OK = 0,            ///< Everything OK
  Error = 1,	     ///< Generic error
  AlreadyExists = 2, ///< Entry already exists in list
  NotExists = 3,     ///< Entry does not exist in list
  AlreadyStopped = 4
};

/**
 *  @class CPCD
 *  @brief Manages processes, letting them be controlled with a TCP connection.
 *
 *  The class implementing the Cluster Process Control Daemon
 */
class CPCD {
public:
  /** @brief Describes the status of a client request */
  class RequestStatus {
  public:
    /** @brief Constructs an empty RequestStatus */
    RequestStatus() { m_status = OK; m_errorstring[0] = '\0'; };

    /** @brief Sets an errorcode and a printable message */
    void err(enum RequestStatusCode, const char *);

    /** @brief Returns the error message */
    char *getErrMsg() { return m_errorstring; };

    /** @brief Returns the error code */
    enum RequestStatusCode getStatus() { return m_status; };
  private:
    enum RequestStatusCode m_status;
    char m_errorstring[256];
  };
  /**
   *  @brief Manages a process
   */
  class Process {
    pid_t m_pid;
#ifdef _WIN32
    HANDLE m_job;
#endif
  public:
    /** 
     * @brief Constructs and empty Process
     */
    Process(const Properties & props, class CPCD *cpcd);
    /**
     *  @brief Monitors the process
     *
     *  The process is started or stopped as needed.
     */
    void monitor();

    /**
     *  @brief Checks if the process is running or not
     *
     *  @return 
     *          - true if the process is running,
     *          - false if the process is not running
     */
    bool isRunning();

    /** @brief Starts the process */
    int start();

    /** @brief Stops the process */
    void stop();      

    /** 
     *  @brief Reads the pid from stable storage
     *
     *  @return The pid number
     */
    int readPid();

    /** 
     *  @brief Writes the pid from stable storage
     *
     *  @return 
     *          - 0 if successful
                - -1 and sets errno if an error occured
     */
    int writePid(int pid);

    /**
     *  @brief Prints a textual description of the process on a file
     */
    void print(FILE *);

    /** Id number of the Process.
     *
     *  @note This is not the same as a pid. This number is used in the
     *        protocol, and will not be changed if a processes is restarted.
     */
    int m_id;

    /** @brief The name shown to the user */
    BaseString m_name;  

    /** @brief Used to group a number of processes */
    BaseString m_group;

    /** @brief Environment variables
     *
     *  Environmentvariables to add for the process.
     *
     *  @note
     *       - The environment cpcd started with is preserved
     *       - There is no way to delete variables
     */
    BaseString m_env;

    /** @brief Path to the binary to run */
    BaseString m_path;

    /** @brief Arguments to the process.
     *
     *  @note 
     *        - This includes argv[0].
     *        - If no argv[0] is given, argv[0] will be set to m_path.
     */
    BaseString m_args;

    /** 
     * @brief Type of process
     *
     *  Either set to "interactive" or "permanent".
     */
    BaseString m_type;
    ProcessType m_processType;
    
    /** 
     *  @brief Working directory
     *
     * Working directory the process will start in.
     */
    BaseString m_cwd;

    /**
     *  @brief Owner of the process.
     *
     *  @note This will not affect the process' uid or gid;
     *        it is only used for managemental purposes.
     *  @see m_runas
     */
    BaseString m_owner;

    /**
     * @bried Run as
     * @note This affects uid
     * @see m_owner
     */
    BaseString m_runas;

    /**
     * @brief redirection for stdin
     */
    BaseString m_stdin;

    /**
     * @brief redirection for stdout
     */
    BaseString m_stdout;

    /**
     * @brief redirection for stderr
     */
    BaseString m_stderr;

    /** @brief Status of the process */
    enum ProcessStatus m_status;

    /**
     * @brief ulimits for process
     * @desc Format c:unlimited d:0 ...
     */
    BaseString m_ulimit;

    /**
     * @brief shutdown options
     */
    BaseString m_shutdown_options;

  private:
    class CPCD *m_cpcd;
    void do_exec();
  };

  /**
   *  @brief Starts and stops processes as needed
   *
   *  At a specified interval (default 5 seconds) calls the monitor function
   *  of all the processes in the CPCDs list, causing the to start or
   *  stop, depending on the configuration.
   */
  class Monitor {
  public:
    /** Creates a new CPCD::Monitor object, connected to the specified
     *	CPCD.
     *  A new thread will be created, which will poll the processes of
     *  the CPCD at the specifed interval.
     */
    Monitor(CPCD *cpcd, int poll = CPCD_DEFAULT_POLLING_INTERVAL);

    /** Stops the monitor, but does not stop the processes */
    ~Monitor();

    /** Runs the monitor thread. */
    void run();

    /** Signals configuration changes to the monitor thread, causing it to
     *  do the check without waiting for the timeout */
    void signal();
  private:
    class CPCD *m_cpcd;
    struct NdbThread *m_monitorThread;
    bool m_monitorThreadQuitFlag;
    struct NdbCondition *m_changeCondition;
    NdbMutex *m_changeMutex;
    int m_pollingInterval; /* seconds */
  };

  /** @brief Constructs a CPCD object */
  CPCD();

  /** 
   * @brief Destroys a CPCD object, 
   * but does not stop the processes it manages 
   */
  ~CPCD();

  /** Adds a Process to the CPCDs list of managed Processes.
   *
   *  @note The process will not be started until it is explicitly
   *        marked as running with CPCD::startProcess().
   *
   *  @return 
   *          - true if the addition was successful,
   *          - false if not
   *          - RequestStatus will be filled in with a suitable error
   *            if an error occured.
   */
  bool defineProcess(RequestStatus *rs, Process * arg);

  /** Removes a Process from the CPCD.
   *
   *  @note A Process that is running cannot be removed.
   *
   *  @return
   *          - true if the removal was successful,
   *          - false if not
   *          - The RequestStatus will be filled in with a suitable error
   *            if an error occured.
   */
  bool undefineProcess(RequestStatus *rs, int id);

  /** Marks a Process for starting.
   *
   *  @note The fact that a process has started does not mean it will actually
   *        start properly. This command only makes sure the CPCD will
   *        try to start it.
   *
   *  @return 
   *          - true if the marking was successful
   *          - false if not
   *          - RequestStatus will be filled in with a suitable error
   *            if an error occured.
   */
  bool startProcess(RequestStatus *rs, int id);

  /** Marks a Process for stopping.
   *
   *  @return 
   *          - true if the marking was successful
   *          - false if not
   *          - The RequestStatus will be filled in with a suitable error
   *            if an error occured.
   */
  bool stopProcess(RequestStatus *rs, int id);
  
  /** Generates a list of processes, and sends them to the CPCD client */
  bool listProcesses(RequestStatus *rs, MutexVector<const char *> &);

  /** Set to true while the CPCD is reading the configuration file */
  bool loadingProcessList;

  /** Saves the list of Processes and their status to the configuration file.
   *  Called whenever the configuration is changed.
   */
  bool saveProcessList();

  /** Loads the list of Processes and their status from the configuration
   *  file.
   *  @note This function should only be called when the CPCD is starting,
   *        calling it at other times will cause unspecified behaviour.
   */
  bool loadProcessList();

  /** Returns the list of processes */
  MutexVector<Process *> *getProcessList();

  /** The list of processes. Should not be used directly */
  MutexVector<Process *> m_processes;

private:
  friend class Process;  
  bool notifyChanges();
  int findUniqueId();
  BaseString m_procfile;
  Monitor *m_monitor;
};

#endif
