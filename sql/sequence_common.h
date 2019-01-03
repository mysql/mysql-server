/* Copyright (c) 2000, 2017, Alibaba and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SEQUENCE_COMMON_INCLUDED
#define SEQUENCE_COMMON_INCLUDED

#include "binary_log_types.h"    // MYSQL_TYPE_LONGLONG
#include "map_helpers.h"         // collation_unordered_map
#include "sql/psi_memory_key.h"  // PSI_memory_key
#include "sql/sql_plugin_ref.h"  // plugin_ref

struct handlerton;
class THD;
struct TABLE;
class Alter_info;

extern PSI_memory_key key_memory_sequence_last_value;

/**
  Sequence table field value structure.
*/
struct st_sequence_value {
  ulonglong currval;
  ulonglong nextval;
  ulonglong minvalue;
  ulonglong maxvalue;
  ulonglong start;
  ulonglong increment;
  ulonglong cache;
  ulonglong cycle;
  ulonglong round;
};

/**
  Sequence create information.
*/
class Sequence_info {
 public:
  /* All the sequence fields.*/
  enum Fields {
    FIELD_NUM_CURRVAL = 0,
    FIELD_NUM_NEXTVAL,
    FIELD_NUM_MINVALUE,
    FIELD_NUM_MAXVALUE,
    FIELD_NUM_START,
    FIELD_NUM_INCREMENT,
    FIELD_NUM_CACHE,
    FIELD_NUM_CYCLE,
    FIELD_NUM_ROUND,
    /* This must be last! */
    FIELD_NUM_END
  };

  /**
    Construtor and Destrutor
  */
  Sequence_info();
  virtual ~Sequence_info() {}

  /* Disable the copy and assign function */
  Sequence_info(const Sequence_info &) = delete;
  Sequence_info(const Sequence_info &&) = delete;
  Sequence_info &operator=(const Sequence_info &) = delete;

  /**
    Sequence field setting function

    @param[in]    field_num   Sequence field number
    @param[in]    value       Sequence field value

    @retval       void
  */
  void init_value(const Fields field_num, const ulonglong value);

  /**
    Sequence field getting function

    @param[in]    field_num   Sequence field number

    @retval       ulonglong   Sequence field value
  */
  ulonglong get_value(const Fields field_num) const;

  /*
    Check whether inited values are valid through
      syntax: 'CREATE SEQUENCE ...'

    @retval       true        Invalid
    @retval       false       valid
  */
  bool check_valid() const;

  const char *db;
  const char *table_name;
  handlerton *base_db_type; /** Sequence table engine */
 private:
  /**
    Assign initial default values of sequence fields

    @retval   void
  */
  void init_default();

  ulonglong values[Fields::FIELD_NUM_END];
};

typedef Sequence_info::Fields Sequence_field;

/**
  Sequence table fields definition.
*/
struct st_sequence_field_info {
  const char *field_name;
  const char *field_length;
  const Sequence_field field_num;
  const enum enum_field_types field_type;
  const LEX_STRING comment;
};

/**
  The sequence value structure should be consistent with Sequence field
  definition
*/
static_assert(sizeof(ulonglong) * Sequence_field::FIELD_NUM_END ==
                  sizeof(struct st_sequence_value),
              "");

/**
  Sequence attributes within TABLE_SHARE object, label the table as sequence
  table.
*/
class Sequence_property {
 public:
  Sequence_property() : m_sequence(false), base_db_type(NULL), m_plugin(NULL) {}

  ~Sequence_property();

  /* Disable these copy and assign functions */
  Sequence_property(const Sequence_property &) = delete;
  Sequence_property(const Sequence_property &&) = delete;
  Sequence_property &operator=(const Sequence_property &) = delete;

  /**
    Configure the sequence flags and base db_type when open_table_share.

    @param[in]    plugin      Storage engine plugin
  */
  void configure(plugin_ref plugin);
  bool is_sequence() { return m_sequence; }
  handlerton *db_type() { return base_db_type; }

 private:
  bool m_sequence;
  handlerton *base_db_type;
  plugin_ref m_plugin;
};

/**
  Sequence scan mode in TABLE object.
*/
class Sequence_scan {
 public:
  /**
    Scan mode example like:

      ORIGINAL_SCAN 'SELECT * FROM s'
      ITERATION_SCAN 'SELECT NEXTVAL(s), CURRVAL(s)'

    Orignal scan only query the base table data.
    Iteration scan will apply the sequence logic.
  */
  enum Scan_mode { ORIGINAL_SCAN = 0, ITERATION_SCAN };

  enum Iter_mode {
    IT_NON,         /* Query the sequence base table */
    IT_NEXTVAL,     /* Query nextval */
    IT_NON_NEXTVAL, /* Query non nextval, maybe currval or others */
  };

  Sequence_scan() : m_mode(ORIGINAL_SCAN) {}

  void reset() { m_mode = ORIGINAL_SCAN; }
  void set(Scan_mode mode) { m_mode = mode; }
  Scan_mode get() { return m_mode; }

  /* Overlap the assignment operator */
  Sequence_scan &operator=(const Sequence_scan &rhs) {
    if (this != &rhs) {
      this->m_mode = rhs.m_mode;
    }
    return *this;
  }

 private:
  Scan_mode m_mode;
};

typedef Sequence_scan::Scan_mode Sequence_scan_mode;
typedef Sequence_scan::Iter_mode Sequence_iter_mode;

/**
  Sequence currval that was saved in THD object.
*/
class Sequence_last_value {
 public:
  Sequence_last_value(){};
  virtual ~Sequence_last_value(){};

  void set_version(ulonglong version) { m_version = version; }
  ulonglong get_version() { return m_version; }

  ulonglong m_values[Sequence_field::FIELD_NUM_END];

 private:
  ulonglong m_version;
};

typedef collation_unordered_map<std::string, Sequence_last_value *>
    Sequence_last_value_hash;

extern const LEX_STRING SEQUENCE_ENGINE_NAME;
extern const LEX_STRING SEQUENCE_BASE_ENGINE_NAME;

/**
  Resolve the sequence engine and sequence base engine, it needs to
  unlock_plugin explicitly if thd is null;
*/
extern plugin_ref ha_resolve_sequence(const THD *thd);
extern plugin_ref ha_resolve_sequence_base(const THD *thd);

extern st_sequence_field_info seq_fields[];

extern bool check_sequence_values_valid(const ulonglong *items);
extern bool check_sequence_fields_valid(Alter_info *alter_info);

extern Sequence_iter_mode sequence_iteration_type(TABLE *table);

/**
  Clear or destroy the global Sequence_share_hash and
  session Sequence_last_value_hash.
*/
template <typename K, typename V>
void clear_hash(collation_unordered_map<K, V> *hash) {
  if (hash) {
    for (auto it = hash->cbegin(); it != hash->cend(); ++it) {
      delete it->second;
    }
    hash->clear();
  }
}
template <typename K, typename V>
void destroy_hash(collation_unordered_map<K, V> *hash) {
  if (hash) {
    clear_hash<K, V>(hash);
    delete hash;
  }
}

#endif
