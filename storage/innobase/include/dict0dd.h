/*****************************************************************************

Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

/** @file include/dict0dd.h
Data dictionary interface */

#ifndef dict0dd_h
#define dict0dd_h

#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "my_compiler.h"
#include "univ.i"

#ifndef UNIV_HOTBACKUP
#include "dd/cache/dictionary_client.h"
#include "dd/dd.h"
#include "dd/dd_schema.h"
#include "dd/dd_table.h"
#include "dd/dictionary.h"
#include "dd/properties.h"
#include "dd/types/column.h"
#include "dd/types/foreign_key.h"
#include "dd/types/foreign_key_element.h"
#include "dd/types/index.h"
#include "dd/types/index_element.h"
#include "dd/types/partition.h"
#include "dd/types/partition_index.h"
#include "dd/types/table.h"
#include "dd/types/tablespace.h"
#include "dd/types/tablespace_file.h"
#include "dd_table_share.h"
#include "sess0sess.h"
#else
#include "mysql_com.h"
#endif /* !UNIV_HOTBACKUP */
#include "mysql_version.h"

#ifndef UNIV_HOTBACKUP
class THD;
class MDL_ticket;

struct CHARSET_INFO;

/** DD functions return false for success and true for failure
because that is the way the server functions are defined. */
constexpr bool DD_SUCCESS = false;
constexpr bool DD_FAILURE = true;

/** Handler name for InnoDB */
static constexpr char handler_name[] = "InnoDB";

static const char innobase_hton_name[] = "InnoDB";

/** String constant for AUTOEXTEND_SIZE option string */
static constexpr char autoextend_size_str[] = "autoextend_size";

/** Determine if give version is a valid row version */
bool dd_is_valid_row_version(uint32_t version);

/** Determine if column is INSTANT ADD */
bool dd_column_is_added(const dd::Column *dd_col);

/** Determine if column is INSTANT DROP */
bool dd_column_is_dropped(const dd::Column *dd_col);

/** Get the row version in which column is INSTANT ADD */
uint32_t dd_column_get_version_added(const dd::Column *dd_col);

/** Get the row version in which column is INSTANT DROP */
uint32_t dd_column_get_version_dropped(const dd::Column *dd_col);

/** Maximum hardcoded data dictionary tables. */
constexpr uint32_t DICT_MAX_DD_TABLES = 1024;

/** InnoDB private keys for dd::Table */
enum dd_table_keys {
  /** Auto-increment counter */
  DD_TABLE_AUTOINC,
  /** DATA DIRECTORY (static metadata) */
  DD_TABLE_DATA_DIRECTORY,
  /** Dynamic metadata version */
  DD_TABLE_VERSION,
  /** Discard flag. Please don't use it directly, and instead use
  dd_is_discarded and dd_set_discarded functions. Discard flag is defined
  for both dd::Table and dd::Partition and it's easy to confuse.
  The functions will choose right implementation for you, depending on
  whether the argument is dd::Table or dd::Partition. */
  DD_TABLE_DISCARD,
  /** Columns before first instant ADD COLUMN, used only for V1 */
  DD_TABLE_INSTANT_COLS,
  /** Sentinel */
  DD_TABLE__LAST
};

/** InnoDB private keys for dd::Column */
/** About the DD_INSTANT_COLUMN_DEFAULT*, please note that if it's a
partitioned table, not every default value is needed for every partition.
For example, after ALTER TABLE ... PARTITION, maybe some partitions only
need part or even none of the default values. Let's say there are two
partitions, p1 and p2. p1 needs 10 default values while p2 needs 2.
If another ALTER ... PARTITION makes p1 a fresh new partition which
doesn't need the default values any more, currently, the extra 8(10 - 2)
default values are not removed form dd::Column::se_private_data. */
enum dd_column_keys {
  /** Default value when it was added instantly */
  DD_INSTANT_COLUMN_DEFAULT,
  /** Default value is null or not */
  DD_INSTANT_COLUMN_DEFAULT_NULL,
  /** Row version when this column was added instantly */
  DD_INSTANT_VERSION_ADDED,
  /** Row version when this column was dropped instantly */
  DD_INSTANT_VERSION_DROPPED,
  /** Column physical position on row when it was created */
  DD_INSTANT_PHYSICAL_POS,
  /** Sentinel */
  DD_COLUMN__LAST
};

#endif /* !UNIV_HOTBACKUP */

/** Server version that the tablespace created */
const uint32_t DD_SPACE_CURRENT_SRV_VERSION = MYSQL_VERSION_ID;

/** The tablespace version that the tablespace created */
const uint32_t DD_SPACE_CURRENT_SPACE_VERSION = 1;

#ifndef UNIV_HOTBACKUP
/** InnoDB private keys for dd::Partition */
enum dd_partition_keys {
  /** Row format for this partition */
  DD_PARTITION_ROW_FORMAT,
  /** Columns before first instant ADD COLUMN.
  This is necessary for each partition because different partition
  may have different instant column numbers, especially, for a
  newly truncated partition, it can have no instant columns.
  So partition level one should be always >= table level one. */
  DD_PARTITION_INSTANT_COLS,
  /** Discard flag. Please don't use it directly, and instead use
  dd_is_discarded and dd_set_discarded functions. Discard flag is defined
  for both dd::Table and dd::Partition and it's easy to confuse.
  The functions will choose right implementation for you, depending on
  whether the argument is dd::Table or dd::Partition. */
  DD_PARTITION_DISCARD,
  /** Sentinel */
  DD_PARTITION__LAST
};

/** InnoDB private keys for dd::Tablespace */
enum dd_space_keys {
  /** Tablespace flags */
  DD_SPACE_FLAGS,
  /** Tablespace identifier */
  DD_SPACE_ID,
  /** Discard attribute */
  DD_SPACE_DISCARD,
  /** Server version */
  DD_SPACE_SERVER_VERSION,
  /** TABLESPACE_VERSION */
  DD_SPACE_VERSION,
  /** Current state attribute */
  DD_SPACE_STATE,
  /** Sentinel */
  DD_SPACE__LAST
};

/** Values for InnoDB private key "state" for dd::Tablespace */
enum dd_space_states {
  /** Normal IBD tablespace */
  DD_SPACE_STATE_NORMAL,
  /** Discarded IBD tablespace */
  DD_SPACE_STATE_DISCARDED,
  /** Corrupted IBD tablespace */
  DD_SPACE_STATE_CORRUPTED,
  /** Active undo tablespace */
  DD_SPACE_STATE_ACTIVE,
  /** Inactive undo tablespace being truncated, selected
  explicitly by ALTER UNDO TABLESPACE SET INACTIVE.
  Note: the DD is not updated when an undo space is selected
  for truncation implicitly by the purge thread. */
  DD_SPACE_STATE_INACTIVE,
  /** Inactive undo tablespace being truncated, selected
  explicitly by ALTER UNDO TABLESPACE SET INACTIVE. */
  DD_SPACE_STATE_EMPTY,
  /** Sentinel */
  DD_SPACE_STATE__LAST
};

/** InnoDB implicit tablespace name or prefix, which should be same to
dict_sys_t::s_file_per_table_name */
static constexpr char reserved_implicit_name[] = "innodb_file_per_table";

/** InnoDB private key strings for dd::Tablespace.
@see dd_space_keys */
const char *const dd_space_key_strings[DD_SPACE__LAST] = {
    "flags", "id", "discard", "server_version", "space_version", "state"};

/** InnoDB private value strings for key string "state" in dd::Tablespace.
@see dd_space_state_values */
const char *const dd_space_state_values[DD_SPACE_STATE__LAST + 1] = {
    "normal",    /* for IBD spaces */
    "discarded", /* for IBD spaces */
    "corrupted", /* for IBD spaces */
    "active",    /* for undo spaces*/
    "inactive",  /* for undo spaces */
    "empty",     /* for undo spaces */
    "unknown"    /* for non-existing or unknown spaces */
};

/** InnoDB private key strings for dd::Table. @see dd_table_keys */
const char *const dd_table_key_strings[DD_TABLE__LAST] = {
    "autoinc", "data_directory", "version", "discard", "instant_col"};

/** InnoDB private key strings for dd::Column, @see dd_column_keys */
const char *const dd_column_key_strings[DD_COLUMN__LAST] = {
    "default", "default_null", "version_added", "version_dropped",
    "physical_pos"};

/** InnoDB private key strings for dd::Partition. @see dd_partition_keys */
const char *const dd_partition_key_strings[DD_PARTITION__LAST] = {
    "format", "instant_col", "discard"};

/** InnoDB private keys for dd::Index or dd::Partition_index */
enum dd_index_keys {
  /** Index identifier */
  DD_INDEX_ID,
  /** Space id */
  DD_INDEX_SPACE_ID,
  /** Table id */
  DD_TABLE_ID,
  /** Root page number */
  DD_INDEX_ROOT,
  /** Creating transaction ID */
  DD_INDEX_TRX_ID,
  /** Sentinel */
  DD_INDEX__LAST
};

/** InnoDB private key strings for dd::Index or dd::Partition_index.
@see dd_index_keys */
const char *const dd_index_key_strings[DD_INDEX__LAST] = {
    "id", "space_id", "table_id", "root", "trx_id"};

/** InnoDB private key strings for dd::Index or dd::Partition_index.
@see dd_index_keys */
extern const char *const dd_index_key_strings[DD_INDEX__LAST];

/** dd::Partition::options() key for INDEX DIRECTORY */
static const dd::String_type index_file_name_key("index_file_name");
/** dd::Partition::options() key for DATA DIRECTORY */
static const dd::String_type data_file_name_key("data_file_name");

/** Table names needed to process I_S queries. */
static const dd::String_type dd_tables_name("mysql/tables");
static const dd::String_type dd_partitions_name("mysql/table_partitions");
static const dd::String_type dd_tablespaces_name("mysql/tablespaces");
static const dd::String_type dd_indexes_name("mysql/indexes");
static const dd::String_type dd_columns_name("mysql/columns");

#ifdef UNIV_DEBUG

/** Hard-coded data dictionary information */
struct innodb_dd_table_t {
  /** Data dictionary table name */
  const char *name;
  /** Number of indexes */
  const uint n_indexes;
};

/** The hard-coded data dictionary tables. */
const innodb_dd_table_t innodb_dd_table[] = {
    INNODB_DD_TABLE("dd_properties", 1),

    INNODB_DD_TABLE("innodb_dynamic_metadata", 1),
    INNODB_DD_TABLE("innodb_table_stats", 1),
    INNODB_DD_TABLE("innodb_index_stats", 1),
    INNODB_DD_TABLE("innodb_ddl_log", 2),

    INNODB_DD_TABLE("catalogs", 2),
    INNODB_DD_TABLE("character_sets", 3),
    INNODB_DD_TABLE("check_constraints", 3),
    INNODB_DD_TABLE("collations", 3),
    INNODB_DD_TABLE("column_statistics", 3),
    INNODB_DD_TABLE("column_type_elements", 1),
    INNODB_DD_TABLE("columns", 5),
    INNODB_DD_TABLE("events", 6),
    INNODB_DD_TABLE("foreign_key_column_usage", 3),
    INNODB_DD_TABLE("foreign_keys", 4),
    INNODB_DD_TABLE("index_column_usage", 3),
    INNODB_DD_TABLE("index_partitions", 3),
    INNODB_DD_TABLE("index_stats", 1),
    INNODB_DD_TABLE("indexes", 3),
    INNODB_DD_TABLE("parameter_type_elements", 1),
    INNODB_DD_TABLE("parameters", 3),
    INNODB_DD_TABLE("resource_groups", 2),
    INNODB_DD_TABLE("routines", 7),
    INNODB_DD_TABLE("schemata", 3),
    INNODB_DD_TABLE("st_spatial_reference_systems", 3),
    INNODB_DD_TABLE("table_partition_values", 1),
    INNODB_DD_TABLE("table_partitions", 7),
    INNODB_DD_TABLE("table_stats", 1),
    INNODB_DD_TABLE("tables", 10),
    INNODB_DD_TABLE("tablespace_files", 2),
    INNODB_DD_TABLE("tablespaces", 2),
    INNODB_DD_TABLE("triggers", 7),
    INNODB_DD_TABLE("view_routine_usage", 2),
    INNODB_DD_TABLE("view_table_usage", 2)};

/** Number of hard-coded data dictionary tables */
static constexpr size_t innodb_dd_table_size = UT_ARR_SIZE(innodb_dd_table);

/** @return total number of indexes of all DD Tables. */
uint32_t dd_get_total_indexes_num();

#endif /* UNIV_DEBUG */

#endif /* !UNIV_HOTBACKUP */

/** Class to decode or encode a stream of default value for instant table.
The decode/encode are necessary because that the default values would b
kept as InnoDB format stream, which is in fact byte stream. However,
to store them in the DD se_private_data, it requires text(char).
So basically, the encode will change the byte stream into char stream,
by splitting every byte into two chars, for example, 0xFF, would be split
into 0x0F and 0x0F. So the final storage space would be double. For the
decode, it's the converse process, combining two chars into one byte. */
class DD_instant_col_val_coder {
 public:
  /** Constructor */
  DD_instant_col_val_coder() : m_result(nullptr) {}

  /** Destructor */
  ~DD_instant_col_val_coder() { cleanup(); }

  /** Encode the specified stream in format of bytes into chars
  @param[in]    stream  stream to encode in bytes
  @param[in]    in_len  length of the stream
  @param[out]   out_len length of the encoded stream
  @return       the encoded stream, which would be destroyed if the class
  itself is destroyed */
  const char *encode(const byte *stream, size_t in_len, size_t *out_len);

  /** Decode the specified stream, which is encoded by encode()
  @param[in]    stream  stream to decode in chars
  @param[in]    in_len  length of the stream
  @param[out]   out_len length of the decoded stream
  @return       the decoded stream, which would be destroyed if the class
  itself is destroyed */
  const byte *decode(const char *stream, size_t in_len, size_t *out_len);

 private:
  /** Clean-up last result */
  void cleanup() { ut::delete_arr(m_result); }

 private:
  /** The encoded or decoded stream */
  byte *m_result;
};

#ifndef UNIV_HOTBACKUP
/** Determine if a dd::Partition is the first leaf partition in the table
@param[in]      dd_part dd::Partition
@return True    If it's the first partition
@retval False   Not the first one */
inline bool dd_part_is_first(const dd::Partition *dd_part) {
  return (dd_part == *(dd_part->table().leaf_partitions().begin()));
}

/** Determine if a dd::Table is partitioned table
@param[in]      table   dd::Table
@return True    If partitioned table
@retval False   non-partitioned table */
inline bool dd_table_is_partitioned(const dd::Table &table) {
  return (table.partition_type() != dd::Table::PT_NONE);
}

#ifdef UNIV_DEBUG
/** Check if the instant columns are consistent with the se_private_data
in dd::Table
@param[in]      dd_table        dd::Table
@return true if consistent, otherwise false */
bool dd_instant_columns_consistent(const dd::Table &dd_table);
#endif /* UNIV_DEBUG */

/** Scan through all the keys to identify the key parts which are
greater than the maximum size supported by the table record format.
@param table         MySQL table definition.
@param max_part_len  Maximum index part length allowed.
@param visitor       Function wrapper to invoke lambda expression.
*/
void dd_visit_keys_with_too_long_parts(
    const TABLE *table, const size_t max_part_len,
    std::function<void(const KEY &)> visitor);

/** Determine if a dd::Table has any INSTANT ADD column(s) in V1
@param[in]      table   dd::Table
@return true if table has instant column(s) in V1, false otherwise */
inline bool dd_table_is_upgraded_instant(const dd::Table &table) {
  if (table.is_temporary()) {
    return false;
  }

  return (table.se_private_data().exists(
      dd_table_key_strings[DD_TABLE_INSTANT_COLS]));
}

/** Determine if dd::Table has INSTANT ADD columns.
@param[in]     table   table definition
@return true if table has INSTANT ADD column(s), false otherwise */
inline bool dd_table_has_instant_add_cols(const dd::Table &table) {
  if (table.is_temporary()) {
    return false;
  }

  for (const auto column : table.columns()) {
    if (dd_column_is_added(column)) {
      return true;
    }
  }

  return false;
}

/** Determine if dd::Table has INSTANT DROPPED columns.
@param[in]     table   table definition
@return true if table has INSTANT DROP column(s), false otherwise */
inline bool dd_table_has_instant_drop_cols(const dd::Table &table) {
  if (table.is_temporary()) {
    return false;
  }

  for (const auto column : table.columns()) {
    if (dd_column_is_dropped(column)) {
      return true;
    }
  }

  return false;
}

static inline bool is_system_column(const char *col_name) {
  ut_ad(col_name != nullptr);
  return (strncmp(col_name, "DB_ROW_ID", 9) == 0 ||
          strncmp(col_name, "DB_TRX_ID", 9) == 0 ||
          strncmp(col_name, "DB_ROLL_PTR", 11) == 0);
}

/** Set different column counters.
@param[in]   table  dd::Table
@param[out]  i_c    initial column count
@param[out]  c_c    current column count
@param[out]  t_c    total column count
@param[in]   current_row_version  current row version */
inline void dd_table_get_column_counters(const dd::Table &table, uint32_t &i_c,
                                         uint32_t &c_c, uint32_t &t_c,
                                         uint32_t &current_row_version) {
  size_t n_dropped_cols = 0;
  size_t n_added_cols = 0;
  size_t n_added_and_dropped_cols = 0;
  size_t n_current_cols = 0;

  for (const auto column : table.columns()) {
    if (is_system_column(column->name().c_str()) || column->is_virtual()) {
      continue;
    }

    if (dd_column_is_dropped(column)) {
      n_dropped_cols++;
      if (dd_column_is_added(column)) {
        n_added_and_dropped_cols++;
      }

      uint32_t v_dropped = dd_column_get_version_dropped(column);
      ut_ad(dd_is_valid_row_version(v_dropped));
      current_row_version = std::max(current_row_version, v_dropped);

      continue;
    }

    if (dd_column_is_added(column)) {
      n_added_cols++;

      uint32_t v_added = dd_column_get_version_added(column);
      ut_ad(dd_is_valid_row_version(v_added));
      current_row_version = std::max(current_row_version, v_added);
    }

    n_current_cols++;
  }

  ut_ad(n_dropped_cols >= n_added_and_dropped_cols);
  size_t n_orig_dropped_cols = n_dropped_cols - n_added_and_dropped_cols;
  c_c = n_current_cols;
  i_c = (n_current_cols - n_added_cols) + n_orig_dropped_cols;
  t_c = n_current_cols + n_dropped_cols;
}

/** Determine if a dd::Table has row versions
@param[in]      table   dd::Table
@return true if table has row versions, false otherwise */
inline bool dd_table_has_row_versions(const dd::Table &table) {
  if (table.is_temporary()) {
    return false;
  }

  bool has_row_version = false;
  for (const auto column : table.columns()) {
    if (column->is_virtual()) {
      continue;
    }

    /* Phy_pos metadata is populated for columns which belongs to table having
    row versions. Check if non virtual column has it. */
    if (column->se_private_data().exists(
            dd_column_key_strings[DD_INSTANT_PHYSICAL_POS])) {
      has_row_version = true;
    }

    /* Checking only for one column is enough. */
    break;
  }

#ifdef UNIV_DEBUG
  if (has_row_version) {
    bool found_inst_add_or_drop_col = false;
    for (const auto column : table.columns()) {
      if (dd_column_is_dropped(column) || dd_column_is_added(column)) {
        found_inst_add_or_drop_col = true;
        break;
      }
    }
    ut_ad(found_inst_add_or_drop_col);
  }
#endif

  return has_row_version;
}

/** Determine if a dd::Table has any INSTANTly ADDed/DROPped column
@param[in]      table   dd::Table
@return true if table has instant column(s), false otherwise */
inline bool dd_table_has_instant_cols(const dd::Table &table) {
  if (table.is_temporary()) {
    return false;
  }

  bool instant_v1 = dd_table_is_upgraded_instant(table);
  bool instant_v2 = dd_table_has_row_versions(table);

  bool instant = instant_v1 || instant_v2;

  /* If table has instant columns, make sure they are consistent with DD */
  ut_ad(!instant || dd_instant_columns_consistent(table));

  return (instant);
}

/** Determine if a dd::Partition has any instant column
@param[in]      part    dd::Partition
@return true    If it's a partitioned table with instant columns
@return false   Not a partitioned table with instant columns */
inline bool dd_part_has_instant_cols(const dd::Partition &part) {
  bool instant = part.se_private_data().exists(
      dd_partition_key_strings[DD_PARTITION_INSTANT_COLS]);
  ut_ad(!instant || dd_table_has_instant_cols(part.table()));

  return (instant);
}

/** Determine if any partition of the table still has instant columns
@param[in]      table   dd::Table of the partitioned table
@return true    If any partition still has instant columns
@return false   No one has instant columns */
inline bool dd_table_part_has_instant_cols(const dd::Table &table) {
  ut_ad(dd_table_is_partitioned(table));

  /* For table having INSTANT ADD cols in v1, will have partition specific
  INSTANT Metadata. */
  if (dd_table_is_upgraded_instant(table)) {
    for (auto part : table.leaf_partitions()) {
      if (dd_part_has_instant_cols(*part)) {
        return (true);
      }
    }
  }

  return (false);
}

/** Determine if dd::Table is discarded. Please note that
in case of partitioned Table, only it's leaf partitions can be marked
as discarded. However, it's fine to call this function on partitioned
Table - it will just return false

@param[in] table non-partitioned dd::Table
@return true if table is marked as discarded
@return false if table is not marked as discarded */
inline bool dd_is_discarded(const dd::Table &table) {
  const dd::Properties &table_private = table.se_private_data();
  bool is_discarded = false;
  if (table_private.exists(dd_table_key_strings[DD_TABLE_DISCARD])) {
    table_private.get(dd_table_key_strings[DD_TABLE_DISCARD], &is_discarded);
  }

  /* In case of partitioned tables, only partitions/subpartitions can ever
  be marked as discarded */
  ut_ad(!is_discarded || !dd_table_is_partitioned(table));

  return is_discarded;
}

/** Determine if dd::Partition is discarded. Please note that
only leaf partitions can be marked as discarded (that is, if partition has
subpartitions, then only subpartitions can be marked as discarded)

Function can be safely called on a partition, even if it has subpartitions -
it will just return false.

@param[in] partition dd::Partition
@return true if partition is marked as discarded
@return false if partition is not marked as discarded */
inline bool dd_is_discarded(const dd::Partition &partition) {
  const dd::Properties &partition_private = partition.se_private_data();
  bool is_discarded = false;
  if (partition_private.exists(
          dd_partition_key_strings[DD_PARTITION_DISCARD])) {
    partition_private.get(dd_partition_key_strings[DD_PARTITION_DISCARD],
                          &is_discarded);
  }

  return is_discarded;
}

/** Sets appropriate discard attribute of dd::Table
Please note that this function must not be called on partitioned tables
@param[in]  table    non-partitioned dd::Table
@param[in]  discard  true if Table is discarded, false otherwise */
void dd_set_discarded(dd::Table &table, bool discard);

/** Sets appropriate discard attribute of dd::Partition
Please note that this function can be only called on leaf_partitions.
@param[in]  partition  leaf dd::Partition
@param[in]  discard    true if Table is discarded, false otherwise */
void dd_set_discarded(dd::Partition &partition, bool discard);

/** Get the first index of a table or partition.
@tparam         Table   dd::Table or dd::Partition
@tparam         Index   dd::Index or dd::Partition_index
@param[in]      table   table containing user columns and indexes
@return the first index
@retval NULL    if there are no indexes */
template <typename Table, typename Index>
inline const Index *dd_first(const Table *table) {
  return (*table->indexes().begin());
}

/** Get the first index of a table.
@param[in]      table   table containing user columns and indexes
@return the first index
@retval NULL    if there are no indexes */
inline const dd::Index *dd_first_index(const dd::Table *table) {
  return (dd_first<dd::Table, dd::Index>(table));
}

/** Get the first index of a partition.
@param[in]      partition       partition or subpartition
@return the first index
@retval NULL    if there are no indexes */
inline const dd::Partition_index *dd_first_index(
    const dd::Partition *partition) {
  return (dd_first<dd::Partition, dd::Partition_index>(partition));
}

#ifdef UNIV_DEBUG
/** Determine if a partition is materialized.
@param[in]      part            partition
@return whether the partition is materialized */
inline bool dd_part_is_stored(const dd::Partition *part) {
  return (part->table().subpartition_type() == dd::Table::ST_NONE ||
          part->parent());
}
#endif /* UNIV_DEBUG */

/** Get the explicit dd::Tablespace::id of a table.
@param[in]      table   non-partitioned table
@return the explicit dd::Tablespace::id
@retval dd::INVALID_OBJECT_ID   if there is no explicit tablespace */
inline dd::Object_id dd_get_space_id(const dd::Table &table) {
  ut_ad(!dd_table_is_partitioned(table));
  return (table.tablespace_id());
}

/** Get the explicit dd::Tablespace::id of a partition.
@param[in]      partition       partition or subpartition
@return the explicit dd::Tablespace::id
@retval dd::INVALID_OBJECT_ID   if there is no explicit tablespace */
inline dd::Object_id dd_get_space_id(const dd::Partition &partition);

/** Set the AUTO_INCREMENT attribute.
@param[in,out]  se_private_data dd::Table::se_private_data
@param[in]      autoinc         the auto-increment value */
void dd_set_autoinc(dd::Properties &se_private_data, uint64_t autoinc);

/** Get the version attribute.
@param[in]      dd_table        dd::Table
@return table dynamic metadata version if exists, otherwise 0 */
inline uint64_t dd_get_version(const dd::Table *dd_table);

/** Copy the AUTO_INCREMENT and version attribute if exist.
@param[in]      src     dd::Table::se_private_data to copy from
@param[out]     dest    dd::Table::se_private_data to copy to */
void dd_copy_autoinc(const dd::Properties &src, dd::Properties &dest);

/** Copy the metadata of a table definition if there was an instant
ADD COLUMN happened. This should be done when it's not an ALTER TABLE
with rebuild.
@param[in,out]  new_table       New table definition
@param[in]      old_table       Old table definition */
void dd_copy_instant_n_cols(dd::Table &new_table, const dd::Table &old_table);

/** Copy the engine-private parts of a table or partition definition
when the change does not affect InnoDB. This mainly copies the common
private data between dd::Table and dd::Partition
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  new_table       Copy of old table or partition
definition
@param[in]      old_table       Old table or partition definition */
template <typename Table>
void dd_copy_private(Table &new_table, const Table &old_table);

/** Copy the engine-private parts of column definitions of a table
@param[in]      ha_alter_info   alter info
@param[in,out]  new_table       Copy of old table
@param[in]      old_table       Old table
@param[in]      dict_table      InnoDB table cache */
void dd_copy_table_columns(const Alter_inplace_info *ha_alter_info,
                           dd::Table &new_table, const dd::Table &old_table,
                           dict_table_t *dict_table);

/** Copy the metadata of a table definition, including the INSTANT
ADD COLUMN information. This should be done when it's not an ALTER TABLE
with rebuild. Basically, check dd::Table::se_private_data and
dd::Column::se_private_data.
@param[in]      ha_alter_info   alter info
@param[in,out]  new_table       Copy of old table definition
@param[in]      old_table       Old table definition */
inline void dd_copy_table(const Alter_inplace_info *ha_alter_info,
                          dd::Table &new_table, const dd::Table &old_table) {
  /* Copy columns first, to make checking in dd_copy_instant_n_cols pass */
  dd_copy_table_columns(ha_alter_info, new_table, old_table, nullptr);
  if (dd_table_is_upgraded_instant(old_table)) {
    dd_copy_instant_n_cols(new_table, old_table);
  }
}

/** Adjust TABLE_ID for partitioned table after ALTER TABLE ... PARTITION.
This makes sure that the TABLE_ID stored in dd::Column::se_private_data
is correct if the first partition got changed
@param[in,out]  new_table       New dd::Table */
void dd_part_adjust_table_id(dd::Table *new_table);

using Columns = std::vector<Field *>;

/** Drop column instantly. It actually updates dropped columns metadata.
@param[in]      old_dd_table    Old dd::Table
@param[in,out]  new_dd_table    New dd::Table
@param[in,out]  new_table       New InnoDB table objecta
@param[in]      cols_to_drop    list of columns to be dropped
@param[in]      cols_to_add     list of columns to be added
@param[in]      ha_alter_info   alter info
@retval true Failure
@retval false Success */
bool dd_drop_instant_columns(const dd::Table *old_dd_table,
                             dd::Table *new_dd_table, dict_table_t *new_table,
                             const Columns &cols_to_drop
                                 IF_DEBUG(, const Columns &cols_to_add,
                                          Alter_inplace_info *ha_alter_info));

/** Add column default values for new instantly added columns
@param[in]      old_dd_table    Old dd::Table
@param[in,out]  new_dd_table    New dd::Table
@param[in,out]  new_table       New InnoDB table object
@param[in]      cols_to_add     columns to be added INSTANTly
@retval true Failure
@retval false Success */
bool dd_add_instant_columns(const dd::Table *old_dd_table,
                            dd::Table *new_dd_table, dict_table_t *new_table,
                            const Columns &cols_to_add);

/** Clear the instant ADD COLUMN information of a table
@param[in,out]  dd_table        dd::Table
@param[in]      clear_version   true if version metadata is to be cleared
@return DB_SUCCESS or error code */
dberr_t dd_clear_instant_table(dd::Table &dd_table, bool clear_version);

/** Clear the instant ADD COLUMN information of a partition, to make it
as a normal partition
@param[in,out]  dd_part         dd::Partition */
void dd_clear_instant_part(dd::Partition &dd_part);

/** Compare the default values between imported column and column defined
in the server. Note that it's absolutely OK if there is no default value
in the column defined in server, since it can be filled in later.
@param[in]      dd_col  dd::Column
@param[in]      col     InnoDB column object
@return true    The default values match
@retval false   Not match */
bool dd_match_default_value(const dd::Column *dd_col, const dict_col_t *col);

/** Write default value of a column to dd::Column
@param[in]      col     default value of this column to write
@param[in,out]  dd_col  where to store the default value */
void dd_write_default_value(const dict_col_t *col, dd::Column *dd_col);

/** Import all metadata which is related to instant ADD COLUMN of a table
to dd::Table. This is used for IMPORT.
@param[in]      table           InnoDB table object
@param[in,out]  dd_table        dd::Table */
void dd_import_instant_add_columns(const dict_table_t *table,
                                   dd::Table *dd_table);

/** Write metadata of a table to dd::Table
@tparam         Table           dd::Table or dd::Partition
@param[in]      dd_space_id     Tablespace id, which server allocates
@param[in,out]  dd_table        dd::Table or dd::Partition
@param[in]      table           InnoDB table object */
template <typename Table>
void dd_write_table(dd::Object_id dd_space_id, Table *dd_table,
                    const dict_table_t *table);

/** Set options of dd::Table according to InnoDB table object
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  dd_table        dd::Table or dd::Partition
@param[in]      table           InnoDB table object */
template <typename Table>
void dd_set_table_options(Table *dd_table, const dict_table_t *table);

/** Update virtual columns with new se_private_data, currently, only
table_id is set
@param[in,out]  dd_table        dd::Table
@param[in]      id              InnoDB table ID to set */
void dd_update_v_cols(dd::Table *dd_table, table_id_t id);

/** Write metadata of a tablespace to dd::Tablespace
@param[in,out]  dd_space        dd::Tablespace
@param[in]      space_id        InnoDB tablespace ID
@param[in]      fsp_flags       InnoDB tablespace flags
@param[in]      state           InnoDB tablespace state */
void dd_write_tablespace(dd::Tablespace *dd_space, space_id_t space_id,
                         uint32_t fsp_flags, dd_space_states state);

/** Add fts doc id column and index to new table
when old table has hidden fts doc id without fulltext index
@param[in,out]  new_table       New dd table
@param[in]      old_table       Old dd table */
void dd_add_fts_doc_id_index(dd::Table &new_table, const dd::Table &old_table);

MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_CLANG_WORKAROUND_TPARAM_DOCBUG()
/** Find the specified dd::Index or dd::Partition_index in an InnoDB table
@tparam         Index                   dd::Index or dd::Partition_index
@param[in]      table                   InnoDB table object
@param[in]      dd_index                Index to search
@return the dict_index_t object related to the index */
template <typename Index>
const dict_index_t *dd_find_index(const dict_table_t *table, Index *dd_index);
MY_COMPILER_DIAGNOSTIC_POP()

/** Acquire a shared metadata lock.
@param[in,out]  thd     current thread
@param[out]     mdl     metadata lock
@param[in]      db      schema name
@param[in]      table   table name
@retval false if acquired, or trylock timed out
@retval true if failed (my_error() will have been called) */
[[nodiscard]] static inline bool dd_mdl_acquire(THD *thd, MDL_ticket **mdl,
                                                const char *db,
                                                const char *table);

/** Release a metadata lock.
@param[in,out]  thd     current thread
@param[in,out]  mdl     metadata lock */
void dd_mdl_release(THD *thd, MDL_ticket **mdl);

/** Returns thd associated with the trx or current_thd
@param[in]      trx     transaction
@return trx->mysql_thd or current_thd */
THD *dd_thd_for_undo(const trx_t &trx);

/** Check if current undo needs a MDL or not
@param[in]      trx     transaction
@return true if MDL is necessary, otherwise false */
bool dd_mdl_for_undo(const trx_t &trx);

/** Load foreign key constraint info for the dd::Table object.
@param[out]     m_table         InnoDB table handle
@param[in]      dd_table        Global DD table
@param[in]      col_names       column names, or NULL
@param[in]      ignore_err      DICT_ERR_IGNORE_FK_NOKEY or DICT_ERR_IGNORE_NONE
@param[in]      dict_locked     True if dict_sys->mutex is already held,
                                otherwise false
@return DB_SUCCESS      if successfully load FK constraint */
dberr_t dd_table_load_fk_from_dd(dict_table_t *m_table,
                                 const dd::Table *dd_table,
                                 const char **col_names,
                                 dict_err_ignore_t ignore_err,
                                 bool dict_locked);

/** Set the AUTO_INCREMENT attribute.
@param[in,out]  se_private_data dd::Table::se_private_data
@param[in]      autoinc         the auto-increment value */
void dd_set_autoinc(dd::Properties &se_private_data, uint64_t autoinc);

/** Scan a new dd system table, like mysql.tables...
@param[in]      thd             THD
@param[in,out]  mdl             MDL lock
@param[in,out]  pcur            Persistent cursor
@param[in,out]  mtr             Mini-transaction
@param[in]      system_table_name       Which dd system table to open
@param[in,out]  table           dict_table_t obj of dd system table
@retval the first rec of the dd system table */
const rec_t *dd_startscan_system(THD *thd, MDL_ticket **mdl, btr_pcur_t *pcur,
                                 mtr_t *mtr, const char *system_table_name,
                                 dict_table_t **table);

/** Process one mysql.tables record and get the dict_table_t
@param[in]      heap            Temp memory heap
@param[in,out]  rec             mysql.tables record
@param[in,out]  table           dict_table_t to fill
@param[in]      dd_tables       dict_table_t obj of dd system table
@param[in]      mdl             MDL on the table
@param[in]      mtr             Mini-transaction
@retval error message, or NULL on success */
const char *dd_process_dd_tables_rec_and_mtr_commit(
    mem_heap_t *heap, const rec_t *rec, dict_table_t **table,
    dict_table_t *dd_tables, MDL_ticket **mdl, mtr_t *mtr);
/** Process one mysql.table_partitions record and get the dict_table_t
@param[in]      heap            Temp memory heap
@param[in,out]  rec             mysql.table_partitions record
@param[in,out]  table           dict_table_t to fill
@param[in]      dd_tables       dict_table_t obj of dd partition table
@param[in]      mdl             MDL on the table
@param[in]      mtr             Mini-transaction
@retval error message, or NULL on success */
const char *dd_process_dd_partitions_rec_and_mtr_commit(
    mem_heap_t *heap, const rec_t *rec, dict_table_t **table,
    dict_table_t *dd_tables, MDL_ticket **mdl, mtr_t *mtr);

/** Process one mysql.columns record and get info to dict_col_t
@param[in,out]  heap            Temp memory heap
@param[in]      rec             mysql.columns record
@param[in,out]  col             dict_col_t to fill
@param[in,out]  table_id        Table id
@param[in,out]  col_name        Column name
@param[in,out]  nth_v_col       Nth v column
@param[in]      dd_columns      dict_table_t obj of mysql.columns
@param[in,out]  mtr             Mini-transaction
@retval true if column is filled */
bool dd_process_dd_columns_rec(mem_heap_t *heap, const rec_t *rec,
                               dict_col_t *col, table_id_t *table_id,
                               char **col_name, ulint *nth_v_col,
                               const dict_table_t *dd_columns, mtr_t *mtr);

/** Process one mysql.columns record for virtual columns
@param[in]      heap            temp memory heap
@param[in,out]  rec             mysql.columns record
@param[in,out]  table_id        Table id
@param[in,out]  pos             Position
@param[in,out]  base_pos        Base column position
@param[in,out]  n_row           Number of rows
@param[in]      dd_columns      dict_table_t obj of mysql.columns
@param[in]      mtr             Mini-transaction
@retval true if virtual info is filled */
bool dd_process_dd_virtual_columns_rec(mem_heap_t *heap, const rec_t *rec,
                                       table_id_t *table_id, ulint **pos,
                                       ulint **base_pos, ulint *n_row,
                                       dict_table_t *dd_columns, mtr_t *mtr);

/** Get next record of new DD system tables
@param[in,out]  pcur            Persistent cursor
@param[in]              mtr                     Mini-transaction
@retval next record */
const rec_t *dd_getnext_system_rec(btr_pcur_t *pcur, mtr_t *mtr);

/** Process one mysql.indexes record and get the dict_index_t
@param[in]      heap            Temp memory heap
@param[in,out]  rec             mysql.indexes record
@param[in,out]  index           dict_index_t to fill
@param[in]      mdl             MDL on index->table
@param[in,out]  parent          Parent table if it's fts aux table.
@param[in,out]  parent_mdl      MDL on parent if it's fts aux table.
@param[in]      dd_indexes      dict_table_t obj of mysql.indexes
@param[in]      mtr             Mini-transaction
@retval true if index is filled */
bool dd_process_dd_indexes_rec(mem_heap_t *heap, const rec_t *rec,
                               const dict_index_t **index, MDL_ticket **mdl,
                               dict_table_t **parent, MDL_ticket **parent_mdl,
                               dict_table_t *dd_indexes, mtr_t *mtr);

/** Process one mysql.indexes record and get brief info to dict_index_t
@param[in]      heap            temp memory heap
@param[in,out]  rec             mysql.indexes record
@param[in,out]  index_id        index id
@param[in,out]  space_id        space id
@param[in]      dd_indexes      dict_table_t obj of mysql.indexes
@retval true if index is filled */
bool dd_process_dd_indexes_rec_simple(mem_heap_t *heap, const rec_t *rec,
                                      space_index_t *index_id,
                                      space_id_t *space_id,
                                      dict_table_t *dd_indexes);

/** Process one mysql.tablespaces record and get info
@param[in]  heap            temp memory heap
@param[in]  rec             mysql.tablespaces record
@param[out] space_id        space id
@param[out] name            space name
@param[out] flags           space flags
@param[out] server_version  server version
@param[out] space_version   space version
@param[out] is_encrypted    true if tablespace is encrypted
@param[out] autoextend_size autoextend_size attribute value
@param[out] state           space state
@param[in]  dd_spaces       dict_table_t obj of mysql.tablespaces
@return true if data is retrieved */
bool dd_process_dd_tablespaces_rec(mem_heap_t *heap, const rec_t *rec,
                                   space_id_t *space_id, char **name,
                                   uint32_t *flags, uint32_t *server_version,
                                   uint32_t *space_version, bool *is_encrypted,
                                   uint64_t *autoextend_size,
                                   dd::String_type *state,
                                   dict_table_t *dd_spaces);

/** Make sure the data_dir_path is saved in dict_table_t if DATA DIRECTORY
was used. Try to read it from the fil_system first, then from new dd.
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  table           Table object
@param[in]      dd_table        DD table object
@param[in]      dict_mutex_own  true if dict_sys->mutex is owned already */
template <typename Table>
void dd_get_and_save_data_dir_path(dict_table_t *table, const Table *dd_table,
                                   bool dict_mutex_own);

/** Make sure the tablespace name is saved in dict_table_t if the table
uses a general tablespace.
Try to read it from the fil_system_t first, then from DD.
@param[in]      table           Table object
@param[in]      dd_table        Global DD table or partition object
@param[in]      dict_mutex_own  true if dict_sys->mutex is owned already */
template <typename Table>
void dd_get_and_save_space_name(dict_table_t *table, const Table *dd_table,
                                bool dict_mutex_own);

/** Get the meta-data filename from the table name for a
single-table tablespace.
@param[in,out]  table           table object
@param[in]      dd_table        DD table object
@param[out]     filename        filename
@param[in]      max_len         filename max length */
void dd_get_meta_data_filename(dict_table_t *table, dd::Table *dd_table,
                               char *filename, ulint max_len);

/** Load foreign key constraint for the table. Note, it could also open
the foreign table, if this table is referenced by the foreign table
@param[in,out]  client          data dictionary client
@param[in]      tbl_name        Table Name
@param[in]      col_names       column names, or NULL
@param[out]     m_table         InnoDB table handle
@param[in]      dd_table        Global DD table
@param[in]      thd             thread THD
@param[in]      dict_locked     True if dict_sys->mutex is already held,
                                otherwise false
@param[in]      check_charsets  whether to check charset compatibility
@param[in,out]  fk_tables       name list for tables that refer to this table
@return DB_SUCCESS      if successfully load FK constraint */
dberr_t dd_table_load_fk(dd::cache::Dictionary_client *client,
                         const char *tbl_name, const char **col_names,
                         dict_table_t *m_table, const dd::Table *dd_table,
                         THD *thd, bool dict_locked, bool check_charsets,
                         dict_names_t *fk_tables);

/** Load foreign key constraint for the table. Note, it could also open
the foreign table, if this table is referenced by the foreign table
@param[in,out]  client          data dictionary client
@param[in]      tbl_name        Table Name
@param[in]      col_names       column names, or NULL
@param[out]     m_table         InnoDB table handle
@param[in]      check_charsets  whether to check charset compatibility
@param[in]      ignore_err      DICT_ERR_IGNORE_FK_NOKEY or DICT_ERR_IGNORE_NONE
@param[in,out]  fk_tables       name list for tables that refer to this table
@return DB_SUCCESS      if successfully load FK constraint */
dberr_t dd_table_check_for_child(dd::cache::Dictionary_client *client,
                                 const char *tbl_name, const char **col_names,
                                 dict_table_t *m_table, bool check_charsets,
                                 dict_err_ignore_t ignore_err,
                                 dict_names_t *fk_tables);

/** Open uncached table definition based on a Global DD object.
@param[in]      thd             thread THD
@param[in]      client          data dictionary client
@param[in]      dd_table        Global DD table object
@param[in]      name            Table Name
@param[out]     ts              MySQL table share
@param[out]     td              MySQL table definition
@retval error number    on error
@retval 0               on success */

int acquire_uncached_table(THD *thd, dd::cache::Dictionary_client *client,
                           const dd::Table *dd_table, const char *name,
                           TABLE_SHARE *ts, TABLE *td);

/** free uncached table definition.
@param[in]      ts              MySQL table share
@param[in]      td              MySQL table definition */

void release_uncached_table(TABLE_SHARE *ts, TABLE *td);

/** Instantiate an InnoDB in-memory table metadata (dict_table_t)
based on a Global DD object or MYSQL table definition.
@param[in]      thd             thread THD
@param[in,out]  client          data dictionary client
@param[in]      dd_table        Global DD table object
@param[in]      dd_part         Global DD partition or subpartition, or NULL
@param[in]      tbl_name        table name, or NULL if not known
@param[out]     table           InnoDB table (NULL if not found or loadable)
@param[in]      td              MYSQL table definition
@return error code
@retval 0       on success */
int dd_table_open_on_dd_obj(THD *thd, dd::cache::Dictionary_client *client,
                            const dd::Table &dd_table,
                            const dd::Partition *dd_part, const char *tbl_name,
                            dict_table_t *&table, const TABLE *td);

#endif /* !UNIV_HOTBACKUP */

/** Open a persistent InnoDB table based on InnoDB table id, and
hold Shared MDL lock on it.
@param[in]      table_id                table identifier
@param[in,out]  thd                     current MySQL connection (for mdl)
@param[in,out]  mdl                     metadata lock (*mdl set if
table_id was found) mdl=NULL if we are resurrecting table IX locks in recovery
@param[in]      dict_locked             dict_sys mutex is held
@param[in]      check_corruption        check if the table is corrupted or not.
@return table
@retval NULL if the table does not exist or cannot be opened */
dict_table_t *dd_table_open_on_id(table_id_t table_id, THD *thd,
                                  MDL_ticket **mdl, bool dict_locked,
                                  bool check_corruption);

/** Close an internal InnoDB table handle.
@param[in,out]  table   InnoDB table handle
@param[in,out]  thd     current MySQL connection (for mdl)
@param[in,out]  mdl     metadata lock (will be set NULL)
@param[in]      dict_locked     whether we hold dict_sys mutex */
void dd_table_close(dict_table_t *table, THD *thd, MDL_ticket **mdl,
                    bool dict_locked);

#ifndef UNIV_HOTBACKUP
/** Set the discard flag for a non-partitioned dd table.
@param[in,out]  thd             current thread
@param[in]      table           InnoDB table
@param[in,out]  table_def       MySQL dd::Table to update
@param[in]      discard         discard flag
@return true    if success
@retval false if fail. */
bool dd_table_discard_tablespace(THD *thd, const dict_table_t *table,
                                 dd::Table *table_def, bool discard);

/** Open an internal handle to a persistent InnoDB table by name.
@param[in,out]  thd             current thread
@param[out]     mdl             metadata lock
@param[in]      name            InnoDB table name
@param[in]      dict_locked     has dict_sys mutex locked
@param[in]      ignore_err      whether to ignore err
@param[out]     error           pointer to error
@return handle to non-partitioned table
@retval NULL if the table does not exist */
dict_table_t *dd_table_open_on_name(THD *thd, MDL_ticket **mdl,
                                    const char *name, bool dict_locked,
                                    ulint ignore_err, int *error = nullptr);

/** Returns a cached table object based on table id.
This function does NOT move the table to the front of MRU, because currently it
is called in contexts where we don't really mean to "use" the table, and believe
that it would actually hurt performance if we moved it to the front, such as:
1. The table would be evicted soon anyway.
2. A batch of FTS tables would be opened from background thread,
   and it is not proper to move these tables to mru.
3. All tables in memory will be accessed sequentially, so it is useless to move.
@param[in]      table_id        table id
@param[in]      dict_locked     true=data dictionary locked
@return table, NULL if does not exist */
static inline dict_table_t *dd_table_open_on_id_in_mem(table_id_t table_id,
                                                       bool dict_locked);

/** Returns a cached table object based on table name.
@param[in]      name            table name
@param[in]      dict_locked     true=data dictionary locked
@return table, NULL if does not exist */
static inline dict_table_t *dd_table_open_on_name_in_mem(const char *name,
                                                         bool dict_locked);

MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_CLANG_WORKAROUND_TPARAM_DOCBUG()
/** Open or load a table definition based on a Global DD object.
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  client          data dictionary client
@param[in]      table           MySQL table definition
@param[in]      norm_name       Table Name
@param[in]      dd_table        Global DD table or partition object
@param[in]      thd             thread THD
@return ptr to dict_table_t filled, otherwise, nullptr */
template <typename Table>
dict_table_t *dd_open_table(dd::cache::Dictionary_client *client,
                            const TABLE *table, const char *norm_name,
                            const Table *dd_table, THD *thd);
MY_COMPILER_DIAGNOSTIC_POP()

/** Open foreign tables reference a table.
@param[in]      fk_list         foreign key name list
@param[in]      dict_locked     dict_sys mutex is locked or not
@param[in]      thd             thread THD */
void dd_open_fk_tables(dict_names_t &fk_list, bool dict_locked, THD *thd);

/** Update the tablespace name and file name for rename
operation.
@param[in]      dd_space_id     dd tablespace id
@param[in]      is_system_cs    true, if space name is in system characters set.
                                While renaming during bootstrap we have it
                                in system cs. Otherwise, in file system cs.
@param[in]      new_space_name  dd_tablespace name
@param[in]      new_path        new data file path
@retval DB_SUCCESS on success. */
dberr_t dd_tablespace_rename(dd::Object_id dd_space_id, bool is_system_cs,
                             const char *new_space_name, const char *new_path);

/** Update the data directory flag in dd::Table key strings
@param[in]      object_id       dd tablespace object id
@param[in]      path            path where the ibd file is located currently
@retval DB_SUCCESS on success. */
dberr_t dd_update_table_and_partitions_after_dir_change(dd::Object_id object_id,
                                                        std::string path);

/** Create metadata for specified tablespace, acquiring exclusive MDL first
@param[in,out]  dd_client       data dictionary client
@param[in,out]  dd_space_name   dd tablespace name
@param[in]      space_id        InnoDB tablespace ID
@param[in]      flags           InnoDB tablespace flags
@param[in]      filename        filename of this tablespace
@param[in]      discarded       true if this tablespace was discarded
@param[in,out]  dd_space_id     dd_space_id
@retval false on success
@retval true on failure */
bool dd_create_tablespace(dd::cache::Dictionary_client *dd_client,
                          const char *dd_space_name, space_id_t space_id,
                          uint32_t flags, const char *filename, bool discarded,
                          dd::Object_id &dd_space_id);

/** Create metadata for implicit tablespace
@param[in,out]  dd_client       data dictionary client
@param[in]      space_id        InnoDB tablespace ID
@param[in]      space_name      tablespace name to be set for the
                                newly created tablespace
@param[in]      filename        tablespace filename
@param[in]      discarded       true if this tablespace was discarded
@param[in,out]  dd_space_id     dd tablespace id
@retval false   on success
@retval true    on failure */
bool dd_create_implicit_tablespace(dd::cache::Dictionary_client *dd_client,
                                   space_id_t space_id, const char *space_name,
                                   const char *filename, bool discarded,
                                   dd::Object_id &dd_space_id);

/** Get the autoextend_size attribute for a tablespace.
@param[in]      dd_client       Data dictionary client
@param[in]      dd_space_id     Tablespace ID
@param[out]     autoextend_size Value of autoextend_size attribute
@retval false   On success
@retval true    On failure */
bool dd_get_tablespace_size_option(dd::cache::Dictionary_client *dd_client,
                                   const dd::Object_id dd_space_id,
                                   uint64_t *autoextend_size);

/** Drop a tablespace
@param[in,out]  dd_client       data dictionary client
@param[in]      dd_space_id     dd tablespace id
@retval false   On success
@retval true    On failure */
bool dd_drop_tablespace(dd::cache::Dictionary_client *dd_client,
                        dd::Object_id dd_space_id);

/** Obtain the private handler of InnoDB session specific data.
@param[in,out]  thd     MySQL thread handler.
@return reference to private handler */

[[nodiscard]] innodb_session_t *&thd_to_innodb_session(THD *thd);

/** Look up a column in a table using the system_charset_info collation.
@param[in]      dd_table        data dictionary table
@param[in]      name            column name
@return the column
@retval nullptr if not found */
static inline const dd::Column *dd_find_column(const dd::Table *dd_table,
                                               const char *name);

/** Add a hidden column when creating a table.
@param[in,out]  dd_table        table containing user columns and indexes
@param[in]      name            hidden column name
@param[in]      length          length of the column, in bytes
@param[in]      type            column type
@return the added column, or NULL if there already was a column by that name */
static inline dd::Column *dd_add_hidden_column(dd::Table *dd_table,
                                               const char *name, uint length,
                                               dd::enum_column_types type);

/** Add a hidden index element at the end.
@param[in,out]  index   created index metadata
@param[in]      column  column of the index */
static inline void dd_add_hidden_element(dd::Index *index,
                                         const dd::Column *column);

/** Initialize a hidden unique B-tree index.
@param[in,out]  index   created index metadata
@param[in]      name    name of the index
@param[in]      column  column of the index
@return the initialized index */
static inline dd::Index *dd_set_hidden_unique_index(dd::Index *index,
                                                    const char *name,
                                                    const dd::Column *column);

/** Check whether there exist a column named as "FTS_DOC_ID", which is
reserved for InnoDB FTS Doc ID
@param[in]      thd             MySQL thread handle
@param[in]      form            information on table
                                columns and indexes
@param[out]     doc_id_col      Doc ID column number if
                                there exist a FTS_DOC_ID column,
                                ULINT_UNDEFINED if column is of the
                                wrong type/name/size
@return true if there exist a "FTS_DOC_ID" column */
static inline bool create_table_check_doc_id_col(THD *thd, const TABLE *form,
                                                 ulint *doc_id_col);

/** Return a display name for the row format
@param[in]      row_format      Row Format
@return row format name */
static inline const char *get_row_format_name(enum row_type row_format);

/** Get the file name of a tablespace.
@param[in]      dd_space        Tablespace metadata
@return file name */
static inline const char *dd_tablespace_get_filename(
    const dd::Tablespace *dd_space) {
  ut_ad(dd_space->id() != dd::INVALID_OBJECT_ID);
  ut_ad(dd_space->files().size() == 1);
  return ((*dd_space->files().begin())->filename().c_str());
}

/** Check if the InnoDB table is consistent with dd::Table
@tparam         Table           dd::Table or dd::Partition
@param[in]      table                   InnoDB table
@param[in]      dd_table                dd::Table or dd::Partition
@return true    if match
@retval false   if not match */
template <typename Table>
bool dd_table_match(const dict_table_t *table, const Table *dd_table);

/** Create dd table for fts aux index table
@param[in]      parent_table    parent table of fts table
@param[in,out]  table           fts table
@param[in]      charset         fts index charset
@return true on success, false on failure */
bool dd_create_fts_index_table(const dict_table_t *parent_table,
                               dict_table_t *table,
                               const CHARSET_INFO *charset);

/** Create dd table for fts aux common table
@param[in]      parent_table    parent table of fts table
@param[in,out]  table           fts table
@param[in]      is_config       flag whether it's fts aux configure table
@return true on success, false on failure */
bool dd_create_fts_common_table(const dict_table_t *parent_table,
                                dict_table_t *table, bool is_config);

/** Drop dd table & tablespace for fts aux table
@param[in]      name            table name
@param[in]      file_per_table  flag whether use file per table
@return true on success, false on failure. */
bool dd_drop_fts_table(const char *name, bool file_per_table);

/** Rename dd table & tablespace files for fts aux table
@param[in]      table           dict table
@param[in]      old_name        old innodb table name
@return true on success, false on failure. */
bool dd_rename_fts_table(const dict_table_t *table, const char *old_name);

/** Open a table from its database and table name, this is currently used by
foreign constraint parser to get the referenced table.
@param[in]      name                    foreign key table name
@param[in]      database_name           table db name
@param[in]      database_name_len       db name length
@param[in]      table_name              table db name
@param[in]      table_name_len          table name length
@param[in,out]  table                   table object or NULL
@param[in,out]  mdl                     mdl on table
@param[in,out]  heap                    heap memory
@return complete table name with database and table name, allocated from
heap memory passed in */
char *dd_get_referenced_table(const char *name, const char *database_name,
                              ulint database_name_len, const char *table_name,
                              ulint table_name_len, dict_table_t **table,
                              MDL_ticket **mdl, mem_heap_t *heap);

/** Set the 'state' value in dd:tablespace::se_private_data starting with
an object id and the space name. Update the transaction when complete.
@param[in]  thd          current thread
@param[in]  dd_space_id  dd::Tablespace
@param[in]  space_name   tablespace name
@param[in]  state        value to set for key 'state'. */
void dd_tablespace_set_state(THD *thd, dd::Object_id dd_space_id,
                             std::string space_name, dd_space_states state);

/** Set the 'state' value in dd:tablespace::se_private_data.
The caller will update the transaction.
@param[in,out]  dd_space        dd::Tablespace object
@param[in]      state           value to set for key 'state' */
void dd_tablespace_set_state(dd::Tablespace *dd_space, dd_space_states state);

/** Set Space ID and state attribute in se_private_data of mysql.tablespaces
for the named tablespace.
@param[in]  space_name  tablespace name
@param[in]  space_id    tablespace id
@param[in]  state       value to set for key 'state'
@return DB_SUCCESS or DD_FAILURE. */
bool dd_tablespace_set_id_and_state(const char *space_name, space_id_t space_id,
                                    dd_space_states state);

/** Get state attribute value in dd::Tablespace::se_private_data
@param[in]     dd_space  dd::Tablespace object
@param[in,out] state     tablespace state attribute
@param[in]     space_id  tablespace ID */
void dd_tablespace_get_state(const dd::Tablespace *dd_space,
                             dd::String_type *state,
                             space_id_t space_id = SPACE_UNKNOWN);

/** Get state attribute value in dd::Tablespace::se_private_data
@param[in]     p         dd::Properties for dd::Tablespace::se_private_data
@param[in,out] state  tablespace state attribute
@param[in]     space_id  tablespace ID */
void dd_tablespace_get_state(const dd::Properties *p, dd::String_type *state,
                             space_id_t space_id = SPACE_UNKNOWN);

/** Get the enum for the state of the undo tablespace
from either dd::Tablespace::se_private_data or undo::Tablespace
@param[in]  dd_space  dd::Tablespace object
@param[in]  space_id  tablespace ID
@return enumerated value associated with the key 'state' */
dd_space_states dd_tablespace_get_state_enum(
    const dd::Tablespace *dd_space, space_id_t space_id = SPACE_UNKNOWN);

/** Get the enum for the state of a tablespace
from either dd::Tablespace::se_private_data or undo::Tablespace
@param[in]  p         dd::Properties for dd::Tablespace::se_private_data
@param[in]  space_id  tablespace ID
@return enumerated value associated with the key 'state' */
dd_space_states dd_tablespace_get_state_enum(
    const dd::Properties *p, space_id_t space_id = SPACE_UNKNOWN);

/** Get the enum for the state of a tablespace. Try the old 'discarded'
key value for IBD spaces or undo::Tablespace.
@param[in]  p         dd::Properties for dd::Tablespace::se_private_data
@param[in]  space_id  tablespace ID
@return enumerated value associated with the key 'state' */
dd_space_states dd_tablespace_get_state_enum_legacy(
    const dd::Properties *p, space_id_t space_id = SPACE_UNKNOWN);

/** Get the discarded state from se_private_data of tablespace
@param[in]      dd_space        dd::Tablespace object */
bool dd_tablespace_is_discarded(const dd::Tablespace *dd_space);

/** Set the autoextend_size attribute for an implicit tablespace
@param[in,out]  dd_client       Data dictionary client
@param[in]      dd_space_id     DD tablespace id
@param[in]      create_info     HA_CREATE_INFO object
@return false   On success
@return true    On failure */
bool dd_implicit_alter_tablespace(dd::cache::Dictionary_client *dd_client,
                                  dd::Object_id dd_space_id,
                                  HA_CREATE_INFO *create_info);

/** Get the MDL for the named tablespace.  The mdl_ticket pointer can
be provided if it is needed by the caller.  If foreground is set to false,
then the caller must explicitly release that ticket with dd_release_mdl().
Otherwise, it will ne released with the transaction.
@param[in]  space_name  tablespace name
@param[in]  mdl_ticket  tablespace MDL ticket, default to nullptr
@param[in]  foreground  true, if the caller is foreground thread. Default
                        is true. For foreground, the lock duration is
                        MDL_TRANSACTION. Otherwise, it is MDL_EXPLICIT.
@return DD_SUCCESS or DD_FAILURE. */
bool dd_tablespace_get_mdl(const char *space_name,
                           MDL_ticket **mdl_ticket = nullptr,
                           bool foreground = true);
/** Set discard attribute value in se_private_dat of tablespace
@param[in]  dd_space  dd::Tablespace object
@param[in]  discard   true if discarded, else false */
void dd_tablespace_set_discard(dd::Tablespace *dd_space, bool discard);

/** Get discard attribute value stored in se_private_dat of tablespace
@param[in]      dd_space        dd::Tablespace object
@retval         true            if Tablespace is discarded
@retval         false           if attribute doesn't exist or if the
                                tablespace is not discarded */
bool dd_tablespace_get_discard(const dd::Tablespace *dd_space);

/** Release the MDL held by the given ticket.
@param[in]  mdl_ticket  tablespace MDL ticket */
void dd_release_mdl(MDL_ticket *mdl_ticket);

/** Copy metadata of already dropped columns from old table def to new
table def.
param[in]     old_dd_table  old table definition
param[in,out] new_dd_table  new table definition
@retval true Failure
@retval false Success */
bool copy_dropped_columns(const dd::Table *old_dd_table,
                          dd::Table *new_dd_table,
                          uint32_t current_row_version);

/** Set Innodb tablespace compression option from DD.
@param[in,out]  client          dictionary client
@param[in]      algorithm       compression algorithm
@param[in]      dd_space_id     DD tablespace ID.
@return true, if failed to set compression. */
bool dd_set_tablespace_compression(dd::cache::Dictionary_client *client,
                                   const char *algorithm,
                                   dd::Object_id dd_space_id);

#endif /* !UNIV_HOTBACKUP */

/** Update all InnoDB tablespace cache objects. This step is done post
dictionary trx rollback, binlog recovery and DDL_LOG apply. So DD is
consistent. Update the cached tablespace objects, if they differ from
the dictionary.
@param[in,out]  thd     thread handle
@retval true    on error
@retval false   on success */

[[nodiscard]] bool dd_tablespace_update_cache(THD *thd);

/* Check if the table belongs to an encrypted tablespace.
@return true if it does. */
bool dd_is_table_in_encrypted_tablespace(const dict_table_t *table);

/** Parse the default value from dd::Column::se_private to dict_col_t
@param[in]      se_private_data dd::Column::se_private
@param[in,out]  col             InnoDB column object
@param[in,out]  heap            Heap to store the default value */
void dd_parse_default_value(const dd::Properties &se_private_data,
                            dict_col_t *col, mem_heap_t *heap);

#ifndef UNIV_HOTBACKUP
/** Add definition of INSTANT dropped column in table cache.
@param[in]      dd_table        Table definition
@param[in,out]  dict_table      Table cache
@param[out]     current_row_version     row_version
@param[in]      heap            heap */
void fill_dict_dropped_columns(const dd::Table *dd_table,
                               dict_table_t *dict_table
                                   IF_DEBUG(, uint32_t &current_row_version),
                               mem_heap_t *heap);

/** Check if given column is renamed during ALTER.
@param[in]      ha_alter_info   alter info
@param[in]      old_name        column old name
@param[out]     new_name        column new name
@return true if column is renamed, false otherwise. */
bool is_renamed(const Alter_inplace_info *ha_alter_info, const char *old_name,
                std::string &new_name);

/** Check if given column is dropped during ALTER.
@param[in]      ha_alter_info   alter info
@param[in]      column_name     Column name
@return true if column is dropped, false otherwise. */
bool is_dropped(const Alter_inplace_info *ha_alter_info,
                const char *column_name);

/** Get the mtype, prtype and len for a field.
@param[in]   dd_tab  dd table definition
@param[in]   m_table innodb table cache
@param[in]   field   MySQL field
@param[out]  col_len length
@param[out]  mtype   mtype
@param[out]  prtype  prtype */
void get_field_types(const dd::Table *dd_tab, const dict_table_t *m_table,
                     const Field *field, unsigned &col_len, ulint &mtype,
                     ulint &prtype);
#endif

#include "dict0dd.ic"
#endif
