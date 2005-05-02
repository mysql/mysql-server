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

#ifndef REPAPI_H
#define REPAPI_H

/**
 * @mainpage NDB Cluster REP API
 *
 * The NDB Cluster Replication API (REP API) consists of a C API 
 * which is used to:
 * - Start and stop replication
 * - Other administrative tasks
 *
 * The functions use simple ASCII based
 * commands to interact with thw Replication Server.
 *
 *
 * @section  General Concepts
 *
 * Each REP API function call needs an rep_C_Api::NdbRepHandle 
 * which initally is created by 
 * calling the function ndb_rep_create_handle().
 *
 * A function can return:
 *  -# An integer value.  If it returns 0 then this indicates success.
 *  -# A pointer value.  If it returns NULL then check the latest error.
 *         If it didn't return NULL, then "something" is returned.
 *         This "something" has to be free:ed by the user of the REP API.
 */

/** @addtogroup REP_C_API
 *  @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#define REPAPI_MAX_NODE_GROUPS 4  	
  /**
   * The NdbRepHandle.
   */
  typedef struct ndb_rep_handle * NdbRepHandle;


  /**
   *   Default reply from the server
   */
  struct ndb_rep_reply {
    int return_code;                        ///< 0 if successful, 
                                            ///< otherwise error code
    char message[256];                      ///< Error or reply message.
  };

  enum QueryCounter {
    PS = 0,            ///< Stored on Primary System REP
    SSReq = 1,         ///< Requested for transfer to Standby System
    SS = 2,            ///< Stored on Standby System REP
    AppReq = 3,        ///< Requested to be applied to Standby System
    App = 4,           ///< Has been applied to Standby System
    DelReq = 5,        ///< Has been requested to be deleted on PS REP & SS REP
    Subscription = 6,   
    ConnectionRep = 7,
    ConnectionDb = 8
  };


  struct rep_state {
    QueryCounter queryCounter;
    unsigned int no_of_nodegroups;
    unsigned int connected_rep;
    unsigned int connected_db;
    unsigned int subid;
    unsigned int subkey;
    unsigned int state;
    unsigned int state_sub;
    unsigned int first[REPAPI_MAX_NODE_GROUPS];  //4 = max no of nodegroups
    unsigned int last[REPAPI_MAX_NODE_GROUPS];   //4 = max no of nodegroups
  };


  


  
  /***************************************************************************
   * FUNCTIONS
   ***************************************************************************/
  /** 
   * Create a handle
   *
   * @return  A handle != 0
   *          or 0 if failed to create one. (Check errno then).
   */
  NdbRepHandle ndb_rep_create_handle();
  
  /**
   * Destroy a handle
   *
   * @param   handle  Rep server  handle
   */
  void ndb_rep_destroy_handle(NdbRepHandle * handle);
  
  /**
   * Get latest error associated with a handle
   *
   * @param   handle  Rep server handle
   * @return  Latest error.
   */
  int ndb_rep_get_latest_error(const NdbRepHandle handle);

  /**
   * Get latest error line associated with a handle
   *
   * @param   handle  Rep server handle.
   * @return  Latest error line.
   */
  int ndb_rep_get_latest_error_line(const NdbRepHandle handle);

  /**
   * Connect to a REP server
   *
   * @param   handle  Rep server handle.
   * @param   repsrv  Hostname and port of the REP server, 
   *                  "hostname:port".
   * @return  0 if OK, sets ndb_rep_handle->last_error otherwise.
   */
  int ndb_rep_connect(NdbRepHandle handle, const char * repsrv);
  
  /**
   * Disconnect from a REP server
   *
   * @param  handle   Rep server handle.
   */
  void ndb_rep_disconnect(NdbRepHandle handle);
  

  /**
   * Global Replication Command
   *
   * @param handle          NDB REP handle.
   * @param request         Type of request
   * @param replicationId   Replication id is returned from function.
   * @param reply           Reply message.
   * @param epoch           Currenty used to STOP at a certain EPOCH
   * @return                0 if successful, error code otherwise.
   */
  int ndb_rep_command(NdbRepHandle           handle,
		      unsigned int           request,
		      unsigned int*          replicationId,
		      struct ndb_rep_reply*  reply,
		      unsigned int           epoch = 0);


    /**
   * Global Replication Command
   *
   * @param handle          NDB REP handle.
   * @param counter         Type of request. If <0, then 
                             "first" and "last" in repstate
			     is set to 0;x			    
   * @param replicationId   Replication id is returned from function.
   * @param reply           Reply message.
   * @param repstate        Struct containing queried data. (Note!
   *                        All values are set in the struct, regardless
                            which QueryCounter that has been set
   * @return                0 if successful, error code otherwise.
   */
  int ndb_rep_query(NdbRepHandle           handle,
		    QueryCounter           counter,
		    unsigned int*          replicationId,
		    struct ndb_rep_reply*  reply, 			  
		    struct rep_state * repstate);
		    

/**
 * @deprecated (will probably be). Can use ndb_rep_query instead.
 */
  int ndb_rep_get_status(NdbRepHandle handle,
			 unsigned int* replication_id,
			 struct ndb_rep_reply* /*reply*/,
			 struct rep_state * repstate);
  

  
  enum RequestStatusCode {
    OK = 0,            ///< Everything OK
    Error = 1,	     ///< Generic error
    AlreadyExists = 2, ///< Entry already exists in list
    NotExists = 3,     ///< Entry does not exist in list
    AlreadyStopped = 4
  };
  

  
#ifdef __cplusplus
}
#endif

/** @} */

#endif
