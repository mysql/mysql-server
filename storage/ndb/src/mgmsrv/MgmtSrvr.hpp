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

#ifndef MgmtSrvr_H
#define MgmtSrvr_H

#include "Config.hpp"
#include "ConfigSubscriber.hpp"

#include <mgmapi.h>
#include <Vector.hpp>
#include <NodeBitmask.hpp>
#include <ndb_version.h>
#include <EventLogger.hpp>

#include <SignalSender.hpp>

#define MGM_ERROR_MAX_INJECT_SESSION_ONLY 10000

class SetLogLevelOrd;

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
  @class MgmtSrvr
  @brief Main class for the management server.
 */
class MgmtSrvr : private ConfigSubscriber, public trp_client {

public:
  /**
   *   This enum specifies the different signal loggig modes possible to set 
   *   with the setSignalLoggingMode method.
   */
  enum LogMode {In, Out, InOut, Off};

  /**
     @struct MgmtOpts
     @brief Options used to control how the management server is started
  */

  struct MgmtOpts {
    int daemon;
    int non_interactive;
    int interactive;
    const char* config_filename;
    int mycnf;
    int config_cache;
    const char* bind_address;
    int no_nodeid_checks;
    int print_full_config;
    const char* configdir;
    int verbose;
    MgmtOpts() : configdir(MYSQLCLUSTERDIR) {};
    int reload;
    int initial;
    NodeBitmask nowait_nodes;
  };

  MgmtSrvr(); // Not implemented
  MgmtSrvr(const MgmtSrvr&); // Not implemented
  MgmtSrvr(const MgmtOpts&);

  ~MgmtSrvr();

private:
  /* Function used from 'init' */
  const char* check_configdir() const;

public:
  /*
    To be called after constructor.
  */
  bool init();

  /*
    To be called after 'init', starts up the services
    this server will expose
   */
  bool start(void);
private:
  /* Functions used from 'start' */
  bool start_transporter(const Config*);
  bool start_mgm_service(const Config*);
  bool connect_to_self(void);

public:

  NodeId getOwnNodeId() const {return _ownNodeId;};

  /**
   * Get status on a node.
   * address may point to a common area (e.g. from inet_addr)
   * There is no guarentee that it is preserved across calls.
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
	     const char **address,
             char *addr_buf,
             size_t addr_buf_size);

  /**
   *   Stop a list of nodes
   */
  int stopNodes(const Vector<NodeId> &node_ids, int *stopCount, bool abort,
                bool force, int *stopSelf);

  int shutdownMGM(int *stopCount, bool abort, int *stopSelf);

  /**
   * shutdown the DB nodes
   */
  int shutdownDB(int * cnt = 0, bool abort = false);

  /**
   *   Maintenance on the system
   */
  int enterSingleUser(int * cnt = 0, Uint32 singleuserNodeId = 0);


  /**
   *   Resume from maintenance on the system
   */
  int exitSingleUser(int * cnt = 0, bool abort = false);

  /**
   *   Start DB process by sending START_ORD to it.
   *   @param   processId: Id of the DB process to start
   *   @return 0 if succeeded, otherwise: as stated above, plus:
   */
 int sendSTART_ORD(int processId);

  /**
   *   Restart a list of nodes
   */
  int restartNodes(const Vector<NodeId> &node_ids,
                   int *stopCount, bool nostart,
                   bool initialStart, bool abort, bool force,
                   int *stopSelf,
                   unsigned int num_secs_to_wait_for_node = 120);

  /**
   *   Restart all DB nodes
   */
  int restartDB(bool nostart, bool initialStart, 
                bool abort = false,
                int * stopCount = 0,
                unsigned int num_secs_to_wait_for_node = 120);
  
  /**
   * Backup functionallity
   */
  int startBackup(Uint32& backupId, int waitCompleted= 2, Uint32 input_backupId= 0, Uint32 backuppoint= 0);
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
  int insertError(int processId, int errorNo, Uint32 * extra = 0);



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
   * Get next node id (node id gt than _nodeId)
   *  of specified type and save it in _nodeId
   *
   *   @return false if none found
   */
  bool getNextNodeId(NodeId * _nodeId, enum ndb_mgm_node_type type) const ;

  bool change_config(Config& new_config, BaseString& msg);

  /**
   *   Get error text
   * 
   *   @param   errorCode: Error code to get a match error text for.
   *   @return  The error text.
   */
  const char* getErrorText(int errorCode, char *buf, int buf_sz);

private:
  void config_changed(NodeId, const Config*);
  void setClusterLog(const Config* conf);
  void configure_eventlogger(const BaseString& logdestination) const;
public:

  /**
   * Returns the port number where MgmApiService is started
   * @return port number.
   */
  int getPort() const { return m_port; };

  int setDbParameter(int node, int parameter, const char * value, BaseString&);
  int setConnectionDbParameter(int node1, int node2, int param, int value,
			       BaseString& msg);

  struct DynPortSpec {
   int node; /* The node which should use below port */
   int port; /* The port to use */
  };
  bool setDynamicPorts(int node, DynPortSpec ports[],
                       unsigned num_ports, BaseString &msg);
  int getConnectionDbParameter(int node1, int node2, int param,
			       int *value, BaseString& msg);

  bool transporter_connect(NDB_SOCKET_TYPE sockfd,
                           BaseString& errormsg,
                           bool& close_with_reset);

  SocketServer *get_socket_server() { return &m_socket_server; }

  int createNodegroup(int *nodes, int count, int *ng);
  int dropNodegroup(int ng);

  int startSchemaTrans(SignalSender& ss, NodeId & out_nodeId,
                       Uint32 transId, Uint32 & out_transKey);
  int endSchemaTrans(SignalSender& ss, NodeId nodeId,
                     Uint32 transId, Uint32 transKey, Uint32 flags);

private:
  int guess_master_node(SignalSender&);

  void status_api(int nodeId,
                  ndb_mgm_node_status& node_status,
                  Uint32& version, Uint32& mysql_version,
                  const char **address,
                  char *addr_buf,
                  size_t addr_buf_size);
  void status_mgmd(NodeId node_id,
                   ndb_mgm_node_status& node_status,
                   Uint32& version, Uint32& mysql_version,
                   const char **address,
                   char *addr_buf,
                   size_t addr_buf_size);

  int sendVersionReq(int processId,
                     Uint32 &version,
                     Uint32& mysql_version,
                     const char **address,
                     char *addr_buf,
                     size_t addr_buf_size);

  int sendStopMgmd(NodeId nodeId,
                   bool abort,
                   bool stop,
                   bool restart,
                   bool nostart,
                   bool initialStart);

  int sendall_STOP_REQ(NodeBitmask &stoppedNodes,
                       bool abort,
                       bool stop,
                       bool restart,
                       bool nostart,
                       bool initialStart);

  int sendSTOP_REQ(const Vector<NodeId> &node_ids,
		   NodeBitmask &stoppedNodes,
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

  bool is_any_node_starting(void);
  bool is_any_node_stopping(void);
  bool is_cluster_single_user(void);

  //**************************************************************************

  const MgmtOpts& m_opts;
  int _blockNumber;
  NodeId _ownNodeId;
  Uint32 m_port;
  SocketServer m_socket_server;

  NdbMutex* m_local_config_mutex;
  const Config* m_local_config;

  BlockReference _ownReference;

  class ConfigManager* m_config_manager;

  bool m_need_restart;

  struct in_addr m_connect_address[MAX_NODES];
  const char *get_connect_address(NodeId node_id,
                                  char *addr_buf,
                                  size_t addr_buf_size);
  void clear_connect_address_cache(NodeId nodeid);

  /**
   * trp_client interface
   */
  virtual void trp_deliver_signal(const NdbApiSignal* signal,
                                  const struct LinearSectionPtr ptr[3]);
  virtual void trp_node_status(Uint32 nodeId, Uint32 event);
  
  /**
   * An event from <i>nodeId</i> has arrived
   */
  void eventReport(const Uint32 * theData, Uint32 len);
 
  class TransporterFacade * theFacade;

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

  ndb_mgm_node_type getNodeType(NodeId) const;

  /**
   * Handles the thread wich upon a 'Node is started' event will
   * set the node's previous loglevel settings.
   */
  struct NdbThread* _logLevelThread;
  static void *logLevelThread_C(void *);
  void logLevelThreadRun();
  void report_unknown_signal(SimpleSignal *signal);

  void make_sync_req(SignalSender& ss, Uint32 nodeId);
public:
  /* Get copy of configuration packed with base64 */
  bool get_packed_config(ndb_mgm_node_type nodetype,
                         BaseString& buf64, BaseString& error);

  /* Get copy of configuration packed with base64 from node nodeid */
  bool get_packed_config_from_node(NodeId nodeid,
                         BaseString& buf64, BaseString& error);

  void print_config(const char* section_filter = NULL,
                    NodeId nodeid_filter = 0,
                    const char* param_filter = NULL,
                    NdbOut& out = ndbout);

  bool reload_config(const char* config_filename,
                     bool mycnf, BaseString& msg);

  void show_variables(NdbOut& out = ndbout);

private:
  class NodeIdReservations {
    struct Reservation {
      NDB_TICKS m_start;
      unsigned m_timeout; // Milliseconds
    };
    Reservation m_reservations[MAX_NODES];
  public:
    NodeIdReservations();
    void check_array(NodeId n) const;
    bool get(NodeId n) const;
    void set(NodeId n, unsigned timeout);
    void clear(NodeId n);
    BaseString pretty_str() const;
    bool has_timedout(NodeId n, NDB_TICKS now) const;
  } m_reserved_nodes;
  NdbMutex* m_reserved_nodes_mutex;

  void release_local_nodeid_reservation(NodeId nodeid);
  struct PossibleNode
  {
    unsigned id;
    BaseString host;
    bool exact_match;
  };
  int alloc_node_id_req(NodeId free_node_id,
                        ndb_mgm_node_type type,
                        Uint32 timeout_ms);
  int try_alloc(NodeId id,
                ndb_mgm_node_type type,
                Uint32 timeout_ms);
  bool try_alloc_from_list(NodeId& nodeid,
                           ndb_mgm_node_type type,
                           Uint32 timeout_ms,
                           Vector<PossibleNode>& nodes_info);
  int find_node_type(NodeId nodeid,
                     ndb_mgm_node_type type,
                     const struct sockaddr* client_addr,
                     Vector<PossibleNode>& nodes_info,
                     int& error_code, BaseString& error_string);
  bool alloc_node_id_impl(NodeId& nodeid,
                          ndb_mgm_node_type type,
                          const struct sockaddr* client_addr,
                          int& error_code, BaseString& error_string,
                          Uint32 timeout_s = 20);
public:
  /*
    Nodeid allocation
    - MGM will never allocate a nodeid since either there is
      only one(and thus it's nodeid is well known by looking
      in config) or when running with two or more, they will
      have nodeid specified on command line when starting.
    - API nodes will always allocate a nodeid in the data nodes
      since those have transporter connections with hearbeat
      mechanism and can thus detect when the allocation should
      be cleared.
    - NDB nodes will prefer to allocate a nodeid in the data nodes
      since the allocations will then be known by all connected
      nodes(as well as hearbeat). But in order to startup a
      nodeid can also be reserved locally in the mgmtsrvr node
      only, such a reservations is cleared as soon as the
      data node has connected via transporter to the mgmtsrvr.
   */
  bool alloc_node_id(NodeId& nodeid,
                     ndb_mgm_node_type type,
		     const struct sockaddr* client_addr,
		     int& error_code, BaseString& error_string,
                     bool log_event = true,
		     Uint32 timeout_s = 20);

private:
  BaseString m_version_string;
public:
  const char* get_version_string(void) const {
    return m_version_string.c_str();
  }

  bool request_events(NdbNodeBitmask nodes, Uint32 reports_per_node,
                      Uint32 dump_type,
                      Vector<SimpleSignal>& events);
};

#endif // MgmtSrvr_H
