/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "await_client_or_server.h"

#include "classic_connection_base.h"

stdx::expected<Processor::Result, std::error_code>
AwaitClientOrServerProcessor::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::WaitBoth:
      return wait_both();
    case Stage::WaitClientCancelled:
      return wait_client_cancelled();
    case Stage::WaitServerCancelled:
      return wait_server_cancelled();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
AwaitClientOrServerProcessor::init() {
  stage(Stage::WaitBoth);

  return Result::RecvFromBoth;
}

/**
 * wait for an read-event from client and server at the same time.
 *
 * two async-reads have been started, which both will call wait_both(). Only one
 * of the two should continue.
 *
 * To ensure that event handlers are properly synchronized:
 *
 * - the first returning event, cancels the other waiter and leaves without
 *   "returning" (::Void)
 * - the cancelled side, continues with executing.
 */
stdx::expected<Processor::Result, std::error_code>
AwaitClientOrServerProcessor::wait_both() {
  switch (connection()->recv_from_either()) {
    case MysqlRoutingClassicConnectionBase::FromEither::RecvedFromServer: {
      // server side sent something.
      //
      // - cancel the client side
      // - read from server in ::wait_client_cancelled

      stage(Stage::WaitClientCancelled);

      (void)connection()->client_conn().cancel();

      // end this execution branch.
      return Result::Void;
    }
    case MysqlRoutingClassicConnectionBase::FromEither::RecvedFromClient: {
      // client side sent something
      //
      // - cancel the server side
      // - read from client in ::wait_server_cancelled
      stage(Stage::WaitServerCancelled);

      (void)connection()->server_conn().cancel();

      // end this execution branch.
      return Result::Void;
    }
    case MysqlRoutingClassicConnectionBase::FromEither::None:
    case MysqlRoutingClassicConnectionBase::FromEither::Started:
      break;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
AwaitClientOrServerProcessor::wait_server_cancelled() {
  stage(Stage::Done);

  on_done_(AwaitResult::ClientReadable);

  return Result::Again;
}

/**
 * read-event from server while waiting for client command.
 *
 * - either a connection-close by the server or
 * - ERR packet before connection-close.
 */
stdx::expected<Processor::Result, std::error_code>
AwaitClientOrServerProcessor::wait_client_cancelled() {
  stage(Stage::Done);

  on_done_(AwaitResult::ServerReadable);

  return Result::Again;
}
