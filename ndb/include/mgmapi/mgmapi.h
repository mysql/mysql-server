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

#ifndef MGMAPI_H
#define MGMAPI_H

/**
 * @mainpage NDB Cluster Management API
 *
 * The NDB Cluster Management API (MGM API) is a C API 
 * that is used to:
 * - Start/stop database nodes (DB nodes)
 * - Start/stop NDB Cluster backups
 * - Control the NDB Cluster log
 * - Other administrative tasks
 *
 * @section  General Concepts
 *
 * Each MGM API function needs a management server handle 
 * (of type Mgm_C_Api::NdbMgmHandle).  
 * This handle is initally is created by calling the 
 * function ndb_mgm_create_handle().
 *
 * A function can return:
 *  -# An integer value.  
 *     If it returns -1 then this indicates an error, and then
 *  -# A pointer value.  If it returns NULL then check the latest error.
 *     If it didn't return NULL, then a "something" is returned.
 *     This "something" has to be free:ed by the user of the MGM API.
 *
 * If there are an error, then the get latest error functions
 * can be used to check what the error was.
 */

/** @addtogroup MGM_C_API
 *  @{
 */

#include "mgmapi_config_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * The NdbMgmHandle.
   */
  typedef struct ndb_mgm_handle * NdbMgmHandle;

  /**
   *   NDB Cluster node types
   */
  enum ndb_mgm_node_type {
    NDB_MGM_NODE_TYPE_UNKNOWN = -1,         /*/< Node type not known*/
    NDB_MGM_NODE_TYPE_API     = NODE_TYPE_API,          /*/< An application node (API)*/
    NDB_MGM_NODE_TYPE_NDB     = NODE_TYPE_DB,          /*/< A database node (DB)*/
    NDB_MGM_NODE_TYPE_MGM     = NODE_TYPE_MGM,          /*/< A management server node (MGM)*/
    NDB_MGM_NODE_TYPE_REP     = NODE_TYPE_REP,          ///< A replication node

    NDB_MGM_NODE_TYPE_MIN     = 0,          /*/< Min valid value*/
    NDB_MGM_NODE_TYPE_MAX     = 3           /*/< Max valid value*/
  };

  /**
   *   Database node status
   */
  enum ndb_mgm_node_status {
    NDB_MGM_NODE_STATUS_UNKNOWN       = 0,  ///< Node status not known
    NDB_MGM_NODE_STATUS_NO_CONTACT    = 1,  ///< No contact with node
    NDB_MGM_NODE_STATUS_NOT_STARTED   = 2,  ///< Has not run starting protocol
    NDB_MGM_NODE_STATUS_STARTING      = 3,  ///< Is running starting protocol
    NDB_MGM_NODE_STATUS_STARTED       = 4,  ///< Running
    NDB_MGM_NODE_STATUS_SHUTTING_DOWN = 5,  ///< Is shutting down
    NDB_MGM_NODE_STATUS_RESTARTING    = 6,  ///< Is restarting
    NDB_MGM_NODE_STATUS_SINGLEUSER    = 7,  ///< Maintenance mode
    NDB_MGM_NODE_STATUS_RESUME        = 8,  ///< Resume mode

    NDB_MGM_NODE_STATUS_MIN           = 0,  ///< Min valid value
    NDB_MGM_NODE_STATUS_MAX           = 6   ///< Max valid value
  };

  /**
   *    Error codes
   */
  enum ndb_mgm_error {
    NDB_MGM_NO_ERROR = 0,

    /* Request for service errors */
    NDB_MGM_ILLEGAL_CONNECT_STRING = 1001,
    NDB_MGM_ILLEGAL_PORT_NUMBER = 1002,
    NDB_MGM_ILLEGAL_SOCKET = 1003,
    NDB_MGM_ILLEGAL_IP_ADDRESS = 1004,
    NDB_MGM_ILLEGAL_SERVER_HANDLE = 1005,
    NDB_MGM_ILLEGAL_SERVER_REPLY = 1006,
    NDB_MGM_ILLEGAL_NUMBER_OF_NODES = 1007,
    NDB_MGM_ILLEGAL_NODE_STATUS = 1008,
    NDB_MGM_OUT_OF_MEMORY = 1009,
    NDB_MGM_SERVER_NOT_CONNECTED = 1010,
    NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET = 1011,

    /* Service errors - Start/Stop Node or System */
    NDB_MGM_START_FAILED = 2001,
    NDB_MGM_STOP_FAILED = 2002,
    NDB_MGM_RESTART_FAILED = 2003,

    /* Service errors - Backup */
    NDB_MGM_COULD_NOT_START_BACKUP = 3001,
    NDB_MGM_COULD_NOT_ABORT_BACKUP = 3002,

    /* Service errors - Single User Mode */
    NDB_MGM_COULD_NOT_ENTER_SINGLE_USER_MODE = 4001,
    NDB_MGM_COULD_NOT_EXIT_SINGLE_USER_MODE = 4002
  };

  struct Ndb_Mgm_Error_Msg {
    enum ndb_mgm_error  code;
    const char *        msg; 
  };

  const struct Ndb_Mgm_Error_Msg ndb_mgm_error_msgs[] = {
    { NDB_MGM_NO_ERROR, "No error" },

    { NDB_MGM_ILLEGAL_CONNECT_STRING, "Illegal connect string" },
    { NDB_MGM_ILLEGAL_PORT_NUMBER, "Illegal port number" },
    { NDB_MGM_ILLEGAL_SOCKET, "Illegal socket" },
    { NDB_MGM_ILLEGAL_IP_ADDRESS, "Illegal IP address" },
    { NDB_MGM_ILLEGAL_SERVER_HANDLE, "Illegal server handle" },
    { NDB_MGM_ILLEGAL_SERVER_REPLY, "Illegal reply from server" },
    { NDB_MGM_ILLEGAL_NUMBER_OF_NODES, "Illegal number of nodes" },
    { NDB_MGM_ILLEGAL_NODE_STATUS, "Illegal node status" },
    { NDB_MGM_OUT_OF_MEMORY, "Out of memory" },
    { NDB_MGM_SERVER_NOT_CONNECTED, "Management server not connected" },
    { NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET, "Could not connect to socket" },

    /* Service errors - Start/Stop Node or System */
    { NDB_MGM_START_FAILED, "Start failed" },
    { NDB_MGM_STOP_FAILED, "Stop failed" },
    { NDB_MGM_RESTART_FAILED, "Restart failed" },

    /* Service errors - Backup */
    { NDB_MGM_COULD_NOT_START_BACKUP, "Could not start backup" },
    { NDB_MGM_COULD_NOT_ABORT_BACKUP, "Could not abort backup" },
    
    /* Service errors - Single User Mode */
    { NDB_MGM_COULD_NOT_ENTER_SINGLE_USER_MODE, 
      "Could not enter single user mode" },
    { NDB_MGM_COULD_NOT_EXIT_SINGLE_USER_MODE, 
      "Could not exit single user mode" }
  };
  
  const int ndb_mgm_noOfErrorMsgs = 
  sizeof(ndb_mgm_error_msgs)/sizeof(struct Ndb_Mgm_Error_Msg);

  /**
   *   Structure returned by ndb_mgm_get_status
   */
  struct ndb_mgm_node_state {
    int node_id;                            ///< NDB Cluster node id
    enum ndb_mgm_node_type   node_type;     ///< Type of NDB Cluster node
    enum ndb_mgm_node_status node_status;   ///< State of node
    int start_phase;                        ///< Start phase. 
                                            ///< @note Start phase is only 
                                            ///< valid if 
                                            ///< node_type is 
                                            ///< NDB_MGM_NODE_TYPE_NDB and
                                            ///< node_status is 
                                            ///< NDB_MGM_NODE_STATUS_STARTING
    int dynamic_id;                         ///< Id for heartbeats and
                                            ///< master take-over
                                            ///< (only valid for DB nodes)
    int node_group;                         ///< Node group of node
                                            ///< (only valid for DB nodes)
    int version;                            ///< Internal version number
    int connect_count;                      ///< No of times node has connected
                                            ///< or disconnected to the mgm srv
    char connect_address[sizeof("000.000.000.000")+1];
  };

  /**
   *   Cluster status
   */
  struct ndb_mgm_cluster_state {
    int no_of_nodes;                        ///< No of entries in the 
                                            ///< node_states array
    struct ndb_mgm_node_state               ///< An array with node_states
    node_states[1];
    const char *hostname;
  };

  /**
   *   Default reply from the server
   */
  struct ndb_mgm_reply {
    int return_code;                        ///< 0 if successful, 
                                            ///< otherwise error code.
    char message[256];                      ///< Error or reply message.
  };

  /**
   *   Default information types
   */
  enum ndb_mgm_info {
    NDB_MGM_INFO_CLUSTER,                   ///< ?
    NDB_MGM_INFO_CLUSTERLOG                 ///< Cluster log
  };

  /**
   *   Signal log modes
   *   (Used only in the development of NDB Cluster.)
   */
  enum ndb_mgm_signal_log_mode {
    NDB_MGM_SIGNAL_LOG_MODE_IN,             ///< Log receiving signals 
    NDB_MGM_SIGNAL_LOG_MODE_OUT,            ///< Log sending signals
    NDB_MGM_SIGNAL_LOG_MODE_INOUT,          ///< Log both sending/receiving
    NDB_MGM_SIGNAL_LOG_MODE_OFF             ///< Log off
  };

  /**
   *   Log severities (used to filter the cluster log)
   */
  enum ndb_mgm_clusterlog_level {
    NDB_MGM_CLUSTERLOG_OFF = 0,             ///< Cluster log off
    NDB_MGM_CLUSTERLOG_DEBUG = 1,           ///< Used in NDB Cluster 
                                            ///< developement
    NDB_MGM_CLUSTERLOG_INFO = 2,            ///< Informational messages
    NDB_MGM_CLUSTERLOG_WARNING = 3,         ///< Conditions that are not
                                            ///< error condition, but 
                                            ///< might require handling
    NDB_MGM_CLUSTERLOG_ERROR = 4,           ///< Conditions that should be
                                            ///< corrected
    NDB_MGM_CLUSTERLOG_CRITICAL = 5,        ///< Critical conditions, like
                                            ///< device errors or out of 
                                            ///< resources
    NDB_MGM_CLUSTERLOG_ALERT = 6,           ///< A condition that should be
                                            ///< corrected immediately,
                                            ///< such as a corrupted system
    NDB_MGM_CLUSTERLOG_ALL = 7              ///< All severities on
  };

  /**
   *   Log categories
   */
  enum ndb_mgm_event_category {
    NDB_MGM_ILLEGAL_EVENT_CATEGORY = -1,     ///< Invalid
    /**
     * Events during all kinds of startups
     */
    NDB_MGM_EVENT_CATEGORY_STARTUP = CFG_LOGLEVEL_STARTUP,
    
    /**
     * Events during shutdown
     */
    NDB_MGM_EVENT_CATEGORY_SHUTDOWN = CFG_LOGLEVEL_SHUTDOWN,

    /**
     * Transaction statistics (Job level, TCP/IP speed)
     */
    NDB_MGM_EVENT_CATEGORY_STATISTIC = CFG_LOGLEVEL_STATISTICS,
    NDB_MGM_EVENT_CATEGORY_CHECKPOINT = CFG_LOGLEVEL_CHECKPOINT,
    NDB_MGM_EVENT_CATEGORY_NODE_RESTART = CFG_LOGLEVEL_NODERESTART,
    NDB_MGM_EVENT_CATEGORY_CONNECTION = CFG_LOGLEVEL_CONNECTION,
    NDB_MGM_EVENT_CATEGORY_DEBUG = CFG_LOGLEVEL_DEBUG,
    NDB_MGM_EVENT_CATEGORY_INFO = CFG_LOGLEVEL_INFO,
    NDB_MGM_EVENT_CATEGORY_WARNING = CFG_LOGLEVEL_WARNING,
    NDB_MGM_EVENT_CATEGORY_ERROR = CFG_LOGLEVEL_ERROR,
    NDB_MGM_EVENT_CATEGORY_GREP = CFG_LOGLEVEL_GREP,
    
    NDB_MGM_MIN_EVENT_CATEGORY = CFG_MIN_LOGLEVEL,
    NDB_MGM_MAX_EVENT_CATEGORY = CFG_MAX_LOGLEVEL
  };
  
  /***************************************************************************/
  /** 
   * @name Functions: Error Handling
   * @{
   */

  /**
   * Get latest error associated with a management server handle
   *
   * @param   handle        Management handle
   * @return                Latest error code
   */
  int ndb_mgm_get_latest_error(const NdbMgmHandle handle);

  /**
   * Get latest main error message associated with a handle
   *
   * @param   handle        Management handle.
   * @return                Latest error message
   */
  const char * ndb_mgm_get_latest_error_msg(const NdbMgmHandle handle);

  /**
   * Get latest error description associated with a handle
   *
   * The error description gives some additional information to 
   * the error message.
   *
   * @param   handle        Management handle.
   * @return                Latest error description
   */
  const char * ndb_mgm_get_latest_error_desc(const NdbMgmHandle handle);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Get latest internal source code error line associated with a handle
   *
   * @param   handle        Management handle.
   * @return                Latest internal source code line of latest error
   * @deprecated 
   */
  int ndb_mgm_get_latest_error_line(const NdbMgmHandle handle);
#endif

  /** @} *********************************************************************/
  /** 
   * @name Functions: Create/Destroy Management Server Handles
   * @{
   */

  /** 
   * Create a handle to a management server
   *
   * @return                A management handle<br>
   *                        or NULL if no management handle could be created. 
   */
  NdbMgmHandle ndb_mgm_create_handle();
  
  /**
   * Destroy a management server handle
   *
   * @param   handle        Management handle
   */
  void ndb_mgm_destroy_handle(NdbMgmHandle * handle);
  
  /** @} *********************************************************************/
  /** 
   * @name Functions: Connect/Disconnect Management Server
   * @{
   */

  /**
   * Connect to a management server
   *
   * @param   handle        Management handle.
   * @param   mgmsrv        Hostname and port of the management server, 
   *                        "hostname:port".
   * @return                -1 on error.
   */
  int ndb_mgm_connect(NdbMgmHandle handle, const char * mgmsrv);
  
  /**
   * Disconnect from a management server
   *
   * @param  handle         Management handle.
   * @return                -1 on error.
   */
  int ndb_mgm_disconnect(NdbMgmHandle handle);
  
  /** @} *********************************************************************/
  /** 
   * @name Functions: Convert between different data formats
   * @{
   */

  /**
   * Convert a string to a ndb_mgm_node_type
   *
   * @param   type          Node type as string.
   * @return                NDB_MGM_NODE_TYPE_UNKNOWN if invalid string.
   */
  enum ndb_mgm_node_type ndb_mgm_match_node_type(const char * type);

  /**
   * Convert an ndb_mgm_node_type to a string
   *
   * @param   type          Node type.
   * @return                NULL if invalid id.
   */
  const char * ndb_mgm_get_node_type_string(enum ndb_mgm_node_type type);

  /**
   * Convert an ndb_mgm_node_type to a alias string
   *
   * @param   type          Node type.
   * @return                NULL if invalid id.
   */
  const char * ndb_mgm_get_node_type_alias_string(enum ndb_mgm_node_type type, const char **str);

  /**
   * Convert a string to a ndb_mgm_node_status
   *
   * @param   status        NDB node status string.
   * @return                NDB_MGM_NODE_STATUS_UNKNOWN if invalid string.
   */
  enum ndb_mgm_node_status ndb_mgm_match_node_status(const char * status);

  /**
   * Convert an id to a string
   *
   * @param   status        NDB node status.
   * @return                NULL if invalid id.
   */
  const char * ndb_mgm_get_node_status_string(enum ndb_mgm_node_status status);

  ndb_mgm_event_category ndb_mgm_match_event_category(const char *);
  const char * ndb_mgm_get_event_category_string(enum ndb_mgm_event_category);

  /** @} *********************************************************************/
  /** 
   * @name Functions: State of cluster
   * @{
   */

  /**
   * Get status of the nodes in an NDB Cluster
   *
   * Note the caller must free the pointer returned.
   *
   * @param   handle        Management handle.
   * @return                Cluster state (or NULL on error).
   */
  struct ndb_mgm_cluster_state * ndb_mgm_get_status(NdbMgmHandle handle);

  /** @} *********************************************************************/
  /** 
   * @name Functions: Start/stop nodes 
   * @{
   */

  /**
   * Stop database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   no of database nodes<br>
   *                        0 - means all database nodes in cluster<br>
   *                        n - Means stop n node(s) specified in the 
   *                            array node_list
   * @param   node_list     List of node ids of database nodes to be stopped
   * @return                No of nodes stopped (or -1 on error)
   *
   * @note    The function is equivalent 
   *          to ndb_mgm_stop2(handle, no_of_nodes, node_list, 0)
   */
  int ndb_mgm_stop(NdbMgmHandle handle, int no_of_nodes, 
		   const int * node_list);

  /**
   * Stop database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   No of database nodes<br>
   *                        0 - means all database nodes in cluster<br>
   *                        n - Means stop n node(s) specified in 
   *                            the array node_list
   * @param   node_list     List of node ids of database nodes to be stopped
   * @param   abort         Don't perform gracefull stop, 
   *                        but rather stop immediatly
   * @return                No of nodes stopped (or -1 on error).
   */
  int ndb_mgm_stop2(NdbMgmHandle handle, int no_of_nodes,
		    const int * node_list, int abort);

  /**
   * Restart database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   No of database nodes<br>
   *                        0 - means all database nodes in cluster<br>
   *                        n - Means stop n node(s) specified in the 
   *                            array node_list
   * @param   node_list     List of node ids of database nodes to be stopped
   * @return                No of nodes stopped (or -1 on error).
   *
   * @note    The function is equivalent to 
   *          ndb_mgm_restart2(handle, no_of_nodes, node_list, 0, 0, 0);
   */
  int ndb_mgm_restart(NdbMgmHandle handle, int no_of_nodes, 
		      const int * node_list);

  /**
   * Restart database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   No of database nodes<br>
   *                        0 - means all database nodes in cluster<br>
   *                        n - Means stop n node(s) specified in the 
   *                            array node_list
   * @param   node_list     List of node ids of database nodes to be stopped
   * @param   initial       Remove filesystem from node(s) restarting
   * @param   nostart       Don't actually start node(s) but leave them 
   *                        waiting for start command
   * @param   abort         Don't perform gracefull restart, 
   *                        but rather restart immediatly
   * @return                No of nodes stopped (or -1 on error).
   */
  int ndb_mgm_restart2(NdbMgmHandle handle, int no_of_nodes,
		       const int * node_list, int initial,
		       int nostart, int abort);
       
  /**
   * Start database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   No of database nodes<br>
   *                        0 - means all database nodes in cluster<br>
   *                        n - Means start n node(s) specified in 
   *                            the array node_list
   * @param   node_list     List of node ids of database nodes to be started
   * @return                No of nodes started (or -1 on error).
   *
   * @note    The nodes to start must have been started with nostart(-n) 
   *          argument.
   *          This means that the database node binary is started and 
   *          waiting for a START management command which will 
   *          actually start the database node functionality
   */
  int ndb_mgm_start(NdbMgmHandle handle,
		    int no_of_nodes,
		    const int * node_list);

  /** @} *********************************************************************/
  /** 
   * @name Functions: Logging and Statistics
   * @{
   */

  /**
   * Filter cluster log
   *
   * @param   handle        NDB management handle.
   * @param   level         A cluster log level to filter.
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_filter_clusterlog(NdbMgmHandle handle,
				enum ndb_mgm_clusterlog_level level,
				struct ndb_mgm_reply* reply);

  /**
   * Get log filter
   * 
   * @param   handle        NDB management handle
   * @return                A vector of seven elements, 
   *                        where each element contains
   *                        1 if a severity is enabled and 0 if not. 
   *                        A severity is stored at position 
   *                        ndb_mgm_clusterlog_level, 
   *                        for example the "error" level is stored in position
   *                        [NDB_MGM_CLUSTERLOG_ERROR-1].
   *                        The first element in the vector signals 
   *                        whether the clusterlog
   *                        is disabled or enabled.
   */
  unsigned int *ndb_mgm_get_logfilter(NdbMgmHandle handle);

  /**
   * Set log category and levels for the cluster log
   *
   * @param   handle        NDB management handle.
   * @param   nodeId        Node id.
   * @param   category      Event category.
   * @param   level         Log level (0-15).
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_set_loglevel_clusterlog(NdbMgmHandle handle,
				      int nodeId,
				      enum ndb_mgm_event_category category,
				      int level,
				      struct ndb_mgm_reply* reply);

  /**
   * Set log category and levels for the Node
   *
   * @param   handle        NDB management handle.
   * @param   nodeId        Node id.
   * @param   category      Event category.
   * @param   level         Log level (0-15).
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_set_loglevel_node(NdbMgmHandle handle,
				int nodeId,
				enum ndb_mgm_event_category category,
				int level,
				struct ndb_mgm_reply* reply);

  /**
   * Returns the port number where statistics information is sent
   *
   * @param   handle        NDB management handle.
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_get_stat_port(NdbMgmHandle handle,
			    struct ndb_mgm_reply* reply);

  /** @} *********************************************************************/
  /** 
   * @name Functions: Backup
   * @{
   */

  /**
   * Start backup
   *
   * @param   handle        NDB management handle.
   * @param   backup_id     Backup id is returned from function.
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_start_backup(NdbMgmHandle handle, unsigned int* backup_id,
			   struct ndb_mgm_reply* reply);

  /**
   * Abort backup
   *
   * @param   handle        NDB management handle.
   * @param   backup_id     Backup Id.
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_abort_backup(NdbMgmHandle handle, unsigned int backup_id,
			   struct ndb_mgm_reply* reply);


  /** @} *********************************************************************/
  /** 
   * @name Functions: Single User Mode
   * @{
   */

  /**
   * Enter Single user mode 
   *
   * @param   handle        NDB management handle.
   * @param   nodeId        Node Id of the single user node
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_enter_single_user(NdbMgmHandle handle, unsigned int nodeId,
				struct ndb_mgm_reply* reply);
  
  /**
   * Exit Single user mode 
   *
   * @param   handle        NDB management handle.
   * @param   nodeId        Node Id of the single user node
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_exit_single_user(NdbMgmHandle handle, 
			       struct ndb_mgm_reply* reply);
  
  /**
   * Get configuration
   * @param   handle     NDB management handle.
   * @param   version    Version of configuration, 0 means latest
   *                     @see MAKE_VERSION
   * @Note the caller must call ndb_mgm_detroy_configuration
   */
  struct ndb_mgm_configuration * ndb_mgm_get_configuration(NdbMgmHandle handle,
							   unsigned version);
  void ndb_mgm_destroy_configuration(struct ndb_mgm_configuration *);

  int ndb_mgm_alloc_nodeid(NdbMgmHandle handle,
			   unsigned version,
			   unsigned *pnodeid,
			   int nodetype);
  /**
   * Config iterator
   */
  typedef struct ndb_mgm_configuration_iterator ndb_mgm_configuration_iterator;

  ndb_mgm_configuration_iterator* ndb_mgm_create_configuration_iterator
  (struct ndb_mgm_configuration *, unsigned type_of_section);
  void ndb_mgm_destroy_iterator(ndb_mgm_configuration_iterator*);
  
  int ndb_mgm_first(ndb_mgm_configuration_iterator*);
  int ndb_mgm_next(ndb_mgm_configuration_iterator*);
  int ndb_mgm_valid(const ndb_mgm_configuration_iterator*);
  int ndb_mgm_find(ndb_mgm_configuration_iterator*, 
		   int param, unsigned value);
  
  int ndb_mgm_get_int_parameter(const ndb_mgm_configuration_iterator*, 
				int param, unsigned * value);
  int ndb_mgm_get_int64_parameter(const ndb_mgm_configuration_iterator*,
				  int param, unsigned long long * value);
  int ndb_mgm_get_string_parameter(const ndb_mgm_configuration_iterator*,
				   int param, const char  ** value);
#ifdef __cplusplus
}
#endif

/** @} */

#endif
