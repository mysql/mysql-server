/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_CLASSIC_PROCESSOR_INCLUDED
#define ROUTING_CLASSIC_PROCESSOR_INCLUDED

#include "basic_protocol_splicer.h"
#include "tracer.h"

class MysqlRoutingClassicConnectionBase;
class ClassicProtocolState;

/**
 * base class of all the processors.
 *
 * Processor
 *
 * - have their own internal state
 * - expose a process() function which will be called until
 *   it returns Result::Done
 *
 * Processors are stacked.
 *
 * The methods:
 *
 * - push_processor()
 * - pop_processor()
 *
 * allow to add and remove elements for the stack.
 *
 * The top-most processor's process() function is called.
 */
class BasicProcessor {
 public:
  enum class Result {
    Again,           // will invoke the process() of the top-most-processor
    RecvFromClient,  // wait for recv from client and invoke ...
    SendToClient,    // wait for send-to-client and invoke ...
    RecvFromServer,  // wait for recv from server and invoke ...
    RecvFromBoth,    // wait for recv from client and server and invoke ..
    SendToServer,    // wait for send-to-server and invoke ...
    SendableToServer,

    Suspend,  // wait for explicit resume
    Done,  // pop this processor and invoke the top-most-processor's process()

    Void,
  };

  BasicProcessor(MysqlRoutingClassicConnectionBase *conn) : conn_(conn) {}

  virtual ~BasicProcessor() = default;

  const MysqlRoutingClassicConnectionBase *connection() const { return conn_; }

  MysqlRoutingClassicConnectionBase *connection() { return conn_; }

  virtual stdx::expected<Result, std::error_code> process() = 0;

 private:
  MysqlRoutingClassicConnectionBase *conn_;
};

/**
 * a processor base class with helper functions.
 */
class Processor : public BasicProcessor {
 public:
  using BasicProcessor::BasicProcessor;

 protected:
  stdx::expected<Result, std::error_code> send_server_failed(
      std::error_code ec);

  stdx::expected<Result, std::error_code> recv_server_failed(
      std::error_code ec);

  stdx::expected<Result, std::error_code> send_client_failed(
      std::error_code ec);

  stdx::expected<Result, std::error_code> recv_client_failed(
      std::error_code ec);

  stdx::expected<Result, std::error_code> server_socket_failed(
      std::error_code ec);

  stdx::expected<Result, std::error_code> client_socket_failed(
      std::error_code ec);

  /**
   * discard to current message.
   *
   * @pre ensure_full_frame() must true.
   */
  stdx::expected<void, std::error_code> discard_current_msg(
      Channel *src_channel, ClassicProtocolState *src_protocol);

  /**
   * log a message with error-code as error.
   */
  static void log_fatal_error_code(const char *msg, std::error_code ec);

  // see MysqlClassicConnection::trace()
  [[deprecated(
      "use 'if (auto &tr = tracer()) { tr.trace(...); } instead")]] void
  trace(Tracer::Event e);

  Tracer &tracer();
};

#endif
