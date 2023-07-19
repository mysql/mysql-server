/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MGMAPI_DEBUG_H
#define MGMAPI_DEBUG_H

#include "mgmapi.h"

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
   * @param errorCode the errorCode.
   * @param reply the reply message.
   * @return 0 if successful or an error code.
   */
  int ndb_mgm_insert_error(NdbMgmHandle handle,
			   int nodeId,
			   int errorCode,
			   struct ndb_mgm_reply* reply);

  /**
   * Provoke an error.
   *
   * @param handle the NDB management handle.
   * @param nodeId the node id.
   * @param errorCode the errorCode.
   * @param reply the reply message.
   * @return 0 if successful or an error code.
   */
  int ndb_mgm_insert_error2(NdbMgmHandle handle,
                            int nodeId,
                            int errorCode,
                            int extra,
                            struct ndb_mgm_reply* reply);

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
