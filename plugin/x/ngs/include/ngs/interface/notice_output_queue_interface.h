/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_NOTICE_OUTPUT_QUEUE_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_NOTICE_OUTPUT_QUEUE_INTERFACE_H_

#include <memory>

#include "plugin/x/ngs/include/ngs/notice_descriptor.h"
#include "plugin/x/ngs/include/ngs/protocol_decoder.h"

namespace ngs {

/**
  Interface that represents queue per client/session
*/
class Notice_output_queue_interface {
 public:
  using Buffer_shared = std::shared_ptr<std::string>;
  using Waiting_for_io_interface = Protocol_decoder::Waiting_for_io_interface;

 public:
  virtual ~Notice_output_queue_interface() = default;

  virtual void emplace(const Notice_type type,
                       const Buffer_shared &binary_notice) = 0;

  virtual Waiting_for_io_interface &get_callbacks_waiting_for_io() = 0;
  virtual void encode_queued_items(const bool last_notice_does_force_fulsh) = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_NOTICE_OUTPUT_QUEUE_INTERFACE_H_
