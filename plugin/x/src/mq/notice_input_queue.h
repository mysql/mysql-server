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

#ifndef PLUGIN_X_SRC_MQ_NOTICE_INPUT_QUEUE_H_
#define PLUGIN_X_SRC_MQ_NOTICE_INPUT_QUEUE_H_

#include <cstdint>
#include <list>
#include <memory>
#include <queue>
#include <string>

#include "plugin/x/src/interface/server_task.h"
#include "plugin/x/src/mq/broker_context.h"
#include "plugin/x/src/mq/notice_input_queue.h"
#include "plugin/x/src/ngs/notice_descriptor.h"

namespace xpl {

/**
  Broker input queue

  The queue provides separation between a thread that generates the event
  and worker thread that is going to encode/dispatch the protobuf message
  to interested clients. The main goal is to make `emplace` method fast.
*/
class Notice_input_queue {
 public:
  using Notice_descriptor = ngs::Notice_descriptor;
  using Notice_type = ngs::Notice_type;
  using State = Broker_context::State;

 public:
  Notice_input_queue();

  bool emplace(const Notice_type notice_id, const std::string &notice_payload);

  std::unique_ptr<iface::Server_task> create_broker_task();

 private:
  std::shared_ptr<Broker_context> m_context;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_MQ_NOTICE_INPUT_QUEUE_H_
