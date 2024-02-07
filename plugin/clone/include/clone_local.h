/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
@file clone/include/clone_local.h
Clone Plugin: Local clone interface

*/

#ifndef CLONE_LOCAL_H
#define CLONE_LOCAL_H

#include "plugin/clone/include/clone.h"
#include "plugin/clone/include/clone_client.h"
#include "plugin/clone/include/clone_hton.h"
#include "plugin/clone/include/clone_server.h"

/* Namespace for all clone data types */
namespace myclone {
/** We create it for Local Clone. It retrieves data from Storage Engines
and applies to the cloned data directory. It uses embedded "Clone Client"
and "Clone Server" object to accomplish the job. "Clone Server" is used to
fetch the data from server which is returned via callbacks. "Clone Client"
handle applies the data to create a cloned database. */
class Local {
 public:
  /** Construct clone local. Initialize "Clone Client" and
  "Clone Server" objects.
  @param[in,out]	thd		server thread handle
  @param[in]		server		shared server handle
  @param[in]		share		shared client information
  @param[in]		index		index of current thread
  @param[in]		is_master	true, if it is master thread */
  Local(THD *thd, Server *server, Client_Share *share, uint32_t index,
        bool is_master);

  /** Get clone client for data transfer.
  @return clone client handle */
  Client *get_client() { return (&m_clone_client); }

  /** Get clone server for data transfer.
  @return clone server handle */
  Server *get_server() { return (m_clone_server); }

  /** Clone current database and update PFS.
  @return error code */
  int clone();

  /** Clone current database to the destination data directory.
  @return error code */
  int clone_exec();

 private:
  /** "Clone Server" object to copy data */
  Server *m_clone_server;

  /** "Clone Client" object to apply data */
  Client m_clone_client;
};

/** Clone Local interface to handle callback from Storage Engines */
class Local_Callback : public Ha_clone_cbk {
 public:
  /** Construct Callback. Set clone local object.
  @param[in]	clone	clone local object */
  Local_Callback(Local *clone) : m_clone_local(clone), m_apply_data(false) {}

  /** Get clone client object
  @return clone client */
  Client *get_clone_client() { return (m_clone_local->get_client()); }

  /** Get clone server object
  @return clone server */
  Server *get_clone_server() { return (m_clone_local->get_server()); }

  /** Get external handle of "Clone Client"
  @return client external handle */
  Data_Link *get_client_data_link() {
    auto client = m_clone_local->get_client();
    MYSQL *conn;

    return (client->get_data_link(conn));
  }

  /** Clone local file callback: Set source file as external handle
  for embedded "Clone Client" object and apply data using storage
  engine interface.
  @param[in]	from_file	source file descriptor
  @param[in]	len		data length
  @return error code */
  int file_cbk(Ha_clone_file from_file, uint len) override;

  /** Clone local buffer callback: Set source buffer as external handle
  for embedded "Clone Client" object and apply data using storage
  engine interface.
  @param[in]	from_buffer	source buffer
  @param[in]	buf_len		data length
  @return error code */
  int buffer_cbk(uchar *from_buffer, uint buf_len) override;

  /** Clone local apply file callback: Copy data from "Clone Client"
  external handle to storage engine file.
  @param[in]	to_file	destination file
  @return error code */
  int apply_file_cbk(Ha_clone_file to_file) override;

  /** Clone local apply callback: get data in buffer
  @param[out]	to_buffer	data buffer
  @param[out]	len		data length
  @return error code */
  int apply_buffer_cbk(uchar *&to_buffer, uint &len) override;

 private:
  /** Apply data using storage engine apply interface.
  @return error code */
  int apply_data();

  /** Acknowledge data transfer.
  @return error code */
  int apply_ack();

  /** Apply data to local file or buffer.
  @param[in,out]	to_file		destination file
  @param[in]		apply_file	copy data to file
  @param[out]		to_buffer	data buffer
  @param[out]		to_len		data length
  @return error code */
  int apply_cbk(Ha_clone_file to_file, bool apply_file, uchar *&to_buffer,
                uint &to_len);

 private:
  /** Clone local object */
  Local *m_clone_local;

  /** Applying cloned data */
  bool m_apply_data;
};

}  // namespace myclone

#endif /* CLONE_LOCAL_H */
