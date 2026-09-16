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

/** @file include/log0redo.h
 Redo log parsing and applying.

 ****************************************************************************/

#ifndef log0redo_h
#define log0redo_h

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "ut0expected.h"

/* Required inside Page_handle_wrapper */
struct buf_block_t;

namespace ib::redo {

/** This class represents a parsed redo record.
@note The body field refers to the original parsing buffer,
      so a caller must ensure that the buffer's lifetime is
      not less than the parsed record's lifetime. */
class Record_view {
 public:
  /** Possible record kinds. */
  enum class Kind {
    /** Auxiliary record. */
    Aux,

    /** Page data record. */
    Page,

    /** Table metadata record. */
    Table,

    /** Table space record. */
    Space,
  };

  /** Auxiliary record tag. */
  struct Aux_tag {
    static constexpr Kind kind = Kind::Aux;
  };

  /** Page data record tag. */
  struct Page_tag {
    static constexpr Kind kind = Kind::Page;

    /** Tablespace id. */
    uint32_t space_id;

    /** Page number. */
    uint32_t page_no;
  };

  /** Table metadata tag. */
  struct Table_tag {
    static constexpr Kind kind = Kind::Table;

    /** Table id. */
    uint64_t table_id;

    /** Metadata version. */
    uint64_t version;
  };

  /** Table space tag. */
  struct Space_tag {
    static constexpr Kind kind = Kind::Space;

    /** Space id. */
    uint32_t space_id;
  };

  template <typename Tag>
  constexpr Record_view(int type, size_t size, std::span<const uint8_t> body,
                        Tag &&tag)
      : m_kind(Tag::kind),
        m_type(type),
        m_size(size),
        m_body(body),
        m_tag(std::forward<Tag>(tag)) {}

  /** Returns record kind. */
  [[nodiscard]] constexpr Kind kind() const noexcept { return m_kind; }

  /** Returns record type with MLOG_SINGLE_REC_FLAG cleared. */
  [[nodiscard]] constexpr int type() const noexcept { return m_type; }

  /** Returns record size in bytes occupied in the parsing buffer. */
  [[nodiscard]] constexpr size_t size() const noexcept { return m_size; }

  /** Returns reference to record body. */
  [[nodiscard]] constexpr std::span<const uint8_t> body() const noexcept {
    return m_body;
  }

  /** Returns page data tag. */
  [[nodiscard]] constexpr const Page_tag &page() const noexcept {
    ut_a(m_kind == Kind::Page);
    return m_tag.page;
  }

  /** Returns table metadata tag. */
  [[nodiscard]] constexpr const Table_tag &table() const noexcept {
    ut_a(m_kind == Kind::Table);
    return m_tag.table;
  }

  /** Returns table space tag. */
  [[nodiscard]] constexpr const Space_tag &space() const noexcept {
    ut_a(m_kind == Kind::Space);
    return m_tag.space;
  }

 private:
  Kind m_kind;

  /** Record type with MLOG_SINGLE_REC_FLAG cleared. */
  int m_type;

  /** Full record size in bytes occupied in the parsing buffer. */
  size_t m_size;

  /** Record data excluding what consumed by m_type and m_tag. */
  std::span<const uint8_t> m_body;

  union Tag {
    Aux_tag aux;
    Page_tag page;
    Table_tag table;
    Space_tag space;

    constexpr Tag(Aux_tag &&tag) : aux(std::forward<Aux_tag>(tag)) {}
    constexpr Tag(Page_tag &&tag) : page(std::forward<Page_tag>(tag)) {}
    constexpr Tag(Table_tag &&tag) : table(std::forward<Table_tag>(tag)) {}
    constexpr Tag(Space_tag &&tag) : space(std::forward<Space_tag>(tag)) {}
  } m_tag;
};

using Record_list = std::vector<Record_view>;

/** This class represents a list of parsed redo records that form an mtr. */
class Mtr_view {
 public:
  /** Returns mtr size in bytes occupied in the parsing buffer. */
  [[nodiscard]] constexpr size_t size() const noexcept { return m_size; }

  /** Returns true if mtr contains no records. */
  [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

  /** Returns record list. */
  [[nodiscard]] constexpr const Record_list &records() const noexcept {
    return m_records;
  }

  /** Add record to the list. */
  template <typename T>
  void add_record(T &&record) {
    m_size += record.size();
    m_records.emplace_back(std::forward<T>(record));
  }

 private:
  /** Full mtr size in bytes occupied in the parsing buffer. */
  size_t m_size = 0;

  Record_list m_records;
};

/** Parse error description. */
class Parse_error {
 public:
  /** Buffer is too short to parse. */
  static constexpr int Incomplete = 1;

  /** Buffer contains corrupted data. */
  static constexpr int Corrupted = 2;

  /** Returns position in parsing buffer where the error occurred.
  The position is aligned to the record boundary. */
  [[nodiscard]] constexpr size_t pos() const noexcept { return m_pos; }

  /** Add position to error description.
  @param pos error position
  @return Reference to self. */
  [[nodiscard]] auto &with_pos(size_t pos) {
    m_pos = pos;
    return *this;
  }

  constexpr Parse_error(int code) noexcept : m_code(code) {}

  constexpr operator int() const noexcept { return m_code; }

 private:
  int m_code;
  size_t m_pos = 0;
};

template <typename T>
using Parse_result = ut::Expected<T, Parse_error>;

/** The handle that represents the page where redo log records are applied.
It includes the compressed and uncompressed page buffers which are named
as frame as per the convention used in InnoDB. If handle represents a
compressed page, then it must maintain metadata which may have updated
during applying the log records.

@note 'const' page buffers in the handle mean that you can't modify direct
fields of them, so you can't change .data nor .size, i.e. can't re-point them
to anything else, or decide to change their size. OTOH, you can change values
of bytes within these buffers. */
struct Page_handle final {
  /** Tablespace id. */
  const uint32_t space_id;

  /** Page number. */
  const uint32_t page_no;

  /** Page buffer */
  const std::span<uint8_t> frame;

  /** Compressed page buffer and metadata about the compressed page */
  struct Zipped {
    /** Compressed page buffer */
    const std::span<uint8_t> frame;
    struct Metadata {
      /** Start offset of the modification log that is right after the
      compressed image in the page */
      uint16_t start_offset;
      /** End offset of the modification log */
      uint16_t end_offset;
      /** Number of externally stored columns on the page; the maximum is 744
      on a 16 KiB page */
      uint16_t n_blobs;
    } metadata;
  };

  /** An optional compressed page buffer that needs to be initialized iff this
  handle represents a compressed page */
  std::optional<Zipped> zipped;
};

/** A convenience wrapper over the Page_handle (where redo log records are
applied). It initializes the page handle from the buffer block and keep them
sane. User must update the block after the redo log records are applied
on the page handle. */
class Page_handle_wrapper {
 public:
  explicit Page_handle_wrapper(buf_block_t &block);

  // Move constructor
  Page_handle_wrapper(Page_handle_wrapper &&other) noexcept
      : m_block(other.m_block),
        m_page_handle(std::move(other.m_page_handle))
#ifdef UNIV_DEBUG
        ,
        m_need_to_update_block(other.m_need_to_update_block)
#endif
  {
#ifdef UNIV_DEBUG
    // no need to update the temporary object
    other.m_need_to_update_block = false;
#endif
  }

  // Explicitly delete copy operations
  Page_handle_wrapper(const Page_handle_wrapper &) = delete;

#ifdef UNIV_DEBUG
  ~Page_handle_wrapper() { ut_ad(!m_need_to_update_block); }
#endif

  [[nodiscard]] constexpr buf_block_t &block() noexcept { return m_block; }

  [[nodiscard]] constexpr Page_handle &handle() noexcept {
    return m_page_handle;
  }

  /** If it is compressed page then update the compressed page metadata in the
  block. Metadata may have changed during applying the log record */
  void update_block();

 private:
  buf_block_t &m_block;
  Page_handle m_page_handle;
#ifdef UNIV_DEBUG
  bool m_need_to_update_block{true};
#endif /* UNIV_DEBUG */
};

/* Page_handle is part of an interface and should be kept simple */
static_assert(std::is_standard_layout_v<Page_handle>);

/** The handle that represents a log record. It contains record type and the
record body. */
struct Record_handle {
  /** Record type */
  uint8_t type;

  /** Record body */
  std::span<const uint8_t> body;
};

/* Record_handle is part of an interface and should be kept simple */
static_assert(std::is_standard_layout_v<Record_handle>);

/** This class allows to parse and apply redo records. */
class Redo_applier final {
 public:
  Redo_applier();

  ~Redo_applier();

  /** Parse an mtr from a buffer.
  @param buffer buffer to parse
  @return Parsed mtr or error. */
  [[nodiscard]] Parse_result<Mtr_view> parse_mtr(
      std::span<const uint8_t> buffer);

  /** Apply a parsed redo record to a page.
  @param record_handle record to apply
  @param page_handle  Page handle to apply the record to
  @return True on success, false on error. In case of error page may
          contain garbage. */
  [[nodiscard]] bool apply(const Record_handle &record_handle,
                           Page_handle &page_handle);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ib::redo

#endif /* !log0redo_h */
