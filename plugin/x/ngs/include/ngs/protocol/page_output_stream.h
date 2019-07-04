/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_OUTPUT_STREAM_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_OUTPUT_STREAM_H_

#include <stdint.h>

#include "plugin/x/ngs/include/ngs/protocol/page_buffer.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

class Page_output_stream : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  Page_output_stream(Page_pool &pool);

  bool Next(void **data, int *size) override;
  void BackUp(int count) override;
  int64_t ByteCount() const override;

 public:
  void visit_buffers(Page_visitor *visitor);

  void backup_current_position();
  void restore_position();

  void *reserve_space(const uint32_t size, const bool update_on_fail = true);
  void reset();

 private:
  bool move_to_next_page();

  Page_buffer m_buffer;
  Page *m_page = nullptr;
  int64_t m_bytes_total = 0;
  int64_t m_backup_bytes_total = 0;
  bool m_fatal = false;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_OUTPUT_STREAM_H_
