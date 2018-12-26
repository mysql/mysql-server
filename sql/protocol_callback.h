/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PROTOCOL_CALLBACK_INCLUDED
#define PROTOCOL_CALLBACK_INCLUDED

/**
@file
  Interface of the Protocol_callback class, which is used by the Command
  service as proxy protocol.
*/


#include "protocol.h"
#include "mysql/service_command.h"

class Protocol_callback : public Protocol
{
public:
  Protocol_callback(const struct st_command_service_cbs *cbs,
                    enum cs_text_or_binary t_or_b, void *cbs_ctx) :
    callbacks_ctx(cbs_ctx),
    callbacks(*cbs),
    client_capabilities(0),
    client_capabilities_set(false),
    text_or_binary(t_or_b),
    in_meta_sending(false)
    {}

  /**
    Forces read of packet from the connection

    @return
      bytes read
      -1 failure
  */
  virtual int read_packet();

  /**
    Reads from the line and parses the data into union COM_DATA

    @return
      bytes read
      -1 failure
  */
  virtual int get_command(COM_DATA *com_data, enum_server_command *cmd);

  /**
    Returns the type of the protocol

    @return
      false  success
      true   failure
  */
  virtual enum enum_protocol_type type() { return PROTOCOL_PLUGIN; }


  /**
    Returns the type of the connection

    @return
      enum enum_vio_type
  */
  virtual enum enum_vio_type connection_type();

  /**
    Sends null value

    @return
      false  success
      true   failure
  */
  virtual bool store_null();

  /**
    Sends TINYINT value

    @param from value

    @return
      false  success
      true   failure
  */
  virtual bool store_tiny(longlong from);

  /**
    Sends SMALLINT value

    @param from value

    @return
      false  success
      true   failure
  */
  virtual bool store_short(longlong from);

  /**
    Sends INT/INTEGER value

    @param from value

    @return
      false  success
      true   failure
  */
  virtual bool store_long(longlong from);

  /**
    Sends BIGINT value

    @param from         value
    @param is_unsigned  from is unsigned

    @return
      false  success
      true   failure
  */
  virtual bool store_longlong(longlong from, bool is_unsigned);

  /**
    Sends DECIMAL value

    @param d    value
    @param prec field's precision, unused
    @param dec  field's decimals, unused

    @return
      false  success
      true   failure
  */
  virtual bool store_decimal(const my_decimal * d, uint, uint);

  /**
    Sends string (CHAR/VARCHAR/TEXT/BLOB) value

    @param d value

    @return
      false  success
      true   failure
  */
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO *fromcs);

  /**
    Sends FLOAT value

    @param from      value
    @param decimals
    @param buffer    auxiliary buffer

    @return
      false  success
      true   failure
  */
  virtual bool store(float from, uint32 decimals, String *buffer);

  /**
    Sends DOUBLE value

    @param from      value
    @param decimals
    @param buffer    auxiliary buffer

    @return
      false  success
      true   failure
  */
  virtual bool store(double from, uint32 decimals, String *buffer);

  /**
    Sends DATETIME value

    @param time      value
    @param precision

    @return
      false  success
      true   failure
  */
  virtual bool store(MYSQL_TIME *time, uint precision);

  /**
    Sends DATE value

    @param time      value

    @return
      false  success
      true   failure
  */
  virtual bool store_date(MYSQL_TIME *time);

  /**
    Sends TIME value

    @param time      value
    @param precision

    @return
      false  success
      true   failure
  */
  virtual bool store_time(MYSQL_TIME *time, uint precision);

  /**
    Sends Field

    @param field

    @return
      false  success
      true   failure
  */
  virtual bool store(Proto_field *field);

  /**
    Returns the capabilities supported by the protocol
  */
  virtual ulong get_client_capabilities();

  /**
    Checks if the protocol supports a capability

    @param cap the capability

    @return
      true   supports
      false  does not support
  */
  virtual bool has_client_capability(unsigned long capability);

  /**
    Called BEFORE sending data row or before field_metadata
  */
  virtual void start_row();

  /**
    Called AFTER sending data row or before field_metadata
  */
  virtual bool end_row();

  /**
    Called when a row is aborted
  */
  virtual void abort_row();

  /**
    Called in case of error while sending data
  */
  virtual void end_partial_result_set();

  /**
    Called when the server shuts down the connection (THD is being destroyed).
    In this regard, this is also called when the server shuts down. The callback
    implementor can differentiate between those 2 events by inspecting the
    shutdown_type parameter.

    @param server_shutdown  Whether this is a normal connection shutdown (false)
                            or a server shutdown (true).

    @return
    0   success
    !0  failure
  */
  virtual int shutdown(bool server_shutdown= false);

  /**
    This function always returns true as in many places in the server this
    is a prerequisite for continuing operations.

    @return
      true   alive
  */
  virtual bool connection_alive();

  /**
    Should return protocol's reading/writing status. Returns 0 (idle) as it
    this is the best guess that can be made as there is no callback for
    get_rw_status().
  */
  virtual uint get_rw_status();

  /**
    Checks if compression is enabled

    @return
      true  enabled
      false disabled
  */
  virtual bool get_compression();

  /**
    Called BEFORE sending metadata

    @param num_cols Number of columns in the result set
    @param flags
    @param resultcs The character set of the results. Can be different from the
                    one in the field metadata.

    @return
      true  failure
     false success
  */
  virtual bool start_result_metadata(uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs);

  /**
    Sends metadata of one field. Called for every column in the result set.

    @param field  Field's metadata
    @param cs     Charset

    @return
      true  failure
      false success
  */
  virtual bool send_field_metadata(Send_field *field, const CHARSET_INFO *cs);

  /**
    Called AFTER sending metadata

    @return
      true  failure
      false success
  */
  virtual bool end_result_metadata();

  /**
    Sends OK

    @param server_status Bit field with different statuses. See SERVER_STATUS_*
    @param warn_count      Warning count from the execution
    @param affected_row    Rows changed/deleted during the operation
    @param last_insert_id  ID of the last insert row, which has AUTO_INCROMENT
                           column
    @param message         Textual message from the execution. May be NULL.

    @return
      true  failure
      false success
  */
  virtual bool send_ok(uint server_status, uint warn_count,
                       ulonglong affected_rows, ulonglong last_insert_id,
                       const char *message);

  /**
    Sends end of file.


    This will be called once all data has been sent.

    @param server_status Bit field with different statuses. See SERVER_STATUS_*
    @param warn_count    The warning count generated by the execution of the
                         statement.

    @return
      true  failure
      false success
  */
  virtual bool send_eof(uint server_status, uint warn_count);

  /**
    Sends error

    @param sql_errno  Error number, beginning from 1000
    @param err_msg    The error message
    @param sql_state  The SQL state - 5 char string

    @return
      true  failure
      false success
  */
  virtual bool send_error(uint sql_errno, const char *err_msg,
                          const char *sql_state);

private:
  void *callbacks_ctx;
  struct st_command_service_cbs callbacks;
  unsigned long client_capabilities;
  bool client_capabilities_set;
  enum cs_text_or_binary text_or_binary;
  bool in_meta_sending;
};

#endif /* PROTOCOL_CALLBACK_INCLUDED */
