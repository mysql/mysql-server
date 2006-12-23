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

#ifndef MGMAPI_DEBUG_H
#define MGMAPI_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Start signal logging.
   *
   * @param handle the NDB management handle.
   * @param nodeId the node Id.
   * @param reply the reply message.
   * @return 0 if successful.
   */
  int ndb_mgm_start_signallog(NdbMgmHandle handle,
			      int nodeId,
			      struct ndb_mgm_reply* reply);

  /**
   * Stop signal logging.
   *
   * @param handle the NDB management handle.
   * @param nodeId the node Id.
   * @param reply the reply message.
   * @return 0 if successful.
   */
  int ndb_mgm_stop_signallog(NdbMgmHandle handle,
			     int nodeId,
			     struct ndb_mgm_reply* reply);

  /**
   * Set the signals to log.
   *
   * @param handle the NDB management handle.
   * @param nodeId the node id.
   * @param mode the signal log mode.
   * @param blockNames the block names (space separated).
   * @param reply the reply message.
   * @return 0 if successful or an error code.
   */
  int ndb_mgm_log_signals(NdbMgmHandle handle,
			  int nodeId, 
			  enum ndb_mgm_signal_log_mode mode, 
			  const char* blockNames,
			  struct ndb_mgm_reply* reply);

  /**
   * Set trace.
   *
   * @param handle the NDB management handle.
   * @param nodeId the node id.
   * @param traceNumber the trace number.
   * @param reply the reply message.
   * @return 0 if successful or an error code.
   */
  int ndb_mgm_set_trace(NdbMgmHandle handle,
			int nodeId,
			int traceNumber,
			struct ndb_mgm_reply* reply);

  /**
   * Provoke an error.
   *
   * @param handle the NDB management handle.
   * @param nodeId the node id.
   * @param errrorCode the errorCode.
   * @param reply the reply message.
   * @return 0 if successful or an error code.
   */
  int ndb_mgm_insert_error(NdbMgmHandle handle,
			   int nodeId, 
			   int errorCode,
			   struct ndb_mgm_reply* reply);

  /**
   * Dump state
   *
   * @param handle the NDB management handle.
   * @param nodeId the node id.
   * @param args integer array
   * @param number of args in int array
   * @param reply the reply message.
   * @return 0 if successful or an error code.
   */
  int ndb_mgm_dump_state(NdbMgmHandle handle,
			 int nodeId,
			 const int * args,
			 int num_args,
			 struct ndb_mgm_reply* reply);
    

  /**
   *
   * @param handle the NDB management handle.
   * @param nodeId the node id. 0 = all db nodes
   * @param errrorCode the errorCode.
   * @param reply the reply message.
   * @return 0 if successful or an error code.
   */
  int ndb_mgm_set_int_parameter(NdbMgmHandle handle,
				int node, 
				int param,
				unsigned value,
				struct ndb_mgm_reply* reply);
  
  int ndb_mgm_set_int64_parameter(NdbMgmHandle handle,
				  int node, 
				  int param,
				  unsigned long long value,
				  struct ndb_mgm_reply* reply);

  int ndb_mgm_set_string_parameter(NdbMgmHandle handle,
				   int node, 
				   int param,
				   const char * value,
				   struct ndb_mgm_reply* reply);

  Uint64 ndb_mgm_get_session_id(NdbMgmHandle handle);

  struct NdbMgmSession {
    Uint64 id;
    Uint32 m_stopSelf;
    Uint32 m_stop;
    Uint32 nodeid;
    Uint32 parser_buffer_len;
    Uint32 parser_status;
  };

  int ndb_mgm_get_session(NdbMgmHandle handle, Uint64 id,
                          struct NdbMgmSession *s, int *len);

#ifdef __cplusplus
}
#endif


#endif
