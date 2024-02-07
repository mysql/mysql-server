/*
   Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SHARED_MEMORY_CONNECTION_INCLUDED
#define SHARED_MEMORY_CONNECTION_INCLUDED

#include <Windows.h>
#include <string>

class Channel_info;
class THD;

/**
  This class abstract a shared memory listener to listen for connection
  events that connect via the shared memory.
*/
class Shared_mem_listener {
  std::string m_shared_mem_name;
  HANDLE m_connect_file_map;
  char *m_connect_map;
  HANDLE m_connect_named_mutex;
  HANDLE m_event_connect_request;
  HANDLE m_event_connect_answer;
  SECURITY_ATTRIBUTES *m_sa_event;
  SECURITY_ATTRIBUTES *m_sa_mapping;
  SECURITY_ATTRIBUTES *m_sa_mutex;
  int m_connect_number;
  char *m_suffix_pos;
  char *m_temp_buffer;

  HANDLE m_handle_client_file_map;
  char *m_handle_client_map;
  HANDLE m_event_client_wrote;
  HANDLE m_event_client_read;  // for transfer data server <-> client
  HANDLE m_event_server_wrote;
  HANDLE m_event_server_read;
  HANDLE m_event_conn_closed;

  void close_shared_mem();

 public:
  /**
    Constructor to create shared memory listener.

    @param  shared_memory_base_name pointer to shared memory base name.
  */
  Shared_mem_listener(const std::string *shared_memory_base_name)
      : m_shared_mem_name(*shared_memory_base_name),
        m_connect_file_map(nullptr),
        m_connect_map(nullptr),
        m_connect_named_mutex(nullptr),
        m_event_connect_request(nullptr),
        m_event_connect_answer(nullptr),
        m_sa_event(nullptr),
        m_sa_mapping(nullptr),
        m_sa_mutex(nullptr),
        m_connect_number(1),
        m_temp_buffer(nullptr),
        m_handle_client_file_map(nullptr),
        m_handle_client_map(nullptr),
        m_event_client_wrote(nullptr),
        m_event_client_read(nullptr),
        m_event_server_wrote(nullptr),
        m_event_server_read(nullptr),
        m_event_conn_closed(nullptr) {}

  /**
    Set up a listener.

    @retval false listener listener has been setup successfully to listen for
    connect events true  failure in setting up the listener.
  */
  bool setup_listener();

  /**
    The body of the event loop that listen for connection events from clients.

    @retval Channel_info   Channel_info object abstracting the connected client
                           details for processing this connection.
  */
  Channel_info *listen_for_connection_event();

  /**
    Spawn admin connection handler thread if separate thread is required to
    accept admin connections. Currently we do not support
    shared memory admin connections.
    Hence this method is noop.

    TODO: Implement for supporting admin connections via shared memory channel.

    @return false as the method is a NOOP.
  */
  bool check_and_spawn_admin_connection_handler_thread() const { return false; }

  /**
    Close the listener.
  */
  void close_listener();
};

#endif  // SHARED_MEMORY_CONNECTION_INCLUDED
