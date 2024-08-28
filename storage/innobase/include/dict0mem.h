/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

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

/** @file include/dict0mem.h
 Data dictionary memory object creation

 Created 1/8/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict0mem_h
#define dict0mem_h

#include "sql/dd/object_id.h"
#include "sql/dd/types/column.h"
#include "univ.i"
#if defined UNIV_COMPILE_TEST_FUNCS || defined UNIV_HOTBACKUP
#include "sql/dd/types/spatial_reference_system.h"
#endif /* UNIV_COMPILE_TEST_FUNCS || UNIV_HOTBACKUP */
#include "btr0types.h"
#include "data0type.h"
#include "dict0types.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "row0types.h"
#ifndef UNIV_HOTBACKUP
#include "lock0types.h"
#include "que0types.h"
#endif /* !UNIV_HOTBACKUP */
#include "hash0hash.h"
#include "sync0rw.h"
#include "trx0types.h"
#include "ut0byte.h"
#include "ut0mem.h"
#include "ut0rnd.h"
#ifndef UNIV_HOTBACKUP
#include "fts0fts.h"
#endif /* !UNIV_HOTBACKUP */
#include "buf0buf.h"
#include "gis0type.h"
#ifndef UNIV_HOTBACKUP
#include "os0once.h"
#endif /* !UNIV_HOTBACKUP */
#include "dict/mem.h"
#include "ut0new.h"

#include "sql/sql_const.h" /* MAX_KEY_LENGTH */
#include "sql/table.h"

#include <algorithm>
#include <iterator>
#include <memory> /* std::unique_ptr */
#include <set>
#include <string>
#include <vector>

/* Forward declaration. */
struct ib_rbt_t;

/** Format of INSTANTLY DROPPED column names. */
constexpr char INSTANT_DROP_SUFFIX_8_0_29[] = "_dropped_v";
constexpr char INSTANT_DROP_PREFIX_8_0_32[] = "!hidden!_dropped_";

/** index/table name used while applying REDO logs during recovery */
constexpr char RECOVERY_INDEX_TABLE_NAME[] = "LOG_DUMMY";

/** Type flags of an index: OR'ing of the flags is allowed to define a
combination of types */
/** @{ */
/** clustered index; for other than  auto-generated clustered indexes, also
 DICT_UNIQUE will be set */
constexpr uint32_t DICT_CLUSTERED = 1;
/** unique index */
constexpr uint32_t DICT_UNIQUE = 2;
/** insert buffer tree */
constexpr uint32_t DICT_IBUF = 8;
/** bit to store the corrupted flag  in SYS_INDEXES.TYPE */
constexpr uint32_t DICT_CORRUPT = 16;
/** FTS index; can't be combined with the other flags */
constexpr uint32_t DICT_FTS = 32;
/** SPATIAL index; can't be combined with the other flags */
constexpr uint32_t DICT_SPATIAL = 64;
/** Index on Virtual column */
constexpr uint32_t DICT_VIRTUAL = 128;
/* Tablespace dictionary Index. Set only in in-memory index structure. */
constexpr uint32_t DICT_SDI = 256;
/** Multi-value index */
constexpr uint32_t DICT_MULTI_VALUE = 512;

/** number of bits used for SYS_INDEXES.TYPE */
constexpr uint32_t DICT_IT_BITS = 10;
/** @} */

#if 0                         /* not implemented, retained for history */
/** Types for a table object */
#define DICT_TABLE_ORDINARY 1 /*!< ordinary table */
#define DICT_TABLE_CLUSTER_MEMBER 2
#define DICT_TABLE_CLUSTER          \
  3 /* this means that the table is \
really a cluster definition */
#endif

/* Table and tablespace flags are generally not used for the Antelope file
format except for the low order bit, which is used differently depending on
where the flags are stored.

==================== Low order flags bit =========================
                    | REDUNDANT | COMPACT | COMPRESSED and DYNAMIC
SYS_TABLES.TYPE     |     1     |    1    |     1
dict_table_t::flags |     0     |    1    |     1
FSP_SPACE_FLAGS     |     0     |    0    |     1
fil_space_t::flags  |     0     |    0    |     1

Before the 5.1 plugin, SYS_TABLES.TYPE was always DICT_TABLE_ORDINARY (1)
and the tablespace flags field was always 0. In the 5.1 plugin, these fields
were repurposed to identify compressed and dynamic row formats.

The following types and constants describe the flags found in dict_table_t
and SYS_TABLES.TYPE.  Similar flags found in fil_space_t and FSP_SPACE_FLAGS
are described in fsp0fsp.h. */

/** @{ */
/** dict_table_t::flags bit 0 is equal to 0 if the row format = Redundant */
/** Redundant row format. */
constexpr uint32_t DICT_TF_REDUNDANT = 0;
/** dict_table_t::flags bit 0 is equal to 1 if the row format = Compact */
/** Compact row format. */
constexpr uint32_t DICT_TF_COMPACT = 1;

/** This bitmask is used in SYS_TABLES.N_COLS to set and test whether
the Compact page format is used, i.e ROW_FORMAT != REDUNDANT */
constexpr uint32_t DICT_N_COLS_COMPACT = 0x80000000UL;

/** Width of the COMPACT flag */
constexpr uint32_t DICT_TF_WIDTH_COMPACT = 1;

/** Width of the ZIP_SSIZE flag */
constexpr uint32_t DICT_TF_WIDTH_ZIP_SSIZE = 4;

/** Width of the ATOMIC_BLOBS flag.  The ROW_FORMAT=REDUNDANT and
ROW_FORMAT=COMPACT broke up BLOB and TEXT fields, storing the first 768 bytes
in the clustered index. ROW_FORMAT=DYNAMIC and ROW_FORMAT=COMPRESSED
store the whole blob or text field off-page atomically.
Secondary indexes are created from this external data using row_ext_t
to cache the BLOB prefixes. */
constexpr uint32_t DICT_TF_WIDTH_ATOMIC_BLOBS = 1;

/** If a table is created with the MYSQL option DATA DIRECTORY and
innodb-file-per-table, an older engine will not be able to find that table.
This flag prevents older engines from attempting to open the table and
allows InnoDB to update_create_info() accordingly. */
constexpr uint32_t DICT_TF_WIDTH_DATA_DIR = 1;

/** Width of the SHARED tablespace flag.
It is used to identify tables that exist inside a shared general tablespace.
If a table is created with the TABLESPACE=tsname option, an older engine will
not be able to find that table. This flag prevents older engines from attempting
to open the table and allows InnoDB to quickly find the tablespace. */

constexpr uint32_t DICT_TF_WIDTH_SHARED_SPACE = 1;

/** Width of all the currently known table flags */
constexpr uint32_t DICT_TF_BITS =
    DICT_TF_WIDTH_COMPACT + DICT_TF_WIDTH_ZIP_SSIZE +
    DICT_TF_WIDTH_ATOMIC_BLOBS + DICT_TF_WIDTH_DATA_DIR +
    DICT_TF_WIDTH_SHARED_SPACE;

/** A mask of all the known/used bits in table flags */
constexpr uint32_t DICT_TF_BIT_MASK = ~(~0U << DICT_TF_BITS);

/** Zero relative shift position of the COMPACT field */
constexpr uint32_t DICT_TF_POS_COMPACT = 0;
/** Zero relative shift position of the ZIP_SSIZE field */
constexpr uint32_t DICT_TF_POS_ZIP_SSIZE =
    DICT_TF_POS_COMPACT + DICT_TF_WIDTH_COMPACT;
/** Zero relative shift position of the ATOMIC_BLOBS field */
constexpr uint32_t DICT_TF_POS_ATOMIC_BLOBS =
    DICT_TF_POS_ZIP_SSIZE + DICT_TF_WIDTH_ZIP_SSIZE;
/** Zero relative shift position of the DATA_DIR field */
constexpr uint32_t DICT_TF_POS_DATA_DIR =
    DICT_TF_POS_ATOMIC_BLOBS + DICT_TF_WIDTH_ATOMIC_BLOBS;
/** Zero relative shift position of the SHARED TABLESPACE field */
constexpr uint32_t DICT_TF_POS_SHARED_SPACE =
    DICT_TF_POS_DATA_DIR + DICT_TF_WIDTH_DATA_DIR;
/** Zero relative shift position of the start of the UNUSED bits */
constexpr uint32_t DICT_TF_POS_UNUSED =
    DICT_TF_POS_SHARED_SPACE + DICT_TF_WIDTH_SHARED_SPACE;

/** Bit mask of the COMPACT field */
constexpr uint32_t DICT_TF_MASK_COMPACT = (~(~0U << DICT_TF_WIDTH_COMPACT))
                                          << DICT_TF_POS_COMPACT;
/** Bit mask of the ZIP_SSIZE field */
constexpr uint32_t DICT_TF_MASK_ZIP_SSIZE = (~(~0U << DICT_TF_WIDTH_ZIP_SSIZE))
                                            << DICT_TF_POS_ZIP_SSIZE;
/** Bit mask of the ATOMIC_BLOBS field */
constexpr uint32_t DICT_TF_MASK_ATOMIC_BLOBS =
    (~(~0U << DICT_TF_WIDTH_ATOMIC_BLOBS)) << DICT_TF_POS_ATOMIC_BLOBS;
/** Bit mask of the DATA_DIR field */
constexpr uint32_t DICT_TF_MASK_DATA_DIR = (~(~0U << DICT_TF_WIDTH_DATA_DIR))
                                           << DICT_TF_POS_DATA_DIR;
/** Bit mask of the SHARED_SPACE field */
constexpr uint32_t DICT_TF_MASK_SHARED_SPACE =
    (~(~0U << DICT_TF_WIDTH_SHARED_SPACE)) << DICT_TF_POS_SHARED_SPACE;

/** Return the value of the COMPACT field */
inline uint32_t DICT_TF_GET_COMPACT(uint32_t flags) {
  return (flags & DICT_TF_MASK_COMPACT) >> DICT_TF_POS_COMPACT;
}
/** Return the value of the ZIP_SSIZE field */
inline uint32_t DICT_TF_GET_ZIP_SSIZE(uint32_t flags) {
  return (flags & DICT_TF_MASK_ZIP_SSIZE) >> DICT_TF_POS_ZIP_SSIZE;
}
/** Return the value of the ATOMIC_BLOBS field */
inline uint32_t DICT_TF_HAS_ATOMIC_BLOBS(uint32_t flags) {
  return (flags & DICT_TF_MASK_ATOMIC_BLOBS) >> DICT_TF_POS_ATOMIC_BLOBS;
}
/** Return the value of the DATA_DIR field */
inline uint32_t DICT_TF_HAS_DATA_DIR(uint32_t flags) {
  return (flags & DICT_TF_MASK_DATA_DIR) >> DICT_TF_POS_DATA_DIR;
}
/** Return the value of the SHARED_SPACE field */
inline uint32_t DICT_TF_HAS_SHARED_SPACE(uint32_t flags) {
  return (flags & DICT_TF_MASK_SHARED_SPACE) >> DICT_TF_POS_SHARED_SPACE;
}
/** Return the contents of the UNUSED bits */
inline uint32_t DICT_TF_GET_UNUSED(uint32_t flags) {
  return flags >> DICT_TF_POS_UNUSED;
}
/** @} */

/** @brief Table Flags set number 2.

These flags will be stored in SYS_TABLES.MIX_LEN.  All unused flags
will be written as 0.  The column may contain garbage for tables
created with old versions of InnoDB that only implemented
ROW_FORMAT=REDUNDANT.  InnoDB engines do not check these flags
for unknown bits in order to protect backward incompatibility. */
/** @{ */
/** Total number of bits in table->flags2. */
constexpr uint32_t DICT_TF2_BITS = 11;
constexpr uint32_t DICT_TF2_UNUSED_BIT_MASK = ~0U << DICT_TF2_BITS;
constexpr uint32_t DICT_TF2_BIT_MASK = ~DICT_TF2_UNUSED_BIT_MASK;

/** TEMPORARY; true for tables from CREATE TEMPORARY TABLE. */
constexpr uint32_t DICT_TF2_TEMPORARY = 1;

/** The table has an internal defined DOC ID column */
constexpr uint32_t DICT_TF2_FTS_HAS_DOC_ID = 2;

/** The table has an FTS index */
constexpr uint32_t DICT_TF2_FTS = 4;

/** Need to add Doc ID column for FTS index build.
This is a transient bit for index build */
constexpr uint32_t DICT_TF2_FTS_ADD_DOC_ID = 8;

/** This bit is used during table creation to indicate that it will
use its own tablespace instead of the system tablespace. */
constexpr uint32_t DICT_TF2_USE_FILE_PER_TABLE = 16;

/** Set when we discard/detach the tablespace */
constexpr uint32_t DICT_TF2_DISCARDED = 32;

/** Intrinsic table bit
Intrinsic table is table created internally by MySQL modules viz. Optimizer,
FTS, etc.... Intrinsic table has all the properties of the normal table except
it is not created by user and so not visible to end-user. */
constexpr uint32_t DICT_TF2_INTRINSIC = 128;

/** Encryption table bit for innodb_file-per-table only. */
constexpr uint32_t DICT_TF2_ENCRYPTION_FILE_PER_TABLE = 256;

/** FTS AUX hidden table bit. */
constexpr uint32_t DICT_TF2_AUX = 512;

/** Table is opened by resurrected trx during crash recovery. */
constexpr uint32_t DICT_TF2_RESURRECT_PREPARED = 1024;
/** @} */

/** Tables could be chained together with Foreign key constraint. When
first load the parent table, we would load all of its descedents.
This could result in rescursive calls and out of stack error eventually.
DICT_FK_MAX_RECURSIVE_LOAD defines the maximum number of recursive loads,
when exceeded, the child table will not be loaded. It will be loaded when
the foreign constraint check needs to be run. */
constexpr uint32_t DICT_FK_MAX_RECURSIVE_LOAD = 20;

/** Similarly, when tables are chained together with foreign key constraints
with on cascading delete/update clause, delete from parent table could
result in recursive cascading calls. This defines the maximum number of
such cascading deletes/updates allowed. When exceeded, the delete from
parent table will fail, and user has to drop excessive foreign constraint
before proceeds. */
constexpr uint32_t FK_MAX_CASCADE_DEL = 15;

/** Adds a virtual column definition to a table.
@param[in,out]  table           table
@param[in]      heap            temporary memory heap, or NULL. It is
                                used to store name when we have not finished
                                adding all columns. When all columns are
                                added, the whole name will copy to memory from
                                table->heap
@param[in]      name            column name
@param[in]      mtype           main datatype
@param[in]      prtype          precise type
@param[in]      len             length
@param[in]      pos             position in a table
@param[in]      num_base        number of base columns
@param[in]      is_visible      True if virtual column is visible to user
@return the virtual column definition */
dict_v_col_t *dict_mem_table_add_v_col(dict_table_t *table, mem_heap_t *heap,
                                       const char *name, ulint mtype,
                                       ulint prtype, ulint len, ulint pos,
                                       ulint num_base, bool is_visible);

/** Adds a stored column definition to a table.
@param[in,out]  table           table
@param[in]      num_base        number of base columns. */
void dict_mem_table_add_s_col(dict_table_t *table, ulint num_base);

/** Renames a column of a table in the data dictionary cache.
@param[in,out] table Table
@param[in] nth_col Column index
@param[in] from Old column name
@param[in] to New column name
@param[in] is_virtual If this is a virtual column */
void dict_mem_table_col_rename(dict_table_t *table, ulint nth_col,
                               const char *from, const char *to,
                               bool is_virtual);

/** This function poplulates a dict_index_t index memory structure with
supplied information.
@param[out]     index           index to be filled
@param[in]      heap            memory heap
@param[in]      table_name      table name
@param[in]      index_name      index name
@param[in]      space           space where the index tree is placed, the
                                clustered type ignored if the index is of
the clustered type
@param[in]      type            DICT_UNIQUE, DICT_CLUSTERED, ... ORed
@param[in]      n_fields        number of fields */
static inline void dict_mem_fill_index_struct(
    dict_index_t *index, mem_heap_t *heap, const char *table_name,
    const char *index_name, ulint space, ulint type, ulint n_fields);

/** Frees an index memory object. */
void dict_mem_index_free(dict_index_t *index); /*!< in: index */
/** Creates and initializes a foreign constraint memory object.
 @return own: foreign constraint struct */
dict_foreign_t *dict_mem_foreign_create(void);

/** Sets the foreign_table_name_lookup pointer based on the value of
 lower_case_table_names.  If that is 0 or 1, foreign_table_name_lookup
 will point to foreign_table_name.  If 2, then another string is
 allocated from the heap and set to lower case. */
void dict_mem_foreign_table_name_lookup_set(
    dict_foreign_t *foreign, /*!< in/out: foreign struct */
    bool do_alloc);          /*!< in: is an alloc needed */

/** Sets the referenced_table_name_lookup pointer based on the value of
 lower_case_table_names.  If that is 0 or 1, referenced_table_name_lookup
 will point to referenced_table_name.  If 2, then another string is
 allocated from the heap and set to lower case. */
void dict_mem_referenced_table_name_lookup_set(
    dict_foreign_t *foreign, /*!< in/out: foreign struct */
    bool do_alloc);          /*!< in: is an alloc needed */

/** Fills the dependent virtual columns in a set.
Reason for being dependent are
1) FK can be present on base column of virtual columns
2) FK can be present on column which is a part of virtual index
@param[in,out]  foreign foreign key information. */
void dict_mem_foreign_fill_vcol_set(dict_foreign_t *foreign);

/** Fill virtual columns set in each fk constraint present in the table.
@param[in,out]  table   innodb table object. */
void dict_mem_table_fill_foreign_vcol_set(dict_table_t *table);

/** Free the vcol_set from all foreign key constraint on the table.
@param[in,out]  table   innodb table object. */
void dict_mem_table_free_foreign_vcol_set(dict_table_t *table);

/** Create a temporary tablename like "#sql-ibtid-inc" where
  tid = the Table ID
  inc = a randomly initialized number that is incremented for each file
The table ID is a 64 bit integer, can use up to 20 digits, and is
initialized at bootstrap. The second number is 32 bits, can use up to 10
digits, and is initialized at startup to a randomly distributed number.
It is hoped that the combination of these two numbers will provide a
reasonably unique temporary file name.
@param[in]      heap    A memory heap
@param[in]      dbtab   Table name in the form database/table name
@param[in]      id      Table id
@return A unique temporary tablename suitable for InnoDB use */
char *dict_mem_create_temporary_tablename(mem_heap_t *heap, const char *dbtab,
                                          table_id_t id);

static inline bool is_valid_row_version(const row_version_t version) {
  /* NOTE : 0 is also a valid row versions for rows which are inserted after
  upgrading from earlier INSTANT implemenation */
  if (std::cmp_less_equal(version, MAX_ROW_VERSION)) {
    return true;
  }

  return false;
}

/** Initialize dict memory variables */
void dict_mem_init(void);

/** SQL identifier name wrapper for pretty-printing */
class id_name_t {
 public:
  /** Default constructor */
  id_name_t() : m_name() {}
  /** Constructor
  @param[in]    name    identifier to assign */
  explicit id_name_t(const char *name) : m_name(name) {}

  /** Assignment operator
  @param[in]    name    identifier to assign */
  id_name_t &operator=(const char *name) {
    m_name = name;
    return (*this);
  }

  /** Implicit type conversion
  @return the name */
  operator const char *() const { return (m_name); }

  /** Explicit type conversion
  @return the name */
  const char *operator()() const { return (m_name); }

 private:
  /** The name in internal representation */
  const char *m_name;
};

/** Table name wrapper for pretty-printing */
struct table_name_t {
  /** The name in internal representation */
  char *m_name;
};

/** Data structure for default value of a column in a table */
struct dict_col_default_t {
  /** Pointer to the column itself */
  dict_col_t *col;
  /** Default value in bytes */
  byte *value;
  /** Length of default value */
  size_t len;

  bool operator==(const dict_col_default_t &other) const;
  bool operator!=(const dict_col_default_t &other) const;
};

/** Data structure for a column in a table */
struct dict_col_t {
  /*----------------------*/
  /** The following are copied from dtype_t,
  so that all bit-fields can be packed tightly. */
  /** @{ */

  /** Default value when this column was added instantly.
  If this is not a instantly added column then this is nullptr. */
  dict_col_default_t *instant_default{nullptr};

  unsigned prtype : 32; /*!< precise type; MySQL data
                        type, charset code, flags to
                        indicate nullability,
                        signedness, whether this is a
                        binary string, whether this is
                        a true VARCHAR where MySQL
                        uses 2 bytes to store the length */
  unsigned mtype : 8;   /*!< main data type */

  /* the remaining fields do not affect alphabetical ordering: */

  unsigned len : 16; /*!< length; for MySQL data this
                     is field->pack_length(),
                     except that for a >= 5.0.3
                     type true VARCHAR this is the
                     maximum byte length of the
                     string data (in addition to
                     the string, MySQL uses 1 or 2
                     bytes to store the string length) */

  unsigned mbminmaxlen : 5; /*!< minimum and maximum length of a
                            character, in bytes;
                            DATA_MBMINMAXLEN(mbminlen,mbmaxlen);
                            mbminlen=DATA_MBMINLEN(mbminmaxlen);
                            mbmaxlen=DATA_MBMINLEN(mbminmaxlen) */
  /*----------------------*/
  /* End of definitions copied from dtype_t */
  /** @} */

  unsigned ind : 10;        /*!< table column position
                            (starting from 0) */
  unsigned ord_part : 1;    /*!< nonzero if this column
                            appears in the ordering fields
                            of an index */
  unsigned max_prefix : 12; /*!< maximum index prefix length on
                            this column. Our current max limit is
                            3072 (REC_VERSION_56_MAX_INDEX_COL_LEN)
                            bytes. */

  /* True, if the column is visible */
  bool is_visible;

 private:
  /* Position of column on physical row.
  If column prefix is part of PK, it appears twice on row. First 2 bytes are
  for prefix position and next 2 bytes are for column position on row. */
  uint32_t phy_pos{UINT32_UNDEFINED};

  /* Row version in which this column was added INSTANTly to the table */
  row_version_t version_added{INVALID_ROW_VERSION};

  /* Row version in which this column was dropped INSTANTly from the table */
  row_version_t version_dropped{INVALID_ROW_VERSION};

 public:
  /* If column prefix is there on row. */
  bool has_prefix_phy_pos() const { return (phy_pos & 0x8000); }

  /* Get the physical position of column prefix on row. */
  uint16_t get_prefix_phy_pos() const {
    ut_ad(has_prefix_phy_pos());
    return ((uint16_t)(phy_pos >> 16));
  }

  /* Set the physical position of column prefix on row. */
  void set_prefix_phy_pos(uint16_t prefix_pos) {
    phy_pos = prefix_pos;
    phy_pos = phy_pos << 16;
    phy_pos |= 0x8000;
  }

  /* Get the physical position of column on row. */
  uint16_t get_col_phy_pos() const { return ((phy_pos & ~0x8000) & 0xFFFF); }

  /* Set the physical position of column on row. */
  void set_col_phy_pos(uint16_t pos) {
    ut_ad(has_prefix_phy_pos());
    phy_pos |= pos;
  }

  /* Set the physical position metadata of column. */
  uint32_t get_phy_pos() const { return phy_pos; }

  /* Get the physical position metadata of column. */
  void set_phy_pos(uint32_t pos) { phy_pos = pos; }

  bool is_instant_added() const {
    if (version_added != INVALID_ROW_VERSION && version_added > 0) {
      return true;
    }
    return false;
  }

  row_version_t get_version_added() const {
    ut_ad(is_instant_added());
    return version_added;
  }

  void set_version_added(row_version_t version) {
    ut_ad(version == INVALID_ROW_VERSION || is_valid_row_version(version));
    version_added = version;
  }

  bool is_version_added_match(const dict_col_t *col) const {
    if (is_instant_added() != col->is_instant_added()) {
      return false;
    }

    if (is_instant_added()) {
      return (get_version_added() == col->get_version_added());
    }

    return true;
  }

  bool is_instant_dropped() const {
    if (version_dropped != INVALID_ROW_VERSION && version_dropped > 0) {
      return true;
    }
    return false;
  }

  row_version_t get_version_dropped() const {
    ut_ad(is_instant_dropped());
    return version_dropped;
  }

  void set_version_dropped(row_version_t version) {
    ut_ad(version == INVALID_ROW_VERSION || is_valid_row_version(version));
    version_dropped = version;
  }

  bool is_version_dropped_match(const dict_col_t *col) const {
    if (is_instant_dropped() != col->is_instant_dropped()) {
      return false;
    }

    if (is_instant_dropped()) {
      return (get_version_dropped() == col->get_version_dropped());
    }

    return true;
  }

  /** Returns the minimum size of the column.
  @return minimum size */
  ulint get_min_size() const {
    return (dtype_get_min_size_low(mtype, prtype, len, mbminmaxlen));
  }

  /** Returns the maximum size of the column.
  @return maximum size */
  ulint get_max_size() const { return (dtype_get_max_size_low(mtype, len)); }

  /** Check if a column is a virtual column
  @return true if it is a virtual column, false otherwise */
  bool is_virtual() const { return (prtype & DATA_VIRTUAL); }

  /** Check if a column is a multi-value virtual column
  @return true if it is a multi-value virtual column, false otherwise */
  bool is_multi_value() const { return ((prtype & DATA_MULTI_VALUE) != 0); }

  /** Check if a column is nullable
  @return true if it is nullable, otherwise false */
  bool is_nullable() const { return ((prtype & DATA_NOT_NULL) == 0); }

  /** Gets the column data type.
  @param[out] type      data type */
  void copy_type(dtype_t *type) const {
    ut_ad(type != nullptr);

    type->mtype = mtype;
    type->prtype = prtype;
    type->len = len;
    type->mbminmaxlen = mbminmaxlen;
  }

  /** Gets the minimum number of bytes per character.
  @return minimum multi-byte char size, in bytes */
  ulint get_mbminlen() const { return (DATA_MBMINLEN(mbminmaxlen)); }

  /** Gets the maximum number of bytes per character.
  @return maximum multi-byte char size, in bytes */
  ulint get_mbmaxlen() const { return (DATA_MBMAXLEN(mbminmaxlen)); }

  /** Sets the minimum and maximum number of bytes per character.
  @param[in] mbminlen   minimum multi byte character size, in bytes
  @param[in] mbmaxlen   mAXimum multi-byte character size, in bytes */
  void set_mbminmaxlen(ulint mbminlen, ulint mbmaxlen) {
    ut_ad(mbminlen < DATA_MBMAX);
    ut_ad(mbmaxlen < DATA_MBMAX);
    ut_ad(mbminlen <= mbmaxlen);

    mbminmaxlen = DATA_MBMINMAXLEN(mbminlen, mbmaxlen);
  }

  /** Returns the size of a fixed size column, 0 if not a fixed size column.
  @param[in] comp               nonzero=ROW_FORMAT=COMPACT
  @return fixed size, or 0 */
  ulint get_fixed_size(ulint comp) const {
    return (dtype_get_fixed_size_low(mtype, prtype, len, mbminmaxlen, comp));
  }

  /** Returns the ROW_FORMAT=REDUNDANT stored SQL NULL size of a column.
  For fixed length types it is the fixed length of the type, otherwise 0.
  @param[in] comp               nonzero=ROW_FORMAT=COMPACT
  @return SQL null storage size in ROW_FORMAT=REDUNDANT */
  ulint get_null_size(ulint comp) const { return (get_fixed_size(comp)); }

  /** Check whether the col is used in spatial index or regular index.
  @return spatial status */
  spatial_status_t get_spatial_status() const {
    spatial_status_t spatial_status = SPATIAL_NONE;

    /* Column is not a part of any index. */
    if (!ord_part) {
      return (spatial_status);
    }

    if (DATA_GEOMETRY_MTYPE(mtype)) {
      if (max_prefix == 0) {
        spatial_status = SPATIAL_ONLY;
      } else {
        /* Any regular index on a geometry column
        should have a prefix. */
        spatial_status = SPATIAL_MIXED;
      }
    }

    return (spatial_status);
  }

  /** Set default value
  @param[in]    value   Default value
  @param[in]    length  Default value length
  @param[in,out]        heap    Heap to allocate memory */
  void set_default(const byte *value, size_t length, mem_heap_t *heap);

  /** Check if column is dropped before the given version.
  @param[in]    version row version
  @return true if the column is dropped before or in the version. */
  bool is_dropped_in_or_before(row_version_t version) const {
    ut_ad(is_valid_row_version(version));

    if (!is_instant_dropped()) {
      return false;
    }

    return (get_version_dropped() <= version);
  }

  /** Check if column is added after the current version.
  @param[in]    version row version
  @return true if column is added after the current row version. */
  bool is_added_after(row_version_t version) const {
    ut_ad(is_valid_row_version(version));

    if (!is_instant_added()) {
      return false;
    }

    return (get_version_added() > version);
  }

  /** Check if a column is visible in given version.
  @param[in]      version         row version
  return true if column is visible in version. */
  bool is_visible_in_version(row_version_t version) const {
    ut_ad(is_valid_row_version(version));
    return (!is_added_after(version) && !is_dropped_in_or_before(version));
  }

#ifdef UNIV_DEBUG
  /** Assert that a column and a data type match.
  param[in] type                data type
  @return true */
  bool assert_equal(const dtype_t *type) const {
    ut_ad(type);

    ut_ad(mtype == type->mtype);
    ut_ad(prtype == type->prtype);
    // ut_ad(col->len == type->len);
#ifndef UNIV_HOTBACKUP
    ut_ad(mbminmaxlen == type->mbminmaxlen);
#endif /* !UNIV_HOTBACKUP */

    return true;
  }

  /** Check if a column name resembles format for dropped column.
  param[in] type                column name
  @return true if column name resembles dropped column. */
  static bool is_instant_dropped_name(const std::string col_name) {
    if (col_name.find(INSTANT_DROP_SUFFIX_8_0_29) != std::string::npos ||
        col_name.find(INSTANT_DROP_PREFIX_8_0_32) != std::string::npos)
      return true;
    return false;
  }
#endif /* UNIV_DEBUG */
};

/** Index information put in a list of virtual column structure. Index
id and virtual column position in the index will be logged.
There can be multiple entries for a given index, with a different position. */
struct dict_v_idx_t {
  /** active index on the column */
  dict_index_t *index;

  /** position in this index */
  ulint nth_field;
};

/** Index list to put in dict_v_col_t */
typedef std::list<dict_v_idx_t, ut::allocator<dict_v_idx_t>> dict_v_idx_list;

/** Data structure for a virtual column in a table */
struct dict_v_col_t {
  /** column structure */
  dict_col_t m_col;

  /** array of base column ptr */
  dict_col_t **base_col;

  /** number of base columns */
  ulint num_base;

  /** column pos in table */
  ulint v_pos;

  /** Virtual index list, and column position in the index,
  the allocated memory is not from table->heap, nor it is
  tracked by dict_sys->size */
  dict_v_idx_list *v_indexes;
};

/** Data structure for newly added virtual column in a table */
struct dict_add_v_col_t {
  /** number of new virtual column */
  ulint n_v_col;

  /** column structures */
  const dict_v_col_t *v_col;

  /** new col names */
  const char **v_col_name;
};

/** Data structure for a stored column in a table. */
struct dict_s_col_t {
  /** Stored column ptr */
  dict_col_t *m_col;
  /** array of base col ptr */
  dict_col_t **base_col;
  /** number of base columns */
  ulint num_base;
  /** column pos in table */
  ulint s_pos;
};

/** list to put stored column for dict_table_t */
typedef std::list<dict_s_col_t, ut::allocator<dict_s_col_t>> dict_s_col_list;

/** @brief DICT_ANTELOPE_MAX_INDEX_COL_LEN is measured in bytes and
is the maximum indexed column length (or indexed prefix length) in
ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT. Also, in any format,
any fixed-length field that is longer than this will be encoded as
a variable-length field.

It is set to 3*256, so that one can create a column prefix index on
256 characters of a TEXT or VARCHAR column also in the UTF-8
charset. In that charset, a character may take at most 3 bytes.  This
constant MUST NOT BE CHANGED, or the compatibility of InnoDB data
files would be at risk! */
constexpr uint32_t DICT_ANTELOPE_MAX_INDEX_COL_LEN =
    REC_ANTELOPE_MAX_INDEX_COL_LEN;

/** Find out maximum indexed column length by its table format.
For ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT, the maximum
field length is REC_ANTELOPE_MAX_INDEX_COL_LEN - 1 (767). For
ROW_FORMAT=COMPRESSED and ROW_FORMAT=DYNAMIC, the length could
be REC_VERSION_56_MAX_INDEX_COL_LEN (3072) bytes */
#define DICT_MAX_FIELD_LEN_BY_FORMAT(table)                              \
  (dict_table_has_atomic_blobs(table) ? REC_VERSION_56_MAX_INDEX_COL_LEN \
                                      : REC_ANTELOPE_MAX_INDEX_COL_LEN - 1)

static inline uint32_t DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(uint32_t flags) {
  if (DICT_TF_HAS_ATOMIC_BLOBS(flags)) {
    return REC_VERSION_56_MAX_INDEX_COL_LEN;
  }
  return REC_ANTELOPE_MAX_INDEX_COL_LEN - 1;
}

/** Defines the maximum fixed length column size */
constexpr uint32_t DICT_MAX_FIXED_COL_LEN = DICT_ANTELOPE_MAX_INDEX_COL_LEN;

/** Data structure for a field in an index */
struct dict_field_t {
  dict_field_t() : col(nullptr), prefix_len(0), fixed_len(0), is_ascending(0) {}

  dict_col_t *col;           /*!< pointer to the table column */
  id_name_t name;            /*!< name of the column */
  unsigned prefix_len : 12;  /*!< 0 or the length of the column
                             prefix in bytes in a MySQL index of
                             type, e.g., INDEX (textcol(25));
                             must be smaller than
                             DICT_MAX_FIELD_LEN_BY_FORMAT;
                             NOTE that in the UTF-8 charset, MySQL
                             sets this to (mbmaxlen * the prefix len)
                             in UTF-8 chars */
  unsigned fixed_len : 10;   /*!< 0 or the fixed length of the
                             column if smaller than
                             DICT_ANTELOPE_MAX_INDEX_COL_LEN */
  unsigned is_ascending : 1; /*!< 0=DESC, 1=ASC */

  uint16_t get_phy_pos() const {
    if (prefix_len != 0) {
      return col->get_prefix_phy_pos();
    }

    return col->get_col_phy_pos();
  }
};

/** PADDING HEURISTIC BASED ON LINEAR INCREASE OF PADDING TO AVOID
 COMPRESSION FAILURES
 (Note: this is relevant only for compressed indexes)
 GOAL: Avoid compression failures by maintaining information about the
 compressibility of data. If data is not very compressible then leave
 some extra space 'padding' in the uncompressed page making it more
 likely that compression of less than fully packed uncompressed page will
 succeed.

 This padding heuristic works by increasing the pad linearly until the
 desired failure rate is reached. A "round" is a fixed number of
 compression operations.
 After each round, the compression failure rate for that round is
 computed. If the failure rate is too high, then padding is incremented
 by a fixed value, otherwise it's left intact.
 If the compression failure is lower than the desired rate for a fixed
 number of consecutive rounds, then the padding is decreased by a fixed
 value. This is done to prevent overshooting the padding value,
 and to accommodate the possible change in data compressibility. */

/** Number of zip ops in one round. */
constexpr uint32_t ZIP_PAD_ROUND_LEN = 128;

/** Number of successful rounds after which the padding is decreased */
constexpr uint32_t ZIP_PAD_SUCCESSFUL_ROUND_LIMIT = 5;

/** Amount by which padding is increased. */
constexpr uint32_t ZIP_PAD_INCR = 128;

/** Percentage of compression failures that are allowed in a single
round */
extern ulong zip_failure_threshold_pct;

/** Maximum percentage of a page that can be allowed as a pad to avoid
compression failures */
extern ulong zip_pad_max;

/** Data structure to hold information about about how much space in
an uncompressed page should be left as padding to avoid compression
failures. This estimate is based on a self-adapting heuristic. */
struct zip_pad_info_t {
  SysMutex *mutex;        /*!< mutex protecting the info */
  std::atomic<ulint> pad; /*!< number of bytes used as pad */
  ulint success;          /*!< successful compression ops during
                          current round */
  ulint failure;          /*!< failed compression ops during
                          current round */
  ulint n_rounds;         /*!< number of currently successful
                         rounds */
#ifndef UNIV_HOTBACKUP
  std::atomic<os_once::state_t> mutex_created;
  /*!< Creation state of mutex member */
#endif /* !UNIV_HOTBACKUP */
};

/** If key is fixed length key then cache the record offsets on first
computation. This will help save computation cycle that generate same
redundant data. */
struct rec_cache_t {
  /** Holds reference to cached offsets for record. */
  const ulint *offsets{nullptr};

  /** Number of NULLable columns among those for which offsets are cached */
  size_t nullable_cols{0};
};

/** Cache position of last inserted or selected record by caching record
and holding reference to the block where record resides.
Note: We don't commit mtr and hold it beyond a transaction lifetime as this is
a special case (intrinsic table) that are not shared across connection. */
class last_ops_cur_t {
 public:
  /** Constructor */
  last_ops_cur_t() : rec(), block(), mtr(), disable_caching(), invalid() {
    /* Do Nothing. */
  }

  /* Commit mtr and re-initialize cache record and block to NULL. */
  void release() {
    if (mtr.is_active()) {
      mtr_commit(&mtr);
    }
    rec = nullptr;
    block = nullptr;
    invalid = false;
  }

 public:
  /** last inserted/selected record. */
  rec_t *rec;

  /** block where record reside. */
  buf_block_t *block;

  /** active mtr that will be re-used for next insert/select. */
  mtr_t mtr;

  /** disable caching. (disabled when table involves blob/text.) */
  bool disable_caching;

  /** If index structure is undergoing structural change viz.
  split then invalidate the cached position as it would be no more
  remain valid. Will be re-cached on post-split insert. */
  bool invalid;
};

/** "GEN_CLUST_INDEX" is the name reserved for InnoDB default
system clustered index when there is no primary key. */
const char innobase_index_reserve_name[] = "GEN_CLUST_INDEX";

namespace dd {
class Spatial_reference_system;
}

#ifdef UNIV_DEBUG
/** Value of dict_index_t::magic_n */
constexpr uint32_t DICT_INDEX_MAGIC_N = 76789786;
#endif

constexpr uint32_t DICT_INDEX_MERGE_THRESHOLD_DEFAULT = 50;
constexpr uint32_t MAX_KEY_LENGTH_BITS = 12;

/** Data structure for an index.  Most fields will be
initialized to 0, NULL or false in dict_mem_index_create(). */
struct dict_index_t {
  /** id of the index */
  space_index_t id;

  /** memory heap */
  mem_heap_t *heap;

  /** index name */
  id_name_t name;

  /** table name */
  const char *table_name;

  /** back pointer to table */
  dict_table_t *table;

  /** space where the index tree is placed */
  unsigned space : 32;

  /** index tree root page number */
  unsigned page : 32;

  /** In the pessimistic delete, if the page data size drops below this limit
  in percent, merging it to a neighbor is tried */
  unsigned merge_threshold : 6;

  /** index type (DICT_CLUSTERED, DICT_UNIQUE, DICT_IBUF, DICT_CORRUPT) */
  unsigned type : DICT_IT_BITS;

  /** position of the trx id column in a clustered index record, if the fields
  before it are known to be of a fixed size, 0 otherwise */
  unsigned trx_id_offset : MAX_KEY_LENGTH_BITS;

  static_assert(1 << MAX_KEY_LENGTH_BITS >= MAX_KEY_LENGTH,
                "1<<MAX_KEY_LENGTH_BITS) < MAX_KEY_LENGTH");

  /** number of columns the user defined to be in the index: in the internal
  representation we add more columns */
  unsigned n_user_defined_cols : 10;

  /** if true, allow duplicate values even if index is created with unique
  constraint */
  unsigned allow_duplicates : 1;

  /** if true, SQL NULL == SQL NULL */
  unsigned nulls_equal : 1;

  /** if true, then disable AHI. Currently limited to intrinsic temporary table
  and SDI table as index id is not unique for such table which is one of the
  validation criterion for  ahi. */
  unsigned disable_ahi : 1;

  /** number of fields from the beginning which are enough to determine an index
  entry uniquely */
  unsigned n_uniq : 10;

  /** number of fields defined so far */
  unsigned n_def : 10;

  /** number of fields in the index */
  unsigned n_fields : 10;

  /** number of total fields in the index (including INSTANT dropped fields) */
  unsigned n_total_fields : 10;

  /** number of nullable fields */
  unsigned n_nullable : 10;

  /** number of nullable fields before first instant ADD COLUMN applied to this
  table. This is valid only when has_instant_cols() is true */
  unsigned n_instant_nullable : 10;

  /** true if the index object is in the dictionary cache */
  unsigned cached : 1;

  /** true if the index is to be dropped; protected by dict_operation_lock */
  unsigned to_be_dropped : 1;

  /** enum online_index_status. Transitions from ONLINE_INDEX_COMPLETE (to
  ONLINE_INDEX_CREATION) are protected by dict_operation_lock and
  dict_sys->mutex. Other changes are protected by index->lock. */
  unsigned online_status : 2;

  /** a flag that is set for secondary indexes that have not been committed to
  the data dictionary yet */
  unsigned uncommitted : 1;

  /** true if the index is clustered index and it has some instant columns */
  unsigned instant_cols : 1;

  /** true if the index is clustered index and table has row versions */
  unsigned row_versions : 1;

  /** spatial reference id */
  uint32_t srid;

  /** says whether SRID is valid - it cane be undefined */
  bool srid_is_valid;

  /** Cached spatial reference system dictionary entry used by R-tree indexes.
   */
  std::unique_ptr<dd::Spatial_reference_system> rtr_srs;

#ifdef UNIV_DEBUG
  uint32_t magic_n; /*!< magic number */
#endif

  /** array of field descriptions */
  dict_field_t *fields;

  /** Array of field pos sorted as per their physical pos in record. Only
  needed for clustered index having INSTANT ADD/DROP columns. */
  std::vector<uint16_t> fields_array;

  /** Number of nullable columns in each version. Only needed for clustered
  index having INSTANT ADD/DROP columns. */
  uint32_t nullables[MAX_ROW_VERSION + 1] = {0};

#ifndef UNIV_HOTBACKUP
  /** fulltext parser plugin */
  st_mysql_ftparser *parser;

  /** true if it's ngram parser */
  bool is_ngram;

  /** whether it has a newly added virtual column in ALTER */
  bool has_new_v_col;

  /** if the index is an hidden index */
  bool hidden;
#endif /* !UNIV_HOTBACKUP */

  /** list of indexes of the table */
  UT_LIST_NODE_T(dict_index_t) indexes;

  /** info used in optimistic searches */
  btr_search_t *search_info;

#ifndef UNIV_HOTBACKUP
  /** the log of modifications during online index creation;
  valid when online_status is ONLINE_INDEX_CREATION */
  row_log_t *online_log;

  /*----------------------*/
  /** Statistics for query optimization */
  /** @{ */
  /** approximate number of different key values for this index, for each
  n-column prefix where 1 <= n <= dict_get_n_unique(index) (the array is
  indexed from 0 to n_uniq-1); we periodically calculate new estimates */
  uint64_t *stat_n_diff_key_vals;

  /** number of pages that were sampled  to calculate each of
  stat_n_diff_key_vals[], e.g. stat_n_sample_sizes[3] pages were sampled to get
  the number stat_n_diff_key_vals[3]. */
  uint64_t *stat_n_sample_sizes;

  /* approximate number of non-null key values for this index, for each column
  where 1 <= n <= dict_get_n_unique(index) (the array is indexed from 0 to
  n_uniq-1); This is used when innodb_stats_method is "nulls_ignored". */
  uint64_t *stat_n_non_null_key_vals;

  /** approximate index size in database pages */
  ulint stat_index_size;
#endif /* !UNIV_HOTBACKUP */
  /** approximate number of leaf pages in the index tree */
  ulint stat_n_leaf_pages;
  /** @} */

  /** cache the last insert position. Currently limited to auto-generated
  clustered index on intrinsic table only. */
  last_ops_cur_t *last_ins_cur;

  /** cache the last selected position. Currently limited to intrinsic table
  only. */
  last_ops_cur_t *last_sel_cur;

  /** cache the field that needs to be re-computed on each insert. Limited to
  intrinsic table as this is common share and can't be used without protection
  if table is accessible to multiple-threads. */
  rec_cache_t rec_cache;

  /** Node sequence number for RTree */
  rtr_ssn_t rtr_ssn;

  /** tracking all R-Tree search cursors */
  rtr_info_track_t *rtr_track;

  /** id of the transaction that created this index, or 0 if the index existed
  when InnoDB was started up */
  trx_id_t trx_id;

  /** Information about state of compression failures and successes */
  zip_pad_info_t zip_pad;

  /** read-write lock protecting the upper levels of the index tree */
  rw_lock_t lock;

  /** Flag whether need to fill dd tables when it's a fulltext index. */
  bool fill_dd;

  /** Set instant nullable
  @param[in]  n  nullable fields before first INSTANT ADD */
  void set_instant_nullable(uint16_t n) { n_instant_nullable = n; }

  /** Get instant nullable.
  @return number of nullable fields before first INSTANT ADD */
  uint16_t get_instant_nullable() const { return n_instant_nullable; }

  /** Get the nullable fields before any INSTANT ADD/DROP
  @return number of nullable fields */
  uint16_t get_nullable_before_instant_add_drop() const {
    if (has_instant_cols()) {
      return get_instant_nullable();
    }

    if (has_row_versions()) {
      return get_nullable_in_version(0);
    }

    return n_nullable;
  }

  /** Determine if the index has been committed to the
  data dictionary.
  @return whether the index definition has been committed */
  bool is_committed() const {
    ut_ad(!uncommitted || !(type & DICT_CLUSTERED));
    return (UNIV_LIKELY(!uncommitted));
  }

  /** Flag an index committed or uncommitted.
  @param[in]    committed       whether the index is committed */
  void set_committed(bool committed) {
    ut_ad(!to_be_dropped);
    ut_ad(committed || !(type & DICT_CLUSTERED));
    uncommitted = !committed;
  }

  /** Get the next index.
  @return       next index
  @retval       NULL    if this was the last index */
  const dict_index_t *next() const {
    const dict_index_t *next = UT_LIST_GET_NEXT(indexes, this);
    ut_ad(magic_n == DICT_INDEX_MAGIC_N);
    return (next);
  }
  /** Get the next index.
  @return       next index
  @retval       NULL    if this was the last index */
  dict_index_t *next() {
    return (const_cast<dict_index_t *>(
        const_cast<const dict_index_t *>(this)->next()));
  }

  /** Check whether the index is corrupted.
  @return true if index is corrupted, otherwise false */
  bool is_corrupted() const {
    ut_ad(magic_n == DICT_INDEX_MAGIC_N);

    return (type & DICT_CORRUPT);
  }

  /* Check whether the index is the clustered index
  @return nonzero for clustered index, zero for other indexes */

  bool is_clustered() const {
    ut_ad(magic_n == DICT_INDEX_MAGIC_N);

    return (type & DICT_CLUSTERED);
  }

  /** Check whether the index is the multi-value index
  @return nonzero for multi-value index, zero for other indexes */
  bool is_multi_value() const {
    ut_ad(magic_n == DICT_INDEX_MAGIC_N);

    return (type & DICT_MULTI_VALUE);
  }

  /** Returns the minimum data size of an index record.
  @return minimum data size in bytes */
  ulint get_min_size() const {
    ulint size = 0;

    for (unsigned i = 0; i < n_fields; i++) {
      size += get_col(i)->get_min_size();
    }

    return (size);
  }

  /** Check whether index can be used by transaction
  @param[in] trx                transaction*/
  bool is_usable(const trx_t *trx) const;

  /** Check whether index has any instantly added columns.
  Possible only if table has INSTANT ADD columns and is upgraded.
  @return true if this is instant affected, otherwise false */
  bool has_instant_cols() const { return (instant_cols); }

  /** Check whether index belongs to a table having row versions
  @return true if table has row versions, otherwise false */
  bool has_row_versions() const { return (row_versions); }

  /** check if either instant or versioned.
  @return true if table has row versions or instant cols, otherwise false */
  bool has_instant_cols_or_row_versions() const {
    if (!is_clustered()) {
      ut_ad(!has_row_versions() && !has_instant_cols());
      return false;
    }

    return (has_row_versions() || has_instant_cols());
  }

  /** Check if tuple is having instant format.
  @param[in]    n_fields_in_tuple       number of fields in tuple
  @return true if yes, false otherwise. */
  bool is_tuple_instant_format(const uint16_t n_fields_in_tuple) const;

  /** Returns the number of nullable fields before specified nth field
  @param[in]    nth     nth field to check */
  uint32_t get_n_nullable_before(uint32_t nth) const {
    uint32_t nullable = 0;
    ut_ad(nth <= n_total_fields);

    for (size_t i = 0; i < nth; ++i) {
      dict_col_t *col = get_field(i)->col;

      ut_ad(!col->is_instant_dropped());

      if (col->is_nullable()) {
        nullable++;
      }
    }

    return (nullable);
  }

  /** Returns total fields including INSTANT DROP fields. */
  uint32_t get_n_total_fields() const {
    ut_ad(is_clustered());
    return n_total_fields;
  }

  /** Returns the number of fields before first instant ADD COLUMN. This is
  needed only for V1 INSTANT ADD. */
  uint32_t get_instant_fields() const;

  size_t calculate_n_instant_nullable(size_t _n_fields) const {
    if (!has_row_versions()) {
      ut_ad(has_instant_cols());
      return get_n_nullable_before(_n_fields);
    }

    size_t n_drop_nullable_cols = 0;
    size_t new_n_nullable = 0;
    for (size_t i = 0; i < n_def; i++) {
      const dict_field_t *field = &fields[i];
      const dict_col_t *col = field->col;

      if (col->is_instant_added()) {
        continue;
      }

      if (col->is_instant_dropped()) {
        if (col->get_col_phy_pos() < _n_fields && col->is_nullable()) {
          n_drop_nullable_cols++;
        }
        continue;
      }

      /* This is regular column */
      if (col->get_col_phy_pos() < _n_fields) {
        if (col->is_nullable()) {
          new_n_nullable++;
        }
      }
    }

    new_n_nullable += n_drop_nullable_cols;

    return new_n_nullable;
  }

  /** Create nullables array.
  @param[in]    current_row_version     current row version of table */
  void create_nullables(uint32_t current_row_version);

  /** Return nullable in a specific row version */
  uint32_t get_nullable_in_version(row_version_t version) const {
    ut_ad(is_valid_row_version(version));

    return nullables[version];
  }

  /** Create fields array sorted by phy_pos of field in row */
  void create_fields_array() {
    fields_array.resize(n_def);
    for (uint32_t i = 0; i < n_def; i++) {
      dict_field_t *field = get_field(i);
      ut_ad(field != nullptr && field->col != nullptr);

      size_t pos = field->get_phy_pos();

      fields_array[pos] = i;
    }
  }

  void destroy_fields_array() {
    /* The dict_index_t destructor is never called. The object is "destructed"
    manually in dict_mem_index_free() and then the memory is just freed. This
    method is called from the mentioned dict_mem_index_free(). Please note that
    this vector is never constructed either - we just zero the memory and start
    using it after calling a "constructor" dict_mem_fill_index_struct(). */
    fields_array.~vector<uint16_t>();
  }

  /** Adds a field definition to an index. NOTE: does not take a copy
  of the column name if the field is a column. The memory occupied
  by the column name may be released only after publishing the index.
  @param[in] name_arg   column name
  @param[in] prefix_len 0 or the column prefix length in a MySQL index
                          like INDEX (textcol(25))
  @param[in] is_ascending       true=ASC, false=DESC */
  void add_field(const char *name_arg, ulint prefix_len, bool is_ascending) {
    dict_field_t *field;

    ut_ad(magic_n == DICT_INDEX_MAGIC_N);

    n_def++;

    field = get_field(n_def - 1);

    field->name = name_arg;
    field->prefix_len = (unsigned int)prefix_len;
    field->is_ascending = is_ascending;
  }

  /** Gets the nth physical pos field.
  @param[in]  pos  physical position of the field
  @return pointer to the field object. */
  dict_field_t *get_physical_field(size_t pos) const {
    ut_ad(pos < n_def);
    ut_ad(magic_n == DICT_INDEX_MAGIC_N);

    if (has_row_versions()) {
      return get_field(fields_array[pos]);
    }

    return get_field(pos);
  }

  /** Gets the nth field of an index.
  @param[in] pos        position of field
  @return pointer to field object */
  dict_field_t *get_field(ulint pos) const {
    ut_ad(pos < n_def);
    ut_ad(magic_n == DICT_INDEX_MAGIC_N);

    return (fields + pos);
  }

  /** Given the physical position, find the logical position of field.
  @param[in]	phy_pos	physical position of field
  @return logical position of field */
  uint16_t get_logical_pos(uint16_t phy_pos) const {
    for (size_t i = 0; i < n_def; i++) {
      if (get_field(i)->get_phy_pos() == phy_pos) {
        return i;
      }
    }
    ut_ad(false);
    return UINT16_UNDEFINED;
  }

  /** Get the physical position of a field on a row. For table having INSTANT
  column, it might differ from field index (pos).
  @param[in]    pos     field index
  @return physical position on row */
  uint16_t get_field_off_pos(ulint pos) const {
    return get_field(pos)->get_phy_pos();
  }

  uint16_t get_field_phy_pos(ulint pos, row_version_t version) const {
    uint16_t phy_pos = get_field(pos)->get_phy_pos();
    if (version == INVALID_ROW_VERSION) {
      return phy_pos;
    }

    uint16_t res = phy_pos;
    for (size_t i = 0; i < phy_pos; i++) {
      if (get_field(fields_array[i])->col->is_dropped_in_or_before(version)) {
        res--;
      }
    }

    return res;
  }

  /** Gets pointer to the nth column in an index.
  @param[in] pos        position of the field
  @return column */
  const dict_col_t *get_col(ulint pos) const { return (get_field(pos)->col); }

  /** Gets the column number the nth field in an index.
  @param[in] pos        position of the field
  @return column number */
  ulint get_col_no(ulint pos) const;

  /** Returns the position of a system column in an index.
  @param[in] type               DATA_ROW_ID, ...
  @return position, ULINT_UNDEFINED if not contained */
  ulint get_sys_col_pos(ulint type) const;

  /** Looks for column n in an index.
  @param[in]    n               column number
  @param[in]    inc_prefix      true=consider column prefixes too
  @param[in]    is_virtual      true==virtual column
  @return position in internal representation of the index;
  ULINT_UNDEFINED if not contained */
  ulint get_col_pos(ulint n, bool inc_prefix = false,
                    bool is_virtual = false) const;

  /** Get the default value of nth field and its length if exists.
  If not exists, both the return value is nullptr and length is 0.
  @param[in]    nth     nth field to get
  @param[in,out]        length  length of the default value
  @return       the default value data of nth field */
  const byte *get_nth_default(ulint nth, ulint *length) const {
    ut_ad(nth < get_n_total_fields());

    const dict_col_t *col = get_physical_field(nth)->col;
    if (col->instant_default == nullptr) {
      *length = 0;
      return (nullptr);
    }

    *length = col->instant_default->len;
    ut_ad(*length == 0 || *length == UNIV_SQL_NULL ||
          col->instant_default->value != nullptr);
    return (col->instant_default->value);
  }

  /** Sets srid and srid_is_valid values
  @param[in]    srid_value              value of SRID, may be garbage
                                          if srid_is_valid_value = false
  @param[in]    srid_is_valid_value     value of srid_is_valid */
  void fill_srid_value(uint32_t srid_value, bool srid_is_valid_value) {
    srid_is_valid = srid_is_valid_value;
    srid = srid_value;
  }

  /** Check if the underlying table is compressed.
  @return true if compressed, false otherwise. */
  inline bool is_compressed() const;

  /** Check if a multi-value index is built on specified multi-value
  virtual column. Please note that there could be only one multi-value
  virtual column on the multi-value index, but not necessary the first
  field of the index.
  @param[in]    mv_col  multi-value virtual column
  @return non-zero means the column is on the index and this is the
  nth position of the column, zero means it's not on the index */
  uint32_t has_multi_value_col(const dict_v_col_t *mv_col) const {
    ut_ad(is_multi_value());
    for (uint32_t i = 0; i < n_fields; ++i) {
      const dict_col_t *col = get_col(i);
      if (mv_col->m_col.ind == col->ind) {
        return (i + 1);
      }

      /* Only one multi-value field, if not match then no match. */
      if (col->is_multi_value()) {
        break;
      }
    }

    return (0);
  }

 public:
  /** Get the page size of the tablespace to which this index belongs.
  @return the page size. */
  page_size_t get_page_size() const;

  /** Get the space id of the tablespace to which this index belongs.
  @return the space id. */
  space_id_t space_id() const { return space; }
};

/** The status of online index creation */
enum online_index_status {
  /** the index is complete and ready for access */
  ONLINE_INDEX_COMPLETE = 0,
  /** the index is being created, online
  (allowing concurrent modifications) */
  ONLINE_INDEX_CREATION,
  /** secondary index creation was aborted and the index
  should be dropped as soon as index->table->n_ref_count reaches 0,
  or online table rebuild was aborted and the clustered index
  of the original table should soon be restored to
  ONLINE_INDEX_COMPLETE */
  ONLINE_INDEX_ABORTED,
  /** the online index creation was aborted, the index was
  dropped from the data dictionary and the tablespace, and it
  should be dropped from the data dictionary cache as soon as
  index->table->n_ref_count reaches 0. */
  ONLINE_INDEX_ABORTED_DROPPED
};

/** Set to store the virtual columns which are affected by Foreign
key constraint. */
typedef std::set<dict_v_col_t *, std::less<dict_v_col_t *>,
                 ut::allocator<dict_v_col_t *>>
    dict_vcol_set;

/** Data structure for a foreign key constraint; an example:
FOREIGN KEY (A, B) REFERENCES TABLE2 (C, D).  Most fields will be
initialized to 0, NULL or false in dict_mem_foreign_create(). */
struct dict_foreign_t {
  mem_heap_t *heap;         /*!< this object is allocated from
                            this memory heap */
  char *id;                 /*!< id of the constraint as a
                            null-terminated string */
  unsigned n_fields : 10;   /*!< number of indexes' first fields
                            for which the foreign key
                            constraint is defined: we allow the
                            indexes to contain more fields than
                            mentioned in the constraint, as long
                            as the first fields are as mentioned */
  unsigned type : 6;        /*!< 0 or DICT_FOREIGN_ON_DELETE_CASCADE
                            or DICT_FOREIGN_ON_DELETE_SET_NULL */
  char *foreign_table_name; /*!< foreign table name */
  char *foreign_table_name_lookup;
  /*!< foreign table name used for dict lookup */
  dict_table_t *foreign_table;    /*!< table where the foreign key is */
  const char **foreign_col_names; /*!< names of the columns in the
                                foreign key */
  char *referenced_table_name;    /*!< referenced table name */
  char *referenced_table_name_lookup;
  /*!< referenced table name for dict lookup*/
  dict_table_t *referenced_table;    /*!< table where the referenced key
                                    is */
  const char **referenced_col_names; /*!< names of the referenced
                                columns in the referenced table */
  dict_index_t *foreign_index;       /*!< foreign index; we require that
                                     both tables contain explicitly defined
                                     indexes for the constraint: InnoDB
                                     does not generate new indexes
                                     implicitly */
  dict_index_t *referenced_index;    /*!< referenced index */

  dict_vcol_set *v_cols; /*!< set of virtual columns affected
                         by foreign key constraint. */
};

std::ostream &operator<<(std::ostream &out, const dict_foreign_t &foreign);

struct dict_foreign_print {
  dict_foreign_print(std::ostream &out) : m_out(out) {}

  void operator()(const dict_foreign_t *foreign) { m_out << *foreign; }

 private:
  std::ostream &m_out;
};

/** Compare two dict_foreign_t objects using their ids. Used in the ordering
of dict_table_t::foreign_set and dict_table_t::referenced_set.  It returns
true if the first argument is considered to go before the second in the
strict weak ordering it defines, and false otherwise. */
struct dict_foreign_compare {
  bool operator()(const dict_foreign_t *lhs, const dict_foreign_t *rhs) const {
    return (ut_strcmp(lhs->id, rhs->id) < 0);
  }
};

/** A function object to find a foreign key with the given index as the
referenced index. Return the foreign key with matching criteria or NULL */
struct dict_foreign_with_index {
  dict_foreign_with_index(const dict_index_t *index) : m_index(index) {}

  bool operator()(const dict_foreign_t *foreign) const {
    return (foreign->referenced_index == m_index);
  }

  const dict_index_t *m_index;
};

/* A function object to check if the foreign constraint is between different
tables.  Returns true if foreign key constraint is between different tables,
false otherwise. */
struct dict_foreign_different_tables {
  bool operator()(const dict_foreign_t *foreign) const {
    return (foreign->foreign_table != foreign->referenced_table);
  }
};

typedef std::set<dict_foreign_t *, dict_foreign_compare,
                 ut::allocator<dict_foreign_t *>>
    dict_foreign_set;

std::ostream &operator<<(std::ostream &out, const dict_foreign_set &fk_set);

/** Function object to check if a foreign key object is there
in the given foreign key set or not.  It returns true if the
foreign key is not found, false otherwise */
struct dict_foreign_not_exists {
  dict_foreign_not_exists(const dict_foreign_set &obj_) : m_foreigns(obj_) {}

  /* Return true if the given foreign key is not found */
  bool operator()(dict_foreign_t *const &foreign) const {
    return (m_foreigns.find(foreign) == m_foreigns.end());
  }

 private:
  const dict_foreign_set &m_foreigns;
};

/** Validate the search order in the foreign key set.
@param[in]      fk_set  the foreign key set to be validated
@return true if search order is fine in the set, false otherwise. */
bool dict_foreign_set_validate(const dict_foreign_set &fk_set);

/** Validate the search order in the foreign key sets of the table
(foreign_set and referenced_set).
@param[in]      table   table whose foreign key sets are to be validated
@return true if foreign key sets are fine, false otherwise. */
bool dict_foreign_set_validate(const dict_table_t &table);

/** Frees a foreign key struct. */
inline void dict_foreign_free(
    dict_foreign_t *foreign) /*!< in, own: foreign key struct */
{
  if (foreign->v_cols != nullptr) {
    ut::delete_(foreign->v_cols);
  }

  mem_heap_free(foreign->heap);
}

/** The destructor will free all the foreign key constraints in the set
by calling dict_foreign_free() on each of the foreign key constraints.
This is used to free the allocated memory when a local set goes out
of scope. */
struct dict_foreign_set_free {
  dict_foreign_set_free(const dict_foreign_set &foreign_set)
      : m_foreign_set(foreign_set) {}

  ~dict_foreign_set_free() {
    std::for_each(m_foreign_set.begin(), m_foreign_set.end(),
                  dict_foreign_free);
  }

  const dict_foreign_set &m_foreign_set;
};

/** The flags for ON_UPDATE and ON_DELETE can be ORed; the default is that
a foreign key constraint is enforced, therefore RESTRICT just means no flag */
/** @{ */
/** ON DELETE CASCADE */
constexpr uint32_t DICT_FOREIGN_ON_DELETE_CASCADE = 1;
/** ON DELETE SET NULL */
constexpr uint32_t DICT_FOREIGN_ON_DELETE_SET_NULL = 2;
/** ON UPDATE CASCADE */
constexpr uint32_t DICT_FOREIGN_ON_UPDATE_CASCADE = 4;
/** ON UPDATE SET NULL */
constexpr uint32_t DICT_FOREIGN_ON_UPDATE_SET_NULL = 8;
/** ON DELETE NO ACTION */
constexpr uint32_t DICT_FOREIGN_ON_DELETE_NO_ACTION = 16;
/** ON UPDATE NO ACTION */
constexpr uint32_t DICT_FOREIGN_ON_UPDATE_NO_ACTION = 32;
/** @} */

/** Display an identifier.
@param[in,out]  s       output stream
@param[in]      id_name SQL identifier (other than table name)
@return the output stream */
std::ostream &operator<<(std::ostream &s, const id_name_t &id_name);

/** Display a table name.
@param[in,out]  s               output stream
@param[in]      table_name      table name
@return the output stream */
std::ostream &operator<<(std::ostream &s, const table_name_t &table_name);

#ifndef UNIV_HOTBACKUP
/** List of locks that different transactions have acquired on a table. This
list has a list node that is embedded in a nested union/structure. We have to
generate a specific template for it. */
struct TableLockGetNode;
typedef ut_list_base<lock_t, TableLockGetNode> table_lock_list_t;
#endif /* !UNIV_HOTBACKUP */

/** mysql template structure defined in row0mysql.cc */
struct mysql_row_templ_t;

/** Structure defines template related to virtual columns and
their base columns */
struct dict_vcol_templ_t {
  /** number of regular columns */
  ulint n_col;

  /** number of virtual columns */
  ulint n_v_col;

  /** array of templates for virtual col and their base columns */
  mysql_row_templ_t **vtempl;

  /** table's database name */
  std::string db_name;

  /** table name */
  std::string tb_name;

  /** share->table_name */
  std::string share_name;

  /** MySQL record length */
  ulint rec_len;

  /** default column value if any */
  byte *default_rec;
};

/** The dirty status of tables, used to indicate if a table has some
dynamic metadata changed to be written back */
enum table_dirty_status {
  /** Some persistent metadata is now dirty in memory, need to be
  written back to DDTableBuffer table and(or directly to) DD table.
  There could be some exceptions, when it's marked as dirty, but
  the metadata has already been written back to DDTableBuffer.
  For example, if a corrupted index is found and marked as corrupted,
  then it gets dropped. At this time, the dirty_status is still of
  this dirty value. Also a concurrent checkpoint make this bit
  out-of-date for other working threads, which still think the
  status is dirty and write-back is necessary.
  There could be either one row or no row for this table in
  DDTableBuffer table */
  METADATA_DIRTY = 0,
  /** Some persistent metadata is buffered in DDTableBuffer table,
  need to be written back to DD table. There is must be one row in
  DDTableBuffer table for this table */
  METADATA_BUFFERED,
  /** All persistent metadata are up to date. There is no row
  for this table in DDTableBuffer table */
  METADATA_CLEAN
};

#ifndef UNIV_HOTBACKUP
/** A vector to collect prebuilt from different readers working on the same
temp table */
typedef std::vector<row_prebuilt_t *> temp_prebuilt_vec;
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
/** Value of 'magic_n'. */
constexpr uint32_t DICT_TABLE_MAGIC_N = 76333786;
#endif

/** Data structure for a database table.  Most fields will be
initialized to 0, NULL or false in dict_mem_table_create(). */
struct dict_table_t {
  /** Check if the table is compressed.
  @return true if compressed, false otherwise. */
  bool is_compressed() const { return (DICT_TF_GET_ZIP_SSIZE(flags) != 0); }

  /** Check if the table is encrypted.  Only for file per table tablespace.
  @return true if encrypted, false otherwise. */
  bool is_encrypted() const {
    return (flags2 & DICT_TF2_ENCRYPTION_FILE_PER_TABLE);
  }

  /** Get reference count.
  @return current value of n_ref_count */
  inline uint64_t get_ref_count() const;

  /** Acquire the table handle. */
  inline void acquire();

  /** Acquire the table handle, with lock() and unlock() the table.
  This function needs to be called for opening table when the table
  is in memory and later the stats information would be initialized */
  inline void acquire_with_lock();

  /** Release the table handle. */
  inline void release();

  /** Lock the table handle. */
  inline void lock();

  /** Unlock the table handle. */
  inline void unlock();

#ifndef UNIV_HOTBACKUP
  /** Get schema and table name in system character set.
  @param[out]   schema  schema name
  @param[out]   table   table name */
  void get_table_name(std::string &schema, std::string &table) const;

  bool is_system_schema() const {
    std::string schema_name;
    std::string table_name;

    get_table_name(schema_name, table_name);

    if (0 == strcmp(schema_name.c_str(), MYSQL_SCHEMA_NAME.str) ||
        0 == strcmp(schema_name.c_str(), "sys") ||
        0 == strcmp(schema_name.c_str(), PERFORMANCE_SCHEMA_DB_NAME.str) ||
        0 == strcmp(schema_name.c_str(), INFORMATION_SCHEMA_NAME.str)) {
      return (true);
    }
    return (false);
  }

  /** Mutex of the table for concurrency access. */
  ib_mutex_t *mutex;

  /** Creation state of mutex. */
  std::atomic<os_once::state_t> mutex_created;
#endif /* !UNIV_HOTBACKUP */

  /** Id of the table. */
  table_id_t id;

  /** Memory heap. If you allocate from this heap after the table has
  been created then be sure to account the allocation into
  dict_sys->size. When closing the table we do something like
  dict_sys->size -= mem_heap_get_size(table->heap) and if that is going
  to become negative then we would assert. Something like this should do:
  old_size = mem_heap_get_size()
  mem_heap_alloc()
  new_size = mem_heap_get_size()
  dict_sys->size += new_size - old_size. */
  mem_heap_t *heap;

  /** Table name. */
  table_name_t name;

  /** Truncate name. */
  table_name_t trunc_name;

  /** NULL or the directory path specified by DATA DIRECTORY. */
  char *data_dir_path;

  /** NULL or the tablespace name that this table is assigned to,
  specified by the TABLESPACE option.*/
  id_name_t tablespace;

  /** Space where the clustered index of the table is placed. */
  space_id_t space;

  /** dd::Tablespace::id of the table */
  dd::Object_id dd_space_id;

  /** Stores information about:
  1 row format (redundant or compact),
  2 compressed page size (zip shift size),
  3 whether using atomic blobs,
  4 whether the table has been created with the option DATA DIRECTORY.
  Use DICT_TF_GET_COMPACT(), DICT_TF_GET_ZIP_SSIZE(),
  DICT_TF_HAS_ATOMIC_BLOBS() and DICT_TF_HAS_DATA_DIR() to parse this
  flag. */
  unsigned flags : DICT_TF_BITS;

  /** Stores information about:
  1 whether the table has been created using CREATE TEMPORARY TABLE,
  2 whether the table has an internally defined DOC ID column,
  3 whether the table has a FTS index,
  4 whether DOC ID column need to be added to the FTS index,
  5 whether the table is being created its own tablespace,
  6 whether the table has been DISCARDed,
  7 whether the aux FTS tables names are in hex.
  8 whether the table is instinc table.
  9 whether the table has encryption setting.
  Use DICT_TF2_FLAG_IS_SET() to parse this flag. */
  unsigned flags2 : DICT_TF2_BITS;

  /** true if the table is an intermediate table during copy alter
  operation or a partition/subpartition which is required for copying
  data and skip the undo log for insertion of row in the table.
  This variable will be set and unset during extra(), or during the
  process of altering partitions */
  unsigned skip_alter_undo : 1;

  /** true if this is in a single-table tablespace and the .ibd file is
  missing. Then we must return in ha_innodb.cc an error if the user
  tries to query such an orphaned table. */
  unsigned ibd_file_missing : 1;

  /** true if the table object has been added to the dictionary cache. */
  unsigned cached : 1;

  /** true if the table is to be dropped, but not yet actually dropped
  (could in the background drop list). It is turned on at the beginning
  of row_drop_table_for_mysql() and turned off just before we start to
  update system tables for the drop. It is protected by
  dict_operation_lock. */
  unsigned to_be_dropped : 1;

  /** Number of non-virtual columns defined so far. */
  unsigned n_def : 10;

  /** Number of non-virtual columns. */
  unsigned n_cols : 10;

  /** Number of non-virtual columns before first instant ADD COLUMN,
  including the system columns like n_cols. This is used only when table has
  instant ADD clumns in V1. */
  unsigned n_instant_cols : 10;

  /** Number of total columns (include virtual and non-virtual) */
  unsigned n_t_cols : 10;

  /** Number of total columns defined so far. */
  unsigned n_t_def : 10;

  /** Number of virtual columns defined so far. */
  unsigned n_v_def : 10;

  /** Number of virtual columns. */
  unsigned n_v_cols : 10;

  /** Number of multi-value virtual columns. */
  unsigned n_m_v_cols : 10;

  /** true if this table is expected to be kept in memory. This table
  could be a table that has FK relationships or is undergoing DDL */
  bool can_be_evicted : 1;

  /** true if this table is not evictable(can_be_evicted) and this is
  because of DDL operation */
  unsigned ddl_not_evictable : 1;

  /** true if some indexes should be dropped after ONLINE_INDEX_ABORTED
  or ONLINE_INDEX_ABORTED_DROPPED. */
  unsigned drop_aborted : 1;

  /** Array of column descriptions. */
  dict_col_t *cols;

  /** Array of virtual column descriptions. */
  dict_v_col_t *v_cols;

  /** List of stored column descriptions. It is used only for foreign key
  check during create table and copy alter operations.
  During copy alter, s_cols list is filled during create table operation
  and need to preserve till rename table operation. That is the
  reason s_cols is a part of dict_table_t */
  dict_s_col_list *s_cols;

  /** Column names packed in a character string
  "name1\0name2\0...nameN\0". Until the string contains n_cols, it will
  be allocated from a temporary heap. The final string will be allocated
  from table->heap. */
  const char *col_names;

  /** Virtual column names */
  const char *v_col_names;

  /** True if the table belongs to a system database (mysql, information_schema
  or performance_schema) */
  bool is_system_table;

  /** Hash chain node. */
  hash_node_t name_hash;

  /** Hash chain node. */
  hash_node_t id_hash;

  /** The FTS_DOC_ID_INDEX, or NULL if no fulltext indexes exist */
  dict_index_t *fts_doc_id_index;

  /** List of indexes of the table. */
  UT_LIST_BASE_NODE_T(dict_index_t, indexes) indexes;

  size_t get_index_count() const { return UT_LIST_GET_LEN(indexes); }

  /** Node of the LRU list of tables. */
  UT_LIST_NODE_T(dict_table_t) table_LRU;

  /** metadata version number of dd::Table::se_private_data() */
  uint64_t version;

  /** Current row version in case columns are added/dropped INSTANTly */
  uint32_t current_row_version{0};

  /** Initial non-virtual column count */
  uint32_t initial_col_count{0};

  /** Current non-virtual column count */
  uint32_t current_col_count{0};

  /** Total non-virtual column count */
  uint32_t total_col_count{0};

  /** Set if table is upgraded instant table */
  bool m_upgraded_instant{false};

  /** table dynamic metadata status, protected by dict_persist->mutex */
  std::atomic<table_dirty_status> dirty_status;

#ifndef UNIV_HOTBACKUP
  /** Node of the dirty table list of tables, which is protected
  by dict_persist->mutex */
  UT_LIST_NODE_T(dict_table_t) dirty_dict_tables;
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
  /** This field is used to mark if a table is in the
  dirty_dict_tables_list. if the dirty_status is not of
  METADATA_CLEAN, the table should be in the list, otherwise not.
  This field should be protected by dict_persist->mutex too. */
  bool in_dirty_dict_tables_list;
#endif /* UNIV_DEBUG */

  /** Maximum recursive level we support when loading tables chained
  together with FK constraints. If exceeds this level, we will stop
  loading child table into memory along with its parent table. */
  unsigned fk_max_recusive_level : 8;

  /** Count of how many foreign key check operations are currently being
  performed on the table. We cannot drop the table while there are
  foreign key checks running on it. */
  std::atomic<ulint> n_foreign_key_checks_running;

  /** Transaction id that last touched the table definition. Either when
  loading the definition or CREATE TABLE, or ALTER TABLE (prepare,
  commit, and rollback phases). */
  trx_id_t def_trx_id;

  /*!< set of foreign key constraints in the table; these refer to
  columns in other tables */
  dict_foreign_set foreign_set;

  /*!< set of foreign key constraints which refer to this table */
  dict_foreign_set referenced_set;

#ifdef UNIV_DEBUG
  /** This field is used to specify in simulations tables which are so
  big that disk should be accessed. Disk access is simulated by putting
  the thread to sleep for a while. NOTE that this flag is not stored to
  the data dictionary on disk, and the database will forget about value
  true if it has to reload the table definition from disk. */
  bool does_not_fit_in_memory;
#endif /* UNIV_DEBUG */

  /** true if the maximum length of a single row exceeds BIG_ROW_SIZE.
  Initialized in dict_table_add_to_cache(). */
  unsigned big_rows : 1;

#ifndef UNIV_HOTBACKUP
  /** Statistics for query optimization. @{ */

  /** Creation state of 'stats_latch'. */
  std::atomic<os_once::state_t> stats_latch_created;

  /** This latch protects:
  "dict_table_t::stat_initialized",
  "dict_table_t::stat_n_rows (*)",
  "dict_table_t::stat_clustered_index_size",
  "dict_table_t::stat_sum_of_other_index_sizes",
  "dict_table_t::stat_modified_counter (*)",
  "dict_table_t::indexes*::stat_n_diff_key_vals[]",
  "dict_table_t::indexes*::stat_index_size",
  "dict_table_t::indexes*::stat_n_leaf_pages".
  (*) Those are not always protected for
  performance reasons. */
  rw_lock_t *stats_latch;

  /** true if statistics have been calculated the first time after
  database startup or table creation. */
  unsigned stat_initialized : 1;

  /** Timestamp of last recalc of the stats. */
  std::chrono::steady_clock::time_point stats_last_recalc;

/** The two bits below are set in the 'stat_persistent' member. They
have the following meaning:
1. _ON=0, _OFF=0, no explicit persistent stats setting for this table,
the value of the global srv_stats_persistent is used to determine
whether the table has persistent stats enabled or not
2. _ON=0, _OFF=1, persistent stats are explicitly disabled for this
table, regardless of the value of the global srv_stats_persistent
3. _ON=1, _OFF=0, persistent stats are explicitly enabled for this
table, regardless of the value of the global srv_stats_persistent
4. _ON=1, _OFF=1, not allowed, we assert if this ever happens. */
#define DICT_STATS_PERSISTENT_ON (1 << 1)
#define DICT_STATS_PERSISTENT_OFF (1 << 2)

  /** Indicates whether the table uses persistent stats or not. See
  DICT_STATS_PERSISTENT_ON and DICT_STATS_PERSISTENT_OFF. */
  uint32_t stat_persistent;

/** The two bits below are set in the 'stats_auto_recalc' member. They
have the following meaning:
1. _ON=0, _OFF=0, no explicit auto recalc setting for this table, the
value of the global srv_stats_persistent_auto_recalc is used to
determine whether the table has auto recalc enabled or not
2. _ON=0, _OFF=1, auto recalc is explicitly disabled for this table,
regardless of the value of the global srv_stats_persistent_auto_recalc
3. _ON=1, _OFF=0, auto recalc is explicitly enabled for this table,
regardless of the value of the global srv_stats_persistent_auto_recalc
4. _ON=1, _OFF=1, not allowed, we assert if this ever happens. */
#define DICT_STATS_AUTO_RECALC_ON (1 << 1)
#define DICT_STATS_AUTO_RECALC_OFF (1 << 2)

  /** Indicates whether the table uses automatic recalc for persistent
  stats or not. See DICT_STATS_AUTO_RECALC_ON and
  DICT_STATS_AUTO_RECALC_OFF. */
  uint32_t stats_auto_recalc;

  /** The number of pages to sample for this table during persistent
  stats estimation. If this is 0, then the value of the global
  srv_stats_persistent_sample_pages will be used instead. */
  ulint stats_sample_pages;

  /** Approximate number of rows in the table. We periodically calculate
  new estimates. */
  uint64_t stat_n_rows;

  /** Approximate clustered index size in database pages. */
  ulint stat_clustered_index_size;

  /** Approximate size of other indexes in database pages. */
  ulint stat_sum_of_other_index_sizes;

  /** If FTS AUX table, parent table id */
  table_id_t parent_id;

  /** How many rows are modified since last stats recalc. When a row is
  inserted, updated, or deleted, we add 1 to this number; we calculate
  new estimates for the table and the indexes if the table has changed
  too much, see row_update_statistics_if_needed(). The counter is reset
  to zero at statistics calculation. This counter is not protected by
  any latch, because this is only used for heuristics. */
  uint64_t stat_modified_counter;

/** Background stats thread is not working on this table. */
#define BG_STAT_NONE 0

/** Set in 'stats_bg_flag' when the background stats code is working
on this table. The DROP TABLE code waits for this to be cleared before
proceeding. */
#define BG_STAT_IN_PROGRESS (1 << 0)

/** Set in 'stats_bg_flag' when DROP TABLE starts waiting on
BG_STAT_IN_PROGRESS to be cleared. The background stats thread will
detect this and will eventually quit sooner. */
#define BG_STAT_SHOULD_QUIT (1 << 1)

  /** The state of the background stats thread wrt this table.
  See BG_STAT_NONE, BG_STAT_IN_PROGRESS and BG_STAT_SHOULD_QUIT.
  Writes are covered by dict_sys->mutex. Dirty reads are possible. */
  byte stats_bg_flag;

  /** @} */
#endif /* !UNIV_HOTBACKUP */

  /** AUTOINC related members. @{ */

  /* The actual collection of tables locked during AUTOINC read/write is
  kept in trx_t. In order to quickly determine whether a transaction has
  locked the AUTOINC lock we keep a pointer to the transaction here in
  the 'autoinc_trx' member. This is to avoid acquiring lock_sys latches and
  scanning the vector in trx_t.
  When an AUTOINC lock has to wait, the corresponding lock instance is
  created on the trx lock heap rather than use the pre-allocated instance
  in autoinc_lock below. */

  /** A buffer for an AUTOINC lock for this table. We allocate the
  memory here so that individual transactions can get it and release it
  without a need to allocate space from the lock heap of the trx:
  otherwise the lock heap would grow rapidly if we do a large insert
  from a select. */
#ifndef UNIV_HOTBACKUP
  lock_t *autoinc_lock;

  /** Creation state of autoinc_mutex member */
  std::atomic<os_once::state_t> autoinc_mutex_created;
#endif /* !UNIV_HOTBACKUP */

  /** Mutex protecting the autoincrement counter. */
  ib_mutex_t *autoinc_mutex;

  /** Autoinc counter value to give to the next inserted row. */
  uint64_t autoinc;

  /** Mutex protecting the persisted autoincrement counter. */
  ib_mutex_t *autoinc_persisted_mutex;

  /** Autoinc counter value that has been persisted in redo logs or
  DDTableBuffer. It's mainly used when we want to write counter back
  to DDTableBuffer.
  This is different from the 'autoinc' above, which could be bigger
  than this one, because 'autoinc' will get updated right after
  some counters are allocated, but we will write the counter to redo
  logs and update this counter later. Once all allocated counters
  have been written to redo logs, 'autoinc' should be exact the next
  counter of this persisted one.
  We want this counter because when we need to write the counter back
  to DDTableBuffer, we had better keep it consistency with the counter
  that has been written to redo logs. Besides, we can't read the 'autoinc'
  directly easily, because the autoinc_lock is required and there could
  be a deadlock.
  This variable is protected by autoinc_persisted_mutex. */
  uint64_t autoinc_persisted;

  /** The position of autoinc counter field in clustered index. This would
  be set when CREATE/ALTER/OPEN TABLE and IMPORT TABLESPACE, and used in
  modifications to clustered index, such as INSERT/UPDATE. There should
  be no conflict to access it, so no protection is needed. */
  ulint autoinc_field_no;

  /** The transaction that currently holds the the AUTOINC lock on this table.
  Protected by lock_sys table shard latch. To "peek" the current value one
  can read it without any latch, understanding that in general it may change.
  Such access pattern is correct if trx thread wants to check if it has the lock
  granted, as the field can only change to other value when lock is released,
  which can not happen concurrently to thread executing the trx. */
  std::atomic<const trx_t *> autoinc_trx;

  /** @} */

#ifndef UNIV_HOTBACKUP
  /** FTS specific state variables. */
  fts_t *fts;
#endif /* !UNIV_HOTBACKUP */

  /** Quiescing states, protected by the dict_index_t::lock. ie. we can
  only change the state if we acquire all the latches (dict_index_t::lock)
  in X mode of this table's indexes. */
  ib_quiesce_t quiesce;

  /** Count of the number of record locks on this table. We use this to
  determine whether we can evict the table from the dictionary cache.
  Writes (atomic increments and decrements) are performed when holding a shared
  latch on lock_sys. (Note that this the table's shard latch is NOT required,
  as this is field counts *record* locks, so a page shard is latched instead)
  Reads should be performed when holding exclusive lock_sys latch, however:
  - Some places assert this field is zero without holding any latch.
  - Some places assert this field is positive holding only shared latch. */
  std::atomic<size_t> n_rec_locks;

#ifndef UNIV_DEBUG
 private:
#endif
  /** Count of how many handles are opened to this table. Dropping of the
  table is NOT allowed until this count gets to zero. MySQL does NOT
  itself check the number of open handles at DROP. */
  std::atomic<uint64_t> n_ref_count;

 public:
#ifndef UNIV_HOTBACKUP
  /** List of locks on the table. Protected by lock_sys shard latch. */
  table_lock_list_t locks;
  /** count_by_mode[M] = number of locks in this->locks with
  lock->type_mode&LOCK_MODE_MASK == M.
  Used to quickly verify that there are no LOCK_S or LOCK_X, which are the only
  modes incompatible with LOCK_IS and LOCK_IX, to avoid costly iteration over
  this->locks when adding LOCK_IS or LOCK_IX.
  We use count_by_mode[LOCK_AUTO_INC] to track the number of granted and pending
  autoinc locks on this table. This value is set after acquiring the lock_sys
  table shard latch, but we peek the contents to determine whether other
  transactions have acquired the AUTOINC lock or not. Of course only one
  transaction can be granted the lock but there can be multiple
  waiters.
  Protected by lock_sys table shard latch. */
  ulong count_by_mode[LOCK_NUM];
#endif /* !UNIV_HOTBACKUP */

  /** Timestamp of the last modification of this table. */
  std::atomic<std::chrono::system_clock::time_point> update_time;
  static_assert(decltype(update_time)::is_always_lock_free);

  /** row-id counter for use by intrinsic table for getting row-id.
  Given intrinsic table semantics, row-id can be locally maintained
  instead of getting it from central generator which involves mutex
  locking. */
  uint64_t sess_row_id;

  /** trx_id counter for use by intrinsic table for getting trx-id.
  Intrinsic table are not shared so don't need a central trx-id
  but just need a increased counter to track consistent view while
  proceeding SELECT as part of UPDATE. */
  uint64_t sess_trx_id;

#ifdef UNIV_DEBUG
  /** Magic number. */
  ulint magic_n;
#endif /* UNIV_DEBUG */
  /** mysql_row_templ_t for base columns used for compute the virtual
  columns */
  dict_vcol_templ_t *vc_templ;

  /** remove the dict_table_t from cache after DDL operation */
  bool discard_after_ddl;

  /** refresh/reload FK info */
  bool refresh_fk;

#ifndef UNIV_HOTBACKUP
  /** multiple cursors can be active on this temporary table */
  temp_prebuilt_vec *temp_prebuilt;
#endif /* !UNIV_HOTBACKUP */

  /** true only for dictionary tables like mysql/tables,
  mysql/columns, mysql/tablespaces, etc. This flag is used
  to do non-locking reads on DD tables. */
  bool is_dd_table;

  /** true if this table is explicitly put to non-LRU list
  during table creation */
  bool explicitly_non_lru;

  /** Check if the table has user defined primary key (PK).
  @return true if table has user defined PK, false otherwise. */
  bool has_pk() const;

  /** @return the clustered index */
  const dict_index_t *first_index() const {
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);
    const dict_index_t *first = UT_LIST_GET_FIRST(indexes);
    return (first);
  }
  /** @return the clustered index */
  dict_index_t *first_index() {
    return (const_cast<dict_index_t *>(
        const_cast<const dict_table_t *>(this)->first_index()));
  }

  /** @returns true if the table has row versions.. */
  bool has_row_versions() const {
    if (current_row_version > 0) {
      ut_ad(has_instant_add_cols() || has_instant_drop_cols());
      return (true);
    }

    return false;
  }

  /** @return if there was any instantly added column.
  This will be true after one or more instant ADD COLUMN, however,
  it would become false after ALTER TABLE which rebuilds or copies
  the old table.
  If this is true, all instantly added columns should have default
  values, and records in the table may have REC_INFO_INSTANT_FLAG set. */
  bool has_instant_cols() const {
    if (is_upgraded_instant() || (n_instant_cols < n_cols)) {
      /* Instant add col V1 */
      return (true);
    }

    return false;
  }

  /** Set the number of columns when the first instant ADD COLUMN happens.
  @param[in]    n_inst_cols     number of fields when first instant
                                ADD COLUMN happens, without system columns */
  void set_instant_cols(uint16_t n_inst_cols) {
    n_instant_cols = static_cast<unsigned>(n_inst_cols) + get_n_sys_cols();
  }

  /** Get the number of user columns when the first instant ADD COLUMN
  happens.
  @return       the number of user columns as described above */
  uint16_t get_instant_cols() const {
    return static_cast<uint16_t>(n_instant_cols - get_n_sys_cols());
  }

  size_t get_n_instant_added_col_v1() const {
    size_t n_cols_dropped = get_n_instant_drop_cols();
    size_t n_cols_added = get_n_instant_add_cols();
    size_t n_instant_added_cols =
        n_cols + n_cols_dropped - n_cols_added - n_instant_cols;

    return (n_instant_added_cols);
  }

  /** Get number of columns added instantly */
  uint32_t get_n_instant_add_cols() const {
    ut_ad(total_col_count >= initial_col_count);
    return total_col_count - initial_col_count;
  }

  /** Get number of columns dropped instantly */
  uint32_t get_n_instant_drop_cols() const {
    ut_ad(total_col_count >= current_col_count);
    return total_col_count - current_col_count;
  }

  /** check if table has INSTANT ADD columns.
  @return true if the table has INSTANT ADD columns, otherwise false */
  bool has_instant_add_cols() const { return (get_n_instant_add_cols() > 0); }

  /** check if table has INSTANT DROP columns.
  @return true if the table has INSTANT DROP columns, otherwise false */
  bool has_instant_drop_cols() const { return (get_n_instant_drop_cols() > 0); }

  /** Set table to be upgraded table with INSTANT ADD columns in V1. */
  void set_upgraded_instant() { m_upgraded_instant = true; }

  /** Checks if table is upgraded table with INSTANT ADD columns in V1.
  @return       true if it is, false otherwise */
  bool is_upgraded_instant() const { return m_upgraded_instant; }

  /** Check whether the table is corrupted.
  @return true if the table is corrupted, otherwise false */
  bool is_corrupted() const {
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);

    const dict_index_t *index = first_index();

    /* It is possible that this table is only half created, in which case
    the clustered index may be NULL.  If the clustered index is corrupted,
    the table is corrupt.  We do not consider the table corrupt if only
    a secondary index is corrupt. */
    ut_ad(index == nullptr || index->is_clustered());

    return (index != nullptr && index->type & DICT_CORRUPT);
  }

  /** Returns a column's name.
  @param[in] col_nr     column number
  @return column name. NOTE: not guaranteed to stay valid if table is
  modified in any way (columns added, etc.). */
  const char *get_col_name(ulint col_nr) const {
    ut_ad(col_nr < n_def);
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);

    const char *s = col_names;
    if (s) {
      for (ulint i = 0; i < col_nr; i++) {
        s += strlen(s) + 1;
      }
    }

    return (s);
  }

  /** Gets the nth column of a table.
  @param[in] pos        position of column
  @return pointer to column object */
  dict_col_t *get_col(uint pos) const {
    ut_ad(pos < n_def);
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);

    return (cols + pos);
  }

  /** Get column by name
  @param[in]    name    column name
  @return column name if found, null otherwise */
  dict_col_t *get_col_by_name(const char *name) const {
    ut_ad(name != nullptr);

    dict_col_t *ret = nullptr;

    const char *s = col_names;
    for (ulint i = 0; i < n_def; i++) {
      if (strcmp(s, name) == 0) {
        ret = get_col(i);
      }
      s += strlen(s) + 1;
    }

    return ret;
  }

  /** Gets the number of user-defined non-virtual columns in a table
  in the dictionary cache.
  @return number of user-defined (e.g., not ROW_ID) non-virtual columns
  of a table */
  uint16_t get_n_user_cols() const {
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);

    return (static_cast<uint16_t>(n_cols) - get_n_sys_cols());
  }

  /** Gets the number of system columns in a table.
  For intrinsic table on ROW_ID column is added for all other
  tables TRX_ID and ROLL_PTR are all also appended.
  @return number of system (e.g., ROW_ID) columns of a table */
  uint16_t get_n_sys_cols() const {
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);

    return (is_intrinsic() ? DATA_ITT_N_SYS_COLS : DATA_N_SYS_COLS);
  }

  /** Gets the number of all non-virtual columns (also system) in a table
  in the dictionary cache.
  @return number of non-virtual columns of a table */
  ulint get_n_cols() const {
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);

    return (n_cols);
  }

  /** Gets the number of all non-virtual columns in a table including columns
  dropped INSTANTly.
  @returns number of non-virtual columns of a table */
  ulint get_total_cols() const {
    if (!has_row_versions()) {
      return n_cols;
    }

    ut_ad((total_col_count + get_n_sys_cols()) ==
          (n_cols + get_n_instant_drop_cols()));
    return n_cols + get_n_instant_drop_cols();
  }

  /** Gets the given system column of a table.
  @param[in] sys DATA_ROW_ID, ...
  @return pointer to column object */
  dict_col_t *get_sys_col(ulint sys) const {
    dict_col_t *col;

    ut_ad(sys < get_n_sys_cols());
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);

    col = get_col(n_cols - get_n_sys_cols() + sys);
    ut_ad(col->mtype == DATA_SYS);
    ut_ad(col->prtype == (sys | DATA_NOT_NULL));

    return (col);
  }

  /** Determine if this is a temporary table. */
  bool is_temporary() const {
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);
    return (flags2 & DICT_TF2_TEMPORARY);
  }

  /** Determine if this is a FTS AUX table. */
  bool is_fts_aux() const {
    ut_ad(magic_n == DICT_TABLE_MAGIC_N);
    return (flags2 & DICT_TF2_AUX);
  }

  /** Determine whether the table is intrinsic.
  An intrinsic table is a special kind of temporary table that
  is invisible to the end user. It can be created internally by InnoDB,
  the MySQL server layer or other modules connected to InnoDB in order
  to gather and use data as part of a larger task. Since access to it
  must be as fast as possible, it does not need UNDO semantics, system
  fields DB_TRX_ID & DB_ROLL_PTR, doublewrite, checksum, insert buffer,
  use of the shared data dictionary, locking, or even a transaction.
  In short, these are not ACID tables at all, just temporary data stored
  and manipulated during a larger process.*/
  bool is_intrinsic() const {
    if (flags2 & DICT_TF2_INTRINSIC) {
      ut_ad(is_temporary());
      return (true);
    }

    return (false);
  }

  /* GAP locks are skipped for DD tables and SDI tables
  @return true if table is DD table or SDI table, else false */
  inline bool skip_gap_locks() const;

  /** Determine if the table can support instant ADD/DROP COLUMN */
  inline bool support_instant_add_drop() const;
};

static inline void DICT_TF2_FLAG_SET(dict_table_t *table, uint32_t flag) {
  table->flags2 |= flag;
}

static inline bool DICT_TF2_FLAG_IS_SET(const dict_table_t *table,
                                        uint32_t flag) {
  return table->flags2 & flag;
}

static inline void DICT_TF2_FLAG_UNSET(dict_table_t *table, uint32_t flag) {
  table->flags2 &= ~flag;
}

bool dict_index_t::is_compressed() const { return (table->is_compressed()); }

/** Persistent dynamic metadata type, there should be 1 to 1
relationship between the metadata and the type. Please keep them in order
so that we can iterate over it */
enum persistent_type_t {
  /** The smallest type, which should be 1 less than the first
  true type */
  PM_SMALLEST_TYPE = 0,

  /** Persistent Metadata type for corrupted indexes */
  PM_INDEX_CORRUPTED = 1,

  /** Persistent Metadata type for autoinc counter */
  PM_TABLE_AUTO_INC = 2,

  /* TODO: Will add following types
  PM_TABLE_UPDATE_TIME = 3,
  Maybe something tablespace related
  PM_TABLESPACE_SIZE = 4,
  PM_TABLESPACE_MAX_TRX_ID = 5, */

  /** The biggest type, which should be 1 bigger than the last
  true type */
  PM_BIGGEST_TYPE = 3
};

typedef std::vector<index_id_t, ut::allocator<index_id_t>> corrupted_ids_t;

/** Persistent dynamic metadata for a table */
class PersistentTableMetadata {
 public:
  /** Constructor
  @param[in]    id      table id
  @param[in]    version table dynamic metadata version */
  PersistentTableMetadata(table_id_t id, uint64_t version)
      : m_id(id), m_version(version), m_corrupted_ids(), m_autoinc(0) {}

  /** Get the corrupted indexes' IDs
  @return the vector of indexes' IDs */
  const corrupted_ids_t &get_corrupted_indexes() const {
    return (m_corrupted_ids);
  }

  /** Add a corrupted index id and space id
  @param[in]    id      corrupted index id */
  void add_corrupted_index(const index_id_t id) {
    m_corrupted_ids.push_back(id);
  }

  /** Set the dynamic metadata version.
  @param[in]    version         dynamic metadata version */
  void set_version(uint64_t version) { m_version = version; }

  /** Get the dynamic metadata version */
  uint64_t get_version() const { return (m_version); }

  /** Get the table id of the metadata
  @return table id */
  table_id_t get_table_id() const { return (m_id); }

  /** Set the autoinc counter of the table if it's bigger
  @param[in]    autoinc autoinc counter */
  void set_autoinc_if_bigger(uint64_t autoinc) {
    /* We only set the biggest autoinc counter. Callers don't
    guarantee passing a bigger number in. */
    if (autoinc > m_autoinc) {
      m_autoinc = autoinc;
    }
  }

  /** Set the autoinc counter of the table
  @param[in]    autoinc autoinc counter */
  void set_autoinc(uint64_t autoinc) { m_autoinc = autoinc; }

  /** Get the autoinc counter of the table
  @return the autoinc counter */
  uint64_t get_autoinc() const { return (m_autoinc); }

 private:
  /** Table ID which this metadata belongs to */
  table_id_t m_id;

  /** Table dynamic metadata version of the change */
  uint64_t m_version;

  /** Storing the corrupted indexes' ID if exist, or else empty */
  corrupted_ids_t m_corrupted_ids;

  /** Autoinc counter of the table */
  uint64_t m_autoinc;

  /* TODO: We will add update_time, etc. here and APIs accordingly */
};

/** Interface for persistent dynamic table metadata. */
class Persister {
 public:
  /** Virtual destructor */
  virtual ~Persister() = default;

  /** Write the dynamic metadata of a table, we can pre-calculate
  the size by calling get_write_size()
  @param[in]    metadata        persistent data
  @param[out]   buffer          write buffer
  @param[in]    size            size of write buffer, should be
                                  at least get_write_size()
  @return the length of bytes written */
  virtual ulint write(const PersistentTableMetadata &metadata, byte *buffer,
                      ulint size) const = 0;

  /** Pre-calculate the size of metadata to be written
  @param[in]    metadata        metadata to be written
  @return the size of metadata */
  virtual ulint get_write_size(
      const PersistentTableMetadata &metadata) const = 0;

  /** Read the dynamic metadata from buffer, and store them to
  metadata object
  @param[out]   metadata        metadata where we store the read data
  @param[in]    buffer          buffer to read
  @param[in]    size            size of buffer
  @param[out]   corrupt         true if we found something wrong in
                                  the buffer except incomplete buffer,
                                  otherwise false
  @return the bytes we read from the buffer if the buffer data
  is complete and we get everything, 0 if the buffer is incompleted */
  virtual ulint read(PersistentTableMetadata &metadata, const byte *buffer,
                     ulint size, bool *corrupt) const = 0;

  /** Aggregate metadata entries into a single metadata instance, considering
  version numbers
  @param[in,out] metadata        metadata object to be modified
  @param[in]     new_entry       metadata entry from logs */
  virtual void aggregate(PersistentTableMetadata &metadata,
                         const PersistentTableMetadata &new_entry) const = 0;

  /** Write MLOG_TABLE_DYNAMIC_META for persistent dynamic
  metadata of table
  @param[in]    id              Table id
  @param[in]    metadata        Metadata used to write the log
  @param[in,out]        mtr             Mini-transaction */
  void write_log(table_id_t id, const PersistentTableMetadata &metadata,
                 mtr_t *mtr) const;
};

/** Persister used for corrupted indexes */
class CorruptedIndexPersister : public Persister {
 public:
  /** Write the corrupted indexes of a table, we can pre-calculate the size
  by calling get_write_size()
  @param[in]    metadata        persistent data
  @param[out]   buffer          write buffer
  @param[in]    size            size of write buffer, should be at least
                                  get_write_size()
  @return the length of bytes written */
  ulint write(const PersistentTableMetadata &metadata, byte *buffer,
              ulint size) const override;

  /** Pre-calculate the size of metadata to be written
  @param[in]    metadata        metadata to be written
  @return the size of metadata */
  ulint get_write_size(const PersistentTableMetadata &metadata) const override;

  /** Read the corrupted indexes from buffer, and store them to
  metadata object
  @param[out]   metadata        metadata where we store the read data
  @param[in]    buffer          buffer to read
  @param[in]    size            size of buffer
  @param[out]   corrupt         true if we found something wrong in
                                  the buffer except incomplete buffer,
                                  otherwise false
  @return the bytes we read from the buffer if the buffer data
  is complete and we get everything, 0 if the buffer is incompleted */
  ulint read(PersistentTableMetadata &metadata, const byte *buffer, ulint size,
             bool *corrupt) const override;

  void aggregate(PersistentTableMetadata &metadata,
                 const PersistentTableMetadata &new_entry) const override;

 private:
  /** The length of index_id_t we will write */
  static const size_t INDEX_ID_LENGTH = 12;
};

/** Persister used for autoinc counters */
class AutoIncPersister : public Persister {
 public:
  /** Write the autoinc counter of a table, we can pre-calculate
  the size by calling get_write_size()
  @param[in]    metadata        persistent metadata
  @param[out]   buffer          write buffer
  @param[in]    size            size of write buffer, should be
                                  at least get_write_size()
  @return the length of bytes written */
  ulint write(const PersistentTableMetadata &metadata, byte *buffer,
              ulint size) const override;

  /** Pre-calculate the size of metadata to be written
  @param[in]    metadata        metadata to be written
  @return the size of metadata */
  inline ulint get_write_size(const PersistentTableMetadata &metadata
                              [[maybe_unused]]) const override {
    /* We just return the max possible size that would be used
    if the counter exists, so we don't calculate every time.
    Here we need 1 byte for dynamic metadata type and 11 bytes
    for the max possible size of counter. */
    return (12);
  }

  /** Read the autoinc counter from buffer, and store them to
  metadata object
  @param[out]   metadata        metadata where we store the read data
  @param[in]    buffer          buffer to read
  @param[in]    size            size of buffer
  @param[out]   corrupt         true if we found something wrong in
                                  the buffer except incomplete buffer,
                                  otherwise false
  @return the bytes we read from the buffer if the buffer data
  is complete and we get everything, 0 if the buffer is incomplete */
  ulint read(PersistentTableMetadata &metadata, const byte *buffer, ulint size,
             bool *corrupt) const override;

  void aggregate(PersistentTableMetadata &metadata,
                 const PersistentTableMetadata &new_entry) const override;
};

/** Container of persisters used in the system. Currently we don't need
to protect this object since we only initialize it at very beginning and
destroy it in the end. During the server running, we only get the persisters */
class Persisters {
  typedef std::map<
      persistent_type_t, Persister *, std::less<persistent_type_t>,
      ut::allocator<std::pair<const persistent_type_t, Persister *>>>
      persisters_t;

 public:
  /** Constructor */
  Persisters() : m_persisters() {}

  /** Destructor */
  ~Persisters();

  /** Get the persister object with specified type
  @param[in]    type    persister type
  @return Persister object required or NULL if not found */
  Persister *get(persistent_type_t type) const;

  /** Add a specified persister of type, we will allocate the Persister
  if there is no such persister exist, otherwise do nothing and return
  the existing one
  @param[in]    type    persister type
  @return the persister of type */
  Persister *add(persistent_type_t type);

  /** Remove a specified persister of type, we will free the Persister
  @param[in]    type    persister type */
  void remove(persistent_type_t type);

  /** Serialize the metadata to a buffer
  @param[in]    metadata        metadata to serialize
  @param[out]   buffer          buffer to store the serialized metadata
  @return the length of serialized metadata */
  size_t write(PersistentTableMetadata &metadata, byte *buffer);

 private:
  /** A map to store all persisters needed */
  persisters_t m_persisters;
};

#ifndef UNIV_HOTBACKUP

/** A function object to add the foreign key constraint to the referenced set
of the referenced table, if it exists in the dictionary cache. */
struct dict_foreign_add_to_referenced_table {
  void operator()(dict_foreign_t *foreign) const {
    if (dict_table_t *table = foreign->referenced_table) {
      std::pair<dict_foreign_set::iterator, bool> ret =
          table->referenced_set.insert(foreign);
      ut_a(ret.second);
    }
  }
};

/** Request for lazy creation of the mutex of a given table.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose mutex is to be created */
inline void dict_table_mutex_create_lazy(dict_table_t *table) {
  table->mutex = nullptr;
  table->mutex_created = os_once::NEVER_DONE;
}

/** Destroy the mutex of a given table.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose mutex is to be created */
inline void dict_table_mutex_destroy(dict_table_t *table) {
  if (table->mutex_created == os_once::DONE) {
    if (table->mutex != nullptr) {
      mutex_free(table->mutex);
      ut::delete_(table->mutex);
    }
  }
}

/** Destroy the autoinc latch of the given table.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose stats latch to destroy */
inline void dict_table_autoinc_destroy(dict_table_t *table) {
  if (table->autoinc_mutex_created == os_once::DONE) {
    if (table->autoinc_mutex != nullptr) {
      mutex_free(table->autoinc_mutex);
      ut::delete_(table->autoinc_mutex);
    }

    if (table->autoinc_persisted_mutex != nullptr) {
      mutex_free(table->autoinc_persisted_mutex);
      ut::delete_(table->autoinc_persisted_mutex);
    }
  }
}

/** Request for lazy creation of the autoinc latch of a given table.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose autoinc latch is to be created. */
inline void dict_table_autoinc_create_lazy(dict_table_t *table) {
  table->autoinc_mutex = nullptr;
  table->autoinc_persisted_mutex = nullptr;
  table->autoinc_mutex_created = os_once::NEVER_DONE;
}

/** Request a lazy creation of dict_index_t::zip_pad::mutex.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  index   index whose zip_pad mutex is to be created */
inline void dict_index_zip_pad_mutex_create_lazy(dict_index_t *index) {
  index->zip_pad.mutex = nullptr;
  index->zip_pad.mutex_created = os_once::NEVER_DONE;
}

/** Destroy the zip_pad_mutex of the given index.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  index   index whose stats latch to destroy */
inline void dict_index_zip_pad_mutex_destroy(dict_index_t *index) {
  if (index->zip_pad.mutex_created == os_once::DONE &&
      index->zip_pad.mutex != nullptr) {
    mutex_free(index->zip_pad.mutex);
    ut::delete_(index->zip_pad.mutex);
  }
}
#endif /* !UNIV_HOTBACKUP */

/** Release the zip_pad_mutex of a given index.
@param[in,out]  index   index whose zip_pad_mutex is to be released */
inline void dict_index_zip_pad_unlock(dict_index_t *index) {
#ifndef UNIV_HOTBACKUP
  mutex_exit(index->zip_pad.mutex);
#endif /* !UNIV_HOTBACKUP */
}

#ifdef UNIV_DEBUG
/** Check if the current thread owns the autoinc_mutex of a given table.
@param[in]      table   the autoinc_mutex belongs to this table
@return true, if the current thread owns the autoinc_mutex, false otherwise.*/
inline bool dict_table_autoinc_own(const dict_table_t *table) {
  return (mutex_own(table->autoinc_mutex));
}
#endif /* UNIV_DEBUG */

#include "dict0mem.ic"

#endif /* dict0mem_h */
