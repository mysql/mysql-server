/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NAMED_PIPE_CONNECTION_INCLUDED
#define NAMED_PIPE_CONNECTION_INCLUDED

#include "my_global.h"               // uint

#include <string>
#include <Windows.h>

class Channel_info;
class THD;


/**
  This class abstracts Named pipe listener that setups a named pipe
  handle to listen and receive client connections.
*/
class Named_pipe_listener
{
  std::string m_pipe_name;
  SECURITY_ATTRIBUTES m_sa_pipe_security;
  SECURITY_DESCRIPTOR m_sd_pipe_descriptor;
  HANDLE m_pipe_handle;
  char m_pipe_path_name[512];
  HANDLE h_connected_pipe;
  OVERLAPPED m_connect_overlapped;

public:
  /**
    Constructor for named pipe listener

    @param  pipe_name name for pipe used in CreateNamedPipe function.
  */
  Named_pipe_listener(const std::string *pipe_name)
  : m_pipe_name(*pipe_name),
    m_pipe_handle(INVALID_HANDLE_VALUE)
  { }


  /**
    Set up a listener.

    @retval false listener listener has been setup successfully to listen for connect events
            true  failure in setting up the listener.
  */
  bool setup_listener();


  /**
    The body of the event loop that listen for connection events from clients.

    @retval Channel_info   Channel_info object abstracting the connected client
                           details for processing this connection.
  */
  Channel_info* listen_for_connection_event();

  /**
    Close the listener
  */
  void close_listener();
};

#endif // NAMED_PIPE_CONNECTION_INCLUDED.
