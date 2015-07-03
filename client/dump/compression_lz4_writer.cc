/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "compression_lz4_writer.h"

using namespace Mysql::Tools::Dump;

void Compression_lz4_writer::prepare_buffer(size_t src_size)
{
  // bug in lz4 (fixed in their r124).
  LZ4F_preferences_t null_preferences;
  memset(&null_preferences, 0, sizeof(LZ4F_preferences_t));
  m_buffer.resize(LZ4F_compressBound(src_size, &null_preferences));
}

void Compression_lz4_writer::process_buffer(size_t lz4_result)
{
  if (LZ4F_isError(lz4_result))
  {
    this->pass_message(Mysql::Tools::Base::Message_data(
      0, "LZ4 compression failed",
      Mysql::Tools::Base::Message_type_error));
  }
  else if (lz4_result > 0)
  {
    this->append_output(std::string(&m_buffer[0], lz4_result));
  }
}

void Compression_lz4_writer::append(const std::string& data_to_append)
{
  my_boost::mutex::scoped_lock lock(m_lz4_mutex);
  if (m_buffer.capacity() == 0)
  {
    LZ4F_createCompressionContext(&m_compression_context, LZ4F_VERSION);
    this->prepare_buffer(0);
    this->process_buffer(LZ4F_compressBegin(m_compression_context,
      (void*)&m_buffer[0], m_buffer.capacity(), NULL));
  }
  this->prepare_buffer(data_to_append.size());
  this->process_buffer(LZ4F_compressUpdate(m_compression_context,
    (void*)&m_buffer[0], m_buffer.capacity(), data_to_append.c_str(),
    data_to_append.size(), NULL));
}

Compression_lz4_writer::~Compression_lz4_writer()
{
  my_boost::mutex::scoped_lock lock(m_lz4_mutex);
  if (m_buffer.capacity() != 0)
  {
    this->process_buffer(LZ4F_compressEnd(m_compression_context,
      (void*)&m_buffer[0], m_buffer.capacity(), NULL));
    LZ4F_freeCompressionContext(m_compression_context);
  }
}

Compression_lz4_writer::Compression_lz4_writer(
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator)
  : Abstract_output_writer_wrapper(message_handler, object_id_generator)
{}
