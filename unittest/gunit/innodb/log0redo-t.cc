/*****************************************************************************

Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include <cstring>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "log0redo.h"

#include "dict0mem.h"
#include "fil0fil.h"
#include "mach0data.h"
#include "page0page.h"
#include "rem0rec.h"

namespace ib::redo::tests {

/** Error printer. */
std::ostream &operator<<(std::ostream &out, const Parse_error &error) {
  switch (error) {
    case Parse_error::Incomplete:
      return out << "Incomplete";
    case Parse_error::Corrupted:
      return out << "Corrupted";
    default:
      ADD_FAILURE() << "Unknown error: " << static_cast<int>(error);
      return out;
  }
}

template <typename Result>
testing::AssertionResult NoError(const Result &result) {
  if (!result.has_value()) {
    return testing::AssertionFailure()
           << "result has error: " << result.error();
  }
  return testing::AssertionSuccess();
}

template <typename Result>
testing::AssertionResult HasError(const Result &result,
                                  typename Result::error_type error) {
  if (result.has_value()) {
    return testing::AssertionFailure() << "result has no error";
  }
  if (result.error() != error) {
    return testing::AssertionFailure()
           << "result has unexpected error: " << result.error();
  }
  return testing::AssertionSuccess();
}

/** Record builder. */
class Record_builder : public std::vector<uint8_t> {
  static const size_t max_length = 4096;

 public:
  Record_builder(int type, bool single) : std::vector<uint8_t>(max_length) {
    if (single) {
      type |= MLOG_SINGLE_REC_FLAG;
    }
    u8(type);
  }

  size_t length() const { return m_length; }

  std::span<const uint8_t> buffer() const { return {data(), m_length}; }

  Record_builder &u8(uint8_t value) {
    mach_write_to_1(pos(), value);
    m_length += 1;
    return *this;
  }

  Record_builder &u16(uint16_t value) {
    mach_write_to_2(pos(), value);
    m_length += 2;
    return *this;
  }

  Record_builder &u32(uint32_t value) {
    mach_write_to_4(pos(), value);
    m_length += 4;
    return *this;
  }

  Record_builder &u64(uint64_t value) {
    mach_write_to_8(pos(), value);
    m_length += 8;
    return *this;
  }

  Record_builder &u32c(uint32_t value) {
    m_length += mach_write_compressed(pos(), value);
    return *this;
  }

  Record_builder &u64c(uint64_t value) {
    m_length += mach_u64_write_compressed(pos(), value);
    return *this;
  }

  Record_builder &u64mc(uint64_t value) {
    m_length += mach_u64_write_much_compressed(pos(), value);
    return *this;
  }

  Record_builder &string(const char *str, size_t len) {
    std::memcpy(pos(), str, len);
    m_length += len;
    return *this;
  }

 private:
  uint8_t *pos() { return data() + m_length; }

 private:
  size_t m_length = 0;
};

/** Page builder. */
class Page_builder {
 public:
  Page_builder(int page_type)
      : m_data(2 * UNIV_PAGE_SIZE),
        m_aligned((uint8_t *)ut_align(m_data.data(), UNIV_PAGE_SIZE)) {
    mach_write_to_2(data() + FIL_PAGE_TYPE, page_type);
  }

  uint8_t *data() { return m_aligned; }

  size_t size() const { return UNIV_PAGE_SIZE; }

  uint8_t &operator[](size_t idx) { return data()[idx]; }

  std::span<uint8_t> buffer() { return {data(), size()}; }

 private:
  std::vector<uint8_t> m_data;
  uint8_t *m_aligned;
};

/*
Tests.
*/

TEST(log0redo, test_parse_mtr) {
  Redo_applier applier;

  /* Should parse single record mtr. */
  {
    auto record = Record_builder(MLOG_1BYTE, true)
                      .u32c(123) /* space id */
                      .u32c(456) /* page num */
                      .u16(111)  /* offset */
                      .u32c(42); /* value */

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(NoError(mtr));

    EXPECT_EQ(mtr->size(), record.length());
    EXPECT_EQ(mtr->records().size(), 1);

    const auto &rec = mtr->records()[0];
    EXPECT_EQ(rec.kind(), Record_view::Kind::Page);
    EXPECT_EQ(rec.type(), MLOG_1BYTE);
    EXPECT_EQ(rec.size(), record.length());
    EXPECT_EQ(rec.body().data(), record.buffer().subspan(4).data());
    EXPECT_EQ(rec.body().size(), 3);
    EXPECT_EQ(rec.page().space_id, 123);
    EXPECT_EQ(rec.page().page_no, 456);
  }

  /* Should parse multi record mtr. */
  {
    auto record = Record_builder(MLOG_1BYTE, false)
                      .u32c(123)       /* space id */
                      .u32c(456)       /* page num */
                      .u16(111)        /* offset */
                      .u32c(42)        /* value */
                      .u8(MLOG_2BYTES) /* type */
                      .u32c(124)       /* space id */
                      .u32c(457)       /* page num */
                      .u16(112)        /* offset */
                      .u32c(4242)      /* value */
                      .u8(MLOG_MULTI_REC_END);

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(NoError(mtr));

    EXPECT_EQ(mtr->size(), record.length());
    EXPECT_EQ(mtr->records().size(), 3);

    const auto &rec1 = mtr->records()[0];
    EXPECT_EQ(rec1.kind(), Record_view::Kind::Page);
    EXPECT_EQ(rec1.type(), MLOG_1BYTE);
    EXPECT_EQ(rec1.size(), 7);
    EXPECT_EQ(rec1.body().data(), record.buffer().subspan(4).data());
    EXPECT_EQ(rec1.body().size(), 3);
    EXPECT_EQ(rec1.page().space_id, 123);
    EXPECT_EQ(rec1.page().page_no, 456);

    const auto &rec2 = mtr->records()[1];
    EXPECT_EQ(rec2.kind(), Record_view::Kind::Page);
    EXPECT_EQ(rec2.type(), MLOG_2BYTES);
    EXPECT_EQ(rec2.size(), 8);
    EXPECT_EQ(rec2.body().data(), record.buffer().subspan(11).data());
    EXPECT_EQ(rec2.body().size(), 4);
    EXPECT_EQ(rec2.page().space_id, 124);
    EXPECT_EQ(rec2.page().page_no, 457);
  }

  /* Should parse corrupted index metadata. */
  {
    auto record = Record_builder(MLOG_TABLE_DYNAMIC_META, true)
                      .u64mc(1)               /* table id */
                      .u64mc(2)               /* metadata version */
                      .u8(PM_INDEX_CORRUPTED) /* persistent type */
                      .u8(1)                  /* num indices */
                      .u32(123)               /* space id */
                      .u64(456);              /* index id */

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(NoError(mtr));

    EXPECT_EQ(mtr->size(), record.length());
    EXPECT_EQ(mtr->records().size(), 1);
  }

  /* Should parse auto increment metadata. */
  {
    auto record = Record_builder(MLOG_TABLE_DYNAMIC_META, true)
                      .u64mc(1)              /* table id */
                      .u64mc(2)              /* metadata version */
                      .u8(PM_TABLE_AUTO_INC) /* persistent type */
                      .u64mc(123);           /* autoinc counter */

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(NoError(mtr));

    EXPECT_EQ(mtr->size(), record.length());
    EXPECT_EQ(mtr->records().size(), 1);
  }

  /* Should parse MLOG_FILE_CREATE record. */
  {
    auto record = Record_builder(MLOG_FILE_CREATE, true)
                      .u32c(123)           /* space id */
                      .u32c(0)             /* page num */
                      .u32(0)              /* flags */
                      .u16(6)              /* path len */
                      .string("t.ibd", 6); /* path */

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(NoError(mtr));

    EXPECT_EQ(mtr->size(), record.length());
    EXPECT_EQ(mtr->records().size(), 1);

    const auto &rec = mtr->records()[0];
    EXPECT_EQ(rec.kind(), Record_view::Kind::Space);
    EXPECT_EQ(rec.type(), MLOG_FILE_CREATE);
    EXPECT_EQ(rec.size(), record.length());
    EXPECT_EQ(rec.body().data(), record.buffer().subspan(3).data());
    EXPECT_EQ(rec.body().size(), 12);
    EXPECT_EQ(rec.space().space_id, 123);
  }

  /* Should parse MLOG_INDEX_LOAD record. */
  {
    auto record = Record_builder(MLOG_INDEX_LOAD, true)
                      .u32c(123) /* space id */
                      .u32c(456) /* page num */
                      .u64(789); /* index id */

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(NoError(mtr));

    EXPECT_EQ(mtr->size(), record.length());
    EXPECT_EQ(mtr->records().size(), 1);

    const auto &rec = mtr->records()[0];
    EXPECT_EQ(rec.kind(), Record_view::Kind::Space);
    EXPECT_EQ(rec.type(), MLOG_INDEX_LOAD);
    EXPECT_EQ(rec.size(), record.length());
    EXPECT_EQ(rec.body().data(), record.buffer().subspan(4).data());
    EXPECT_EQ(rec.body().size(), 8);
    EXPECT_EQ(rec.space().space_id, 123);
  }
}

TEST(log0redo, test_parse_mtr_errors) {
  Redo_applier applier;

  /* Should return Incomplete on empty buffer. */
  {
    auto mtr = applier.parse_mtr({});
    EXPECT_TRUE(HasError(mtr, Parse_error::Incomplete));
  }

  /* Should return Incomplete on short page data record. */
  {
    auto record = Record_builder(MLOG_1BYTE, true);

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(HasError(mtr, Parse_error::Incomplete));
  }

  /* Should return Incomplete on short table metadata record. */
  {
    auto record = Record_builder(MLOG_TABLE_DYNAMIC_META, true);

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(HasError(mtr, Parse_error::Incomplete));
  }

  /* Should return Incomplete if no MLOG_MULTI_REC_END found. */
  {
    auto record = Record_builder(MLOG_1BYTE, false)
                      .u32c(123) /* space id */
                      .u32c(456) /* page num */
                      .u16(111)  /* offset */
                      .u32c(42); /* value */

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(HasError(mtr, Parse_error::Incomplete));
  }

  /* Should return Corrupted on single record inside multi record mtr. */
  {
    auto record = Record_builder(MLOG_1BYTE, false)
                      .u32c(123)                             /* space id */
                      .u32c(456)                             /* page num */
                      .u16(111)                              /* offset */
                      .u32c(42)                              /* value */
                      .u8(MLOG_1BYTE | MLOG_SINGLE_REC_FLAG) /* type */
                      .u32c(123)                             /* space id */
                      .u32c(456)                             /* page num */
                      .u16(111)                              /* offset */
                      .u32c(42);                             /* value */

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(HasError(mtr, Parse_error::Corrupted));
  }

  /* Should return Corrupted if record type == 0 or
  record type > MLOG_BIGGEST_TYPE. */
  for (int type : {0, MLOG_BIGGEST_TYPE + 1}) {
    auto record = Record_builder(type, true);

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(HasError(mtr, Parse_error::Corrupted));
  }

  /* Should return Corrupted on invalid persistent type. */
  for (int ptype : {PM_SMALLEST_TYPE, PM_BIGGEST_TYPE}) {
    auto record = Record_builder(MLOG_TABLE_DYNAMIC_META, true)
                      .u64mc(1)  /* table id */
                      .u64mc(2)  /* metadata version */
                      .u8(ptype) /* persistent type */
                      .u8(0);    /* dummy */
    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(HasError(mtr, Parse_error::Corrupted));
  }
}

TEST(log0redo, test_apply_mlog_xbytes) {
  os_event_global_init();
  sync_check_init(1);
  Redo_applier applier;

  /* Should apply MLOG_{1,2,4,8}BYTES records. */
  for (int type : {MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES, MLOG_8BYTES}) {
    auto record = Record_builder(type, true)
                      .u32c(1)   /* space id */
                      .u32c(2)   /* page num */
                      .u16(256); /* offset */
    if (type == MLOG_8BYTES) {
      record.u64c(42); /* value */
    } else {
      record.u32c(42); /* value */
    }

    auto page = Page_builder(FIL_PAGE_INDEX);

    auto mtr = applier.parse_mtr(record.buffer());
    EXPECT_TRUE(NoError(mtr));

    EXPECT_EQ(mtr->size(), record.length());
    EXPECT_EQ(mtr->records().size(), 1);

    ib::redo::Page_handle page_handle{
        .space_id = 1, .page_no = 2, .frame = page.buffer(), .zipped = {}};

    const ib::redo::Record_handle record_handle(type, mtr->records()[0].body());

    auto success = applier.apply(record_handle, page_handle);
    EXPECT_TRUE(success);

    switch (type) {
      case MLOG_1BYTE:
        EXPECT_EQ(mach_read_from_1(&page[256]), 42);
        break;
      case MLOG_2BYTES:
        EXPECT_EQ(mach_read_from_2(&page[256]), 42);
        break;
      case MLOG_4BYTES:
        EXPECT_EQ(mach_read_from_4(&page[256]), 42);
        break;
      case MLOG_8BYTES:
        EXPECT_EQ(mach_read_from_8(&page[256]), 42);
        break;
    }
  }
  sync_check_close();
  os_event_global_destroy();
}

TEST(log0redo, test_apply_mlog_rec_insert) {
  os_event_global_init();
  sync_check_init(1);

  Redo_applier applier;

  const std::string data = "X";
  constexpr auto rec_off = 100;
  constexpr auto next_rec_off = 200;

  const auto record = Record_builder(MLOG_REC_INSERT, true)
                          .u32c(1)                /* space id */
                          .u32c(2)                /* page num */
                          .u8(1)                  /* index log version */
                          .u8(0)                  /* flags */
                          .u16(rec_off)           /* rec offset */
                          .u32c(data.size() << 1) /* end seg len */
                          .string(data.data(), data.size()) /* data */
      ;

  auto page = Page_builder(FIL_PAGE_INDEX);

  page_header_set_field(page.data(), nullptr, PAGE_HEAP_TOP, 1000);
  mach_write_to_8(page.data() + PAGE_HEADER + PAGE_INDEX_ID, 1234);

  auto *rec = &page[rec_off];
  rec_set_n_fields_old(rec, 1);
  rec_set_next_offs_old(rec, next_rec_off);
  rec = &page[next_rec_off];
  rec_set_n_owned_old(rec, 1);

  const auto mtr = applier.parse_mtr(record.buffer());
  EXPECT_TRUE(NoError(mtr));

  EXPECT_EQ(mtr->size(), record.length());
  EXPECT_EQ(mtr->records().size(), 1);

  ib::redo::Page_handle page_handle{
      .space_id = 1, .page_no = 2, .frame = page.buffer(), .zipped = {}};

  const ib::redo::Record_handle record_handle(MLOG_REC_INSERT,
                                              mtr->records()[0].body());

  const auto success = applier.apply(record_handle, page_handle);
  EXPECT_TRUE(success);

  sync_check_close();
  os_event_global_destroy();
}

}  // namespace ib::redo::tests
