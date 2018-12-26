/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql_service_context.h"
#include "plugin_log.h"


int Sql_service_context::start_result_metadata(uint ncols, uint flags,
                                           const CHARSET_INFO *resultcs)
{
  DBUG_ENTER("Sql_service_context::start_result_metadata");
  DBUG_PRINT("info",("resultcs->name: %s", resultcs->name));
  if (resultset)
  {
    resultset->set_cols(ncols);
    resultset->set_charset(resultcs);
  }
  DBUG_RETURN(0);
}

int Sql_service_context::field_metadata(struct st_send_field *field,
                                    const CHARSET_INFO *charset)
{
  DBUG_ENTER("Sql_service_context::field_metadata");
  DBUG_PRINT("info",("field->flags: %d", (int)field->flags));
  DBUG_PRINT("info",("field->type: %d", (int)field->type));

  if (resultset)
  {
    Field_type ftype = {
                        field->db_name, field->table_name,
                        field->org_table_name, field->col_name,
                        field->org_col_name, field->length,
                        field->charsetnr, field->flags,
                        field->decimals, field->type
                      };
    resultset->set_metadata(ftype);
  }
  DBUG_RETURN(0);
}

int Sql_service_context::end_result_metadata(uint server_status,
                                         uint warn_count)
{
  DBUG_ENTER("Sql_service_context::end_result_metadata");
  DBUG_RETURN(0);
}

int Sql_service_context::start_row()
{
  DBUG_ENTER("Sql_service_context::start_row");
  if (resultset)
    resultset->new_row();
  DBUG_RETURN(0);
}

int Sql_service_context::end_row()
{
  DBUG_ENTER("Sql_service_context::end_row");
  if (resultset)
    resultset->increment_rows();
  DBUG_RETURN(0);
}

void Sql_service_context::abort_row()
{
  DBUG_ENTER("Sql_service_context::abort_row");
  DBUG_VOID_RETURN;
}

ulong Sql_service_context::get_client_capabilities()
{
  DBUG_ENTER("Sql_service_context::get_client_capabilities");
  DBUG_RETURN(0);
}

int Sql_service_context::get_null()
{
  DBUG_ENTER("Sql_service_context::get_null");
  if (resultset)
    resultset->new_field(NULL);
  DBUG_RETURN(0);
}

int Sql_service_context::get_integer(longlong value)
{
  DBUG_ENTER("Sql_service_context::get_integer");
  if (resultset)
    resultset->new_field(new Field_value(value));
  DBUG_RETURN(0);
}

int Sql_service_context::get_longlong(longlong value,
                                  uint is_unsigned)
{
  DBUG_ENTER("Sql_service_context::get_longlong");
  if (resultset)
    resultset->new_field(new Field_value(value, is_unsigned));
  DBUG_RETURN(0);
}

int Sql_service_context::get_decimal(const decimal_t * value)
{
  DBUG_ENTER("Sql_service_context::get_decimal");
  if (resultset)
    resultset->new_field(new Field_value(*value));
  DBUG_RETURN(0);
}

int Sql_service_context::get_double(double value,
                                uint32 decimals)
{
  DBUG_ENTER("Sql_service_context::get_double");
  if (resultset)
    resultset->new_field(new Field_value(value));
  DBUG_RETURN(0);
}

int Sql_service_context::get_date(const MYSQL_TIME * value)
{
  DBUG_ENTER("Sql_service_context::get_date");
  if (resultset)
    resultset->new_field(new Field_value(*value));
  DBUG_RETURN(0);
}

int Sql_service_context::get_time(const MYSQL_TIME * value,
                              uint decimals)
{
  DBUG_ENTER("Sql_service_context::get_time");
  if (resultset)
    resultset->new_field(new Field_value(*value));
  DBUG_RETURN(0);
}

int Sql_service_context::get_datetime(const MYSQL_TIME * value,
                                  uint decimals)
{
  DBUG_ENTER("Sql_service_context::get_datetime");
  if (resultset)
    resultset->new_field(new Field_value(*value));
  DBUG_RETURN(0);
}


int Sql_service_context::get_string(const char * const value,
                                size_t length,
                                const CHARSET_INFO * const valuecs)
{
  DBUG_ENTER("Sql_service_context::get_string");
  DBUG_PRINT("info",("value: %s", value));
  if (resultset)
    resultset->new_field(new Field_value(value, length));
  DBUG_RETURN(0);
}

void Sql_service_context::handle_ok(uint server_status, uint statement_warn_count,
                                ulonglong affected_rows,
                                ulonglong last_insert_id,
                                const char * const message)
{
  DBUG_ENTER("Sql_service_context::handle_ok");

  if (resultset)
  {
    resultset->set_server_status(server_status);
    resultset->set_warn_count(statement_warn_count);
    resultset->set_affected_rows(affected_rows);
    resultset->set_last_insert_id(last_insert_id);
    resultset->set_message(message ? message : "");
  }
  DBUG_VOID_RETURN;
}

void Sql_service_context::handle_error(uint sql_errno,
                                   const char * const err_msg,
                                   const char * const sqlstate)
{
  DBUG_ENTER("Sql_service_context::handle_error");
  DBUG_PRINT("info",("sql_errno: %d", (int)sql_errno));
  DBUG_PRINT("info",("err_msg: %s", err_msg));
  DBUG_PRINT("info",("sqlstate: %s", sqlstate));

  if (resultset)
  {
    resultset->set_rows(0);
    resultset->set_sql_errno(sql_errno);
    resultset->set_err_msg(err_msg ? err_msg : "");
    resultset->set_sqlstate(sqlstate ? sqlstate : "");
  }
  DBUG_VOID_RETURN;
}


void Sql_service_context::shutdown(int flag)
{
  DBUG_ENTER("Sql_service_context::shutdown");
  if (resultset)
    resultset->set_killed();
  DBUG_VOID_RETURN;
}
