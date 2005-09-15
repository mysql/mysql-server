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

#ifndef MgmtSrvr_H
#define MgmtSrvr_H

#include <kernel_types.h>
#include "Config.hpp"
#include <NdbCondition.h>
#include <mgmapi.h>


#include <Vector.hpp>
#include <NodeBitmask.hpp>
#include <signaldata/ManagementServer.hpp>
#include "SignalQueue.hpp"
#include <ndb_version.h>
#include <EventLogger.hpp>
#include <signaldata/EventSubscribeReq.hpp>

/**
 * @desc Block number for Management server.
 * @todo This should probably be somewhere else. I don't know where atm.
 */
#define MGMSRV 1

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
  
  void log(int eventType, const Uint32* theData, NodeId nodeId);
  
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
    void reserve_node(NodeId id);
    bool is_reserved(NodeId nodeId) { return m_reserved_nodes.get(nodeId); }
    bool is_reserved(NodeBitmask mask) { return !mask.bitAND(m_reserved_nodes).isclear(); }
    bool isclear() { return m_reserved_nodes.isclear(); }
    NodeId get_nodeid() const;
  private:
    MgmtSrvr &m_mgmsrv;
    NodeBitmask m_reserved_nodes;
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

  STATIC_CONST( NO_CONTACT_WITH_PROCESS = 5000 );
  STATIC_CONST( PROCESS_NOT_CONFIGURED = 5001 );
  STATIC_CONST( WRONG_PROCESS_TYPE = 5002 );
  STATIC_CONST( COULD_NOT_ALLOCATE_MEMORY = 5003 );
  STATIC_CONST( SEND_OR_RECEIVE_FAILED = 5005 );
  STATIC_CONST( INVALID_LEVEL = 5006 );
  STATIC_CONST( INVALID_ERROR_NUMBER = 5007 );
  STATIC_CONST( INVALID_TRACE_NUMBER = 5008 );
  STATIC_CONST( NOT_IMPLEMENTED = 5009 );
  STATIC_CONST( INVALID_BLOCK_NAME = 5010 );

  STATIC_CONST( CONFIG_PARAM_NOT_EXIST = 5011 );
  STATIC_CONST( CONFIG_PARAM_NOT_UPDATEABLE = 5012 );
  STATIC_CONST( VALUE_WRONG_FORMAT_INT_EXPECTED = 5013 );
  STATIC_CONST( VALUE_TOO_LOW = 5014 );
  STATIC_CONST( VALUE_TOO_HIGH = 5015 );
  STATIC_CONST( VALUE_WRONG_FORMAT_BOOL_EXPECTED = 5016 );

  STATIC_CONST( CONFIG_FILE_OPEN_WRITE_ERROR = 5017 );
  STATIC_CONST( CONFIG_FILE_OPEN_READ_ERROR = 5018 );
  STATIC_CONST( CONFIG_FILE_WRITE_ERROR = 5019 );
  STATIC_CONST( CONFIG_FILE_READ_ERROR = 5020 );
  STATIC_CONST( CONFIG_FILE_CLOSE_ERROR = 5021 );

  STATIC_CONST( CONFIG_CHANGE_REFUSED_BY_RECEIVER = 5022 );
  STATIC_CONST( COULD_NOT_SYNC_CONFIG_CHANGE_AGAINST_PHYSICAL_MEDIUM = 5023 );
  STATIC_CONST( CONFIG_FILE_CHECKSUM_ERROR = 5024 );
  STATIC_CONST( NOT_POSSIBLE_TO_SEND_CONFIG_UPDATE_TO_PROCESS_TYPE = 5025 );

  STATIC_CONST( NODE_SHUTDOWN_IN_PROGESS = 5026 );
  STATIC_CONST( SYSTEM_SHUTDOWN_IN_PROGRESS = 5027 );
  STATIC_CONST( NODE_SHUTDOWN_WOULD_CAUSE_SYSTEM_CRASH = 5028 );
  STATIC_CONST( NO_CONTACT_WITH_CLUSTER = 6666 );
  STATIC_CONST( OPERATION_IN_PROGRESS = 6667 );
  
  STATIC_CONST( NO_CONTACT_WITH_DB_NODES = 5030 );
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
  bool start(BaseString &error_string);

  ~MgmtSrvr();

  int status(int processId, 
	     ndb_mgm_node_status * status, 
	     Uint32 * version,
	     Uint32 * phase,
	     bool * systemShutdown,
	     Uint32 * dynamicId,
	     Uint32 * nodeGroup,
	     Uint32 * connectCount);
  
  // All the functions below may return any of this error codes:
  // NO_CONTACT_WITH_PROCESS, PROCESS_NOT_CONFIGURED, WRONG_PROCESS_TYPE,
  // COULD_NOT_ALLOCATE_MEMORY, SEND_OR_RECEIVE_FAILED


  typedef void (* StopCallback)(int nodeId, void * anyData, int errorCode);

  typedef void (* VersionCallback)(int nodeId, int version,
				   void * anyData, int errorCode);


  typedef void (* EnterSingleCallback)(int nodeId, void * anyData, 
				       int errorCode);
  typedef void (* ExitSingleCallback)(int nodeId, void * anyData, 
				      int errorCode);

  /**
   * Lock configuration
   */
  int lockConf();

  /**
   * Unlock configuration, and commit it if commit is true
   */
  int unlockConf(bool commit);

  /**
   * Commit new configuration
   */
  int commitConfig();

  /**
   * Rollback configuration
   */
  int rollbackConfig();

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
  int stopNode(int nodeId, bool abort = false, StopCallback = 0, void *any= 0);

  /**
   *   Stop the system
   */
  int stop(int * cnt = 0, bool abort = false, StopCallback = 0, void *any = 0);

  /**
   *   print version info about a node
   * 
   *   @param   processId: Id of the DB process to stop
   *   @return  0 if succeeded, otherwise: as stated above, plus:
   */
  int versionNode(int nodeId, bool abort = false, 
		  VersionCallback = 0, void *any= 0);

  /**
   *   print version info about all node in the system
   */
  int version(int * cnt = 0, bool abort = false, 
	      VersionCallback = 0, void *any = 0);
  
  /**
   *   Maintenance on the system
   */
  int enterSingleUser(int * cnt = 0, Uint32 singleuserNodeId = 0,
		      EnterSingleCallback = 0, void *any = 0);


  /**
   *   Resume from maintenance on the system
   */
  int exitSingleUser(int * cnt = 0, bool abort = false, 
	     ExitSingleCallback = 0, void *any = 0);

  /**
   *   Start DB process.
   *   @param   processId: Id of the DB process to start
   *   @return 0 if succeeded, otherwise: as stated above, plus:
   */
 int start(int processId);

  /**
   *   Restart a node
   *   @param processId: Id of the DB process to start
   */
  int restartNode(int processId, bool nostart, bool initialStart, 
		  bool abort = false,
		  StopCallback = 0, void * anyData = 0);
  
  /**
   *   Restart the system
   */
  int restart(bool nostart, bool initialStart, 
	      bool abort = false,
	      int * stopCount = 0, StopCallback = 0, void * anyData = 0);
  
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
	Uint32 BackupId;
	Uint32 NoOfBytes;
	Uint32 NoOfRecords;
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
		     struct sockaddr *client_addr, SOCKET_SIZE_TYPE *client_addr_len,
		     BaseString &error_string);
  
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
   *   Change configuration paramter
   */
  bool changeConfig(const BaseString &section,
		    const BaseString &param,
		    const BaseString &value);

  /**
   * Returns the node count for the specified node type.
   *
   *  @param type The node type.
   *  @return The number of nodes of the specified type.
   */
  int getNodeCount(enum ndb_mgm_node_type type) const;

  /**
   * Returns the nodeId of the management master
   */
  NodeId getPrimaryNode() const;

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

  int set_connect_string(const char *str);

  void transporter_connect(NDB_SOCKET_TYPE sockfd);

  ConfigRetriever *get_config_retriever() { return m_config_retriever; };

  const char *get_connect_address(Uint32 node_id);
  void get_connected_nodes(NodeBitmask &connected_nodes) const;
  SocketServer *get_socket_server() { return m_socket_server; }

  //**************************************************************************
private:
  //**************************************************************************

  int setEventReportingLevel(int processId, LogLevel::EventCategory, Uint32); 
  
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

  void handle_MGM_LOCK_CONFIG_REQ(NdbApiSignal *signal);
  void handle_MGM_UNLOCK_CONFIG_REQ(NdbApiSignal *signal);

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
    WAIT_SET_VAR,		// Accept SET_VAR_CONF and SET_VAR_REF
    WAIT_SUBSCRIBE_CONF,	// Accept event subscription confirmation
    WAIT_STOP,
    WAIT_BACKUP_STARTED,
    WAIT_BACKUP_COMPLETED,
    WAIT_VERSION,
    WAIT_NODEFAILURE
  };

  /**
   *   Get an unused signal
   *   @return  A signal if succeeded, NULL otherwise
   */
  NdbApiSignal* getSignal();

  /**
   *   Add a signal to the list of unused signals
   *   @param  signal: The signal to add
   */
  void releaseSignal(NdbApiSignal* signal);

  /**
   *   Remove a signal from the list of unused signals and delete
   *   the memory for it.
   */
  void freeSignal();
  
  /**
   *   Send a signal
   *   @param   processId: Id of the receiver process
   *   @param   waitState: State denoting a set of signals we accept to receive
   *   @param   signal: The signal to send
   *   @return  0 if succeeded, -1 otherwise
   */
  int sendSignal(Uint16 processId, WaitSignalType waitState, 
		 NdbApiSignal* signal, bool force = false);
  
  /**
   *   Send a signal and wait for an answer signal
   *   @param   processId: Id of the receiver process
   *   @param   waitState: State denoting a set of signals we accept to receive.
   *   @param   signal: The signal to send
   *   @return  0 if succeeded, -1 otherwise (for example failed to send or 
   *            failed to receive expected signal).
   */
  int sendRecSignal(Uint16 processId, WaitSignalType waitState, 
		    NdbApiSignal* signal, bool force = false,
		    int waitTime = WAIT_FOR_RESPONSE_TIMEOUT);
  
  /**
   *   Wait for a signal to arrive.
   *   @return  0 if signal arrived, -1 otherwise
   */
  int receiveOptimisedResponse(int waitTime);
  
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
  void eventReport(const Uint32 * theData);
 

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

  class TransporterFacade * theFacade;

  class SignalQueue m_signalRecvQueue;

  struct StopRecord {
    StopRecord(){ inUse = false; callback = 0; singleUserMode = false;}
    bool inUse;
    bool singleUserMode;
    int sentCount;
    int reply;
    int nodeId;
    void * anyData;
    StopCallback callback;
  };
  StopRecord m_stopRec;

  struct VersionRecord {
    VersionRecord(){ inUse = false; callback = 0;}
    bool inUse;
    Uint32 version[MAX_NODES];
    VersionCallback callback;
  };
  VersionRecord m_versionRec;
  int  sendVersionReq( int processId);


  void handleStopReply(NodeId nodeId, Uint32 errCode);
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
  
  /**
   * Handles the thread wich upon a 'Node is started' event will
   * set the node's previous loglevel settings.
   */
  struct NdbThread* _logLevelThread;
  static void *logLevelThread_C(void *);
  void logLevelThreadRun();
  
  struct NdbThread *m_signalRecvThread;
  static void *signalRecvThread_C(void *);
  void signalRecvThreadRun();
  
  Config *_props;

  int send(class NdbApiSignal* signal, Uint32 node, Uint32 node_type);

  ConfigRetriever *m_config_retriever;
};

inline
const Config *
MgmtSrvr::getConfig() const {
  return _config;
}

#endif // MgmtSrvr_H
