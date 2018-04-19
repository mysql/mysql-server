/*
   Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "named_pipe_connection.h"

#include <errno.h>

#include "channel_info.h"                // Channel_info
#include "connection_handler_manager.h"  // Connection_handler_manager
#include "init_net_server_extension.h"   // init_net_server_extension
#include "mysql/components/services/log_builtins.h"
#include "sql/log.h"
#include "sql/mysqld.h"      // global_system_variables
#include "sql/named_pipe.h"  // create_server_named_pipe.
#include "sql/sql_class.h"   // THD
#include "violite.h"         // Vio

///////////////////////////////////////////////////////////////////////////
// Channel_info_named_pipe implementation
///////////////////////////////////////////////////////////////////////////

/**
  This class abstracts the info. about  windows named pipe of communication
  with server from client.
*/
class Channel_info_named_pipe : public Channel_info {
  // Handle to named pipe.
  HANDLE m_handle;

 protected:
  virtual Vio *create_and_init_vio() const {
    return vio_new_win32pipe(m_handle);
  }

 public:
  /**
    Constructor that sets the pipe handle

    @param handle    connected pipe handle
  */
  Channel_info_named_pipe(HANDLE handle) : m_handle(handle) {}

  virtual THD *create_thd() {
    THD *thd = Channel_info::create_thd();

    if (thd != NULL) {
      init_net_server_extension(thd);
      thd->security_context()->set_host_ptr(my_localhost, strlen(my_localhost));
    }
    return thd;
  }

  virtual void send_error_and_close_channel(uint errorcode, int error,
                                            bool senderror) {
    Channel_info::send_error_and_close_channel(errorcode, error, senderror);

    DisconnectNamedPipe(m_handle);
    CloseHandle(m_handle);
  }
};

///////////////////////////////////////////////////////////////////////////
// Named_pipe_listener implementation
///////////////////////////////////////////////////////////////////////////

bool Named_pipe_listener::setup_listener() {
  m_connect_overlapped.hEvent = CreateEvent(NULL, true, false, NULL);
  if (!m_connect_overlapped.hEvent) {
    LogErr(ERROR_LEVEL, ER_CONN_PIP_CANT_CREATE_EVENT, GetLastError());
    return true;
  }

  m_pipe_handle = create_server_named_pipe(
      &m_sa_pipe_security, &m_sd_pipe_descriptor,
      global_system_variables.net_buffer_length, m_pipe_name.c_str(),
      m_pipe_path_name, sizeof(m_pipe_path_name));
  if (m_pipe_handle == INVALID_HANDLE_VALUE) return true;

  return false;
}

Channel_info *Named_pipe_listener::listen_for_connection_event() {
  /* wait for named pipe connection */
  BOOL fConnected = ConnectNamedPipe(m_pipe_handle, &m_connect_overlapped);
  if (!fConnected && (GetLastError() == ERROR_IO_PENDING)) {
    /*
      ERROR_IO_PENDING says async IO has started but not yet finished.
      GetOverlappedResult will wait for completion.
    */
    DWORD bytes;
    fConnected =
        GetOverlappedResult(m_pipe_handle, &m_connect_overlapped, &bytes, true);
  }
  if (connection_events_loop_aborted()) return NULL;
  if (!fConnected) fConnected = GetLastError() == ERROR_PIPE_CONNECTED;
  if (!fConnected) {
    CloseHandle(m_pipe_handle);
    if ((m_pipe_handle = CreateNamedPipe(
             m_pipe_path_name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
             PIPE_UNLIMITED_INSTANCES,
             (int)global_system_variables.net_buffer_length,
             (int)global_system_variables.net_buffer_length,
             NMPWAIT_USE_DEFAULT_WAIT, &m_sa_pipe_security)) ==
        INVALID_HANDLE_VALUE) {
      LogErr(ERROR_LEVEL, ER_CONN_PIP_CANT_CREATE_PIPE, strerror(errno));
      return NULL;
    }
  }
  HANDLE hConnectedPipe = m_pipe_handle;
  /* create new pipe for new connection */
  if ((m_pipe_handle = CreateNamedPipe(
           m_pipe_path_name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
           PIPE_UNLIMITED_INSTANCES,
           (int)global_system_variables.net_buffer_length,
           (int)global_system_variables.net_buffer_length,
           NMPWAIT_USE_DEFAULT_WAIT, &m_sa_pipe_security)) ==
      INVALID_HANDLE_VALUE) {
    LogErr(ERROR_LEVEL, ER_CONN_PIP_CANT_CREATE_PIPE, strerror(errno));
    m_pipe_handle = hConnectedPipe;
    return NULL;  // We have to try again
  }

  Channel_info *channel_info =
      new (std::nothrow) Channel_info_named_pipe(hConnectedPipe);
  if (channel_info == NULL) {
    DisconnectNamedPipe(hConnectedPipe);
    CloseHandle(hConnectedPipe);
    return NULL;
  }
  return channel_info;
}

void Named_pipe_listener::close_listener() {
  if (m_pipe_handle == INVALID_HANDLE_VALUE) return;

  DBUG_PRINT("quit", ("Deintializing Named_pipe_connection_acceptor"));

  /* Create connection to the handle named pipe handler to break the loop */
  HANDLE temp;
  if ((temp = CreateFile(m_pipe_path_name, GENERIC_READ | GENERIC_WRITE, 0,
                         NULL, OPEN_EXISTING, 0, NULL)) !=
      INVALID_HANDLE_VALUE) {
    WaitNamedPipe(m_pipe_path_name, 1000);
    DWORD dwMode = PIPE_READMODE_BYTE | PIPE_WAIT;
    SetNamedPipeHandleState(temp, &dwMode, NULL, NULL);
    CancelIo(temp);
    DisconnectNamedPipe(temp);
    CloseHandle(temp);
  }
  CloseHandle(m_connect_overlapped.hEvent);
}
