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
 * - Start and stop database nodes (ndbd processes)
 * - Start and stop NDB Cluster backups
 * - Control the NDB Cluster log
 * - Perform other administrative tasks
 *
 * @section  secMgmApiGeneral General Concepts
 *
 * Each MGM API function needs a management server handle
 * of type @ref NdbMgmHandle.
 * This handle is initally created by calling the
 * function ndb_mgm_create_handle() and freed by calling 
 * ndb_mgm_destroy_handle().
 *
 * A function can return:
 *  -# An integer value.
 *     A value of <b>-1</b> indicates an error.
 *  -# A non-const pointer value.  A <var>NULL</var> value indicates an error;
 *     otherwise, the return value must be freed
 *     by the user of the MGM API
 *  -# A const pointer value.  A <var>NULL</var> value indicates an error.
 *     Returned value should not be freed.
 *
 * Error conditions can be identified by using the appropriate
 * error-reporting functions ndb_mgm_get_latest_error() and 
 * @ref ndb_mgm_error.
 *
 * Below is an example of usage (without error handling for brevity).
 * @code
 *   NdbMgmHandle handle= ndb_mgm_create_handle();
 *   ndb_mgm_connect(handle,0,0,0);
 *   struct ndb_mgm_cluster_state *state= ndb_mgm_get_status(handle);
 *   for(int i=0; i < state->no_of_nodes; i++) 
 *   {
 *     printf("node with id=%d ", state->node_states[i].node_id);
 *     if(state->node_states[i].version != 0)
 *       printf("connected\n");
 *     else
 *       printf("not connected\n");
 *   }
 *   free((void*)state);
 *   ndb_mgm_destroy_handle(&handle);
 * @endcode
 *
 * @section secLogEvents  Log Events
 *
 * The database nodes and management server(s) regularly and on specific
 * occations report on various log events that occurs in the cluster. These
 * log events are written to the cluster log.  Optionally a mgmapi client
 * may listen to these events by using the method ndb_mgm_listen_event().
 * Each log event belongs to a category, @ref ndb_mgm_event_category, and
 * has a severity, @ref ndb_mgm_event_severity, associated with it.  Each
 * log event also has a level (0-15) associated with it.
 *
 * Which log events that come out is controlled with ndb_mgm_listen_event(),
 * ndb_mgm_set_clusterlog_loglevel(), and 
 * ndb_mgm_set_clusterlog_severity_filter().
 *
 * Below is an example of how to listen to events related to backup.
 *
 * @code
 *   int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP, 0 };
 *   int fd = ndb_mgm_listen_event(handle, filter);
 * @endcode
 */

/** @addtogroup MGM_C_API
 *  @{
 */

#include <ndb_types.h>
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
    NDB_MGM_NODE_TYPE_UNKNOWN = -1  /** Node type not known*/
    ,NDB_MGM_NODE_TYPE_API    /** An application node (API) */
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = NODE_TYPE_API
#endif
    ,NDB_MGM_NODE_TYPE_NDB    /** A database node (DB) */
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = NODE_TYPE_DB
#endif
    ,NDB_MGM_NODE_TYPE_MGM    /** A mgmt server node (MGM)*/
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = NODE_TYPE_MGM
#endif
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    ,NDB_MGM_NODE_TYPE_REP = NODE_TYPE_REP  /** A replication node */
    ,NDB_MGM_NODE_TYPE_MIN     = 0          /** Min valid value*/
    ,NDB_MGM_NODE_TYPE_MAX     = 3          /** Max valid value*/
#endif
  };

  /**
   *   Database node status
   */
  enum ndb_mgm_node_status {
    /** Node status not known*/
    NDB_MGM_NODE_STATUS_UNKNOWN       = 0,
    /** No contact with node*/
    NDB_MGM_NODE_STATUS_NO_CONTACT    = 1,
    /** Has not run starting protocol*/
    NDB_MGM_NODE_STATUS_NOT_STARTED   = 2,
    /** Is running starting protocol*/
    NDB_MGM_NODE_STATUS_STARTING      = 3,
    /** Running*/
    NDB_MGM_NODE_STATUS_STARTED       = 4,
    /** Is shutting down*/
    NDB_MGM_NODE_STATUS_SHUTTING_DOWN = 5,
    /** Is restarting*/
    NDB_MGM_NODE_STATUS_RESTARTING    = 6,
    /** Maintenance mode*/
    NDB_MGM_NODE_STATUS_SINGLEUSER    = 7,
    /** Resume mode*/
    NDB_MGM_NODE_STATUS_RESUME        = 8,
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /** Min valid value*/
    NDB_MGM_NODE_STATUS_MIN           = 0,
    /** Max valid value*/
    NDB_MGM_NODE_STATUS_MAX           = 8
#endif
  };

  /**
   *    Error codes
   */
  enum ndb_mgm_error {
    /** Not an error */
    NDB_MGM_NO_ERROR = 0,

    /* Request for service errors */
    /** Supplied connectstring is illegal */
    NDB_MGM_ILLEGAL_CONNECT_STRING = 1001,
    /** Supplied NdbMgmHandle is illegal */
    NDB_MGM_ILLEGAL_SERVER_HANDLE = 1005,
    /** Illegal reply from server */
    NDB_MGM_ILLEGAL_SERVER_REPLY = 1006,
    /** Illegal number of nodes */
    NDB_MGM_ILLEGAL_NUMBER_OF_NODES = 1007,
    /** Illegal node status */
    NDB_MGM_ILLEGAL_NODE_STATUS = 1008,
    /** Memory allocation error */
    NDB_MGM_OUT_OF_MEMORY = 1009,
    /** Management server not connected */
    NDB_MGM_SERVER_NOT_CONNECTED = 1010,
    /** Could not connect to socker */
    NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET = 1011,

    /* Service errors - Start/Stop Node or System */
    /** Start failed */
    NDB_MGM_START_FAILED = 2001,
    /** Stop failed */
    NDB_MGM_STOP_FAILED = 2002,
    /** Restart failed */
    NDB_MGM_RESTART_FAILED = 2003,

    /* Service errors - Backup */
    /** Unable to start backup */
    NDB_MGM_COULD_NOT_START_BACKUP = 3001,
    /** Unable to abort backup */
    NDB_MGM_COULD_NOT_ABORT_BACKUP = 3002,

    /* Service errors - Single User Mode */
    /** Unable to enter single user mode */
    NDB_MGM_COULD_NOT_ENTER_SINGLE_USER_MODE = 4001,
    /** Unable to exit single user mode */
    NDB_MGM_COULD_NOT_EXIT_SINGLE_USER_MODE = 4002,

    /* Usage errors */
    /** Usage error */
    NDB_MGM_USAGE_ERROR = 5001
  };

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  struct Ndb_Mgm_Error_Msg {
    enum ndb_mgm_error  code;
    const char *        msg;
  };
  const struct Ndb_Mgm_Error_Msg ndb_mgm_error_msgs[] = {
    { NDB_MGM_NO_ERROR, "No error" },

    /* Request for service errors */
    { NDB_MGM_ILLEGAL_CONNECT_STRING, "Illegal connect string" },
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
      "Could not exit single user mode" },

    /* Usage errors */
    { NDB_MGM_USAGE_ERROR,
      "Usage error" }
  };
  const int ndb_mgm_noOfErrorMsgs =
  sizeof(ndb_mgm_error_msgs)/sizeof(struct Ndb_Mgm_Error_Msg);
#endif

  /**
   *   Status of a node in the cluster
   *
   *   Sub-structure in enum ndb_mgm_cluster_state
   *   returned by ndb_mgm_get_status()
   *
   *   @note @ref node_status, @ref start_phase, @ref dynamic_id 
   *         and @ref node_group are only relevant for database nodes
   */
  struct ndb_mgm_node_state {
    /** NDB Cluster node id*/
    int node_id;
    /** Type of NDB Cluster node*/
    enum ndb_mgm_node_type   node_type;
   /** State of node*/
    enum ndb_mgm_node_status node_status;
    /** Start phase.
     *
     *  @note Start phase is only valid if node_type is
     *        NDB_MGM_NODE_TYPE_NDB and node_status is 
     *        NDB_MGM_NODE_STATUS_STARTING
     */
    int start_phase;
    /** Id for heartbeats and master take-over (only valid for DB nodes)
     */
    int dynamic_id;
    /** Node group of node (only valid for DB nodes)*/
    int node_group;
    /** Internal version number*/
    int version;
    /** Number of times node has connected or disconnected to the 
     *  management server
     */
    int connect_count;
    /** Ip adress of node when it connected to the management server.
     *  @note it will be empty if the management server has restarted
     *        after the node connected.
     */
    char connect_address[
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
			 sizeof("000.000.000.000")+1
#endif
    ];
  };

  /**
   *   State of all nodes in the cluster returned from 
   *   ndb_mgm_get_status()
   */
  struct ndb_mgm_cluster_state {
    /** No of entries in the node_states array */
    int no_of_nodes;
    /** An array with node_states*/
    struct ndb_mgm_node_state node_states[
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
					  1
#endif
    ];
  };

  /**
   *   Default reply from the server (for future use, not used today)
   */
  struct ndb_mgm_reply {
    /** 0 if successful, otherwise error code. */
    int return_code;
    /** Error or reply message.*/
    char message[256];
  };

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   *   Default information types
   */
  enum ndb_mgm_info {
    /** ?*/
    NDB_MGM_INFO_CLUSTER,
    /** Cluster log*/
    NDB_MGM_INFO_CLUSTERLOG
  };

  /**
   *   Signal log modes
   *   (Used only in the development of NDB Cluster.)
   */
  enum ndb_mgm_signal_log_mode {
    /** Log receiving signals */
    NDB_MGM_SIGNAL_LOG_MODE_IN,
    /** Log sending signals*/
    NDB_MGM_SIGNAL_LOG_MODE_OUT,
    /** Log both sending/receiving*/
    NDB_MGM_SIGNAL_LOG_MODE_INOUT,
    /** Log off*/
    NDB_MGM_SIGNAL_LOG_MODE_OFF
  };
#endif

  /**
   *   Log event severities (used to filter the cluster log, 
   *   ndb_mgm_set_clusterlog_severity_filter(), and filter listening to events
   *   ndb_mgm_listen_event())
   */
  enum ndb_mgm_event_severity {
    NDB_MGM_ILLEGAL_EVENT_SEVERITY = -1,
    /* must range from 0 and up, indexes into an array */
    /** Cluster log on */
    NDB_MGM_EVENT_SEVERITY_ON    = 0,
    /** Used in NDB Cluster developement */
    NDB_MGM_EVENT_SEVERITY_DEBUG = 1,
    /** Informational messages*/
    NDB_MGM_EVENT_SEVERITY_INFO = 2,
    /** Conditions that are not error condition, but might require handling
     */
    NDB_MGM_EVENT_SEVERITY_WARNING = 3,
    /** Conditions that should be corrected */
    NDB_MGM_EVENT_SEVERITY_ERROR = 4,
    /** Critical conditions, like device errors or out of resources */
    NDB_MGM_EVENT_SEVERITY_CRITICAL = 5,
    /** A condition that should be corrected immediately,
     *  such as a corrupted system
     */
    NDB_MGM_EVENT_SEVERITY_ALERT = 6,
    /* must be next number, works as bound in loop */
    /** All severities */
    NDB_MGM_EVENT_SEVERITY_ALL = 7
  };

  /**
   *  Log event categories, used to set filter level on the log events using
   *  ndb_mgm_set_clusterlog_loglevel() and ndb_mgm_listen_event()
   */
  enum ndb_mgm_event_category {
    /**
     * Invalid log event category
     */
    NDB_MGM_ILLEGAL_EVENT_CATEGORY = -1,
    /**
     * Log events during all kinds of startups
     */
    NDB_MGM_EVENT_CATEGORY_STARTUP = CFG_LOGLEVEL_STARTUP,
    /**
     * Log events during shutdown
     */
    NDB_MGM_EVENT_CATEGORY_SHUTDOWN = CFG_LOGLEVEL_SHUTDOWN,
    /**
     * Statistics log events
     */
    NDB_MGM_EVENT_CATEGORY_STATISTIC = CFG_LOGLEVEL_STATISTICS,
    /**
     * Log events related to checkpoints
     */
    NDB_MGM_EVENT_CATEGORY_CHECKPOINT = CFG_LOGLEVEL_CHECKPOINT,
    /**
     * Log events during node restart
     */
    NDB_MGM_EVENT_CATEGORY_NODE_RESTART = CFG_LOGLEVEL_NODERESTART,
    /**
     * Log events related to connections between cluster nodes
     */
    NDB_MGM_EVENT_CATEGORY_CONNECTION = CFG_LOGLEVEL_CONNECTION,
    /**
     * Backup related log events
     */
    NDB_MGM_EVENT_CATEGORY_BACKUP = CFG_LOGLEVEL_BACKUP,
    /**
     * Congestion related log events
     */
    NDB_MGM_EVENT_CATEGORY_CONGESTION = CFG_LOGLEVEL_CONGESTION,
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /**
     * Loglevel debug
     */
    NDB_MGM_EVENT_CATEGORY_DEBUG = CFG_LOGLEVEL_DEBUG,
#endif
    /**
     * Uncategorized log events (severity info)
     */
    NDB_MGM_EVENT_CATEGORY_INFO = CFG_LOGLEVEL_INFO,
    /**
     * Uncategorized log events (severity warning or higher)
     */
    NDB_MGM_EVENT_CATEGORY_ERROR = CFG_LOGLEVEL_ERROR,
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    NDB_MGM_MIN_EVENT_CATEGORY = CFG_MIN_LOGLEVEL,
    NDB_MGM_MAX_EVENT_CATEGORY = CFG_MAX_LOGLEVEL
#endif
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
   * @return                 A management handle<br>
   *                         or NULL if no management handle could be created.
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
   * Set connect string to management server
   *
   * @param   handle         Management handle
   * @param   connect_string Connect string to the management server,
   *
   * @return                -1 on error.
   *
   * @code
   * <connectstring> := [<nodeid-specification>,]<host-specification>[,<host-specification>]
   * <nodeid-specification> := nodeid=<id>
   * <host-specification> := <host>[:<port>]
   * <id> is an integer larger than 1 identifying a node in config.ini
   * <port> is an integer referring to a regular unix port
   * <host> is a string which is a valid Internet host address
   * @endcode
   */
  int ndb_mgm_set_connectstring(NdbMgmHandle handle,
				const char *connect_string);

  /**
   * Get connectstring used for connection
   *
   * @note returns what the connectstring defaults to if the 
   *       ndb_mgm_set_connectstring() call has not been performed
   *
   * @param   handle         Management handle
   *
   * @return                 connectstring
   */
  const char *ndb_mgm_get_connectstring(NdbMgmHandle handle, char *buf, int buf_sz);

  /**
   * Connect to a management server. Coonect string is set by
   * ndb_mgm_set_connectstring().
   *
   * @param   handle        Management handle.
   * @return                -1 on error.
   */
  int ndb_mgm_connect(NdbMgmHandle handle, int no_retries,
		      int retry_delay_in_seconds, int verbose);

  /**
   * Disconnect from a management server
   *
   * @param  handle         Management handle.
   * @return                -1 on error.
   */
  int ndb_mgm_disconnect(NdbMgmHandle handle);

  /**
   * Get nodeid used in the connection
   *
   * @param   handle         Management handle
   *
   * @return                 node id, 0 indicated that no nodeid has been
   *                         specified
   */
  int ndb_mgm_get_configuration_nodeid(NdbMgmHandle handle);

  /**
   * Get port used in the connection
   *
   * @param   handle         Management handle
   *
   * @return                 port
   */
  int ndb_mgm_get_connected_port(NdbMgmHandle handle);

  /**
   * Get host used in the connection
   *
   * @param   handle         Management handle
   *
   * @return                 hostname
   */
  const char *ndb_mgm_get_connected_host(NdbMgmHandle handle);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
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
  const char * ndb_mgm_get_node_type_alias_string(enum ndb_mgm_node_type type,
						  const char **str);

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

  const char * ndb_mgm_get_event_severity_string(enum ndb_mgm_event_severity);
  ndb_mgm_event_category ndb_mgm_match_event_category(const char *);
  const char * ndb_mgm_get_event_category_string(enum ndb_mgm_event_category);
#endif

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
   *
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
   *
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
   *
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
   *
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
   *
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
   *
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
   * @name Functions: Controlling Clusterlog output
   * @{
   */

  /**
   * Filter cluster log severities
   *
   * @param   handle        NDB management handle.
   * @param   severity      A cluster log severity to filter.
   * @param   enable        set 1=enable, 0=disable
   * @param   reply         Reply message.
   *
   * @return                -1 on error.
   */
  int ndb_mgm_set_clusterlog_severity_filter(NdbMgmHandle handle,
					     enum ndb_mgm_event_severity severity,
					     int enable,
					     struct ndb_mgm_reply* reply);
  /**
   * Get clusterlog severity filter
   *
   * @param   handle        NDB management handle
   *
   * @return                A vector of seven elements,
   *                        where each element contains
   *                        1 if a severity is enabled and 0 if not.
   *                        A severity is stored at position
   *                        ndb_mgm_event_severity,
   *                        for example the "error" severity is stored in 
   *                        position [NDB_MGM_EVENT_SEVERITY_ERROR].
   *                        The first element [NDB_MGM_EVENT_SEVERITY_ON] 
   *                        in the vector signals
   *                        whether the clusterlog is disabled or enabled.
   */
  const unsigned int *ndb_mgm_get_clusterlog_severity_filter(NdbMgmHandle handle);

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
  int ndb_mgm_set_clusterlog_loglevel(NdbMgmHandle handle,
				      int nodeId,
				      enum ndb_mgm_event_category category,
				      int level,
				      struct ndb_mgm_reply* reply);

  /** @} *********************************************************************/
  /**
   * @name Functions: Listening to log events
   * @{
   */

  /**
   * Listen to log events. The are read from the return file descriptor
   * and the format is textual, and the same as in the cluster log.
   *
   * @param handle NDB management handle.
   * @param filter pairs of { level, ndb_mgm_event_category } that will be
   *               pushed to fd, level=0 ends list.
   *
   * @return fd which events will be pushed to
   */
  int ndb_mgm_listen_event(NdbMgmHandle handle, const int filter[]);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
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
#endif

  /** @} *********************************************************************/
  /**
   * @name Functions: Backup
   * @{
   */

  /**
   * Start backup
   *
   * @param   handle        NDB management handle.
   * @param   wait_completed 0=don't wait for confirmation,
   *                         1=wait for backup started,
   *                         2=wait for backup completed
   * @param   backup_id     Backup id is returned from function.
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int ndb_mgm_start_backup(NdbMgmHandle handle, int wait_completed,
			   unsigned int* backup_id,
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
   *
   * @return                -1 on error.
   */
  int ndb_mgm_exit_single_user(NdbMgmHandle handle,
			       struct ndb_mgm_reply* reply);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /** @} *********************************************************************/
  /**
   * @name Configuration handling
   * @{
   */

  /**
   * Get configuration
   * @param   handle     NDB management handle.
   * @param   version    Version of configuration, 0 means latest
   *                     (which is the only supported input at this point)
   *
   * @return configuration
   *
   * @note the caller must call ndb_mgm_destroy_configuration()
   */
  struct ndb_mgm_configuration * ndb_mgm_get_configuration(NdbMgmHandle handle,
							   unsigned version);
  void ndb_mgm_destroy_configuration(struct ndb_mgm_configuration *);

  int ndb_mgm_alloc_nodeid(NdbMgmHandle handle,
			   unsigned version, int nodetype);
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
				  int param, Uint64 * value);
  int ndb_mgm_get_string_parameter(const ndb_mgm_configuration_iterator*,
				   int param, const char  ** value);
  int ndb_mgm_purge_stale_sessions(NdbMgmHandle handle, char **);
  int ndb_mgm_check_connection(NdbMgmHandle handle);
#endif

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  enum ndb_mgm_clusterlog_level {
     NDB_MGM_ILLEGAL_CLUSTERLOG_LEVEL = -1,
     NDB_MGM_CLUSTERLOG_ON    = 0,
     NDB_MGM_CLUSTERLOG_DEBUG = 1,
     NDB_MGM_CLUSTERLOG_INFO = 2,
     NDB_MGM_CLUSTERLOG_WARNING = 3,
     NDB_MGM_CLUSTERLOG_ERROR = 4,
     NDB_MGM_CLUSTERLOG_CRITICAL = 5,
     NDB_MGM_CLUSTERLOG_ALERT = 6,
     NDB_MGM_CLUSTERLOG_ALL = 7
  };
  inline
  int ndb_mgm_filter_clusterlog(NdbMgmHandle h,
				enum ndb_mgm_clusterlog_level s,
				int e, struct ndb_mgm_reply* r)
  { return ndb_mgm_set_clusterlog_severity_filter(h,(ndb_mgm_event_severity)s,
						  e,r); }

  inline
  const unsigned int *ndb_mgm_get_logfilter(NdbMgmHandle h)
  { return ndb_mgm_get_clusterlog_severity_filter(h); }

  inline
  int ndb_mgm_set_loglevel_clusterlog(NdbMgmHandle h, int n,
				      enum ndb_mgm_event_category c,
				      int l, struct ndb_mgm_reply* r)
  { return ndb_mgm_set_clusterlog_loglevel(h,n,c,l,r); }
#endif

#ifdef __cplusplus
}
#endif

/** @} */

#endif
