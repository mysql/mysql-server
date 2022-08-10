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

#ifndef PLUGIN_X_SRC_MQ_BROKER_TASK_H_
#define PLUGIN_X_SRC_MQ_BROKER_TASK_H_

#include <cstdint>
#include <memory>
#include <string>

#include "plugin/x/src/interface/server_task.h"
#include "plugin/x/src/mq/broker_context.h"
#include "plugin/x/src/ngs/notice_descriptor.h"

namespace xpl {

class Broker_task : public iface::Server_task {
 public:
  using Notice_descriptor = ngs::Notice_descriptor;
  using Notice_type = ngs::Notice_type;
  using Publish_queue = Broker_context::Publish_queue;
  using State = Broker_context::State;

 public:
  explicit Broker_task(std::shared_ptr<Broker_context> context);

  bool prepare(Task_context *context) override;
  void stop(const Stop_cause) override;

  void pre_loop() override;
  void post_loop() override;
  void loop() override;

 private:
  void wait_for_data_and_swap_queues(
      Broker_context::Publish_queue *workers_queue);

  void distribute(const Notice_descriptor &notice_descriptor);

  std::shared_ptr<Notice_descriptor> create_notice_message(
      const Notice_descriptor &notice_description);

  std::shared_ptr<Broker_context> m_broker_context;
  Task_context m_task_context;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_MQ_BROKER_TASK_H_
