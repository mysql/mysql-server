// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <memory>

#include "mysql/psi/psi_memory.h"
#include "sql/basic_ostream.h"
#include "sql/binlog_reader.h"
#include "sql/changestreams/apply/storage/relay_log/cached_event_payload.h"
#include "sql/log_event.h"

#ifdef HAVE_PSI_MEMORY_INTERFACE

namespace mysql::csa::unittest {

/// Counts my_malloc() and my_free() calls via a swapped PSI memory service.
class Memory_counter {
 public:
  Memory_counter() : m_saved(psi_memory_service) {
    s_allocs = 0;
    s_frees = 0;
    s_live_bytes = 0;
    psi_memory_service = &s_service;
  }
  ~Memory_counter() { psi_memory_service = m_saved; }

  Memory_counter(const Memory_counter &) = delete;
  Memory_counter &operator=(const Memory_counter &) = delete;

  long allocs() const { return s_allocs; }
  long frees() const { return s_frees; }
  long long live_bytes() const { return s_live_bytes; }

 private:
  static void register_memory(const char *, PSI_memory_info *, int) {}
  static PSI_memory_key memory_alloc(PSI_memory_key key, size_t size,
                                     PSI_thread **owner) {
    ++s_allocs;
    s_live_bytes += size;
    *owner = nullptr;
    return key;
  }
  static PSI_memory_key memory_realloc(PSI_memory_key key, size_t old_size,
                                       size_t new_size, PSI_thread **owner) {
    s_live_bytes += static_cast<long long>(new_size) - old_size;
    *owner = nullptr;
    return key;
  }
  static PSI_memory_key memory_claim(PSI_memory_key key, size_t,
                                     PSI_thread **owner, bool) {
    *owner = nullptr;
    return key;
  }
  static void memory_free(PSI_memory_key, size_t size, PSI_thread *) {
    ++s_frees;
    s_live_bytes -= size;
  }

  static inline std::atomic<long> s_allocs{0};
  static inline std::atomic<long> s_frees{0};
  static inline std::atomic<long long> s_live_bytes{0};
  static inline PSI_memory_service_t s_service{
      register_memory, memory_alloc, memory_realloc, memory_claim, memory_free};

  PSI_memory_service_t *m_saved;
};

class RplCachedEventPayloadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_fde = std::make_shared<Format_description_log_event>();
    // Format_description_log_event::write() requires a checksum.
    m_fde->common_footer->checksum_alg =
        mysql::binlog::event::BINLOG_CHECKSUM_ALG_CRC32;
    ASSERT_FALSE(m_fde->write(&m_serialized_fde));
  }

  /// Allocates a payload buffer as the relay log reader does.
  uint8_t *allocate_payload(const void *data, size_t length) {
    uint8_t *buffer = m_allocator.allocate(length);
    memcpy(buffer, data, length);
    return buffer;
  }

  std::shared_ptr<Log_event> m_fde;
  StringBuffer_ostream<1024> m_serialized_fde;
  Default_binlog_event_allocator m_allocator;
};

// A payload destroyed without decode() frees its buffer.
TEST_F(RplCachedEventPayloadTest, DestroyWithoutDecodeFreesBuffer) {
  const std::string data(4096, 'x');
  Memory_counter counter;
  {
    Cached_event_payload payload(
        Event_payload(allocate_payload(data.data(), data.size()), data.size(),
                      false),
        m_fde);
    EXPECT_EQ(1, counter.allocs());
  }
  EXPECT_EQ(counter.allocs(), counter.frees());
  EXPECT_EQ(0, counter.live_bytes());
}

// A successful decode() hands the buffer to the event, which frees it.
TEST_F(RplCachedEventPayloadTest, DecodeTransfersBufferToEvent) {
  const auto length = m_serialized_fde.length();
  Memory_counter counter;
  uint8_t *buffer = allocate_payload(m_serialized_fde.ptr(), length);
  std::shared_ptr<Log_event> event;
  {
    Cached_event_payload payload(Event_payload(buffer, length, false), m_fde);
    event = payload.decode();
    ASSERT_NE(nullptr, event);
    EXPECT_EQ(mysql::binlog::event::FORMAT_DESCRIPTION_EVENT,
              event->get_type_code());
    EXPECT_EQ(reinterpret_cast<char *>(buffer), event->temp_buf);
  }
  // The event still owns the buffer.
  EXPECT_GT(counter.live_bytes(), 0);
  event.reset();
  EXPECT_EQ(counter.allocs(), counter.frees());
  EXPECT_EQ(0, counter.live_bytes());
}

// A failed decode() frees the buffer exactly once.
TEST_F(RplCachedEventPayloadTest, DecodeErrorFreesBufferOnce) {
  // Header length field does not match the buffer length.
  const std::string data(64, '\0');
  Memory_counter counter;
  {
    Cached_event_payload payload(
        Event_payload(allocate_payload(data.data(), data.size()), data.size(),
                      false),
        m_fde);
    EXPECT_EQ(nullptr, payload.decode());
    EXPECT_EQ(1, counter.frees());
    EXPECT_EQ(nullptr, payload.decode());
  }
  EXPECT_EQ(1, counter.allocs());
  EXPECT_EQ(1, counter.frees());
  EXPECT_EQ(0, counter.live_bytes());
}

}  // namespace mysql::csa::unittest

#endif  // HAVE_PSI_MEMORY_INTERFACE
