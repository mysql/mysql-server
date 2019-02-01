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

#ifndef PLUGIN_X_SRC_MQ_NOTICE_OUTPUT_QUEUE_H_
#define PLUGIN_X_SRC_MQ_NOTICE_OUTPUT_QUEUE_H_

#include <memory>
#include <queue>

#include "plugin/x/ngs/include/ngs/interface/notice_configuration_interface.h"
#include "plugin/x/ngs/include/ngs/interface/notice_output_queue_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/notice_descriptor.h"
#include "plugin/x/ngs/include/ngs/protocol_decoder.h"
#include "plugin/x/src/helper/multithread/mutex.h"
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
class Notice_output_queue : public ngs::Notice_output_queue_interface {
 public:
  Notice_output_queue(ngs::Protocol_encoder_interface *encoder,
                      ngs::Notice_configuration_interface *notice_configuration)
      : m_encoder(encoder), m_notice_configuration(notice_configuration) {}

  void emplace(const ngs::Notice_type type,
               const Buffer_shared &binary_notice) override;
  Waiting_for_io_interface &get_callbacks_waiting_for_io() override;
  void encode_queued_items(const bool last_notice_does_force_fulsh) override;

 private:
  class Idle_reporting;

  ngs::Protocol_encoder_interface *m_encoder;
  ngs::Notice_configuration_interface *m_notice_configuration;
  std::queue<Buffer_shared> m_queue;
  std::unique_ptr<Waiting_for_io_interface> m_decoder_io_callbacks;
  Mutex m_queue_mutex{KEY_mutex_x_notice_output_queue};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_MQ_NOTICE_OUTPUT_QUEUE_H_
