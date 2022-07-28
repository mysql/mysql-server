/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_INTERFACE_NOTICE_OUTPUT_QUEUE_H_
#define PLUGIN_X_SRC_INTERFACE_NOTICE_OUTPUT_QUEUE_H_

#include <memory>
#include <string>

#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/waiting_for_io.h"
#include "plugin/x/src/ngs/notice_descriptor.h"

namespace xpl {
namespace iface {

/**
  Interface that represents queue per client/session
*/
class Notice_output_queue {
 public:
  using Buffer_shared = std::shared_ptr<ngs::Notice_descriptor>;

 public:
  virtual ~Notice_output_queue() = default;

  virtual void set_encoder(Protocol_encoder *encoder) = 0;

  virtual void emplace(const Buffer_shared &notice) = 0;

  virtual xpl::iface::Waiting_for_io *get_callbacks_waiting_for_io() = 0;
  virtual void encode_queued_items(const bool last_notice_does_force_fulsh) = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_NOTICE_OUTPUT_QUEUE_H_
