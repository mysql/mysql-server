/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef _NGS_SESSION_INTERFACE_H_
#define _NGS_SESSION_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"

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
  virtual void on_auth_success(const Authentication_interface::Response &response) = 0;
  virtual void on_auth_failure(const Authentication_interface::Response &response) = 0;

  // handle a single message, returns true if message was handled false if not
  virtual bool handle_message(Request &command) = 0;


public:
  virtual State state() const = 0;
  virtual State state_before_close() const = 0;

  virtual Client_interface &client() = 0;

  virtual void mark_as_tls_session() = 0;
  virtual THD* get_thd() const = 0;
  virtual Sql_session_interface &data_context() = 0;
  virtual Protocol_encoder_interface &proto() = 0;

};

} // namespace ngs

#endif // _NGS_SESSION_INTERFACE_H_
