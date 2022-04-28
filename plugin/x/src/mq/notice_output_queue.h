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

#ifndef PLUGIN_X_SRC_MQ_NOTICE_OUTPUT_QUEUE_H_
#define PLUGIN_X_SRC_MQ_NOTICE_OUTPUT_QUEUE_H_

#include <memory>
#include <queue>
#include <string>

#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/interface/notice_configuration.h"
#include "plugin/x/src/interface/notice_output_queue.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/waiting_for_io.h"
#include "plugin/x/src/ngs/notice_descriptor.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace xpl {

/**
  Class implements per client/session queue

  The queue is filled by `emplace` method, and read by `Protocol_decoder`
  idle-interface. The interface switches the decoder from blocking IO
  to short blocks. It is important to switch the short-blocks only when
  its needed. To mark that, `enable_emplace` method was added.
  In general it tell that user would like to receive a global notice, enables
  the queue and switches the decoder to `short-blocks`.
  Data can be also read from the queue manually (in custom places) by
  calling `encode_queued_items`.
*/
class Notice_output_queue : public iface::Notice_output_queue {
 public:
  Notice_output_queue(iface::Protocol_encoder *encoder,
                      iface::Notice_configuration *notice_configuration);

  void emplace(const Buffer_shared &notice) override;
  xpl::iface::Waiting_for_io *get_callbacks_waiting_for_io() override;
  void encode_queued_items(const bool last_notice_does_force_fulsh) override;
  void set_encoder(iface::Protocol_encoder *encoder) override;

 private:
  class Idle_reporting;
  iface::Protocol_encoder *m_encoder;
  iface::Notice_configuration *m_notice_configuration;
  std::queue<Buffer_shared> m_queue;
  std::unique_ptr<xpl::iface::Waiting_for_io> m_decoder_io_callbacks;
  Mutex m_queue_mutex{KEY_mutex_x_notice_output_queue};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_MQ_NOTICE_OUTPUT_QUEUE_H_
