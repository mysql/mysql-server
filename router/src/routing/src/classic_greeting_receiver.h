/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_GREETING_RECEIVER_INCLUDED
#define ROUTING_CLASSIC_GREETING_RECEIVER_INCLUDED

#include "processor.h"

/**
 * classic protocol handshake between client<->router (and router<->server).
 *
 *
 */
class ClientGreetor : public Processor {
 public:
  using Processor::Processor;

  /**
   * stages of the handshake flow.
   */
  enum class Stage {
    Init,
    ServerGreeting,
    ServerFirstGreeting,
    ClientGreeting,
    TlsAcceptInit,
    TlsAccept,
    ClientGreetingAfterTls,
    RequestPlaintextPassword,
    PlaintextPassword,
    Accepted,
    Authenticated,

    Error,
    Ok,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> server_greeting();
  stdx::expected<Result, std::error_code> server_first_greeting();
  stdx::expected<Result, std::error_code> client_greeting();
  stdx::expected<Result, std::error_code> tls_accept_init();
  stdx::expected<Result, std::error_code> tls_accept();
  stdx::expected<Result, std::error_code> client_greeting_after_tls();
  stdx::expected<Result, std::error_code> request_plaintext_password();
  stdx::expected<Result, std::error_code> plaintext_password();
  stdx::expected<Result, std::error_code> accepted();
  stdx::expected<Result, std::error_code> authenticated();
  stdx::expected<Result, std::error_code> error();

  Stage stage_{Stage::Init};

  classic_protocol::message::server::Error connect_err_{};
};

#endif
