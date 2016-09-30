/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_OUTPUT_BUFFER_H_
#define _NGS_OUTPUT_BUFFER_H_

#include <stdint.h>
#include <vector>
#include "ngs_common/protocol_protobuf.h"

#include "ngs_common/types.h"
#include "ngs/protocol/buffer.h"
#include "ngs/memory.h"


namespace ngs
{

class Output_buffer;
typedef ngs::Memory_instrumented<Output_buffer>::Unique_ptr Output_buffer_unique_ptr;

class Output_buffer : public Buffer, public google::protobuf::io::ZeroCopyOutputStream
{
public:
  Output_buffer(Page_pool& page_pool);

  bool add_int32(int32_t i);
  bool add_int8(int8_t i);
  bool add_bytes(const char *data, size_t length);

  Const_buffer_sequence get_buffers();

  void save_state();
  void rollback(); // restores last saved state

public:
  virtual bool Next(void** data, int* size);
  virtual void BackUp(int count);

  virtual int64_t ByteCount() const;

private:
  size_t m_saved_length;
};

} // namespace ngs

#endif // _NGS_OUTPUT_BUFFER_H_
