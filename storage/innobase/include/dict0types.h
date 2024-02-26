/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/dict0types.h
 Data dictionary global types

 Created 1/8/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict0types_h
#define dict0types_h

#include "fsp0types.h"
#include "ibuf0types.h" /* IBUF_SPACE_ID */
#include "mysql_com.h"
#include "rem0types.h"
#include "ut0mutex.h"

struct dict_sys_t;
struct dict_col_t;
struct dict_field_t;
struct dict_index_t;
struct dict_table_t;
struct dict_foreign_t;
struct dict_v_col_t;

struct ind_node_t;
struct tab_node_t;
struct dict_add_v_col_t;

namespace dd {
class Partition;
}

/** Innodb data dictionary name.
NOTE: Innodb dictionary table name is always in my_charset_filename. Hence,
dictionary name (dict_name) and partition string input parameters in dict_name::
interfaces are assumed to be in my_charset_filename. */
namespace dict_name {
/** Partition separator in dictionary table name and file name. */
constexpr char PART_SEPARATOR[] = "#p#";

/** Partition separator length excluding terminating NULL */
constexpr size_t PART_SEPARATOR_LEN = sizeof(PART_SEPARATOR) - 1;

/** Sub-Partition separator in dictionary table name and file name. */
constexpr char SUB_PART_SEPARATOR[] = "#sp#";

/** Sub-Partition separator length excluding terminating NULL */
constexpr size_t SUB_PART_SEPARATOR_LEN = sizeof(SUB_PART_SEPARATOR) - 1;

/** Alternative partition separator from 8.0.17 and older versions. */
constexpr char ALT_PART_SEPARATOR[] = "#P#";

/** Alternative sub-partition separator from 8.0.17 and older versions. */
constexpr char ALT_SUB_PART_SEPARATOR[] = "#SP#";

/** Schema separator is forward slash irrespective of platform. */
constexpr char SCHEMA_SEPARATOR[] = "/";
constexpr size_t SCHEMA_SEPARATOR_LEN = sizeof(SCHEMA_SEPARATOR) - 1;

/** The maximum length in bytes that a database name can occupy when
stored in UTF8MB3, including the terminating null. */
constexpr size_t MAX_DB_UTF8MB3_LEN = NAME_LEN + 1;

/** The maximum length in characters for database name. */
constexpr size_t MAX_DB_CHAR_LEN = NAME_CHAR_LEN;

/** The maximum length in bytes that a table name can occupy when stored in
UTF8MB3, including the terminating null. NAME_LEN is added 3 times to consider
table name, partition name and sub-partition name for a partitioned table.
In innodb each partition/sub-partition is a separate table named as below.
table_name<PART_SEPARATOR>partition_name<SUB_PART_SEPARATOR>subpartition_name
This macro only applies to table name, without any database name prefixed. */
constexpr size_t MAX_TABLE_UTF8MB3_LEN = NAME_LEN + PART_SEPARATOR_LEN +
                                         NAME_LEN + SUB_PART_SEPARATOR_LEN +
                                         NAME_LEN + 1;

/** The maximum length in characters for table name. */
constexpr size_t MAX_TABLE_CHAR_LEN = NAME_CHAR_LEN + PART_SEPARATOR_LEN +
                                      NAME_CHAR_LEN + SUB_PART_SEPARATOR_LEN +
                                      NAME_CHAR_LEN;

/** Postfix for a table name which is being altered. Since during
ALTER TABLE ... PARTITION, new partitions have to be created before
dropping existing partitions, so a postfix is appended to the name
to prevent name conflicts. This is also used for EXCHANGE PARTITION */
constexpr char TMP_POSTFIX[] = "#tmp";
constexpr size_t TMP_POSTFIX_LEN = sizeof(TMP_POSTFIX) - 1;

/** Maximum space name length. Space name includes schema name, table name
along with partition and sub-partition name for partitioned table. */
constexpr size_t MAX_SPACE_NAME_LEN =
    NAME_LEN + SCHEMA_SEPARATOR_LEN + NAME_LEN + PART_SEPARATOR_LEN + NAME_LEN +
    SUB_PART_SEPARATOR_LEN + NAME_LEN + TMP_POSTFIX_LEN;

/** Name string conversion callback. Used for character set conversion. */
using Convert_Func = std::function<void(std::string &)>;

/** Conversion function to change for system to file name cs.
@param[in,out]  name    identifier name.
@param[in]      quiet   true, if we allow error during conversion. */
void file_to_table(std::string &name, bool quiet);

/** Conversion function to change for file name to system cs.
@param[in,out]  name    identifier name. */
void table_to_file(std::string &name);

/** Check if it is a table partition.
@param[in]      dict_name       table name in dictionary
@return true, iff it is table partition. */
bool is_partition(const std::string &dict_name);

/** Get schema, table name, partition string and temporary attribute from
dictionary table name.
@param[in]      dict_name       table name in dictionary
@param[in]      convert         convert schema & table name to system cs
@param[out]     schema          schema name
@param[out]     table           table name
@param[out]     partition       partition string if table partition
@param[out]     is_tmp          true, iff temporary table created by DDL */
void get_table(const std::string &dict_name, bool convert, std::string &schema,
               std::string &table, std::string &partition, bool &is_tmp);

/** Get schema and table name from dictionary table name.
@param[in]      dict_name       table name in dictionary
@param[out]     schema          schema name
@param[out]     table           table name */
void get_table(const std::string &dict_name, std::string &schema,
               std::string &table);

/** Get partition and sub-partition name from partition string
@param[in]      partition       partition string from dictionary table name
@param[in]      convert         convert partition names to system cs
@param[out]     part            partition name
@param[out]     sub_part        sub partition name if present */
void get_partition(const std::string &partition, bool convert,
                   std::string &part, std::string &sub_part);

/* Build dictionary table name. Table name in dictionary is always in filename
character set.
@param[in]      schema          schema name
@param[in]      table           table name
@param[in]      partition       partition string if table partition
@param[in]      is_tmp          true, iff temporary table created by DDL
@param[in]      convert         convert all names from system cs
@param[out]     dict_name       table name for innodb dictionary */
void build_table(const std::string &schema, const std::string &table,
                 const std::string &partition, bool is_tmp, bool convert,
                 std::string &dict_name);

/** Build partition string from dd object.
@param[in]      dd_part         partition object from DD
@param[out]     partition       partition string for dictionary table name */
void build_partition(const dd::Partition *dd_part, std::string &partition);

/** Build 5.7 style partition string from dd object.
@param[in]      dd_part         partition object from DD
@param[out]     partition       partition string for dictionary table name */
void build_57_partition(const dd::Partition *dd_part, std::string &partition);

/** Check if dd partition matches with innodb dictionary table name.
@param[in]      dict_name       table name in innodb dictionary
@param[in]      dd_part         partition object from DD
@return true, iff the name matches the partition from DD. */
bool match_partition(const std::string &dict_name,
                     const dd::Partition *dd_part);

/* Build space name by converting schema, table, partition and sub partition
names within a dictionary table name.
@param[in,out]  dict_name       table name in dictionary */
void convert_to_space(std::string &dict_name);

/* Rebuild space name by replacing partition string from dictionary table name.
@param[in]      dict_name       table name in dictionary
@param[in,out]  space_name      space name to be rebuilt */
void rebuild_space(const std::string &dict_name, std::string &space_name);

/** Rebuild table name to convert from 5.7 format to 8.0.
@param[in,out]  dict_name       table name in dictionary */
void rebuild(std::string &dict_name);

}  // namespace dict_name

/* Space id and page no where the dictionary header resides */
constexpr uint32_t DICT_HDR_SPACE = 0; /* the SYSTEM tablespace */
constexpr uint32_t DICT_HDR_PAGE_NO = FSP_DICT_HDR_PAGE_NO;

/* The ibuf table and indexes's ID are assigned as the number
DICT_IBUF_ID_MIN plus the space id */
constexpr uint64_t DICT_IBUF_ID_MIN = 0xFFFFFFFF00000000ULL;

/** Table or partition identifier (unique within an InnoDB instance). */
typedef ib_id_t table_id_t;
/** Index identifier (unique within a tablespace). */
typedef ib_id_t space_index_t;

/** Globally unique index identifier */
class index_id_t {
 public:
  /** Constructor.
  @param[in]    space_id        Tablespace identifier
  @param[in]    index_id        Index identifier */
  index_id_t(space_id_t space_id, space_index_t index_id)
      : m_space_id(space_id), m_index_id(index_id) {}

  /** Compare this to another index identifier.
  @param other  the other index identifier
  @return whether this is less than other */
  bool operator<(const index_id_t &other) const {
    return (m_space_id < other.m_space_id ||
            (m_space_id == other.m_space_id && m_index_id < other.m_index_id));
  }
  /** Compare this to another index identifier.
  @param other  the other index identifier
  @return whether the identifiers are equal */
  bool operator==(const index_id_t &other) const {
    return (m_space_id == other.m_space_id && m_index_id == other.m_index_id);
  }

  /** Convert an index_id to a 64 bit integer.
  @return a 64 bit integer */
  uint64_t conv_to_int() const {
    ut_ad((m_index_id & 0xFFFFFFFF00000000ULL) == 0);

    return (static_cast<uint64_t>(m_space_id) << 32 | m_index_id);
  }

  /** Check if the index belongs to the insert buffer.
  @return true if the index belongs to the insert buffer */
  bool is_ibuf() const {
    return (m_space_id == IBUF_SPACE_ID &&
            m_index_id == DICT_IBUF_ID_MIN + IBUF_SPACE_ID);
  }

  /** Tablespace identifier */
  space_id_t m_space_id;
  /** Index identifier within the tablespace */
  space_index_t m_index_id;
};

/** Display an index identifier.
@param[in,out]  out     the output stream
@param[in]      id      index identifier
@return the output stream */
inline std::ostream &operator<<(std::ostream &out, const index_id_t &id) {
  return (out << "[space=" << id.m_space_id << ",index=" << id.m_index_id
              << "]");
}

/** Error to ignore when we load table dictionary into memory. However,
the table and index will be marked as "corrupted", and caller will
be responsible to deal with corrupted table or index.
Note: please define the IGNORE_ERR_* as bits, so their value can
be or-ed together */
enum dict_err_ignore_t {
  DICT_ERR_IGNORE_NONE = 0,       /*!< no error to ignore */
  DICT_ERR_IGNORE_INDEX_ROOT = 1, /*!< ignore error if index root
                                  page is FIL_NULL or incorrect value */
  DICT_ERR_IGNORE_CORRUPT = 2,    /*!< skip corrupted indexes */
  DICT_ERR_IGNORE_FK_NOKEY = 4,   /*!< ignore error if any foreign
                                  key is missing */
  DICT_ERR_IGNORE_RECOVER_LOCK = 8,
  /*!< Used when recovering table locks
  for resurrected transactions.
  Silently load a missing
  tablespace, and do not load
  incomplete index definitions. */
  DICT_ERR_IGNORE_ALL = 0xFFFF /*!< ignore all errors */
};

/** Quiescing states for flushing tables to disk. */
enum ib_quiesce_t {
  QUIESCE_NONE,
  QUIESCE_START,   /*!< Initialise, prepare to start */
  QUIESCE_COMPLETE /*!< All done */
};

#ifndef UNIV_HOTBACKUP
typedef ib_mutex_t DictSysMutex;
#endif /* !UNIV_HOTBACKUP */

/** Prefix for tmp tables, adopted from sql/table.h */
#define TEMP_FILE_PREFIX "#sql"
#define TEMP_FILE_PREFIX_LENGTH 4
#define TEMP_FILE_PREFIX_INNODB "#sql-ib"

#define TEMP_TABLE_PREFIX "#sql"
#define TEMP_TABLE_PATH_PREFIX "/" TEMP_TABLE_PREFIX

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
extern uint ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** Shift for spatial status */
constexpr uint32_t SPATIAL_STATUS_SHIFT = 12;

/** Mask to encode/decode spatial status. */
constexpr uint32_t SPATIAL_STATUS_MASK = 3 << SPATIAL_STATUS_SHIFT;

static_assert(SPATIAL_STATUS_MASK >= REC_VERSION_56_MAX_INDEX_COL_LEN,
              "SPATIAL_STATUS_MASK < REC_VERSION_56_MAX_INDEX_COL_LEN");

/** whether a col is used in spatial index or regular index
Note: the spatial status is part of persistent undo log,
so we should not modify the values in MySQL 5.7 */
enum spatial_status_t {
  /* Unknown status (undo format in 5.7.9) */
  SPATIAL_UNKNOWN = 0,

  /** Not used in gis index. */
  SPATIAL_NONE = 1,

  /** Used in both spatial index and regular index. */
  SPATIAL_MIXED = 2,

  /** Only used in spatial index. */
  SPATIAL_ONLY = 3
};

#endif
