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

#ifndef MgmtSrvr_H
#define MgmtSrvr_H

#include <kernel_types.h>
#include "Config.hpp"
#include <NdbCondition.h>
#include <mgmapi.h>

#include <NdbTCP.h>
#include <ConfigRetriever.hpp>
#include <Vector.hpp>
#include <NodeBitmask.hpp>
#include <signaldata/ManagementServer.hpp>
#include <ndb_version.h>
#include <EventLogger.hpp>
#include <signaldata/EventSubscribeReq.hpp>

#include <SignalSender.hpp>

/**
 * @desc Block number for Management server.
 * @todo This should probably be somewhere else. I don't know where atm.
 */
#define MGMSRV 1

#define MGM_ERROR_MAX_INJECT_SESSION_ONLY 10000

extern int g_errorInsert;

class ConfigInfoServer;
class NdbApiSignal;
class Config;
class SetLogLevelOrd;
class SocketServer;

class Ndb_mgmd_event_service : public EventLoggerBase 
{
  friend class MgmtSrvr;
public:
  struct Event_listener : public EventLoggerBase {
    Event_listener() {}
    NDB_SOCKET_TYPE m_socket;
    Uint32 m_parsable;
  };
  
private:  
  class MgmtSrvr * m_mgmsrv;
  MutexVector<Event_listener> m_clients;
public:
  Ndb_mgmd_event_service(class MgmtSrvr * m) : m_clients(5) {
    m_mgmsrv = m;
  }
  
  void add_listener(const Event_listener&);
  void check_listeners();
  void update_max_log_level(const LogLevel&);
  void update_log_level(const LogLevel&);
  
  void log(int eventType, const Uint32* theData, Uint32 len, NodeId nodeId);
  
  void stop_sessions();

  Event_listener& operator[](unsigned i) { return m_clients[i]; }
  const Event_listener& operator[](unsigned i) const { return m_clients[i]; }
  void lock() { m_clients.lock(); }
  void unlock(){ m_clients.unlock(); }
};

/**
 * @class MgmtSrvr
 * @brief Main class for the management server. 
 *
 * It has one interface to be used by a local client. 
 * With the methods it's possible to send different kind of commands to 
 * DB processes, as log level, set trace number etc. 
 *
 * A MgmtSrvr creates a ConfigInfoServer which serves request on TCP sockets. 
 * The requests come typical from DB and API processes which want
 * to fetch its configuration parameters. The MgmtSrvr knows about the
 * configuration by reading a configuration file.
 *
 * The MgmtSrvr class corresponds in some ways to the Ndb class in API. 
 * It creates a TransporterFacade, receives signals and defines signals
 * to send and receive.
 */
class MgmtSrvr {
  
public:
  // some compilers need all of this
  class Allocated_resources;
  friend class Allocated_resources;
  class Allocated_resources {
  public:
    Allocated_resources(class MgmtSrvr &m);
    ~Allocated_resources();
    // methods to reserve/allocate resources which
    // will be freed when running destructor
    void reserve_node(NodeId id, NDB_TICKS timeout);
    bool is_timed_out(NDB_TICKS tick);
    bool is_reserved(NodeId nodeId) { return m_reserved_nodes.get(nodeId); }
    bool is_reserved(NodeBitmask mask) { return !mask.bitAND(m_reserved_nodes).isclear(); }
    bool isclear() { return m_reserved_nodes.isclear(); }
    NodeId get_nodeid() const;
  private:
    MgmtSrvr &m_mgmsrv;
    NodeBitmask m_reserved_nodes;
    NDB_TICKS m_alloc_timeout;
  };
  NdbMutex *m_node_id_mutex;

  /**
   * Start/initate the event log.
   */
  void startEventLog();

  /**
   * Stop the event log.
   */
  void stopEventLog();

  /**
   * Enable/disable eventlog log levels/severities.
   *
   * @param serverity the log level/serverity.
   * @return true if the severity was enabled.
   */
  bool setEventLogFilter(int severity, int enable);

  /**
   * Returns true if the log level/severity is enabled.
   *
   * @param severity the severity level.
   */
  bool isEventLogFilterEnabled(int severity);

  /**
   *   This enum specifies the different signal loggig modes possible to set 
   *   with the setSignalLoggingMode method.
   */
  enum LogMode {In, Out, InOut, Off};

  /* Constructor */

  MgmtSrvr(SocketServer *socket_server,
	   const char *config_filename,      /* Where to save config */
	   const char *connect_string); 
  int init();
  NodeId getOwnNodeId() const {return _ownNodeId;};

  /**
   *   Read (initial) config file, create TransporterFacade, 
   *   define signals, create ConfigInfoServer.
   *   @return true if succeeded, otherwise false
   */
  bool check_start(); // may be run before start to check that some things are ok
  bool start(BaseString &error_string, const char * bindaddress = 0);

  ~MgmtSrvr();

  /**
   * Get status on a node.
   * address may point to a common area (e.g. from inet_addr)
   * There is no gaurentee that it is preserved across calls.
   * Copy the string if you are not going to use it immediately.
   */
  int status(int nodeId,
	     ndb_mgm_node_status * status,
	     Uint32 * version,
	     Uint32 * mysql_version,
	     Uint32 * phase,
	     bool * systemShutdown,
	     Uint32 * dynamicId,
	     Uint32 * nodeGroup,
	     Uint32 * connectCount,
	     const char **address);
  
  // All the functions below may return any of this error codes:
  // NO_CONTACT_WITH_PROCESS, PROCESS_NOT_CONFIGURED, WRONG_PROCESS_TYPE,
  // COULD_NOT_ALLOCATE_MEMORY, SEND_OR_RECEIVE_FAILED

  /**
   * Save a configuration to permanent storage
   */
  int saveConfig(const Config *);

  /**
   * Save the running configuration
   */
  int saveConfig() {
    return saveConfig(_config);
  };

  /**
   * Read configuration from file, or from another MGM server
   */
  Config *readConfig();

  /**
   * Fetch configuration from another MGM server
   */
  Config *fetchConfig();

  /**
   *   Stop a node
   * 
   *   @param   processId: Id of the DB process to stop
   *   @return  0 if succeeded, otherwise: as stated above, plus:
   */
  int stopNodes(const Vector<NodeId> &node_ids, int *stopCount, bool abort,
                int *stopSelf);

  int shutdownMGM(int *stopCount, bool abort, int *stopSelf);

  /**
   * shutdown the DB nodes
   */
  int shutdownDB(int * cnt = 0, bool abort = false);

  /**
   *   print version info about a node
   * 
   *   @param   processId: Id of the DB process to stop
   *   @return  0 if succeeded, otherwise: as stated above, plus:
   */
  int versionNode(int nodeId, Uint32 &version, Uint32 &mysql_version,
		  const char **address);

  /**
   *   Maintenance on the system
   */
  int enterSingleUser(int * cnt = 0, Uint32 singleuserNodeId = 0);


  /**
   *   Resume from maintenance on the system
   */
  int exitSingleUser(int * cnt = 0, bool abort = false);

  /**
   *   Start DB process.
   *   @param   processId: Id of the DB process to start
   *   @return 0 if succeeded, otherwise: as stated above, plus:
   */
 int start(int processId);

  /**
   *   Restart nodes
   *   @param processId: Id of the DB process to start
   */
  int restartNodes(const Vector<NodeId> &node_ids,
                   int *stopCount, bool nostart,
                   bool initialStart, bool abort, int *stopSelf);
  
  /**
   *   Restart all DB nodes
   */
  int restartDB(bool nostart, bool initialStart, 
                bool abort = false,
                int * stopCount = 0);
  
  struct BackupEvent {
    enum Event {
      BackupStarted = 1,
      BackupFailedToStart = 2,
      BackupCompleted = 3,
      BackupAborted = 4
    } Event;
    
    NdbNodeBitmask Nodes;
    union {
      struct {
	Uint32 BackupId;
      } Started ;
      struct {
	Uint32 ErrorCode;
      } FailedToStart ;
      struct {
	Uint64 NoOfBytes;
	Uint64 NoOfRecords;
	Uint32 BackupId;
	Uint32 NoOfLogBytes;
	Uint32 NoOfLogRecords;
	Uint32 startGCP;
	Uint32 stopGCP;
      } Completed ;
      struct {
	Uint32 BackupId;
	Uint32 Reason;
	Uint32 ErrorCode;
      } Aborted ;
    };
  };
  
  /**
   * Backup functionallity
   */
  int startBackup(Uint32& backupId, int waitCompleted= 2);
  int abortBackup(Uint32 backupId);
  int performBackup(Uint32* backupId);

  //**************************************************************************
  // Description: Set event report level for a DB process
  // Parameters:
  //  processId: Id of the DB process
  //  level: Event report level
  //  isResend: Flag to indicate for resending log levels during node restart
  // Returns: 0 if succeeded, otherwise: as stated above, plus:
  //  INVALID_LEVEL
  //**************************************************************************

  int setEventReportingLevelImpl(int processId, const EventSubscribeReq& ll);
  int setNodeLogLevelImpl(int processId, const SetLogLevelOrd & ll);

  /**
   *   Insert an error in a DB process.
   *   @param   processId: Id of the DB process
   *   @param   errorNo: The error number. > 0.
   *   @return  0 if succeeded, otherwise: as stated above, plus:
   *            INVALID_ERROR_NUMBER
   */
  int insertError(int processId, int errorNo);



  int setTraceNo(int processId, int traceNo);
  //**************************************************************************
  // Description: Set trace number in a DB process.
  // Parameters:
  //  processId: Id of the DB process
  //  trace: Trace number
  // Returns: 0 if succeeded, otherwise: as stated above, plus:
  //  INVALID_TRACE_NUMBER
  //**************************************************************************


  int setSignalLoggingMode(int processId, LogMode mode, 
			   const Vector<BaseString> &blocks);

  int setSignalLoggingMode(int processId, LogMode mode,
			   BaseString &block) {
    Vector<BaseString> v;
    v.push_back(block);
    return setSignalLoggingMode(processId, mode, v);
  }
  //**************************************************************************
  // Description: Set signal logging mode for blocks in a DB process.
  // Parameters:
  //  processId: Id of the DB process
  //  mode: The log mode
  //  blocks: Which blocks to be affected (container of strings)
  // Returns: 0 if succeeded, otherwise: as stated above, plus:
  //  INVALID_BLOCK_NAME
  //**************************************************************************


  int startSignalTracing(int processId);
  //**************************************************************************
  // Description: Start signal tracing for a DB process.
  // Parameters:
  //  processId: Id of the DB process
  // Returns: 0 if succeeded, otherwise: as stated above.
  //**************************************************************************


  int stopSignalTracing(int processId);
  //**************************************************************************
  // Description: Stop signal tracing for a DB process.
  // Parameters:
  //  processId: Id of the DB process
  // Returns: 0 if succeeded, otherwise: as stated above.
  //**************************************************************************

  /**
   *   Dump State 
   */
  int dumpState(int processId, const Uint32 args[], Uint32 argNo);
  int dumpState(int processId, const char* args);

  /**
   * Get next node id (node id gt that _nodeId)
   *  of specified type and save it in _nodeId
   *
   *   @return false if none found
   */
  bool getNextNodeId(NodeId * _nodeId, enum ndb_mgm_node_type type) const ;
  bool alloc_node_id(NodeId * _nodeId, enum ndb_mgm_node_type type,
		     struct sockaddr *client_addr,
                     SOCKET_SIZE_TYPE *client_addr_len,
		     int &error_code, BaseString &error_string,
                     int log_event = 1);
  
  /**
   *
   */
  enum ndb_mgm_node_type getNodeType(NodeId) const;

  /**
   *   Get error text
   * 
   *   @param   errorCode: Error code to get a match error text for.
   *   @return  The error text.
   */
  const char* getErrorText(int errorCode, char *buf, int buf_sz);

  /**
   *   Get configuration
   */
  const Config * getConfig() const;

  /**
   * Returns the node count for the specified node type.
   *
   *  @param type The node type.
   *  @return The number of nodes of the specified type.
   */
  int getNodeCount(enum ndb_mgm_node_type type) const;

  /**
   * Returns the port number.
   * @return port number.
   */
  int getPort() const;

  int setDbParameter(int node, int parameter, const char * value, BaseString&);
  int setConnectionDbParameter(int node1, int node2, int param, int value,
			       BaseString& msg);
  int getConnectionDbParameter(int node1, int node2, int param,
			       int *value, BaseString& msg);

  int connect_to_self(const char* bindaddress = 0);

  void transporter_connect(NDB_SOCKET_TYPE sockfd);

  ConfigRetriever *get_config_retriever() { return m_config_retriever; };

  const char *get_connect_address(Uint32 node_id);
  void get_connected_nodes(NodeBitmask &connected_nodes) const;
  SocketServer *get_socket_server() { return m_socket_server; }

  void updateStatus();

  //**************************************************************************
private:
  //**************************************************************************

  int sendStopMgmd(NodeId nodeId,
                   bool abort,
                   bool stop,
                   bool restart,
                   bool nostart,
                   bool initialStart);

  int sendSTOP_REQ(const Vector<NodeId> &node_ids,
		   NdbNodeBitmask &stoppedNodes,
		   Uint32 singleUserNodeId,
		   bool abort,
		   bool stop,
		   bool restart,
		   bool nostart,
		   bool initialStart,
                   int *stopSelf);
 
  /**
   *   Check if it is possible to send a signal to a (DB) process
   *
   *   @param   processId: Id of the process to send to
   *   @return  0 OK, 1 process dead, 2 API or MGMT process, 3 not configured
   */
  int okToSendTo(NodeId nodeId, bool unCond = false);

  /**
   *   Get block number for a block
   *
   *   @param   blockName: Block to get number for
   *   @return  -1 if block not found, otherwise block number
   */
  int getBlockNumber(const BaseString &blockName);

  int alloc_node_id_req(NodeId free_node_id, enum ndb_mgm_node_type type);
  //**************************************************************************
  
  int _blockNumber;
  NodeId _ownNodeId;
  SocketServer *m_socket_server;

  BlockReference _ownReference; 
  NdbMutex *m_configMutex;
  const Config * _config;
  Config * m_newConfig;
  BaseString m_configFilename;
  Uint32 m_nextConfigGenerationNumber;
  
  NodeBitmask m_reserved_nodes;
  struct in_addr m_connect_address[MAX_NODES];

  //**************************************************************************
  // Specific signal handling methods
  //**************************************************************************

  static void defineSignals(int blockNumber);
  //**************************************************************************
  // Description: Define all signals to be sent or received for a block
  // Parameters:
  //  blockNumber: The block number send/receive
  // Returns: -
  //**************************************************************************

  void handleReceivedSignal(NdbApiSignal* signal);
  //**************************************************************************
  // Description: This method is called from "another" thread when a signal
  //  is received. If expect the received signal and succeed to handle it
  //  we signal with a condition variable to the waiting
  //  thread (receiveOptimisedResponse) that the signal has arrived.
  // Parameters:
  //  signal: The recieved signal
  // Returns: -
  //**************************************************************************

  void handleStatus(NodeId nodeId, bool alive, bool nfComplete);
  //**************************************************************************
  // Description: Handle the death of a process
  // Parameters:
  //  processId: Id of the dead process.
  // Returns: -
  //**************************************************************************

  //**************************************************************************
  // Specific signal handling data
  //**************************************************************************


  //**************************************************************************
  //**************************************************************************
  // General signal handling methods
  // This functions are more or less copied from the Ndb class.

  
  /**
   * WaitSignalType defines states where each state define a set of signals
   * we accept to receive. 
   * The state is set after we have sent a signal.
   * When a signal arrives we first check current state (handleReceivedSignal)
   * to verify that we expect the arrived signal. 
   * It's only then we are in state accepting the arrived signal 
   * we handle the signal.
   */
  enum WaitSignalType { 
    NO_WAIT,			// We don't expect to receive any signal
    WAIT_SUBSCRIBE_CONF 	// Accept event subscription confirmation
  };
  
  /**
   *   This function is called from "outside" of MgmtSrvr
   *   when a signal is sent to MgmtSrvr.
   *   @param  mgmtSrvr: The MgmtSrvr object which shall recieve the signal.
   *   @param  signal: The received signal.
   */
  static void signalReceivedNotification(void* mgmtSrvr, 
					 NdbApiSignal* signal, 
					 struct LinearSectionPtr ptr[3]);

  /**
   *   Called from "outside" of MgmtSrvr when a DB process has died.
   *   @param  mgmtSrvr:   The MgmtSrvr object wreceiveOptimisedResponsehich 
   *                       shall receive the notification.
   *   @param  processId:  Id of the dead process.
   */
  static void nodeStatusNotification(void* mgmSrv, Uint32 nodeId, 
				     bool alive, bool nfCompleted);
  
  /**
   * An event from <i>nodeId</i> has arrived
   */
  void eventReport(const Uint32 * theData, Uint32 len);
 

  //**************************************************************************
  //**************************************************************************
  // General signal handling data

  STATIC_CONST( WAIT_FOR_RESPONSE_TIMEOUT = 300000 ); // Milliseconds
  // Max time to wait for a signal to arrive

  NdbApiSignal* theSignalIdleList;
  // List of unused signals
  
  Uint32 theWaitNode;
  WaitSignalType theWaitState;
  // State denoting a set of signals we accept to recieve.

  NdbCondition* theMgmtWaitForResponseCondPtr; 
  // Condition variable used when we wait for a signal to arrive/a 
  // signal arrives.
  // We wait in receiveOptimisedResponse and signal in handleReceivedSignal.

  NdbMgmHandle m_local_mgm_handle;
  char m_local_mgm_connect_string[20];
  class TransporterFacade * theFacade;

  int  sendVersionReq( int processId, Uint32 &version, Uint32& mysql_version,
		       const char **address);
  int translateStopRef(Uint32 errCode);
  
  bool _isStopThread;
  int _logLevelThreadSleep;
  MutexVector<NodeId> m_started_nodes;
  MutexVector<EventSubscribeReq> m_log_level_requests;
  LogLevel m_nodeLogLevel[MAX_NODES];
  enum ndb_mgm_node_type nodeTypes[MAX_NODES];
  friend class MgmApiSession;
  friend class Ndb_mgmd_event_service;
  Ndb_mgmd_event_service m_event_listner;
  
  NodeId m_master_node;

  /**
   * Handles the thread wich upon a 'Node is started' event will
   * set the node's previous loglevel settings.
   */
  struct NdbThread* _logLevelThread;
  static void *logLevelThread_C(void *);
  void logLevelThreadRun();
  
  Config *_props;

  ConfigRetriever *m_config_retriever;
};

inline
const Config *
MgmtSrvr::getConfig() const {
  return _config;
}

#endif // MgmtSrvr_H
