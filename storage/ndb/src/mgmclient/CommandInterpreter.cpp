/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <mgmapi.h>
#include <ndbd_exit_codes.h>

#include <util/BaseString.hpp>
#include <util/Vector.hpp>
#include <kernel/BlockNumbers.h>

/** 
 *  @class CommandInterpreter
 *  @brief Reads command line in management client
 *
 *  For command syntax, see the HELP command.
 */ 
class CommandInterpreter {
public:
  /**
   *   Constructor
   *   @param mgmtSrvr: Management server to use when executing commands
   */
  CommandInterpreter(const char *, int verbose);
  ~CommandInterpreter();
  
  /**
   *   Reads one line from the stream, parse the line to find 
   *   a command and then calls a suitable method which executes 
   *   the command.
   *
   *   @return true until quit/bye/exit has been typed
   */
  bool execute(const char *line, int try_reconnect = -1,
               bool interactive = true, int *error = NULL);

private:
  void printError();
  bool execute_impl(const char *line, bool interactive);

  /**
   *   Analyse the command line, after the first token.
   *
   *   @param  processId:           DB process id to send command to or -1 if
   *                                command will be sent to all DB processes.
   *   @param  allAfterFirstToken:  What the client gave after the 
   *                                first token on the command line
   *   @return: 0 if analyseAfterFirstToken succeeds, otherwise -1 
   */
  int  analyseAfterFirstToken(int processId, char* allAfterFirstTokenCstr);

  int  executeCommand(Vector<BaseString> &command_list,
                      unsigned command_pos,
                      int *node_ids, int no_of_nodes);
  /**
   *   Parse the block specification part of the LOG* commands,
   *   things after LOG*: [BLOCK = {ALL|<blockName>+}]
   *
   *   @param  allAfterLog: What the client gave after the second token 
   *                        (LOG*) on the command line
   *   @param  blocks, OUT: ALL or name of all the blocks
   *   @return: true if correct syntax, otherwise false
   */
  bool parseBlockSpecification(const char* allAfterLog, 
			       Vector<BaseString>& blocks);
  
  /**
   *   A bunch of execute functions: Executes one of the commands
   *
   *   @param  processId:   DB process id to send command to
   *   @param  parameters:  What the client gave after the command name 
   *                        on the command line.
   *   For example if complete input from user is: "1 LOGLEVEL 22" then the
   *   parameters argument is the string with everything after LOGLEVEL, in
   *   this case "22". Each function is responsible to check the parameters
   *   argument.
   */
  int  executeHelp(char* parameters);
  int  executeShow(char* parameters);
  int  executePurge(char* parameters);
  int  executeConnect(char* parameters, bool interactive);
  int  executeShutdown(char* parameters);
  void executeClusterLog(char* parameters);

public:
  int  executeStop(int processId, const char* parameters, bool all);
  int  executeEnterSingleUser(char* parameters);
  int  executeExitSingleUser(char* parameters);
  int  executeStart(int processId, const char* parameters, bool all);
  int  executeRestart(int processId, const char* parameters, bool all);
  int  executeLogLevel(int processId, const char* parameters, bool all);
  int  executeError(int processId, const char* parameters, bool all);
  int  executeLog(int processId, const char* parameters, bool all);
  int  executeTestOn(int processId, const char* parameters, bool all);
  int  executeTestOff(int processId, const char* parameters, bool all);
  int  executeStatus(int processId, const char* parameters, bool all);
  int  executeEventReporting(int processId, const char* parameters, bool all);
  int  executeDumpState(int processId, const char* parameters, bool all);
  int  executeReport(int processId, const char* parameters, bool all);
  int  executeStartBackup(char * parameters, bool interactive);
  int  executeAbortBackup(char * parameters);
  int  executeStop(Vector<BaseString> &command_list, unsigned command_pos,
                   int *node_ids, int no_of_nodes);
  int  executeRestart(Vector<BaseString> &command_list, unsigned command_pos,
                      int *node_ids, int no_of_nodes);
  int  executeStart(Vector<BaseString> &command_list, unsigned command_pos,
                    int *node_ids, int no_of_nodes);
  int executeCreateNodeGroup(char* parameters);
  int executeDropNodeGroup(char* parameters);
public:
  bool connect(bool interactive);
  void disconnect(void);

  /**
   * A execute function definition
   */
public:
  typedef int (CommandInterpreter::* ExecuteFunction)(int processId, 
						       const char * param, 
						       bool all);
  
  struct CommandFunctionPair {
    const char * command;
    ExecuteFunction executeFunction;
  };
private:
  /**
   * 
   */
  int  executeForAll(const char * cmd, 
		     ExecuteFunction fun,
		     const char * param);

  NdbMgmHandle m_mgmsrv;
  NdbMgmHandle m_mgmsrv2;
  const char *m_constr;
  bool m_connected;
  int m_verbose;
  int m_try_reconnect;
  int m_error;
  struct NdbThread* m_event_thread;
  NdbMutex *m_print_mutex;
};

NdbMutex* print_mutex;

/*
 * Facade object for CommandInterpreter
 */

#include "ndb_mgmclient.hpp"

Ndb_mgmclient::Ndb_mgmclient(const char *host,int verbose)
{
  m_cmd= new CommandInterpreter(host,verbose);
}
Ndb_mgmclient::~Ndb_mgmclient()
{
  delete m_cmd;
}
bool Ndb_mgmclient::execute(const char *line, int try_reconnect,
                            bool interactive, int *error)
{
  return m_cmd->execute(line, try_reconnect, interactive, error);
}

/*
 * The CommandInterpreter
 */

#include <mgmapi_debug.h>

#include <util/version.h>
#include <util/NdbAutoPtr.hpp>
#include <util/NdbOut.hpp>

#include <portlib/NdbSleep.h>
#include <portlib/NdbThread.h>

#include <debugger/EventLogger.hpp>
#include <signaldata/SetLogLevelOrd.hpp>

/*****************************************************************************
 * HELP
 *****************************************************************************/
static const char* helpText =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help\n"
"---------------------------------------------------------------------------\n"
"HELP                                   Print help text\n"
"HELP COMMAND                           Print detailed help for COMMAND(e.g. SHOW)\n"
#ifdef VM_TRACE // DEBUG ONLY
"HELP DEBUG                             Help for debug compiled version\n"
#endif
"SHOW                                   Print information about cluster\n"
"CREATE NODEGROUP <id>,<id>...          Add a Nodegroup containing nodes\n"
"DROP NODEGROUP <NG>                    Drop nodegroup with id NG\n"
"START BACKUP [NOWAIT | WAIT STARTED | WAIT COMPLETED]\n"
"START BACKUP [<backup id>] [NOWAIT | WAIT STARTED | WAIT COMPLETED]\n"
"START BACKUP [<backup id>] [SNAPSHOTSTART | SNAPSHOTEND] [NOWAIT | WAIT STARTED | WAIT COMPLETED]\n"
"                                       Start backup (default WAIT COMPLETED,SNAPSHOTEND)\n"
"ABORT BACKUP <backup id>               Abort backup\n"
"SHUTDOWN                               Shutdown all processes in cluster\n"
"CLUSTERLOG ON [<severity>] ...         Enable Cluster logging\n"
"CLUSTERLOG OFF [<severity>] ...        Disable Cluster logging\n"
"CLUSTERLOG TOGGLE [<severity>] ...     Toggle severity filter on/off\n"
"CLUSTERLOG INFO                        Print cluster log information\n"
"<id> START                             Start data node (started with -n)\n"
"<id> RESTART [-n] [-i] [-a] [-f]       Restart data or management server node\n"
"<id> STOP [-a] [-f]                    Stop data or management server node\n"
"ENTER SINGLE USER MODE <id>            Enter single user mode\n"
"EXIT SINGLE USER MODE                  Exit single user mode\n"
"<id> STATUS                            Print status\n"
"<id> CLUSTERLOG {<category>=<level>}+  Set log level for cluster log\n"
"PURGE STALE SESSIONS                   Reset reserved nodeid's in the mgmt server\n"
"CONNECT [<connectstring>]              Connect to management server (reconnect if already connected)\n"
"<id> REPORT <report-type>              Display report for <report-type>\n"
"QUIT                                   Quit management client\n"
;

static const char* helpTextShow =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for SHOW command\n"
"---------------------------------------------------------------------------\n"
"SHOW Print information about cluster\n\n"
"SHOW               Print information about cluster.The status reported is from\n"
"                   the perspective of the data nodes. API and Management Server nodes\n"
"                   are only reported as connected once the data nodes have started.\n"
;

static const char* helpTextHelp =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for HELP command\n"
"---------------------------------------------------------------------------\n"
"HELP List available commands of NDB Cluster Management Client\n\n"
"HELP               List available commands.\n"
;

static const char* helpTextBackup =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for BACKUP command\n"
"---------------------------------------------------------------------------\n"
"BACKUP  A backup is a snapshot of the database at a given time. \n"
"        The backup consists of three main parts:\n\n"
"        Metadata: the names and definitions of all database tables. \n"
"        Table records: the data actually stored in the database tables \n"
"        at the time that the backup was made.\n"
"        Transaction log: a sequential record telling how \n"
"        and when data was stored in the database.\n\n"
"        Backups are stored on each data node in the cluster that \n"
"        participates in the backup.\n\n"
"        The cluster log records backup related events (such as \n"
"        backup started, aborted, finished).\n"
;

static const char* helpTextStartBackup =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for START BACKUP command\n"
"---------------------------------------------------------------------------\n"
"START BACKUP  Start a cluster backup\n\n"
"START BACKUP [<backup id>] [SNAPSHOTSTART | SNAPSHOTEND] [NOWAIT | WAIT STARTED | WAIT COMPLETED]\n"
"                   Start a backup for the cluster.\n"
"                   Each backup gets an ID number that is reported to the\n"
"                   user. This ID number can help you find the backup on the\n"
"                   file system, or ABORT BACKUP if you wish to cancel a \n"
"                   running backup.\n"
"                   You can also start specified backup using START BACKUP <backup id> \n\n"
"                   <backup id> \n"
"                     Start a specified backup using <backup id> as bakcup ID number.\n" 
"                   SNAPSHOTSTART \n"
"                     Backup snapshot is taken around the time the backup is started.\n" 
"                   SNAPSHOTEND \n"
"                     Backup snapshot is taken around the time the backup is completed.\n" 
"                   NOWAIT \n"
"                     Start a cluster backup and return immediately.\n"
"                     The management client will return control directly\n"
"                     to the user without waiting for the backup\n"
"                     to have started.\n"
"                     The status of the backup is recorded in the Cluster log.\n"
"                   WAIT STARTED\n"
"                     Start a cluster backup and return until the backup has\n"
"                     started. The management client will wait for the backup \n"
"                     to have started before returning control to the user.\n"
"                   WAIT COMPLETED\n"
"                     Start a cluster backup and return until the backup has\n"
"                     completed. The management client will wait for the backup\n"
"                     to complete before returning control to the user.\n"
;

static const char* helpTextAbortBackup =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for ABORT BACKUP command\n"
"---------------------------------------------------------------------------\n"
"ABORT BACKUP  Abort a cluster backup\n\n"
"ABORT BACKUP <backup id>  \n"
"                   Abort a backup that is already in progress.\n"
"                   The backup id can be seen in the cluster log or in the\n"
"                   output of the START BACKUP command.\n"
;

static const char* helpTextShutdown =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for SHUTDOWN command\n"
"---------------------------------------------------------------------------\n"
"SHUTDOWN  Shutdown the cluster\n\n"
"SHUTDOWN           Shutdown the data nodes and management nodes.\n"
"                   MySQL Servers and NDBAPI nodes are currently not \n"
"                   shut down by issuing this command.\n"
;

static const char* helpTextClusterlogOn =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for CLUSTERLOG ON command\n"
"---------------------------------------------------------------------------\n"
"CLUSTERLOG ON  Enable Cluster logging\n\n"
"CLUSTERLOG ON [<severity>] ... \n"
"                   Turn the cluster log on.\n"
"                   It tells management server which severity levels\n"
"                   messages will be logged.\n\n"
"                   <severity> can be any one of the following values:\n"
"                   ALERT, CRITICAL, ERROR, WARNING, INFO, DEBUG.\n"
;

static const char* helpTextClusterlogOff =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for CLUSTERLOG OFF command\n"
"---------------------------------------------------------------------------\n"
"CLUSTERLOG OFF  Disable Cluster logging\n\n"
"CLUSTERLOG OFF [<severity>] ...  \n"
"                   Turn the cluster log off.\n"
"                   It tells management server which serverity\n"
"                   levels logging will be disabled.\n\n"
"                   <severity> can be any one of the following values:\n"
"                   ALERT, CRITICAL, ERROR, WARNING, INFO, DEBUG.\n"
;

static const char* helpTextClusterlogToggle =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for CLUSTERLOG TOGGLE command\n"
"---------------------------------------------------------------------------\n"
"CLUSTERLOG TOGGLE  Toggle severity filter on/off\n\n"
"CLUSTERLOG TOGGLE [<severity>] ...  \n"
"                   Toggle serverity filter on/off.\n"
"                   If a serverity level is already enabled,then it will\n"
"                   be disabled after you use the command,vice versa.\n\n"
"                   <severity> can be any one of the following values:\n"
"                   ALERT, CRITICAL, ERROR, WARNING, INFO, DEBUG.\n"
;

static const char* helpTextClusterlogInfo =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for CLUSTERLOG INFO command\n"
"---------------------------------------------------------------------------\n"
"CLUSTERLOG INFO  Print cluster log information\n\n"
"CLUSTERLOG INFO    Display which severity levels have been enabled,\n"
"                   see HELP CLUSTERLOG for list of the severity levels.\n"
;

static const char* helpTextStart =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for START command\n"
"---------------------------------------------------------------------------\n"
"START  Start data node (started with -n)\n\n"
"<id> START         Start the data node identified by <id>.\n"
"                   Only starts data nodes that have not\n"
"                   yet joined the cluster. These are nodes\n"
"                   launched or restarted with the -n(--nostart)\n"
"                   option.\n\n"
"                   It does not launch the ndbd process on a remote\n"
"                   machine.\n"
;

static const char* helpTextRestart =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for RESTART command\n"
"---------------------------------------------------------------------------\n"
"RESTART  Restart data or management server node\n\n"
"<id> RESTART [-n] [-i] [-a] [-f]\n"
"                   Restart the data or management node <id>(or All data nodes).\n\n"
"                   -n (--nostart) restarts the node but does not\n"
"                   make it join the cluster. Use '<id> START' to\n"
"                   join the node to the cluster.\n\n"
"                   -i (--initial) perform initial start.\n"
"                   This cleans the file system (ndb_<id>_fs)\n"
"                   and the node will copy data from another node\n"
"                   in the same node group during start up.\n\n"
"                   Consult the documentation before using -i.\n\n" 
"                   INCORRECT USE OF -i WILL CAUSE DATA LOSS!\n\n"
"                   -a Aborts the node, not syncing GCP.\n\n"
"                   -f Force restart even if that would mean the\n"
"                      whole cluster would need to be restarted\n"
;

static const char* helpTextStop =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for STOP command\n"
"---------------------------------------------------------------------------\n"
"STOP  Stop data or management server node\n\n"
"<id> STOP [-a] [-f]\n"
"                   Stop the data or management server node <id>.\n\n"
"                   ALL STOP will just stop all data nodes.\n\n"
"                   If you desire to also shut down management servers,\n"
"                   use SHUTDOWN instead.\n\n"
"                   -a Aborts the node, not syncing GCP.\n\n"
"                   -f Force stop even if that would mean the\n"
"                      whole cluster would need to be stopped\n"
;

static const char* helpTextEnterSingleUserMode =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for ENTER SINGLE USER MODE command\n"
"---------------------------------------------------------------------------\n"
"ENTER SINGLE USER MODE  Enter single user mode\n\n"
"ENTER SINGLE USER MODE <id> \n"
"                   Enters single-user mode, whereby only the MySQL Server or NDBAPI\n" 
"                   node identified by <id> is allowed to access the database. \n"
;

static const char* helpTextExitSingleUserMode =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for EXIT SINGLE USER MODE command\n"
"---------------------------------------------------------------------------\n"
"EXIT SINGLE USER MODE  Exit single user mode\n\n"
"EXIT SINGLE USER MODE \n"
"                   Exits single-user mode, allowing all SQL nodes \n"
"                   (that is, all running mysqld processes) to access the database. \n" 
;

static const char* helpTextStatus =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for STATUS command\n"
"---------------------------------------------------------------------------\n"
"STATUS  Print status\n\n"
"<id> STATUS        Displays status information for the data node <id>\n"
"                   or for All data nodes. \n\n"
"                   e.g.\n"
"                      ALL STATUS\n"
"                      1 STATUS\n\n"
"                   When a node is starting, the start phase will be\n"
"                   listed.\n\n"
"                   Start Phase   Meaning\n"
"                   1             Clear the cluster file system(ndb_<id>_fs). \n"
"                                 This stage occurs only when the --initial option \n"
"                                 has been specified.\n"
"                   2             This stage sets up Cluster connections, establishes \n"
"                                 inter-node communications and starts Cluster heartbeats.\n"
"                   3             The arbitrator node is elected.\n"
"                   4             Initializes a number of internal cluster variables.\n"
"                   5             For an initial start or initial node restart,\n"
"                                 the redo log files are created.\n"
"                   6             If this is an initial start, create internal system tables.\n"
"                   7             Update internal variables. \n"
"                   8             In a system restart, rebuild all indexes.\n"
"                   9             Update internal variables. \n"
"                   10            The node can be connected by APIs and can receive events.\n"
"                   11            At this point,event delivery is handed over to\n"
"                                 the node joining the cluster.\n"
"(see manual for more information)\n"
;

static const char* helpTextClusterlog =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for CLUSTERLOG command\n"
"---------------------------------------------------------------------------\n"
"CLUSTERLOG  Set log level for cluster log\n\n"
" <id> CLUSTERLOG {<category>=<level>}+  \n"
"                   Logs <category> events with priority less than \n"
"                   or equal to <level> in the cluster log.\n\n"
"                   <category> can be any one of the following values:\n"
"                   STARTUP, SHUTDOWN, STATISTICS, CHECKPOINT, NODERESTART,\n"
"                   CONNECTION, ERROR, INFO, CONGESTION, DEBUG, or BACKUP. \n\n"
"                   <level> is represented by one of the numbers \n"
"                   from 1 to 15 inclusive, where 1 indicates 'most important' \n"
"                   and 15 'least important'.\n\n"
"                   <severity> can be any one of the following values:\n"
"                   ALERT, CRITICAL, ERROR, WARNING, INFO, DEBUG.\n"
;


static const char* helpTextPurgeStaleSessions =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for PURGE STALE SESSIONS command\n"
"---------------------------------------------------------------------------\n"
"PURGE STALE SESSIONS  Reset reserved nodeid's in the mgmt server\n\n"
"PURGE STALE SESSIONS \n"
"                   Running this statement forces all reserved \n"
"                   node IDs to be checked; any that are not \n"
"                   being used by nodes acutally connected to \n"
"                   the cluster are then freed.\n\n"   
"                   This command is not normally needed, but may be\n"
"                   required in some situations where failed nodes \n"
"                   cannot rejoin the cluster due to failing to\n"
"                   allocate a node id.\n" 
;

static const char* helpTextConnect =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for CONNECT command\n"
"---------------------------------------------------------------------------\n"
"CONNECT  Connect to management server (reconnect if already connected)\n\n"
"CONNECT [<connectstring>] \n"
"                   Connect to management server.\n"
"                   The optional parameter connectstring specifies the \n"
"                   connect string to user.\n\n"
"                   A connect string may be:\n"
"                       mgm-server\n"
"                       mgm-server:port\n"
"                       mgm1:port,mgm2:port\n"
"                   With multiple management servers comma separated.\n"
"                   The management client with try to connect to the \n"
"                   management servers in the order they are listed.\n\n"
"                   If no connect string is specified, the default \n"
"                   is used. \n"
;

static const char* helpTextReport =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for REPORT command\n"
"---------------------------------------------------------------------------\n"
"REPORT  Displays a report of type <report-type> for the specified data \n"
"        node, or for all data nodes using ALL\n"
;
static void helpTextReportFn();
static void helpTextReportTypeOptionFn();


static const char* helpTextQuit =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for QUIT command\n"
"---------------------------------------------------------------------------\n"
"QUIT  Quit management client\n\n"
"QUIT               Terminates the management client. \n"                    
;


#ifdef VM_TRACE // DEBUG ONLY
static const char* helpTextDebug =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for Debugging (Internal use only)\n"
"---------------------------------------------------------------------------\n"
"SHOW PROPERTIES                       Print config properties object\n"
"<id> LOGLEVEL {<category>=<level>}+   Set log level\n"
#ifdef ERROR_INSERT
"<id> ERROR <errorNo>                  Inject error into NDB node\n"
#endif
"<id> LOG [BLOCK = {ALL|<block>+}]     Set logging on in & out signals\n"
"<id> TESTON                           Start signal logging\n"
"<id> TESTOFF                          Stop signal logging\n"
"<id> DUMP <arg>                       Dump system state to cluster.log\n"
"\n"
"<id>       = ALL | Any database node id\n"
;
#endif

struct st_cmd_help {
  const char *cmd;
  const char * help;
  void (* help_fn)();
}help_items[]={
  {"SHOW", helpTextShow, NULL},
  {"HELP", helpTextHelp, NULL},
  {"BACKUP", helpTextBackup, NULL},
  {"START BACKUP", helpTextStartBackup, NULL},
  {"START BACKUP NOWAIT", helpTextStartBackup, NULL},
  {"START BACKUP WAIT STARTED", helpTextStartBackup, NULL},
  {"START BACKUP WAIT", helpTextStartBackup, NULL},
  {"START BACKUP WAIT COMPLETED", helpTextStartBackup, NULL},
  {"ABORT BACKUP", helpTextAbortBackup, NULL},
  {"SHUTDOWN", helpTextShutdown, NULL},
  {"CLUSTERLOG ON", helpTextClusterlogOn, NULL},
  {"CLUSTERLOG OFF", helpTextClusterlogOff, NULL},
  {"CLUSTERLOG TOGGLE", helpTextClusterlogToggle, NULL},
  {"CLUSTERLOG INFO", helpTextClusterlogInfo, NULL},
  {"START", helpTextStart, NULL},
  {"RESTART", helpTextRestart, NULL},
  {"STOP", helpTextStop, NULL},
  {"ENTER SINGLE USER MODE", helpTextEnterSingleUserMode, NULL},
  {"EXIT SINGLE USER MODE", helpTextExitSingleUserMode, NULL},
  {"STATUS", helpTextStatus, NULL},
  {"CLUSTERLOG", helpTextClusterlog, NULL},
  {"PURGE STALE SESSIONS", helpTextPurgeStaleSessions, NULL},
  {"CONNECT", helpTextConnect, NULL},
  {"REPORT", helpTextReport, helpTextReportFn},
  {"QUIT", helpTextQuit, NULL},
#ifdef VM_TRACE // DEBUG ONLY
  {"DEBUG", helpTextDebug, NULL},
#endif //VM_TRACE
  {NULL, NULL, NULL}
};

static bool
convert(const char* s, int& val) {
  
  if (s == NULL)
    return false;

  if (strlen(s) == 0)
    return false;

  errno = 0;
  char* p;
  long v = strtol(s, &p, 10);
  if (errno != 0)
    return false;

  if (p != &s[strlen(s)])
    return false;
  
  val = v;
  return true;
}

/*
 * Constructor
 */
CommandInterpreter::CommandInterpreter(const char *host,int verbose) :
  m_constr(host),
  m_connected(false),
  m_verbose(verbose),
  m_try_reconnect(0),
  m_error(-1),
  m_event_thread(NULL)
{
  m_print_mutex= NdbMutex_Create();
}

/*
 * Destructor
 */
CommandInterpreter::~CommandInterpreter() 
{
  disconnect();
  NdbMutex_Destroy(m_print_mutex);
}

static bool 
emptyString(const char* s) 
{
  if (s == NULL) {
    return true;
  }

  for (unsigned int i = 0; i < strlen(s); ++i) {
    if (! isspace(s[i])) {
      return false;
    }
  }

  return true;
}

void
CommandInterpreter::printError() 
{
  if (m_mgmsrv)
  {
    ndbout_c("* %5d: %s", 
             ndb_mgm_get_latest_error(m_mgmsrv),
             ndb_mgm_get_latest_error_msg(m_mgmsrv));
    ndbout_c("*        %s", ndb_mgm_get_latest_error_desc(m_mgmsrv));
    if (ndb_mgm_check_connection(m_mgmsrv))
    {
      disconnect();
    }
  }
}

/*
 * print log event from mgmsrv to console screen
 */
#define make_uint64(a,b) (((Uint64)(a)) + (((Uint64)(b)) << 32))
#define Q64(a) make_uint64(event->EVENT.a ## _lo, event->EVENT.a ## _hi)
#define R event->source_nodeid
#define Q(a) event->EVENT.a
#define QVERSION getMajor(Q(version)), getMinor(Q(version)), getBuild(Q(version))
#define NDB_LE_(a) NDB_LE_ ## a
static void
printLogEvent(struct ndb_logevent* event)
{
  switch (event->type) {
    /** 
     * NDB_MGM_EVENT_CATEGORY_BACKUP
     */
#undef  EVENT
#define EVENT BackupStarted
  case NDB_LE_BackupStarted:
      ndbout_c("Node %u: Backup %u started from node %d",
               R, Q(backup_id), Q(starting_node));
      break;
#undef EVENT
#define EVENT BackupStatus
    case NDB_LE_BackupStatus:
      if (Q(starting_node))
        ndbout_c("Node %u: Local backup status: backup %u started from node %u\n" 
                 " #Records: %llu #LogRecords: %llu\n"
                 " Data: %llu bytes Log: %llu bytes", R,
                 Q(backup_id),
                 Q(starting_node),
                 Q64(n_records),
                 Q64(n_log_records),
                 Q64(n_bytes),
                 Q64(n_log_bytes));
      else
        ndbout_c("Node %u: Backup not started", R);
      break;
#undef  EVENT
#define EVENT BackupFailedToStart
    case NDB_LE_BackupFailedToStart:
      ndbout_c("Node %u: Backup request from %d failed to start. Error: %d",
               R, Q(starting_node), Q(error));
      break;
#undef  EVENT
#define EVENT BackupCompleted
    case NDB_LE_BackupCompleted:
      ndbout_c("Node %u: Backup %u started from node %u completed\n" 
               " StartGCP: %u StopGCP: %u\n" 
               " #Records: %u #LogRecords: %u\n" 
               " Data: %u bytes Log: %u bytes", R,
               Q(backup_id), Q(starting_node),
               Q(start_gci), Q(stop_gci),
               Q(n_records), Q(n_log_records),
               Q(n_bytes),   Q(n_log_bytes));
      break;
#undef  EVENT
#define EVENT BackupAborted
    case NDB_LE_BackupAborted:
      ndbout_c("Node %u: Backup %u started from %d has been aborted. Error: %d",
               R, Q(backup_id), Q(starting_node), Q(error));
      break;
    /** 
     * NDB_MGM_EVENT_CATEGORY_STARTUP
     */ 
#undef  EVENT
#define EVENT NDBStartStarted
    case NDB_LE_NDBStartStarted:
      ndbout_c("Node %u: Start initiated (version %d.%d.%d)",
               R, QVERSION);
      break;
#undef  EVENT
#define EVENT NDBStartCompleted
    case NDB_LE_NDBStartCompleted:
      ndbout_c("Node %u: Started (version %d.%d.%d)",
               R, QVERSION);
      break;
#undef  EVENT
#define EVENT NDBStopStarted
    case NDB_LE_NDBStopStarted:
      ndbout_c("Node %u: %s shutdown initiated", R,
               (Q(stoptype) == 1 ? "Cluster" : "Node"));
      break;
#undef  EVENT
#define EVENT NDBStopCompleted
    case NDB_LE_NDBStopCompleted:
      {
        BaseString action_str("");
        BaseString signum_str("");
        getRestartAction(Q(action), action_str);
        if (Q(signum))
          signum_str.appfmt(" Initiated by signal %d.", 
                            Q(signum));
        ndbout_c("Node %u: Node shutdown completed%s.%s", 
                 R, action_str.c_str(), signum_str.c_str());
      }
      break;
#undef  EVENT
#define EVENT NDBStopForced
    case NDB_LE_NDBStopForced:
      {
        BaseString action_str("");
        BaseString reason_str("");
        BaseString sphase_str("");
        int signum = Q(signum);
        int error = Q(error); 
        int sphase = Q(sphase); 
        int extra = Q(extra); 
        getRestartAction(Q(action), action_str);
        if (signum)
          reason_str.appfmt(" Initiated by signal %d.", signum);
        if (error)
        {
          ndbd_exit_classification cl;
          ndbd_exit_status st;
          const char *msg = ndbd_exit_message(error, &cl);
          const char *cl_msg = ndbd_exit_classification_message(cl, &st);
          const char *st_msg = ndbd_exit_status_message(st);
          reason_str.appfmt(" Caused by error %d: \'%s(%s). %s\'.", 
                            error, msg, cl_msg, st_msg);
          if (extra != 0)
            reason_str.appfmt(" (extra info %d)", extra);
        }
        if (sphase < 255)
          sphase_str.appfmt(" Occured during startphase %u.", sphase);
        ndbout_c("Node %u: Forced node shutdown completed%s.%s%s",
                 R, action_str.c_str(), sphase_str.c_str(), 
                 reason_str.c_str());
      }
      break;
#undef  EVENT
#define EVENT StopAborted
    case NDB_LE_NDBStopAborted:
      ndbout_c("Node %u: Node shutdown aborted", R);
      break;
    /** 
     * NDB_MGM_EVENT_CATEGORY_STATISTIC
     */ 
#undef EVENT
#define EVENT MemoryUsage
    case NDB_LE_MemoryUsage:
    {

      if (Q(gth) == 0)
      {
        // Only print MemoryUsage report for increased/decreased
        break;
      }

      const int percent = Q(pages_total) ? (Q(pages_used)*100)/Q(pages_total) : 0;
      ndbout_c("Node %u: %s usage %s %d%s(%d %dK pages of total %d)", R,
               (Q(block) == DBACC ? "Index" : (Q(block) == DBTUP ?"Data":"<unknown>")),
               (Q(gth) > 0 ? "increased to" : "decreased to"),
               percent, "%",
               Q(pages_used), Q(page_size_kb)/1024, Q(pages_total));
      break;
    }
    /** 
     * default nothing to print
     */ 
    default:
      break;
  }
}

//*****************************************************************************
//*****************************************************************************

struct event_thread_param {
  NdbMgmHandle *m;
  NdbMutex **p;
};

static int do_event_thread = 0;

static void*
event_thread_run(void* p)
{
  DBUG_ENTER("event_thread_run");

  struct event_thread_param param= *(struct event_thread_param*)p;
  NdbMgmHandle handle= *(param.m);
  NdbMutex* printmutex= *(param.p);

  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
		   1, NDB_MGM_EVENT_CATEGORY_STARTUP,
                   5, NDB_MGM_EVENT_CATEGORY_STATISTIC,
		   0 };

  NdbLogEventHandle log_handle= NULL;
  struct ndb_logevent log_event;

  log_handle= ndb_mgm_create_logevent_handle(handle, filter);
  if (log_handle) 
  {
    do_event_thread= 1;
    do {
      int res= ndb_logevent_get_next(log_handle, &log_event, 2000);
      if (res > 0)
      {
        Guard g(printmutex);
        printLogEvent(&log_event);
      }
      else if (res < 0)
        break;
    } while(do_event_thread);
    ndb_mgm_destroy_logevent_handle(&log_handle);
  }
  else
  {
    do_event_thread= 0;
  }

  DBUG_RETURN(NULL);
}

bool
CommandInterpreter::connect(bool interactive)
{
  DBUG_ENTER("CommandInterpreter::connect");

  if(m_connected)
    DBUG_RETURN(m_connected);

  m_mgmsrv = ndb_mgm_create_handle();
  if(m_mgmsrv == NULL) {
    ndbout_c("Can't create handle to management server.");
    exit(-1);
  }

  if (interactive) {
    m_mgmsrv2 = ndb_mgm_create_handle();
    if(m_mgmsrv2 == NULL) {
      ndbout_c("Can't create 2:nd handle to management server.");
      exit(-1);
    }
  }

  if (ndb_mgm_set_connectstring(m_mgmsrv, m_constr))
  {
    printError();
    exit(-1);
  }

  if(ndb_mgm_connect(m_mgmsrv, m_try_reconnect-1, 5, 1))
    DBUG_RETURN(m_connected); // couldn't connect, always false

  const char *host= ndb_mgm_get_connected_host(m_mgmsrv);
  unsigned port= ndb_mgm_get_connected_port(m_mgmsrv);
  if (interactive) {
    BaseString constr;
    constr.assfmt("%s:%d",host,port);
    if(!ndb_mgm_set_connectstring(m_mgmsrv2, constr.c_str()) &&
       !ndb_mgm_connect(m_mgmsrv2, m_try_reconnect-1, 5, 1))
    {
      DBUG_PRINT("info",("2:ndb connected to Management Server ok at: %s:%d",
                         host, port));
      assert(m_event_thread == NULL);
      assert(do_event_thread == 0);
      do_event_thread= 0;
      struct event_thread_param p;
      p.m= &m_mgmsrv2;
      p.p= &m_print_mutex;
      m_event_thread = NdbThread_Create(event_thread_run,
                                        (void**)&p,
                                        0, // default stack size
                                        "CommandInterpreted_event_thread",
                                        NDB_THREAD_PRIO_LOW);
      if (m_event_thread)
      {
        DBUG_PRINT("info",("Thread created ok, waiting for started..."));
        int iter= 1000; // try for 30 seconds
        while(do_event_thread == 0 &&
              iter-- > 0)
          NdbSleep_MilliSleep(30);
      }
      if (m_event_thread == NULL ||
          do_event_thread == 0 ||
          do_event_thread == -1)
      {
        DBUG_PRINT("info",("Warning, event thread startup failed, "
                           "degraded printouts as result, errno=%d",
                           errno));
        printf("Warning, event thread startup failed, "
               "degraded printouts as result, errno=%d\n", errno);
        do_event_thread= 0;
        if (m_event_thread)
        {
          void *res;
          NdbThread_WaitFor(m_event_thread, &res);
          NdbThread_Destroy(&m_event_thread);
        }
        ndb_mgm_disconnect(m_mgmsrv2);
      }
    }
    else
    {
      DBUG_PRINT("warning",
                 ("Could not do 2:nd connect to mgmtserver for event listening"));
      DBUG_PRINT("info", ("code: %d, msg: %s",
                          ndb_mgm_get_latest_error(m_mgmsrv2),
                          ndb_mgm_get_latest_error_msg(m_mgmsrv2)));
      printf("Warning, event connect failed, degraded printouts as result\n");
      printf("code: %d, msg: %s\n",
             ndb_mgm_get_latest_error(m_mgmsrv2),
             ndb_mgm_get_latest_error_msg(m_mgmsrv2));
    }
  }
  m_connected= true;
  DBUG_PRINT("info",("Connected to Management Server at: %s:%d", host, port));
  if (m_verbose)
  {
    printf("Connected to Management Server at: %s:%d\n",
           host, port);
  }

  DBUG_RETURN(m_connected);
}

void
CommandInterpreter::disconnect(void)
{
  DBUG_ENTER("CommandInterpreter::disconnect");

  if (m_event_thread) {
    void *res;
    do_event_thread= 0;
    NdbThread_WaitFor(m_event_thread, &res);
    NdbThread_Destroy(&m_event_thread);
    m_event_thread= NULL;
    ndb_mgm_destroy_handle(&m_mgmsrv2);
  }
  if (m_connected)
  {
    ndb_mgm_destroy_handle(&m_mgmsrv);
    m_connected= false;
  }
  DBUG_VOID_RETURN;
}

//*****************************************************************************
//*****************************************************************************

bool
CommandInterpreter::execute(const char *_line, int try_reconnect,
			    bool interactive, int *error)
{
  if (try_reconnect >= 0)
    m_try_reconnect = try_reconnect;
  bool result= execute_impl(_line, interactive);
  if (error)
    *error= m_error;

  return result;
}

static void
invalid_command(const char *cmd, const char *msg=0)
{
  ndbout << "Invalid command: " << cmd << endl;
  if(msg)
      ndbout << msg << endl;
  ndbout << "Type HELP for help." << endl << endl;
}


// Utility class for easier checking of args
// given to the commands
class ClusterInfo {
  ndb_mgm_cluster_state* m_status;

public:
  ClusterInfo() :
    m_status(NULL) {};

  ~ClusterInfo() {
    if (m_status)
      free(m_status);
  }

  bool fetch(NdbMgmHandle handle, bool all_nodes = false) {

    const ndb_mgm_node_type types[2] = {
      NDB_MGM_NODE_TYPE_NDB,
      NDB_MGM_NODE_TYPE_UNKNOWN
    };
    m_status = ndb_mgm_get_status2(handle,
                                   !all_nodes ? types : 0);
    if (m_status == NULL)
    {
      ndbout_c("ERROR: couldn't fetch cluster status");
      return false;
    }
    return true;
  }

  bool is_valid_ndb_nodeid(int nodeid) const {
    // Check valid NDB nodeid
    if (nodeid < 1 || nodeid >= MAX_NDB_NODES)
    {
      ndbout_c("ERROR: illegal nodeid %d!", nodeid);
      return false;
    }
    return true;
  }

  bool is_ndb_node(int nodeid) const {

    if (!is_valid_ndb_nodeid(nodeid))
      return false;

    bool found = false;
    for (int i = 0; i < m_status->no_of_nodes; i++)
    {
      if (m_status->node_states[i].node_id == nodeid &&
          m_status->node_states[i].node_type == NDB_MGM_NODE_TYPE_NDB)
      {
        found = true;
        break;
      }
    }

    if (!found)
      ndbout_c("ERROR: node %d is not a NDB node!", nodeid);

    return found;
  }
};



static void
split_args(const char* line, Vector<BaseString>& args)
{
  // Split the command line on space
  BaseString tmp(line);
  tmp.split(args);

  // Remove any empty args which come from double
  // spaces in the command line
  // ie. "hello<space><space>world" becomes ("hello, "", "world")
  //
  for (unsigned i= 0; i < args.size(); i++)
    if (args[i].length() == 0)
      args.erase(i--);
}


bool
CommandInterpreter::execute_impl(const char *_line, bool interactive)
{
  DBUG_ENTER("CommandInterpreter::execute_impl");
  DBUG_PRINT("enter",("line='%s'", _line));
  m_error= 0;

  if(_line == NULL)
  {
    // Pressing Ctrl-C on some platforms will cause 'readline' to
    // to return NULL, handle it as graceful exit of ndb_mgm 
    m_error = -1;
    DBUG_RETURN(false); // Terminate gracefully
  }

  char* line = strdup(_line);
  if (line == NULL)
  {
    ndbout_c("ERROR: Memory allocation error at %s:%d.", __FILE__, __LINE__);
    m_error = -1;
    DBUG_RETURN(false); // Terminate gracefully
  }
  NdbAutoPtr<char> ap(line);

  int do_continue;
  do {
    do_continue= 0;
    BaseString::trim(line," \t");
    if (line[0] == 0 ||
	line[0] == '#')
    {
      DBUG_RETURN(true);
    }
    // for mysql client compatability remove trailing ';'
    {
      unsigned last= (unsigned)(strlen(line)-1);
      if (line[last] == ';')
      {
	line[last]= 0;
	do_continue= 1;
      }
    }
  } while (do_continue);

  // if there is anything in the line proceed
  Vector<BaseString> command_list;
  split_args(line, command_list);

  char* firstToken = strtok(line, " ");
  char* allAfterFirstToken = strtok(NULL, "");

  if (native_strcasecmp(firstToken, "HELP") == 0 ||
      native_strcasecmp(firstToken, "?") == 0) {
    m_error = executeHelp(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (native_strcasecmp(firstToken, "CONNECT") == 0) {
    m_error = executeConnect(allAfterFirstToken, interactive);
    DBUG_RETURN(true);
  }
  else if (native_strcasecmp(firstToken, "SLEEP") == 0) {
    if (allAfterFirstToken)
      NdbSleep_SecSleep(atoi(allAfterFirstToken));
    DBUG_RETURN(true);
  }
  else if((native_strcasecmp(firstToken, "QUIT") == 0 ||
	  native_strcasecmp(firstToken, "EXIT") == 0 ||
	  native_strcasecmp(firstToken, "BYE") == 0) && 
	  allAfterFirstToken == NULL){
    DBUG_RETURN(false);
  }

  if (!connect(interactive)){
    m_error = -1;
    DBUG_RETURN(true);
  }

  if (ndb_mgm_check_connection(m_mgmsrv))
  {
    disconnect();
    connect(interactive);
  }

  if (native_strcasecmp(firstToken, "SHOW") == 0) {
    Guard g(m_print_mutex);
    m_error = executeShow(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (native_strcasecmp(firstToken, "SHUTDOWN") == 0) {
    m_error= executeShutdown(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (native_strcasecmp(firstToken, "CLUSTERLOG") == 0){
    executeClusterLog(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(native_strcasecmp(firstToken, "START") == 0 &&
	  allAfterFirstToken != NULL &&
	  native_strncasecmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    m_error= executeStartBackup(allAfterFirstToken, interactive);
    DBUG_RETURN(true);
  }
  else if(native_strcasecmp(firstToken, "ABORT") == 0 &&
	  allAfterFirstToken != NULL &&
	  native_strncasecmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    m_error = executeAbortBackup(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (native_strcasecmp(firstToken, "PURGE") == 0) {
    m_error = executePurge(allAfterFirstToken);
    DBUG_RETURN(true);
  }                
  else if(native_strcasecmp(firstToken, "ENTER") == 0 &&
	  allAfterFirstToken != NULL &&
	  allAfterFirstToken != NULL &&
	  native_strncasecmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    m_error = executeEnterSingleUser(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(native_strcasecmp(firstToken, "EXIT") == 0 &&
	  allAfterFirstToken != NULL &&
	  native_strncasecmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    m_error = executeExitSingleUser(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(native_strcasecmp(firstToken, "CREATE") == 0 &&
	  allAfterFirstToken != NULL &&
	  native_strncasecmp(allAfterFirstToken, "NODEGROUP",
                      sizeof("NODEGROUP") - 1) == 0){
    m_error = executeCreateNodeGroup(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(native_strcasecmp(firstToken, "DROP") == 0 &&
	  allAfterFirstToken != NULL &&
	  native_strncasecmp(allAfterFirstToken, "NODEGROUP",
                      sizeof("NODEGROUP") - 1) == 0){
    m_error = executeDropNodeGroup(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (native_strcasecmp(firstToken, "ALL") == 0) {
    m_error = analyseAfterFirstToken(-1, allAfterFirstToken);
  } else {
    /**
     * First tokens should be digits, node ID's
     */
    int node_ids[MAX_NODES];
    unsigned pos;
    for (pos= 0; pos < command_list.size(); pos++)
    {
      int node_id;
      if (convert(command_list[pos].c_str(), node_id))
      {
        if (node_id <= 0 || node_id > MAX_NODES) {
          ndbout << "Invalid node ID: " << command_list[pos].c_str()
                 << "." << endl;
          DBUG_RETURN(true);
        }
        node_ids[pos]= node_id;
        continue;
      }
      break;
    }
    int no_of_nodes= pos;
    if (no_of_nodes == 0)
    {
      /* No digit found */
      invalid_command(_line);
      m_error = -1;
      DBUG_RETURN(true);
    }
    if (pos == command_list.size())
    {
      /* No command found */
      invalid_command(_line);
      m_error = -1;
      DBUG_RETURN(true);
    }
    if (no_of_nodes == 1)
    {
      m_error = analyseAfterFirstToken(node_ids[0], allAfterFirstToken);
      DBUG_RETURN(true);
    }
    m_error = executeCommand(command_list, pos, node_ids, no_of_nodes);
    DBUG_RETURN(true);
  }
  DBUG_RETURN(true);
}


/**
 * List of commands used as second command argument
 */
static const CommandInterpreter::CommandFunctionPair commands[] = {
  { "START", &CommandInterpreter::executeStart }
  ,{ "RESTART", &CommandInterpreter::executeRestart }
  ,{ "STOP", &CommandInterpreter::executeStop }
  ,{ "STATUS", &CommandInterpreter::executeStatus }
  ,{ "LOGLEVEL", &CommandInterpreter::executeLogLevel }
  ,{ "CLUSTERLOG", &CommandInterpreter::executeEventReporting }
#ifdef ERROR_INSERT
  ,{ "ERROR", &CommandInterpreter::executeError }
#endif
  ,{ "LOG", &CommandInterpreter::executeLog }
  ,{ "TESTON", &CommandInterpreter::executeTestOn }
  ,{ "TESTOFF", &CommandInterpreter::executeTestOff }
  ,{ "DUMP", &CommandInterpreter::executeDumpState }
  ,{ "REPORT", &CommandInterpreter::executeReport }
};


//*****************************************************************************
//*****************************************************************************
int
CommandInterpreter::analyseAfterFirstToken(int processId,
					   char* allAfterFirstToken) {
  
  int retval = 0;
  if (emptyString(allAfterFirstToken)) {
    ndbout << "Expected a command after "
	   << ((processId == -1) ? "ALL." : "node ID.") << endl;
    return -1;
  }
  
  char* secondToken = strtok(allAfterFirstToken, " ");
  char* allAfterSecondToken = strtok(NULL, "\0");

  const int tmpSize = sizeof(commands)/sizeof(CommandFunctionPair);
  ExecuteFunction fun = 0;
  const char * command = 0;
  for(int i = 0; i<tmpSize; i++){
    if(native_strcasecmp(secondToken, commands[i].command) == 0){
      fun = commands[i].executeFunction;
      command = commands[i].command;
      break;
    }
  }
  
  if(fun == 0){
    invalid_command(secondToken);
    return -1;
  }
  
  if(processId == -1){
    retval = executeForAll(command, fun, allAfterSecondToken);
  } else {
    retval = (this->*fun)(processId, allAfterSecondToken, false);
  }
  ndbout << endl;
  return retval;
}

int
CommandInterpreter::executeCommand(Vector<BaseString> &command_list,
                                   unsigned command_pos,
                                   int *node_ids, int no_of_nodes)
{
  const char *cmd= command_list[command_pos].c_str();
  int retval = 0;

  if (native_strcasecmp("STOP", cmd) == 0)
  {
    retval = executeStop(command_list, command_pos+1, node_ids, no_of_nodes);
    return retval;
  }
  if (native_strcasecmp("RESTART", cmd) == 0)
  {
    retval = executeRestart(command_list, command_pos+1, node_ids, no_of_nodes);
    return retval;
  }
  if (native_strcasecmp("START", cmd) == 0)
  {
    retval = executeStart(command_list, command_pos+1, node_ids, no_of_nodes);
    return retval;
  }
  ndbout_c("Invalid command: '%s' after multi node id list. "
           "Expected STOP, START, or RESTART.", cmd);
  return -1;
}

/**
 * Get next nodeid larger than the give node_id. node_id will be
 * set to the next node_id in the list. node_id should be set
 * to 0 (zero) on the first call.
 *
 * @param handle the NDB management handle
 * @param node_id last node_id retreived, 0 at first call
 * @param type type of node to look for
 * @return 1 if a node was found, 0 if no more node exist
 */
static 
int 
get_next_nodeid(struct ndb_mgm_cluster_state *cl,
		int *node_id,
		enum ndb_mgm_node_type type)
{
  int i;
  
  if(cl == NULL)
    return 0;
  
  i=0;
  while((i < cl->no_of_nodes)) {
    if((*node_id < cl->node_states[i].node_id) &&
       (cl->node_states[i].node_type == type)) {
      
      if(i >= cl->no_of_nodes)
	return 0;
      
      *node_id = cl->node_states[i].node_id;
      return 1;
    }
    i++;
  }
  
  return 0;
}

int
CommandInterpreter::executeForAll(const char * cmd, ExecuteFunction fun, 
				  const char * allAfterSecondToken)
{
  int nodeId = 0;
  int retval = 0;

  if(native_strcasecmp(cmd, "STOP") == 0) {
    ndbout_c("Executing STOP on all nodes.");
    retval = (this->*fun)(nodeId, allAfterSecondToken, true);
  } else if(native_strcasecmp(cmd, "RESTART") == 0) {
    retval = (this->*fun)(nodeId, allAfterSecondToken, true);
  } else if (native_strcasecmp(cmd, "STATUS") == 0) {
    (this->*fun)(nodeId, allAfterSecondToken, true);
  } else if (native_strcasecmp(cmd, "REPORT") == 0) {
    Guard g(m_print_mutex);
    retval = executeReport(nodeId, allAfterSecondToken, true);
  } else {
    Guard g(m_print_mutex);
    struct ndb_mgm_cluster_state *cl= ndb_mgm_get_status(m_mgmsrv);
    if(cl == 0){
      ndbout_c("Unable get status from management server");
      printError();
      return -1;
    }
    NdbAutoPtr<char> ap1((char*)cl);
    while(get_next_nodeid(cl, &nodeId, NDB_MGM_NODE_TYPE_NDB))
      retval = (this->*fun)(nodeId, allAfterSecondToken, true);
  }
  return retval;
}

//*****************************************************************************
//*****************************************************************************
bool 
CommandInterpreter::parseBlockSpecification(const char* allAfterLog,
					    Vector<BaseString>& blocks)
{
  // Parse: [BLOCK = {ALL|<blockName>+}]

  if (emptyString(allAfterLog)) {
    return true;
  }

  // Copy allAfterLog since strtok will modify it  
  char* newAllAfterLog = strdup(allAfterLog);
  if (newAllAfterLog == NULL)
  {
    ndbout_c("ERROR: Memory allocation error at %s:%d.", __FILE__, __LINE__);
    return false; // Error parsing
  }

  NdbAutoPtr<char> ap1(newAllAfterLog);
  char* firstTokenAfterLog = strtok(newAllAfterLog, " ");
  for (unsigned int i = 0; i < strlen(firstTokenAfterLog); ++i) {
    firstTokenAfterLog[i] = toupper(firstTokenAfterLog[i]);
  }
  
  if (native_strcasecmp(firstTokenAfterLog, "BLOCK") != 0) {
    ndbout << "Unexpected value: " << firstTokenAfterLog 
	   << ". Expected BLOCK." << endl;
    return false;
  }

  char* allAfterFirstToken = strtok(NULL, "\0");
  if (emptyString(allAfterFirstToken)) {
    ndbout << "Expected =." << endl;
    return false;
  }

  char* secondTokenAfterLog = strtok(allAfterFirstToken, " ");
  if (native_strcasecmp(secondTokenAfterLog, "=") != 0) {
    ndbout << "Unexpected value: " << secondTokenAfterLog 
	   << ". Expected =." << endl;
    return false;
  }

  char* blockName = strtok(NULL, " ");
  bool all = false;
  if (blockName != NULL && (native_strcasecmp(blockName, "ALL") == 0)) {
    all = true;
  }
  while (blockName != NULL) {
    blocks.push_back(blockName);
    blockName = strtok(NULL, " ");
  }

  if (blocks.size() == 0) {
    ndbout << "No block specified." << endl;
    return false;
  }
  if (blocks.size() > 1 && all) {
    // More than "ALL" specified
    ndbout << "Nothing expected after ALL." << endl;
    return false;
  }
  
  return true;
}



/*****************************************************************************
 * HELP
 *****************************************************************************/
int
CommandInterpreter::executeHelp(char* parameters)
{
  if (emptyString(parameters)) {
    ndbout << helpText;

    ndbout << endl 
	   << "<severity> = " 
	   << "ALERT | CRITICAL | ERROR | WARNING | INFO | DEBUG"
	   << endl;

    ndbout << "<category> = ";
    for(int i = CFG_MIN_LOGLEVEL; i <= CFG_MAX_LOGLEVEL; i++){
      const char *str= ndb_mgm_get_event_category_string((ndb_mgm_event_category)i);
      if (str) {
	if (i != CFG_MIN_LOGLEVEL)
	  ndbout << " | ";
	ndbout << str;
      }
    }
    ndbout << endl;

    helpTextReportTypeOptionFn();

    ndbout << "<level>    = " << "0 - 15" << endl;
    ndbout << "<id>       = " << "ALL | Any database node id" << endl;
    ndbout << endl;
    ndbout << "For detailed help on COMMAND, use HELP COMMAND." << endl;
  } else {
    int i = 0;
    for (i = 0; help_items[i].cmd != NULL; i++) 
    {
      if (native_strcasecmp(parameters, help_items[i].cmd) == 0)
      {
        if (help_items[i].help)
          ndbout << help_items[i].help;
        if (help_items[i].help_fn)
          (*help_items[i].help_fn)();
        break;
      }     
    }
    if (help_items[i].cmd == NULL){
      ndbout << "No help for " << parameters << " available" << endl;
      return -1;
    }
  }
  return 0;
}


/*****************************************************************************
 * SHUTDOWN
 *****************************************************************************/

int
CommandInterpreter::executeShutdown(char* parameters) 
{ 
  ndb_mgm_cluster_state *state = ndb_mgm_get_status(m_mgmsrv);
  if(state == NULL) {
    ndbout_c("Could not get status");
    printError();
    return 1;
  }
  NdbAutoPtr<char> ap1((char*)state);

  int result = 0;
  int need_disconnect;
  result = ndb_mgm_stop3(m_mgmsrv, -1, 0, 0, &need_disconnect);
  if (result < 0) {
    ndbout << "Shutdown of NDB Cluster node(s) failed." << endl;
    printError();
    return result;
  }

  ndbout << result << " NDB Cluster node(s) have shutdown." << endl;

  if(need_disconnect) {
    ndbout << "Disconnecting to allow management server to shutdown."
           << endl;
    disconnect();
  }
  return 0;
}

/*****************************************************************************
 * SHOW
 *****************************************************************************/


static
const char *status_string(ndb_mgm_node_status status)
{
  switch(status){
  case NDB_MGM_NODE_STATUS_NO_CONTACT:
    return "not connected";
  case NDB_MGM_NODE_STATUS_NOT_STARTED:
    return "not started";
  case NDB_MGM_NODE_STATUS_STARTING:
    return "starting";
  case NDB_MGM_NODE_STATUS_STARTED:
    return "started";
  case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
    return "shutting down";
  case NDB_MGM_NODE_STATUS_RESTARTING:
    return "restarting";
  case NDB_MGM_NODE_STATUS_SINGLEUSER:
    return "single user mode";
  default:
    return "unknown state";
  }
}

static void
print_nodes(ndb_mgm_cluster_state *state, ndb_mgm_configuration_iterator *it,
	    const char *proc_name, int no_proc, ndb_mgm_node_type type,
	    int master_id)
{ 
  int i;
  ndbout << "[" << proc_name
	 << "(" << ndb_mgm_get_node_type_string(type) << ")]\t"
	 << no_proc << " node(s)" << endl;
  for(i=0; i < state->no_of_nodes; i++) {
    struct ndb_mgm_node_state *node_state= &(state->node_states[i]);
    if(node_state->node_type == type) {
      int node_id= node_state->node_id;
      ndbout << "id=" << node_id;
      if(node_state->version != 0) {
	const char *hostname= node_state->connect_address;
	if (hostname == 0
	    || strlen(hostname) == 0
	    || native_strcasecmp(hostname,"0.0.0.0") == 0)
	  ndbout << " ";
	else
	  ndbout << "\t@" << hostname;

	char tmp[100];
	ndbout << "  (" << ndbGetVersionString(node_state->version,
                                               node_state->mysql_version,
                                               0,
                                               tmp, sizeof(tmp));
	if (type == NDB_MGM_NODE_TYPE_NDB) {
	  if (node_state->node_status != NDB_MGM_NODE_STATUS_STARTED) {
	    ndbout << ", " << status_string(node_state->node_status);
	  }
	  if (node_state->node_group >= 0 && node_state->node_group != (int)RNIL) {
	    ndbout << ", Nodegroup: " << node_state->node_group;
          }
          else if (node_state->node_group == (int)RNIL)
          {
            ndbout << ", no nodegroup";
          }
          if (node_state->node_group >= 0 || node_state->node_group == (int)RNIL)
	    if (master_id && node_state->dynamic_id == master_id)
	      ndbout << ", *";
        }
	ndbout << ")" << endl;
      } else {
	ndb_mgm_first(it);
	if(ndb_mgm_find(it, CFG_NODE_ID, node_id) == 0){
	  const char *config_hostname= 0;
	  ndb_mgm_get_string_parameter(it, CFG_NODE_HOST, &config_hostname);
	  if (config_hostname == 0 || config_hostname[0] == 0)
	    config_hostname= "any host";
	  ndbout_c(" (not connected, accepting connect from %s)",
		   config_hostname);
	}
	else
	{
	  ndbout_c("Unable to find node with id: %d", node_id);
	}
      }
    }
  }
  ndbout << endl;
}

int
CommandInterpreter::executePurge(char* parameters) 
{ 
  int command_ok= 0;
  do {
    if (emptyString(parameters))
      break;
    char* firstToken = strtok(parameters, " ");
    char* nextToken = strtok(NULL, " \0");
    if (native_strcasecmp(firstToken,"STALE") == 0 &&
	nextToken &&
	native_strcasecmp(nextToken, "SESSIONS") == 0) {
      command_ok= 1;
      break;
    }
  } while(0);

  if (!command_ok) {
    ndbout_c("Unexpected command, expected: PURGE STALE SESSIONS");
    return -1;
  }

  char *str;
  
  if (ndb_mgm_purge_stale_sessions(m_mgmsrv, &str)) {
    ndbout_c("Command failed");
    return -1;
  }
  if (str) {
    ndbout_c("Purged sessions with node id's: %s", str);
    free(str);
  }
  else
  {
    ndbout_c("No sessions purged");
  }
  return 0;
}

int
CommandInterpreter::executeShow(char* parameters) 
{ 
  int i;
  if (emptyString(parameters)) {
    ndb_mgm_cluster_state *state = ndb_mgm_get_status(m_mgmsrv);
    if(state == NULL) {
      ndbout_c("Could not get status");
      printError();
      return -1;
    }
    NdbAutoPtr<char> ap1((char*)state);

    ndb_mgm_configuration * conf = ndb_mgm_get_configuration(m_mgmsrv,0);
    if(conf == 0){
      ndbout_c("Could not get configuration");
      printError();
      return -1;
    }

    ndb_mgm_configuration_iterator * it;
    it = ndb_mgm_create_configuration_iterator((struct ndb_mgm_configuration *)conf, CFG_SECTION_NODE);

    if(it == 0){
      ndbout_c("Unable to create config iterator");
      ndb_mgm_destroy_configuration(conf);
      return -1;
    }
    NdbAutoPtr<ndb_mgm_configuration_iterator> ptr(it);

    int
      master_id= 0,
      ndb_nodes= 0,
      api_nodes= 0,
      mgm_nodes= 0;

    for(i=0; i < state->no_of_nodes; i++) {
      if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_NDB &&
	 state->node_states[i].version != 0){
	master_id= state->node_states[i].dynamic_id;
	break;
      }
    }
    
    for(i=0; i < state->no_of_nodes; i++) {
      switch(state->node_states[i].node_type) {
      case NDB_MGM_NODE_TYPE_API:
	api_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_NDB:
	if (state->node_states[i].dynamic_id &&
	    state->node_states[i].dynamic_id < master_id)
	  master_id= state->node_states[i].dynamic_id;
	ndb_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_MGM:
	mgm_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_UNKNOWN:
        ndbout << "Error: Unknown Node Type" << endl;
        return -1;
      case NDB_MGM_NODE_TYPE_MAX:
        break;                                  /* purify: deadcode */
      }
    }

    ndbout << "Cluster Configuration" << endl
	   << "---------------------" << endl;
    print_nodes(state, it, "ndbd",     ndb_nodes, NDB_MGM_NODE_TYPE_NDB, master_id);
    print_nodes(state, it, "ndb_mgmd", mgm_nodes, NDB_MGM_NODE_TYPE_MGM, 0);
    print_nodes(state, it, "mysqld",   api_nodes, NDB_MGM_NODE_TYPE_API, 0);
    ndb_mgm_destroy_configuration(conf);
    return 0;
  } else {
    ndbout << "Invalid argument: '" << parameters << "'" << endl;
    return -1;
  }
  return 0;
}

int
CommandInterpreter::executeConnect(char* parameters, bool interactive) 
{
  BaseString *basestring = NULL;

  disconnect();
  if (!emptyString(parameters)) {
    basestring= new BaseString(parameters);
    m_constr= basestring->trim().c_str();
  }
  if ( connect(interactive) == false ){
    return -1;
  }
  if (basestring != NULL)
    delete basestring;

  return 0;
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeClusterLog(char* parameters) 
{
  DBUG_ENTER("CommandInterpreter::executeClusterLog");
  int i;
  if (emptyString(parameters))
  {
    ndbout_c("ERROR: Missing argument(s).");
    m_error = -1;
    DBUG_VOID_RETURN;
  }

  enum ndb_mgm_event_severity severity = NDB_MGM_EVENT_SEVERITY_ALL;
    
  char * tmpString = strdup(parameters);
  if (tmpString == NULL)
  {
    ndbout_c("ERROR: Memory allocation error at %s:%d.", __FILE__, __LINE__);
    m_error = -1;
    DBUG_VOID_RETURN;
  }

  NdbAutoPtr<char> ap1(tmpString);
  char * tmpPtr = 0;
  char * item = my_strtok_r(tmpString, " ", &tmpPtr);
  int enable;

  ndb_mgm_severity enabled[NDB_MGM_EVENT_SEVERITY_ALL] = 
    {{NDB_MGM_EVENT_SEVERITY_ON,0},
     {NDB_MGM_EVENT_SEVERITY_DEBUG,0},
     {NDB_MGM_EVENT_SEVERITY_INFO,0},
     {NDB_MGM_EVENT_SEVERITY_WARNING,0},
     {NDB_MGM_EVENT_SEVERITY_ERROR,0},
     {NDB_MGM_EVENT_SEVERITY_CRITICAL,0},
     {NDB_MGM_EVENT_SEVERITY_ALERT,0}};
  int returned = ndb_mgm_get_clusterlog_severity_filter(m_mgmsrv,
                                                  &enabled[0],
                                                  NDB_MGM_EVENT_SEVERITY_ALL);
  if(returned != NDB_MGM_EVENT_SEVERITY_ALL) {
    ndbout << "Couldn't get status" << endl;
    printError();
    m_error = -1;
    DBUG_VOID_RETURN;
  }

  /********************
   * CLUSTERLOG INFO
   ********************/
  if (native_strcasecmp(item, "INFO") == 0) {
    DBUG_PRINT("info",("INFO"));
    if(enabled[0].value == 0)
    {
      ndbout << "Cluster logging is disabled." << endl;
      m_error = 0;
      DBUG_VOID_RETURN;
    }
#if 0 
    for(i = 0; i<DB_MGM_EVENT_SEVERITY_ALL;i++)
      printf("enabled[%d] = %d\n", i, enabled[i].value);
#endif
    ndbout << "Severities enabled: ";
    for(i = 1; i < (int)NDB_MGM_EVENT_SEVERITY_ALL; i++) {
      const char *str= ndb_mgm_get_event_severity_string(enabled[i].category);
      if (str == 0)
      {
	DBUG_ASSERT(false);
	continue;
      }
      if(enabled[i].value)
	ndbout << BaseString(str).ndb_toupper() << " ";
    }
    ndbout << endl;
    m_error = 0;
    DBUG_VOID_RETURN;

  } 
  else if (native_strcasecmp(item, "FILTER") == 0 ||
	   native_strcasecmp(item, "TOGGLE") == 0)
  {
    DBUG_PRINT("info",("TOGGLE"));
    enable= -1;
  } 
  else if (native_strcasecmp(item, "OFF") == 0) 
  {
    DBUG_PRINT("info",("OFF"));
    enable= 0;
  } else if (native_strcasecmp(item, "ON") == 0) {
    DBUG_PRINT("info",("ON"));
    enable= 1;
  } else {
    ndbout << "Invalid argument." << endl;
    m_error = -1;
    DBUG_VOID_RETURN;
  }

  int res_enable;
  item = my_strtok_r(NULL, " ", &tmpPtr);
  if (item == NULL) {
    res_enable=
      ndb_mgm_set_clusterlog_severity_filter(m_mgmsrv,
					     NDB_MGM_EVENT_SEVERITY_ON,
					     enable, NULL);
    if (res_enable < 0)
    {
      ndbout << "Couldn't set filter" << endl;
      printError();
      m_error = -1;
      DBUG_VOID_RETURN;
    }
    ndbout << "Cluster logging is " << (res_enable ? "enabled.":"disabled") << endl;
    m_error = 0;
    DBUG_VOID_RETURN;
  }

  do {
    severity= NDB_MGM_ILLEGAL_EVENT_SEVERITY;
    if (native_strcasecmp(item, "ALL") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ALL;	
    } else if (native_strcasecmp(item, "ALERT") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ALERT;
    } else if (native_strcasecmp(item, "CRITICAL") == 0) { 
      severity = NDB_MGM_EVENT_SEVERITY_CRITICAL;
    } else if (native_strcasecmp(item, "ERROR") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ERROR;
    } else if (native_strcasecmp(item, "WARNING") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_WARNING;
    } else if (native_strcasecmp(item, "INFO") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_INFO;
    } else if (native_strcasecmp(item, "DEBUG") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_DEBUG;
    } else if (native_strcasecmp(item, "OFF") == 0 ||
	       native_strcasecmp(item, "ON") == 0) {
      if (enable < 0) // only makes sense with toggle
	severity = NDB_MGM_EVENT_SEVERITY_ON;
    }
    if (severity == NDB_MGM_ILLEGAL_EVENT_SEVERITY) {
      ndbout << "Invalid severity level: " << item << endl;
      m_error = -1;
      DBUG_VOID_RETURN;
    }

    res_enable= ndb_mgm_set_clusterlog_severity_filter(m_mgmsrv, severity,
						       enable, NULL);
    if (res_enable < 0)
    {
      ndbout << "Couldn't set filter" << endl;
      printError();
      m_error = -1;
      DBUG_VOID_RETURN;
    }
    ndbout << BaseString(item).ndb_toupper().c_str() << " " << (res_enable ? "enabled":"disabled") << endl;

    item = my_strtok_r(NULL, " ", &tmpPtr);	
  } while(item != NULL);
  
  m_error = 0;
  DBUG_VOID_RETURN;
} 

//*****************************************************************************
//*****************************************************************************

int
CommandInterpreter::executeStop(int processId, const char *parameters,
                                bool all) 
{
  Vector<BaseString> command_list;
  if (parameters)
    split_args(parameters, command_list);

  int retval;
  if (all)
    retval = executeStop(command_list, 0, 0, 0);
  else
    retval = executeStop(command_list, 0, &processId, 1);

  return retval;
}

int
CommandInterpreter::executeStop(Vector<BaseString> &command_list,
                                unsigned command_pos,
                                int *node_ids, int no_of_nodes)
{
  int need_disconnect;
  int abort= 0;
  int retval = 0;
  int force = 0;

  for (; command_pos < command_list.size(); command_pos++)
  {
    const char *item= command_list[command_pos].c_str();
    if (native_strcasecmp(item, "-A") == 0)
    {
      abort= 1;
      continue;
    }
    if (native_strcasecmp(item, "-F") == 0)
    {
      force = 1;
      continue;
    }
    ndbout_c("Invalid option: %s. Expecting -A or -F after STOP",
             item);
    return -1;
  }

  int result= ndb_mgm_stop4(m_mgmsrv, no_of_nodes, node_ids, abort,
                            force, &need_disconnect);
  if (result < 0)
  {
    ndbout_c("Shutdown failed.");
    printError();
    retval = -1;
  }
  else
  {
    if (node_ids == 0)
      ndbout_c("NDB Cluster has shutdown.");
    else
    {
      ndbout << "Node";
      for (int i= 0; i < no_of_nodes; i++)
          ndbout << " " << node_ids[i];
      ndbout_c(" has shutdown.");
    }
  }

  if(need_disconnect)
  {
    ndbout << "Disconnecting to allow Management Server to shutdown" << endl;
    disconnect();
  }

  return retval;
}

int
CommandInterpreter::executeEnterSingleUser(char* parameters) 
{
  strtok(parameters, " ");
  struct ndb_mgm_reply reply;
  char* id = strtok(NULL, " ");
  id = strtok(NULL, " ");
  id = strtok(NULL, "\0");
  int nodeId = -1;
  if(id == 0 || sscanf(id, "%d", &nodeId) != 1){
    ndbout_c("Invalid arguments: expected <NodeId>");
    ndbout_c("Use SHOW to see what API nodes are configured");
    return -1;
  }
  int result = ndb_mgm_enter_single_user(m_mgmsrv, nodeId, &reply);
  
  if (result != 0) {
    ndbout_c("Entering single user mode for node %d failed", nodeId);
    printError();
    return -1;
  } else {
    ndbout_c("Single user mode entered");
    ndbout_c("Access is granted for API node %d only.", nodeId);
  }
  return 0;
}

int
CommandInterpreter::executeExitSingleUser(char* parameters) 
{
  int result = ndb_mgm_exit_single_user(m_mgmsrv, 0);
  if (result != 0) {
    ndbout_c("Exiting single user mode failed.");
    printError();
    return -1;
  } else {
    ndbout_c("Exiting single user mode in progress.");
    ndbout_c("Use ALL STATUS or SHOW to see when single user mode has been exited.");
    return 0;
  }
}

int
CommandInterpreter::executeStart(int processId, const char* parameters,
				 bool all) 
{
  int result;
  int retval = 0;
  if(all) {
    result = ndb_mgm_start(m_mgmsrv, 0, 0);
  } else {
    result = ndb_mgm_start(m_mgmsrv, 1, &processId);
  }

  if (result <= 0) {
    ndbout << "Start failed." << endl;
    printError();
    retval = -1;
  } else
    {
      if(all)
	ndbout_c("NDB Cluster is being started.");
      else
	ndbout_c("Database node %d is being started.", processId);
    }
  return retval;
}

int
CommandInterpreter::executeStart(Vector<BaseString> &command_list,
                                 unsigned command_pos,
                                 int *node_ids, int no_of_nodes)
{
  int result;
  result= ndb_mgm_start(m_mgmsrv, no_of_nodes, node_ids);

  if (result <= 0) {
    ndbout_c("Start failed.");
    printError();
    return -1;
  }
  else
  {
    ndbout << "Node";
    for (int i= 0; i < no_of_nodes; i++)
      ndbout << " " << node_ids[i];
    ndbout_c(" is being started");
  }
  return 0;
}

int
CommandInterpreter::executeRestart(int processId, const char* parameters,
				   bool all)
{
  Vector<BaseString> command_list;
  if (parameters)
    split_args(parameters, command_list);

  int retval;
  if (all)
    retval = executeRestart(command_list, 0, 0, 0);
  else
    retval = executeRestart(command_list, 0, &processId, 1);

  return retval;
}

int
CommandInterpreter::executeRestart(Vector<BaseString> &command_list,
                                   unsigned command_pos,
                                   int *node_ids, int no_of_nodes)
{
  int result;
  int retval = 0;
  int nostart= 0;
  int initialstart= 0;
  int abort= 0;
  int need_disconnect= 0;
  int force = 0;

  for (; command_pos < command_list.size(); command_pos++)
  {
    const char *item= command_list[command_pos].c_str();
    if (native_strcasecmp(item, "-N") == 0)
    {
      nostart= 1;
      continue;
    }
    if (native_strcasecmp(item, "-I") == 0)
    {
      initialstart= 1;
      continue;
    }
    if (native_strcasecmp(item, "-A") == 0)
    {
      abort= 1;
      continue;
    }
    if (native_strcasecmp(item, "-F") == 0)
    {
      force = 1;
      continue;
    }
    ndbout_c("Invalid option: %s. Expecting -A,-N,-I or -F after RESTART",
             item);
    return -1;
  }

  struct ndb_mgm_cluster_state *cl = ndb_mgm_get_status(m_mgmsrv);
  if(cl == NULL)
  {
    ndbout_c("Could not get status");
    printError();
    return -1;
  }
  NdbAutoPtr<char> ap1((char*)cl);

  // We allow 'all restart' in single user mode
  if(node_ids != 0) {
    for (int i = 0; i<cl->no_of_nodes; i++) {
      if((cl->node_states+i)->node_status == NDB_MGM_NODE_STATUS_SINGLEUSER)
      {
        ndbout_c("Cannot restart nodes: single user mode");
        return -1;
      }
    }
  }

  if (node_ids == 0) {
    ndbout_c("Executing RESTART on all nodes.");
    ndbout_c("Starting shutdown. This may take a while. Please wait...");
  }

  for (int i= 0; i < no_of_nodes; i++)
  {
    int j = 0;
    while((j < cl->no_of_nodes) && cl->node_states[j].node_id != node_ids[i])
      j++;

    if(cl->node_states[j].node_id != node_ids[i])
    {
      ndbout << node_ids[i] << ": Node not found" << endl;
      return -1;
    }

    if(cl->node_states[j].node_type == NDB_MGM_NODE_TYPE_MGM)
    {
      ndbout << "Shutting down MGM node"
	     << " " << node_ids[i] << " for restart" << endl;
    }
  }

  result= ndb_mgm_restart4(m_mgmsrv, no_of_nodes, node_ids,
                           initialstart, nostart, abort, force,
                           &need_disconnect);

  if (result <= 0) {
    ndbout_c("Restart failed.");
    printError();
    retval = -1;
  }
  else
  {
    if (node_ids == 0)
      ndbout_c("All DB nodes are being restarted.");
    else
    {
      ndbout << "Node";
      for (int i= 0; i < no_of_nodes; i++)
        ndbout << " " << node_ids[i];
      ndbout_c(" is being restarted");
    }
    if(need_disconnect)
      disconnect();
  }
  return retval;
}

/**
 * print status of one node
 */
static
void
print_status(const ndb_mgm_node_state * state)
{
  Uint32 version = state->version;
  if (state->node_type != NDB_MGM_NODE_TYPE_NDB)
  {
    if (version != 0)
    {
      ndbout << "Node " << state->node_id <<": connected" ;
      ndbout_c(" (Version %d.%d.%d)",
               getMajor(version) ,
               getMinor(version),
               getBuild(version));
      
    }
    else
    {
      ndbout << "Node " << state->node_id << ": not connected" << endl;
    }
    return;
  }
  
  ndbout << "Node " << state->node_id 
         << ": " << status_string(state->node_status);
  switch(state->node_status){
  case NDB_MGM_NODE_STATUS_STARTING:
    ndbout << " (Last completed phase " << state->start_phase << ")";
    break;
  case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
    ndbout << " (Last completed phase " << state->start_phase << ")";
    break;
  default:
    break;
  }

  if(state->node_status != NDB_MGM_NODE_STATUS_NO_CONTACT)
  {
    char tmp[100];
    ndbout_c(" (%s)", ndbGetVersionString(version, 
                                          state->mysql_version, 0, 
					  tmp, sizeof(tmp))); 
  }
  else
  {
    ndbout << endl;
  }
}

int
CommandInterpreter::executeStatus(int processId, 
				  const char* parameters, bool all) 
{
  if (! emptyString(parameters)) {
    ndbout_c("No parameters expected to this command.");
    return -1;
  }

  ndb_mgm_node_type types[2] = {
    NDB_MGM_NODE_TYPE_NDB,
    NDB_MGM_NODE_TYPE_UNKNOWN
  };
  struct ndb_mgm_cluster_state *cl;
  cl = ndb_mgm_get_status2(m_mgmsrv, all ? types : 0);
  if(cl == NULL) 
  {
    ndbout_c("Can't get status of node %d.", processId);
    printError();
    return -1;
  }
  NdbAutoPtr<char> ap1((char*)cl);

  if (all)
  {
    for (int i = 0; i<cl->no_of_nodes; i++)
      print_status(cl->node_states+i);
    return 0;
  }
  else
  {
    for (int i = 0; i<cl->no_of_nodes; i++)
    {
      if (cl->node_states[i].node_id == processId)
      {
        print_status(cl->node_states + i);
        return 0;
      }
    }
    ndbout << processId << ": Node not found" << endl;
    return -1;
  }
  return 0;
} //

int
CommandInterpreter::executeDumpState(int processId, const char* parameters,
				     bool all) 
{
  if(emptyString(parameters))
  {
    ndbout_c("ERROR: Expected argument!");
    return -1;
  }

  int params[25];
  int num_params = 0;
  const size_t max_params = sizeof(params)/sizeof(params[0]);

  Vector<BaseString> args;
  split_args(parameters, args);

  if (args.size() > max_params)
  {
    ndbout_c("ERROR: Too many arguments, max %d allowed", (int)max_params);
    return -1;
  }

  for (unsigned i = 0; i < args.size(); i++)
  {
    const char* arg = args[i].c_str();

    if (my_strtoll(arg, NULL, 0) < 0 ||
        my_strtoll(arg, NULL, 0) > 0xffffffff)
    {
      ndbout_c("ERROR: Illegal value '%s' in argument to signal.\n"
               "(Value must be between 0 and 0xffffffff.)", arg);
      return -1;
    }
    assert(num_params < (int)max_params);
    params[num_params] = (int)my_strtoll(arg, NULL, 0);
    num_params++;
  }

  ndbout << "Sending dump signal with data:" << endl;
  for (int i = 0; i < num_params; i++)
  {
    ndbout.setHexFormat(1) << params[i] << " ";
    if (!((i+1) & 0x3)) ndbout << endl;
  }
  ndbout << endl;

  struct ndb_mgm_reply reply;
  return ndb_mgm_dump_state(m_mgmsrv, processId, params, num_params, &reply);
}

static void
report_memoryusage(const ndb_logevent& event)
{
  const ndb_logevent_MemoryUsage& usage = event.MemoryUsage;
  const Uint32 block = usage.block;
  const Uint32 total = usage.pages_total;
  const Uint32 used = usage.pages_used;
  assert(event.type == NDB_LE_MemoryUsage);

  ndbout_c("Node %u: %s usage is %d%%(%d %dK pages of total %d)",
           event.source_nodeid,
           (block == DBACC ? "Index" : (block == DBTUP ? "Data" : "<unknown>")),
           (total ? (used * 100 / total) : 0),
           used,
           usage.page_size_kb/1024,
           total);
}


static void
report_backupstatus(const ndb_logevent& event)
{
  const ndb_logevent_BackupStatus& status = event.BackupStatus;
  assert(event.type == NDB_LE_BackupStatus);


  if (status.starting_node)
    ndbout_c("Node %u: Local backup status: backup %u started from node %u\n"
             " #Records: %llu #LogRecords: %llu\n"
             " Data: %llu bytes Log: %llu bytes",
             event.source_nodeid,
             status.backup_id,
             refToNode(status.starting_node),
             make_uint64(status.n_records_lo, status.n_records_hi),
             make_uint64(status.n_log_records_lo, status.n_log_records_hi),
             make_uint64(status.n_bytes_lo, status.n_bytes_hi),
             make_uint64(status.n_log_bytes_lo, status.n_log_bytes_hi));
  else
    ndbout_c("Node %u: Backup not started",
             event.source_nodeid);
}

static
void
report_events(const ndb_logevent& event)
{
  Uint32 threshold = 0;
  Logger::LoggerLevel severity = Logger::LL_WARNING;
  LogLevel::EventCategory cat= LogLevel::llInvalid;
  EventLogger::EventTextFunction textF;

  const EventReport * real_event = (const EventReport*)event.SavedEvent.data;
  Uint32 type = real_event->getEventType();

  if (EventLoggerBase::event_lookup(type,cat,threshold,severity,textF))
    return;

  char out[1024];
  Uint32 pos = 0;
  if (event.source_nodeid != 0)
  {
    BaseString::snprintf(out, sizeof(out), "Node %u: ", event.source_nodeid);
    pos= (Uint32)strlen(out);
  }
  textF(out+pos, sizeof(out)-pos, event.SavedEvent.data, event.SavedEvent.len);

  char timestamp_str[64];
  Logger::format_timestamp(event.SavedEvent.time, timestamp_str, sizeof(timestamp_str));

  ndbout_c("%s %s", timestamp_str, out);
}

static int
sort_log(const void *_a, const void *_b)
{
  const ndb_logevent * a = (const ndb_logevent*)_a;
  const ndb_logevent * b = (const ndb_logevent*)_b;

  if (a->source_nodeid == b->source_nodeid)
  {
    return a->SavedEvent.seq - b->SavedEvent.seq;
  }

  if (a->SavedEvent.time < b->SavedEvent.time)
    return -1;
  if (a->SavedEvent.time > b->SavedEvent.time)
    return 1;

  if (a->SavedEvent.seq < b->SavedEvent.seq)
    return -1;
  if (a->SavedEvent.seq > b->SavedEvent.seq)
    return 1;

  return (a->source_nodeid - b->source_nodeid);
}

static const
struct st_report_cmd {
  const char *name;
  const char *help;
  Ndb_logevent_type type;
  void (*print_event_fn)(const ndb_logevent&);
  int (* sort_fn)(const void *_a, const void *_b);
} report_cmds[] = {

  { "BackupStatus",
    "Report backup status of respective node",
    NDB_LE_BackupStatus,
    report_backupstatus, 0 },

  { "MemoryUsage",
    "Report memory usage of respective node",
    NDB_LE_MemoryUsage,
    report_memoryusage, 0 },

  { "EventLog",
    "Report events in datanodes circular event log buffer",
    NDB_LE_SavedEvent,
    report_events, sort_log },

  { 0, 0, NDB_LE_ILLEGAL_TYPE, 0, 0 }
};


int
CommandInterpreter::executeReport(int nodeid, const char* parameters,
                                  bool all) 
{
  if (emptyString(parameters))
  {
    ndbout_c("ERROR: missing report type specifier!");
    return -1;
  }

  Vector<BaseString> args;
  split_args(parameters, args);

  const st_report_cmd* report_cmd = report_cmds;
  for (; report_cmd->name; report_cmd++)
  {
    if (native_strncasecmp(report_cmd->name, args[0].c_str(),
                    args[0].length()) == 0)
      break;
  }

  if (!report_cmd->name)
  {
    ndbout_c("ERROR: '%s' - report type specifier unknown!", args[0].c_str());
    return -1;
  }

  if (!all)
  {
    ClusterInfo info;
    if (!info.fetch(m_mgmsrv))
    {
      printError();
      return -1;
    }

    // Check that given nodeid is a NDB node
    if (!info.is_ndb_node(nodeid))
      return -1;
  }

  struct ndb_mgm_events* events =
    ndb_mgm_dump_events(m_mgmsrv, report_cmd->type,
                        all ? 0 : 1, &nodeid);
  if (!events)
  {
    ndbout_c("ERROR: failed to fetch report!");
    printError();
    return -1;
  }

  if (report_cmd->sort_fn)
  {
    qsort(events->events, events->no_of_events,
          sizeof(events->events[0]), report_cmd->sort_fn);
  }

  for (int i = 0; i < events->no_of_events; i++)
  {
    const ndb_logevent& event = events->events[i];
    report_cmd->print_event_fn(event);
  }

  free(events);
  return 0;
}


static void
helpTextReportFn()
{
  ndbout_c("  <report-type> =");
  const st_report_cmd* report_cmd = report_cmds;
  for (; report_cmd->name; report_cmd++)
    ndbout_c("    %s\t- %s", report_cmd->name, report_cmd->help);
}

static void 
helpTextReportTypeOptionFn()
{
  ndbout << "<report-type> = ";
  const st_report_cmd* report_cmd = report_cmds;
  for (; report_cmd->name; report_cmd++){
    if (report_cmd != report_cmds)
      ndbout << " | ";
    ndbout << BaseString(report_cmd->name).ndb_toupper().c_str();
  }
  ndbout << endl;
}

//*****************************************************************************
//*****************************************************************************

int
CommandInterpreter::executeLogLevel(int processId, const char* parameters, 
				    bool all) 
{
  (void) all;
  if (emptyString(parameters)) {
    ndbout << "Expected argument" << endl;
    return -1;
  } 
  BaseString tmp(parameters);
  Vector<BaseString> spec;
  tmp.split(spec, "=");
  if(spec.size() != 2){
    ndbout << "Invalid loglevel specification: " << parameters << endl;
    return -1;
  }

  spec[0].trim().ndb_toupper();
  int category = ndb_mgm_match_event_category(spec[0].c_str());
  if(category == NDB_MGM_ILLEGAL_EVENT_CATEGORY){
    category = atoi(spec[0].c_str());
    if(category < NDB_MGM_MIN_EVENT_CATEGORY ||
       category > NDB_MGM_MAX_EVENT_CATEGORY){
      ndbout << "Unknown category: \"" << spec[0].c_str() << "\"" << endl;
      return -1;
    }
  }
  
  int level = atoi(spec[1].c_str());
  if(level < 0 || level > 15){
    ndbout << "Invalid level: " << spec[1].c_str() << endl;
    return -1;
  }
  
  ndbout << "Executing LOGLEVEL on node " << processId << flush;

  struct ndb_mgm_reply reply;
  int result;
  result = ndb_mgm_set_loglevel_node(m_mgmsrv, 
				     processId,
				     (ndb_mgm_event_category)category,
				     level, 
				     &reply);
  
  if (result < 0) {
    ndbout_c(" failed.");
    printError();
    return -1;
  } else {
    ndbout_c(" OK!");
  }  
  return 0;
}

//*****************************************************************************
//*****************************************************************************
int CommandInterpreter::executeError(int processId, 
				      const char* parameters, bool /* all */) 
{
  if (emptyString(parameters))
  {
    ndbout_c("ERROR: Missing error number.");
    return -1;
  }

  Vector<BaseString> args;
  split_args(parameters, args);

  if (args.size() >= 2)
  {
    ndbout << "ERROR: Too many arguments." << endl;
    return -1;
  }

  int errorNo;
  if (! convert(args[0].c_str(), errorNo)) {
    ndbout << "ERROR: Expected an integer." << endl;
    return -1;
  }

  return ndb_mgm_insert_error(m_mgmsrv, processId, errorNo, NULL);
}

//*****************************************************************************
//*****************************************************************************

int
CommandInterpreter::executeLog(int processId,
			       const char* parameters, bool all) 
{
  struct ndb_mgm_reply reply;
  Vector<BaseString> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return -1;
  }

  BaseString block_names;
  for (unsigned i = 0; i<blocks.size(); i++)
    block_names.appfmt("%s|", blocks[i].c_str());

  int result = ndb_mgm_log_signals(m_mgmsrv,
				   processId, 
				   NDB_MGM_SIGNAL_LOG_MODE_INOUT, 
				   block_names.c_str(),
				   &reply);
  if (result != 0) {
    ndbout_c("Execute LOG on node %d failed.", processId);
    printError();
    return -1;
  }
  return 0;
}


//*****************************************************************************
//*****************************************************************************
int
CommandInterpreter::executeTestOn(int processId,
				  const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return -1;
  }
  struct ndb_mgm_reply reply;
  int result = ndb_mgm_start_signallog(m_mgmsrv, processId, &reply);
  if (result != 0) {
    ndbout_c("Execute TESTON failed.");
    printError();
    return -1;
  }
  return 0;
}

//*****************************************************************************
//*****************************************************************************
int
CommandInterpreter::executeTestOff(int processId,
				   const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return -1;
  }
  struct ndb_mgm_reply reply;
  int result = ndb_mgm_stop_signallog(m_mgmsrv, processId, &reply);
  if (result != 0) {
    ndbout_c("Execute TESTOFF failed.");
    printError();
    return -1;
  }
  return 0;
}


//*****************************************************************************
//*****************************************************************************

int
CommandInterpreter::executeEventReporting(int processId,
					  const char* parameters, 
					  bool all) 
{
  int retval = 0;
  if (emptyString(parameters)) {
    ndbout << "Expected argument" << endl;
    return -1;
  }

  Vector<BaseString> specs;
  split_args(parameters, specs);

  for (int i=0; i < (int) specs.size(); i++)
  {
    Vector<BaseString> spec;
    specs[i].split(spec, "=");
    if(spec.size() != 2){
      ndbout << "Invalid loglevel specification: " << specs[i] << endl;
      continue;
    }

    spec[0].trim().ndb_toupper();
    int category = ndb_mgm_match_event_category(spec[0].c_str());
    if(category == NDB_MGM_ILLEGAL_EVENT_CATEGORY){
      if(!convert(spec[0].c_str(), category) ||
	 category < NDB_MGM_MIN_EVENT_CATEGORY ||
	 category > NDB_MGM_MAX_EVENT_CATEGORY){
	ndbout << "Unknown category: \"" << spec[0].c_str() << "\"" << endl;
	continue;
      }
    }

    int level;
    if (!convert(spec[1].c_str(),level))
    {
      ndbout << "Invalid level: " << spec[1].c_str() << endl;
      continue;
    }

    ndbout << "Executing CLUSTERLOG " << spec[0] << "=" << spec[1]
	   << " on node " << processId << flush;

    struct ndb_mgm_reply reply;
    int result;
    result = ndb_mgm_set_loglevel_clusterlog(m_mgmsrv, 
					     processId,
					     (ndb_mgm_event_category)category,
					     level, 
					     &reply);
  
    if (result != 0) {
      ndbout_c(" failed."); 
      printError();
      retval = -1;
    } else {
      ndbout_c(" OK!"); 
    }
  }
  return retval;
}


/*****************************************************************************
 * Backup
 *****************************************************************************/
int
CommandInterpreter::executeStartBackup(char* parameters, bool interactive)
{
  struct ndb_mgm_reply reply;
  unsigned int backupId;
  unsigned int input_backupId = 0;
  unsigned long long int tmp_backupId = 0;

  Vector<BaseString> args;
  if (parameters)
    split_args(parameters, args);

  for (unsigned i= 0; i < args.size(); i++)
    args[i].ndb_toupper();

  int sz= args.size();

  int result;
  int flags = 2;
  //1,snapshot at start time. 0 snapshot at end time
  unsigned int backuppoint = 0;
  bool b_log = false;
  bool b_nowait = false;
  bool b_wait_completed = false;
  bool b_wait_started = false;

  /*
   All the commands list as follow:
   start backup <backupid> nowait | start backup <backupid> snapshotstart/snapshotend nowati | start backup <backupid> nowait snapshotstart/snapshotend
   start backup <backupid> | start backup <backupid> wait completed | start backup <backupid> snapshotstart/snapshotend
   start backup <backupid> snapshotstart/snapshotend wait completed | start backup <backupid> wait completed snapshotstart/snapshotend
   start backup <backupid> wait started | start backup <backupid> snapshotstart/snapshotend wait started
   start backup <backupid> wait started snapshotstart/snapshotend
  */
  for (int i= 1; i < sz; i++)
  {
    if (i == 1 && sscanf(args[1].c_str(), "%llu", &tmp_backupId) == 1) {
      char out[1024];
      BaseString::snprintf(out, sizeof(out), "%u: ", MAX_BACKUPS);
      // to detect wraparound due to overflow, check if number of digits in 
      // input backup ID <= number of digits in max backup ID
      if (tmp_backupId > 0 && tmp_backupId < MAX_BACKUPS && args[1].length() <= strlen(out)) {
        input_backupId = static_cast<unsigned>(tmp_backupId);
        continue;
      } else {
        BaseString::snprintf(out, sizeof(out), "Backup ID out of range [1 - %u]", MAX_BACKUPS-1);
        invalid_command(parameters, out);
        return -1;
      }
    }

    if (args[i] == "SNAPSHOTEND") {
      if (b_log ==true) {
        invalid_command(parameters);
        return -1;
      }
      b_log = true;
      backuppoint = 0;
      continue;
    }
    if (args[i] == "SNAPSHOTSTART") {
      if (b_log ==true) {
        invalid_command(parameters);
        return -1;
      }
      b_log = true;
      backuppoint = 1;
      continue;
    }
    if (args[i] == "NOWAIT") {
      if (b_nowait == true || b_wait_completed == true || b_wait_started ==true) {
        invalid_command(parameters);
        return -1;
      }
      b_nowait = true;
      flags = 0;
      continue;
    }
    if (args[i] == "WAIT") {
      if (b_nowait == true || b_wait_completed == true || b_wait_started ==true) {
        invalid_command(parameters);
        return -1;
      }
      if (i+1 < sz) {
        if (args[i+1] == "COMPLETED") {
          b_wait_completed = true;
          flags = 2; 
          i++;
        }
        else if (args[i+1] == "STARTED") {
          b_wait_started = true;
          flags = 1;
          i++;
        }
        else {
          invalid_command(parameters);
          return -1;
        }
      }
      else {
        invalid_command(parameters);
        return -1;
      }
      continue;
    }
    invalid_command(parameters);
    return -1;
  }

  //print message
  if (flags == 2)
    ndbout_c("Waiting for completed, this may take several minutes");
  if (flags == 1)
    ndbout_c("Waiting for started, this may take several minutes");

  NdbLogEventHandle log_handle= NULL;
  struct ndb_logevent log_event;
  if (flags > 0 && !interactive)
  {
    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP, 0, 0 };
    log_handle = ndb_mgm_create_logevent_handle(m_mgmsrv, filter);
    if (!log_handle)
    {
      ndbout << "Initializing start of backup failed" << endl;
      printError();
      return -1;
    }
  }

  //start backup N | start backup snapshotstart/snapshotend
  if (input_backupId > 0 || b_log == true)
    result = ndb_mgm_start_backup3(m_mgmsrv, flags, &backupId, &reply, input_backupId, backuppoint);
  //start backup
  else
    result = ndb_mgm_start_backup(m_mgmsrv, flags, &backupId, &reply);

  if (result != 0) {
    ndbout << "Backup failed" << endl;
    printError();

    if (log_handle) 
      ndb_mgm_destroy_logevent_handle(&log_handle);
    return result;
  }

  /**
   * If interactive, event listner thread is already running
   */
  if (log_handle && !interactive)
  {
    int count = 0;
    int retry = 0;
    int res;
    do {
      if ((res= ndb_logevent_get_next(log_handle, &log_event, 60000)) > 0)
      {
        int print = 0;
        switch (log_event.type) {
          case NDB_LE_BackupStarted:
            if (log_event.BackupStarted.backup_id == backupId)
              print = 1;
            break;
          case NDB_LE_BackupCompleted:
            if (log_event.BackupCompleted.backup_id == backupId)
              print = 1;
            break;
          case NDB_LE_BackupAborted:
            if (log_event.BackupAborted.backup_id == backupId)
              print = 1;
            break;
          default:
            break;
        }
        if (print)
        {
          Guard g(m_print_mutex);
          printLogEvent(&log_event);
          count++;
          // for WAIT STARTED, exit after printing "Backup started" logevent
          if(flags == 1 && log_event.type == NDB_LE_BackupStarted) 
          {
            ndb_mgm_destroy_logevent_handle(&log_handle);
            return 0;
          }
        }
      }
      else
      {
        retry++;
      }
    } while(res >= 0 && count < 2 && retry < 3);

    if (retry >= 3)
      ndbout << "get backup event failed for " << retry << " times" << endl;

    ndb_mgm_destroy_logevent_handle(&log_handle);
  }

  return 0;
}

int
CommandInterpreter::executeAbortBackup(char* parameters) 
{
  unsigned int bid = 0;
  unsigned long long int tmp_bid = 0;
  struct ndb_mgm_reply reply;
  if (emptyString(parameters))
    goto executeAbortBackupError1;

  {
    strtok(parameters, " ");
    char* id = strtok(NULL, "\0");
    if(id == 0 || sscanf(id, "%llu", &tmp_bid) != 1) 
      goto executeAbortBackupError1;

    // to detect wraparound due to overflow, check if number of digits in 
    // input backup ID > number of digits in max backup ID
    char out[1024];
    BaseString::snprintf(out, sizeof(out), "%u", MAX_BACKUPS);
    if(tmp_bid <= 0 || tmp_bid >= MAX_BACKUPS || strlen(id) > strlen(out))
      goto executeAbortBackupError2;
    else 
      bid = static_cast<unsigned>(tmp_bid);
  }
  {
    int result= ndb_mgm_abort_backup(m_mgmsrv, bid, &reply);
    if (result != 0) {
      ndbout << "Abort of backup " << bid << " failed" << endl;
      printError();
      return -1;
    } else {
      ndbout << "Abort of backup " << bid << " ordered" << endl;
    }
  }
  return 0;
 executeAbortBackupError1:
  ndbout << "Invalid arguments: expected <BackupId>" << endl;
  return -1;
 executeAbortBackupError2:
  ndbout << "Invalid arguments: <BackupId> out of range [1-" << MAX_BACKUPS-1 << "]" << endl;
  return -1;
}

int
CommandInterpreter::executeCreateNodeGroup(char* parameters)
{
  char *id= strchr(parameters, ' ');
  if (emptyString(id))
    goto err;

  {
    Vector<int> nodes;
    BaseString args(id);
    Vector<BaseString> nodelist;
    args.split(nodelist, ",");

    for (Uint32 i = 0; i<nodelist.size(); i++)
    {
      nodes.push_back(atoi(nodelist[i].c_str()));
    }
    nodes.push_back(0);

    int ng;
    struct ndb_mgm_reply reply;
    const int result= ndb_mgm_create_nodegroup(m_mgmsrv, nodes.getBase(),
                                               &ng, &reply);
    if (result != 0) {
      printError();
      return -1;
    } else {
      ndbout << "Nodegroup " << ng << " created" << endl;
    }

  }

  return 0;
err:
  ndbout << "Invalid arguments: expected <id>,<id>..." << endl;
  return -1;
}

int
CommandInterpreter::executeDropNodeGroup(char* parameters)
{
  int ng = -1;
  if (emptyString(parameters))
    goto err;

  {
    char* id = strchr(parameters, ' ');
    if(id == 0 || sscanf(id, "%d", &ng) != 1)
      goto err;
  }

  {
    struct ndb_mgm_reply reply;
    const int result= ndb_mgm_drop_nodegroup(m_mgmsrv, ng, &reply);
    if (result != 0) {
      printError();
      return -1;
    } else {
      ndbout << "Drop Node Group " << ng << " done" << endl;
    }
  }
  return 0;
err:
  ndbout << "Invalid arguments: expected <NG>" << endl;
  return -1;
}
