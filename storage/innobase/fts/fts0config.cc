/*****************************************************************************

Copyright (c) 2007, 2024, Oracle and/or its affiliates.

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

/** @file fts/fts0config.cc
 Full Text Search configuration table.

 Created 2007/5/9 Sunny Bains
 ***********************************************************************/

#include <stdlib.h>

#include "fts0priv.h"

#include "row0sel.h"
#include "trx0roll.h"

/** Callback function for fetching the config value.
 @return always returns true */
static bool fts_config_fetch_value(void *row,      /*!< in: sel_node_t* */
                                   void *user_arg) /*!< in: pointer to
                                                    ib_vector_t */
{
  sel_node_t *node = static_cast<sel_node_t *>(row);
  fts_string_t *value = static_cast<fts_string_t *>(user_arg);

  dfield_t *dfield = que_node_get_val(node->select_list);
  dtype_t *type = dfield_get_type(dfield);
  ulint len = dfield_get_len(dfield);
  void *data = dfield_get_data(dfield);

  ut_a(dtype_get_mtype(type) == DATA_VARCHAR);

  if (len != UNIV_SQL_NULL) {
    ulint max_len = std::min(value->f_len - 1, len);

    memcpy(value->f_str, data, max_len);
    value->f_len = max_len;
    value->f_str[value->f_len] = '\0';
  }

  return true;
}

/** Get value from the config table. The caller must ensure that enough
 space is allocated for value to hold the column contents.
 @return DB_SUCCESS or error code */
dberr_t fts_config_get_value(trx_t *trx,             /*!< transaction */
                             fts_table_t *fts_table, /*!< in: the indexed
                                                     FTS table */
                             const char *name,    /*!< in: get config value for
                                                  this parameter name */
                             fts_string_t *value) /*!< out: value read from
                                                  config table */
{
  pars_info_t *info;
  que_t *graph;
  dberr_t error;
  ulint name_len = strlen(name);
  char table_name[MAX_FULL_NAME_LEN];

  info = pars_info_create();

  *value->f_str = '\0';
  ut_a(value->f_len > 0);

  pars_info_bind_function(info, "my_func", fts_config_fetch_value, value);

  /* The len field of value must be set to the max bytes that
  it can hold. On a successful read, the len field will be set
  to the actual number of bytes copied to value. */
  pars_info_bind_varchar_literal(info, "name", (byte *)name, name_len);

  fts_table->suffix = FTS_SUFFIX_CONFIG;
  fts_get_table_name(fts_table, table_name);
  pars_info_bind_id(info, true, "table_name", table_name);

  graph = fts_parse_sql(fts_table, info,
                        "DECLARE FUNCTION my_func;\n"
                        "DECLARE CURSOR c IS SELECT value FROM $table_name"
                        " WHERE key = :name;\n"
                        "BEGIN\n"
                        ""
                        "OPEN c;\n"
                        "WHILE 1 = 1 LOOP\n"
                        "  FETCH c INTO my_func();\n"
                        "  IF c % NOTFOUND THEN\n"
                        "    EXIT;\n"
                        "  END IF;\n"
                        "END LOOP;\n"
                        "CLOSE c;");

  trx->op_info = "getting FTS config value";

  error = fts_eval_sql(trx, graph);

  que_graph_free(graph);

  return (error);
}

/** Create the config table name for retrieving index specific value.
 @return index config parameter name */
char *fts_config_create_index_param_name(
    const char *param,         /*!< in: base name of param */
    const dict_index_t *index) /*!< in: index for config */
{
  ulint len;
  char *name;

  /* The format of the config name is: name_<index_id>. */
  len = strlen(param);

  /* Caller is responsible for deleting name. */
  name = static_cast<char *>(ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, len + FTS_AUX_MIN_TABLE_ID_LENGTH + 2));
  ::strcpy(name, param);
  name[len] = '_';

  fts_write_object_id(index->id, name + len + 1);

  return (name);
}

/** Get value specific to an FTS index from the config table. The caller
 must ensure that enough space is allocated for value to hold the
 column contents.
 @return DB_SUCCESS or error code */
dberr_t fts_config_get_index_value(trx_t *trx,          /*!< transaction */
                                   dict_index_t *index, /*!< in: index */
                                   const char *param, /*!< in: get config value
                                                      for this parameter name */
                                   fts_string_t *value) /*!< out: value read
                                                        from config table */
{
  char *name;
  dberr_t error;
  fts_table_t fts_table;

  FTS_INIT_FTS_TABLE(&fts_table, FTS_SUFFIX_CONFIG, FTS_COMMON_TABLE,
                     index->table);

  /* We are responsible for free'ing name. */
  name = fts_config_create_index_param_name(param, index);

  error = fts_config_get_value(trx, &fts_table, name, value);

  ut::free(name);

  return (error);
}

/** Set the value in the config table for name.
 @return DB_SUCCESS or error code */
dberr_t fts_config_set_value(
    trx_t *trx,                /*!< transaction */
    fts_table_t *fts_table,    /*!< in: the indexed
                               FTS table */
    const char *name,          /*!< in: get config value for
                               this parameter name */
    const fts_string_t *value) /*!< in: value to update */
{
  pars_info_t *info;
  que_t *graph;
  dberr_t error;
  undo_no_t undo_no;
  undo_no_t n_rows_updated;
  ulint name_len = strlen(name);
  char table_name[MAX_FULL_NAME_LEN];

  info = pars_info_create();

  pars_info_bind_varchar_literal(info, "name", (byte *)name, name_len);
  pars_info_bind_varchar_literal(info, "value", value->f_str, value->f_len);

  fts_table->suffix = FTS_SUFFIX_CONFIG;
  fts_get_table_name(fts_table, table_name);
  pars_info_bind_id(info, true, "table_name", table_name);

  graph = fts_parse_sql(fts_table, info,
                        "BEGIN UPDATE $table_name SET value = :value"
                        " WHERE key = :name;");

  trx->op_info = "setting FTS config value";

  undo_no = trx->undo_no;

  error = fts_eval_sql(trx, graph);

  que_graph_free(graph);

  n_rows_updated = trx->undo_no - undo_no;

  /* Check if we need to do an insert. */
  if (n_rows_updated == 0) {
    info = pars_info_create();

    pars_info_bind_varchar_literal(info, "name", (byte *)name, name_len);

    pars_info_bind_varchar_literal(info, "value", value->f_str, value->f_len);

    fts_get_table_name(fts_table, table_name);
    pars_info_bind_id(info, true, "table_name", table_name);

    graph = fts_parse_sql(fts_table, info,
                          "BEGIN\n"
                          "INSERT INTO $table_name VALUES(:name, :value);");

    trx->op_info = "inserting FTS config value";

    error = fts_eval_sql(trx, graph);

    que_graph_free(graph);
  }

  return (error);
}

/** Set the value specific to an FTS index in the config table.
 @return DB_SUCCESS or error code */
dberr_t fts_config_set_index_value(trx_t *trx,          /*!< transaction */
                                   dict_index_t *index, /*!< in: index */
                                   const char *param, /*!< in: get config value
                                                      for this parameter name */
                                   fts_string_t *value) /*!< out: value read
                                                        from config table */
{
  char *name;
  dberr_t error;
  fts_table_t fts_table;

  FTS_INIT_FTS_TABLE(&fts_table, FTS_SUFFIX_CONFIG, FTS_COMMON_TABLE,
                     index->table);

  /* We are responsible for free'ing name. */
  name = fts_config_create_index_param_name(param, index);

  error = fts_config_set_value(trx, &fts_table, name, value);

  ut::free(name);

  return (error);
}

#ifdef FTS_OPTIMIZE_DEBUG
/** Get an ulint value from the config table.
 @return DB_SUCCESS if all OK else error code */
dberr_t fts_config_get_index_ulint(trx_t *trx,          /*!< in: transaction */
                                   dict_index_t *index, /*!< in: FTS index */
                                   const char *name,    /*!< in: param name */
                                   ulint *int_value)    /*!< out: value */
{
  dberr_t error;
  fts_string_t value;

  /* We set the length of value to the max bytes it can hold. This
  information is used by the callback that reads the value.*/
  value.f_len = FTS_MAX_CONFIG_VALUE_LEN;
  value.f_str = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, value.f_len + 1));

  error = fts_config_get_index_value(trx, index, name, &value);

  if (UNIV_UNLIKELY(error != DB_SUCCESS)) {
    ib::error(ER_IB_MSG_457)
        << "(" << ut_strerr(error) << ") reading `" << name << "'";
  } else {
    *int_value = strtoul((char *)value.f_str, NULL, 10);
  }

  ut::free(value.f_str);

  return (error);
}

/** Set an ulint value in the config table.
 @return DB_SUCCESS if all OK else error code */
dberr_t fts_config_set_index_ulint(trx_t *trx,          /*!< in: transaction */
                                   dict_index_t *index, /*!< in: FTS index */
                                   const char *name,    /*!< in: param name */
                                   ulint int_value)     /*!< in: value */
{
  dberr_t error;
  fts_string_t value;

  /* We set the length of value to the max bytes it can hold. This
  information is used by the callback that reads the value.*/
  value.f_len = FTS_MAX_CONFIG_VALUE_LEN;
  value.f_str = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, value.f_len + 1));

  // FIXME: Get rid of snprintf
  ut_a(FTS_MAX_INT_LEN < FTS_MAX_CONFIG_VALUE_LEN);

  value.f_len =
      snprintf((char *)value.f_str, FTS_MAX_INT_LEN, "%lu", int_value);

  error = fts_config_set_index_value(trx, index, name, &value);

  if (UNIV_UNLIKELY(error != DB_SUCCESS)) {
    ib::error(ER_IB_MSG_458)
        << "(" << ut_strerr(error) << ") writing `" << name << "'";
  }

  ut::free(value.f_str);

  return (error);
}
#endif /* FTS_OPTIMIZE_DEBUG */

/** Get an ulint value from the config table.
 @return DB_SUCCESS if all OK else error code */
dberr_t fts_config_get_ulint(trx_t *trx,             /*!< in: transaction */
                             fts_table_t *fts_table, /*!< in: the indexed
                                                     FTS table */
                             const char *name,       /*!< in: param name */
                             ulint *int_value)       /*!< out: value */
{
  dberr_t error;
  fts_string_t value;

  /* We set the length of value to the max bytes it can hold. This
  information is used by the callback that reads the value.*/
  value.f_len = FTS_MAX_CONFIG_VALUE_LEN;
  value.f_str = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, value.f_len + 1));

  error = fts_config_get_value(trx, fts_table, name, &value);

  if (UNIV_UNLIKELY(error != DB_SUCCESS)) {
    ib::error(ER_IB_MSG_459)
        << "(" << ut_strerr(error) << ") reading `" << name << "'";
  } else {
    *int_value = strtoul((char *)value.f_str, nullptr, 10);
  }

  ut::free(value.f_str);

  return (error);
}

/** Set an ulint value in the config table.
 @return DB_SUCCESS if all OK else error code */
dberr_t fts_config_set_ulint(trx_t *trx,             /*!< in: transaction */
                             fts_table_t *fts_table, /*!< in: the indexed
                                                     FTS table */
                             const char *name,       /*!< in: param name */
                             ulint int_value)        /*!< in: value */
{
  dberr_t error;
  fts_string_t value;

  /* We set the length of value to the max bytes it can hold. This
  information is used by the callback that reads the value.*/
  value.f_len = FTS_MAX_CONFIG_VALUE_LEN;
  value.f_str = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, value.f_len + 1));

  static_assert(FTS_MAX_INT_LEN < FTS_MAX_CONFIG_VALUE_LEN);

  value.f_len =
      snprintf((char *)value.f_str, FTS_MAX_INT_LEN, ULINTPF, int_value);

  error = fts_config_set_value(trx, fts_table, name, &value);

  if (UNIV_UNLIKELY(error != DB_SUCCESS)) {
    ib::error(ER_IB_MSG_460)
        << "(" << ut_strerr(error) << ") writing `" << name << "'";
  }

  ut::free(value.f_str);

  return (error);
}
