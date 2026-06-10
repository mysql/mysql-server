/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "sql/json_duality_view/dml.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include "scope_guard.h"
#include "sql/sql_error.h"

#include <concepts>
#include <ranges>
#include <type_traits>

#include "base64.h"
#include "field_types.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "sql-common/json_dom.h"
#include "sql/binlog.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"
#include "sql/item_json_func.h"  //  get_json_wrapper()
#include "sql/json_duality_view/content_tree.h"
#include "sql/json_duality_view/ostream_utils.h"
#include "sql/json_duality_view/utils.h"
#include "sql/parse_tree_nodes.h"
#include "sql/sql_class.h"
#include "sql/sql_data_change.h"
#include "sql/sql_insert.h"  // Sql_cmd_insert_base
#include "sql/sql_lex.h"
#include "sql/statement/statement.h"
#include "sql/table.h"
#include "sql_string.h"

namespace jdv {

// We provide explicit overloads for those builtin types that we actually use,
// rather than a single generic function which accepts all types. This allows us
// to detect incorrectly passing pointer instead of ref/value.
static const char *em_wrap(const char *arg) { return arg; }
static int em_wrap(int arg) { return arg; }

/**
  Overload which wraps string_views in strings which are
  guaranteed to be null-terminated.

  @param sv string view to wrap.

  @return string containing copy of argument
*/
static std::string em_wrap(std::string_view sv) {
  return {sv.data(), sv.size()};
}

/**
  Overload which wraps Json_paths in strings, after first
  creating string representation in String buffer. This allows
  passing Json_paths directly to error message function as
  const char *without having to worry about ownership.

  @param jp Json_path to wrap
  @return std:string containing the string representation of the path
*/
static std::string em_wrap(const Json_path &jp) {
  String buf;
  jp.to_string(&buf);
  return {buf.ptr(), buf.length()};
}

/**
  Identity unwrapper.

  @param arg argument to return
  @return argument passed in
*/
static decltype(auto) em_unwrap(auto arg) { return arg; }

/**
  Overload which unwraps strings by calling c_str()-

  @param str string to call c_str() on
  @return c-string from string
*/
static const char *em_unwrap(const std::string &str) { return str.c_str(); }

/**
  Wraps calls to my_error() unwrapping wrapped arguments.

  @tparam CODE error code to report
  @param args_to_wrap arguments for format specifiers in error message
 */
template <int CODE, typename... Args>
static void my_jdv_error(Args... args_to_wrap) {
  [](const auto &...wrapped_args) {
    my_error(CODE, MYF(0), em_unwrap(wrapped_args)...);
  }(em_wrap(std::forward<Args>(args_to_wrap))...);
}

static constexpr std::string_view metadatakey = "_metadata";
static constexpr std::string_view etagkey = "etag";

/** Comparator which orders in the same way as Json_object orders its keys. */
struct Size_first_comparator {
  template <typename RA, typename RB>
    requires std::ranges::contiguous_range<RA> &&
             std::ranges::contiguous_range<RB> &&
             std::is_same_v<std::ranges::range_value_t<RA>,
                            std::ranges::range_value_t<RB>>
  constexpr bool operator()(RA &&ra, RB &&rb) const {
    auto sra = std::ranges::size(ra);
    auto srb = std::ranges::size(rb);
    return sra != srb
               ? sra < srb
               : std::memcmp(std::ranges::data(std::forward<RA>(ra)),
                             std::ranges::data(std::forward<RB>(rb)),
                             sra * sizeof(std::ranges::range_value_t<RA>)) < 0;
  }
};
constexpr Size_first_comparator size_less;

template <typename JSON_TYPE>
struct Json_type_traits;

template <>
struct Json_type_traits<Json_object> {
  static constexpr enum_json_type jt = enum_json_type::J_OBJECT;
  static constexpr std::string_view name = "Json_object";
};

template <>
struct Json_type_traits<Json_array> {
  static constexpr enum_json_type jt = enum_json_type::J_ARRAY;
  static constexpr std::string_view name = "Json_array";
};

/**
  Inspects an assumed valid Json_dom (typically an exising value in UPDATE).
  Returns nullptr if the argument is nullptr or has type J_NULL. Otherwise
  returns the dom cast JT*. Asserts if the type is not JT.

  @tparam JT the expected actual type of the argument
  @param valid_dom assumed valid Json_dom
  @return the dom cast to the expectd type or nullptr
 */
template <typename JT>
static JT *inspect_valid_dom(Json_dom *valid_dom) {
  if (valid_dom == nullptr) {
    return nullptr;
  }

  enum_json_type vjt = valid_dom->json_type();
  if (vjt == enum_json_type::J_NULL) {
    return nullptr;
  }
  assert(vjt == Json_type_traits<JT>::jt);

  return static_cast<JT *>(valid_dom);
}

/**
  Inspects a Json_dom which may or may not have the expected type and may also
  be nullptr, (typically a value provided as user input). Returns
  {J_NULL,nullptr} if the argument is nullptr or has type J_NULL. Returns
  {J_ERROR, nullptr} if the argument does not have type JT. Otherwise returns
  {enum_json_type of JT, argument cast to JT*}.

  @tparam JT the expected actual type of the argument
  @param dom dom that may not have the expected type
  @return pair of actual json type enum value and the argument cast to JT* or
  nullptr
 */
template <typename JT>
static std::pair<enum_json_type, JT *> inspect_dom(Json_dom *dom) {
  if (dom == nullptr) {
    return {enum_json_type::J_NULL, nullptr};
  }
  enum_json_type jt = dom->json_type();
  if (jt == enum_json_type::J_NULL) {
    return {jt, nullptr};
  }
  if (jt != Json_type_traits<JT>::jt) {
    return {enum_json_type::J_ERROR, nullptr};
  }
  return {jt, static_cast<JT *>(dom)};
}

/**
  Formats a Json_wrapper as string for debugging.

  @param jw Json_wrapper to format
  @return string containing formatted representation
 */
static std::string json_wrapper_to_string(const Json_wrapper &jw) {
  String pretty;
  if (jw.to_pretty_string(&pretty, "json_wrapper_to_string",
                          JsonDepthErrorHandler)) {
    return "<Failed to format Json_value as pretty string>";
  }
  return to_string(pretty);
}

/**
  Formats a Json_dom as string for debugging.

  @param jdom Json_dom to format
  @return string containing formatted representation
 */
static std::string json_dom_to_string(Json_dom *jdom) {
  if (jdom == nullptr) {
    return "Json_dom{nullptr}";
  }
  return json_wrapper_to_string(Json_wrapper{jdom, true});
}

/**
  Predicate to determine if a dom does not represent a valid value i.e.
  that either the pointer itself is nullptr, or that the dom is a Json_null
  (has type enum_json_type::J_NULL).

  @param jdom dom to check
  @return true if represents a void value
 */
static bool is_nil_dom(Json_dom *jdom) {
  return jdom == nullptr || jdom->json_type() == enum_json_type::J_NULL;
}

/**
 Convenience wrapper predicate which returns true for AUTO_INCREMENT columns.

 @param fld column to report for.

 @return true if column is AUTO_INCREMENT
*/
static bool is_auto_increment(const Field &fld) {
  return (fld.auto_flags & Field::NEXT_NUMBER) != 0;
}

/**
  Compares Json_dom pointers which may be nullptr by considering nullptrs to be
  greater than all non-nullptr values. This convention is chosen so that
  sorting with {compare_doms(a,b) < 0} places nullptr entries last (the
  assumption being that searches into the sorted range most often are for
  a non-nullptr elements).

  @param ajd lhs
  @param bjd rhs

  @return -1,0,1 for less, equal, greater.
*/
static int compare_doms(Json_dom *ajd, Json_dom *bjd) {
  if (ajd != nullptr && bjd != nullptr) {
    return Json_wrapper{ajd, true}.compare(Json_wrapper{bjd, true});
  }
  if (ajd == nullptr && bjd == nullptr) {
    return 0;
  }

  return bjd == nullptr ? -1 : 1;
}

/**
  Predicate to determine if json dom is expected to be base64 encoded based on
  SQL type and charset-

  @param ft actual field type
  @param csi character set object

  @return true if json dom is expected to be base64 encooded
*/
[[nodiscard]] static bool expect_b64_dom(enum_field_types ft,
                                         const CHARSET_INFO &csi) {
  bool bin_csi = (&csi == &my_charset_bin);
  switch (ft) {
    case MYSQL_TYPE_BIT:
      return true;

    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
      DBUG_LOG("jdv_dml",
               " ft:" << ft << " bin_cs:" << bin_csi << " returns " << bin_csi);
      return bin_csi;

    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_STRING: {
      DBUG_LOG("jdv_dml",
               " ft:" << ft << " bin_csi:" << bin_csi
                      << " ci.mbminlen:" << csi.mbminlen
                      << " returns: " << (bin_csi || csi.mbminlen > 1));
      return (bin_csi || csi.mbminlen > 1);
    }

    default:
      return false;
  }
}

/**
  Predicate to determine if base column expects json values to be base64
  encoded.

  @param kci base table column

  @return true if column expects json values to be base64 encooded
*/
[[nodiscard]] static bool col_expects_b64(const Key_column_info &kci) {
  return expect_b64_dom(kci.field()->type(), *kci.field()->charset());
}

static constexpr std::string_view TYPE_HEADER_PREFIX = "base64:type";
static constexpr std::size_t TYPE_HEADER_MAXSZ =
    TYPE_HEADER_PREFIX.size() + 4;  // 3 digits and :

static constexpr auto ERROR_INDICATOR = static_cast<std::size_t>(-1);

/**
  Converts base64 string to its binary representation, escapes this to be
  a string literal, wraps in single quotes, prepends character
  set introducer _binary before appending to statement.
  Checks to see if input is valid base64.

  @param sbufp statement buffer
  @param buf string representation of dom
  @param jd dom (for error reporting)

  @return true in case of errors
*/
[[nodiscard]] static bool append_b64_dom(std::string *sbufp, const String &buf,
                                         Json_dom *jd) {
  // Assume to_string() value inside buf is base64-encoded with metadata type
  // header (as if returned by select).
  const auto type_header =
      std::string_view(buf.ptr(), std::min(TYPE_HEADER_MAXSZ, buf.length()));

  if (!type_header.starts_with(TYPE_HEADER_PREFIX)) {
    my_jdv_error<ER_JDV_INVALID_BINARY_TYPE_HEADER>(jd->get_location());
    return true;
  }

  auto type_header_last = type_header.find(':', TYPE_HEADER_PREFIX.size());
  if (type_header_last == std::string_view::npos) {
    my_jdv_error<ER_JDV_INVALID_BINARY_TYPE_HEADER>(jd->get_location());
    return true;
  }
  assert(type_header_last <= TYPE_HEADER_PREFIX.size() + 3);
  const auto bv = std::string_view(buf.ptr(), buf.length());
  const auto b64_payload =
      std::string_view(bv.begin() + type_header_last + 1, bv.end());

  auto decode_needed_size = base64_needed_decoded_length(b64_payload.size());
  char *decode_buf =
      current_thd->mem_root->ArrayAlloc<char>(decode_needed_size);
  if (decode_buf == nullptr) {
    return true;
  }
  auto decode_resulting_size = base64_decode(
      b64_payload.data(), b64_payload.size(), decode_buf, nullptr, 0);

  if (decode_resulting_size == -1) {
    my_jdv_error<ER_BASE64_DECODE_ERROR>();
    return true;
  }

  sbufp->append("_binary '");

  std::size_t sbuf_existing_size = sbufp->size();
  // *2 is worst-case when every byte must be escaped
  auto escape_needed_size = decode_resulting_size * 2;

  sbufp->resize(sbufp->size() + escape_needed_size);
  char *escape_dst = sbufp->data() + sbuf_existing_size;
  auto escape_resulting_size =
      escape_string_for_mysql(&my_charset_bin, escape_dst, escape_needed_size,
                              decode_buf, decode_resulting_size);

  if (escape_resulting_size == ERROR_INDICATOR) {
    assert(false);  // Only fails if buffer is too small - not expected.
    my_jdv_error<ER_BASE64_DECODE_ERROR>();
    return true;
  }
  sbufp->resize(sbuf_existing_size + escape_resulting_size);
  sbufp->append(1, '\'');
  DBUG_LOG("jdv_dml",
           "escape_resulting_size:" << escape_resulting_size
                                    << " b64 decoded string literal: "
                                    << sbufp->substr(sbuf_existing_size));
  return false;
}

/**
  Append a Json_dom to a string. If the Json_dom pointer is nullptr 'DEFAULT' is
  appended. If the json type is J_NULL, 'NULL' is appended. For numeric values
  the bare to_string value is appended. For other non-base64 types the
  to_string value is quote escaped and wrapped in single quotes with the
  character set introducer _utf8mb4.

  If the base table column returns values as base64 the raw string
  representation of the dom is expected to be a type header followed by a
  base64 encoded payload. This is handled by append_b64_dom().

  @param sbufp statement buffer
  @param jd dom to convert to string and append
  @param b64 true if resulting string is expected to be base64 encoded
  @return true if error
 */
[[nodiscard]] static bool append_json_dom(std::string *sbufp, Json_dom *jd,
                                          bool b64 = false) {
  assert(sbufp != nullptr);
  std::string &sbuf = *sbufp;
  if (jd == nullptr) {
    sbuf.append("DEFAULT");
    return false;
  }

  Json_wrapper jw{jd, true};
  String buf;
  jw.to_string(&buf, false, "", []() {});
  DBUG_LOG("jdv_dml",
           "jw(jd).to_string():'" << buf.c_ptr_safe() << "', b64:" << b64);

  switch (jd->json_type()) {
    case enum_json_type::J_DECIMAL:
    case enum_json_type::J_INT:
    case enum_json_type::J_UINT:
    case enum_json_type::J_DOUBLE:
    case enum_json_type::J_BOOLEAN:
      sbuf.append(buf.ptr(), buf.length());
      break;
    case enum_json_type::J_NULL:
      sbuf.append("NULL");
      break;

    case enum_json_type::J_STRING:
    case enum_json_type::J_OPAQUE:
      if (b64) {
        return append_b64_dom(&sbuf, buf, jd);
      }
      [[fallthrough]];
    case enum_json_type::J_DATE:
    case enum_json_type::J_TIME:
    case enum_json_type::J_DATETIME:
    case enum_json_type::J_TIMESTAMP:
    case enum_json_type::J_OBJECT:
    case enum_json_type::J_ARRAY: {
      sbuf.append("_utf8mb4 '");
      auto cur_size = sbuf.size();
      std::size_t bytes_generated = ERROR_INDICATOR;
      for (sbuf.resize(cur_size + 1);
           (bytes_generated = escape_string_for_mysql(
                &my_charset_utf8mb4_bin, &sbuf.front() + cur_size,
                sbuf.size() - cur_size, buf.ptr(), buf.length())) ==
           ERROR_INDICATOR;
           sbuf.resize((sbuf.size() * 2) - cur_size)) {
      }
      sbuf.resize(cur_size + bytes_generated);
      sbuf.append(1, '\'');
      break;
    }

    // These are not expected
    case enum_json_type::J_ERROR:
    default:
      assert(false);
      my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>(jd->get_location(), "Unknown");
      return true;
      break;
  }
  return false;
}

/**
  Sets up the environment for using Regular_statement_handle
  for JDVs. The creation and usage of
  the Regular_statement_handle is done in the callable F passed
  in which returns true to indicate errors. The caller DA is passed as an
  argument to F. The callable f is responsible for copying
  information back to caller_da if/when needed.

  @param thd THD
  @param f callable to invoke

  @return value of f
*/
[[nodiscard]] static bool do_in_substatement_context(THD *thd, auto &&f) {
  bool ret_val = false;

  Diagnostics_area *caller_da = thd->get_stmt_da();
  Diagnostics_area da(false);
  thd->push_diagnostics_area(&da);

  // Stores state before sub-statement execution.
  Open_tables_backup open_tables_state_backup;
  thd->reset_n_backup_open_tables_state(&open_tables_state_backup, 0);

  // Backup current item list.
  Item *saved_item_list = thd->item_list();
  thd->reset_item_list();

  /*
     This is needed to indicate statements are sub-statements. Statements access
     tables and acquire locks for which connection already has MDL locks. So
     executing statement should be safe with this. Also statements are executed
     under main transaction and committed/rolled back as part of main
     transaction only (which is not possible with rw_transaction).
  */
  Sub_statement_state statement_state;
  thd->reset_sub_statement_state(&statement_state, SUB_STMT_DUALITY_VIEW);

  ret_val = f(caller_da);

  thd->restore_sub_statement_state(&statement_state);

  // Restore item list state.
  thd->free_items();
  thd->set_item_list(saved_item_list);

  if (ret_val) {
    caller_da->set_error_status(thd->get_stmt_da()->mysql_errno(),
                                thd->get_stmt_da()->message_text(),
                                thd->get_stmt_da()->returned_sqlstate());
  }
  thd->pop_diagnostics_area();

  /*
     Restore table open state only.
     We can not invoke "restore_backup_open_tables_state" here.It will
     release even MDL locks. MDL locks acquired by INSERT on v1 and base
     tables is same. Releasing MDL locks here will release lock acquired
     on base tables too. So only table state is restored here.
  */
  thd->set_open_tables_state(&open_tables_state_backup);
  return ret_val;
}

/**
  Convenience function for obtaining the member of a Json_object.
  Returns nullptr if the Json_object argument is nullptr or does not
  have the requested key.
 */
static Json_dom *get_val(const Json_object *jo, std::string_view k) {
  return jo == nullptr ? nullptr : jo->get(k);
}

/** Reusable object to simplify comparisons against uint 0 (explicit
    generation request) */
static Json_uint ZERO{0};

/** Reusable Json_wrapper around ZERO object to simplify comparisons
    against uint 0 (explicit generation request) */
static const Json_wrapper ZEROW{&ZERO, true};

/** Convenience function for comparing a Json document to unsigned 0.
  @param doc Json document to test

  @return true if doc compares equal to unsigned 0
*/
static bool is_zero(Json_dom *doc) {
  return Json_wrapper{doc, true}.compare(ZEROW) == 0;
}

/** Convenience function for checking if the connection supports using 0
    to request explicit generation of AUTO_INCREMENT values.
    @param thd THD

    @return true if supported
*/
static bool auto_generate_on_zero(THD *thd) {
  return (thd->variables.sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO) == 0;
}

/** Convenience function which checks if a Json document value inserted in
    a field will result in a generated AUTO_INCREMENT value.

    @param thd THD
    @param fld Field object describing column
    @param doc Json_dom value to insert

    @return true if a generated value will be inserted
*/
static bool field_will_be_auto_generated(THD *thd, const Field &fld,
                                         Json_dom *doc) {
  return is_auto_increment(fld) &&
         (is_nil_dom(doc) || (auto_generate_on_zero(thd) && is_zero(doc)));
}

/**
 Represents columns in the base table which will be populated based on the Json
 document passed in.
*/
struct Resolve_column {
  const Key_column_info *kci = nullptr;
  Json_dom *value = nullptr;
  bool deferred_resolve = false;
};

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const Resolve_column &rc) {
  os << "{" << *rc.kci << ", " << path_str(rc.value) << ":"
     << json_dom_to_string(rc.value)
     << ", dr: " << (rc.deferred_resolve ? "yes}" : "no}");
  return os;
}

using Resolve_column_vec = std::vector<Resolve_column>;

/**
  Holds the resolved values for a row of a base table. Index in vector
  corresponds to index of columns in content tree node.
 */
struct Resolve_row {
  Resolve_row *parent = nullptr;
  Json_uint autoinc_value = Json_uint{0ULL};
  Resolve_column *unresolved_autoinc_column = nullptr;
  Resolve_column_vec columns;
};

[[maybe_unused]] static std::ostream &operator<<(std::ostream &os,
                                                 const Resolve_row &rr) {
  os << "Resolve_row{autoincval:" << rr.autoinc_value.value()
     << ", unresolved aicol:";
  if (rr.unresolved_autoinc_column == nullptr) {
    os << "nullptr";
  } else {
    os << *rr.unresolved_autoinc_column;
  }
  os << "}[" << rr.columns.size() << "]";
  return os;
}

template <typename BIN>
static int compare_bindings(const BIN &a, const BIN &b);

/**
  Binds a single Json_object and a jdv::Content_tree_node.
  Single_object_binding is used for INSERT and DELETE operations
  on JSON duality view.
 */
struct Single_object_binding {
  Json_object *bound_object = nullptr;
  const Content_tree_node *ct_node = nullptr;
  std::unique_ptr<Resolve_row> resolve_row;

  int operator<=>(const Single_object_binding &other) const {
    return compare_bindings(*this, other);
  }

  // Equality is only determined by the Content_tree pointer and the primary key
  // value, but compare_bindings() still defined an order for objects which are
  // equal according to this definition. So operator<=>(that) == 0 implies
  // equality, but the converse is not true. Note that operator==() is only
  // generated from a defaulted operator<=>(). The justfication for this seems
  // to be that with a custom operator<=>() the struct typically would benefit
  // from having a customized operator==() since equality-comparison often can
  // be done cheaper than relying on operator<=>().
  bool operator==(const Single_object_binding &that) const {
    const Resolve_column_vec &rcols = that.resolve_row->columns;
    bool eq = (ct_node == that.ct_node &&
               compare_doms(
                   rcols[ct_node->primary_key_column_index()].value,
                   rcols[that.ct_node->primary_key_column_index()].value) == 0);
    assert(operator<=>(that) != 0 || eq);
    return eq;
  }
  [[nodiscard]] bool is_empty() const { return bound_object == nullptr; }
  void set_empty() { bound_object = nullptr; }
};
static_assert(std::totally_ordered<Single_object_binding>);

[[maybe_unused]] static std::ostream &operator<<(
    std::ostream &os, const Single_object_binding &bin) {
  os << "Single{" << *bin.ct_node;
  if (bin.is_empty()) {
    os << ":EMPTY}";
    return os;
  }

  os << ", " << JX_(path_str(bin.bound_object));
  if (bin.resolve_row->columns.empty()) {
    os << ":UNRESOLVED}";
    return os;
  }
  const Resolve_column &pkrc =
      bin.resolve_row->columns[bin.ct_node->primary_key_column_index()];
  os << JX_(pkrc) << "}";
  return os;
}

struct Two_object_binding;
static Json_dom *get_pk_dom(const Two_object_binding &);

/**
  Binds two Json_objects (existing_object and new_object) and a
  jdv::Content_tree_node.
  Two_object_binding is used for UPDATE operations on duality view.
 */
struct Two_object_binding {
  Json_object *bound_object = nullptr;
  Json_object *existing_object = nullptr;
  const Content_tree_node *ct_node = nullptr;
  std::unique_ptr<Resolve_row> resolve_row;

  int operator<=>(const Two_object_binding &other) const {
    return compare_bindings(*this, other);
  }

  // Equality is only determined by the Content_tree pointer and the primary key
  // value, but compare_bindings() still defined an order for objects which are
  // equal according to this definition. So operator<=>(that) == 0 implies
  // equality, but the converse is not true. Note that operator==() is only
  // generated from a defaulted operator<=>(). The justfication for this seems
  // to be that with a custom operator<=>() the struct typically would benefit
  // from having a customized operator==() since equality-comparison often can
  // be done cheaper than relying on operator<=>().
  bool operator==(const Two_object_binding &that) const {
    bool eq = (ct_node == that.ct_node &&
               compare_doms(get_pk_dom(*this), get_pk_dom(that)) == 0);
    assert(operator<=>(that) != 0 || eq);
    return eq;
  }

  [[nodiscard]] bool is_empty() const {
    return existing_object == nullptr && bound_object == nullptr;
  }
  void set_empty() {
    existing_object = nullptr;
    bound_object = nullptr;
  }
};
static_assert(std::totally_ordered<Two_object_binding>);

[[maybe_unused]] static std::ostream &operator<<(
    std::ostream &os, const Two_object_binding &bin) {
  os << "Two{" << *bin.ct_node;
  if (bin.is_empty()) {
    os << ":EMPTY}";
    return os;
  }
  os << " bo:" << path_str(bin.bound_object)
     << " eo:" << path_str(bin.existing_object) << " ";
  if (bin.resolve_row->columns.empty()) {
    os << ":UNRESOLVED}";
    return os;
  }
  const Resolve_column &pkrc =
      bin.resolve_row->columns[bin.ct_node->primary_key_column_index()];
  os << JX_(pkrc) << "}";
  os << "}";
  return os;
}

/** Convenience alias */
template <typename T>
using Index_entry = std::reference_wrapper<T>;

/** Convenience alias */
template <typename T>
using Index = std::span<Index_entry<T>>;

/** Spaceship operator which forwards to the indexed type. */
template <typename T>
static int operator<=>(const Index_entry<T> &a, const Index_entry<T> &b) {
  return a.get() <=> b.get();
}

/** Equality operator which forwards to the indexed type. */
template <typename T>
static bool operator==(const Index_entry<T> &a, const Index_entry<T> &b) {
  return a.get() == b.get();
}

/** Lambda object to unwrap an index entry. Useful to create transform_views
or use as projection in algorithms. */
template <typename T>
static auto unwrap_index_entry =
    [](const Index_entry<T> &ie) -> T & { return ie.get(); };

/**
  Convenience function which returns the TABLE_SHARE* for the binding.

  @param bin binding
  @return TABLE_SHARE* for the binding
 */
[[nodiscard]] static const TABLE_SHARE *get_share(const auto &bin) {
  return bin.ct_node->table_ref()->table->s;
}

/**
  Convenience function which returns the table_cache_key of the binding
  as a std::string_view.

  @param bin binding
  @return std::string_view over the table_cache_key for the binding
 */
[[nodiscard]] static std::string_view get_table_cache_key(const auto &bin) {
  return {bin.ct_node->table_ref()->table->s->table_cache_key.str,
          bin.ct_node->table_ref()->table->s->table_cache_key.length};
}

/**
  Create an index over the bindings vector which is allocated on THD::mem_root.
  The index is a span over the memory buffer allocated on the mem_root.

  @param thd THD
  @param bindings vector of bindings

  @return Index
*/
template <typename RT, typename BIN>
[[nodiscard]] static Index<RT> make_mr_index(THD *thd, BIN &bindings) {
  static_assert(sizeof(Index_entry<RT>) == sizeof(RT *));
  static_assert(std::is_trivially_destructible_v<Index_entry<RT>>);
  auto *index_buf = pointer_cast<Index_entry<RT> *>(
      thd->mem_root->Alloc(bindings.size() * sizeof(Index_entry<RT>)));

  assert((reinterpret_cast<std::size_t>(index_buf) &
          (alignof(Index_entry<RT>) - 1)) == 0);

  auto ip = index_buf;
  for (auto &rt : bindings) {
    new (ip) Index_entry<RT>{rt};
    ++ip;
  }
  return {index_buf, bindings.size()};
}

/**
  Goes through the input json (bound_object) and uses the join condition to
  infer column values which are not provided explicitly. Reports error if the
  values provided in the input do not match what is expressed by the join
  condition.

  @param bindings range of bindings
  @return true if error
 */
[[nodiscard]] static bool resolve_column_range(
    const std::ranges::random_access_range auto &bindings) {
  // Look for resolving opportunities in join conditions
  // Outer loop ensures that resolving opportunities in earlier bindings
  // which are made possible by the current iteration can also be discovered
  for (bool restart = true; restart;) {
    restart = false;
    for (const auto &bin : bindings) {
      // Note that in the case of update we sometimes have to create separate
      // "half-bindings", one for the logical delete and one for the logical
      // insert of the update, (this is necessary because it generally is not
      // possible to match the before and after Json_dom using primary key
      // until after resloving is done). The half-binding for the logical
      // delete will only have an existing object and so bin.bound_object ==
      // nullptr for them.
      if (bin.bound_object == nullptr) {
        continue;
      }
      auto &resolved_columns = bin.resolve_row->columns;
      DBUG_LOG("jdv_dml",
               "Show sizes before assert: "
                   << JX_(resolved_columns.size())
                   << JX_(bin.ct_node->key_column_info_list().size()));
      assert(resolved_columns.size() ==
             bin.ct_node->key_column_info_list().size());
      DBUG_LOG("jdv_dml", "bin.bound_object: " << bin.bound_object
                                               << " resolved_columns.empty():"
                                               << resolved_columns.empty());
      if (!bin.ct_node->has_join_condition()) {
        continue;
      }

      std::size_t cix = bin.ct_node->join_column_index();
      std::size_t pix = bin.ct_node->parent_join_column_index();
      auto &[child_kci, child_val, cdef] = resolved_columns[cix];
      auto &[parent_kci, parent_val, pdef] =
          bin.resolve_row->parent->columns[pix];

      bool child_auto_generated = field_will_be_auto_generated(
          current_thd, *child_kci->field(), child_val);
      bool parent_auto_generated = field_will_be_auto_generated(
          current_thd, *parent_kci->field(), parent_val);

      if (bin.ct_node->is_singleton_child()) {
        if (parent_val == nullptr) {
          DBUG_LOG("dml_resolve", "DR: " << bin.ct_node->parent()->name()
                                         << " <- " << bin.ct_node->name());
          pdef |= cdef;
        }
        if ((child_val == nullptr || is_nil_dom(child_val)) &&
            child_auto_generated) {
          cdef = true;
        }
      }

      if (bin.ct_node->is_nested_child()) {
        if (child_val == nullptr) {
          DBUG_LOG("dml_resolve", "DR: " << bin.ct_node->name() << " <- "
                                         << bin.ct_node->parent()->name());
          cdef |= pdef;
        }
        if ((parent_val == nullptr || is_nil_dom(parent_val)) &&
            parent_auto_generated) {
          pdef = true;
        }
      }

      if (!child_auto_generated &&
          // Note that we do not use is_nil_dom(parent_val), as an explicit null
          // value should not trigger deduction via the join condition.
          !is_nil_dom(child_val) && parent_val == nullptr) {
        // resolve parent column from our column
        parent_val = child_val;
        restart = true;
        DBUG_LOG("jdv_dml", "DML-RESOLVE: Resolved "
                                << parent_kci->column_name() << "(" << pix
                                << ") in " << *bin.ct_node->parent() << " from "
                                << child_kci->column_name() << "(" << cix
                                << ") in " << *bin.ct_node);
        continue;
      }

      // Same check here, but for the parent.
      if (!parent_auto_generated && !is_nil_dom(parent_val) &&
          child_val == nullptr) {
        // resolve our column from parent column
        child_val = parent_val;
        restart = true;
        DBUG_LOG("jdv_dml", "DML-RESOLVE: Resolved "
                                << child_kci->column_name() << "(" << cix
                                << ") in " << *bin.ct_node << " from "
                                << parent_kci->column_name() << "(" << pix
                                << ") in " << *bin.ct_node->parent());
      }
    }
  }

  // Check that resolved values are valid and consistent
  // (no PK values are missing (unless they can be resolved later)
  // and values used in join conditions match)
  for (const auto &bin : bindings) {
    if (bin.bound_object == nullptr) {
      continue;
    }
    assert(bin.resolve_row->columns.size() ==
           bin.ct_node->key_column_info_list().size());

    const auto &columns = bin.resolve_row->columns;
    const Resolve_column &pkrc =
        columns[bin.ct_node->primary_key_column_index()];

    if (!pkrc.deferred_resolve) {
      if (bin.bound_object != nullptr && is_nil_dom(pkrc.value)) {
        my_jdv_error<ER_JDV_PRIMARY_KEY_MUST_BE_PROVIDED>(
            bin.ct_node->quoted_qualified_table_name(), pkrc.kci->column_name(),
            bin.bound_object->get_location(), pkrc.kci->key());
        return true;
      }
    }

    if (!bin.ct_node->has_join_condition()) {
      continue;
    }

    std::size_t cix = bin.ct_node->join_column_index();
    std::size_t pix = bin.ct_node->parent_join_column_index();

    auto &[child_kci, child_val, cdef] = bin.resolve_row->columns[cix];
    auto &[parent_kci, parent_val, pdef] =
        bin.resolve_row->parent->columns[pix];

    bool child_auto_generated = field_will_be_auto_generated(
        current_thd, *child_kci->field(), child_val);
    bool parent_auto_generated = field_will_be_auto_generated(
        current_thd, *parent_kci->field(), parent_val);

    bool child_missing = is_nil_dom(child_val) || child_auto_generated;
    bool parent_missing = is_nil_dom(parent_val) || parent_auto_generated;

    // Report error if
    // 1. join condition column values have been provided, but are not equal, or
    // 2. child_val is nil, and does not have deferred resolve, or
    // 3. parent_val is nil, and does not have deferred resolve
    if ((!child_missing && !parent_missing &&
         compare_doms(child_val, parent_val) != 0) ||
        (child_missing && !cdef) || (parent_missing && !pdef)) {
      DBUG_LOG("jdv_dml",
               "DML-RESOLVE-CHECK: Invalid join condition on "
                   << JX_(*bin.ct_node) << JX_(child_kci->column_name())
                   << JX_(*bin.ct_node->parent())
                   << JX_(parent_kci->column_name()) << JX_(cdef) << JX_(pdef));
      const Key_column_info &jci = bin.ct_node->join_column_info();
      const Key_column_info &pjci = bin.ct_node->parent_join_column_info();
      my_jdv_error<ER_JDV_JOIN_CONDITION_NOT_SATISFIED>(
          child_val == nullptr ? Json_path{0} : child_val->get_location(),
          bin.ct_node->quoted_qualified_table_name(), jci.column_name(),
          parent_val == nullptr ? Json_path{0} : parent_val->get_location(),
          bin.ct_node->parent()->quoted_qualified_table_name(),
          pjci.column_name());
      return true;
    }
  }
  return false;
}

/**
  Iterates over a range of bindings and initializes the resolve_columns vector.
  Then calls resolve_range to do the actual resolving.

  @param bindings vector of bindings
  @return true if error
  */
[[nodiscard]] static bool resolve_columns(
    const std::ranges::random_access_range auto &bindings) {
  // Populate Resolve_row with values provided directly by json document
  DBUG_LOG("jdv_dml", "DML-RESOLVE: ");
  for (const auto &bin : bindings) {
    DBUG_LOG("jdv_dml", JX_(*bin.ct_node));

    for ([[maybe_unused]] auto &col : bin.ct_node->key_column_info_list()) {
      DBUG_LOG("jdv_dml", col.column_name() << ",");
    }
    if (bin.bound_object == nullptr) {
      continue;
    }

    auto &resolved_columns = bin.resolve_row->columns;
    for (auto &col : bin.ct_node->key_column_info_list()) {
      // For non-projected columns an empty string_view is stored for the key
      Json_dom *val = get_val(bin.bound_object, col.key());
      resolved_columns.emplace_back(
          &col, val,
          field_will_be_auto_generated(current_thd, *col.field(), val));
    }
  }

  return resolve_column_range(bindings);
}

/**
  Returns the resolved pk value for a Single_object binding.

  @param bin binding to get pk dom for
  @return dom representing PK
 */
static Json_dom *get_pk_dom(const Single_object_binding &bin) {
  auto pk_col_idx = bin.ct_node->primary_key_column_index();
  return bin.resolve_row->columns[pk_col_idx].value;
}

/**
  Returns either the existing or the resolved pk value for a Two_object_binding.
  @param bin binding to get pk dom for
  @return dom representing PK
 */
static Json_dom *get_pk_dom(const Two_object_binding &bin) {
  if (bin.bound_object == nullptr) {
    assert(bin.existing_object != nullptr);
    return bin.existing_object->get(bin.ct_node->primary_key_column().key());
  }

  return bin.resolve_row->columns[bin.ct_node->primary_key_column_index()]
      .value;
}

/**
  Returns a const ref to the Resolve_row of the primary key column.

  @param bin Binding
  @return Resolve_row for primary key column.
 */
static const Resolve_column &get_pk_rc(const auto &bin) {
  assert(bin.resolve_row);
  assert(!bin.resolve_row->columns.empty());
  return bin.resolve_row->columns[bin.ct_node->primary_key_column_index()];
}

/**
  Returns an integer weight which allows sorting bindings with
  deferred_resolve for the primary key after non-generated values.

  @param bin Binding
  @return 1 if primary key has deferred resolve, 0 otherwise
 */
static int get_deferred_resolve_weight(const auto &bin) {
  if (bin.resolve_row->columns.empty()) {
    return 0;
  }
  return get_pk_rc(bin).deferred_resolve ? 1 : 0;
}

/**
  Compares bindings based on:
   - fk_dep_weight,
   - jdv::Content_tree_node pointer value,
   - resolved primary key value
   Sorting with this comparator < 0 orders bindings so that inserts don't
  violate fk-constraints, and groups statements for the same SO ordered by pk so
   that duplicates can be found and removed.

   - In addition to this it is necessary to maintain a predictable order also
   for bindings with the same primary key value, so that half-bindings (bindings
   which have either only the existing or only the bound object) can be
   correctly merged, and duplicates in user input identified.

  @param abin lhs
  @param bbin rhs

  @return -1,0,1 for less, equal, greater.
  */
template <typename BIN>
static int compare_bindings(const BIN &abin, const BIN &bbin) {
  if (abin.ct_node->dependency_weight() < bbin.ct_node->dependency_weight()) {
    return -1;
  }
  if (abin.ct_node->dependency_weight() > bbin.ct_node->dependency_weight()) {
    return 1;
  }
  assert(abin.ct_node->dependency_weight() ==
         bbin.ct_node->dependency_weight());

  if (abin.ct_node->id() != bbin.ct_node->id()) {
    assert(abin.ct_node != bbin.ct_node);
    return abin.ct_node->id() - bbin.ct_node->id();
  }
  // Multiple bindings can reference the same ct_node for nested, for singleton
  // descendants of nested
  assert(abin.ct_node == bbin.ct_node);

  int drw = get_deferred_resolve_weight(abin);
  drw -= get_deferred_resolve_weight(bbin);
  if (drw != 0) {
    return drw;
  }

  Json_dom *apk = get_pk_dom(abin);
  assert(apk != nullptr || get_pk_rc(abin).deferred_resolve);
  Json_dom *bpk = get_pk_dom(bbin);
  assert(bpk != nullptr || get_pk_rc(bbin).deferred_resolve);
  int cd = compare_doms(apk, bpk);
  if (cd != 0) {
    return cd;
  }
  // PK values compare equal (this is the case whenever there
  // are separate bindings for existing and updated object).
  // Need a predictable order also of bindings which have the same pk-value.
  // Sort existing_object bindings before bound_object bindings (there can be
  // duplicate bound_object bindings)
  if (abin.bound_object == nullptr && bbin.bound_object != nullptr) {
    return -1;
  }
  if (abin.bound_object != nullptr && bbin.bound_object == nullptr) {
    return 1;
  }
  return 0;
}

/**
  Less comparator which compares Index_entry objects based on their TABLE_SHARE
  and primary key column value.
*/
static auto share_pk_less = [](const auto &iea, const auto &ieb) {
  const auto &abin = iea.get();
  const auto &bbin = ieb.get();
  if (get_share(abin) != get_share(bbin)) {
    return get_table_cache_key(abin) < get_table_cache_key(bbin);
  }
  // If they, in fact reference the same table, these should be the same
  assert(abin.ct_node->primary_key_column_index() ==
         bbin.ct_node->primary_key_column_index());
  return compare_doms(get_pk_rc(abin).value, get_pk_rc(bbin).value) < 0;
};

/**
  Creates an index over the bindings sorted on
  TABLE_SHARE* and pk-dom value. Verifies that duplicates
  are identical for all columns and marks it as empty.
  Otherwise reports error.

  @param thd THD
  @param bindings bindings vector
  @return true if error
*/
template <typename BV>
[[nodiscard]] static bool check_for_share_pk_duplicates(THD *thd,
                                                        BV &bindings) {
  DBUG_LOG("jdv_dml", "DML-DUP-CHECK: Enter");

  // Need to create an index over the bindings which is sorted just on share and
  // pk to find all duplicates even if they reference different
  // Content_tree_nodes.
  using BT = typename BV::value_type;

  auto share_pk_index = make_mr_index<BT>(thd, bindings);
  auto pks_with_explicit_vals = std::ranges::remove_if(
      share_pk_index,
      [](const BT &bin) {
        // By dropping both delete half bindings and also bindings for which
        // the primary key is generated, we limit the check to only those
        // pk which are explicitly provided. This is ok as we sort all bindings
        // with generated PK later in insertion order.
        return bin.bound_object == nullptr;
      },
      unwrap_index_entry<BT>);

  // Create a range of those index entries which have a valid bound object
  // Note that all bindings in this range will have a valid resolve_row.
  std::ranges::subrange valid_share_pk_index{share_pk_index.begin(),
                                             pks_with_explicit_vals.begin()};

  std::ranges::stable_sort(valid_share_pk_index, share_pk_less);
  DBUG_LOG("jdv_dml", "DML-DUP-CHECK: " << JX_(valid_share_pk_index.size()));

#ifndef NDEBUG
  for (Index_entry<BT> &ie : valid_share_pk_index) {
    BT &bin = ie.get();
    const Resolve_column &pkrc = get_pk_rc(bin);
    DBUG_LOG("jdv_dml", "__DML-DUP-CHECK: "
                            << JX_(pkrc.kci->column_name())
                            << JX_(json_dom_to_string(pkrc.value))
                            << JX_(pkrc.deferred_resolve) << JX_(*bin.ct_node));
  }
#endif /* NDEBUG */

  // Loop over the index - examining entries belonging to the same table
  for (auto index_it = valid_share_pk_index.begin();
       index_it != valid_share_pk_index.end();) {
    std::ranges::subrange remaining{index_it, valid_share_pk_index.end()};
    auto same_table = std::ranges::equal_range(
        remaining, *index_it,
        [](const Index_entry<BT> &a, const Index_entry<BT> &b) {
          return get_table_cache_key(a.get()) < get_table_cache_key(b.get());
        });

#ifndef NDEBUG
    for (const Index_entry<BT> &ie : same_table) {
      DBUG_LOG("jdv_dml", JX_(em_wrap(ie.get().bound_object->get_location())));
    }
#endif /* NDEBUG */

    // FUT.dt: Could limit this check to cases when there are more than one
    // binding referencing the table?

    // We need to check that deferred_resolve is consistent
    // for all tables, since that can be set also for tables which don't
    // themselves have an AUTO_INCREMENT PK. We need to make sure all the
    // 'pkrc's for this table either all have deferred_resolve, or that none of
    // them do. If all 'pkrc's have deferred_resolve we can skip to the next
    // table. If none of them do we need to check each one for duplicates.
    bool first_deferred_resolve = get_pk_rc(index_it->get()).deferred_resolve;

    //  Look for a pkval which does not have the same deferred_resolve value
    //  as the first
    auto badit = std::ranges::find_if(
        same_table | std::ranges::views::drop(1),
        [&](const Index_entry<BT> &ie) {
          return get_pk_rc(ie.get()).deferred_resolve != first_deferred_resolve;
        });
    if (badit != same_table.end()) {
      my_jdv_error<ER_JDV_GENERATED_AND_EXPLICIT_PKS_NOT_ALLOWED>(
          index_it->get().bound_object->get_location(),
          badit->get().bound_object->get_location());
      return true;
    }
    if (first_deferred_resolve) {
      // All bindings in same_table have generated primary key, so we can
      // continue with the next table.
      index_it += same_table.size();
      continue;
    }
    // fallthrough to normal duplicate check

    DBUG_LOG("jdv_dml", "DML-DUP-CHECK: " << JX_(same_table.size()));

    // None of the pkrc's have deferred_resolve, so we need to check them for
    // duplicates
    for (auto same_tbl_it = same_table.begin();
         same_tbl_it != same_table.end();) {
      DBUG_LOG("jdv_dml", "DML-DUP-CHECK: same_table loop");
      std::ranges::subrange same_table_remaining{same_tbl_it, same_table.end()};
      const BT &first_pk_bin = same_tbl_it->get();
      const Resolve_column &first_pk_pkrc = get_pk_rc(first_pk_bin);

      DBUG_LOG("jdv_dml", "DML-DUP-CHECK2: On share@"
                              << get_share(first_pk_bin) << ", pk:"
                              << json_dom_to_string(first_pk_pkrc.value) << " "
                              << RFL(first_pk_pkrc.value->get_location())
                              << JX_(*first_pk_bin.ct_node));

      auto same_pk = std::ranges::equal_range(same_table_remaining,
                                              *same_tbl_it, share_pk_less);

      DBUG_LOG("jdv_dml", "DML-DUP-CHECK: " << JX_(same_pk.size()));

      for ([[maybe_unused]] const Index_entry<BT> &ie : same_pk) {
        DBUG_LOG("jdv_dml",
                 "Eqr: " << em_wrap(ie.get().bound_object->get_location()));
      }

      const Resolve_column_vec &first_pk_cols =
          first_pk_bin.resolve_row->columns;

      // Loop over all those that have the same share-pk as the first,
      // and verify that all column values are identical
      for (Index_entry<BT> &ie : same_pk | std::ranges::views::drop(1)) {
        BT &same_pk_bin = ie.get();
        const Content_tree_node &ct_node = *same_pk_bin.ct_node;

        DBUG_LOG("jdv_dml",
                 "DML-DUP-CHECK: Duplicate from: "
                     << ct_node << " "
                     << RFL(get_pk_rc(same_pk_bin).value->get_location()));

        const Resolve_column_vec &same_pk_cols =
            same_pk_bin.resolve_row->columns;
        // run mismatch to see if any columns have a different value
        auto mmr = std::ranges::mismatch(
            first_pk_cols, same_pk_cols,
            [](const Resolve_column &a, const Resolve_column &b) {
              return compare_doms(a.value, b.value) == 0;
            });

        if (mmr.in1 != first_pk_cols.end()) {
          DBUG_LOG("jdv_dml",
                   "DML-DUP-CHECK: Invalid duplicate from: "
                       << ct_node
                       << ", offending column: " << mmr.in1->kci->column_name()
                       << ", values (" << json_dom_to_string(mmr.in1->value)
                       << " vs. " << json_dom_to_string(mmr.in2->value) << ")");

          my_jdv_error<ER_JDV_PK_DUPLICATES_NOT_IDENTICAL>(
              same_pk_bin.bound_object->get_location(),
              ct_node.quoted_qualified_table_name(),
              first_pk_pkrc.kci->column_name(), mmr.in1->kci->column_name(),
              mmr.in2->value->get_location(), mmr.in1->value->get_location());
          return true;
        }
        // If same_pk_bin is actually identical, mark it as empty so that no
        // statement is generated for it.
        same_pk_bin.set_empty();
      }
      // Advance table loop iterator past the end of the duplicate range
      same_tbl_it += same_pk.size();
    }  // for (auto same_tbl_it = same_table.begin();

    // Advance index loop iterator past the end of the same table range
    index_it += same_table.size();
  }  // for (auto index_it = valid_share_pk_index.begin();

  return false;
}

/**
  Merges bindings for existing and input rows when doing UPDATE. This cannot
  be done when binding as the pk value for the input row may not be known until
  after resolving.

  @param bindings Binding index to merge
 */
static void merge_bindings_for_update(Index<Two_object_binding> &bindings) {
  assert(std::ranges::is_sorted(bindings));

  // Look for adjacent same-pk bindings.
  // We need to copy exiting_object from "delete" binding to "insert" binding to
  // create an "update" binding.
  for (auto adjit = std::ranges::adjacent_find(bindings);
       adjit != bindings.end();
       adjit = std::ranges::adjacent_find(adjit + 1, bindings.end())) {
    Two_object_binding &cur = adjit->get();
    Two_object_binding &nxt = (adjit + 1)->get();
    DBUG_LOG("jdv_dml",
             "cur.(bound_object:" << cur.bound_object << " .existing_object:"
                                  << cur.existing_object << JX_(*cur.ct_node)
                                  << ", nxt.(bound_object:" << nxt.bound_object
                                  << " .existing_object:" << nxt.existing_object
                                  << JX_(*nxt.ct_node));

    if (cur.existing_object == nullptr) {
      // Nothing to merge
      continue;
    }
    if (nxt.existing_object != nullptr) {
      // Existing/input duplicate - can not happen directly in array_agg but if
      // different array_aggs element reference same subobject
      cur.existing_object = nullptr;
      assert(cur.bound_object == nullptr);
      continue;
    }

    // We are not guaranteed that cur is the "delete"-binding, since there
    // may be duplicate "insert" bindings
    assert(nxt.existing_object == nullptr);
    // Copy the existing object "forward",
    // delete binding -> insert binding(>update binding) -> duplicate insert
    // binding
    nxt.existing_object = cur.existing_object;

    DBUG_LOG("jdv_dml", "DML-UPDATE-MERGE: Merging bindings for "
                            << *cur.ct_node << " pkcol:"
                            << cur.ct_node->primary_key_column().column_name()
                            << " = "
                            << json_dom_to_string(cur.existing_object->get(
                                   cur.ct_node->primary_key_column().key())));
    // Clear the existing object of "delete" binding, so that it becomes a noop
    if (cur.bound_object == nullptr) {
      cur.existing_object = nullptr;
    }
  }
}

/**
  Check Json_object being passed in for keys which are not present in the
  JDV definition (including _metadata for the root object), and report error
  if that is the case.

  @param thd       Thread handle.
  @param input_obj input object
  @param ct_node jdv definition

  @return true if error
 */
[[nodiscard]] static bool check_for_unmatched_input_keys(
    THD *thd, Json_object *input_obj, const Content_tree_node &ct_node) {
  assert(
      std::ranges::is_sorted(*input_obj | std::ranges::views::keys, size_less));
  auto keys_as_string_view_adaptor = std::ranges::views::transform(
      [](const auto &a) -> std::string_view { return a.first; });

  // Add 1 for _metadata in root objects
  std::size_t jdv_key_count = ct_node.key_column_map().size() +
                              ct_node.children().size() +
                              (ct_node.is_root_object() ? 1 : 0);

  // Mem-root string_view array to hold the JDV keys and all of the input keys
  // (if they are all unmatched)
  std::string_view *all_keys = thd->mem_root->ArrayAlloc<std::string_view>(
      jdv_key_count + input_obj->cardinality());

  // Create a view of the array slice for the JDV keys
  auto jdv_keys_rng = std::ranges::views::counted(all_keys, jdv_key_count);

  // Create a view of the array slice where the unmatched keys will be stored
  auto unmatched_dst_rng =
      std::ranges::views::counted(jdv_keys_rng.end(), input_obj->cardinality());

  // Grab all the JDV keys and sort them in the Json way
  // Returns std::ranges::copy_result
  auto cpr =
      std::ranges::copy(ct_node.key_column_map() | std::ranges::views::keys,
                        jdv_keys_rng.begin());

  // Returns std::ranges::unary_transform_result
  auto utr = std::ranges::transform(
      ct_node.children(), cpr.out,
      [](const auto *ctn) -> std::string_view { return ctn->name(); });

  // Add the _metadata key for root nodes
  if (ct_node.is_root_object()) {
    *utr.out = metadatakey;
  }
  std::ranges::sort(jdv_keys_rng, size_less);

  // Use set_difference to find unmatched keys and report an error if (at least)
  // one was found.
  auto diffres = std::ranges::set_difference(
      *input_obj | keys_as_string_view_adaptor, jdv_keys_rng,
      unmatched_dst_rng.begin(), size_less);
  if (diffres.out != unmatched_dst_rng.begin()) {
    my_jdv_error<ER_JDV_UNMAPPED_KEY>(
        input_obj->get(*unmatched_dst_rng.begin())->get_location(),
        ct_node.quoted_qualified_table_name());
    return true;
  }
  return false;
}

/**
  Compare Json_doms for equality using a Json_wrapper.

  @param ajd left-hand side of comparison
  @param bjd right-hand side of comparison
  @return true if equal
*/
static bool is_equal(Json_dom *ajd, Json_dom *bjd) {
  auto ajw = Json_wrapper{ajd, /*alias*/ true};
  auto bjw = Json_wrapper{bjd, /*alias*/ true};
  return ajw.compare(bjw) == 0;
}

/**
  Helper function to create unique_ptr to Resolve_row. (On mac
  std::make_unique() does not do init-list initialization).

  @param parent parent
  @return resolve_row
 */

[[nodiscard]] static std::unique_ptr<Resolve_row> make_rr_up(
    Resolve_row *parent) {
  auto rrptr = std::make_unique<Resolve_row>();
  rrptr->parent = parent;
  return rrptr;
}

/**
 Overload for Single_object_binding (INSERT/DELETE) with Json_object
 (JSON_OBJECT).
 The Json_object* argument is used as tag for choosing the correct overload,
 (instead of creating a separate tag type).

 @param pbx parent binding index
 @param child_ct_node child content tree node
 @param stack binding stack
 @return true if error
*/
[[nodiscard]] static bool push_object_child_bindings(
    std::size_t pbx, const Content_tree_node *child_ct_node,
    std::vector<Single_object_binding> *stack) {
  auto &stk = *stack;
  Json_dom *child_dom = stk[pbx].bound_object->get(child_ct_node->name());
  Resolve_row *prr = stk[pbx].resolve_row.get();

  auto [jtc, child_object] = inspect_dom<Json_object>(child_dom);
  if (jtc == enum_json_type::J_ERROR) {
    my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>(
        child_dom->get_location(), Json_type_traits<Json_object>::name);
    return true;
  }
  if (child_object != nullptr) {
    stack->push_back({child_object, child_ct_node, make_rr_up(prr)});
  }
  return false;
}

/**
  Overload for Single_object_binding (INSERT/DELETE) with Json_array
  (JSON_ARRAYAGG).
  The Json_array* argument is used as tag for choosing the correct overload,
  (instead of creating a separate tag type).

  @param pbx parent binding index
  @param child_ct_node child content tree node
  @param stack binding stack
  @return true if error
 */
[[nodiscard]] static bool push_array_child_bindings(
    std::size_t pbx, const Content_tree_node *child_ct_node,
    std::vector<Single_object_binding> *stack) {
  auto &stk = *stack;
  Json_dom *child_dom = stk[pbx].bound_object->get(child_ct_node->name());
  Resolve_row *prr = stk[pbx].resolve_row.get();
  auto [jtc, child_array] = inspect_dom<Json_array>(child_dom);
  if (jtc == enum_json_type::J_ERROR) {
    my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>(
        child_dom->get_location(), Json_type_traits<Json_array>::name);
    return true;
  }
  if (child_array == nullptr) {
    return false;
  }

  for (const auto &elt : *child_array) {
    auto [ejtc, elt_object] = inspect_dom<Json_object>(elt.get());
    if (ejtc == enum_json_type::J_ERROR) {
      my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>(
          elt->get_location(), Json_type_traits<Json_object>::name);
      return true;
    }
    if (elt_object != nullptr) {
      stack->push_back({elt_object, child_ct_node, make_rr_up(prr)});
    }
  }
  return false;
}

/**
  Overload for Two_object_binding (UPDATE) with Json_object (JSON_OBJECT).
  The Json_object* argument is used as tag for choosing the correct overload,
  (instead of creating a separate tag type).

  @param pbx parent binding index
  @param child_ct_node child content tree node
  @param stack binding stack
  @return true if error
*/
[[nodiscard]] static bool push_object_child_bindings(
    std::size_t pbx, const Content_tree_node *child_ct_node,
    std::vector<Two_object_binding> *stack) {
  auto &stk = *stack;
  Resolve_row *prr = stk[pbx].resolve_row.get();
  const std::string_view &child_name = child_ct_node->name();
  Json_object *existing_child_object = inspect_valid_dom<Json_object>(
      get_val(stk[pbx].existing_object, child_name));

  Json_dom *bound_child_dom = get_val(stk[pbx].bound_object, child_name);
  auto [jtc, bound_child_object] = inspect_dom<Json_object>(bound_child_dom);
  if (jtc == enum_json_type::J_ERROR) {
    my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>(
        bound_child_dom->get_location(), Json_type_traits<Json_object>::name);
    return true;
  }
  if (existing_child_object != nullptr || bound_child_object != nullptr) {
    stack->push_back({.bound_object = bound_child_object,
                      .existing_object = existing_child_object,
                      .ct_node = child_ct_node,
                      .resolve_row = make_rr_up(prr)});
  }
  return false;
}

/**
  Overload for Two_object_binding (UPDATE) with Json_array (JSON_ARRAY_AGG).
  The Json_array* argument is used as tag for choosing the correct overload,
  (instead of creating a separate tag type).

  @param pbx parent binding index
  @param child_ct_node child content tree node
  @param stack binding stack
  @return true if error
 */
[[nodiscard]] static bool push_array_child_bindings(
    std::size_t pbx, const Content_tree_node *child_ct_node,
    std::vector<Two_object_binding> *stack) {
  DBUG_LOG("jdv_dml", "DML-BIND: On child " << *child_ct_node);
  auto &stk = *stack;
  Resolve_row *prr = stk[pbx].resolve_row.get();
  const std::string_view &child_name = child_ct_node->name();

  Json_array *existing_child_array = inspect_valid_dom<Json_array>(
      get_val(stk[pbx].existing_object, child_name));

  Json_dom *bound_child_dom = stk[pbx].bound_object->get(child_name);
  auto [jtc, bound_child_array] = inspect_dom<Json_array>(bound_child_dom);
  if (jtc == enum_json_type::J_ERROR) {
    my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>(
        bound_child_dom->get_location(), Json_type_traits<Json_array>::name);
    return true;
  }

  DBUG_LOG("jdv_dml", "DML-BIND: At line " << __LINE__);

  if (existing_child_array != nullptr) {
    DBUG_LOG("jdv_dml", "DML-BIND: existing_child_array->size(): "
                            << existing_child_array->size());
    for (const auto &elt : *existing_child_array) {
      Json_object *elt_object = inspect_valid_dom<Json_object>(elt.get());
      if (elt_object != nullptr) {
        stack->push_back({.bound_object = nullptr,
                          .existing_object = elt_object,
                          .ct_node = child_ct_node,
                          .resolve_row = make_rr_up(prr)});
      }
    };
  }
  if (bound_child_array != nullptr) {
    DBUG_LOG("jdv_dml", "DML-BIND: bound_child_array->size(): "
                            << bound_child_array->size());
    for (const auto &elt : *bound_child_array) {
      auto [ejtc, elt_object] = inspect_dom<Json_object>(elt.get());
      if (ejtc == enum_json_type::J_ERROR) {
        my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>(
            elt->get_location(), Json_type_traits<Json_object>::name);
        return true;
      }
      if (elt_object != nullptr) {
        stack->push_back({.bound_object = elt_object,
                          .existing_object = nullptr,
                          .ct_node = child_ct_node,
                          .resolve_row = make_rr_up(prr)});
      }
    }
  }
  return false;
}

/**
  Takes a stack (std::vector) with a single binding at the top. Creates
  bindings for all the children of that binding by combining each Json child
  with the corresponding CTN child and place these on the stack. Proceeds with
  the next element now on the stack. When there are no more element left on the
  stack the process is complete and the stack contains a flattened sequence of
  bindings for each Json_object in the input which corresponds to a
  jdv::Content_tree_node. This flattened sequence can then be sorted and
  traversed in either direction as needed when generating statements
  for the base tables.

  @param stack binding stack
  @return true if error
  */
template <typename BIN>
[[nodiscard]] static bool flatten(std::vector<BIN> *stack) {
  for (std::size_t pbx = 0; pbx < stack->size(); ++pbx) {
    if (std::ranges::any_of(
            (*stack)[pbx].ct_node->children(),
            [&](const Content_tree_node *child_ct_node) {
              return (child_ct_node->is_root_object() ||
                      child_ct_node->is_singleton_child())
                         ? push_object_child_bindings(pbx, child_ct_node, stack)
                         : push_array_child_bindings(pbx, child_ct_node, stack);
            })) {
      return true;
    }
  }
  return false;
}

/**
  Check if the row represented by the binding already exists, and if
  so return a diff vector (true for columns which currently does not have
  the value of the binding).
  This function can only be called when already inside
  do_in_substatement_context().

  @param thd THD
  @param binding checkee
  @return diff vector or nullopt if error or row not found
 */

[[nodiscard]] static std::optional<std::vector<bool>> select_diff_vector(
    THD *thd, const auto &binding) {
  assert((thd->in_sub_stmt & SUB_STMT_DUALITY_VIEW) != 0);
  const auto &ct_node = *binding.ct_node;

  const Resolve_column &pkrc =
      binding.resolve_row->columns[ct_node.primary_key_column_index()];
  if (pkrc.value == nullptr) {
    assert(false);
    return std::nullopt;
  }

  std::string query = "SELECT ";
  for (const Resolve_column &rc : binding.resolve_row->columns) {
    if (&rc == &pkrc || rc.value == nullptr) {
      // If we are not going to modify the value, we don't care what it
      // currently is
      query.append("0, ");
      continue;
    }

    append_identifier(&query, rc.kci->column_name());
    query.append(" <> ");
    if (append_json_dom(&query, rc.value, col_expects_b64(*rc.kci))) {
      return std::nullopt;
    }

    query.append(", ");
  }
  query.replace(query.size() - 2, 2, " FROM ");
  query.append(ct_node.quoted_qualified_table_name());
  query.append(" WHERE ");
  append_identifier(&query, pkrc.kci->column_name());
  query.append(" = ");
  assert(!col_expects_b64(*pkrc.kci));
  if (append_json_dom(&query, pkrc.value)) {
    return std::nullopt;
  }
  query.append(" FOR UPDATE");

  DBUG_LOG("jdv_dml", "DML-DIFF: query:" << query);
  Regular_statement_handle stmt_handle(thd, query.data(), query.length());
  stmt_handle.set_capacity(
      (binding.resolve_row->columns.size() * sizeof(std::int64_t)) +
      std::size_t{1});
  bool ret_val = stmt_handle.execute();
  if (ret_val) {
    DBUG_LOG("jdv_dml", "DML-DIFF: SELECT query '" << query << "' failed");
    return std::nullopt;
  }
  Result_set *rs = stmt_handle.get_current_result_set();
  assert(rs != nullptr);
  DBUG_LOG("jdv_dml",
           "DML-DIFF: SELECT query returned " << rs->size() << " rows");

  if (rs->size() != 1) {
    // Row does not exist
    return std::nullopt;
  }
  const auto *row = rs->get_next_row();
  assert(row != nullptr);

  std::optional<std::vector<bool>> ret = std::vector<bool>{};
  for (std::size_t ci = 0; ci < row->size(); ++ci) {
    auto *c = row->get_column(ci);
    if (c->index() == 0) {
      ret->push_back(true);
      continue;
    }
    DBUG_LOG("jdv_dml", "c->index(): " << c->index());
    std::int64_t cv = *std::get<std::int64_t *>(*c);
    ret->push_back(cv != 0);
  }
  return ret;
}

/**
  Uses a Regular_statement_handle to execute a single statement passed as
  argument.

  @param       thd            THD
  @param       caller_da      DA in which to accumulate diagnostics
  @param       stmt           statement text
  @param [out] affected_rows  Number of affected rows.

  @return true if error
 */
static bool run_substmt(THD *thd, Diagnostics_area *caller_da,
                        std::string_view stmt, ulonglong *affected_rows) {
  assert(affected_rows != nullptr);
  caller_da->mark_preexisting_sql_conditions();

  Regular_statement_handle stmt_handle(thd, stmt.data(), stmt.length());
  stmt_handle.set_clear_diagnostics_area_on_success(false);
  if (stmt_handle.execute()) {
    DBUG_LOG("jdv_dml", "DML: statement " << stmt << " failed");
    caller_da->copy_new_sql_conditions(thd, thd->get_stmt_da());
    return true;
  }

  assert(!thd->is_error());
  /*
    Update affected_rows from DA after successful execution of a statement.
  */
  *affected_rows = *affected_rows + thd->get_stmt_da()->affected_rows();

  caller_da->copy_new_sql_conditions(thd, thd->get_stmt_da());

  // Reset DA status for next statement execution.
  thd->get_stmt_da()->reset_diagnostics_area();

  return false;
}

// These values are arbitrary.
constexpr std::size_t STMT_RESERVE_SIZE = 512;
constexpr std::size_t BINDINGS_RESERVE_SIZE = 48;

enum class Stmt_state { EXECUTE = 0, SKIP = 1, ERROR = 2 };

/**
  Produces an INSERT statement from a binding. For INSERT this is
  a Single_object_binding, but when an update decays into an INSERT,
  this function is also invoked with a Two_object_binding.

  @param thd THD
  @param binding insertee
  @param sbufp statement text buffer
  @return Statement_status - indicates if statement must be executed, skipped or
  an error occured
 */
[[nodiscard]] static Stmt_state make_insert(THD *thd, const auto &binding,
                                            auto *sbufp) {
  assert(binding.bound_object != nullptr);
  assert(binding.ct_node != nullptr);

  const Content_tree_node &ct_node = *binding.ct_node;
  assert(!ct_node.key_column_info_list().empty());
  assert(binding.resolve_row);
  const auto &c_resolved_columns =
      binding.resolve_row->columns;  // get_resolved_columns(binding);
  assert(c_resolved_columns.size() == ct_node.key_column_info_list().size());

  for (Resolve_column &rc : binding.resolve_row->columns) {
    if (field_will_be_auto_generated(thd, *rc.kci->field(), rc.value)) {
      DBUG_LOG("jdv_dml", "Found unresolved AICOL for " << JX_(ct_node));

      // We only need to do this if we actually need
      // last_insert_id propagated through join conditions (or some other
      // purpose), but it is easier to do it always.
      binding.resolve_row->unresolved_autoinc_column = &rc;
    }
  }
  assert(!thd->is_error());

  bool is_with_insert = ct_node.allows_insert();
  bool is_with_update = ct_node.allows_update();

  auto &sbuf = *sbufp;
  sbuf.reserve(STMT_RESERVE_SIZE);

  constexpr bool is_insert_for_update =
      std::is_same_v<decltype(binding), const Two_object_binding &>;
  bool is_root_or_nested_insert_update =
      ct_node.is_root_object() ||
      (ct_node.is_nested_child() && is_insert_for_update);
  DBUG_LOG("jdv_dml",
           "DML-INSERT: is_insert_for_update:" << is_insert_for_update);

  if (is_root_or_nested_insert_update && !is_with_insert) {
    my_jdv_error<ER_JDV_MISSING_INSERT_TAG>(
        ct_node.quoted_qualified_table_name(),
        binding.bound_object->get_location());
    return Stmt_state::ERROR;
  }

  const Resolve_column &pkrc = get_pk_rc(binding);
  if (!is_root_or_nested_insert_update && pkrc.value != nullptr) {
    assert(pkrc.value != nullptr);

    auto dr = select_diff_vector(thd, binding);
    if (thd->is_error()) {
      return Stmt_state::ERROR;
    }
    bool row_exists = static_cast<bool>(dr);

    if (!row_exists) {
      if (!is_with_insert) {
        DBUG_LOG("jdv_dml", "Diff check returned no rows");
        if (ct_node.is_singleton_child() && is_insert_for_update) {
          my_jdv_error<ER_JDV_MISSING_INSERT_TAG>(
              ct_node.quoted_qualified_table_name(),
              binding.bound_object->get_location());
          return Stmt_state::ERROR;
        }
        my_jdv_error<ER_JDV_MISSING_READONLY_SUBOBJECT>(
            binding.bound_object->get_location(),
            ct_node.quoted_qualified_table_name(), pkrc.kci->column_name(),
            json_dom_to_string(pkrc.value));
        return Stmt_state::ERROR;
      }
    } else {
      if (!is_with_update) {
        auto cs = dr.value();
        if (cs[ct_node.join_column_index()]) {
          // The existing value of the join condition does not match what is
          // being inserted.
          my_jdv_error<ER_JDV_JOIN_CONDITION_VIOLATION>(
              binding.bound_object->get_location(),
              ct_node.quoted_qualified_table_name(),
              ct_node.key_column_info_list()[ct_node.join_column_index()]
                  .column_name());
          return Stmt_state::ERROR;
        }
        return Stmt_state::SKIP;
      }
    }
  }

  sbuf.reserve(STMT_RESERVE_SIZE);
  sbuf.append("INSERT INTO ");
  sbuf.append(ct_node.quoted_qualified_table_name());
  sbuf.append(" (");

  DBUG_LOG("jdv_dml", "DML-INSERT: Listing columns to be populated");
  const auto &kcilst = ct_node.key_column_info_list();
  for ([[maybe_unused]] const auto &kc : kcilst) {
    DBUG_LOG("jdv_dml", kc.column_name() << " => \"" << kc.key() << "\":, ");
  }

  for (const auto &col : kcilst) {
    append_identifier(&sbuf, col.column_name());
    sbuf.append(", ");
  }
  sbuf.replace(sbuf.size() - 2, 2, ") VALUES (");

  if (is_root_or_nested_insert_update) {
    for (const Resolve_column &rc : c_resolved_columns) {
      if (rc.value == nullptr && is_auto_increment(*rc.kci->field())) {
        sbuf.append("NULL");
      } else {
        if (append_json_dom(&sbuf, rc.value, col_expects_b64(*rc.kci)))
          return Stmt_state::ERROR;
      }
      sbuf.append(", ");
    }
    sbuf.replace(sbuf.size() - 2, 2, ")");
    return Stmt_state::EXECUTE;
  }

  // Updatable sub-nodes can have existing row, so need ON DUPLICATE KEY UPDATE
  auto values_pos = sbuf.size();
  for (const Resolve_column &rc : c_resolved_columns) {
    DBUG_LOG("jdv_dml", "DML-INSERT: Considering " << rc.kci->column_name()
                                                   << " <- " << rc.value);
    if (rc.value == nullptr) {
      if (is_auto_increment(*rc.kci->field())) {
        sbuf.insert(values_pos, "NULL, ");
        values_pos += 6;
        continue;
      }
      // If we could not deduce a value for the column, we add DEFAULT to the
      // values list, and omit the column from the ON DUPLICATE assignment list.
      sbuf.insert(values_pos, "DEFAULT, ");
      values_pos += 9;
      continue;
    }
    append_identifier(&sbuf, rc.kci->column_name());
    sbuf.append(" = ");
    auto vstart = sbuf.size();

    if (append_json_dom(&sbuf, rc.value, col_expects_b64(*rc.kci))) {
      return Stmt_state::ERROR;
    }

    sbuf.append(", ");
    auto vsize = sbuf.size() - vstart;

    // Insert value into VALUES list also
    sbuf.insert(values_pos, sbuf.c_str() + vstart, vsize);
    values_pos += vsize;
  }

  // Replace trailing ", " in values list with the start of the ON DUPLICATE
  // section
  // Only append ON DUP... if UPDATE assignments have been added.
  sbuf.replace(
      values_pos - 2, 2,
      (sbuf.size() > values_pos ? ") ON DUPLICATE KEY UPDATE " : ")  "));
  sbuf.resize(sbuf.size() - 2);

  return Stmt_state::EXECUTE;
}

static Stmt_state make_delete(THD *, const Single_object_binding &, auto *);

/**
  Obtains the dom of the etag sub-object.

  @param root_object containing the etag sub-object
  @return etag sub-object dom
 */
static Json_dom *etag_dom(Json_object *root_object) {
  assert(root_object != nullptr);
  if (root_object == nullptr) {
    return nullptr;
  }
  Json_dom *metadata = root_object->get(metadatakey);
  if (metadata == nullptr) {
    return nullptr;
  }
  Json_object *metadata_object = down_cast<Json_object *>(metadata);
  return metadata_object->get(etagkey);
}

/**
  Checks if etags in existing and bound objects match. Succeeds with warning if
  etag is not provided in bound object.

  @param binding root binding
  @return true if error (mismatch)
 */
static bool check_etag(const Two_object_binding &binding) {
  Json_dom *before_etag = etag_dom(binding.existing_object);
  assert(before_etag != nullptr);

  Json_dom *cur_etag = etag_dom(binding.bound_object);
  if (cur_etag == nullptr) {
    push_warning(current_thd, ER_JDV_MISSING_ETAG);
    return false;
  }
  if (is_equal(before_etag, cur_etag)) {
    return false;
  }

  my_jdv_error<ER_JDV_ETAG_MISMATCH>(json_dom_to_string(cur_etag).c_str());
  return true;
}

/**
  Produces an UPDATE statement from a Two_object_binding. If there is no
  existing object an INSERT is generated. If there is no bound object a DELETE
  statement is generated.

  @param thd THD
  @param binding updatee
  @param sbufp statement text buffer
  @return Statement_status - indicates if statement must be executed, skipped or
  an error occured
 */
[[nodiscard]] static Stmt_state make_update(THD *thd,
                                            const Two_object_binding &binding,
                                            auto *sbufp) {
  const Content_tree_node &ct_node = *binding.ct_node;
  assert(!ct_node.key_column_info_list().empty());
  assert(binding.resolve_row);

  if (binding.existing_object == nullptr) {
    DBUG_LOG("jdv_dml",
             "DML-UPDATE: b.existing_object == nullptr for UPDATE of "
                 << ct_node << ", generating INSERT of bound object instead.");
    return make_insert(thd, binding, sbufp);
  }
  assert(binding.existing_object != nullptr);
  if (binding.bound_object == nullptr) {
    DBUG_LOG("jdv_dml",
             "DML-UPDATE: b.bound_object == nullptr for UPDATE of "
                 << ct_node.name()
                 << ", generating DELETE of existing object instead.");
    return make_delete(thd,
                       {.bound_object = binding.existing_object,
                        .ct_node = binding.ct_node,
                        .resolve_row = {}},
                       sbufp);
  }

  // Check if any singleton child is removed or set to null
  for (const auto &c : binding.ct_node->children()) {
    if (!c->is_singleton_child()) {
      continue;
    }
    if (!is_nil_dom(binding.existing_object->get(c->name())) &&
        is_nil_dom(binding.bound_object->get(c->name()))) {
      my_jdv_error<ER_JDV_MISSING_DELETE_TAG>(
          c->quoted_qualified_table_name(),
          binding.existing_object->get(c->name())->get_location());
      return Stmt_state::ERROR;
    }
  }

  auto &sbuf = *sbufp;
  sbuf.reserve(STMT_RESERVE_SIZE);

  sbuf.append("UPDATE ");
  sbuf.append(ct_node.quoted_qualified_table_name());
  sbuf.append(" SET ");
  auto emptysize = sbuf.size();

  const auto &resolved_columns = binding.resolve_row->columns;
  assert(resolved_columns.size() == ct_node.key_column_info_list().size());

  // On update all columns must be resolved in order the perform
  // etag check.
  if (std::ranges::any_of(resolved_columns, [&](const auto &rc) {
        if (rc.value == nullptr) {
          my_jdv_error<ER_JDV_MISSING_VALUE>(
              binding.bound_object->get_location(),
              ct_node.quoted_qualified_table_name(), rc.kci->column_name());
          return true;
        }
        return false;
      })) {
    return Stmt_state::ERROR;
  }

  const auto &pk_col_info = ct_node.primary_key_column();
  Json_dom *pk_edom = binding.existing_object->get(pk_col_info.key());
  assert(pk_edom != nullptr);
  Json_dom *pk_rdom =
      resolved_columns[ct_node.primary_key_column_index()].value;
  assert(pk_rdom != nullptr);
  if (!is_equal(pk_edom, pk_rdom)) {
    // This is a PK update.
    if (ct_node.is_root_object()) {
      my_jdv_error<ER_JDV_PK_UPDATES_NOT_ALLOWED>(
          ct_node.quoted_qualified_table_name(), pk_col_info.column_name(),
          pk_rdom->get_location());
      return Stmt_state::ERROR;
    }
    assert(ct_node.is_singleton_child());

    // We can only proceed if a row with the new
    // PK value already exists in the base table.
    auto dr = select_diff_vector(thd, binding);
    if (thd->is_error()) {
      return Stmt_state::ERROR;
    }

    if (!dr) {
      // Seems like it would be more appopriate to report an error
      // about pk updates not being allowed here... but
      my_jdv_error<ER_JDV_MISSING_UPDATE_TAG>(
          ct_node.quoted_qualified_table_name(), pk_col_info.column_name(),
          pk_rdom->get_location());
      return Stmt_state::ERROR;
    }
    assert(dr);

    // A row with this pk already exists in the base table
    DBUG_LOG("jdv_dml",
             "DML-UPDATE: pk value of "
                 << ct_node
                 << " is changed, but a row with the new pk value exists");

    if (ct_node.read_only()) {
      // Note that this implies that the existing subobject need not be
      // identical to what is being passed in
      return Stmt_state::SKIP;
    }

    // Must generate an update statement for this existing row. However,
    // comparisons with the existing json object from the binding is
    // meaningless since the row which will actually be updated is completely
    // unrelated in this case.
    const auto &kcis = ct_node.key_column_info_list();
    for (std::size_t i = 0; i < dr->size(); ++i) {
      if ((*dr)[i]) {
        append_identifier(&sbuf, kcis[i].column_name());
        sbuf.append(" = ");
        if (append_json_dom(&sbuf, resolved_columns[i].value,
                            col_expects_b64(kcis[i]))) {
          return Stmt_state::ERROR;
        }
        sbuf.append(", ");
      }
    }
  }  // Pk update
  else {
    // Generate assignment list from projected columns in base_columns and the
    // resolved column values. Presumably resolving is not actually necessary
    // for true UPDATEs (only when UPDATE performs INSERT), but since we have
    // already resolved the columns, it is cheaper to get them from resolve_row,
    // even if they must also exist in binding.bound_object
    for (const Resolve_column &rc : resolved_columns) {
      Json_dom *existing_val = binding.existing_object->get(rc.kci->key());
      Json_dom *updated_val = rc.value;
      if (updated_val == nullptr) {
        DBUG_LOG("jdv_dml",
                 "DML-UPDATE: " << rc.kci->key()
                                << " not found in new value. Skipping");
        continue;
      }

      if (existing_val != nullptr && updated_val != nullptr &&
          is_equal(updated_val, existing_val)) {
        std::string before_value;
        if (append_json_dom(&before_value, existing_val,
                            col_expects_b64(*rc.kci))) {
          return Stmt_state::ERROR;
        }
        DBUG_LOG("jdv_dml", "DML-UPDATE: Key: '"
                                << rc.kci->key() << "', col: '"
                                << rc.kci->column_name() << "' is unchanged, ('"
                                << before_value << "'). Skipping.");
        continue;
      }
      assert(rc.kci != &ct_node.primary_key_column());

      if (!rc.kci->allows_update()) {
        my_jdv_error<ER_JDV_MISSING_UPDATE_TAG>(
            ct_node.quoted_qualified_table_name(), rc.kci->column_name(),
            updated_val->get_location());
        return Stmt_state::ERROR;
      }

      append_identifier(&sbuf, rc.kci->column_name());
      sbuf.append(" = ");
      if (append_json_dom(&sbuf, updated_val, col_expects_b64(*rc.kci))) {
        return Stmt_state::ERROR;
      }
      sbuf.append(", ");
    }
  }  // else

  // Check if any columns were actually added. If not return empty statement.
  if (sbuf.size() == emptysize) {
    sbuf = "/* Nothing to do for ";
    sbuf.append(ct_node.name()).append(" (");
    sbuf.append(ct_node.quoted_qualified_table_name());
    sbuf.append(") */");
    return Stmt_state::SKIP;
  }
  sbuf.replace(sbuf.size() - 2, 2, " WHERE ");

  append_identifier(&sbuf, pk_col_info.column_name());
  sbuf.append(" = ");
  assert(!col_expects_b64(pk_col_info));
  return append_json_dom(&sbuf, pk_rdom) ? Stmt_state::ERROR
                                         : Stmt_state::EXECUTE;
}

/**
  Produces a delete statement from a Single_object_binding.
  When an update decays into a DELETE, this function is called
  a with a Single_object_binding created on the fly from
  the Two_object_binding - which is only possible because
  DELETE does not require a populated resolve_row.

  @param binding deletee
  @param sbufp statement text buffer
  @return Statement_status - indicates if statement must be executed, skipped or
  an error occured
 */
[[nodiscard]] static Stmt_state make_delete(
    THD *, const Single_object_binding &binding, auto *sbufp) {
  assert(binding.bound_object != nullptr);
  const auto &ct_node = *binding.ct_node;
  assert(!ct_node.key_column_info_list().empty());
  if (!ct_node.allows_delete()) {
    if (ct_node.is_singleton_child()  //&&
                                      // Not delete for update
                                      // binding.resolve_row.get() != nullptr
    ) {
      return Stmt_state::SKIP;
    }
    DBUG_LOG("jdv_dml", "DELETE on " << *binding.ct_node
                                     << " rejected due to missing TAG");
    my_jdv_error<ER_JDV_MISSING_DELETE_TAG>(
        binding.ct_node->quoted_qualified_table_name(),
        binding.bound_object->get_location());
    return Stmt_state::ERROR;
  }

  auto &sbuf = *sbufp;
  sbuf.reserve(STMT_RESERVE_SIZE);

  sbuf.append("DELETE FROM ");
  sbuf.append(ct_node.quoted_qualified_table_name());
  sbuf.append(" WHERE ");

  append_identifier(&sbuf, ct_node.primary_key_column().column_name());
  sbuf.append(" = ");

  Json_dom *pk_val =
      binding.bound_object->get(ct_node.primary_key_column().key());
  assert(pk_val != nullptr);
  assert(!col_expects_b64(ct_node.primary_key_column()));
  return append_json_dom(&sbuf, pk_val) ? Stmt_state::ERROR
                                        : Stmt_state::EXECUTE;
}

[[nodiscard]] static bool check_input_json(Json_dom *input_dom) {
  if (input_dom->json_type() != enum_json_type::J_OBJECT) {
    my_jdv_error<ER_JDV_UNEXPECTED_JSON_TYPE>("$", "Json_object");
    return true;
  }
  return false;
}

/**
  Creates bindings from inserted Json_object,
  orders them, creates and executes INSERT statements against the base tables.

  @param thd THD
  @param jw JSON object to insert
  @param ct_node content tree root node of view
  @param[out] affected_rows number of affected base table rows
 */
[[nodiscard]] static bool jdv_handle_insert(THD *thd, Json_wrapper *jw,
                                            Content_tree_node *ct_node,
                                            ulonglong *affected_rows) {
  Json_dom *dom = jw->to_dom();
  assert(dom != nullptr);
  if (check_input_json(dom)) {
    return true;
  }
  assert(dom->json_type() == enum_json_type::J_OBJECT);

  std::vector<Single_object_binding> bindings;
  bindings.reserve(BINDINGS_RESERVE_SIZE);
  bindings.push_back({down_cast<Json_object *>(dom), ct_node,
                      std::make_unique<Resolve_row>()});

  if (flatten(&bindings)) {
    return true;
  }

#ifndef NDEBUG
  for ([[maybe_unused]] const auto &bin : bindings) {
    assert(bin.ct_node->is_root_object() || bin.ct_node->id() != 0);
    DBUG_LOG("jdv_dml", "DML-INSERT: Flattened order: " << *bin.ct_node);
  }
#endif

  if (std::ranges::any_of(bindings, [&](const auto &bin) {
        return check_for_unmatched_input_keys(thd, bin.bound_object,
                                              *bin.ct_node);
      })) {
    return true;
  }

  if (resolve_columns(bindings)) {
    return true;
  }

  auto rank_index = make_mr_index<const Single_object_binding>(thd, bindings);

  // Sort the binding index so that
  // - statements are executed in the correct order (so that FK relationships
  // are satisfied)
  // - It becomes possible to use algorithms such as unique and adjacent_find
  // (which operate on sorted ranges)
  std::ranges::sort(rank_index);

  DBUG_LOG("jdv_dml", "DML-INSERT: Rank-sorted order on INDEX: ");
#ifndef NDEBUG
  for (const Index_entry<const Single_object_binding> &ie : rank_index) {
    const Single_object_binding &bin = ie.get();
    DBUG_LOG("jdv_dml", "Path:" << em_wrap(bin.bound_object->get_location())
                                << " node:" << *bin.ct_node);
  }
#endif /* NDEBUG */

  if (check_for_share_pk_duplicates(thd, bindings)) {
    return true;
  }

  return do_in_substatement_context(thd, [&](Diagnostics_area *caller_da) {
    std::string stmt;
    return std::ranges::any_of(
        rank_index, [&](const Index_entry<const Single_object_binding> &ie) {
          const Single_object_binding &bin = ie.get();
          if (bin.is_empty()) {
            return false;
          }
          stmt.resize(0);
          auto res = make_insert(thd, bin, &stmt);
          DBUG_LOG("jdv_dml", "DML-INSERT: Insert from node "
                                  << *bin.ct_node << " using '" << stmt << "'");

          if (res == Stmt_state::ERROR) {
            return true;
          }
          if (res == Stmt_state::SKIP) {
            return false;
          }

          bool do_fetch_last_insert_id =
              (bin.resolve_row->unresolved_autoinc_column != nullptr);

#ifndef NDEBUG
          const auto &tr = *bin.ct_node->table_ref();

          if (do_fetch_last_insert_id) {
            DBUG_LOG("jdv_dml",
                     "AINC: (pre-insert) "
                         << JX_(tr.table_name)
                         << JX_(thd->first_successful_insert_id_in_prev_stmt));
          }
#endif /* NDEBUG */
          if (run_substmt(thd, caller_da, stmt, affected_rows)) return true;

          if (do_fetch_last_insert_id) {
            ::new (&bin.resolve_row->autoinc_value)
                Json_uint{thd->first_successful_insert_id_in_prev_stmt};
            Json_uint &last_insert_id = bin.resolve_row->autoinc_value;
            bin.resolve_row->unresolved_autoinc_column->value = &last_insert_id;

            DBUG_LOG("jdv_dml",
                     "AINC: (post-insert) Unresolved autoinc value for "
                         << JX_(tr.table_name) << "resolved to "
                         << JX_(bin.resolve_row->autoinc_value.value())
                         << "(thd->first_successful_insert_id_in_prev_stmt) "
                         << JX_(json_dom_to_string(&last_insert_id)));
            if (resolve_column_range(bindings)) {
              return true;
            }
          }

          return false;
        });
  });
}

/**
  Creates bindings from existing and updated Json_object,
  orders them, creates and executes UPDATE/INSERT/DELETE
  statements against the base tables.

  @param thd THD
  @param jw updated JSON object
  @param existing existing JSON object
  @param ct_node content tree root node
  @param[out] affected_rows affected base table rows
 */
[[nodiscard]] static bool jdv_handle_update(THD *thd, Json_wrapper *jw,
                                            Json_wrapper *existing,
                                            Content_tree_node *ct_node,
                                            ulonglong *affected_rows) {
  assert(jw != nullptr);
  assert(existing != nullptr);
  assert(ct_node != nullptr);
  Json_dom *dom = jw->to_dom();
  assert(dom != nullptr);
  if (check_input_json(dom)) {
    return true;
  }
  assert(dom->json_type() == enum_json_type::J_OBJECT);

  std::vector<Two_object_binding> bindings;
  bindings.reserve(BINDINGS_RESERVE_SIZE);
  bindings.push_back({down_cast<Json_object *>(dom),
                      down_cast<Json_object *>(existing->to_dom()), ct_node,
                      std::make_unique<Resolve_row>()});

  if (check_etag(bindings.front())) {
    return true;
  }

  if (flatten(&bindings)) {
    return true;
  }

#ifndef NDEBUG
  for ([[maybe_unused]] const auto &bin : bindings) {
    assert(bin.ct_node->is_root_object() || bin.ct_node->id() != 0);
    DBUG_LOG("jdv_dml", "DML-UPDATE: Flattened order: " << *bin.ct_node);
  }
#endif

  if (std::ranges::any_of(bindings, [&](const auto &bin) {
        return (bin.bound_object != nullptr &&
                check_for_unmatched_input_keys(thd, bin.bound_object,
                                               *bin.ct_node));
      })) {
    return true;
  }

  if (resolve_columns(bindings)) {
    return true;
  }

  // Need a non-const index here, as we will need to modify bindings during
  // merging
  auto rank_index = make_mr_index<Two_object_binding>(thd, bindings);

  // Sort the index so that
  // - statements are executed in the correct order (so that FK relationships
  // are satisfied)
  // - It becomes possible to use algorithms such as unique and adjacent_find
  // (which operate on sorted ranges)
  // - We can correctly merge together bindings that represent a single update.
  std::ranges::sort(rank_index);

  DBUG_LOG(
      "jdv_dml", "__DML-UPDATE: Rank-sorted order:" << [&]() {
        for (const auto &ie : rank_index) {
          sout << "\n" << ie.get();
        }
        return "";
      }());

  merge_bindings_for_update(rank_index);

  if (check_for_share_pk_duplicates(thd, bindings)) {
    return true;
  }

  return do_in_substatement_context(thd, [&](Diagnostics_area *caller_da) {
    std::string stmt;
    return std::ranges::any_of(
        rank_index, [&](const Index_entry<Two_object_binding> &ie) {
          const Two_object_binding &bin = ie.get();
          if (bin.is_empty()) {
            return false;
          }
          stmt.resize(0);
          auto res = make_update(thd, bin, &stmt);
          DBUG_LOG("jdv_dml", "DML-UPDATE: Update from node '"
                                  << *bin.ct_node << "'"
                                  << " using '" << stmt << "'");

          return res == Stmt_state::ERROR ||
                 (res != Stmt_state::SKIP &&
                  run_substmt(thd, caller_da, stmt, affected_rows));
        });
  });
}

/**
  Creates bindings from deleted Json_object,
  orders them, creates and executes DELETE statements against the base tables.

  @param thd THD
  @param jw existing JSON object to delete
  @param ct_node content tree root node
  @param[out] affected_rows affected base table rows
 */
[[nodiscard]] static bool jdv_handle_delete(THD *thd, Json_wrapper *jw,
                                            Content_tree_node *ct_node,
                                            ulonglong *affected_rows) {
  assert(jw != nullptr);
  Json_dom *dom = jw->to_dom();
  assert(dom != nullptr);
  assert(dom->json_type() == enum_json_type::J_OBJECT);

  std::vector<Single_object_binding> bindings;
  bindings.push_back({down_cast<Json_object *>(dom), ct_node,
                      std::make_unique<Resolve_row>()});

  if (flatten(&bindings)) {
    return true;
  }
  if (resolve_columns(bindings)) {
    return true;
  }
  return do_in_substatement_context(thd, [&](Diagnostics_area *caller_da) {
    std::string stmt;
    return std::ranges::any_of(
        std::ranges::reverse_view{bindings},
        [&](const Single_object_binding &bin) {
          stmt.resize(0);
          auto res = make_delete(thd, bin, &stmt);
          DBUG_LOG("jdv_dml", "DML-DELETE: Delete from node '"
                                  << *bin.ct_node << "'"
                                  << " using '" << stmt << "'");

          return res == Stmt_state::ERROR ||
                 (res != Stmt_state::SKIP &&
                  run_substmt(thd, caller_da, stmt, affected_rows));
        });
  });
}

/**
  Performs common sanity checks.

  @param thd THD
  @param view JDV to check
  @param is_single_table_plan true if single table plan
  @return true if error
 */
[[nodiscard]] static bool jdv_prepare_base(THD *thd, const Table_ref *view,
                                           bool is_single_table_plan) {
  // LOW PRIORITY is not supported.
  if (view->lock_descriptor().type == TL_WRITE_LOW_PRIORITY) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0), "LOW_PRIORITY modifier");
    return true;
  }

  // IGNORE
  if (thd->lex->is_ignore()) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0), "IGNORE modifier");
    return true;
  }

  // Is Multi-table operation ?
  if (!is_single_table_plan) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0),
             "Multi-table DML operation");
    return true;
  }

  return false;
}

// API

/**
  Performs sanity checks specific to insert.

  @param thd THD
  @param view JDV to check
  @param sql_insert_cmd insert command object
  @return true if error
 */
bool jdv_prepare_insert(THD *thd, const Table_ref *view,
                        Sql_cmd_insert_base *sql_insert_cmd) {
  assert(view->is_json_duality_view());
  if (!view->is_json_duality_view()) {
    return false;
  }

  // REPLACE statement is not supported.
  if (sql_insert_cmd->duplicates == DUP_REPLACE) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0), "REPLACE");
    return true;
  }

  // INSERT... ON DUPLICATE KEY UPDATE is not supported.
  if (sql_insert_cmd->duplicates == DUP_UPDATE) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0),
             "INSERT... ON DUPLICATE KEY UPDATE");
    return true;
  }

  // INSERT... SELECT is not supported.
  const bool select_insert = sql_insert_cmd->insert_many_values.empty();
  if (select_insert) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0), "INSERT... SELECT");
    return true;
  }

  // HIGH PRIORITY is not supported.
  if (view->lock_descriptor().type == TL_WRITE) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0), "INSERT HIGH_PRIORITY");
    return true;
  }

  // Column list (i.e. column "data") is not supported.
  if (sql_insert_cmd->insert_field_list.size() > 0) {
    my_error(ER_JDV_OPERATION_NOT_SUPPORTED, MYF(0), "Column list in INSERT");
    return true;
  }

  if (jdv_prepare_base(thd, view, sql_insert_cmd->is_single_table_plan())) {
    return true;
  }

  return false;
}

/**
  Performs sanity checks specific to update.

  @param thd THD
  @param view JDV to check
  @param is_single_table_plan true if single table plan
  @return true if error
 */
bool jdv_prepare_update(THD *thd, const Table_ref *view,
                        bool is_single_table_plan) {
  if (view->query_block->order_list.first != nullptr) {
    my_jdv_error<ER_JDV_OPERATION_NOT_SUPPORTED>("UPDATE with ORDER BY clause");
    return true;
  }

  // LIMIT without ORDER BY is meaningless and disallowing is consistent
  // with DELETE.
  DBUG_LOG("jdv_dml", "DML-UPDATE: "
                          << " vqb_limit:"
                          << view->query_block->get_limit(thd));
  if (view->query_block->get_limit(thd) != HA_POS_ERROR) {
    my_jdv_error<ER_JDV_OPERATION_NOT_SUPPORTED>("UPDATE with LIMIT clause");
    return true;
  }

  // Mark the root object table's columns for read here. For sub-objects
  // table, columns are already marked during resolve stage by
  // Item_subselect::fix_fields().
  //
  // TODO: Find a better way to handle read_set marking for root object
  //       table columns
  const auto content_tree = view->jdv_content_tree;
  for (const auto &kci : *content_tree->key_column_info_list()) {
    auto field = kci.field();
    bitmap_set_bit(field->table->read_set, field->field_index());
  }

  return jdv_prepare_base(thd, view, is_single_table_plan);
}

/**
  Performs sanity checks specific to delete.

  @param thd THD
  @param view JDV to check
  @param is_single_table_plan true if single table plan
  @return true if error
 */

bool jdv_prepare_delete(THD *thd, const Table_ref *view,
                        bool is_single_table_plan) {
  if (view->query_block->order_list.first != nullptr) {
    my_jdv_error<ER_JDV_OPERATION_NOT_SUPPORTED>("DELETE with ORDER BY clause");
    return true;
  }

  // LIMIT without WHERE does not work with JDVs since the rows will not be
  // fetched so there is no way to generate statements against the base table.
  // (LIMIT n for n > 1 would not work anyway since we multi-object deletes are
  // not allowed. LIMIT 1 with WHERE could be made to work, but as long as
  // ORDER BY is not supported it makes little sense).
  DBUG_LOG("jdv_dml",
           "DML-DELETE: vqb_limit:" << view->query_block->get_limit(thd));

  if (view->query_block->get_limit(thd) != HA_POS_ERROR) {
    my_jdv_error<ER_JDV_OPERATION_NOT_SUPPORTED>("DELETE with LIMIT clause");
    return true;
  }

  return jdv_prepare_base(thd, view, is_single_table_plan);
}

/**
  Installs the view's security context if it exists and creates a scope
  guard to restore the original context.

  @param thd THD
  @param dvtr duality view reference

  @return scope guard which restores original security context
*/
[[nodiscard]] static decltype(auto) create_sctx_guard(THD *thd,
                                                      const Table_ref *dvtr) {
  Security_context *orig_sctx = thd->security_context();
  if (dvtr->view_sctx != nullptr) {
    thd->set_security_context(dvtr->view_sctx);
  }

  return create_scope_guard([=] { thd->set_security_context(orig_sctx); });
}

/**
  Performs statement-based binlogging for DML operations on a JSON Duality view.

  @param thd THD
  @return true if error
*/
static bool write_binlog(THD *thd) {
  /*
    If row-based binlogging, we don't need to binlog the function's call, let
    each substatement be binlogged its way.
  */
  bool need_binlog_call = mysql_bin_log.is_open() &&
                          (thd->variables.option_bits & OPTION_BIN_LOG) &&
                          !thd->is_current_stmt_binlog_format_row();
  if (!need_binlog_call) {
    return false;
  }

  if (thd->binlog_evt_union.unioned_events) {
    int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
    Query_log_event qinfo(thd, thd->query().str, thd->query().length,
                          thd->binlog_evt_union.unioned_events_trans, false,
                          false, errcode);
    if (mysql_bin_log.write_event(&qinfo)) {
      push_warning(thd, Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                   "Failed to write event to binary log.");
      return true;
    }
  }

  return false;
}

/**
  Entry point called from sql_insert.cc,
  bool Sql_cmd_insert_values::execute_inner(THD *thd);

  @param thd    Thread handle.
  @param dvtr   Table_ref instance of a duality view.
  @param values Values.

  @return true if error
 */
bool jdv_insert(THD *thd, const Table_ref *dvtr,
                const mem_root_deque<List_item *> &values) {
  auto scg = create_sctx_guard(thd, dvtr);
  if (values.size() > 1) {
    my_jdv_error<ER_JDV_OPERATION_NOT_SUPPORTED>("Multiple object insert");
    return true;
  }

  assert(dvtr->jdv_content_tree != nullptr);
  auto &content_tree = *(dvtr->jdv_content_tree);

  DBUG_LOG("jdv_dml", "DML-INSERT: stmt:'" << thd->query().str << "'");

  ulonglong affected_rows = 0;
  for (List_item *li : values) {
    if (li->empty()) {
      my_jdv_error<ER_WRONG_VALUE_COUNT_ON_ROW>(1);
      return true;
    }

    for (Item *itm : *li) {
      if (!itm->fixed) {
        my_jdv_error<ER_JDV_INSERT_VALUE_NOT_FIXED>();
        return true;
      }
      Json_wrapper jw;
      String buf;
      if (get_json_wrapper(&itm, 0, &buf, "get_json_wrapper", &jw)) {
        DBUG_LOG("jdv_dml", "DML-INSERT item data type:"
                                << itm->data_type()
                                << ", error from get_json_wrapper(): "
                                << thd->get_stmt_da()->message_text());
        return true;
      }
      if (jw.empty()) {
        DBUG_LOG("jdv_dml", "DML-INSERT item data type:"
                                << itm->data_type()
                                << ", produced an empty json wrapper.");

        my_jdv_error<ER_JDV_NULL_INSERT_VALUE>();
        return true;
      }
      DBUG_LOG("jdv_dml",
               "DML-INSERT: new value: " << json_wrapper_to_string(jw));
      if (jdv_handle_insert(thd, &jw, &content_tree, &affected_rows)) {
        return true;
      }
    }
  }

  if (write_binlog(thd)) return true;

  char buff[MYSQL_ERRMSG_SIZE];
  std::ignore = snprintf(
      buff, sizeof(buff), ER_THD(thd, ER_JDV_DML_INFO),
      static_cast<long>(affected_rows),
      static_cast<long>(thd->get_stmt_da()->current_statement_cond_count()));
  my_ok(thd, affected_rows, 0, buff);

  return false;
}

/**
  Entry point called from sql_update.cc
  bool Sql_cmd_update::update_single_table(THD *thd);

  @param thd THD
  @param dvtr duality view
  @param seldq selected items
  @param upddq updated items
  @param[out]  affected_rows  Number of affected rows.
  @return true if error
*/
bool jdv_update(THD *thd, const Table_ref *dvtr,
                const mem_root_deque<Item *> *seldq,
                const mem_root_deque<Item *> *upddq, ulonglong *affected_rows) {
  auto scg = create_sctx_guard(thd, dvtr);
  DBUG_LOG("jdv_dml", "DML-UPDATE: for query:'" << thd->query().str << "'");

  assert(dvtr != nullptr && dvtr->is_json_duality_view() &&
         dvtr->jdv_content_tree != nullptr);
  auto *content_tree = dvtr->jdv_content_tree;

  assert(seldq != nullptr && seldq->size() == 1);
  Item *sel_itm = seldq->front();
  assert(sel_itm->data_type() == enum_field_types::MYSQL_TYPE_JSON);
  assert(sel_itm->type() == Item::REF_ITEM);
  Item *itm = sel_itm;
  for (; itm->type() == Item::REF_ITEM;
       itm = down_cast<Item_ref *>(itm)->ref_item()) {
  }
  assert(itm->type() == Item::FUNC_ITEM);

  auto *sel_func_item = down_cast<Item_func *>(itm);
  assert(sel_func_item == dvtr->field_translation->item);
  Json_wrapper sel_jw;
  if (sel_func_item->val_json(&sel_jw)) {
    return true;
  }
  DBUG_LOG("jdv_dml",
           "DML-UPDATE: selected data: " << json_wrapper_to_string(sel_jw));

  assert(upddq != nullptr && upddq->size() == 1);
  Item *upd_itm = upddq->front();
  if (upd_itm->null_value) {
    my_jdv_error<ER_JDV_NULL_UPDATE_VALUE>();
    return true;
  }

  for (; upd_itm->type() == Item::REF_ITEM;
       upd_itm = down_cast<Item_ref *>(upd_itm)->ref_item()) {
    DBUG_LOG("jdv_dml", "DML-UPDATE: Stripping ref-layer from upd-item");
  }

  if (!upd_itm->fixed) {
    my_jdv_error<ER_JDV_UPDATE_VALUE_NOT_FIXED>();
    return true;
  }

  Json_wrapper upd_jw;
  String buf;
  if (get_json_wrapper(&upd_itm, 0, &buf, "get_json_wrapper", &upd_jw)) {
    DBUG_LOG("jdv_dml", "DML-UPDATE: item data type:" << upd_itm->data_type());
    return true;
  }
  DBUG_LOG("jdv_dml",
           "DML-UPDATE: set data to: " << json_wrapper_to_string(upd_jw));

  if (jdv_handle_update(thd, &upd_jw, &sel_jw, content_tree, affected_rows)) {
    assert(thd->is_error());
    return true;
  }

  if (write_binlog(thd)) return true;

  return false;
}

/**
  Entry point called from sql_delete.cc,
  bool Sql_cmd_delete::delete_from_single_table(THD *thd);

  @param thd THD
  @param dvtr duality view
  @param[out] affected_rows  Number of affected rows.
  @return true if error
*/
bool jdv_delete(THD *thd, const Table_ref *dvtr, ulonglong *affected_rows) {
  auto scg = create_sctx_guard(thd, dvtr);
  DBUG_LOG("jdv_dml", "DML-DELETE:  for query:'" << thd->query().str);
  Item *field_xlation = dvtr->field_translation->item;

  Json_wrapper fldx_jw;
  String fldx_buf;
  if (get_json_wrapper(&field_xlation, 0, &fldx_buf, "get_json_wrapper",
                       &fldx_jw)) {
    DBUG_LOG("jdv_dml", "DML-DELETE: data type:" << field_xlation->data_type());
    return true;
  }
  DBUG_LOG("jdv_dml", "DML-DELETE: field_translation item = "
                          << json_wrapper_to_string(fldx_jw));

  assert(dvtr != nullptr && dvtr->is_json_duality_view() &&
         dvtr->jdv_content_tree != nullptr);

  if (jdv_handle_delete(thd, &fldx_jw, dvtr->jdv_content_tree, affected_rows)) {
    assert(thd->is_error());
    return true;
  }

  if (write_binlog(thd)) return true;

  return false;
}

}  // namespace jdv

// Functions to allow unit testing of static functions
namespace jdv_unit {

/**
  Uses debug ostream operators to generate a string which can be checked in unit
  tests. This is primarily to get coverage of code only used in debugging which
  is not triggered during normal testing.

  @return a string containing the result of applying ostream operators.
 */
std::string test_ostream_operators() {
  jdv::Key_column_info kci;
  jdv::Resolve_column rc;
  rc.kci = &kci;
  jdv::Resolve_row rr;

  jdv::Content_tree_node node;
  node.set_primary_key_column_index(0);
  auto jo = Json_object{};

  std::stringstream ss;
  ss << "empty:"
     << jdv::Single_object_binding{.bound_object = nullptr,
                                   .ct_node = &node,
                                   .resolve_row = {}}
     << "\n";

  jdv::Single_object_binding sbin = {
      .bound_object = &jo,
      .ct_node = &node,
      .resolve_row = std::make_unique<jdv::Resolve_row>()};
  ss << "unresolved:" << sbin << "\n";
  sbin.resolve_row->columns.push_back(rc);
  ss << "resolved:" << sbin << "\n";

  // Two_object_binding
  ss << "empty:"
     << jdv::Two_object_binding{.bound_object = nullptr,
                                .ct_node = &node,
                                .resolve_row = {}}
     << "\n";
  jdv::Two_object_binding tbin = {
      .bound_object = &jo,
      .ct_node = &node,
      .resolve_row = std::make_unique<jdv::Resolve_row>()};

  ss << "unresolved:" << tbin << "\n";
  tbin.resolve_row->columns.push_back(rc);
  ss << "resolved:" << tbin;

  return ss.str();
}
}  // namespace jdv_unit
