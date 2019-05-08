/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PLUGIN_X_SRC_MQ_BORKER_CONTEXT_H_
#define PLUGIN_X_SRC_MQ_BORKER_CONTEXT_H_

#include <list>
#include <queue>

#include "my_inttypes.h"

#include "plugin/x/ngs/include/ngs/notice_descriptor.h"
#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/src/helper/multithread/synchronize.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace xpl {

/**
  Shared state of the broker

  Class that contains a shared state of input-queue and broker-task, which
  time of life must be managed by shared-pointer to make sure that its freed
  in case when both object holding the reference to it are released.
*/
class Broker_context {
 public:
  enum class State { k_initializing, k_running, k_closing, k_eol };
  using Notice_descriptor = ngs::Notice_descriptor;
  using Publish_list = std::list<Notice_descriptor>;
  using Publish_queue = std::queue<Notice_descriptor, Publish_list>;

 public:
  /** State of the broker */
  State m_state{State::k_initializing};

  /** Queue with events, shared by broker and input-queue */
  Publish_queue m_queue;

  /** Queue synch for pushing thread and reading thread */
  Synchronize m_synchronize{KEY_mutex_x_broker_context_sync,
                            KEY_cond_x_broker_context_sync};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_MQ_BORKER_CONTEXT_H_
