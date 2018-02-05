/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "protocol_callback.h"
#include "sql_class.h"
#include <stdarg.h>

/**
@file
  Implementation of the Protocol_callback class, which is used by the Command
  service as proxy protocol.
*/


/**
  Practically does nothing.
  Returns -1, error, for the case this will be called. It should happen.
  read_packet() is called by get_command() which in turn is called by
  do_command() in sql_parse. After that COM_DATA is filled with proper info
  that in turn is passed to dispatch_command().
  The Command service doesn't use do_command() but dispatch_command() and
  passes COM_DATA directly from the user(plugin).

  @return
    -1 failure
*/
int Protocol_callback::read_packet()
{
  return -1;
};

/**
  Practically does nothing. See the comment of ::read_packet().
  Always returns -1.

  @return
    -1
*/
int Protocol_callback::get_command(COM_DATA *com_data, enum_server_command *cmd)
{
  return read_packet();
};


/**
  Returns the type of the connection.

  @return
    VIO_TYPE_PLUGIN
*/
enum enum_vio_type Protocol_callback::connection_type()
{
  return VIO_TYPE_PLUGIN;
}


/**
  Sends null value

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_null()
{
  if (callbacks.get_null)
    return callbacks.get_null(callbacks_ctx);

  return false;
}

/**
  Sends TINYINT value

  @param from value

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_tiny(longlong from)
{
  if (callbacks.get_integer)
    return callbacks.get_integer(callbacks_ctx, from);
  return false;
}

/**
  Sends SMALLINT value

  @param from value

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_short(longlong from)
{
  if (callbacks.get_integer)
    return callbacks.get_integer(callbacks_ctx, from);
  return false;
}

/**
  Sends INT/INTEGER value

  @param from value

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_long(longlong from)
{
  if (callbacks.get_integer)
    return callbacks.get_integer(callbacks_ctx, from);
  return false;
}

/**
  Sends BIGINT value

  @param from         value
  @param is_unsigned  from is unsigned

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_longlong(longlong from, bool is_unsigned)
{
  if (callbacks.get_integer)
    return callbacks.get_longlong(callbacks_ctx, from, is_unsigned);
  return false;
}

/**
  Sends DECIMAL value

  @param d    value
  @param prec field's precision, unused
  @param dec  field's decimals, unused

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_decimal(const my_decimal * d, uint prec, uint dec)
{
  if (callbacks.get_decimal)
    return callbacks.get_decimal(callbacks_ctx, d);
  return false;
}

/**
  Sends string (CHAR/VARCHAR/TEXT/BLOB) value

  @param d value

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store(const char *from, size_t length,
                              const CHARSET_INFO *fromcs)
{
  if (callbacks.get_string)
    return callbacks.get_string(callbacks_ctx, from, length, fromcs);
  return false;
}

/**
  Sends FLOAT value

  @param from      value
  @param decimals
  @param buffer    auxiliary buffer

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store(float from, uint32 decimals, String *buffer)
{
  if (callbacks.get_double)
    return callbacks.get_double(callbacks_ctx, from, decimals);
  return false;
}

/**
  Sends DOUBLE value

  @param from      value
  @param decimals
  @param buffer    auxiliary buffer

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store(double from, uint32 decimals, String *buffer)
{
  if (callbacks.get_double)
    return callbacks.get_double(callbacks_ctx, from, decimals);
  return false;
}


/**
  Sends DATETIME value

  @param time      value
  @param precision

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store(MYSQL_TIME *time, uint precision)
{
  if (callbacks.get_datetime)
    return callbacks.get_datetime(callbacks_ctx, time, precision);
  return false;
}

/**
  Sends DATE value

  @param time      value

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_date(MYSQL_TIME *time)
{
  if (callbacks.get_datetime)
    return callbacks.get_date(callbacks_ctx, time);
  return false;
}

/**
  Sends TIME value

  @param time      value
  @param precision

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store_time(MYSQL_TIME *time, uint precision)
{
  if (callbacks.get_time)
    return callbacks.get_time(callbacks_ctx, time, precision);
  return false;
}

/**
  Sends Field

  @param field

  @return
    false  success
    true   failure
*/
bool Protocol_callback::store(Proto_field *field)
{
  switch (text_or_binary)
  {
  case CS_TEXT_REPRESENTATION:
    return field->send_text(this);
  case CS_BINARY_REPRESENTATION:
    return field->send_binary(this);
  }
  return true;
}

/**
  Returns the capabilities supported by the protocol
*/
ulong Protocol_callback::get_client_capabilities()
{
  if (!client_capabilities_set && callbacks.get_client_capabilities)
  {
    client_capabilities_set= true;
    client_capabilities= callbacks.get_client_capabilities(callbacks_ctx);
  }
  return client_capabilities;
}

/**
  Checks if the protocol supports a capability

  @param cap the capability

  @return
    true   supports
    false  does not support
*/
bool Protocol_callback::has_client_capability(unsigned long capability)
{
  if (!client_capabilities_set)
    (void) get_client_capabilities();
  return client_capabilities & capability;
}

/**
  Called BEFORE sending data row or before field_metadata
*/
void Protocol_callback::start_row()
{
  /*
    start_row() is called for metadata for convenience in Protocol_classic
    This is not wanted for protocol plugins as otherwise we get more calls
    and the plugin has to track state internally if it is in meta or not.
    start_meta(),
      start_row(), field_meta(), end_row(),
      ...
    end_meta()
    Calling start_row() is a left over from the era where metadata was
    sent with only one call send_metadata, and start_row() + end_row()
    were usable hooks in this send_metadata. Now it doesn't make any more
    sense as start_row() is called just before field_meta() and end_row()
    just after. The same code can go in to field_meta().
  */
  if (!in_meta_sending && callbacks.start_row)
    callbacks.start_row(callbacks_ctx);
}

/**
  Called AFTER sending all fields of a row, or after field_metadata().
  Please read the big comment in start_row() for explanation why
  in_meta_sending is used.

  @return
    true   ok
    false  not ok
*/
bool Protocol_callback::end_row()
{
  /* See start_row() */
  if (!in_meta_sending && callbacks.end_row)
    return callbacks.end_row(callbacks_ctx);
  return false;
}

/**
  Called when a row is aborted
*/
void Protocol_callback::abort_row()
{
  if (callbacks.end_row)
    callbacks.abort_row(callbacks_ctx);
}

/**
  Called in case of error while sending data
*/
void Protocol_callback::end_partial_result_set()
{
  /* Protocol_callback shouldn't be used in this context */
  assert(0);
}

/**
  Called when the server shuts down the connection (THD is being destroyed).
  In this regard, this is also called when the server shuts down. The callback
  implementor can differentiate between those 2 events by inspecting the
  server_shutdown parameter.

  @param server_shutdown  Whether this is a normal connection shutdown (false)
                          or a server shutdown (true).

  @return
  0   success
  !0  failure
*/
int Protocol_callback::shutdown(bool server_shutdown)
{
  if (callbacks.shutdown)
    callbacks.shutdown(callbacks_ctx, server_shutdown? 1 : 0);
  return 0;
}

/**
  Returns if the connection is alive or dead.

  @note This function always returns true as in many places in the server this
  is a prerequisite for continuing operations.

  @return
    true  alive
*/
bool Protocol_callback::connection_alive()
{
  return true;
}

/**
  Should return protocol's reading/writing status. Returns 0 (idle) as it this
  is the best guess that can be made as there is no callback for
  get_rw_status().

  @return
    0
*/
uint Protocol_callback::get_rw_status()
{
  return 0;
}

/**
  Should check if compression is enabled. Returns always false (no compression)

  @return
    false disabled
*/
bool Protocol_callback::get_compression()
{
  return false;
}

/**
  Called BEFORE sending metadata

  @return
    true  failure
    false success
*/
bool Protocol_callback::start_result_metadata(uint num_cols, uint flags,
                                              const CHARSET_INFO *resultcs)
{
  in_meta_sending= true;
  if (callbacks.start_result_metadata)
    return callbacks.start_result_metadata(callbacks_ctx, num_cols,
                                           flags, resultcs);
  return false;
}

/**
  Sends metadata of one field. Called for every column in the result set.

  @return
    true  failure
    false success
*/
bool Protocol_callback::send_field_metadata(Send_field *field,
                                            const CHARSET_INFO *cs)
{
  if (callbacks.field_metadata)
  {
    struct st_send_field f;

    f.db_name= field->db_name;
    f.table_name= field->table_name;
    f.org_table_name= field->org_table_name;
    f.col_name= field->col_name;
    f.org_col_name= field->org_col_name;
    f.length= field->length;
    f.charsetnr= field->charsetnr;
    f.flags= field->flags;
    f.decimals= field->decimals;
    f.type= field->type;

    return callbacks.field_metadata(callbacks_ctx, &f, cs);
  }
  return true;
}

/**
  Called AFTER sending metadata

  @return
    true  failure
    false success
*/
bool Protocol_callback::end_result_metadata()
{
  in_meta_sending= false;

  if (callbacks.end_result_metadata)
  {
    THD *t= current_thd;
    uint status= t->server_status;
    uint warn_count= t->get_stmt_da()->current_statement_cond_count();

    return callbacks.end_result_metadata(callbacks_ctx, status, warn_count);
  }
  return false;
}

/**
  Sends OK

  @return
    true  failure
    false success
*/
bool Protocol_callback::send_ok(uint server_status, uint warn_count,
                                ulonglong affected_rows,
                                ulonglong last_insert_id,
                                const char *message)
{
  if (callbacks.handle_ok)
    callbacks.handle_ok(callbacks_ctx, server_status, warn_count,
                        affected_rows, last_insert_id, message);
  return false;
}

/**
  Sends end of file

  @return
    true  failure
    false success
*/
bool Protocol_callback::send_eof(uint server_status, uint warn_count)
{
  if (callbacks.handle_ok)
    callbacks.handle_ok(callbacks_ctx, server_status, warn_count, 0, 0, NULL);
  return false;
}

/**
  Sends error

  @return
    true  failure
    false success
*/
bool Protocol_callback::send_error(uint sql_errno, const char *err_msg,
                                   const char *sql_state)
{
  if (callbacks.handle_error)
    callbacks.handle_error(callbacks_ctx, sql_errno, err_msg, sql_state);
  return false;
}
