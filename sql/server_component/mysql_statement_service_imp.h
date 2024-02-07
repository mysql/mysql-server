/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_STATEMENT_SERVICE_IMP_H
#define MYSQL_STATEMENT_SERVICE_IMP_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_statement_service.h>
#include "field_types.h"
#include "mysql/components/services/defs/mysql_string_defs.h"

class mysql_stmt_factory_imp {
 public:
  static DEFINE_BOOL_METHOD(init, (my_h_statement * stmt_handle));

  static DEFINE_BOOL_METHOD(close, (my_h_statement stmt_handle));
};

class mysql_stmt_attributes_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (my_h_statement statement,
                                  mysql_cstring_with_length name, void *value));

  static DEFINE_BOOL_METHOD(set, (my_h_statement statement,
                                  mysql_cstring_with_length name,
                                  const void *value));
};

class mysql_stmt_bind_imp {
 public:
  static DEFINE_BOOL_METHOD(bind_param,
                            (my_h_statement statement, uint32_t index,
                             bool is_null, uint64_t type, bool is_unsigned,
                             const void *data, unsigned long data_length,
                             const char *name, unsigned long name_length));
};

class mysql_stmt_execute_imp {
 public:
  static DEFINE_BOOL_METHOD(execute, (my_h_statement stmt_handle));
  static DEFINE_BOOL_METHOD(prepare, (mysql_cstring_with_length query,
                                      my_h_statement stmt_handle));
  static DEFINE_BOOL_METHOD(reset, (my_h_statement statement));
};

class mysql_stmt_execute_direct_imp {
 public:
  static DEFINE_BOOL_METHOD(execute, (mysql_cstring_with_length query,
                                      my_h_statement stmt_handle));
};

class mysql_stmt_metadata_imp {
 public:
  static DEFINE_BOOL_METHOD(param_count, (my_h_statement statement,
                                          uint32_t *parameter_count));

  static DEFINE_BOOL_METHOD(param_metadata,
                            (my_h_statement statement, uint32_t index,
                             const char *member, void *data));
};

class mysql_stmt_result_imp {
 public:
  static DEFINE_BOOL_METHOD(next_result,
                            (my_h_statement statement, bool *has_next));

  static DEFINE_BOOL_METHOD(fetch, (my_h_statement statement, my_h_row *row));
};

class mysql_stmt_resultset_metadata_imp {
 public:
  static DEFINE_BOOL_METHOD(fetch_field,
                            (my_h_statement statement, uint32_t column_index,
                             my_h_field *field));

  static DEFINE_BOOL_METHOD(field_count,
                            (my_h_statement statement, uint32_t *num_fields));

  static DEFINE_BOOL_METHOD(field_info,
                            (my_h_field field, const char *name, void *data));
};

class mysql_stmt_diagnostics_imp {
 public:
  static DEFINE_BOOL_METHOD(affected_rows,
                            (my_h_statement statement, uint64_t *num_rows));
  static DEFINE_BOOL_METHOD(insert_id,
                            (my_h_statement statement, uint64_t *retval));
  static DEFINE_BOOL_METHOD(error_id,
                            (my_h_statement stmt_handle, uint64_t *error_id));

  static DEFINE_BOOL_METHOD(error, (my_h_statement stmt_handle,
                                    mysql_cstring_with_length *error_message));

  static DEFINE_BOOL_METHOD(
      sqlstate, (my_h_statement stmt_handle,
                 mysql_cstring_with_length *sqlstate_error_message));

  static DEFINE_BOOL_METHOD(num_warnings,
                            (my_h_statement stmt_handle, uint32_t *count));

  static DEFINE_BOOL_METHOD(get_warning,
                            (my_h_statement stmt_handle, uint32_t warning_index,
                             my_h_warning *warning));

  static DEFINE_BOOL_METHOD(warning_level,
                            (my_h_warning warning, uint32_t *level));

  static DEFINE_BOOL_METHOD(warning_code,
                            (my_h_warning warning, uint32_t *code));

  static DEFINE_BOOL_METHOD(warning_message,
                            (my_h_warning warning,
                             mysql_cstring_with_length *error_message));
};

class mysql_stmt_get_integer_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index,
                                  int64_t *data, bool *is_null));
};

class mysql_stmt_get_unsigned_integer_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index,
                                  uint64_t *data, bool *is_null));
};

class mysql_stmt_get_double_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index,
                                  double *data, bool *is_null));
};

class mysql_stmt_get_time_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index,
                                  mle_time *time, bool *is_null));
};

class mysql_stmt_get_string_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (my_h_row row, uint32_t column_index,
                             mysql_cstring_with_length *data, bool *is_null));
};

#endif /* MYSQL_STATEMENT_SERVICE_IMP_H */
