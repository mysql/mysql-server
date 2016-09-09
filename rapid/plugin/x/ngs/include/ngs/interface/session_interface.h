/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_SESSION_INTERFACE_H_
#define _NGS_SESSION_INTERFACE_H_

#include "ngs/protocol_authentication.h"

namespace ngs
{

class Request;
class Client_interface;

class Session_interface
{
public:
  typedef int32_t Session_id;

  enum State
  {
    // start as Authenticating
    Authenticating,
    // once authenticated, we can handle work
    Ready,
    // connection is closing, but wait for data to flush out first
    Closing
  };

public:
  virtual ~Session_interface() { }

  virtual Session_id session_id() const = 0;
  virtual Error_code init() = 0;

public:
  virtual void on_close(const bool update_old_state = false) = 0;
  virtual void on_kill() = 0;
  virtual void on_auth_success(const Authentication_handler::Response &response) = 0;
  virtual void on_auth_failure(const Authentication_handler::Response &response) = 0;

  // handle a single message, returns true if message was handled false if not
  virtual bool handle_message(Request &command) = 0;

public:
  virtual State state() const = 0;
  virtual State state_before_close() const = 0;

  virtual Client_interface &client() = 0;

  virtual void mark_as_tls_session() = 0;
  virtual bool is_handled_by(const void *handler) const = 0;
};

} // namespace ngs

#endif // _NGS_SESSION_INTERFACE_H_
