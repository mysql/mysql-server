/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
#include <sql_class.h>
#include <sql_show.h>
#include <sql_table.h>
#include <sql_tablespace.h>
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0load.h"
#include "row0sel.h"
#include "ha_innodb.h"
#include "ha_innopart.h"
#include "dict0upgrade.h"
#include "srv0start.h"

/* This is used only during upgrade. We don't use ids
from DICT_HDR during upgrade because unlike bootstrap case,
the ids are moved after user table creation.  Since we
want to create dictionary tables with fixed ids, we use
in-memory counter for upgrade */
uint32_t	dd_upgrade_indexes_num = 1;

/** Vector of tables that have FTS indexes. Used for
reverting from 8.0 format FTS AUX table names to
5.7 FTS AUX table names */
static std::vector<std::string> tables_with_fts;

/** Fill foreign key information from InnoDB table to
server table
@param[in]	ib_table	InnoDB table object
@param[in,out]	dd_table	DD table object
@return false on success, otherwise true */
static bool dd_upgrade_table_fk(dict_table_t* ib_table, dd::Table* dd_table) {
  for (dict_foreign_set::iterator it = ib_table->foreign_set.begin();
       it != ib_table->foreign_set.end(); ++it) {
    dict_foreign_t* foreign = *it;

    /* Set the foreign_key name. */
    dd::Foreign_key* fk_obj = dd_table->add_foreign_key();

    /* Check if the foreign key name is valid */
    if (innobase_check_identifier_length(strchr(foreign->id,'/') + 1)) {
      ib::error() << "Foreign key name:" << foreign->id <<
        " is too long, for the table:" << dd_table->name() <<
        ". Please ALTER the foreign key name to use less"
        " than 64 characters and try upgrade again.\n";
      return true;
    }

    /* Ignore the schema name prefixed with the foreign_key name */
    if (strchr(foreign->id, '/'))
      fk_obj->set_name(strchr(foreign->id, '/')+1);
    else
      fk_obj->set_name(foreign->id);

    /* Don't set unique constraint name, it will be set by SQL-layer later. */

    /* Set match option. Unused for InnoDB */
    fk_obj->set_match_option(dd::Foreign_key::OPTION_NONE);

    /* Set Update rule */
    if (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE) {
      fk_obj->set_update_rule(dd::Foreign_key::RULE_CASCADE);
    } else if (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL) {
      fk_obj->set_update_rule(dd::Foreign_key::RULE_SET_NULL);
    } else if (foreign->type & DICT_FOREIGN_ON_UPDATE_NO_ACTION) {
      fk_obj->set_update_rule(dd::Foreign_key::RULE_NO_ACTION);
    } else {
      fk_obj->set_update_rule(dd::Foreign_key::RULE_RESTRICT);
    }

    /* Set delete rule */
    if (foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE) {
      fk_obj->set_delete_rule(dd::Foreign_key::RULE_CASCADE);
    } else if (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL) {
      fk_obj->set_delete_rule(dd::Foreign_key::RULE_SET_NULL);
    } else if (foreign->type & DICT_FOREIGN_ON_DELETE_NO_ACTION) {
      fk_obj->set_delete_rule(dd::Foreign_key::RULE_NO_ACTION);
    } else {
      fk_obj->set_delete_rule(dd::Foreign_key::RULE_RESTRICT);
    }

    /* Set catalog name */
    fk_obj->set_referenced_table_catalog_name("def");

    /* Set refernced table schema name */
    char db_buf[MAX_FULL_NAME_LEN + 1];
    char tbl_buf[MAX_FULL_NAME_LEN + 1];

    dd_parse_tbl_name(foreign->referenced_table_name, db_buf, tbl_buf,
		      nullptr, nullptr, nullptr);

    fk_obj->set_referenced_table_schema_name(db_buf);
    fk_obj->set_referenced_table_name(tbl_buf);

    /* Set referencing columns */
    for (uint32_t i = 0; i < foreign->n_fields; i++) {
      dd::Foreign_key_element* fk_col_obj = fk_obj->add_element();

      const char* foreign_col = foreign->foreign_col_names[i];
      ut_ad(foreign_col != NULL);
      const dd::Column* column = dd_table->get_column(
          dd::String_type(foreign_col, strlen(foreign_col)));
      ut_ad(column != NULL);
      fk_col_obj->set_column(column);

      const char* referenced_col = foreign->referenced_col_names[i];
      ut_ad(referenced_col != NULL);

      DBUG_EXECUTE_IF("dd_upgrade",
                      ib::info()
                          << "FK on table: " << ib_table->name
                          << " col: " << foreign_col << " references col: "
                          << " of table: " << foreign->referenced_table_name;);

      fk_col_obj->referenced_column_name(
          dd::String_type(referenced_col, strlen(referenced_col)));
    }

    DBUG_EXECUTE_IF(
        "dd_upgrade", ib::info() << "foreign name: " << foreign->id;
        ib::info() << " foreign fields: " << foreign->n_fields;
        ib::info() << " foreign type: " << foreign->type;
        ib::info() << " foreign table name: " << foreign->foreign_table_name;
        ib::info() << " referenced table name: "
                   << foreign->referenced_table_name;
        ib::info() << " foreign index: " << foreign->foreign_index->name;
        ib::info() << " foreign table: "
                   << foreign->foreign_index->table->name;);
  }

  return false;
}

/** Get Server Tablespace object for a InnoDB table. The tablespace is
acquired with MDL and for modification, so the caller can update the
dd::Tablespace object returned.
@param[in]	thd		server thread object
@param[in,out]	dd_client	dictionary client to retrieve tablespace
				object
@param[in]	ib_table	InnoDB table
@return dd::Tablespace object or nullptr */
static dd::Tablespace* dd_upgrade_get_tablespace(THD* thd,
                                     dd::cache::Dictionary_client* dd_client,
                                     dict_table_t* ib_table) {
  char name[MAX_FULL_NAME_LEN];

  dd::Tablespace*	ts_obj = nullptr;
  ut_ad(ib_table->space != SPACE_UNKNOWN);
  ut_ad(ib_table->space != SYSTEM_TABLE_SPACE);

  if (dict_table_is_file_per_table(ib_table)) {
    std::string tablespace_name;
    dd_filename_to_spacename(ib_table->name.m_name, &tablespace_name);
    strncpy(name, tablespace_name.c_str(), MAX_FULL_NAME_LEN);
  } else {
    ut_ad(DICT_TF_HAS_SHARED_SPACE(ib_table->flags));
    ut_ad(ib_table->tablespace != NULL);
    strncpy(name, ib_table->tablespace(), MAX_FULL_NAME_LEN);
  }

  DBUG_EXECUTE_IF("dd_upgrade",
                  ib::info() << "The derived tablespace name is: " << name;);

  /* MDL on tablespace name */
  if (dd::acquire_exclusive_tablespace_mdl(thd, name, false)) {
    ut_a(false);
  }

  /* For file per table tablespaces and general tablespaces, we will get
  the tablespace object and then get space_id. */
  if (dd_client->acquire_for_modification(name, &ts_obj)) {
    ut_a(false);
  }

  return(ts_obj);
}

/** Get field from Server table object
@param[in]	srv_table	server table object
@param[in]	name		Field name
@return field object if found or null on failure. */
static Field* dd_upgrade_get_field(const TABLE* srv_table, const char* name) {
  for (uint i = 0; i < srv_table->s->fields; i++) {
    Field* field = srv_table->field[i];
    if (strcmp(field->field_name, name) == 0) {
      return (field);
    }
  }
  return (nullptr);
}

/** Return true if table has primary key given by user else false
@param[in]	dd_table	Server table object
@retval		true		Table has PK given by user
@retval		false		Primary Key is hidden and generated */
static bool dd_has_explicit_pk(const dd::Table* dd_table) {
  return (!dd_table->indexes().front()->is_hidden());
}

/** Match InnoDB column object and Server column object
@param[in]	field	Server field object
@param[in]	col	InnoDB column object
@retval		false	column definition matches
@retval		true	column definition mismatch */
static bool dd_upgrade_match_single_col(const Field* field,
                                        const dict_col_t* col) {
  ulint unsigned_type;
  ulint col_type = get_innobase_type_from_mysql_type(&unsigned_type, field);

  bool failure = false;

  DBUG_EXECUTE_IF("dd_upgrade_strict_mode", ut_ad(col->mtype == col_type););

  if (col->mtype != col_type) {
    ib::error() << "Column datatype mismatch for col: " << field->field_name;
    failure = true;
  }

  ulint nulls_allowed = field->real_maybe_null() ? 0 : DATA_NOT_NULL;
  ulint binary_type = field->binary() ? DATA_BINARY_TYPE : 0;
  ulint charset_no = 0;

  if (dtype_is_string_type(col_type)) {
    charset_no = (ulint)field->charset()->number;

    if (charset_no > MAX_CHAR_COLL_NUM) {
      ib::error() << "In InnoDB, charset-collation codes"
                  << " must be below 256. Unsupported code " << charset_no;
      DBUG_EXECUTE_IF("dd_upgrade_strict_mode", bool invalid_collation = true;
                      ut_ad(!invalid_collation););

      failure = true;
    }
  }
  ulint col_len = field->pack_length();

  /* The MySQL pack length contains 1 or 2 bytes length field
  for a true VARCHAR. Let us subtract that, so that the InnoDB
  column length in the InnoDB data dictionary is the real
  maximum byte length of the actual data. */

  ulint long_true_varchar = 0;

  if (field->type() == MYSQL_TYPE_VARCHAR) {
    col_len -= ((Field_varstring*)field)->length_bytes;

    if (((Field_varstring*)field)->length_bytes == 2) {
      long_true_varchar = DATA_LONG_TRUE_VARCHAR;
    }
  }

  if (col_type == DATA_POINT) {
    col_len = DATA_POINT_LEN;
  }

  ulint is_virtual = (innobase_is_v_fld(field)) ? DATA_VIRTUAL : 0;

  ulint server_prtype =
      (static_cast<ulint>(field->type()) | nulls_allowed | unsigned_type |
       binary_type | long_true_varchar | is_virtual);

  /* First two bytes store charset, last two bytes store precision
  value. Get the last two bytes. i.e precision value */
  ulint innodb_prtype= (col->prtype & 0x0000FFFF);

  if (server_prtype != innodb_prtype) {
    ib::error() << "Column precision type mismatch(i.e NULLs, SIGNED/UNSIGNED "
                   "etc) for col: " << field->field_name;
    failure = true;
  }

  /* Numeric columns from 5.1 might have charset as my_charset_bin
  while 5.5+ will have charset as my_charset_latin1. Compare charsets
  only if field supports character set. */
  if (field->has_charset()) {
    ulint col_charset = col->prtype >> 16;
    if (charset_no != col_charset) {
      ib::error() << "Column character set mismatch for col: "
                  << field->field_name;
      failure = true;
    }
  }

  DBUG_EXECUTE_IF("dd_upgrade_strict_mode", ut_ad(col->len == col_len););

  if (col_len != col->len) {
    ib::error() << "Column length mismatch for col: " << field->field_name;
    failure = true;
  }

  return (failure);
}

/* Match defintion of all columns in InnoDB table and DD table
@param[in]	srv_table	Server table object
@param[in]	dd_table	New DD table object
@param[in]	ib_table	InnoDB table object
@retval		true		failure
@retval		false		success, all columns matched */
static bool dd_upgrade_match_cols(const TABLE* srv_table, const dd::Table* dd_table,
                                  const dict_table_t* ib_table) {
  uint32_t innodb_num_cols = ib_table->n_t_cols;
  bool has_explicit_pk = dd_has_explicit_pk(dd_table);
  if (has_explicit_pk) {
    /* Even when there is explicit PK, InnoDB registers DB_ROW_ID
    in list of columns. It is unused though. */
    innodb_num_cols = innodb_num_cols - 1 /* DB_ROW_ID */;
  }

  if (innodb_num_cols != dd_table->columns().size()) {
    ib::error() << "table: " << dd_table->name() << " has "
                << dd_table->columns().size()
                << " columns but InnoDB dictionary"
                << " has " << innodb_num_cols << " columns";
    DBUG_EXECUTE_IF("dd_upgrade_strict_mode", bool columns_num_mismatch = true;
                    ut_ad(!columns_num_mismatch););
    return (true);
  }

  /* Match columns */
  uint32_t idx = 0;
  uint32_t v_idx = 0;
  for (const dd::Column* col_obj : dd_table->columns()) {
    dict_col_t* ib_col = nullptr;
    const char* ib_col_name = nullptr;
    if (col_obj->is_virtual()) {
      dict_v_col_t* v_col = dict_table_get_nth_v_col(ib_table, v_idx);

      ib_col = &v_col->m_col;
      ib_col_name = dict_table_get_v_col_name(ib_table, v_idx);
      ++v_idx;
    } else {
      ib_col_name = ib_table->get_col_name(idx);
      if (has_explicit_pk && strcmp(ib_col_name, "DB_ROW_ID") == 0) {
        ++idx;
      }

      ib_col = ib_table->get_col(idx);
      ib_col_name = ib_table->get_col_name(idx);
      ++idx;
    }

    if (strcmp(ib_col_name, col_obj->name().c_str()) == 0) {
      /* Skip matching hidden fields like DB_ROW_ID, DB_TRX_ID because
      these don't exist in TABLE* object of server. */
      if (!col_obj->is_hidden()) {
        /* Match col object and field */
        Field* field = dd_upgrade_get_field(srv_table, ib_col_name);
        ut_ad(field != NULL);
        ut_ad(ib_col != NULL);
        bool failure = dd_upgrade_match_single_col(field, ib_col);
        if (failure) {
          ib::error() << "Column " << col_obj->name()
                      << " for table: " << ib_table->name
                      << " mismatches with InnoDB Dictionary";
          DBUG_EXECUTE_IF("dd_upgrade_strict_mode", bool column_mismatch = true;
                          ut_ad(!column_mismatch););
          return (true);
	}
      }
    } else {
      ib::error() << "Column name mismatch: From InnoDB: " << ib_col_name
                  << " From Server: " << col_obj->name();
      DBUG_EXECUTE_IF("dd_upgrade_strict_mode",
                      bool column_name_mismatch = true;
                      ut_ad(!column_name_mismatch););
      return (true);
    }
  }

#ifdef UNIV_DEBUG
  uint32_t processed_columns_num = idx + v_idx;
  if (has_explicit_pk) {
    processed_columns_num -= 1;
  }
  ut_ad(processed_columns_num == dd_table->columns().size());
#endif /* UNIV_DEBUG */

  return (false);
}

/** Find key number from a server table object
@param[in]	srv_table	server table object
@param[in]	name		index name
@retval	UINT32_MAX if index not found, else key number */
static uint32_t dd_upgrade_find_index(TABLE* srv_table, const char* name) {
  for (uint32_t i = 0; i < srv_table->s->keys; i++) {
    KEY* key = srv_table->key_info + i;
    if (strcmp(key->name, name) == 0) {
      return (i);
    }
  }
  return (UINT32_MAX);
}

/** Match InnoDB index definition from Server object
@param[in]	srv_table	Server table object
@param[in]	index		InnoDB index
@retval		false		Index definition matches
@retval		true		Index definition mismatch */
static bool dd_upgrade_match_index(TABLE* srv_table, dict_index_t* index) {
  uint32_t key_no = dd_upgrade_find_index(srv_table, index->name);

  if (key_no == UINT32_MAX) {
    ib::info() << "Index: " << index->name << " exists"
               << " in InnoDB but not in Server";
    DBUG_EXECUTE_IF("dd_upgrade_strict_mode", bool index_not_found = true;
                    ut_ad(!index_not_found););
    return (true);
  }

  KEY* key = srv_table->key_info + key_no;

  ut_ad(key != nullptr);

  DBUG_EXECUTE_IF("dd_upgrade_strict_mode", ut_ad(key->user_defined_key_parts ==
                                                  index->n_user_defined_cols););

  if (key->user_defined_key_parts != index->n_user_defined_cols) {
    ib::error() << "The number of fields in index " << index->name
                << " according to Server: " << key->user_defined_key_parts
                << " according to InnoDB: " << index->n_user_defined_cols;
    return (true);
  }

  ulint ind_type = 0;
  if (key_no == srv_table->s->primary_key) {
    ind_type |= DICT_CLUSTERED;
  }

  if (key->flags & HA_NOSAME) {
    ind_type |= DICT_UNIQUE;
  }

  if (key->flags & HA_SPATIAL) {
    ind_type |= DICT_SPATIAL;
  }

  if (key->flags & HA_FULLTEXT) {
    ind_type |= DICT_FTS;
  }

  ulint nulls_equal = (key->flags & HA_NULL_ARE_EQUAL) ? true : false;

  DBUG_EXECUTE_IF("dd_upgrade_strict_mode",
                  ut_ad(nulls_equal == index->nulls_equal););

  if (nulls_equal != index->nulls_equal) {
    ib::error() << "In index: " << index->name
                << " NULL equal from Server: " << nulls_equal
                << " From InnoDB: " << index->nulls_equal;
    return (true);
  }

  for (ulint i = 0; i < key->user_defined_key_parts; i++) {
    KEY_PART_INFO* key_part = key->key_part + i;

    Field* field = srv_table->field[key_part->field->field_index];
    if (field == NULL) ut_error;

    const char* field_name = key_part->field->field_name;
    dict_field_t* idx_field = index->get_field(i);

    DBUG_EXECUTE_IF("dd_upgrade_strict_mode",
                    ut_ad(strcmp(field_name, idx_field->name()) == 0););

    if (strcmp(field_name, idx_field->name()) != 0) {
      ib::error() << "In index: " << index->name
                  << " field name mismatches: from server: " << field_name
                  << " from InnoDB: " << idx_field->name();
      return (true);
    }

    ulint is_unsigned;
    ulint col_type =
        get_innobase_type_from_mysql_type(&is_unsigned, key_part->field);
    ulint prefix_len;

    if (DATA_LARGE_MTYPE(col_type) ||
        (key_part->length < field->pack_length() &&
         field->type() != MYSQL_TYPE_VARCHAR) ||
        (field->type() == MYSQL_TYPE_VARCHAR &&
         key_part->length <
             field->pack_length() - ((Field_varstring*)field)->length_bytes)) {
      switch (col_type) {
        default:
          prefix_len = key_part->length;
          break;
        case DATA_INT:
        case DATA_FLOAT:
        case DATA_DOUBLE:
        case DATA_DECIMAL:
          prefix_len = 0;
      }
    } else {
      prefix_len = 0;
    }

    if (!(index->type & (DICT_FTS | DICT_SPATIAL))) {
      if (prefix_len != index->get_field(i)->prefix_len) {
        ib::error() << "In Index: " << index->name
                    << " prefix_len mismatches: from server: " << prefix_len
                    << " from InnoDB: " << index->get_field(i)->prefix_len;
        DBUG_EXECUTE_IF("dd_upgrade_strict_mode",
                        ut_ad(prefix_len == index->get_field(i)->prefix_len););
        return (true);
      }
    }

    if (innobase_is_v_fld(key_part->field)) {
      ind_type |= DICT_VIRTUAL;
    }
  }

  DBUG_EXECUTE_IF("dd_upgrade_srict_mode", ut_ad(index->type == ind_type););

  if (index->type != ind_type) {
    ib::error() << "Index name: " << index->name
                << " type mismatches: from server: " << ind_type
                << " from InnoDB: " << index->type;
    return (true);
  }

  return (false);
}

/* Check if the table has auto inc field
@param[in]	srv_tabl		server table object
@param[in,out]	auto_inc_index_name	Index name on which auto inc exists
@param[in,out]	auto_inc_col_name	Column name of the auto inc field
@retval		true			if auto inc field exists
@retval		false			if auot inc field doesn't exist */
static bool dd_upgrade_check_for_autoinc(TABLE* srv_table,
                                         const char*& auto_inc_index_name,
                                         const char*& auto_inc_col_name) {
  if (srv_table->s->found_next_number_field) {
    const Field* field = *srv_table->s->found_next_number_field;
    KEY* key;
    key = srv_table->s->key_info + srv_table->s->next_number_index;

    auto_inc_index_name = key->name;
    auto_inc_col_name = field->field_name;

    DBUG_EXECUTE_IF("dd_upgrade",
                    ib::info() << "Index with auto_increment " << key->name;);
    if (auto_inc_index_name == nullptr || auto_inc_col_name == nullptr) {
      return (false);
    } else {
      return (true);
    }
  } else {
    auto_inc_index_name = nullptr;
    auto_inc_col_name = nullptr;
    return (false);
  }
}

/** Set auto-inc field value in dd::Table object
@param[in]	srv_table	server table object
@param[in,out]	dd_table	dd table object to be filled
@param[in,out]	auto_inc_value	auto_inc value */
static void dd_upgrade_set_auto_inc(const TABLE* srv_table, dd::Table* dd_table,
                                    uint64_t auto_inc_value) {
  ulonglong col_max_value;
  const Field* field = *srv_table->s->found_next_number_field;

  col_max_value = field->get_max_int_value();

  /* At the this stage we do not know the increment
  nor the offset, so use a default increment of 1. */

  auto_inc_value =
      innobase_next_autoinc(auto_inc_value, 1, 1, 0, col_max_value);

  dd::Properties& table_properties = dd_table->se_private_data();
  dd_set_autoinc(table_properties, auto_inc_value);
}

/* Set DD Index se_private_data and also read auto_inc if it the index
matches with auto_inc index name
@param[in,out]	dd_index		dd::Index object
@param[in]	index			InnoDB index object
@param[in]	dd_space_id		Server space id for index
					(not InnoDB space id)
@param[in]	has_auto_inc		true if table has auto inc field
@param[in]	auto_inc_index_name	Index name on which auto_inc exists
@param[in]	auto_inc_col_name	col name on which auto_inc exists
@param[in,out]	read_auto_inc		auto inc value read */
template <typename Index>
static void dd_upgrade_process_index(Index dd_index, dict_index_t* index,
				    dd::Object_id dd_space_id,
                                    bool has_auto_inc,
                                    const char* auto_inc_index_name,
                                    const char* auto_inc_col_name,
                                    uint64_t* read_auto_inc) {
  dd_index->set_tablespace_id(dd_space_id);
  dd::Properties& p = dd_index->se_private_data();

  p.set_uint32(dd_index_key_strings[DD_INDEX_ROOT], index->page);
  p.set_uint64(dd_index_key_strings[DD_INDEX_SPACE_ID], index->space);
  p.set_uint64(dd_index_key_strings[DD_INDEX_ID], index->id);
  p.set_uint32(dd_index_key_strings[DD_TABLE_ID], index->table->id);
  p.set_uint64(dd_index_key_strings[DD_INDEX_TRX_ID], 0);

  if (has_auto_inc) {
    ut_ad(auto_inc_index_name != nullptr);
    ut_ad(auto_inc_col_name != nullptr);
    if (strcmp(index->name(), auto_inc_index_name) == 0) {
      dberr_t err =
          row_search_max_autoinc(index, auto_inc_col_name, read_auto_inc);
          if (err != DB_SUCCESS) {
            ut_ad(0);
          }
    }
  }
}

/** Migrate partitions to new dictionary
@param[in]	thd		Server thread object
@param[in]	norm_name	partition table name
@param[in,out]	dd_table	Server new DD table object to be filled
@param[in]	srv_table	Server table object
@return false on success, true on error */
static bool dd_upgrade_partitions(THD* thd, const char* norm_name,
                                  dd::Table* dd_table, TABLE* srv_table) {
  char partition_name[FN_REFLEN];
  size_t table_name_len;
  char* partition_name_start;

  strcpy(partition_name, norm_name);
  table_name_len = strlen(norm_name);
  partition_name_start = partition_name + table_name_len;

  /* Check for auto inc */
  const char* auto_inc_index_name = NULL;
  const char* auto_inc_col_name = NULL;

  bool has_auto_inc = dd_upgrade_check_for_autoinc(
      srv_table, auto_inc_index_name, auto_inc_col_name);

  uint64_t max_auto_inc = 0;

  for (dd::Partition* part_obj : *dd_table->leaf_partitions()) {

    size_t len = Ha_innopart_share::create_partition_postfix(
        partition_name_start, FN_REFLEN - table_name_len, part_obj);

    if (table_name_len + len >= FN_REFLEN) {
      ut_ad(0);
    }

    dict_table_t* part_table = dict_table_open_on_name(
        partition_name, FALSE, TRUE, DICT_ERR_IGNORE_NONE);
    dict_table_close(part_table, false, false);

    DBUG_EXECUTE_IF("dd_upgrade",
                    ib::info()
                        << "Part table name from server: " << partition_name
                        << " from InnoDB: " << part_table->name.m_name;);

    /* Set table id */
    part_obj->set_se_private_id(part_table->id);

    /* Set DATA_DIRECTORY attribute in se_private_data */
    if (DICT_TF_HAS_DATA_DIR(part_table->flags)) {
      ut_ad(dict_table_is_file_per_table(part_table));
      part_obj->se_private_data().set_bool(
          dd_table_key_strings[DD_TABLE_DATA_DIRECTORY], true);
    }

    /* Set Discarded attribute in DD table se_private_data */
    if (dict_table_is_discarded(part_table)) {
      part_obj->se_private_data().set_bool(
          dd_table_key_strings[DD_TABLE_DISCARD], true);
    }

    dd::Object_id dd_space_id;

    if (part_table->space == SYSTEM_TABLE_SPACE) {
      dd_space_id = dict_sys_t::s_dd_sys_space_id;
      /* Tables in system tablespace cannot be discarded. */
      ut_ad(!dict_table_is_discarded(part_table));
    } else {
      dd::cache::Dictionary_client* dd_client = dd::get_dd_client(thd);
      dd::cache::Dictionary_client::Auto_releaser releaser(dd_client);
      dd::Tablespace* dd_space =
          dd_upgrade_get_tablespace(thd, dd_client, part_table);
      ut_ad(dd_space != nullptr);

      if (dd_space == nullptr) {
        dict_table_close(part_table, false, false);
        return (true);
      }

      dd_space_id = dd_space->id();
      /* If table is discarded, set discarded attribute in tablespace
      object */
      if (dict_table_is_discarded(part_table)) {
	dd_tablespace_set_discard(dd_space, true);
        if (dd_client->update(dd_space)) {
          ut_ad(0);
        }
      }
    }

    dd_set_table_options(part_obj, part_table);

    uint32_t processed_indexes_num = 0;
    for (dd::Partition_index* part_index : *part_obj->indexes()) {

      DBUG_EXECUTE_IF("dd_upgrade",
                      ib::info()
                          << "Partition Index " << part_index->name()
                          << " from server for table: " << part_table->name;);

      for (dict_index_t* index = UT_LIST_GET_FIRST(part_table->indexes);
           index != NULL; index = UT_LIST_GET_NEXT(indexes, index)) {

        if (strcmp(part_index->name().c_str(), index->name()) == 0) {

	  uint64_t read_auto_inc = 0;
	  dd_upgrade_process_index(part_index, index, dd_space_id, has_auto_inc,
				   auto_inc_index_name,
				   auto_inc_col_name,
				   &read_auto_inc);
	  ++processed_indexes_num;
	  if (has_auto_inc) {
            set_if_bigger(max_auto_inc, read_auto_inc);
	  }
          break;
        }
      }
    }

    if (processed_indexes_num != part_obj->indexes()->size()) {
      ib::error() << "Num of Indexes in InnoDB Partition doesn't match"
                  << " with Indexes from server";
      ib::error() << "Indexes from InnoDB: " << processed_indexes_num
                  << " Indexes from Server: " << dd_table->indexes()->size();
      return (true);
    }
  }

  /* Set auto increment properties */
  if (has_auto_inc) {
    dd_upgrade_set_auto_inc(srv_table, dd_table, max_auto_inc);
  }

  return (false);
}

/* Set the ROW_FORMAT in dd_table based on InnoDB dictionary table
@param[in]	ib_table	InnoDB table
@param[in,out]	dd_table	Server table object */
static void dd_upgrade_set_row_type(dict_table_t* ib_table,
                                    dd::Table* dd_table) {
  if (ib_table) {
    const ulint flags = ib_table->flags;

    switch (dict_tf_get_rec_format(flags)) {
      case REC_FORMAT_REDUNDANT:
        dd_table->set_row_format(dd::Table::RF_REDUNDANT);
        break;
      case REC_FORMAT_COMPACT:
        dd_table->set_row_format(dd::Table::RF_COMPACT);
        break;
      case REC_FORMAT_COMPRESSED:
        dd_table->set_row_format(dd::Table::RF_COMPRESSED);
        break;
      case REC_FORMAT_DYNAMIC:
        dd_table->set_row_format(dd::Table::RF_DYNAMIC);
        break;
      default:
        ut_ad(0);
    }
  }
}

/** Migrate table from InnoDB Dictionary (INNODB SYS_*) tables to new Data
Dictionary. Since FTS tables contain table_id in their physical file name
and during upgrade we reserve DICT_MAX_DD_TABLES for dictionary tables.
So we rename FTS tablespace files
@param[in]	thd		Server thread object
@param[in]	db_name		database name
@param[in]	table_name	table name
@param[in,out]	dd_table	new dictionary table object to be filled
@param[in]	srv_table	server table object
@return false on success, true on failure. */
bool dd_upgrade_table(THD* thd, const char* db_name, const char* table_name,
                      dd::Table* dd_table, TABLE* srv_table) {

  char norm_name[FN_REFLEN];
  dict_table_t* ib_table = NULL;
  /* 2 * NAME_CHAR_LEN is for dbname and tablename, 5 assumes max bytes
  for charset, + 2 is for path separator and +1 is for NULL. */
  char buf[2 * NAME_CHAR_LEN * 5 + 2 + 1];
  bool truncated;

  build_table_filename(buf, sizeof(buf), db_name, table_name, NULL, 0,
                       &truncated);
  ut_ad(!truncated);

  normalize_table_name(norm_name, buf);

  bool is_part = dd_table->leaf_partitions()->size() != 0;

  if (is_part) {
    return (dd_upgrade_partitions(thd, norm_name, dd_table, srv_table));
  }

  ib_table =
      dict_table_open_on_name(norm_name, FALSE, TRUE, DICT_ERR_IGNORE_NONE);

  if (ib_table == NULL) {
    ib::error() << "Table " << norm_name
                << " is not found in InnoDB dictionary";
    return (true);
  }

  bool failure = dd_upgrade_match_cols(srv_table, dd_table, ib_table);

  if (failure) {
    dict_table_close(ib_table, false, false);
    return (failure);
  }

  dd::Object_id dd_space_id;
  if (ib_table->space == SYSTEM_TABLE_SPACE) {
    dd_space_id = dict_sys_t::s_dd_sys_space_id;
    /* Tables in system tablespace cannot be discarded. */
    ut_ad(!dict_table_is_discarded(ib_table));
  } else {
    dd::cache::Dictionary_client* dd_client = dd::get_dd_client(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(dd_client);
    dd::Tablespace* dd_space =
        dd_upgrade_get_tablespace(thd, dd_client, ib_table);
    ut_ad(dd_space != nullptr);

    if (dd_space == nullptr) {
      dict_table_close(ib_table, false, false);
      return (true);
    }

    dd_space_id = dd_space->id();
    /* If table is discarded, set discarded attribute in tablespace
    object */
    if (dict_table_is_discarded(ib_table)) {
      dd_tablespace_set_discard(dd_space, true);
      if (dd_client->update(dd_space)) {
        ut_ad(0);
      }
    }
  }

  dd_table->set_se_private_id(ib_table->id);

  /* Set DATA_DIRECTORY attribute in se_private_data */
  if (DICT_TF_HAS_DATA_DIR(ib_table->flags)) {
    ut_ad(dict_table_is_file_per_table(ib_table));
    dd_table->se_private_data().set_bool(
        dd_table_key_strings[DD_TABLE_DATA_DIRECTORY], true);
  }

  /* Set Discarded attribute in DD table se_private_data */
  if (dict_table_is_discarded(ib_table)) {
    dd_table->se_private_data().set_bool(
        dd_table_key_strings[DD_TABLE_DISCARD], true);
  }

  /* Set row_type */
  dd_upgrade_set_row_type(ib_table, dd_table);

  /* Check for auto inc */
  const char* auto_inc_index_name = nullptr;
  const char* auto_inc_col_name = nullptr;

  bool has_auto_inc = dd_upgrade_check_for_autoinc(
      srv_table, auto_inc_index_name, auto_inc_col_name);

  uint64_t auto_inc = UINT64_MAX;

  dd_set_table_options(dd_table, ib_table);

  /* The number of indexes has to match. */
  DBUG_EXECUTE_IF("dd_upgrade_strict_mode",
    ut_ad(dd_table->indexes()->size() == UT_LIST_GET_LEN(ib_table->indexes));
  );

  if (UT_LIST_GET_LEN(ib_table->indexes) != dd_table->indexes()->size()) {
    ib::error() << "Num of Indexes in InnoDB doesn't match"
                << " with Indexes from server";
    ib::error() << "Indexes from InnoDB: " << UT_LIST_GET_LEN(ib_table->indexes)
                << " Indexes from Server: " << dd_table->indexes()->size();
    dict_table_close(ib_table, false, false);
    return (true);
  }

  uint32_t processed_indexes_num = 0;
  for (dd::Index* dd_index : *dd_table->indexes()) {

    DBUG_EXECUTE_IF("dd_upgrade",
                    ib::info()
                        << "Index " << dd_index->name()
                        << " from server for table: " << ib_table->name;);

    for (dict_index_t* index = UT_LIST_GET_FIRST(ib_table->indexes);
         index != NULL; index = UT_LIST_GET_NEXT(indexes, index)) {

      if (strcmp(dd_index->name().c_str(), index->name()) == 0) {

        if (!dd_index->is_hidden()) {
          failure = dd_upgrade_match_index(srv_table, index);
        }

        dd_upgrade_process_index(dd_index, index, dd_space_id, has_auto_inc,
                                 auto_inc_index_name, auto_inc_col_name,
                                 &auto_inc);
        ++processed_indexes_num;
        break;
      }
    }
  }

  if (processed_indexes_num != dd_table->indexes()->size()) {
    ib::error() << "Num of Indexes in InnoDB doesn't match"
                << " with Indexes from server";
    ib::error() << "Indexes from InnoDB: " << processed_indexes_num
                << " Indexes from Server: " << dd_table->indexes()->size();
    dict_table_close(ib_table, false, false);
    return (true);
  }

  /* Set auto increment properties */
  if (has_auto_inc) {
    ut_ad(auto_inc != UINT64_MAX);
    dd_upgrade_set_auto_inc(srv_table, dd_table, auto_inc);
    ib_table->autoinc = auto_inc;
  }

  if (dict_table_has_fts_index(ib_table)) {
    dberr_t err = fts_upgrade_aux_tables(ib_table);

    if (err != DB_SUCCESS) {
      dict_table_close(ib_table, false, false);
      return (true);
    } else {

      mutex_enter(&dict_sys->mutex);
      dict_table_prevent_eviction(ib_table);
      mutex_exit(&dict_sys->mutex);

      tables_with_fts.push_back(ib_table->name.m_name);
    }
  }

  failure= failure || dd_upgrade_table_fk(ib_table, dd_table);

  dict_table_close(ib_table, false, false);
  return (failure);
}

/** Tablespace information required to create a
dd::Tablespace object */
typedef struct {
  /** InnoDB space id */
  space_id_t id;
  /** Tablespace name */
  const char* name;
  /** Tablespace flags */
  ulint flags;
  /** Path of the tablespace file */
  const char* path;
} upgrade_space_t;

/** Register InnoDB tablespace to mysql
dictionary table mysql.tablespaces
@param[in]	dd_client	dictionary client
@param[in]	dd_space	server tablespace object
@param[in]	upgrade_space	upgrade tablespace object
@return 0 on success, non-zero on error */
static uint32_t dd_upgrade_register_tablespace(
    dd::cache::Dictionary_client* dd_client, dd::Tablespace* dd_space,
    upgrade_space_t* upgrade_space) {
  dd_space->set_engine(innobase_hton_name);
  dd_space->set_name(upgrade_space->name);

  dd::Properties& p = dd_space->se_private_data();
  p.set_uint32(dd_space_key_strings[DD_SPACE_ID],
               static_cast<uint32>(upgrade_space->id));
  p.set_uint32(dd_space_key_strings[DD_SPACE_FLAGS],
               static_cast<uint32>(upgrade_space->flags));
  p.set_uint32(dd_space_key_strings[DD_SPACE_SERVER_VERSION],
	       DD_SPACE_CURRENT_SRV_VERSION);
  p.set_uint32(dd_space_key_strings[DD_SPACE_VERSION],
	       DD_SPACE_CURRENT_SPACE_VERSION);
  dd::Tablespace_file* dd_file = dd_space->add_file();

  dd_file->set_filename(upgrade_space->path);

  if (dd_client->store(dd_space)) {
    /* It would be better to return thd->get_stmt_da()->mysql_errno(),
    however, server doesn't fill in the errno during bootstrap. */
    return (HA_ERR_GENERIC);
  }

  return (0);
}

/** Migrate tablespace entries from InnoDB SYS_TABLESPACES to new data
dictionary. FTS Tablespaces are not registered as they are handled differently.
FTS tablespaces have table_id in their name and we increment table_id of each
table by DICT_MAX_DD_TABLES.
@param[in,out]  thd             THD
@return MySQL error code*/
int dd_upgrade_tablespace(THD* thd) {

  DBUG_ENTER("innobase_migrate_tablespace");
  btr_pcur_t pcur;
  const rec_t* rec;
  mem_heap_t* heap;
  mtr_t mtr;

  heap = mem_heap_create(1000);
  dd::cache::Dictionary_client* dd_client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(dd_client);
  mutex_enter(&dict_sys->mutex);
  mtr_start(&mtr);

  for (rec = dict_startscan_system(&pcur, &mtr, SYS_TABLESPACES); rec != NULL;
       rec = dict_getnext_system(&pcur, &mtr)) {
    const char* err_msg;
    space_id_t space;
    const char* name;
    ulint flags;
    std::string new_tablespace_name;

    /* Extract necessary information from a SYS_TABLESPACES row */
    err_msg = dict_process_sys_tablespaces(heap, rec, &space, &name, &flags);

    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    std::string tablespace_name(name);

    if (!err_msg && (tablespace_name.find("FTS") == std::string::npos)) {
      // Fill the dictionary object here
      DBUG_EXECUTE_IF(
          "dd_upgrade",
          ib::info() << "Creating dictionary entry for tablespace: " << name;);

      std::unique_ptr<dd::Tablespace> dd_space(
          dd::create_object<dd::Tablespace>());

      upgrade_space_t upgrade_space;
      upgrade_space.id = space;
      upgrade_space.flags = flags;

      bool is_file_per_table = !fsp_is_system_or_temp_tablespace(space) &&
                               !fsp_is_shared_tablespace(flags);

      std::string file_per_name;
      if (is_file_per_table) {
	std::string orig_tablespace_name(tablespace_name);
        if ((tablespace_name.compare("mysql/innodb_table_stats") == 0) ||
          (tablespace_name.find("mysql/innodb_index_stats") == 0)) {
	    orig_tablespace_name.append("_backup57");
	}
	dd_filename_to_spacename(orig_tablespace_name.c_str(),
				 &new_tablespace_name);
        upgrade_space.name = new_tablespace_name.c_str();
      } else {
        upgrade_space.name = name;
      }

      mutex_enter(&dict_sys->mutex);
      char* filename = dict_get_first_path(space);
      mutex_exit(&dict_sys->mutex);

      std::string orig_name(filename);
      ut_free(filename);
      /* To migrate statistics from 57 satistics tables, we rename the
      5.7 statistics tables/tablespaces so that it doesn't conflict
      with statistics table names in 8.0 */
      if ((tablespace_name.compare("mysql/innodb_table_stats") == 0) ||
          (tablespace_name.find("mysql/innodb_index_stats") == 0)) {
        orig_name.erase(orig_name.end() - 4, orig_name.end());
        orig_name.append("_backup57.ibd");
      }

      ut_ad(filename != NULL);
      upgrade_space.path = orig_name.c_str();

      if (dd_upgrade_register_tablespace(dd_client, dd_space.get(),
                                         &upgrade_space)) {
        mem_heap_free(heap);
        DBUG_RETURN(HA_ERR_GENERIC);
      }

    } else {
      if (err_msg != nullptr) {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_CANT_FIND_SYSTEM_REC, "%s", err_msg);
      }
    }

    mem_heap_empty(heap);

    /* Get the next record */
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
  }

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);

  /* These are file_per_table tablespaces(created using 5.5 or
  earlier). These are not found in SYS_TABLESPACES but discovered
  from SYS_TABLES */
  for (auto space : missing_spaces) {

    std::string tablespace_name(space->name);
    /* FTS tablespaces will be registered later */
    if (tablespace_name.find("FTS") != std::string::npos) {
      continue;
    }
    std::string new_tablespace_name;
    std::unique_ptr<dd::Tablespace> dd_space(
        dd::create_object<dd::Tablespace>());

    upgrade_space_t upgrade_space;
    upgrade_space.id = space->id;
    upgrade_space.flags = space->flags;
    dd_space->set_engine(innobase_hton_name);
    dd_filename_to_spacename(tablespace_name.c_str(),
			     &new_tablespace_name);
    upgrade_space.name = new_tablespace_name.c_str();

    Datafile df;

    df.init(space->name, space->flags);
    df.make_filepath(nullptr, space->name, IBD);

    upgrade_space.path = df.filepath();

    if (dd_upgrade_register_tablespace(dd_client, dd_space.get(),
                                       &upgrade_space)) {
      mem_heap_free(heap);
      DBUG_RETURN(HA_ERR_GENERIC);
    }
  }

  mem_heap_free(heap);

  DBUG_RETURN(0);
}

/** Add server version number to tablespace while upgrading.
@param[in]      space_id              space id of tablespace
@return false on success, true on failure. */
bool upgrade_space_version(const uint32 space_id)
{
  buf_block_t*    block;
  page_t*         page;
  mtr_t           mtr;

  fil_space_t*    space = fil_space_acquire(space_id);

  if (space == nullptr) {
    return(true);
  }

  const page_size_t       page_size(space->flags);

  mtr_start(&mtr);

  block = buf_page_get(page_id_t(space_id, 0),
                             page_size,
                             RW_SX_LATCH, &mtr);

  page = buf_block_get_frame(block);


  mlog_write_ulint(page + FIL_PAGE_SRV_VERSION,
                   DD_SPACE_CURRENT_SRV_VERSION,
                   MLOG_4BYTES,
                   &mtr);

  mlog_write_ulint(page + FIL_PAGE_SPACE_VERSION,
                   DD_SPACE_CURRENT_SPACE_VERSION,
                   MLOG_4BYTES,
                   &mtr);

  mtr_commit(&mtr);
  fil_space_release(space);
  return(false);
}

/** Add server version number to tablespace while upgrading.
@param[in]      tablespace              dd::Tablespace
@return false on success, true on failure. */
bool upgrade_space_version(dd::Tablespace* tablespace)
{
  uint32  space_id;

  if (tablespace->se_private_data().get_uint32("id", &space_id)) {
    /* error, attribute not found */
    ut_ad(0);
    return(true);
  }
  return(upgrade_space_version(space_id));
}


/** Upgrade innodb undo logs after upgrade. Also increment the table_id
offset by DICT_MAX_DD_TABLES. This offset increment is because the
first 256 table_ids are reserved for dictionary.
@param[in,out]  thd             THD
@return MySQL error code*/
int dd_upgrade_logs(THD* thd) {
  int error = 0; /* return zero for success */
  DBUG_ENTER("innobase_upgrade_engine_logs");

  mtr_t mtr;
  mtr.start();
  dict_hdr_t* dict_hdr = dict_hdr_get(&mtr);
  table_id_t table_id = mach_read_from_8(dict_hdr + DICT_HDR_TABLE_ID);

  DBUG_EXECUTE_IF("dd_upgrade",
                  ib::info() << "Incrementing table_id from: " << table_id
                             << " to " << table_id + DICT_MAX_DD_TABLES;);

  /* Increase the offset of table_id by DICT_MAX_DD_TABLES */
  mlog_write_ull(dict_hdr + DICT_HDR_TABLE_ID, table_id + DICT_MAX_DD_TABLES,
                 &mtr);
  mtr.commit();

  log_buffer_flush_to_disk();

  DBUG_RETURN(error);
}

/** Drop all InnoDB Dictionary tables (SYS_*). This is done only at
the end of successful upgrade */
static
void dd_upgrade_drop_sys_tables() {
  ut_ad(srv_is_upgrade_mode);

  mutex_enter(&dict_sys->mutex);

  bool found;
  const page_size_t page_size(
      fil_space_get_page_size(SYSTEM_TABLE_SPACE, &found));
  ut_ad(found);
  ut_ad(page_size.equals_to(univ_page_size));

  for (uint32_t i = 0; i < SYS_NUM_SYSTEM_TABLES; i++) {
    dict_table_t* system_table = dict_table_get_low(SYSTEM_TABLE_NAME[i]);
    ut_ad(system_table != nullptr);
    ut_ad(system_table->space == SYSTEM_TABLE_SPACE);

    for (dict_index_t* index = system_table->first_index(); index != nullptr;
         index = index->next()) {
      ut_ad(index->space == system_table->space);

      const page_id_t root(index->space, index->page);

      mtr_t mtr;
      mtr_start(&mtr);

      btr_free_if_exists(root, page_size, index->id, &mtr);

      mtr_commit(&mtr);
    }
    dict_table_remove_from_cache(system_table);
  }

  dict_sys->sys_tables = nullptr;
  dict_sys->sys_columns = nullptr;
  dict_sys->sys_indexes = nullptr;
  dict_sys->sys_fields = nullptr;
  dict_sys->sys_virtual = nullptr;

  mutex_exit(&dict_sys->mutex);
}

/** Rename back the FTS AUX tablespace names from 8.0 format to 5.7
format on upgrade failure, else mark FTS aux tables evictable
@param[in]	failed_upgrade		true on upgrade failure, else
                                        false */
static void dd_upgrade_fts_rename_cleanup(bool failed_upgrade) {
  for (std::string& name : tables_with_fts) {
    dict_table_t* ib_table = dict_table_open_on_name(name.c_str(), FALSE, TRUE,
                                                     DICT_ERR_IGNORE_NONE);
    ut_ad(ib_table != nullptr);
    if (ib_table != nullptr) {
      fts_upgrade_rename(ib_table, failed_upgrade);

      mutex_enter(&dict_sys->mutex);
      dict_table_allow_eviction(ib_table);
      dict_table_close(ib_table, true, false);
      mutex_exit(&dict_sys->mutex);

    }
  }
}

/** If upgrade is successful, this API is used to flush innodb
dirty pages to disk. In case of server crash, this function
sets storage engine for rollback any changes.
@param[in,out]  thd             THD
@param[in]	failed_upgrade	true when upgrade failed
@return MySQL error code*/
int dd_upgrade_finish(THD* thd, bool failed_upgrade) {
  DBUG_ENTER("innobase_finish_se_upgrade");

  dd_upgrade_fts_rename_cleanup(failed_upgrade);

  if (failed_upgrade) {
    srv_downgrade_logs = true;
  } else {
    /* Delete the old undo tablespaces and the references to them
    in the TRX_SYS page. */
    srv_undo_tablespaces_upgrade();

    /* Drop InnoDB Dictionary tables (SYS_*) */
    dd_upgrade_drop_sys_tables();

    /* Flush entire buffer pool. */
    buf_flush_sync_all_buf_pools();
  }

  tables_with_fts.clear();
  tables_with_fts.shrink_to_fit();
  srv_is_upgrade_mode = false;

  DBUG_RETURN(0);
}
