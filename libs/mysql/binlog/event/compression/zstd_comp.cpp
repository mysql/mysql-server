/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysql/binlog/event/compression/zstd_comp.h"
//#include <my_byteorder.h>  // TODO: fix this include
#include <algorithm>                               // std::min
#include "mysql/binlog/event/wrapper_functions.h"  // BAPI_TRACE
#include "scope_guard.h"                           // Scope_guard

namespace mysql::binlog::event::compression {

Zstd_comp::Zstd_comp(const Memory_resource_t &memory_resource)
    : m_memory_resource(memory_resource),
      m_zstd_custom_mem{zstd_mem_res_alloc, zstd_mem_res_free,
                        static_cast<void *>(&m_memory_resource)} {}

Zstd_comp::~Zstd_comp() { destroy(); }

void Zstd_comp::destroy() {
  BAPI_TRACE;

  ZSTD_freeCStream(m_ctx);  // Can't fail
  m_ctx = nullptr;
  m_current_compression_level = uninitialized_compression_level;
}

type Zstd_comp::do_get_type_code() const { return type_code; }

void Zstd_comp::set_compression_level(Compression_level_t compression_level) {
  m_next_compression_level = compression_level;
}

void Zstd_comp::do_reset() {
  BAPI_TRACE;

  reset_compressor();

  // Clear pointer to input data.
  m_ibuf.src = nullptr;
  m_ibuf.size = 0;
  m_ibuf.pos = 0;

  // Allow next call to do_compress to change compression level
  m_started = false;
}

void Zstd_comp::reset_compressor() {
  // If context is not allocated, defer initialization until do_compress.
  if (m_ctx != nullptr) {
    // Try to initialize compression context; if it fails, free and
    // set it to nullptr.
    auto init_status = ZSTD_initCStream(m_ctx, m_next_compression_level);
    if (ZSTD_isError(init_status) != 0) {
      BAPI_LOG("info", BAPI_VAR(ZSTD_getErrorName(init_status)));
      destroy();
    } else
      m_current_compression_level = m_next_compression_level;
  }
}

void Zstd_comp::do_feed(const Char_t *input_data, size_t input_size) {
  BAPI_TRACE;

  // Protect against two successive calls to `feed` without a call to
  // `decompress` between them.
  assert(m_ibuf.pos == m_ibuf.size);

  // Store pointer to input data.
  m_ibuf.src = static_cast<const void *>(input_data);
  m_ibuf.size = input_size;
  m_ibuf.pos = 0;
}

Compress_status Zstd_comp::do_compress(Managed_buffer_sequence_t &out) {
  BAPI_TRACE;

  // Create ZSTD compression context if not already created.
  if (m_ctx == nullptr) {
#ifdef BINLOG_EVENT_COMPRESSION_USE_ZSTD_bundled
    m_ctx = ZSTD_createCStream_advanced(m_zstd_custom_mem);
#else
    if (ZSTD_versionNumber() >=
        mysql::binlog::event::compression::ZSTD_INSTRUMENTED_BELOW_VERSION) {
      m_ctx = ZSTD_createCStream();
    } else {
      m_ctx = ZSTD_createCStream_advanced(m_zstd_custom_mem);
    }
#endif

    if (m_ctx == nullptr) return Compress_status::out_of_memory;
  }

  // First invocation of this function for this frame.
  if (!m_started) {
    // Update the compression level if needed.
    if (m_next_compression_level != m_current_compression_level) {
      BAPI_LOG("info", "reset compressor to update compression level from "
                           << m_current_compression_level << " to "
                           << m_next_compression_level);
      reset_compressor();

      // reset_compressor may get OOM error, in which case m_ctx is
      // nullptr and we have to return out_of_memory.
      if (m_ctx == nullptr) return Compress_status::out_of_memory;
    }
#if ZSTD_VERSION_NUMBER >= 10400
    // If user has set a "pledged input size", pass that to Zstd,
    // allowing it to optimize memory usage.  The API is only
    // available in ZSTD version 1.4.0 and higher.
    auto pledged_input_size = get_pledged_input_size();
    if (pledged_input_size != pledged_input_size_unset)
      ZSTD_CCtx_setPledgedSrcSize(m_ctx, pledged_input_size);
#endif
  }
  m_started = true;

  // Consume all input.
  while (m_ibuf.pos < m_ibuf.size) {
    // Make place for more output.
    ZSTD_outBuffer obuf;
    auto grow_status = get_obuf(out, obuf);
    if (grow_status != Compress_status::success) return grow_status;

    // Consume more input and possibly output some more output.
    auto zstd_status = ZSTD_compressStream(m_ctx, &obuf, &m_ibuf);
    BAPI_LOG("info", BAPI_VAR(zstd_status) << " " << BAPI_VAR(obuf.pos));
    if (ZSTD_isError(zstd_status) != 0) {
      BAPI_LOG("info", BAPI_VAR(ZSTD_getErrorName(zstd_status)));
      return Compress_status::out_of_memory;
    }

    // Move the position in the Managed_buffer_sequence.
    move_position(out, obuf.pos);
  }

  BAPI_LOG("info", BAPI_VAR(m_ibuf.pos) << " " << BAPI_VAR(m_ibuf.size));

  return Compress_status::success;
}

Compress_status Zstd_comp::do_finish(Managed_buffer_sequence_t &out) {
  BAPI_TRACE;

  size_t zstd_status{0};
  // Produce all output
  do {
    // Make place for more output.
    ZSTD_outBuffer obuf;
    auto grow_status = get_obuf(out, obuf);
    if (grow_status != Compress_status::success) return grow_status;

    // Request ZSTD to end the stream after the current input, and to
    // flush as much as possible of internal buffers to the output.
    zstd_status = ZSTD_endStream(m_ctx, &obuf);

    BAPI_LOG("info", BAPI_VAR(zstd_status) << " " << BAPI_VAR(obuf.pos));
    if (ZSTD_isError(zstd_status) != 0) {
      BAPI_LOG("info", BAPI_VAR(ZSTD_getErrorName(zstd_status)));
      return Compress_status::out_of_memory;
    }
    // Move the position mark in the Managed_buffer_sequence.
    move_position(out, obuf.pos);
  } while (zstd_status > 0);
  BAPI_LOG("info", BAPI_VAR(m_ibuf.pos) << " " << BAPI_VAR(m_ibuf.size));
  m_started = false;
  return Compress_status::success;
}

void Zstd_comp::move_position(
    Managed_buffer_sequence_t &managed_buffer_sequence, Size_t delta) {
  managed_buffer_sequence.increase_position(delta);
}

[[NODISCARD]] Compress_status Zstd_comp::get_obuf(
    Managed_buffer_sequence_t &managed_buffer_sequence, ZSTD_outBuffer &obuf) {
  BAPI_TRACE;
  auto &write_part = managed_buffer_sequence.write_part();
  if (write_part.size() == 0) {
    // Reserve at least one byte.  Why one?  Because the only strict
    // requirement on this function is to get more space, i.e., at
    // least one byte.  Then there is logic encoded in the
    // Grow_calculator and in the parameters for the Grow_calculator
    // which typically make it allocate more.  Those are
    // performance-improving heuristics and not strict requirements.
    auto grow_status = managed_buffer_sequence.reserve_write_size(1);
    if (grow_status != Compress_status::success) return grow_status;
  }
  // Write output
  auto buffer_it = write_part.begin();
  assert(buffer_it != write_part.end());
  obuf.dst = buffer_it->data();
  obuf.size = buffer_it->size();
  obuf.pos = 0;
  return Compress_status::success;
}

Compressor::Grow_constraint_t Zstd_comp::do_get_grow_constraint_hint() const {
  Grow_constraint_t ret;
  ret.set_grow_increment(ZSTD_CStreamOutSize());
  if (get_pledged_input_size() != pledged_input_size_unset)
    ret.set_max_size(ZSTD_compressBound(get_pledged_input_size()));
  return ret;
}

void *Zstd_comp::zstd_mem_res_alloc(void *opaque, size_t size) {
  Memory_resource_t *mem_res = static_cast<Memory_resource_t *>(opaque);
  return mem_res->allocate(size);
}

void Zstd_comp::zstd_mem_res_free(void *opaque, void *address) {
  Memory_resource_t *mem_res = static_cast<Memory_resource_t *>(opaque);
  mem_res->deallocate(address);
}

}  // namespace mysql::binlog::event::compression
