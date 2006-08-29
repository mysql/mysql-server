/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <my_sys.h>

//#define HAVE_GLOBAL_REPLICATION

#include <Vector.hpp>
#ifdef  HAVE_GLOBAL_REPLICATION
#include "../rep/repapi/repapi.h"
#endif

#include <mgmapi.h>
#include <util/BaseString.hpp>

class MgmtSrvr;

/** 
 *  @class CommandInterpreter
 *  @brief Reads command line in management client
 *
 *  This class has one public method which reads a command line 
 *  from a stream. It then interpret that commmand line and calls a suitable 
 *  method in the MgmtSrvr class which executes the command.
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
  int execute(const char *_line, int _try_reconnect=-1, bool interactive=1, int *error= 0);

private:
  void printError();
  int execute_impl(const char *_line, bool interactive=1);

  /**
   *   Analyse the command line, after the first token.
   *
   *   @param  processId:           DB process id to send command to or -1 if
   *                                command will be sent to all DB processes.
   *   @param  allAfterFirstToken:  What the client gave after the 
   *                                first token on the command line
   */
  void analyseAfterFirstToken(int processId, char* allAfterFirstTokenCstr);

  void executeCommand(Vector<BaseString> &command_list,
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
			       Vector<const char*>& blocks);
  
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
  void executeHelp(char* parameters);
  void executeShow(char* parameters);
  void executeConnect(char* parameters, bool interactive);
  void executePurge(char* parameters);
  int  executeShutdown(char* parameters);
  void executeRun(char* parameters);
  void executeInfo(char* parameters);
  void executeClusterLog(char* parameters);

public:
  void executeStop(int processId, const char* parameters, bool all);
  void executeStop(Vector<BaseString> &command_list, unsigned command_pos,
                   int *node_ids, int no_of_nodes);
  void executeEnterSingleUser(char* parameters);
  void executeExitSingleUser(char* parameters);
  void executeStart(int processId, const char* parameters, bool all);
  void executeRestart(int processId, const char* parameters, bool all);
  void executeRestart(Vector<BaseString> &command_list, unsigned command_pos,
                      int *node_ids, int no_of_nodes);
  void executeLogLevel(int processId, const char* parameters, bool all);
  void executeError(int processId, const char* parameters, bool all);
  void executeLog(int processId, const char* parameters, bool all);
  void executeLogIn(int processId, const char* parameters, bool all);
  void executeLogOut(int processId, const char* parameters, bool all);
  void executeLogOff(int processId, const char* parameters, bool all);
  void executeTestOn(int processId, const char* parameters, bool all);
  void executeTestOff(int processId, const char* parameters, bool all);
  void executeSet(int processId, const char* parameters, bool all);
  void executeGetStat(int processId, const char* parameters, bool all);
  void executeStatus(int processId, const char* parameters, bool all);
  void executeEventReporting(int processId, const char* parameters, bool all);
  void executeDumpState(int processId, const char* parameters, bool all);
  int executeStartBackup(char * parameters);
  void executeAbortBackup(char * parameters);

  void executeRep(char* parameters);

  void executeCpc(char * parameters);

public:
  bool connect(bool interactive);
  bool disconnect();

  /**
   * A execute function definition
   */
public:
  typedef void (CommandInterpreter::* ExecuteFunction)(int processId, 
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
  void executeForAll(const char * cmd, 
		     ExecuteFunction fun,
		     const char * param);

  NdbMgmHandle m_mgmsrv;
  NdbMgmHandle m_mgmsrv2;
  const char *m_constr;
  bool m_connected;
  int m_verbose;
  int try_reconnect;
  int m_error;
#ifdef HAVE_GLOBAL_REPLICATION  
  NdbRepHandle m_repserver;
  const char *rep_host;
  bool rep_connected;
#endif
  struct NdbThread* m_event_thread;
  NdbMutex *m_print_mutex;
};

struct event_thread_param {
  NdbMgmHandle *m;
  NdbMutex **p;
};

NdbMutex* print_mutex;

/*
 * Facade object for CommandInterpreter
 */

#include "ndb_mgmclient.hpp"
#include "ndb_mgmclient.h"

Ndb_mgmclient::Ndb_mgmclient(const char *host,int verbose)
{
  m_cmd= new CommandInterpreter(host,verbose);
}
Ndb_mgmclient::~Ndb_mgmclient()
{
  delete m_cmd;
}
int Ndb_mgmclient::execute(const char *_line, int _try_reconnect, bool interactive, int *error)
{
  return m_cmd->execute(_line,_try_reconnect,interactive, error);
}
int
Ndb_mgmclient::disconnect()
{
  return m_cmd->disconnect();
}

extern "C" {
  Ndb_mgmclient_handle ndb_mgmclient_handle_create(const char *connect_string)
  {
    return (Ndb_mgmclient_handle) new Ndb_mgmclient(connect_string);
  }
  int ndb_mgmclient_execute(Ndb_mgmclient_handle h, int argc, char** argv)
  {
    return ((Ndb_mgmclient*)h)->execute(argc, argv, 1);
  }
  int ndb_mgmclient_handle_destroy(Ndb_mgmclient_handle h)
  {
    delete (Ndb_mgmclient*)h;
    return 0;
  }
}
/*
 * The CommandInterpreter
 */

#include <mgmapi.h>
#include <mgmapi_debug.h>
#include <version.h>
#include <NdbAutoPtr.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbMem.h>
#include <EventLogger.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/GrepImpl.hpp>
#ifdef HAVE_GLOBAL_REPLICATION

#endif // HAVE_GLOBAL_REPLICATION
#include "MgmtErrorReporter.hpp"
#include <Parser.hpp>
#include <SocketServer.hpp>
#include <util/InputStream.hpp>
#include <util/OutputStream.hpp>

int Ndb_mgmclient::execute(int argc, char** argv, int _try_reconnect, bool interactive, int *error)
{
  if (argc <= 0)
    return 0;
  BaseString _line(argv[0]);
  for (int i= 1; i < argc; i++)
  {
    _line.appfmt(" %s", argv[i]);
  }
  return m_cmd->execute(_line.c_str(),_try_reconnect, interactive, error);
}

/*****************************************************************************
 * HELP
 *****************************************************************************/
static const char* helpText =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help\n"
"---------------------------------------------------------------------------\n"
"HELP                                   Print help text\n"
"HELP COMMAND                           Print detailed help for COMMAND(e.g. SHOW)\n"
#ifdef HAVE_GLOBAL_REPLICATION
"HELP REPLICATION                       Help for global replication\n"
#endif // HAVE_GLOBAL_REPLICATION
#ifdef VM_TRACE // DEBUG ONLY
"HELP DEBUG                             Help for debug compiled version\n"
#endif
"SHOW                                   Print information about cluster\n"
#if 0
"SHOW CONFIG                            Print configuration\n"
"SHOW PARAMETERS                        Print configuration parameters\n"
#endif
"START BACKUP [NOWAIT | WAIT STARTED | WAIT COMPLETED]\n"
"                                       Start backup (default WAIT COMPLETED)\n"
"ABORT BACKUP <backup id>               Abort backup\n"
"SHUTDOWN                               Shutdown all processes in cluster\n"
"CLUSTERLOG ON [<severity>] ...         Enable Cluster logging\n"
"CLUSTERLOG OFF [<severity>] ...        Disable Cluster logging\n"
"CLUSTERLOG TOGGLE [<severity>] ...     Toggle severity filter on/off\n"
"CLUSTERLOG INFO                        Print cluster log information\n"
"<id> START                             Start data node (started with -n)\n"
"<id> RESTART [-n] [-i]                 Restart data or management server node\n"
"<id> STOP                              Stop data or management server node\n"
"ENTER SINGLE USER MODE <id>            Enter single user mode\n"
"EXIT SINGLE USER MODE                  Exit single user mode\n"
"<id> STATUS                            Print status\n"
"<id> CLUSTERLOG {<category>=<level>}+  Set log level for cluster log\n"
#ifdef HAVE_GLOBAL_REPLICATION
"REP CONNECT <host:port>                Connect to REP server on host:port\n"
#endif
"PURGE STALE SESSIONS                   Reset reserved nodeid's in the mgmt server\n"
"CONNECT [<connectstring>]              Connect to management server (reconnect if already connected)\n"
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
#if 0
"SHOW CONFIG        Print configuration (in initial config file format)\n" 
"SHOW PARAMETERS    Print information about configuration parameters\n\n"
#endif
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
"START BACKUP [NOWAIT | WAIT STARTED | WAIT COMPLETED]\n"
"                   Start a backup for the cluster.\n"
"                   Each backup gets an ID number that is reported to the\n"
"                   user. This ID number can help you find the backup on the\n"
"                   file system, or ABORT BACKUP if you wish to cancel a \n"
"                   running backup.\n\n"
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
"<id> RESTART [-n] [-i] \n"
"                   Restart the data or management node <id>(or All data nodes).\n\n"
"                   -n (--nostart) restarts the node but does not\n"
"                   make it join the cluster. Use '<id> START' to\n"
"                   join the node to the cluster.\n\n"
"                   -i (--initial) perform initial start.\n"
"                   This cleans the file system (ndb_<id>_fs)\n"
"                   and the node will copy data from another node\n"
"                   in the same node group during start up.\n\n"
"                   Consult the documentation before using -i.\n\n" 
"                   INCORRECT USE OF -i WILL CAUSE DATA LOSS!\n"
;

static const char* helpTextStop =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for STOP command\n"
"---------------------------------------------------------------------------\n"
"STOP  Stop data or management server node\n\n"
"<id> STOP          Stop the data or management server node <id>.\n\n"
"                   ALL STOP will just stop all data nodes.\n\n"
"                   If you desire to also shut down management servers,\n"
"                   use SHUTDOWN instead.\n" 
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

static const char* helpTextQuit =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for QUIT command\n"
"---------------------------------------------------------------------------\n"
"QUIT  Quit management client\n\n"
"QUIT               Terminates the management client. \n"                    
;


#ifdef HAVE_GLOBAL_REPLICATION
static const char* helpTextRep =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for Global Replication\n"
"---------------------------------------------------------------------------\n"
"Commands should be executed on the standby NDB Cluster\n"
"These features are in an experimental release state.\n"
"\n"
"Simple Commands:\n"
"REP START              Start Global Replication\n" 
"REP START REQUESTOR    Start Global Replication Requestor\n" 
"REP STATUS             Show Global Replication status\n" 
"REP STOP               Stop Global Replication\n"
"REP STOP REQUESTOR     Stop Global Replication Requestor\n"
"\n" 
"Advanced Commands:\n"
"REP START <protocol>   Starts protocol\n"
"REP STOP <protocol>    Stops protocol\n"
"<protocol> = TRANSFER | APPLY | DELETE\n"
"\n"
#ifdef VM_TRACE // DEBUG ONLY
"Debugging commands:\n"
"REP DELETE             Removes epochs stored in primary and standy systems\n"
"REP DROP <tableid>     Drop a table in SS identified by table id\n"
"REP SLOWSTOP           Stop Replication (Tries to synchonize with primary)\n" 
"REP FASTSTOP           Stop Replication (Stops in consistent state)\n" 
"<component> = SUBSCRIPTION\n"
"              METALOG | METASCAN | DATALOG | DATASCAN\n"
"              REQUESTOR | TRANSFER | APPLY | DELETE\n"
#endif
;
#endif // HAVE_GLOBAL_REPLICATION

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
"<id> LOGIN [BLOCK = {ALL|<block>+}]   Set logging on in signals\n"
"<id> LOGOUT [BLOCK = {ALL|<block>+}]  Set logging on out signals\n"
"<id> LOGOFF [BLOCK = {ALL|<block>+}]  Unset signal logging\n"
"<id> TESTON                           Start signal logging\n"
"<id> TESTOFF                          Stop signal logging\n"
"<id> SET <configParamName> <value>    Update configuration variable\n"
"<id> DUMP <arg>                       Dump system state to cluster.log\n"
"<id> GETSTAT                          Print statistics\n"
"\n"
"<id>       = ALL | Any database node id\n"
;
#endif

struct {
  const char *cmd;
  const char * help;
}help_items[]={
  {"SHOW", helpTextShow},
  {"HELP", helpTextHelp},
  {"BACKUP", helpTextBackup},
  {"START BACKUP", helpTextStartBackup},
  {"START BACKUP NOWAIT", helpTextStartBackup},
  {"START BACKUP WAIT STARTED", helpTextStartBackup},
  {"START BACKUP WAIT", helpTextStartBackup},
  {"START BACKUP WAIT COMPLETED", helpTextStartBackup},
  {"ABORT BACKUP", helpTextAbortBackup},
  {"SHUTDOWN", helpTextShutdown},
  {"CLUSTERLOG ON", helpTextClusterlogOn},
  {"CLUSTERLOG OFF", helpTextClusterlogOff},
  {"CLUSTERLOG TOGGLE", helpTextClusterlogToggle},
  {"CLUSTERLOG INFO", helpTextClusterlogInfo},
  {"START", helpTextStart},
  {"RESTART", helpTextRestart},
  {"STOP", helpTextStop},
  {"ENTER SINGLE USER MODE", helpTextEnterSingleUserMode},
  {"EXIT SINGLE USER MODE", helpTextExitSingleUserMode},
  {"STATUS", helpTextStatus},
  {"CLUSTERLOG", helpTextClusterlog},
  {"PURGE STALE SESSIONS", helpTextPurgeStaleSessions},
  {"CONNECT", helpTextConnect},
  {"QUIT", helpTextQuit},
#ifdef HAVE_GLOBAL_REPLICATION
  {"REPLICATION", helpTextRep},
  {"REP", helpTextRep},
#endif // HAVE_GLOBAL_REPLICATION
#ifdef VM_TRACE // DEBUG ONLY
  {"DEBUG", helpTextDebug},
#endif //VM_TRACE
  {NULL, NULL}
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
CommandInterpreter::CommandInterpreter(const char *_host,int verbose) 
  : m_verbose(verbose)
{
  m_constr= _host;
  m_connected= false;
  m_event_thread= NULL;
  try_reconnect = 0;
  m_print_mutex= NdbMutex_Create();
#ifdef HAVE_GLOBAL_REPLICATION
  rep_host = NULL;
  m_repserver = NULL;
  rep_connected = false;
#endif
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
  ndbout_c("* %5d: %s", 
	   ndb_mgm_get_latest_error(m_mgmsrv),
	   ndb_mgm_get_latest_error_msg(m_mgmsrv));
  ndbout_c("*        %s", ndb_mgm_get_latest_error_desc(m_mgmsrv));
  if (ndb_mgm_check_connection(m_mgmsrv))
  {
    disconnect();
  }
}

//*****************************************************************************
//*****************************************************************************

static int do_event_thread;
static void*
event_thread_run(void* p)
{
  DBUG_ENTER("event_thread_run");

  struct event_thread_param param= *(struct event_thread_param*)p;
  NdbMgmHandle handle= *(param.m);
  NdbMutex* printmutex= *(param.p);

  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
		   1, NDB_MGM_EVENT_CATEGORY_STARTUP,
		   0 };
  int fd = ndb_mgm_listen_event(handle, filter);
  if (fd != NDB_INVALID_SOCKET)
  {
    do_event_thread= 1;
    char *tmp= 0;
    char buf[1024];
    SocketInputStream in(fd,10);
    do {
      if (tmp == 0) NdbSleep_MilliSleep(10);
      if((tmp = in.gets(buf, 1024)))
      {
	const char ping_token[]= "<PING>";
	if (memcmp(ping_token,tmp,sizeof(ping_token)-1))
	  if(tmp && strlen(tmp))
          {
            Guard g(printmutex);
            ndbout << tmp;
          }
      }
    } while(do_event_thread);
    NDB_CLOSE_SOCKET(fd);
  }
  else
  {
    do_event_thread= -1;
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
    ndbout_c("Cannot create handle to management server.");
    exit(-1);
  }
  if (interactive) {
    m_mgmsrv2 = ndb_mgm_create_handle();
    if(m_mgmsrv2 == NULL) {
      ndbout_c("Cannot create 2:nd handle to management server.");
      exit(-1);
    }
  }

  if (ndb_mgm_set_connectstring(m_mgmsrv, m_constr))
  {
    printError();
    exit(-1);
  }

  if(ndb_mgm_connect(m_mgmsrv, try_reconnect-1, 5, 1))
    DBUG_RETURN(m_connected); // couldn't connect, always false

  const char *host= ndb_mgm_get_connected_host(m_mgmsrv);
  unsigned port= ndb_mgm_get_connected_port(m_mgmsrv);
  if (interactive) {
    BaseString constr;
    constr.assfmt("%s:%d",host,port);
    if(!ndb_mgm_set_connectstring(m_mgmsrv2, constr.c_str()) &&
       !ndb_mgm_connect(m_mgmsrv2, try_reconnect-1, 5, 1))
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
                                        32768,
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

bool 
CommandInterpreter::disconnect() 
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
  DBUG_RETURN(true);
}

//*****************************************************************************
//*****************************************************************************

int 
CommandInterpreter::execute(const char *_line, int _try_reconnect,
			    bool interactive, int *error) 
{
  if (_try_reconnect >= 0)
    try_reconnect=_try_reconnect;
  int result= execute_impl(_line, interactive);
  if (error)
    *error= m_error;

  return result;
}

static void
invalid_command(const char *cmd)
{
  ndbout << "Invalid command: " << cmd << endl;
  ndbout << "Type HELP for help." << endl << endl;
}

int 
CommandInterpreter::execute_impl(const char *_line, bool interactive) 
{
  DBUG_ENTER("CommandInterpreter::execute_impl");
  DBUG_PRINT("enter",("line=\"%s\"",_line));
  m_error= 0;

  char * line;
  if(_line == NULL) {
    DBUG_RETURN(false);
  }
  line = my_strdup(_line,MYF(MY_WME));
  My_auto_ptr<char> ptr(line);

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
      unsigned last= strlen(line)-1;
      if (line[last] == ';')
      {
	line[last]= 0;
	do_continue= 1;
      }
    }
  } while (do_continue);
  // if there is anything in the line proceed
  Vector<BaseString> command_list;
  {
    BaseString tmp(line);
    tmp.split(command_list);
    for (unsigned i= 0; i < command_list.size();)
      command_list[i].c_str()[0] ? i++ : (command_list.erase(i),0);
  }
  char* firstToken = strtok(line, " ");
  char* allAfterFirstToken = strtok(NULL, "");

  if (strcasecmp(firstToken, "HELP") == 0 ||
      strcasecmp(firstToken, "?") == 0) {
    executeHelp(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "CONNECT") == 0) {
    executeConnect(allAfterFirstToken, interactive);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "SLEEP") == 0) {
    if (allAfterFirstToken)
      sleep(atoi(allAfterFirstToken));
    DBUG_RETURN(true);
  }
  else if((strcasecmp(firstToken, "QUIT") == 0 ||
	  strcasecmp(firstToken, "EXIT") == 0 ||
	  strcasecmp(firstToken, "BYE") == 0) && 
	  allAfterFirstToken == NULL){
    DBUG_RETURN(false);
  }

  if (!connect(interactive))
    DBUG_RETURN(true);

  if (ndb_mgm_check_connection(m_mgmsrv))
  {
    disconnect();
    connect(interactive);
  }

  if (strcasecmp(firstToken, "SHOW") == 0) {
    Guard g(m_print_mutex);
    executeShow(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "SHUTDOWN") == 0) {
    m_error= executeShutdown(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "CLUSTERLOG") == 0){
    executeClusterLog(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(strcasecmp(firstToken, "START") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    m_error= executeStartBackup(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(strcasecmp(firstToken, "ABORT") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    executeAbortBackup(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "PURGE") == 0) {
    executePurge(allAfterFirstToken);
    DBUG_RETURN(true);
  } 
#ifdef HAVE_GLOBAL_REPLICATION
  else if(strcasecmp(firstToken, "REPLICATION") == 0 ||
	  strcasecmp(firstToken, "REP") == 0) {
    executeRep(allAfterFirstToken);
    DBUG_RETURN(true);
  }
#endif // HAVE_GLOBAL_REPLICATION
  else if(strcasecmp(firstToken, "ENTER") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeEnterSingleUser(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(strcasecmp(firstToken, "EXIT") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeExitSingleUser(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "ALL") == 0) {
    analyseAfterFirstToken(-1, allAfterFirstToken);
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
        if (node_id <= 0) {
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
      DBUG_RETURN(true);
    }
    if (pos == command_list.size())
    {
      /* No command found */
      invalid_command(_line);
      DBUG_RETURN(true);
    }
    if (no_of_nodes == 1)
    {
      analyseAfterFirstToken(node_ids[0], allAfterFirstToken);
      DBUG_RETURN(true);
    }
    executeCommand(command_list, pos, node_ids, no_of_nodes);
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
  ,{ "LOGIN", &CommandInterpreter::executeLogIn }
  ,{ "LOGOUT", &CommandInterpreter::executeLogOut }
  ,{ "LOGOFF", &CommandInterpreter::executeLogOff }
  ,{ "TESTON", &CommandInterpreter::executeTestOn }
  ,{ "TESTOFF", &CommandInterpreter::executeTestOff }
  ,{ "SET", &CommandInterpreter::executeSet }
  ,{ "GETSTAT", &CommandInterpreter::executeGetStat }
  ,{ "DUMP", &CommandInterpreter::executeDumpState }
};


//*****************************************************************************
//*****************************************************************************
void
CommandInterpreter::analyseAfterFirstToken(int processId,
					   char* allAfterFirstToken) {
  
  if (emptyString(allAfterFirstToken)) {
    ndbout << "Expected a command after "
	   << ((processId == -1) ? "ALL." : "node ID.") << endl;
    return;
  }
  
  char* secondToken = strtok(allAfterFirstToken, " ");
  char* allAfterSecondToken = strtok(NULL, "\0");

  const int tmpSize = sizeof(commands)/sizeof(CommandFunctionPair);
  ExecuteFunction fun = 0;
  const char * command = 0;
  for(int i = 0; i<tmpSize; i++){
    if(strcasecmp(secondToken, commands[i].command) == 0){
      fun = commands[i].executeFunction;
      command = commands[i].command;
      break;
    }
  }
  
  if(fun == 0){
    invalid_command(secondToken);
    return;
  }
  
  if(processId == -1){
    executeForAll(command, fun, allAfterSecondToken);
  } else {
    (this->*fun)(processId, allAfterSecondToken, false);
  }
  ndbout << endl;
}

void
CommandInterpreter::executeCommand(Vector<BaseString> &command_list,
                                   unsigned command_pos,
                                   int *node_ids, int no_of_nodes)
{
  const char *cmd= command_list[command_pos].c_str();
  if (strcasecmp("STOP", cmd) == 0)
  {
    executeStop(command_list, command_pos+1, node_ids, no_of_nodes);
    return;
  }
  if (strcasecmp("RESTART", cmd) == 0)
  {
    executeRestart(command_list, command_pos+1, node_ids, no_of_nodes);
    return;
  }
  ndbout_c("Invalid command: '%s' after multi node id list. "
           "Expected STOP or RESTART.", cmd);
  return;
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

void
CommandInterpreter::executeForAll(const char * cmd, ExecuteFunction fun, 
				  const char * allAfterSecondToken)
{
  int nodeId = 0;
  if(strcasecmp(cmd, "STOP") == 0) {
    ndbout_c("Executing STOP on all nodes.");
    (this->*fun)(nodeId, allAfterSecondToken, true);
  } else if(strcasecmp(cmd, "RESTART") == 0) {
    ndbout_c("Executing RESTART on all nodes.");
    ndbout_c("Starting shutdown. This may take a while. Please wait...");
    (this->*fun)(nodeId, allAfterSecondToken, true);
    ndbout_c("Trying to start all nodes of system.");
    ndbout_c("Use ALL STATUS to see the system start-up phases.");
  } else {
    Guard g(m_print_mutex);
    struct ndb_mgm_cluster_state *cl= ndb_mgm_get_status(m_mgmsrv);
    if(cl == 0){
      ndbout_c("Unable get status from management server");
      printError();
      return;
    }
    NdbAutoPtr<char> ap1((char*)cl);
    while(get_next_nodeid(cl, &nodeId, NDB_MGM_NODE_TYPE_NDB))
      (this->*fun)(nodeId, allAfterSecondToken, true);
  }
}

//*****************************************************************************
//*****************************************************************************
bool 
CommandInterpreter::parseBlockSpecification(const char* allAfterLog,
					    Vector<const char*>& blocks) 
{
  // Parse: [BLOCK = {ALL|<blockName>+}]

  if (emptyString(allAfterLog)) {
    return true;
  }

  // Copy allAfterLog since strtok will modify it  
  char* newAllAfterLog = my_strdup(allAfterLog,MYF(MY_WME));
  My_auto_ptr<char> ap1(newAllAfterLog);
  char* firstTokenAfterLog = strtok(newAllAfterLog, " ");
  for (unsigned int i = 0; i < strlen(firstTokenAfterLog); ++i) {
    firstTokenAfterLog[i] = toupper(firstTokenAfterLog[i]);
  }
  
  if (strcasecmp(firstTokenAfterLog, "BLOCK") != 0) {
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
  if (strcasecmp(secondTokenAfterLog, "=") != 0) {
    ndbout << "Unexpected value: " << secondTokenAfterLog 
	   << ". Expected =." << endl;
    return false;
  }

  char* blockName = strtok(NULL, " ");
  bool all = false;
  if (blockName != NULL && (strcasecmp(blockName, "ALL") == 0)) {
    all = true;
  }
  while (blockName != NULL) {
    blocks.push_back(strdup(blockName));
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
void 
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

    ndbout << "<level>    = " << "0 - 15" << endl;
    ndbout << "<id>       = " << "ALL | Any database node id" << endl;
    ndbout << endl;
    ndbout << "For detailed help on COMMAND, use HELP COMMAND." << endl;
  } else {
    int i = 0;
    for (i = 0; help_items[i].cmd != NULL; i++) 
    {
      if (strcasecmp(parameters, help_items[i].cmd) == 0)
      {
        ndbout << help_items[i].help;
        break;
      }     
    }
    if (help_items[i].cmd == NULL)
      ndbout << "No help for " << parameters << " available" << endl;
  }
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
	    || strcasecmp(hostname,"0.0.0.0") == 0)
	  ndbout << " ";
	else
	  ndbout << "\t@" << hostname;
	ndbout << "  (Version: "
	       << getMajor(node_state->version) << "."
	       << getMinor(node_state->version) << "."
	       << getBuild(node_state->version);
	if (type == NDB_MGM_NODE_TYPE_NDB) {
	  if (node_state->node_status != NDB_MGM_NODE_STATUS_STARTED) {
	    ndbout << ", " << status_string(node_state->node_status);
	  }
	  if (node_state->node_group >= 0) {
	    ndbout << ", Nodegroup: " << node_state->node_group;
	    if (master_id && node_state->dynamic_id == master_id)
	      ndbout << ", Master";
	  }
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

void
CommandInterpreter::executePurge(char* parameters) 
{ 
  int command_ok= 0;
  do {
    if (emptyString(parameters))
      break;
    char* firstToken = strtok(parameters, " ");
    char* nextToken = strtok(NULL, " \0");
    if (strcasecmp(firstToken,"STALE") == 0 &&
	nextToken &&
	strcasecmp(nextToken, "SESSIONS") == 0) {
      command_ok= 1;
      break;
    }
  } while(0);

  if (!command_ok) {
    ndbout_c("Unexpected command, expected: PURGE STALE SESSIONS");
    return;
  }

  int i;
  char *str;
  
  if (ndb_mgm_purge_stale_sessions(m_mgmsrv, &str)) {
    ndbout_c("Command failed");
    return;
  }
  if (str) {
    ndbout_c("Purged sessions with node id's: %s", str);
    free(str);
  }
  else
  {
    ndbout_c("No sessions purged");
  }
}

void
CommandInterpreter::executeShow(char* parameters) 
{ 
  int i;
  if (emptyString(parameters)) {
    ndb_mgm_cluster_state *state = ndb_mgm_get_status(m_mgmsrv);
    if(state == NULL) {
      ndbout_c("Could not get status");
      printError();
      return;
    }
    NdbAutoPtr<char> ap1((char*)state);

    ndb_mgm_configuration * conf = ndb_mgm_get_configuration(m_mgmsrv,0);
    if(conf == 0){
      ndbout_c("Could not get configuration");
      printError();
      return;
    }

    ndb_mgm_configuration_iterator * it;
    it = ndb_mgm_create_configuration_iterator((struct ndb_mgm_configuration *)conf, CFG_SECTION_NODE);

    if(it == 0){
      ndbout_c("Unable to create config iterator");
      ndb_mgm_destroy_configuration(conf);
      return;
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
        return;
      case NDB_MGM_NODE_TYPE_REP:
	abort();
      }
    }

    ndbout << "Cluster Configuration" << endl
	   << "---------------------" << endl;
    print_nodes(state, it, "ndbd",     ndb_nodes, NDB_MGM_NODE_TYPE_NDB, master_id);
    print_nodes(state, it, "ndb_mgmd", mgm_nodes, NDB_MGM_NODE_TYPE_MGM, 0);
    print_nodes(state, it, "mysqld",   api_nodes, NDB_MGM_NODE_TYPE_API, 0);
    //    ndbout << helpTextShow;
    ndb_mgm_destroy_configuration(conf);
    return;
  } else if (strcasecmp(parameters, "PROPERTIES") == 0 ||
	     strcasecmp(parameters, "PROP") == 0) {
    ndbout << "SHOW PROPERTIES is not yet implemented." << endl;
    //  ndbout << "_mgmtSrvr.getConfig()->print();" << endl; /* XXX */
  } else if (strcasecmp(parameters, "CONFIGURATION") == 0 ||
	     strcasecmp(parameters, "CONFIG") == 0){
    ndbout << "SHOW CONFIGURATION is not yet implemented." << endl;
    //nbout << "_mgmtSrvr.getConfig()->printConfigFile();" << endl; /* XXX */
  } else if (strcasecmp(parameters, "PARAMETERS") == 0 ||
	     strcasecmp(parameters, "PARAMS") == 0 ||
	     strcasecmp(parameters, "PARAM") == 0) {
    ndbout << "SHOW PARAMETERS is not yet implemented." << endl;
    //    ndbout << "_mgmtSrvr.getConfig()->getConfigInfo()->print();" 
    //           << endl; /* XXX */
  } else {
    ndbout << "Invalid argument." << endl;
  }
}

void
CommandInterpreter::executeConnect(char* parameters, bool interactive) 
{
  BaseString *basestring = NULL;

  disconnect();
  if (!emptyString(parameters)) {
    basestring= new BaseString(parameters);
    m_constr= basestring->trim().c_str();
  }
  connect(interactive);
  if (basestring != NULL)
    delete basestring;
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
    ndbout << "Missing argument." << endl;
    DBUG_VOID_RETURN;
  }

  enum ndb_mgm_event_severity severity = NDB_MGM_EVENT_SEVERITY_ALL;
    
  char * tmpString = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1(tmpString);
  char * tmpPtr = 0;
  char * item = strtok_r(tmpString, " ", &tmpPtr);
  int enable;

  const unsigned int *enabled= ndb_mgm_get_logfilter(m_mgmsrv);
  if(enabled == NULL) {
    ndbout << "Couldn't get status" << endl;
    printError();
    DBUG_VOID_RETURN;
  }

  /********************
   * CLUSTERLOG INFO
   ********************/
  if (strcasecmp(item, "INFO") == 0) {
    DBUG_PRINT("info",("INFO"));
    if(enabled[0] == 0)
    {
      ndbout << "Cluster logging is disabled." << endl;
      DBUG_VOID_RETURN;
    }
#if 0 
    for(i = 0; i<7;i++)
      printf("enabled[%d] = %d\n", i, enabled[i]);
#endif
    ndbout << "Severities enabled: ";
    for(i = 1; i < (int)NDB_MGM_EVENT_SEVERITY_ALL; i++) {
      const char *str= ndb_mgm_get_event_severity_string((ndb_mgm_event_severity)i);
      if (str == 0)
      {
	DBUG_ASSERT(false);
	continue;
      }
      if(enabled[i])
	ndbout << BaseString(str).ndb_toupper() << " ";
    }
    ndbout << endl;
    DBUG_VOID_RETURN;

  } 
  else if (strcasecmp(item, "FILTER") == 0 ||
	   strcasecmp(item, "TOGGLE") == 0)
  {
    DBUG_PRINT("info",("TOGGLE"));
    enable= -1;
  } 
  else if (strcasecmp(item, "OFF") == 0) 
  {
    DBUG_PRINT("info",("OFF"));
    enable= 0;
  } else if (strcasecmp(item, "ON") == 0) {
    DBUG_PRINT("info",("ON"));
    enable= 1;
  } else {
    ndbout << "Invalid argument." << endl;
    DBUG_VOID_RETURN;
  }

  int res_enable;
  item = strtok_r(NULL, " ", &tmpPtr);
  if (item == NULL) {
    res_enable=
      ndb_mgm_set_clusterlog_severity_filter(m_mgmsrv,
					     NDB_MGM_EVENT_SEVERITY_ON,
					     enable, NULL);
    if (res_enable < 0)
    {
      ndbout << "Couldn't set filter" << endl;
      printError();
      DBUG_VOID_RETURN;
    }
    ndbout << "Cluster logging is " << (res_enable ? "enabled.":"disabled") << endl;
    DBUG_VOID_RETURN;
  }

  do {
    severity= NDB_MGM_ILLEGAL_EVENT_SEVERITY;
    if (strcasecmp(item, "ALL") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ALL;	
    } else if (strcasecmp(item, "ALERT") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ALERT;
    } else if (strcasecmp(item, "CRITICAL") == 0) { 
      severity = NDB_MGM_EVENT_SEVERITY_CRITICAL;
    } else if (strcasecmp(item, "ERROR") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ERROR;
    } else if (strcasecmp(item, "WARNING") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_WARNING;
    } else if (strcasecmp(item, "INFO") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_INFO;
    } else if (strcasecmp(item, "DEBUG") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_DEBUG;
    } else if (strcasecmp(item, "OFF") == 0 ||
	       strcasecmp(item, "ON") == 0) {
      if (enable < 0) // only makes sense with toggle
	severity = NDB_MGM_EVENT_SEVERITY_ON;
    }
    if (severity == NDB_MGM_ILLEGAL_EVENT_SEVERITY) {
      ndbout << "Invalid severity level: " << item << endl;
      DBUG_VOID_RETURN;
    }

    res_enable= ndb_mgm_set_clusterlog_severity_filter(m_mgmsrv, severity,
						       enable, NULL);
    if (res_enable < 0)
    {
      ndbout << "Couldn't set filter" << endl;
      printError();
      DBUG_VOID_RETURN;
    }
    ndbout << BaseString(item).ndb_toupper().c_str() << " " << (res_enable ? "enabled":"disabled") << endl;

    item = strtok_r(NULL, " ", &tmpPtr);	
  } while(item != NULL);

  DBUG_VOID_RETURN;
} 

//*****************************************************************************
//*****************************************************************************

void
CommandInterpreter::executeStop(int processId, const char *parameters,
                                bool all) 
{
  Vector<BaseString> command_list;
  if (parameters)
  {
    BaseString tmp(parameters);
    tmp.split(command_list);
    for (unsigned i= 0; i < command_list.size();)
      command_list[i].c_str()[0] ? i++ : (command_list.erase(i),0);
  }
  if (all)
    executeStop(command_list, 0, 0, 0);
  else
    executeStop(command_list, 0, &processId, 1);
}

void
CommandInterpreter::executeStop(Vector<BaseString> &command_list,
                                unsigned command_pos,
                                int *node_ids, int no_of_nodes)
{
  int need_disconnect;
  int abort= 0;
  for (; command_pos < command_list.size(); command_pos++)
  {
    const char *item= command_list[command_pos].c_str();
    if (strcasecmp(item, "-A") == 0)
    {
      abort= 1;
      continue;
    }
    ndbout_c("Invalid option: %s. Expecting -A after STOP",
             item);
    return;
  }

  int result= ndb_mgm_stop3(m_mgmsrv, no_of_nodes, node_ids, abort,
                            &need_disconnect);
  if (result < 0)
  {
    ndbout_c("Shutdown failed.");
    printError();
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

}

void
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
    return;
  }
  int result = ndb_mgm_enter_single_user(m_mgmsrv, nodeId, &reply);
  
  if (result != 0) {
    ndbout_c("Entering single user mode for node %d failed", nodeId);
    printError();
  } else {
    ndbout_c("Single user mode entered");
    ndbout_c("Access is granted for API node %d only.", nodeId);
  }
}

void 
CommandInterpreter::executeExitSingleUser(char* parameters) 
{
  int result = ndb_mgm_exit_single_user(m_mgmsrv, 0);
  if (result != 0) {
    ndbout_c("Exiting single user mode failed.");
    printError();
  } else {
    ndbout_c("Exiting single user mode in progress.");
    ndbout_c("Use ALL STATUS or SHOW to see when single user mode has been exited.");
  }
}

void
CommandInterpreter::executeStart(int processId, const char* parameters,
				 bool all) 
{
  int result;
  if(all) {
    result = ndb_mgm_start(m_mgmsrv, 0, 0);
  } else {
    result = ndb_mgm_start(m_mgmsrv, 1, &processId);
  }

  if (result <= 0) {
    ndbout << "Start failed." << endl;
    printError();
  } else
    {
      if(all)
	ndbout_c("NDB Cluster is being started.");
      else
	ndbout_c("Database node %d is being started.", processId);
    }
}

void
CommandInterpreter::executeRestart(int processId, const char* parameters,
				   bool all)
{
  Vector<BaseString> command_list;
  if (parameters)
  {
    BaseString tmp(parameters);
    tmp.split(command_list);
    for (unsigned i= 0; i < command_list.size();)
      command_list[i].c_str()[0] ? i++ : (command_list.erase(i),0);
  }
  if (all)
    executeRestart(command_list, 0, 0, 0);
  else
    executeRestart(command_list, 0, &processId, 1);
}

void
CommandInterpreter::executeRestart(Vector<BaseString> &command_list,
                                   unsigned command_pos,
                                   int *node_ids, int no_of_nodes)
{
  int result;
  int nostart= 0;
  int initialstart= 0;
  int abort= 0;
  int need_disconnect= 0;

  for (; command_pos < command_list.size(); command_pos++)
  {
    const char *item= command_list[command_pos].c_str();
    if (strcasecmp(item, "-N") == 0)
    {
      nostart= 1;
      continue;
    }
    if (strcasecmp(item, "-I") == 0)
    {
      initialstart= 1;
      continue;
    }
    if (strcasecmp(item, "-A") == 0)
    {
      abort= 1;
      continue;
    }
    ndbout_c("Invalid option: %s. Expecting -A,-N or -I after RESTART",
             item);
    return;
  }

  result= ndb_mgm_restart3(m_mgmsrv, no_of_nodes, node_ids,
                           initialstart, nostart, abort, &need_disconnect);

  if (result <= 0) {
    ndbout_c("Restart failed.");
    printError();
  }
  else
  {
    if (node_ids == 0)
      ndbout_c("NDB Cluster is being restarted.");
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
}

void
CommandInterpreter::executeDumpState(int processId, const char* parameters,
				     bool all) 
{
  if(emptyString(parameters)){
    ndbout << "Expected argument" << endl;
    return;
  }

  Uint32 no = 0;
  int pars[25];
  
  char * tmpString = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1(tmpString);
  char * tmpPtr = 0;
  char * item = strtok_r(tmpString, " ", &tmpPtr);
  while(item != NULL){
    if (0x0 <= strtoll(item, NULL, 0) && strtoll(item, NULL, 0) <= 0xffffffff){
      pars[no] = strtoll(item, NULL, 0); 
    } else {
      ndbout << "Illegal value in argument to signal." << endl
	     << "(Value must be between 0 and 0xffffffff.)" 
	     << endl;
      return;
    }
    no++;
    item = strtok_r(NULL, " ", &tmpPtr);
  }
  ndbout << "Sending dump signal with data:" << endl;
  for (Uint32 i=0; i<no; i++) {
    ndbout.setHexFormat(1) << pars[i] << " ";
    if (!(i+1 & 0x3)) ndbout << endl;
  }
  
  struct ndb_mgm_reply reply;
  ndb_mgm_dump_state(m_mgmsrv, processId, pars, no, &reply);
}

void 
CommandInterpreter::executeStatus(int processId, 
				  const char* parameters, bool all) 
{
  if (! emptyString(parameters)) {
    ndbout_c("No parameters expected to this command.");
    return;
  }

  ndb_mgm_node_status status;
  Uint32 startPhase, version;
  bool system;
  
  struct ndb_mgm_cluster_state *cl;
  cl = ndb_mgm_get_status(m_mgmsrv);
  if(cl == NULL) {
    ndbout_c("Cannot get status of node %d.", processId);
    printError();
    return;
  }
  NdbAutoPtr<char> ap1((char*)cl);

  int i = 0;
  while((i < cl->no_of_nodes) && cl->node_states[i].node_id != processId)
    i++;
  if(cl->node_states[i].node_id != processId) {
    ndbout << processId << ": Node not found" << endl;
    return;
  }
  status = cl->node_states[i].node_status;
  startPhase = cl->node_states[i].start_phase;
  version = cl->node_states[i].version;

  ndbout << "Node " << processId << ": " << status_string(status);
  switch(status){
  case NDB_MGM_NODE_STATUS_STARTING:
    ndbout << " (Phase " << startPhase << ")";
    break;
  case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
    ndbout << " (Phase " << startPhase << ")";
    break;
  default:
    break;
  }
  if(status != NDB_MGM_NODE_STATUS_NO_CONTACT)
    ndbout_c(" (Version %d.%d.%d)", 
	     getMajor(version) ,
	     getMinor(version),
	     getBuild(version));
  else
    ndbout << endl;
}


//*****************************************************************************
//*****************************************************************************

void 
CommandInterpreter::executeLogLevel(int processId, const char* parameters, 
				    bool all) 
{
  (void) all;
  if (emptyString(parameters)) {
    ndbout << "Expected argument" << endl;
    return;
  } 
  BaseString tmp(parameters);
  Vector<BaseString> spec;
  tmp.split(spec, "=");
  if(spec.size() != 2){
    ndbout << "Invalid loglevel specification: " << parameters << endl;
    return;
  }

  spec[0].trim().ndb_toupper();
  int category = ndb_mgm_match_event_category(spec[0].c_str());
  if(category == NDB_MGM_ILLEGAL_EVENT_CATEGORY){
    category = atoi(spec[0].c_str());
    if(category < NDB_MGM_MIN_EVENT_CATEGORY ||
       category > NDB_MGM_MAX_EVENT_CATEGORY){
      ndbout << "Unknown category: \"" << spec[0].c_str() << "\"" << endl;
      return;
    }
  }
  
  int level = atoi(spec[1].c_str());
  if(level < 0 || level > 15){
    ndbout << "Invalid level: " << spec[1].c_str() << endl;
    return;
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
  } else {
    ndbout_c(" OK!");
  }  
  
}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeError(int processId, 
				      const char* parameters, bool /* all */) 
{
  if (emptyString(parameters)) {
    ndbout << "Missing error number." << endl;
    return;
  }

  // Copy parameters since strtok will modify it
  char* newpar = my_strdup(parameters,MYF(MY_WME)); 
  My_auto_ptr<char> ap1(newpar);
  char* firstParameter = strtok(newpar, " ");

  int errorNo;
  if (! convert(firstParameter, errorNo)) {
    ndbout << "Expected an integer." << endl;
    return;
  }

  char* allAfterFirstParameter = strtok(NULL, "\0");
  if (! emptyString(allAfterFirstParameter)) {
    ndbout << "Nothing expected after error number." << endl;
    return;
  }

  ndb_mgm_insert_error(m_mgmsrv, processId, errorNo, NULL);
}

//*****************************************************************************
//*****************************************************************************

void 
CommandInterpreter::executeLog(int processId,
			       const char* parameters, bool all) 
{
  struct ndb_mgm_reply reply;
  Vector<const char *> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }
  int len=1;
  Uint32 i;
  for(i=0; i<blocks.size(); i++) {
    len += strlen(blocks[i]) + 1;
  }
  char * blockNames = (char*)my_malloc(len,MYF(MY_WME));
  My_auto_ptr<char> ap1(blockNames);
  
  blockNames[0] = 0;
  for(i=0; i<blocks.size(); i++) {
    strcat(blockNames, blocks[i]);
    strcat(blockNames, "|");
  }
  
  int result = ndb_mgm_log_signals(m_mgmsrv,
				   processId, 
				   NDB_MGM_SIGNAL_LOG_MODE_INOUT, 
				   blockNames,
				   &reply);
  if (result != 0) {
    ndbout_c("Execute LOG on node %d failed.", processId);
    printError();
  }
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogIn(int /* processId */,
				 const char* parameters, bool /* all */) 
{
  ndbout << "Command LOGIN not implemented." << endl;
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogOut(int /*processId*/, 
				  const char* parameters, bool /*all*/) 
{
  ndbout << "Command LOGOUT not implemented." << endl;
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogOff(int /*processId*/,
				  const char* parameters, bool /*all*/) 
{
  ndbout << "Command LOGOFF not implemented." << endl;
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeTestOn(int processId,
				  const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }
  struct ndb_mgm_reply reply;
  int result = ndb_mgm_start_signallog(m_mgmsrv, processId, &reply);
  if (result != 0) {
    ndbout_c("Execute TESTON failed.");
    printError();
  }
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeTestOff(int processId,
				   const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }
  struct ndb_mgm_reply reply;
  int result = ndb_mgm_stop_signallog(m_mgmsrv, processId, &reply);
  if (result != 0) {
    ndbout_c("Execute TESTOFF failed.");
    printError();
  }
}


//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeSet(int /*processId*/, 
			       const char* parameters, bool /*all*/) 
{
  if (emptyString(parameters)) {
    ndbout << "Missing parameter name." << endl;
    return;
  }
#if 0
  // Copy parameters since strtok will modify it
  char* newpar = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1(newpar);
  char* configParameterName = strtok(newpar, " ");

  char* allAfterParameterName = strtok(NULL, "\0");
  if (emptyString(allAfterParameterName)) {
    ndbout << "Missing parameter value." << endl;
    return;
  }

  char* value = strtok(allAfterParameterName, " ");

  char* allAfterValue = strtok(NULL, "\0");
  if (! emptyString(allAfterValue)) {
    ndbout << "Nothing expected after parameter value." << endl;
    return;
  }

  bool configBackupFileUpdated;
  bool configPrimaryFileUpdated;
  
  // TODO The handling of the primary and backup config files should be 
  // analysed further.
  // How it should be handled if only the backup is possible to write.

  int result = _mgmtSrvr.updateConfigParam(processId, configParameterName, 
					   value, configBackupFileUpdated, 
					   configPrimaryFileUpdated);
  if (result == 0) {
    if (configBackupFileUpdated && configPrimaryFileUpdated) {
      ndbout << "The configuration is updated." << endl;
    }
    else if (configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "The configuration is updated but it was only possible " 
	     << "to update the backup configuration file, not the primary." 
	     << endl;
    }
    else {
      assert(false);
    }
  }
  else {
    ndbout << get_error_text(result) << endl;
    if (configBackupFileUpdated && configPrimaryFileUpdated) {
      ndbout << "The configuration files are however updated and "
	     << "the value will be used next time the process is restarted." 
	     << endl;
    }
    else if (configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "It was only possible to update the backup "
	     << "configuration file, not the primary." << endl;
    }
    else if (!configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "The configuration files are not updated." << endl;
    }
    else {
      // The primary is not tried to write if the write of backup file fails
      abort();
    }
  }
#endif
}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeGetStat(int /*processId*/,
					const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

#if 0
  MgmtSrvr::Statistics statistics;
  int result = _mgmtSrvr.getStatistics(processId, statistics);
  if (result != 0) {
    ndbout << get_error_text(result) << endl;
    return;
  }
#endif
  // Print statistic...
  /*
  ndbout << "Number of GETSTAT commands: " 
  << statistics._test1 << endl;
  */
}

//*****************************************************************************
//*****************************************************************************
				 
void 
CommandInterpreter::executeEventReporting(int processId,
					  const char* parameters, 
					  bool all) 
{
  if (emptyString(parameters)) {
    ndbout << "Expected argument" << endl;
    return;
  }
  BaseString tmp(parameters);
  Vector<BaseString> specs;
  tmp.split(specs, " ");

  for (int i=0; i < specs.size(); i++)
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
    } else {
      ndbout_c(" OK!"); 
    }
  }
}

/*****************************************************************************
 * Backup
 *****************************************************************************/
int
CommandInterpreter::executeStartBackup(char* parameters)
{
  struct ndb_mgm_reply reply;
  unsigned int backupId;
#if 0
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP, 0 };
  int fd = ndb_mgm_listen_event(m_mgmsrv, filter);
  if (fd < 0)
  {
    ndbout << "Initializing start of backup failed" << endl;
    printError();
    return fd;
  }
#endif
  Vector<BaseString> args;
  {
    BaseString(parameters).split(args);
    for (unsigned i= 0; i < args.size(); i++)
      if (args[i].length() == 0)
	args.erase(i--);
      else
	args[i].ndb_toupper();
  }
  int sz= args.size();

  int result;
  if (sz == 2 &&
      args[1] == "NOWAIT")
  {
    result = ndb_mgm_start_backup(m_mgmsrv, 0, &backupId, &reply);
  }
  else if (sz == 1 ||
	   (sz == 3 &&
	    args[1] == "WAIT" &&
	    args[2] == "COMPLETED"))
  {
    ndbout_c("Waiting for completed, this may take several minutes");
    result = ndb_mgm_start_backup(m_mgmsrv, 2, &backupId, &reply);
  }
  else if (sz == 3 &&
	   args[1] == "WAIT" &&
	   args[2] == "STARTED")
  {
    ndbout_c("Waiting for started, this may take several minutes");
    result = ndb_mgm_start_backup(m_mgmsrv, 1, &backupId, &reply);
  }
  else
  {
    invalid_command(parameters);
    return -1;
  }

  if (result != 0) {
    ndbout << "Backup failed" << endl;
    printError();
#if 0
    close(fd);
#endif
    return result;
  }
#if 0
  ndbout_c("Waiting for completed, this may take several minutes");
  char *tmp;
  char buf[1024];
  {
    SocketInputStream in(fd);
    int count = 0;
    do {
      tmp = in.gets(buf, 1024);
      if(tmp)
      {
	ndbout << tmp;
	unsigned int id;
	if(sscanf(tmp, "%*[^:]: Backup %d ", &id) == 1 && id == backupId){
	  count++;
	}
      }
    } while(count < 2);
  }

  SocketInputStream in(fd, 10);
  do {
    tmp = in.gets(buf, 1024);
    if(tmp && tmp[0] != 0)
    {
      ndbout << tmp;
    }
  } while(tmp && tmp[0] != 0);

  close(fd);
#endif  
  return 0;
}

void
CommandInterpreter::executeAbortBackup(char* parameters) 
{
  int bid = -1;
  struct ndb_mgm_reply reply;
  if (emptyString(parameters))
    goto executeAbortBackupError1;

  {
    strtok(parameters, " ");
    char* id = strtok(NULL, "\0");
    if(id == 0 || sscanf(id, "%d", &bid) != 1)
      goto executeAbortBackupError1;
  }
  {
    int result= ndb_mgm_abort_backup(m_mgmsrv, bid, &reply);
    if (result != 0) {
      ndbout << "Abort of backup " << bid << " failed" << endl;
      printError();
    } else {
      ndbout << "Abort of backup " << bid << " ordered" << endl;
    }
  }
  return;
 executeAbortBackupError1:
  ndbout << "Invalid arguments: expected <BackupId>" << endl;
  return;
}

#ifdef HAVE_GLOBAL_REPLICATION
/*****************************************************************************
 * Global Replication
 *
 * For information about the different commands, see
 * GrepReq::Request in file signaldata/grepImpl.cpp.
 *
 * Below are commands as of 2003-07-05 (may change!):
 * START = 0,            ///< Start Global Replication (all phases)
 * START_METALOG = 1,    ///< Start Global Replication (all phases)
 * START_METASCAN = 2,   ///< Start Global Replication (all phases)
 * START_DATALOG = 3,    ///< Start Global Replication (all phases)
 * START_DATASCAN = 4,   ///< Start Global Replication (all phases)
 * START_REQUESTOR = 5,  ///< Start Global Replication (all phases)
 * ABORT = 6,            ///< Immediate stop (removes subscription)
 * SLOW_STOP = 7,        ///< Stop after finishing applying current GCI epoch
 * FAST_STOP = 8,        ///< Stop after finishing applying all PS GCI epochs
 * START_TRANSFER = 9,   ///< Start SS-PS transfer
 * STOP_TRANSFER = 10,   ///< Stop SS-PS transfer
 * START_APPLY = 11,     ///< Start applying GCI epochs in SS
 * STOP_APPLY = 12,      ///< Stop applying GCI epochs in SS
 * STATUS = 13,           ///< Status
 * START_SUBSCR = 14,
 * REMOVE_BUFFERS = 15,
 * DROP_TABLE = 16

 *****************************************************************************/

void
CommandInterpreter::executeRep(char* parameters) 
{
  if (emptyString(parameters)) {
    ndbout << helpTextRep;
    return;
  }

  char * line = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1((char*)line);
  char * firstToken = strtok(line, " ");
  
  struct ndb_rep_reply  reply;
  unsigned int          repId;


  if (!strcasecmp(firstToken, "CONNECT")) {
    char * host = strtok(NULL, "\0");
    for (unsigned int i = 0; i < strlen(host); ++i) {
      host[i] = tolower(host[i]);
    }
    
    if(host == NULL)
    {
      ndbout_c("host:port must be specified.");
      return;
    }
    
    if(rep_connected) {
      if(m_repserver != NULL) {
	ndb_rep_disconnect(m_repserver);
	rep_connected = false;
      }       
    }
          
    if(m_repserver == NULL)
      m_repserver = ndb_rep_create_handle();
    if(ndb_rep_connect(m_repserver, host) < 0)
      ndbout_c("Failed to connect to %s", host); 
    else
      rep_connected=true;
    return;
    
    if(!rep_connected) {
      ndbout_c("Not connected to REP server");
    }
  }
    
  /********
   * START 
   ********/
  if (!strcasecmp(firstToken, "START")) {
    
    unsigned int          req;
    char *startType = strtok(NULL, "\0");
    
    if (startType == NULL) {                
      req = GrepReq::START;
    } else if (!strcasecmp(startType, "SUBSCRIPTION")) {  
      req = GrepReq::START_SUBSCR;
    } else if (!strcasecmp(startType, "METALOG")) { 
      req = GrepReq::START_METALOG;
    } else if (!strcasecmp(startType, "METASCAN")) {
      req = GrepReq::START_METASCAN;
    } else if (!strcasecmp(startType, "DATALOG")) {
      req = GrepReq::START_DATALOG;
    } else if (!strcasecmp(startType, "DATASCAN")) {
      req = GrepReq::START_DATASCAN;
    } else if (!strcasecmp(startType, "REQUESTOR")) {
      req = GrepReq::START_REQUESTOR;
    } else if (!strcasecmp(startType, "TRANSFER")) {
      req = GrepReq::START_TRANSFER;
    } else if (!strcasecmp(startType, "APPLY")) {
      req = GrepReq::START_APPLY;
    } else if (!strcasecmp(startType, "DELETE")) {
      req = GrepReq::START_DELETE;
    } else {
      ndbout_c("Illegal argument to command 'REPLICATION START'");
      return;
    }

    int result = ndb_rep_command(m_repserver, req, &repId, &reply);
    
    if (result != 0) {
      ndbout << "Start of Global Replication failed" << endl;
    } else {
      ndbout << "Start of Global Replication ordered" << endl;
    }
    return;
  }

  /********
   * STOP
   ********/
  if (!strcasecmp(firstToken, "STOP")) {    
    unsigned int          req;
    char *startType = strtok(NULL, " ");
    unsigned int epoch = 0;
    
    if (startType == NULL) {                 
      /**
       * Stop immediately
       */
      req = GrepReq::STOP;
    } else if (!strcasecmp(startType, "EPOCH")) {  
      char *strEpoch = strtok(NULL, "\0");
      if(strEpoch == NULL) {
	ndbout_c("Epoch expected!");
	return;
      }
      req = GrepReq::STOP;
      epoch=atoi(strEpoch);      
    } else if (!strcasecmp(startType, "SUBSCRIPTION")) {  
      req = GrepReq::STOP_SUBSCR;
    } else if (!strcasecmp(startType, "METALOG")) { 
      req = GrepReq::STOP_METALOG;
    } else if (!strcasecmp(startType, "METASCAN")) {
      req = GrepReq::STOP_METASCAN;
    } else if (!strcasecmp(startType, "DATALOG")) {
      req = GrepReq::STOP_DATALOG;
    } else if (!strcasecmp(startType, "DATASCAN")) {
      req = GrepReq::STOP_DATASCAN;
    } else if (!strcasecmp(startType, "REQUESTOR")) {
      req = GrepReq::STOP_REQUESTOR;
    } else if (!strcasecmp(startType, "TRANSFER")) {
      req = GrepReq::STOP_TRANSFER;
    } else if (!strcasecmp(startType, "APPLY")) {
      req = GrepReq::STOP_APPLY;
    } else if (!strcasecmp(startType, "DELETE")) {
      req = GrepReq::STOP_DELETE;
    } else {
      ndbout_c("Illegal argument to command 'REPLICATION STOP'");
      return;
    }
    int result = ndb_rep_command(m_repserver, req, &repId, &reply, epoch);
    
    if (result != 0) {
      ndbout << "Stop command failed" << endl;
    } else {
      ndbout << "Stop ordered" << endl;
    }
    return;
  }

  /*********
   * STATUS
   *********/
  if (!strcasecmp(firstToken, "STATUS")) {
    struct rep_state repstate;
    int result = 
      ndb_rep_get_status(m_repserver, &repId, &reply, &repstate);
    
    if (result != 0) {
      ndbout << "Status request of Global Replication failed" << endl;
    } else {
      ndbout << "Status request of Global Replication ordered" << endl;
      ndbout << "See printout at one of the DB nodes" << endl;
      ndbout << "(Better status report is under development.)" << endl;
      ndbout << " SubscriptionId " << repstate.subid 
	     << " SubscriptionKey " << repstate.subkey << endl;
    }
    return;
  }

  /*********
   * QUERY (see repapi.h for querable counters)
   *********/
  if (!strcasecmp(firstToken, "QUERY")) {
    char *query = strtok(NULL, "\0");
    int queryCounter=-1;
    if(query != NULL) {
      queryCounter = atoi(query);
    }
    struct rep_state repstate;
    unsigned repId = 0;
    int result = ndb_rep_query(m_repserver, (QueryCounter)queryCounter,
			       &repId, &reply, &repstate);
    
    if (result != 0) {
      ndbout << "Query repserver failed" << endl;
    } else {
      ndbout << "Query repserver sucessful" << endl;
      ndbout_c("repstate : QueryCounter %d, f=%d l=%d"
	       " nodegroups %d" , 
	       repstate.queryCounter,
	       repstate.first[0], repstate.last[0],
	       repstate.no_of_nodegroups );
    }
    return;
  }
}
#endif // HAVE_GLOBAL_REPLICATION

template class Vector<char const*>;
