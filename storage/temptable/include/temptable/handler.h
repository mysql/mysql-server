/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/handler.h
TempTable public handler API declaration. */

/** @page PAGE_TEMPTABLE TempTable storage engine
The TempTable storage engine is designed to be used by the Optimizer for
creating temporary tables internally when dealing with complex queries.

@subpage PAGE_TEMPTABLE_GLOSSARY

@subpage PAGE_TEMPTABLE_ROW_FORMAT

@page PAGE_TEMPTABLE_GLOSSARY Glossary

Following is a list of the terms used in the TempTable storage engine source
code.

This is to avoid confusion because in the surrounding code (outside of
storage/temptable) different terms are used to designate the one thing and in
some cases a given term can designate different things.

For example some surrounding code reads `key->records_per_key(j)` where
the first "key" (`key->...`) designates an index and the second "key"
(`..._key(j)`) designates distinct indexed cells.

Below are the terms used in TempTable source code with some explanations.

@section TABLE Table

A table consists of rows and columns.

id | color_name | hex_code
-- | ---------- | --------
 1 | Red        | FF0000
 2 | Orange     | FF8800
 3 | Yellow     | FFFF00
 4 | Green      | 00FF00
 5 | Cyan       | 00FFFF
 6 | Blue       | 0000FF
 7 | Pink       | FF00FF

@section ROW Row

A row is a horizontal slice from the table.

id    | color_name | hex_code
----- | ---------- | --------
 1    | Red        | FF0000
 2    | Orange     | FF8800
 @b 3 | @b Yellow  | @b FFFF00
 4    | Green      | 00FF00
 5    | Cyan       | 00FFFF
 6    | Blue       | 0000FF
 7    | Pink       | FF00FF

Also called "record" elsewhere.

@section COLUMN Column

A column is a vertical slice from the table. It has a name - "hex_code" in the
example.

id | color_name | @b hex_code
-- | ---------- | --------
 1 | Red        | @b FF0000
 2 | Orange     | @b FF8800
 3 | Yellow     | @b FFFF00
 4 | Green      | @b 00FF00
 5 | Cyan       | @b 00FFFF
 6 | Blue       | @b 0000FF
 7 | Pink       | @b FF00FF

Also called "field" elsewhere.

@section CELL Cell

A cell is where a row intersects with a column.

id   | color_name | @b hex_code
---- | ---------- | --------
 1   | Red        | @b FF0000
 2   | Orange     | @b FF8800
@b 3 | @b Yellow  | @b @e FFFF00
 4   | Green      | @b 00FF00
 5   | Cyan       | @b 00FFFF
 6   | Blue       | @b 0000FF
 7   | Pink       | @b FF00FF

Also called "field" elsewhere.

@section INDEX Index

An index is a complex structure covering one or more columns.
Also called "key" elsewhere.

@section INDEXED_COLUMN Indexed column

A column that is covered by an index.

@section INDEXED_CELL Indexed cell

An indexed cell is a cell that is covered by an index. An intersection between
a row and an indexed column.
Also called "key", "field", "subkey", "key part", "key segment" elsewhere.
*/

#ifndef TEMPTABLE_HANDLER_H
#define TEMPTABLE_HANDLER_H

#include "sql/handler.h"
#include "sql/table.h"
#include "storage/temptable/include/temptable/storage.h"
#include "storage/temptable/include/temptable/table.h"

namespace temptable {

/** Forward declarations. */
class Block;

/** Temptable engine handler. */
class Handler : public ::handler {
 public:
  /** Constructor. */
  Handler(
      /** [in] Handlerton, saved in `ht` in this class for later usage. */
      handlerton *hton,
      /** [in] Table data that is shared between handlers, saved in
       * `table_share` in this class for later usage. */
      TABLE_SHARE *table_share);

  /** Destructor. */
  ~Handler() override = default;

  /** Create an in-memory table.
   * @return 0 on success or HA_ERR_* error code */
  int create(
      /** [in] Name of the new table. The engine does not try to parse it, so it
       * can be anything as long as it is unique wrt other created tables and as
       * long as the same name is given to the `open()` method. */
      const char *table_name,
      /** [in] New table structure (columns and indexes). Some of the members in
       * `mysql_table->s` are remembered for later usage. */
      TABLE *mysql_table,
      /** [in] Unused. */
      HA_CREATE_INFO *,
      /** [in] Unused. */
      dd::Table *) override;

  /** Delete a table.
   * @return 0 on success or HA_ERR_* error code */
  int delete_table(
      /** [in] Name of the table to delete. */
      const char *table_name,
      /** [in] Unused. */
      const dd::Table *) override;

  /** Open an existing table. A reference to the opened table is kept
   * internally. Only one table can be opened at a time and the read&write
   * methods operate on it.
   * @return 0 on success or HA_ERR_* error code */
  int open(
      /** [in] Name of the table to open. */
      const char *table_name,
      /** [in] Unused. */
      int,
      /** [in] Unused. */
      uint,
      /** [in] Unused. */
      const dd::Table *) override;

  /** Close the opened table.
   * @return 0 on success or HA_ERR_* error code */
  int close() override;

  /** Begin a table scan. The cursor is positioned _before_ the first row in
   * insertion order and subsequent iterations with `rnd_next()` will return
   * the rows in insertion order. `position()` must not be called immediately
   * after `rnd_init()` without a call to `rnd_next()` after `rnd_init()`.
   * @return 0 on success or HA_ERR_* error code */
  int rnd_init(
      /** [in] Unused. */
      bool) override;

  /** Advance the cursor to the next row in insertion order and retrieve it.
   * If no more rows remain in the table, then `HA_ERR_END_OF_FILE` is returned.
   * This method can then be called multiple times and it will keep returning
   * `HA_ERR_END_OF_FILE`. If a new row is inserted after this method has
   * returned `HA_ERR_END_OF_FILE` and this method is called again then it will
   * fetch the newly inserted row.
   * @return 0 on success or HA_ERR_* error code */
  int rnd_next(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row) override;

  /** Fetch the record pointed by `position`.
   * @return 0 on success or HA_ERR_* error code */
  int rnd_pos(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row,
      /** [in] Position pointing to a row. Must be retrieved from
       * `handler::ref` after a call to `position()`. */
      uchar *position) override;

  /** End a table scan. The table scan cursor is invalidated.
   * @return 0 on success or HA_ERR_* error code */
  int rnd_end() override;

  /** Set the index to be used by subsequent `index_*()` calls.
   * @return 0 on success or HA_ERR_* error code */
  int index_init(
      /** [in] index number (0-based). */
      uint index_no,
      /** [in] Unused. */
      bool) override;

  /** Read a row from the currently opened table using the index set with
   * `index_init()`.
   * @return 0 on success or HA_ERR_* error code */
  int index_read(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row,
      /** [in] Concatenated cells that are to be searched for. For example if
       * the table has columns `c1=INT, c2=INT, c3=VARCHAR(16)` and an index
       * on `(c1, c3)` and we are searching for `WHERE c1=5 AND c3='foo'`,
       * then this will contain 5 and 'foo' concatenated. It may also contain
       * a prefix of the indexed columns. */
      const uchar *mysql_search_cells,
      /** [in] The length of `mysql_search_cells` in bytes. */
      uint mysql_search_cells_len_bytes,
      /** [in] Flag denoting how to search for the row. */
      ha_rkey_function find_flag) override;

  /** Advance the index cursor and read the row at that position. Iteration is
   * started by `index_first()` or `index_read()`.
   * @return 0 on success or HA_ERR_* error code */
  int index_next(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row) override;

  /** Advance the index cursor and read the row at that position if its indexed
   * cells are the same as in the current row.
   * @return 0 on success or HA_ERR_* error code */
  int index_next_same(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row,
      /** [in] Unused. */
      const uchar *,
      /** [in] Unused. */
      uint) override;

  /** A condition used by `index_next_conditional()` to control whether to fetch
   * the next row or not. */
  enum class NextCondition {
    /** No condition - fetch the next row unconditionally. */
    NO,
    /** Fetch the next row only if it is the same as the current one. */
    ONLY_IF_SAME,
  };

  /** Advance the index cursor and read the row at that position, conditionally
   * - depending on the specified condition.
   * @return 0 on success or HA_ERR_* error code */
  Result index_next_conditional(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row,
      /** [in] Condition which dictates whether to fetch the next row or not. */
      NextCondition condition);

  /** Read the last row that matches `mysql_search_cells` (in index order).
  @return 0 on success or HA_ERR_* error code */
  int index_read_last(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row,
      /** [in] Concatenated cells that are to be searched for.
       * @see `index_read()`. */
      const uchar *mysql_search_cells,
      /** [in] The length of `mysql_search_cells` in bytes. */
      uint mysql_search_cells_len_bytes) override;

  /** Step to the previous row in index order.
  @return 0 on success or HA_ERR_* error code */
  int index_prev(
      /** [out] Output where the retrieved row is written to. */
      uchar *mysql_row) override;

  /** End an index scan.
   * @return 0 on success or HA_ERR_* error code */
  int index_end() override;

  /** Store position to current row inside the handler. */
  void position(
      /** [in] Unused. */
      const uchar *) override;

  /** Insert a new row to the currently opened table.
   * @return 0 on success or HA_ERR_* error code */
  int write_row(
      /** [in] Row to insert. */
      uchar *mysql_row) override;

  /** Update a row.
   * @return 0 on success or HA_ERR_* error code */
  int update_row(
      /** [in] Original row to find and update. */
      const uchar *mysql_row_old,
      /** [in] New row to put into the place of the old one. */
      uchar *mysql_row_new) override;

  /** Delete the row where the handler is currently positioned. This row must
   * be equal to `mysql_row` and the handler must be positioned.
   * @return 0 on success or HA_ERR_* error code */
  int delete_row(
      /** [in] Copy of the row to be deleted. */
      const uchar *mysql_row) override;

  /** Delete all rows in the table.
   * @return 0 on success or HA_ERR_* error code */
  int truncate(
      /** [in] Unused. */
      dd::Table *) override;

  /** Delete all rows in the table.
   * @return 0 on success or HA_ERR_* error code */
  int delete_all_rows() override;

  /** Refresh table stats.
   * @return 0 on success or HA_ERR_* error code. */
  int info(
      /** [in] Which stats to refresh. */
      uint flag) override;

  /** Get the limit on the memory usage. */
  longlong get_memory_buffer_size() const override;

  /** Get the name of the storage engine.
   * @return name */
  const char *table_type() const override;

  /** Get the table flags.
   * @return table flags */
  Table_flags table_flags() const override;

  /** Get the flags for a given index.
   * @return index flags */
  ulong index_flags(
      /** [in] Index number (0-based). */
      uint index_no,
      /** [in] Unused. */
      uint,
      /** [in] Unused. */
      bool) const override;

  /** Get the default index algorithm.
   * @return index type */
  ha_key_alg get_default_index_algorithm() const override;

  /** Check whether an index algorithm is supported.
   * @return true if supported */
  bool is_index_algorithm_supported(
      /** [in] Algorithm to check if supported. */
      ha_key_alg algorithm) const override;

  /** Get the maximum supported index length in bytes.
   * @return max length */
  uint max_supported_key_length() const override;

  /** Get the maximum supported indexed columns length.
   * @return max length */
  uint max_supported_key_part_length(
      HA_CREATE_INFO *create_info) const override;

#if 0
  /* This is disabled in order to mimic ha_heap's implementation which relies on
   * the method from the parent class which adds a magic +10. */

  /** Get an upper bound of how many rows will be retrieved by a table scan.
   * The number returned by this method is precisely the number of rows in the
   * table.
   * @return number of rows in the table */
  ha_rows estimate_rows_upper_bound() override;
#endif

  /** Not implemented. */
  THR_LOCK_DATA **store_lock(THD *, THR_LOCK_DATA **, thr_lock_type) override;

  /** Scan time. The unit of the return value is "disk access". E.g. if the
   * operation would require the disk to be accessed 5 times, then 5.0 would
   * be returned.
   * @deprecated This function is deprecated and will be removed in a future
   * version. Use handler::table_scan_cost() instead.
   * @return estimate based on the number of rows */
  double scan_time() override;

  /** Read time. The unit of the return value is "disk access". E.g. if the
   * operation would require the disk to be accessed 5 times, then 5.0 would
   * be returned.
   * @deprecated This function is deprecated and will be removed in a future
   * version. Use handler::read_cost() instead.
   * @return estimate based on the number of rows */
  double read_time(
      /** [in] Unused. */
      uint,
      /** [in] Unused. */
      uint,
      /** [in] Total number of rows to be read. */
      ha_rows rows) override;

  /** Disable indexes.
   * @return 0 on success or HA_ERR_* error code */
  int disable_indexes(
      /** [in] Mode. Only HA_KEY_SWITCH_ALL (disable all) is supported. */
      uint mode) override;

  /** Enable indexes. This is only supported if the currently opened table is
   * empty.
   * @return 0 on success or HA_ERR_* error code */
  int enable_indexes(
      /** [in] Mode. Only HA_KEY_SWITCH_ALL (enable all) is supported. */
      uint mode) override;

  /* Not implemented methods. */

  /** Not implemented.
  @return 0 */
  int external_lock(THD *, int) override;

  /** Not implemented. */
  void unlock_row() override;

  /** Not implemented.
  @return nullptr */
  handler *clone(const char *, MEM_ROOT *) override;

  /** Not implemented.
  @return 0 */
  int index_first(uchar *) override;

  /** Not implemented.
  @return 0 */
  int index_last(uchar *) override;

  /** Not implemented.
  @return 0 */
  int analyze(THD *, HA_CHECK_OPT *) override;

  /** Not implemented.
  @return 0 */
  int optimize(THD *, HA_CHECK_OPT *) override;

  /** Not implemented.
  @return 0 */
  int check(THD *, HA_CHECK_OPT *) override;

  /** Not implemented.
  @return 0 */
  int start_stmt(THD *, thr_lock_type) override;

  /** Not implemented.
  @return 0 */
  int reset() override;

  /** Not implemented.
  @return 0 */
  int records(ha_rows *) override;

  /** Not implemented. */
  void update_create_info(HA_CREATE_INFO *) override;

  /** Not implemented.
  @return 0 */
  int rename_table(const char *, const char *, const dd::Table *,
                   dd::Table *) override;

  /** Not implemented. */
  void init_table_handle_for_HANDLER() override;

  /** Not implemented.
  @return false */
  bool get_error_message(int, String *) override;

  /** Not implemented.
  @return false */
  bool primary_key_is_clustered() const override;

  /** Not implemented.
  @return 0 */
  int cmp_ref(const uchar *, const uchar *) const override;

  /** Not implemented.
  @return false */
  bool check_if_incompatible_data(HA_CREATE_INFO *, uint) override;

 private:
  void opened_table_validate();

  /** Checks if field has a fixed size.
   * @return true if field has fixed size, false otherwise */
  bool is_field_type_fixed_size(
      /** [in] Field descriptor. */
      const Field &mysql_field) const;

  /** Currently opened table, or `nullptr` if none is opened. */
  Table *m_opened_table;

  /** Pointer to the non-owned shared-block of memory to be re-used by all
   * `Allocator` instances or copies made by `Table`. */
  Block *m_shared_block;

  /** Iterator used by `rnd_init()`, `rnd_next()` and `rnd_end()` methods.
   * It points to the row that was retrieved by the last read call (e.g.
   * `rnd_next()`). */
  Storage::Iterator m_rnd_iterator;

  /** Flag that denotes whether `m_rnd_iterator` is positioned. `rnd_init()`
   * "unpositions" the iterator, so that `rnd_next()` knows to start from
   * the first row when the iterator is not positioned. */
  bool m_rnd_iterator_is_positioned;

  /** Cursor used by `index_*()` methods.
   * It points to the current record that will be retrieved by the next
   * read call (e.g. `index_next()`). */
  Cursor m_index_cursor;

  /** Number of cells to compare in `index_next()` after `index_read()` has
   * positioned `m_index_cursor`. If we have an index on two columns, e.g.
   * (c1, c2) and rows:
   * (5, 6), (5, 7)
   * and `index_read()` is requested to fetch the row where c1=5 then we
   * will fetch the first row and position the index cursor on (5, 6).
   * A subsequent call to `index_next()` must go to the next row if it
   * is the same as the current, but only comparing the first cell.
   * So in order to be able to treat (5, 6) equal to (5, 7) during
   * `index_next()` (because the `index_read()` call only specified the first
   * cell) we remember the number of cells to compare in this variable. */
  size_t m_index_read_number_of_cells;

  /** Number of deleted rows by this handler object. */
  size_t m_deleted_rows;
};

inline void Handler::opened_table_validate() {
  assert(m_opened_table != nullptr);
  assert(handler::table != nullptr);
  assert(m_opened_table->mysql_table_share() == handler::table->s);
}

inline bool Handler::is_field_type_fixed_size(const Field &mysql_field) const {
  switch (mysql_field.type()) {
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_VARCHAR:
      return false;
    default:
      return true;
  }
}

void kv_store_shards_debug_dump();
void shared_block_pool_release(THD *thd);

} /* namespace temptable */

#endif /* TEMPTABLE_HANDLER_H */
