/* Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

#ifndef KEY_INCLUDED
#define KEY_INCLUDED

#include <assert.h>
#include <stddef.h>
#include <sys/types.h>

#include "lex_string.h"
#include "my_base.h" /* ha_rows, ha_key_alg */

#include "my_inttypes.h"
#include "sql/key_spec.h"       /* fk_option */
#include "sql/sql_plugin_ref.h" /* plugin_ref */

class Field;
class String;
struct MY_BITMAP;
struct TABLE;

class FOREIGN_KEY {
 public:
  const char *name;
  const char *unique_index_name;
  uint key_parts;
  LEX_CSTRING *key_part;
  LEX_CSTRING *fk_key_part;
  LEX_CSTRING ref_db;
  LEX_CSTRING ref_table;
  fk_option delete_opt;
  fk_option update_opt;
  fk_match_opt match_opt;
};

class KEY_PART_INFO { /* Info about a key part */
 public:
  Field *field{nullptr};
  uint offset{0};      /* offset in record (from 0) */
  uint null_offset{0}; /* Offset to null_bit in record */
  /* Length of key part in bytes, excluding NULL flag and length bytes */
  uint16 length{0};
  /*
    Number of bytes required to store the keypart value. This may be
    different from the "length" field as it also counts
     - possible NULL-flag byte (see HA_KEY_NULL_LENGTH)
     - possible HA_KEY_BLOB_LENGTH bytes needed to store actual value length.
  */
  uint16 store_length{0};
  uint16 fieldnr{0};       /* Fieldnum in UNIREG */
  uint16 key_part_flag{0}; /* 0 or HA_REVERSE_SORT */
  uint8 type{0};
  uint8 null_bit{0}; /* Position to null_bit */
  /**
    True - if key part allows trivial binary comparison,
    False - if charset collation function needs to be involved.

    @note Not set for KEY_PART_INFO which are used for creating tables,
          only set when table is opened or for internal temporary tables.

    This value is set a bit too optimistically and disregards the way
    in which value is stored in record (e.g. it is true for BLOB types).
    So in practice key_cmp_if_same() also has to check key_part_flag for
    presence of HA_BLOB_PART, HA_VAR_LENGTH_PART and HA_BIT_PART flags.
  */
  bool bin_cmp{false};
  void init_from_field(Field *fld); /** Fill data from given field */
  void init_flags();                /** Set key_part_flag from field */
};

/**
  Data type for records per key estimates that are stored in the
  KEY::rec_per_key_float[] array.
*/
typedef float rec_per_key_t;

/**
  If an entry for a key part in KEY::rec_per_key_float[] has this value,
  then the storage engine has not provided a value for it and the rec_per_key
  value for this key part is unknown.
*/
#define REC_PER_KEY_UNKNOWN -1.0f

/**
  If the "in memory estimate" for a table (in
  ha_statistics.table_in_mem_estimate) or index (in
  KEY::m_in_memory_estimate) is not known or not set by the storage
  engine, then it should have the following value.
*/
#define IN_MEMORY_ESTIMATE_UNKNOWN -1.0

class KEY {
 public:
  /** Tot length of key */
  uint key_length{0};
  /** dupp key and pack flags */
  ulong flags{0};
  /** dupp key and pack flags for actual key parts */
  ulong actual_flags{0};
  /** How many key_parts */
  uint user_defined_key_parts{0};
  /** How many key_parts including hidden parts */
  uint actual_key_parts{0};
  /**
     Key parts allocated for primary key parts extension but
     not used due to some reasons(no primary key, duplicated key parts)
  */
  uint unused_key_parts{0};
  /** Should normally be = actual_key_parts */
  uint usable_key_parts{0};
  uint block_size{0};
  /// @cond Doxygen_is_confused
  enum ha_key_alg algorithm { HA_KEY_ALG_SE_SPECIFIC };
  /// @endcond
  /**
    A flag which indicates that index algorithm for this key was explicitly
    specified by user. So, for example, it should be mentioned in SHOW CREATE
    TABLE output.
  */
  bool is_algorithm_explicit{false};
  /**
    Note that parser is used when the table is opened for use, and
    parser_name is used when the table is being created.
  */
  /** Fulltext [pre]parser */
  plugin_ref parser{nullptr};
  /** Fulltext [pre]parser name */
  LEX_CSTRING parser_name{nullptr, 0};

  KEY_PART_INFO *key_part{nullptr};
  /** Name of key */
  const char *name{nullptr};

  /**
    Array of AVG(number of records with the same field value) for 1st ... Nth
    key part. 0 means 'not known'. For internally created temporary tables,
    this member can be nullptr.
  */
  ulong *rec_per_key{nullptr};

  /**
    @retval true if this is a functional index (at least one of the key parts
                 is a functional key part).
    @retval false if this isn't a functional index.
  */
  bool is_functional_index() const;

  // Can't use in-class initialization as long as we memset-initialize
  // the struct
  LEX_CSTRING engine_attribute{nullptr, 0};
  LEX_CSTRING secondary_engine_attribute{nullptr, 0};

 private:
  /**
    Estimate for how much of the index data that is currently
    available in a memory buffer. Valid range is [0..1]. This will be
    initialized to a IN_MEMORY_ESTIMATE_UNKNOWN. If it still has this
    value when used, it means that the storage engine has not supplied
    a value.
  */
  double m_in_memory_estimate{0.0};

  /**
    Array of AVG(number of records with the same field value) for 1st ... Nth
    key part. For internally created temporary tables, this member can be
    nullptr. This is the same information as stored in the above
    rec_per_key array but using float values instead of integer
    values. If the storage engine has supplied values in this array,
    these will be used. Otherwise the value in rec_per_key will be
    used.  @todo In the next release the rec_per_key array above
    should be removed and only this should be used.
  */
  rec_per_key_t *rec_per_key_float{nullptr};

 public:
  /**
    True if this index is visible to the query optimizer. The optimizer may
    only use visible indexes.
  */
  bool is_visible{false};

  TABLE *table{nullptr};
  LEX_CSTRING comment{nullptr, 0};

  /**
    Check if records per key estimate is available for given key part.

    @param key_part_no key part number, must be in [0, KEY::actual_key_parts)

    @return true if records per key estimate is available, false otherwise
  */

  bool has_records_per_key(uint key_part_no) const {
    assert(key_part_no < actual_key_parts);

    return ((rec_per_key_float &&
             rec_per_key_float[key_part_no] != REC_PER_KEY_UNKNOWN) ||
            (rec_per_key && rec_per_key[key_part_no] != 0));
  }

  /**
    Retrieve an estimate for the average number of records per distinct value,
    when looking only at the first key_part_no+1 columns.

    If no record per key estimate is available for this key part,
    REC_PER_KEY_UNKNOWN is returned.

    @param key_part_no key part number, must be in [0, KEY::actual_key_parts)

    @return Number of records having the same key value
      @retval REC_PER_KEY_UNKNOWN    no records per key estimate available
      @retval != REC_PER_KEY_UNKNOWN record per key estimate
  */

  rec_per_key_t records_per_key(uint key_part_no) const {
    assert(key_part_no < actual_key_parts);

    /*
      If the storage engine has provided rec per key estimates as float
      then use this. If not, use the integer version.
    */
    if (rec_per_key_float[key_part_no] != REC_PER_KEY_UNKNOWN)
      return rec_per_key_float[key_part_no];

    return (rec_per_key[key_part_no] != 0)
               ? static_cast<rec_per_key_t>(rec_per_key[key_part_no])
               : REC_PER_KEY_UNKNOWN;
  }

  /**
    Set the records per key estimate for a key part.

    The records per key estimate must be in [1.0,..> or take the value
    REC_PER_KEY_UNKNOWN.

    @param key_part_no     the number of key parts that the estimate includes,
                           must be in [0, KEY::actual_key_parts)
    @param rec_per_key_est new records per key estimate
  */

  void set_records_per_key(uint key_part_no, rec_per_key_t rec_per_key_est) {
    assert(key_part_no < actual_key_parts);
    assert(rec_per_key_est == REC_PER_KEY_UNKNOWN || rec_per_key_est >= 1.0);
    assert(rec_per_key_float != nullptr);

    rec_per_key_float[key_part_no] = rec_per_key_est;
  }

  /**
    Check if this key supports storing records per key information.

    @return true if it has support for storing records per key information,
            false otherwise.
  */

  bool supports_records_per_key() const {
    if (rec_per_key_float != nullptr && rec_per_key != nullptr) return true;

    return false;
  }

  /**
    Assign storage for the rec per key arrays to the KEY object.

    This is used when allocating memory and creating KEY objects. The
    caller is responsible for allocating the correct size for the
    two arrays. If needed, the caller is also responsible for
    de-allocating the memory when the KEY object is no longer used.

    @param rec_per_key_arg       pointer to allocated array for storing
                                 records per key using ulong
    @param rec_per_key_float_arg pointer to allocated array for storing
                                 records per key using float
  */

  void set_rec_per_key_array(ulong *rec_per_key_arg,
                             rec_per_key_t *rec_per_key_float_arg) {
    rec_per_key = rec_per_key_arg;
    rec_per_key_float = rec_per_key_float_arg;
  }

  /**
    Move rec_per_key arrays from old to new position.

    Ignore if arrays have not been set up yet.

    @param rec_per_key_arg       pointer to adjusted rec per key array
    @param rec_per_key_float_arg pointer to adjusted rec per key array (float)
  */

  void move_rec_per_key(ulong *rec_per_key_arg,
                        rec_per_key_t *rec_per_key_float_arg) {
    if (rec_per_key_float == nullptr || rec_per_key == nullptr) return;

    rec_per_key_t *old_rec_per_key_float = rec_per_key_float;
    ulong *old_rec_per_key = rec_per_key;
    set_rec_per_key_array(rec_per_key_arg, rec_per_key_float_arg);
    for (uint i = 0; i < actual_key_parts; i++) {
      rec_per_key[i] = old_rec_per_key[i];
      rec_per_key_float[i] = old_rec_per_key_float[i];
    }
  }

  /**
    Retrieve the estimate for how much of the index data that is available
    in a memory buffer.

    The returned estimate will be in the interval [0..1].

    @return Estimate for how much of index data is available in memory buffer
      @retval IN_MEMORY_ESTIMATE_UNKNOWN no estimate available
      @retval != IN_MEMORY_ESTIMATE_UNKNOWN estimate
  */

  double in_memory_estimate() const {
    assert(m_in_memory_estimate == IN_MEMORY_ESTIMATE_UNKNOWN ||
           (m_in_memory_estimate >= 0.0 && m_in_memory_estimate <= 1.0));

    return m_in_memory_estimate;
  }

  /**
    Set the estimate for how much of this index that is currently in a
    memory buffer.

    The estimate must be in the interval [0..1] or take the value
    IN_MEMORY_ESTIMATE_UNKNOWN.
  */

  void set_in_memory_estimate(double in_memory_estimate) {
    assert(in_memory_estimate == IN_MEMORY_ESTIMATE_UNKNOWN ||
           (in_memory_estimate >= 0.0 && in_memory_estimate <= 1.0));

    m_in_memory_estimate = in_memory_estimate;
  }
};

int find_ref_key(KEY *key, uint key_count, uchar *record, Field *field,
                 uint *key_length, uint *keypart);
void key_copy(uchar *to_key, const uchar *from_record, const KEY *key_info,
              uint key_length);
void key_restore(uchar *to_record, const uchar *from_key, const KEY *key_info,
                 uint key_length);
bool key_cmp_if_same(const TABLE *table, const uchar *key, uint index,
                     uint key_length);
void key_unpack(String *to, TABLE *table, KEY *key);
void field_unpack(String *to, Field *field, uint max_length, bool prefix_key);
bool is_key_used(TABLE *table, uint idx, const MY_BITMAP *fields);
int key_cmp(KEY_PART_INFO *key_part, const uchar *key, uint key_length);
int key_cmp2(KEY_PART_INFO *key_part, const uchar *key1, uint key1_length,
             const uchar *key2, uint key2_length);
int key_rec_cmp(KEY **key_info, uchar *a, uchar *b);

#endif /* KEY_INCLUDED */
