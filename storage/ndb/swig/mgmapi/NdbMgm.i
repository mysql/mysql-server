/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

%{

  struct BackupWrapper {
    unsigned int backupId;
    ndb_mgm_reply* theReply;
  };

  %}


/**
 * The NdbMgmHandle.
 */
%rename ndb_mgm_handle NdbMgm;
%typemap(freearg) (ndb_mgm_reply *) "free($1);";

/*We don't really need this - we're going to free in the typemap
  %typemap(newfree) (BackupWrapper *) "free($1->theReply); free($1);";
  %newobject NdbMgm::startBackup;
*/

struct ndb_mgm_handle {
private:

  ndb_mgm_handle();
  ~ndb_mgm_handle();

};

typedef ndb_mgm_handle * NdbMgmHandle;



%extend ndb_mgm_handle {

public:
  /**
   * Destroy a management server handle.
   *
   * @param   handle        Management handle
   */
  ~ndb_mgm_handle() {
    ndb_mgm_destroy_handle(&$self);
  }


  /***************************************************************************/
  /**
   * @name Functions: Error Handling
   * @{
   */

  /**
   *  Get the most recent error
   *
   * @return                Latest error code
   */

  int getNdbMgmErrorCode() {
    return ndb_mgm_get_latest_error($self);
  }

  int getLatestErrorCode() {
    return ndb_mgm_get_latest_error($self);
  }

  /**
   * Get the most recent general error message
   *
   * @return                Latest error message
   */
  const char * getNdbMgmErrorMsg() {
    return ndb_mgm_get_latest_error_msg($self);
  }

  const char * getLatestErrorMsg()
  {
    return ndb_mgm_get_latest_error_msg($self);
  }


  /**
   * Get the most recent error description
   *
   * The error description gives some additional information regarding
   * the error message.
   *
   * @return                Latest error description
   */
  const char * getLatestErrorDesc()
  {
    return ndb_mgm_get_latest_error_desc($self);
  }

  /**
   * Set error stream
   *
   void setErrorStream(FILE * errStream)
   {
   ndb_mgm_set_error_stream($self, errStream);
   } */


  /**
   * Set a name of the handle.  Name is reported in cluster log.
   *
   * @param   name          Name
   */
  void setName(const char *name) {
    ndb_mgm_set_name($self, name);
  }

  %ndbexception("NdbMgmException") {
    $action
      if (result == -1) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }
  /** @} *********************************************************************/
  /**
   * @name Functions: Connect/Disconnect Management Server
   * @{
   */

  /**
   * Sets the connectstring for a management server
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
   * <id> is an integer greater than 0 identifying a node in config.ini
   * <port> is an integer referring to a regular unix port
   * <host> is a string containing a valid network host address
   * @endcode
   */
  int setConnectstring(const char *connect_string) {
    return ndb_mgm_set_connectstring($self,connect_string);
  }

  /**
   * Returns the number of management servers in the connect string
   * (as set by ndb_mgm_set_connectstring()). This can be used
   * to help work out how long the maximum amount of time that
   * ndb_mgm_connect can take.
   *
   * @param   handle         Management handle
   *
   * @return                < 0 on error
   */

  /**
   * Set local bindaddress
   * @param arg - Srting of form "host[:port]"
   * @note must be called before connect
   * @note Error on binding local address will not be reported until connect
   * @return 0 on success
   */
  int setBindaddress(const char * arg) {
    return ndb_mgm_set_bindaddress($self, arg);
  }

  /**
   * Gets the connectstring used for a connection
   *
   * @note This function returns the default connectstring if no call to
   *       ndb_mgm_set_connectstring() has been performed. Also, the
   *       returned connectstring may be formatted differently.
   *
   * @param   handle         Management handle
   * @param   buf            Buffer to hold result
   * @param   buf_sz         Size of buffer.
   *
   * @return                 connectstring (same as <var>buf</var>)
   */
// TODO: Do something sensible with this
//  const char *ndb_mgm_get_connectstring(NdbMgmHandle handle, char *buf, int buf_sz);

  /**
   *
   * @param handle  NdbMgmHandle
   * @param seconds number of seconds
   * @return non-zero on success
   */
  int setConnectTimeout(Uint32 seconds) {
    return ndb_mgm_set_connect_timeout($self, seconds);
  }

  /**
   * Connects to a management server. Connectstring is set by
   * ndb_mgm_set_connectstring().
   *
   * The timeout value is for connect to each management server.
   * Use ndb_mgm_number_of_mgmd_in_connect_string to work out
   * the approximate maximum amount of time that could be spent in this
   * function.
   *
   * @param   handle        Management handle.
   * @param   no_retries    Number of retries to connect
   *                        (0 means connect once).
   * @param   retry_delay_in_seconds
   *                        How long to wait until retry is performed.
   * @param   verbose       Make printout regarding connect retries.
   *
   * @return                -1 on error.
   */
  int connect( const char * connectString,int no_retries, int retry_delay_in_seconds, bool verbose) {
    if (ndb_mgm_set_connectstring($self,connectString)==-1)
      return -1;
    return ndb_mgm_connect($self, no_retries, retry_delay_in_seconds, verbose);
  }
  int connect( int no_retries, int retry_delay_in_seconds, bool verbose) {
    return ndb_mgm_connect($self, no_retries, retry_delay_in_seconds, verbose);
  }


  /**
   * Disconnects from a management server
   *
   * @param  handle         Management handle.
   * @return                -1 on error.
   */
  int disconnect() {
    return ndb_mgm_disconnect($self);
  }

  /**
   * Gets connection node ID
   *
   * @param   handle         Management handle
   *
   * @return                 Node ID; 0 indicates that no node ID has been
   *                         specified
   */
  int getConfigurationNodeid() {
    return ndb_mgm_get_configuration_nodeid($self);
  }

  /**
   * Gets connection port
   *
   * @param   handle         Management handle
   *
   * @return                 port
   */
  int getConnectedPort() {
    return ndb_mgm_get_connected_port($self);
  }


  %ndbnoexception;
  /**
   * Return true if connected.
   *
   * @param   handle        Management handle
   * @return  0 if not connected, non-zero if connected.
   */
  bool isConnected() {
    return ( ndb_mgm_is_connected($self) == 0 ) ? false : true ;
  }

  %ndbexception("NdbMgmException") {
    $action
      if (result == NULL) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  /**
   * Gets connection host
   *
   * @param   handle         Management handle
   *
   * @return                 hostname
   */
  const char * getConnectedHost() {
    return ndb_mgm_get_connected_host($self);
  }



  /** @} *********************************************************************/
  /**
   * @name Functions: Backup
   * @{
   */

  /**
   * Start backup
   *
   * @param   wait_completed  0:  Don't wait for confirmation<br>
   *                          1:  Wait for backup to be started<br>
   *                          2:  Wait for backup to be completed
   * @param   backup_id       Backup ID is returned from function.
   * @return                  Reply message. NULL on error.
   * @note                    backup_id will not be returned if
   *                          wait_completed == 0
   */

  %ndbexception("NdbMgmException") {
    $action
      if (result->theReply == NULL) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  BackupWrapper * startBackup(BackupStartOption wait_completed) {

    ndb_mgm_reply * reply = (ndb_mgm_reply *)malloc(sizeof(ndb_mgm_reply));
    unsigned int backup_id=0;
    if (reply == NULL) {
      return NULL;
    }
    int ret = ndb_mgm_start_backup($self, (int)wait_completed,
                                   &backup_id, reply);
    if (ret == -1) {
      free(reply);
      return NULL;
    }
    BackupWrapper * reply_wrapper = (BackupWrapper *)malloc(sizeof(BackupWrapper));
    if (reply_wrapper == NULL) {
      free(reply);
      return NULL;
    }
    reply_wrapper->theReply=reply;
    reply_wrapper->backupId=backup_id;
    return reply_wrapper;
  }

  /**
   * Abort backup
   *
   * @param   backup_id     Backup ID.
   * @return                Reply message. NULL on error.
   */
  %ndbexception("NdbMgmException") {
    $action
      if (result == NULL) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  %newobject abortBackup;
  ndb_mgm_reply* abortBackup(unsigned int backup_id) {
    ndb_mgm_reply * reply = (ndb_mgm_reply *)malloc(sizeof(ndb_mgm_reply));
    if (reply == NULL) {
      return NULL;
    }
    int ret = ndb_mgm_abort_backup($self, backup_id, reply);
    if (ret == -1) {
      free(reply);
      return NULL;
    }
    return reply;
  }

  /** @} *********************************************************************/
  /**
   * @name Functions: Cluster status
   * @{
   */

  /**
   * Gets status of the nodes in an NDB Cluster
   *
   * @note The caller must free the pointer returned by this function.
   *
   *
   * @return                Cluster state (or <var>NULL</var> on error).
   */
  %newobject getStatus;
  ndb_mgm_cluster_state * getStatus() {
    return ndb_mgm_get_status($self);
  }


  %ndbexception("NdbMgmException") {
    $action
      if (result != NULL && result->return_code < 0) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        NDB_exception(NdbMgmException,result->message);
      } else if (result == NULL) {
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  /** @} *********************************************************************/
  /**
   * @name Functions: Single User Mode
   * @{
   */

  /**
   * Enter Single user mode
   *
   * @param   handle        NDB management handle.
   * @param   nodeId        Node ID of the single user node
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  %newobject enterSingleUserMode;
  ndb_mgm_reply * enterSingleUserMode(unsigned int nodeId)
  {
    ndb_mgm_reply * reply = (ndb_mgm_reply *)malloc(sizeof(ndb_mgm_reply));
    if (reply == NULL) {
      return NULL;
    }
    memset(reply,0,sizeof(ndb_mgm_reply));
    int ret = ndb_mgm_enter_single_user($self, nodeId, reply);

    if (ret == -1) {
      free(reply);
      return NULL;
    }
    return reply;
  }

  /**
   * Exit Single user mode
   *
   * @param   handle        NDB management handle.
   * @param   reply         Reply message.
   *
   * @return                -1 on error.
   */
  %newobject exitSingleUserMode;
  ndb_mgm_reply * exitSingleUserMode()
  {
    ndb_mgm_reply * reply = (ndb_mgm_reply *)malloc(sizeof(ndb_mgm_reply));
    if (reply == NULL) {
      return NULL;
    }
    memset(reply,0,sizeof(ndb_mgm_reply));
    int ret = ndb_mgm_exit_single_user($self,reply);
    if (ret == -1) {
      free(reply);
      return NULL;
    }
    return reply;
  }


  %ndbexception("NdbMgmException") {
    $action
      if (result == -1) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  /** @} *********************************************************************/
  /**
   * @name Functions: Start/stop nodes
   * @{
   */

  /**
   * Stops database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   Number of database nodes to be stopped<br>
   *                          0: All database nodes in cluster<br>
   *                          n: Stop the <var>n</var> node(s) specified in the
   *                            array node_list
   * @param   node_list     List of node IDs for database nodes to be stopped
   *
   * @return                Number of nodes stopped (-1 on error)
   *
   * @note    This function is equivalent
   *          to calling ndb_mgm_stop2(handle, no_of_nodes, node_list, 0)
   */
  %apply int *INOUT { int *node_list };
  int stop(int no_of_nodes, const int * node_list) {
    return ndb_mgm_stop($self,no_of_nodes,node_list);
  }

  /**
   * Stops database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   Number of database nodes to stop<br>
   *                          0: All database nodes in cluster<br>
   *                          n: Stop the <var>n</var> node(s) specified in
   *                            the array node_list
   * @param   node_list     List of node IDs of database nodes to be stopped
   * @param   abort         Don't perform graceful stop,
   *                        but rather stop immediately
   *
   * @return                Number of nodes stopped (-1 on error).
   */
  int stop(int no_of_nodes, const int * node_list, bool abort) {
    return ndb_mgm_stop2($self,no_of_nodes,node_list,abort);
  }

  /**
   * Stops cluster nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   Number of database nodes to stop<br>
   *                         -1: All database and management nodes<br>
   *                          0: All database nodes in cluster<br>
   *                          n: Stop the <var>n</var> node(s) specified in
   *                            the array node_list
   * @param   node_list     List of node IDs of database nodes to be stopped
   * @param   abort         Don't perform graceful stop,
   *                        but rather stop immediately
   * @param   disconnect    Returns true if you need to disconnect to apply
   *                        the stop command (e.g. stopping the mgm server
   *                        that handle is connected to)
   *
   * @return                Number of nodes stopped (-1 on error).
   */
  %apply int *OUTPUT { int *disconnect };

  int stop(int no_of_nodes, const int * node_list, bool abort, int *disconnect) {
    return ndb_mgm_stop3($self,no_of_nodes,node_list,abort,disconnect);
  }


  /**
   * Restart database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   Number of database nodes to restart<br>
   *                          0: All database nodes in cluster<br>
   *                          n: Restart the <var>n</var> node(s) specified in the
   *                            array node_list
   * @param   node_list     List of node IDs of database nodes to be restarted
   *
   * @return                Number of nodes restarted (-1 on error).
   *
   * @note    This function is equivalent to calling
   *          ndb_mgm_restart2(handle, no_of_nodes, node_list, 0, 0, 0);
   */
  int restart(int no_of_nodes, const int * node_list) {
    return ndb_mgm_restart($self,no_of_nodes,node_list);
  }

  /**
   * Restart database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   Number of database nodes to be restarted:<br>
   *                          0: Restart all database nodes in the cluster<br>
   *                          n: Restart the <var>n</var> node(s) specified in the
   *                            array node_list
   * @param   node_list     List of node IDs of database nodes to be restarted
   * @param   initial       Remove filesystem from restarting node(s)
   * @param   nostart       Don't actually start node(s) but leave them
   *                        waiting for start command
   * @param   abort         Don't perform graceful restart,
   *                        but rather restart immediately
   *
   * @return                Number of nodes stopped (-1 on error).
   */
  int restart(int no_of_nodes,const int * node_list, bool initial,
              bool nostart, bool abort) {
    return ndb_mgm_restart2($self, no_of_nodes, node_list, initial, nostart, abort);
  }

  /**
   * Restart nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   Number of database nodes to be restarted:<br>
   *                          0: Restart all database nodes in the cluster<br>
   *                          n: Restart the <var>n</var> node(s) specified in the
   *                            array node_list
   * @param   node_list     List of node IDs of database nodes to be restarted
   * @param   initial       Remove filesystem from restarting node(s)
   * @param   nostart       Don't actually start node(s) but leave them
   *                        waiting for start command
   * @param   abort         Don't perform graceful restart,
   *                        but rather restart immediately
   * @param   disconnect    Returns true if mgmapi client must disconnect from
   *                        server to apply the requested operation. (e.g.
   *                        restart the management server)
   *
   *
   * @return                Number of nodes stopped (-1 on error).
   */
  int restart(int no_of_nodes, const int * node_list, int initial,
              int nostart, int abort, int *disconnect) {

    return ndb_mgm_restart3($self, no_of_nodes, node_list, initial, nostart,
                            abort, disconnect);
  }

  /**
   * Start database nodes
   *
   * @param   handle        Management handle.
   * @param   no_of_nodes   Number of database nodes to be started<br>
   *                        0: Start all database nodes in the cluster<br>
   *                        n: Start the <var>n</var> node(s) specified in
   *                            the array node_list
   * @param   node_list     List of node IDs of database nodes to be started
   *
   * @return                Number of nodes actually started (-1 on error).
   *
   * @note    The nodes to be started must have been started with nostart(-n)
   *          argument.
   *          This means that the database node binary is started and
   *          waiting for a START management command which will
   *          actually enable the database node
   */
  int start(int no_of_nodes,
            const int * node_list) {
    return ndb_mgm_start($self, no_of_nodes, node_list);
  }


  /** @} *********************************************************************/
  /**
   * @name Functions: Controlling Clusterlog output
   * @{
   */

  %ndbexception("NdbMgmException") {
    $action
      if (result == -1) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  /**
   * Filter cluster log severities
   *
   * @param   handle        NDB management handle.
   * @param   severity      A cluster log severity to filter.
   * @param   enable        set 1=enable o 0=disable
   * @param   reply         Reply message.
   *
   * @return                -1 on error.
   */
  int setClusterlogSeverityFilter(ndb_mgm_event_severity severity,
                                  int enable) {

    ndb_mgm_reply * reply = NULL;
    int ret = ndb_mgm_set_clusterlog_severity_filter($self, severity, enable, reply);
    if (reply->return_code != 0) {
      ret = -1;
    }
    free(reply);
    return ret;
  }

  /**
   * Set log category and levels for the cluster log
   *
   * @param   handle        NDB management handle.
   * @param   nodeId        Node ID.
   * @param   category      Event category.
   * @param   level         Log level (0-15).
   * @param   reply         Reply message.
   * @return                -1 on error.
   */
  int getClusterlogLoglevel(int nodeId,
                            ndb_mgm_event_category category,
                            int level, ndb_mgm_reply * reply) {

    return ndb_mgm_set_clusterlog_loglevel(self,
                                           nodeId, category, level,
                                           reply);
  }

  /**
   * Listen to log events. They are read from the return file descriptor
   * and the format is textual, and the same as in the cluster log.
   *
   * @param handle NDB management handle.
   * @param filter pairs of { level, ndb_mgm_event_category } that will be
   *               pushed to fd, level=0 ends list.
   *
   * @return fd    filedescriptor to read events from
   */
  // TODO: Returns a filedescriptor - um, gonna have to figure that out
  //int ndb_mgm_listen_event(NdbMgmHandle handle, const int filter[])


  %ndbexception("NdbMgmException") {
    $action
      if (result == NULL) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  /**
   * Get clusterlog severity filter
   *
   * @param   handle        NDB management handle
   *
   * @param loglevel        A vector of seven (NDB_MGM_EVENT_SEVERITY_ALL)
   *                        elements of struct ndb_mgm_severity,
   *                        where each element contains
   *                        1 if a severity indicator is enabled and 0 if not.
   *                        A severity level is stored at position
   *                        ndb_mgm_clusterlog_level;
   *                        for example the "error" level is stored in position
   *                        [NDB_MGM_EVENT_SEVERITY_ERROR].
   *                        The first element [NDB_MGM_EVENT_SEVERITY_ON] in
   *                        the vector signals whether the cluster log
   *                        is disabled or enabled.
   * @param severity_size   The size of the vector (NDB_MGM_EVENT_SEVERITY_ALL)
   * @return                Number of returned severities or -1 on error
   */
  %newobject getClusterLogSeverityFilter;
  ndb_mgm_severity* getClusterLogSeverityFilter() {
    ndb_mgm_severity * theSeverity = NULL;
    int ret = ndb_mgm_get_clusterlog_severity_filter($self,
                                                     theSeverity,
                                                     NDB_MGM_EVENT_SEVERITY_ALL);
    if (ret == -1) {
      theSeverity = NULL;
    }
    return theSeverity;
  }



  /**
   * get log category and levels
   *
   * @param   handle        NDB management handle.
   * @param loglevel        A vector of twelve (MGM_LOGLEVELS) elements
   *                        of struct ndb_mgm_loglevel,
   *                        where each element contains
   *                        loglevel of corresponding category
   * @param loglevel_size   The size of the vector (MGM_LOGLEVELS)
   * @return                Number of returned loglevels or -1 on error
   */
  ndb_mgm_loglevel * getClusterlogLoglevel() {
    ndb_mgm_loglevel * theLoglevel = NULL;
    int ret = ndb_mgm_get_clusterlog_loglevel($self, theLoglevel, MGM_LOGLEVELS);
    if (ret == -1) {
      theLoglevel = NULL;
    }
    return theLoglevel;
  }

  /**
   * Listen to log events.
   *
   * @param handle NDB management handle.
   * @param filter pairs of { level, ndb_mgm_event_category } that will be
   *               pushed to fd, level=0 ends list.
   *
   * @return       NdbLogEventHandle
   */
  %newobject createNdbLogEventManager;
  NdbLogEventManager * createNdbLogEventManager(const std::vector<NdbFilterItem> filter) {

    int theFilter[filter.size()*2+1];
    for(unsigned x=0;x<filter.size();x++) {
      theFilter[x*2]=filter[x].level;
      theFilter[x*2+1]=(int)(filter[x].category);
    }
    theFilter[filter.size()*2]=0;
    return new NdbLogEventManager(ndb_mgm_create_logevent_handle($self,theFilter));
  }
  %apply int *INOUT { int * filter };
  NdbLogEventManager * createNdbLogEventManager(const int * filter) {

    return new NdbLogEventManager(ndb_mgm_create_logevent_handle($self,filter));
  }


  %newobject dumpState;
  %apply int *INOUT { const int * args };
  ndb_mgm_reply * dumpState(int nodeId,
                            const int * args,
                            int num_args) {
    ndb_mgm_reply * reply = (ndb_mgm_reply *)malloc(sizeof(ndb_mgm_reply));
    if (reply == NULL) {
      return NULL;
    }
    int ret = ndb_mgm_dump_state($self,nodeId,args,num_args,reply);
    if (ret == -1) {
      free(reply);
      return NULL;
    }
    return reply;

  }

  %ndbexception("NdbMgmException") {
    $action
      if (result < 0) {
        //int errCode = ndb_mgm_get_latest_error(arg1);
        const char * errMsg = ndb_mgm_get_latest_error_msg(arg1);
        NDB_exception(NdbMgmException,errMsg);
      }
  }

  int dumpState(int nodeId, int theState) {
    ndb_mgm_reply * reply = (ndb_mgm_reply *)malloc(sizeof(ndb_mgm_reply));
    if (reply == NULL) {
      return -1;
    }
    int theArgs[1];
    theArgs[0]=theState;
    int ret = ndb_mgm_dump_state($self,nodeId,theArgs,1,reply);
    free(reply);
    return ret;
  }

  %ndbnoexception;
};

