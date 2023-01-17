/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
@file clone/include/clone_server.h
Clone Plugin: Server interface

*/

#ifndef CLONE_SERVER_H
#define CLONE_SERVER_H

#include "plugin/clone/include/clone.h"
#include "plugin/clone/include/clone_hton.h"
#include "plugin/clone/include/clone_os.h"

/* Namespace for all clone data types */
namespace myclone {
/** For Remote Clone, "Clone Server" is created at donor. It retrieves data
from Storage Engines and transfers over network to remote "Clone Client". */
class Server {
 public:
  /** Construct clone server. Initialize storage and external handle
  @param[in,out]	thd	server thread handle
  @param[in]	socket	network socket to remote client */
  Server(THD *thd, MYSQL_SOCKET socket);

  /** Destructor: Free the transfer buffer, if created. */
  ~Server();

  /** Get storage handle vector for data transfer.
  @return storage handle vector */
  Storage_Vector &get_storage_vector() { return (m_storage_vec); }

  /** Get clone locator for a storage engine at specified index.
  @param[in]	index	locator index
  @param[out]	loc_len	locator length in bytes
  @return storage locator */
  const uchar *get_locator(uint index, uint &loc_len) const {
    assert(index < m_storage_vec.size());
    loc_len = m_storage_vec[index].m_loc_len;
    return (m_storage_vec[index].m_loc);
  }

  /** Get tasks for different SE
  @return task vector */
  Task_Vector &get_task_vector() { return (m_tasks); }

  /** Get external handle for data transfer. This is the
  network socket to remote client.
  @return external handle */
  Data_Link *get_data_link() { return (&m_ext_link); }

  /** Get server thread handle
  @return server thread */
  THD *get_thd() { return (m_server_thd); }

  /** Allocate and return buufer for data copy
  @param[in]	len	buffer length
  @return allocated pointer */
  uchar *alloc_copy_buffer(uint len) {
    auto err = m_copy_buff.allocate(len);

    if (err != 0) {
      return (nullptr);

    } else {
      assert(m_copy_buff.m_length >= len);
      return (m_copy_buff.m_buffer);
    }
  }

  /** Clone database and send data to remote client.
  @return error code */
  int clone();

  /** Send descriptor data to remote client
  @param[in,out]	hton		SE handlerton
  @param[in]	secure		validate secure connection
  @param[in]	loc_index	current locator index
  @param[in]	desc_buf	descriptor buffer
  @param[in]	desc_len	buffer length
  @return error code */
  int send_descriptor(handlerton *hton, bool secure, uint loc_index,
                      const uchar *desc_buf, uint desc_len);

  /** Send one string value.
  @param[in]	rcmd	response command
  @param[in]	key_str	string key
  @param[in]	val_str	string value
  @return error code */
  int send_key_value(Command_Response rcmd, String_Key &key_str,
                     String_Key &val_str);

  /** Send configurations.
  @param[in]	rcmd	response command
  @return error code */
  int send_configs(Command_Response rcmd);

  /** @return true iff need to send only plugin name for old clone version. */
  bool send_only_plugin_name() const {
    return m_protocol_version < CLONE_PROTOCOL_VERSION_V2;
  }

  /** @return true iff skip sending additional configurations. */
  bool skip_other_configs() const {
    return m_protocol_version < CLONE_PROTOCOL_VERSION_V3;
  }

 private:
  /** Extract client ddl timeout and backup lock flag.
  @param[in]	client_timeout	timeout value received from client */
  void set_client_timeout(uint32_t client_timeout) {
    m_backup_lock = ((client_timeout & NO_BACKUP_LOCK_FLAG) == 0);
    m_client_ddl_timeout = client_timeout & ~NO_BACKUP_LOCK_FLAG;
  }

  /** @return true if clone needs to block concurrent DDL. */
  bool block_ddl() const { return (m_is_master && m_backup_lock); }

  /** Check if network error
  @param[in]	err	error code
  @return true if network error */
  static bool is_network_error(int err) {
    if (err == ER_NET_ERROR_ON_WRITE || err == ER_NET_READ_ERROR ||
        err == ER_NET_WRITE_INTERRUPTED || err == ER_NET_READ_INTERRUPTED ||
        err == ER_NET_WAIT_ERROR) {
      return (true);
    }

    /* Check for protocol error */
    if (err == ER_NET_PACKETS_OUT_OF_ORDER || err == ER_NET_UNCOMPRESS_ERROR ||
        err == ER_NET_PACKET_TOO_LARGE || err == ER_CLONE_PROTOCOL) {
      return (true);
    }

    return (false);
  }

  /** Send status back to client
  @param[in]	err	error code
  @return error code */
  int send_status(int err);

  /** Initialize storage engine using command buffer.
  @param[in]	mode	clone start mode
  @param[in]	com_buf	command buffer
  @param[in]	com_len	command buffer length
  @return error code */
  int init_storage(Ha_clone_mode mode, uchar *com_buf, size_t com_len);

  /** Parse command buffer and execute
  @param[in]	command	command type
  @param[in]	com_buf	buffer to parse
  @param[in]	com_len	buffer length
  @param[out]	done	true if all clone commands are done
  @return error code */
  int parse_command_buffer(uchar command, uchar *com_buf, size_t com_len,
                           bool &done);

  /** Deserialize COM_INIT command buffer to extract version and locators
  @param[in]	init_buf	INIT command buffer
  @param[in]	init_len	buffer length
  @return error code */
  int deserialize_init_buffer(const uchar *init_buf, size_t init_len);

  /** Deserialize COM_ACK command buffer to extract descriptor
  @param[in]	ack_buf		ACK command buffer
  @param[in]	ack_len		buffer length
  @param[in,out]	cbk		callback object
  @param[out]	err_code	remote error
  @param[out]	loc		Locator object
  @return error code */
  int deserialize_ack_buffer(const uchar *ack_buf, size_t ack_len,
                             Ha_clone_cbk *cbk, int &err_code, Locator *loc);

  /** Send back the locators
  @return error code */
  int send_locators();

  /** Send mysql server parameters
  @return error code */
  int send_params();

 private:
  /** Server thread object */
  THD *m_server_thd;

  /** If this is the master task */
  bool m_is_master;

  /** Intermediate buffer for data copy when zero copy is not used. */
  Buffer m_copy_buff;

  /** Buffer holding data for RPC response */
  Buffer m_res_buff;

  /** Clone external handle. Data is transferred from
  storage handle to external handle(network). */
  Data_Link m_ext_link;

  /** Clone storage handle */
  Storage_Vector m_storage_vec;

  /** Task IDs for different SE */
  Task_Vector m_tasks;

  /** Storage vector is initialized */
  bool m_storage_initialized;

  /** PFS statement is initialized */
  bool m_pfs_initialized;

  /** If backup lock is acquired */
  bool m_acquired_backup_lock;

  /** Negotiated protocol version */
  uint32_t m_protocol_version;

  /** DDL timeout from client */
  uint32_t m_client_ddl_timeout;

  /** If backup lock should be acquired */
  bool m_backup_lock;
};

/** Clone server interface to handle callback from Storage Engine */
class Server_Cbk : public Ha_clone_cbk {
 public:
  /** Construct Callback. Set clone server object.
  @param[in]	clone	clone server object */
  Server_Cbk(Server *clone) : m_clone_server(clone) {}

  /** Get clone object
  @return clone server object */
  Server *get_clone_server() const { return (m_clone_server); }

  /** Send descriptor data to remote client */
  int send_descriptor();

  /** Clone server file callback: Send data from file to remote client
  @param[in]	from_file	source file descriptor
  @param[in]	len		data length
  @return error code */
  int file_cbk(Ha_clone_file from_file, uint len) override;

  /** Clone server buffer callback: Send data from buffer to remote client
  @param[in]	from_buffer	source buffer
  @param[in]	buf_len		data length
  @return error code */
  int buffer_cbk(uchar *from_buffer, uint buf_len) override;

  /** Clone server apply callback: Not used for server.
  @param[in]	to_file	destination file descriptor
  @return error code */
  int apply_file_cbk(Ha_clone_file to_file) override;

  /** Clone server apply callback: Not used for server.
  @param[out]  to_buffer  data buffer
  @param[out]  len        data length
  @return error code */
  int apply_buffer_cbk(uchar *&to_buffer, uint &len) override;

 private:
  /** Clone server object */
  Server *m_clone_server;
};

}  // namespace myclone

#endif /* CLONE_SERVER_H */
