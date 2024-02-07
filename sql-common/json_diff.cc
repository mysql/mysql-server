/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "sql-common/json_diff.h"

#include <cassert>
#include <optional>
#include <utility>

#include "my_alloc.h"
#include "my_byteorder.h"
#include "my_dbug.h"  // assert
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql-common/json_binary.h"
#include "sql-common/json_dom.h"  // Json_dom, Json_wrapper
#include "sql-common/json_error_handler.h"
#include "sql-common/json_path.h"  // Json_path
#include "sql/current_thd.h"       // current_thd
#include "sql/log_event.h"         // net_field_length_checked
#include "sql/psi_memory_key.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql_string.h"  // StringBuffer
#include "template_utils.h"

using std::nullopt;
using std::optional;

// Define constructor and destructor here instead of in the header file, to
// avoid dependencies on psi_memory_key.h and json_dom.h from the header file.

Json_diff::Json_diff(const Json_seekable_path &path,
                     enum_json_diff_operation operation,
                     std::unique_ptr<Json_dom> value)
    : m_path(key_memory_JSON),
      m_operation(operation),
      m_value(std::move(value)) {
  for (const Json_path_leg *leg : path) m_path.append(*leg);
}

Json_diff::~Json_diff() = default;

Json_wrapper Json_diff::value() const {
  Json_wrapper result(m_value.get());
  result.set_alias();
  return result;
}

/**
  Return the total size of a data field, plus the size of the
  preceding integer that describes the length, when the integer is
  stored in net_field_length() format

  @param length The length of the data
  @return The length of the data plus the length of the length field.
*/
static size_t length_of_length_and_string(size_t length) {
  return length + net_length_size(length);
}

/**
  Encode a String as (length, data) pair, with length being stored in
  net_field_length() format.

  @param to Buffer where length and data will be stored.
  @param from Source string containing the data.
  @return true on out of memory, false on success.
*/
static bool write_length_and_string(String *to, const String &from) {
  // Serialize length.
  size_t length = from.length();
  DBUG_EXECUTE_IF("binlog_corrupt_write_length_and_string_bad_length", {
    DBUG_SET("-d,binlog_corrupt_write_length_and_string_bad_length");
    length = 1 << 30;
  });
  char length_buf[9];
  const size_t length_length =
      net_store_length((uchar *)length_buf, length) - (uchar *)length_buf;
  DBUG_PRINT("info", ("write_length_and_string: length=%lu length_length=%lu",
                      (unsigned long)length, (unsigned long)length_length));
  DBUG_EXECUTE_IF(
      "binlog_corrupt_write_length_and_string_truncate_before_string", {
        DBUG_SET(
            "-d,binlog_corrupt_write_length_and_string_truncate_before_string");
        return false;
      });
  DBUG_EXECUTE_IF("binlog_corrupt_write_length_and_string_bad_char", {
    DBUG_SET("-d,binlog_corrupt_write_length_and_string_bad_char");
    // Instead of "some text", write "\xffsome tex"
    // This is sure to corrupt both JSON paths and
    // binary JSON.
    return to->append(length_buf, length_length) ||
           to->append(static_cast<char>(0xff)) ||
           to->append(from.ptr(), from.length() - 1);
  });
  // Allocate memory and append
  return to->append(length_buf, length_length) || to->append(from);
}

size_t Json_diff::binary_length() const {
  DBUG_TRACE;

  // operation
  size_t ret = ENCODED_OPERATION_BYTES;

  /*
    It would be better to compute length without serializing the path
    and json.  And given that we serialize the path and json, it would
    be better if we dealt with out-of-memory errors in a better way.

    In the future, we should remove the need to pre-compute the size.
    Currently this is only needed by the binlog writer.  And it would
    be better to rewrite the binlog writer so that it streams rows
    directly to the thread caches instead of storing them in memory.

    And currently, this function is used from
    Row_data_memory::max_row_length, and the return value of that
    function is used in Row_data_memory::allocate_memory, which
    doesn't check out-of-memory conditions at all and might just
    dereference nullptr in case of out of memory.  So these asserts do
    not make the situation worse.
  */
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buf;

  // path
  if (m_path.to_string(&buf)) assert(0); /* purecov: deadcode */
  ret += length_of_length_and_string(buf.length());

  if (operation() != enum_json_diff_operation::REMOVE) {
    // value
    buf.length(0);
    const THD *const thd = current_thd;
    if (value().to_binary(JsonSerializationDefaultErrorHandler(thd), &buf))
      assert(0); /* purecov: deadcode */
    if (buf.length() > thd->variables.max_allowed_packet) {
      my_error(ER_WARN_ALLOWED_PACKET_OVERFLOWED, MYF(0),
               "json_binary::serialize", thd->variables.max_allowed_packet);
      assert(0);
    }
    ret += length_of_length_and_string(buf.length());
  }

  return ret;
}

bool Json_diff::write_binary(String *to) const {
  DBUG_TRACE;

  // Serialize operation
  char operation = (char)m_operation;
  DBUG_EXECUTE_IF("binlog_corrupt_json_diff_bad_op", {
    DBUG_SET("-d,binlog_corrupt_json_diff_bad_op");
    operation = 127;
  });
  if (to->append(&operation, ENCODED_OPERATION_BYTES))
    return true; /* purecov: inspected */  // OOM, error is reported
  DBUG_PRINT("info", ("wrote JSON operation=%d", (int)operation));

  /**
    @todo This first serializes in one buffer and then copies to
    another buffer.  It would be better if we could write directly to
    the output and save a round of memory allocation + copy. /Sven
  */

  // Serialize JSON path
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buf;
#ifndef NDEBUG
  bool return_early = false;
  DBUG_EXECUTE_IF("binlog_corrupt_json_diff_truncate_before_path_length", {
    DBUG_SET("-d,binlog_corrupt_json_diff_truncate_before_path_length");
    return false;
  });
  DBUG_EXECUTE_IF("binlog_corrupt_json_diff_bad_path_length", {
    DBUG_SET("-d,binlog_corrupt_json_diff_bad_path_length");
    DBUG_SET("+d,binlog_corrupt_write_length_and_string_bad_length");
  });
  DBUG_EXECUTE_IF("binlog_corrupt_json_diff_truncate_before_path", {
    DBUG_SET("-d,binlog_corrupt_json_diff_truncate_before_path");
    DBUG_SET(
        "+d,binlog_corrupt_write_length_and_string_truncate_before_string");
    return_early = true;
  });
  DBUG_EXECUTE_IF("binlog_corrupt_json_diff_bad_path_char", {
    DBUG_SET("-d,binlog_corrupt_json_diff_bad_path_char");
    DBUG_SET("+d,binlog_corrupt_write_length_and_string_bad_char");
  });
#endif  // ifndef NDEBUG
  if (m_path.to_string(&buf) || write_length_and_string(to, buf))
    return true; /* purecov: inspected */  // OOM, error is reported
#ifndef NDEBUG
  if (return_early) return false;
#endif
  DBUG_PRINT("info", ("wrote JSON path '%s' of length %lu", buf.ptr(),
                      (unsigned long)buf.length()));

  if (m_operation != enum_json_diff_operation::REMOVE) {
    // Serialize JSON value
    buf.length(0);
#ifndef NDEBUG
    DBUG_EXECUTE_IF("binlog_corrupt_json_diff_truncate_before_doc_length", {
      DBUG_SET("-d,binlog_corrupt_json_diff_truncate_before_doc_length");
      return false;
    });
    DBUG_EXECUTE_IF("binlog_corrupt_json_diff_bad_doc_length", {
      DBUG_SET("-d,binlog_corrupt_json_diff_bad_doc_length");
      DBUG_SET("+d,binlog_corrupt_write_length_and_string_bad_length");
    });
    DBUG_EXECUTE_IF("binlog_corrupt_json_diff_truncate_before_doc", {
      DBUG_SET("-d,binlog_corrupt_json_diff_truncate_before_doc");
      DBUG_SET(
          "+d,binlog_corrupt_write_length_and_string_truncate_before_string");
    });
    DBUG_EXECUTE_IF("binlog_corrupt_json_diff_bad_doc_char", {
      DBUG_SET("-d,binlog_corrupt_json_diff_bad_doc_char");
      DBUG_SET("+d,binlog_corrupt_write_length_and_string_bad_char");
    });
#endif  // ifndef NDEBUG
    const THD *const thd = current_thd;
    if (value().to_binary(JsonSerializationDefaultErrorHandler(thd), &buf) ||
        write_length_and_string(to, buf)) {
      return true; /* purecov: inspected */  // OOM, error is reported
    }
    if (buf.length() > thd->variables.max_allowed_packet) {
      my_error(ER_WARN_ALLOWED_PACKET_OVERFLOWED, MYF(0),
               "json_binary::serialize", thd->variables.max_allowed_packet);
      return true;
    }
    DBUG_PRINT("info",
               ("wrote JSON value of length %lu", (unsigned long)buf.length()));
  }

  return false;
}

Json_diff_vector::Json_diff_vector(allocator_type arg)
    : m_vector(std::vector<Json_diff, allocator_type>(arg)),
      m_binary_length(0) {}

static MEM_ROOT empty_json_diff_vector_mem_root(PSI_NOT_INSTRUMENTED, 256);
const Json_diff_vector Json_diff_vector::EMPTY_JSON_DIFF_VECTOR{
    Json_diff_vector::allocator_type{&empty_json_diff_vector_mem_root}};

void Json_diff_vector::add_diff(Json_diff diff) {
  m_vector.push_back(std::move(diff));
  m_binary_length += m_vector.back().binary_length();
}

void Json_diff_vector::add_diff(const Json_seekable_path &path,
                                enum_json_diff_operation operation,
                                std::unique_ptr<Json_dom> dom) {
  add_diff({path, operation, std::move(dom)});
}

void Json_diff_vector::add_diff(const Json_seekable_path &path,
                                enum_json_diff_operation operation) {
  add_diff({path, operation, /*value=*/nullptr});
}

void Json_diff_vector::clear() {
  m_vector.clear();
  m_binary_length = 0;
}

size_t Json_diff_vector::binary_length(bool include_metadata) const {
  return m_binary_length + (include_metadata ? ENCODED_LENGTH_BYTES : 0);
}

bool Json_diff_vector::write_binary(String *to) const {
  // Insert placeholder where we will store the length, once that is known.
  char length_buf[ENCODED_LENGTH_BYTES] = {0, 0, 0, 0};
  if (to->append(length_buf, ENCODED_LENGTH_BYTES))
    return true; /* purecov: inspected */  // OOM, error is reported

  // Store all the diffs.
  for (const Json_diff &diff : *this)
    if (diff.write_binary(to))
      return true; /* purecov: inspected */  // OOM, error is reported

  // Store the length.
  const size_t length = to->length() - ENCODED_LENGTH_BYTES;
  int4store(to->ptr(), (uint32)length);

  DBUG_PRINT("info", ("Wrote JSON diff vector length %lu=%02x %02x %02x %02x",
                      (unsigned long)length, length_buf[0], length_buf[1],
                      length_buf[2], length_buf[3]));

  return false;
}

bool Json_diff_vector::read_binary(const char **from, const TABLE *table,
                                   const char *field_name) {
  DBUG_TRACE;
  const uchar *p = pointer_cast<const uchar *>(*from);

  // Caller should have validated that the buffer is least 4 + length
  // bytes long.
  size_t length = uint4korr(p);
  p += 4;

  DBUG_PRINT("info", ("length=%zu p=%p", length, p));

  while (length > 0) {
    DBUG_PRINT(
        "info",
        ("length=%zu bytes remaining to decode into Json_diff_vector", length));

    optional<ReadJsonDiffResult> result = read_json_diff(p, length);
    if (!result.has_value()) {
      if (current_thd->is_error()) return true;
      my_error(ER_CORRUPTED_JSON_DIFF, MYF(0),
               static_cast<int>(table->s->table_name.length),
               table->s->table_name.str, field_name);
      return true;
    }

    auto &[diff, bytes_read] = result.value();
#ifndef NDEBUG
    if (Json_wrapper wrapper = diff.value(); !wrapper.empty()) {
      wrapper.dbug_print("", JsonDepthErrorHandler);
    }
#endif

    p += bytes_read;
    length -= bytes_read;
    add_diff(std::move(diff));
  }

  *from = pointer_cast<const char *>(p);
  return false;
}

optional<ReadJsonDiffResult> read_json_diff(const unsigned char *pos,
                                            size_t length) {
  const unsigned char *const start = pos;
  const unsigned char *const end = pos + length;

  // Read operation.
  if (pos >= end) return nullopt;
  const int operation_number = *pos;
  DBUG_PRINT("info", ("operation_number=%d", operation_number));
  if (operation_number >= JSON_DIFF_OPERATION_COUNT) return nullopt;
  const enum_json_diff_operation operation =
      static_cast<enum_json_diff_operation>(operation_number);
  ++pos;

  // Read path length.
  size_t path_length;
  if (size_t max_length = end - pos;
      net_field_length_checked<size_t>(&pos, &max_length, &path_length) ||
      path_length > max_length) {
    return nullopt;
  }

  // Read path.
  Json_path path{key_memory_JSON};
  DBUG_PRINT("info", ("path='%.*s'", static_cast<int>(path_length), pos));
  if (size_t bad_index; parse_path(path_length, pointer_cast<const char *>(pos),
                                   &path, &bad_index)) {
    return nullopt;
  }
  pos += path_length;

  if (operation == enum_json_diff_operation::REMOVE) {
    return ReadJsonDiffResult{{path, operation, /*value=*/nullptr},
                              static_cast<size_t>(pos - start)};
  }

  // Read value length.
  size_t value_length;
  if (size_t max_length = end - pos;
      net_field_length_checked<size_t>(&pos, &max_length, &value_length) ||
      value_length > max_length) {
    return std::nullopt;
  }

  // Read value.
  json_binary::Value value =
      json_binary::parse_binary(pointer_cast<const char *>(pos), value_length);
  if (value.type() == json_binary::Value::ERROR) return nullopt;
  std::unique_ptr<Json_dom> dom = Json_dom::parse(value);
  if (dom == nullptr) return nullopt;
  pos += value_length;

  return ReadJsonDiffResult{{path, operation, std::move(dom)},
                            static_cast<size_t>(pos - start)};
}

/**
  Find the value at the specified path in a JSON DOM. The path should
  not contain any wildcard or ellipsis, only simple array cells or
  member names. Auto-wrapping is not performed.

  @param dom        the root of the DOM
  @param first_leg  the first path leg
  @param last_leg   the last path leg (exclusive)
  @return the JSON DOM at the given path, or `nullptr` if the path is not found
 */
static Json_dom *seek_exact_path(Json_dom *dom,
                                 const Json_path_iterator &first_leg,
                                 const Json_path_iterator &last_leg) {
  for (auto it = first_leg; it != last_leg; ++it) {
    const Json_path_leg *leg = *it;
    const auto leg_type = leg->get_type();
    assert(leg_type == jpl_member || leg_type == jpl_array_cell);
    switch (dom->json_type()) {
      case enum_json_type::J_ARRAY: {
        const auto array = down_cast<Json_array *>(dom);
        if (leg_type != jpl_array_cell) return nullptr;
        Json_array_index idx = leg->first_array_index(array->size());
        if (!idx.within_bounds()) return nullptr;
        dom = (*array)[idx.position()];
        continue;
      }
      case enum_json_type::J_OBJECT: {
        const auto object = down_cast<Json_object *>(dom);
        if (leg_type != jpl_member) return nullptr;
        dom = object->get(leg->get_member_name());
        if (dom == nullptr) return nullptr;
        continue;
      }
      default:
        return nullptr;
    }
  }

  return dom;
}

enum_json_diff_status apply_json_diff(const Json_diff &diff, Json_dom *dom) {
  Json_wrapper val_to_apply = diff.value();
  const Json_path &path = diff.path();

  switch (diff.operation()) {
    case enum_json_diff_operation::REPLACE: {
      assert(path.leg_count() > 0);
      Json_dom *old = seek_exact_path(dom, path.begin(), path.end());
      if (old == nullptr) return enum_json_diff_status::REJECTED;
      assert(old->parent() != nullptr);
      old->parent()->replace_dom_in_container(old, val_to_apply.clone_dom());
      return enum_json_diff_status::SUCCESS;
    }
    case enum_json_diff_operation::INSERT: {
      assert(path.leg_count() > 0);
      Json_dom *parent = seek_exact_path(dom, path.begin(), path.end() - 1);
      if (parent == nullptr) return enum_json_diff_status::REJECTED;
      const Json_path_leg *last_leg = path.last_leg();
      if (parent->json_type() == enum_json_type::J_OBJECT &&
          last_leg->get_type() == jpl_member) {
        auto obj = down_cast<Json_object *>(parent);
        if (obj->get(last_leg->get_member_name()) != nullptr)
          return enum_json_diff_status::REJECTED;
        if (obj->add_alias(last_leg->get_member_name(),
                           val_to_apply.clone_dom()))
          return enum_json_diff_status::ERROR; /* purecov: inspected */
        return enum_json_diff_status::SUCCESS;
      }
      if (parent->json_type() == enum_json_type::J_ARRAY &&
          last_leg->get_type() == jpl_array_cell) {
        auto array = down_cast<Json_array *>(parent);
        Json_array_index idx = last_leg->first_array_index(array->size());
        if (array->insert_alias(idx.position(), val_to_apply.clone_dom()))
          return enum_json_diff_status::ERROR; /* purecov: inspected */
        return enum_json_diff_status::SUCCESS;
      }
      return enum_json_diff_status::REJECTED;
    }
    case enum_json_diff_operation::REMOVE: {
      assert(path.leg_count() > 0);
      Json_dom *parent = seek_exact_path(dom, path.begin(), path.end() - 1);
      if (parent == nullptr) return enum_json_diff_status::REJECTED;
      const Json_path_leg *last_leg = path.last_leg();
      if (parent->json_type() == enum_json_type::J_OBJECT) {
        auto object = down_cast<Json_object *>(parent);
        if (last_leg->get_type() != jpl_member ||
            !object->remove(last_leg->get_member_name()))
          return enum_json_diff_status::REJECTED;
      } else if (parent->json_type() == enum_json_type::J_ARRAY) {
        if (last_leg->get_type() != jpl_array_cell)
          return enum_json_diff_status::REJECTED;
        auto array = down_cast<Json_array *>(parent);
        Json_array_index idx = last_leg->first_array_index(array->size());
        if (!idx.within_bounds() || !array->remove(idx.position()))
          return enum_json_diff_status::REJECTED;
      } else {
        return enum_json_diff_status::REJECTED;
      }
      return enum_json_diff_status::SUCCESS;
    }
  }

  /* purecov: begin deadcode */
  assert(false);
  return enum_json_diff_status::ERROR;
  /* purecov: end */
}
