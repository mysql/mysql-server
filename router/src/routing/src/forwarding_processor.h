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

#ifndef ROUTING_CLASSIC_FORWARDING_PROCESSOR_INCLUDED
#define ROUTING_CLASSIC_FORWARDING_PROCESSOR_INCLUDED

#include "processor.h"

/**
 * a processor base class with helper functions.
 */
class ForwardingProcessor : public Processor {
 public:
  using Processor::Processor;

 protected:
  /**
   * forward the current packet from the server-side to the client-side.
   *
   * use 'noflush' if the next message is from the server side too to allow
   * merging of multiple server-side packets into one "send-to-client".
   *
   * Useful for resultsets which is split into multiple packets.
   *
   * Pushes a ServerToClientForwarder to the processor-stack.
   *
   * @param noflush if true, it isn't required to wait until the packet is sent
   * to the client.
   */
  stdx::expected<Result, std::error_code> forward_server_to_client(
      bool noflush = false);

  /**
   * forward the current packet from the client-side to the server-side.
   *
   * Pushes a ClientToServerForwarder to the processor-stack.
   *
   * @param noflush if true, it isn't required to wait until the packet is sent
   * to the server.
   */
  stdx::expected<Result, std::error_code> forward_client_to_server(
      bool noflush = false);
};

#endif
