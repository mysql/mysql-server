/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#ifndef OPT_EXPLAIN_FORMAT_INCLUDED
#define OPT_EXPLAIN_FORMAT_INCLUDED

/**
  @file sql/opt_explain_format.h
  EXPLAIN FORMAT=@<format@> @<command@>.
*/

#include <assert.h>
#include <sys/types.h>

#include <cstring>
#include <optional>
#include <string_view>

#include "my_alloc.h"  // MEM_ROOT
#include "my_compiler.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "sql/parse_tree_node_base.h"
#include "sql/sql_list.h"
#include "sql_string.h"

class Opt_trace_object;
class Query_result;
class Query_expression;
class Window;
class Json_object;

enum class enum_explain_type;

/**
  Types of traditional "extra" column parts and property names for hierarchical

  The traditional_extra_tags[] and json_extra_tags[] arrays must be in sync
  with this enum.
*/
enum Extra_tag {
  ET_none,
  ET_USING_TEMPORARY,
  ET_USING_FILESORT,
  ET_USING_INDEX_CONDITION,
  ET_USING,
  ET_RANGE_CHECKED_FOR_EACH_RECORD,
  ET_USING_PUSHED_CONDITION,
  ET_USING_WHERE,
  ET_NOT_EXISTS,
  ET_USING_MRR,
  ET_USING_INDEX,
  ET_FULL_SCAN_ON_NULL_KEY,
  ET_USING_INDEX_FOR_GROUP_BY,
  ET_USING_INDEX_FOR_SKIP_SCAN,
  ET_DISTINCT,
  ET_LOOSESCAN,
  ET_START_TEMPORARY,
  ET_END_TEMPORARY,
  ET_FIRST_MATCH,
  ET_MATERIALIZE,
  ET_START_MATERIALIZE,
  ET_END_MATERIALIZE,
  ET_SCAN,
  ET_USING_JOIN_BUFFER,
  ET_CONST_ROW_NOT_FOUND,
  ET_UNIQUE_ROW_NOT_FOUND,
  ET_IMPOSSIBLE_ON_CONDITION,
  ET_PUSHED_JOIN,
  ET_FT_HINTS,
  ET_BACKWARD_SCAN,
  ET_RECURSIVE,
  ET_TABLE_FUNCTION,
  ET_SKIP_RECORDS_IN_RANGE,
  ET_USING_SECONDARY_ENGINE,
  ET_REMATERIALIZE,
  //------------------------------------
  ET_total
};

/**
  Emulate lazy computation
*/
class Lazy {
 public:
  virtual ~Lazy() = default;

  /**
    Deferred evaluation of encapsulated expression

    @param [out] ret    Return string value

    @retval false       Success
    @retval true        Failure (OOM)
  */
  virtual bool eval(String *ret) = 0;
};

/**
  Base class for all EXPLAIN context descriptor classes

  In structured EXPLAIN implementation Explain_context is a base class for
  notes of an intermediate tree.
*/
struct Explain_context {
  enum_parsing_context type;  ///< type tag

  explicit Explain_context(enum_parsing_context type_arg) : type(type_arg) {}
};

namespace opt_explain_json_namespace  // for forward declaration of "context"
{
class context;
}

// Table modification type
enum enum_mod_type { MT_NONE, MT_INSERT, MT_UPDATE, MT_DELETE, MT_REPLACE };

/**
  Helper class for table property buffering

  For traditional EXPLAIN this structure contains cached data for a single
  output row.

  For hierarchical EXPLAIN this structure contains property values for a single
  CTX_TABLE/CTX_QEP_TAB context node of the intermediate tree.
*/

class qep_row {
 private:
  /* Don't copy this structure */
  explicit qep_row(const qep_row &x);    // undefined
  qep_row &operator=(const qep_row &x);  // undefined

 public:
  /**
    A wrapper for numeric table properties

    For traditional EXPLAIN this structure contains a value of one cell of the
    output row (excluding textual column values - see mem_root_str, and
    "Extra" column - see the col_extra list).

    For hierarchical EXPLAIN this structure contains a numeric property value
    for a single CTX_TABLE/CTX_QEP_TAB context node of the intermediate tree.
  */
  template <typename T>
  struct column {
   private:
    bool nil;  ///< true if the column contains NULL
   public:
    T value;

   public:
    column() { cleanup(); }
    bool is_empty() const { return nil; }
    void cleanup() { nil = true; }
    void set(T value_arg) {
      value = value_arg;
      nil = false;
    }
    T get() const {
      assert(!nil);
      return value;
    }
  };

  /**
    Helper class to keep string data in MEM_ROOT before passing to Item_string

    Since Item_string constructors doesn't copy input string parameter data
    in the most cases, those input strings must have the same lifetime as
    Item_string objects, i.e. lifetime of MEM_ROOT.
    This class allocates input parameters for Item_string objects in MEM_ROOT.

    @note Call to is_empty() is necessary before the access to "str" and
          "length" fields, since is_empty() may trigger an evaluation of
          an associated expression that updates these fields.
  */
  struct mem_root_str {
    const char *str;
    size_t length;
    Lazy *
        deferred;  ///< encapsulated expression to evaluate it later (on demand)

    mem_root_str() { cleanup(); }
    void cleanup() {
      str = nullptr;
      length = 0;
      deferred = nullptr;
    }
    bool is_empty();
    bool set(const char *str_arg) { return set(str_arg, strlen(str_arg)); }
    bool set(const String &s) { return set(s.ptr(), s.length()); }
    /**
      Make a copy of the string in MEM_ROOT

      @param str_arg    string to copy
      @param length_arg input string length

      @return false if success, true if error
    */
    bool set(const char *str_arg, size_t length_arg);

    /**
      Save expression for further evaluation

      @param x  Expression
    */
    void set(Lazy *x) {
      deferred = x;
      str = nullptr;
      length = 0;
    }
    /**
      Make a copy of string constant

      Variant of set() usable when the str_arg argument lives longer
      than the mem_root_str instance.
    */
    void set_const(const char *str_arg) {
      return set_const(str_arg, strlen(str_arg));
    }
    void set_const(const char *str_arg, size_t length_arg) {
      deferred = nullptr;
      str = str_arg;
      length = length_arg;
    }

    static char *strndup_root(MEM_ROOT *root, const char *str, size_t len) {
      if (len == 0 || str == nullptr) return const_cast<char *>("");
      if (str[len - 1] == 0)
        return static_cast<char *>(memdup_root(root, str, len));

      char *ret = static_cast<char *>(root->Alloc(len + 1));
      if (ret != nullptr) {
        memcpy(ret, str, len);
        ret[len] = 0;
      }
      return ret;
    }
  };

  /**
    Part of traditional "extra" column or related hierarchical property
  */
  struct extra {
    /**
      A property name or a constant text head of the "extra" column part
    */
    const Extra_tag tag;
    /**
      Property value or a variable tail of the "extra" column part

      If data == NULL, hierarchical formatter outputs a boolean property
      value of "true".
    */
    const char *const data;

    explicit extra(Extra_tag tag_arg, const char *data_arg = nullptr)
        : tag(tag_arg), data(data_arg) {}
  };

  /*
    Next "col_*" fields are intended to be filling by "explain_*()" functions.

    NOTE: NULL value or mem_root_str.is_empty()==true means that Item_null
    object will be pushed into "items" list instead.
  */
  column<uint> col_id;  ///< "id" column: seq. number of SELECT within the query
  column<enum_explain_type> col_select_type;  ///< "select_type" column
  mem_root_str col_table_name;  ///< "table" to which the row of output refers
  List<const char> col_partitions;  ///< "partitions" column
  mem_root_str col_join_type;       ///< "type" column, see join_type_str array
  List<const char>
      col_possible_keys;  ///< "possible_keys": comma-separated list
  mem_root_str
      col_key;  ///< "key" column: index that is actually decided to use
  mem_root_str col_key_len;  ///< "key_length" column: length of the "key" above
  List<const char>
      col_ref;  ///< "ref":columns/constants which are compared to "key"
  column<float> col_filtered;  ///< "filtered": % of rows filtered by condition
  List<extra> col_extra;  ///< "extra" column (traditional) or property list

  // non-TRADITIONAL stuff:
  mem_root_str col_message;  ///< replaces "Extra" column if not empty
  mem_root_str col_attached_condition;  ///< former "Using where"

  /// "rows": estimated number of examined table rows per single scan
  column<ulonglong> col_rows;
  /// "rows": estimated number of examined table rows per query
  column<ulonglong> col_prefix_rows;

  column<double> col_read_cost;  ///< Time to read the table
  /// Cost of the partial join including this table
  column<double> col_prefix_cost;
  /// Cost of evaluating conditions on this table per query
  column<double> col_cond_cost;

  /// Size of data expected to be read  per query
  mem_root_str col_data_size_query;

  /// List of used columns
  List<const char> col_used_columns;

  /// List of columns that can be updated using partial update.
  List<const char> col_partial_update_columns;

  /* For structured EXPLAIN in CTX_QEP_TAB context: */
  uint query_block_id;  ///< query block id for materialized subqueries

  /**
    List of "derived" subquery trees
  */
  List<opt_explain_json_namespace::context> derived_from;

  List<const char> col_key_parts;  ///< used parts of the key

  bool is_dependent;
  bool is_cacheable;
  bool using_temporary;
  enum_mod_type mod_type;
  bool is_materialized_from_subquery;
  /**
     If a clone of a materialized derived table, this is the ID of the first
     underlying query block of the first materialized derived table. 0
     otherwise.
  */
  uint derived_clone_id;

  List<Window> *m_windows;  ///< Windows to describe in this node

  qep_row()
      : query_block_id(0),
        is_dependent(false),
        is_cacheable(true),
        using_temporary(false),
        mod_type(MT_NONE),
        is_materialized_from_subquery(false),
        derived_clone_id(0),
        m_windows(nullptr) {}

  virtual ~qep_row() = default;

  void cleanup() {
    col_id.cleanup();
    col_table_name.cleanup();
    col_partitions.clear();
    col_join_type.cleanup();
    col_possible_keys.clear();
    col_key.cleanup();
    col_key_len.cleanup();
    col_ref.clear();
    col_filtered.cleanup();
    col_extra.clear();
    col_message.cleanup();
    col_attached_condition.cleanup();
    col_key_parts.clear();

    col_rows.cleanup();
    col_prefix_rows.cleanup();

    col_read_cost.cleanup();
    col_prefix_cost.cleanup();
    col_cond_cost.cleanup();

    col_data_size_query.cleanup();

    /*
      Not needed (we call cleanup() for structured EXPLAIN only,
      just for the consistency).
    */
    query_block_id = 0;
    derived_from.clear();
    is_dependent = false;
    is_cacheable = true;
    using_temporary = false;
    mod_type = MT_NONE;
    is_materialized_from_subquery = false;
  }

  /**
    Remember a subquery's unit

    JOIN_TAB inside a JOIN, a table in a join-less query (single-table
    UPDATE/DELETE) or a table that's optimized out may have a WHERE
    condition. We create the Explain_context of such a JOIN_TAB or
    table when the Explain_context objects of its in-WHERE subqueries
    don't exist.
    This function collects unit pointers of WHERE subqueries that are
    associated with the current JOIN_TAB or table. Then we can match these
    units with units of newly-created Explain_context objects of WHERE
    subqueries.

    @param subquery     WHERE clause subquery's unit
  */
  virtual void register_where_subquery(Query_expression *subquery
                                       [[maybe_unused]]) {}

  void format_extra(Opt_trace_object *obj);
};

/**
  Enumeration of ORDER BY, GROUP BY and DISTINCT clauses for array indexing

  See Explain_format_flags::sorts
*/
enum Explain_sort_clause {
  ESC_none = 0,
  ESC_ORDER_BY = 1,
  ESC_GROUP_BY = 2,
  ESC_DISTINCT = 3,
  ESC_BUFFER_RESULT = 4,
  ESC_WINDOWING = 5,
  //-----------------
  ESC_MAX
};

/**
  Bit flags to explain GROUP BY, ORDER BY and DISTINCT clauses
*/
enum Explain_sort_property {
  ESP_none = 0,
  ESP_EXISTS = 1 << 0,     ///< Original query has this clause
  ESP_IS_SIMPLE = 1 << 1,  ///< Clause is effective for single JOIN_TAB only
  ESP_USING_FILESORT = 1 << 2,  ///< Clause causes a filesort
  ESP_USING_TMPTABLE = 1 << 3,  ///< Clause creates an intermediate table
  ESP_DUPS_REMOVAL = 1 << 4     ///< Duplicate removal for DISTINCT
};

class Explain_format_flags {
  /**
    Bitmasks of Explain_sort_property flags for Explain_sort_clause clauses
  */
  uint8 sorts[ESC_MAX];

 public:
  Explain_format_flags() { memset(sorts, 0, sizeof(sorts)); }

  /**
    Set property bit flag for the clause
  */
  void set(Explain_sort_clause clause, Explain_sort_property property) {
    sorts[clause] |= property | ESP_EXISTS;
  }

  void set(Explain_format_flags &flags) {
    memcpy(sorts, flags.sorts, sizeof(sorts));
  }

  /**
    Clear property bit flag for the clause
  */
  void reset(Explain_sort_clause clause, Explain_sort_property property) {
    sorts[clause] &= ~property;
  }

  /**
    Return true if property is set for the clause
  */
  bool get(Explain_sort_clause clause, Explain_sort_property property) const {
    return sorts[clause] & property;
  }

  /**
    Return true if any of clauses has this property set

    @param property Check if this property is present in any of the sorts
           except clause's sort if specified
    @param clause Optional. Do not check for the property for this clause. The
           default is to check all clauses.
  */
  bool any(Explain_sort_property property,
           Explain_sort_clause clause = ESC_none) const {
    for (size_t i = ESC_none + 1; i <= ESC_MAX - 1; i++) {
      if (i != clause && (sorts[i] & property)) return true;
    }
    return false;
  }
};

/**
  Base class for structured and hierarchical EXPLAIN output formatters
*/

class Explain_format {
 private:
  /* Don't copy Explain_format values */
  Explain_format(Explain_format &);             // undefined
  Explain_format &operator=(Explain_format &);  // undefined

 protected:
  Query_result *output;  ///< output resulting data there

 private:
  std::optional<std::string_view> m_explain_into_variable_name;

 public:
  /* Which schema this EXPLAIN statement should be run for. */
  LEX_CSTRING m_schema_name_for_explain;

 protected:
  Explain_format() : output(nullptr) {}
  explicit Explain_format(
      std::optional<std::string_view> explain_into_variable_name)
      : output(nullptr),
        m_explain_into_variable_name(explain_into_variable_name) {}

 public:
  virtual ~Explain_format() = default;

  /**
    A hierarchical text or a plain table

    @retval true        Formatter produces hierarchical text
    @retval false       Traditional explain
  */
  virtual bool is_hierarchical() const = 0;

  /**
    Whether the format closely resembles the final plan to be executed by
    execution iterators (See RowIterator). These formats share a common logic
    that uses AccessPath structure to generate the information, so they all
    display exactly the same information, even though the style of each format
    might be different.

    @note: The new json format for hypergraph and the tree format are examples
    of iterator-based formats.

    @retval true        Format is Iterator-based.
    @retval false       Format is not Iterator-based.
  */
  virtual bool is_iterator_based(THD *explain_thd [[maybe_unused]],
                                 const THD *query_thd [[maybe_unused]]) const {
    return false;
  }

  /**
   * Whether the output of an EXPLAIN statement should be stored in a user
   * variable or sent to the client. If this function returns true,
   * explain_into_variable_name() returns the name of the variable.
   *
   * @retval true       EXPLAIN output should be stored in a user variable.
   * @retval false      EXPLAIN output should be sent to the client.
   */
  bool is_explain_into() const {
    return m_explain_into_variable_name.has_value();
  }

  /**
   * Whether the EXPLAIN statement should be run in another schema than the
   * current active schema. If this returns true, m_schema_name_for_explain
   * contains the name of the schema to use for EXPLAIN.
   *
   * @return true       The EXPLAIN statement should be run in another schema.
   * @return false      The EXPLAIN statement should be run in the current
   *                    active schema.
   */
  bool is_explain_for_schema() const {
    return m_schema_name_for_explain.length != 0;
  }

  /**
   * Returns the name of the user variable the output of this EXPLAIN
   * statement is to be stored in. Should only be called if this is an
   * EXPLAIN INTO statement.
   *
   * @return std::string_view The name of the variable to store the output in.
   */
  std::string_view explain_into_variable_name() const {
    assert(is_explain_into());
    return m_explain_into_variable_name.value();
  }

  /**
    Send EXPLAIN header item(s) to output stream

    @note: This function caches the output result set pointer for further use.

    @param result       output result set

    @retval false       OK
    @retval true        Error
  */
  virtual bool send_headers(Query_result *result) {
    output = result;
    return false;
  }

  /**
    Enter a specified context

    @param context      context type
    @param subquery     for CTX_WHERE: unit of the subquery
    @param flags        Format flags, see Explain_format_flags.
  */
  virtual bool begin_context(enum_parsing_context context,
                             Query_expression *subquery = nullptr,
                             const Explain_format_flags *flags = nullptr) = 0;

  /**
    Leave the current context

    @param context      current context type (for validation/debugging)
  */
  virtual bool end_context(enum_parsing_context context) = 0;

  /**
    Flush TABLE/JOIN_TAB property set

    For traditional EXPLAIN: output a single EXPLAIN row.
  */
  virtual bool flush_entry() = 0;

  /**
    Get a pointer to the current TABLE/JOIN_TAB property set
  */
  virtual qep_row *entry() = 0;

  /**
    Convert Json object to string. Should only be called for iterator-based
    formats.
  */
  virtual std::string ExplainJsonToString(Json_object *json [[maybe_unused]]) {
    assert(false);
    return nullptr;
  }
};

#endif  // OPT_EXPLAIN_FORMAT_INCLUDED
