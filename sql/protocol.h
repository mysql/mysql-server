#ifndef PROTOCOL_INCLUDED
#define PROTOCOL_INCLUDED

/* Copyright (c) 2002, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_error.h"
#include "my_decimal.h"                         /* my_decimal */

#include "violite.h"                            /* SSL && enum_vio_type */

#ifdef __cplusplus
class THD;
#define MYSQL_THD THD*
#else
#define MYSQL_THD void*
#endif

#include "mysql/com_data.h"

class Send_field;
class Proto_field;


class Protocol {
public:
  /**
    Read packet from client

    @returns
      -1  fatal error
       0  ok
       1 non-fatal error
  */
  virtual int read_packet()= 0;

  /**
    Reads the command from the protocol and creates a command.

    @param com_data  out parameter
    @param cmd       out parameter
    @param pkt       packet to be parsed
    @param length    size of the packet

    @returns
      -1  fatal protcol error
      0   ok
      1   non-fatal protocol or parsing error
  */
  virtual int get_command(COM_DATA *com_data, enum_server_command *cmd)= 0;

  /**
    Enum used by type() to specify the protocol type
  */
  enum enum_protocol_type
  {
    /*
      Before adding a new type, please make sure
      there is enough storage for it in Query_cache_query_flags.
    */
    PROTOCOL_TEXT= 0,            // text Protocol type used mostly
                                 // for the old (MySQL 4.0 protocol)
    PROTOCOL_BINARY= 1,          // binary protocol type
    PROTOCOL_LOCAL= 2,           // local protocol type(intercepts communication)
    PROTOCOL_ERROR = 3,          // error protocol instance
    PROTOCOL_PLUGIN = 4          // pluggable protocol type
  };

  /**
    Flags available to alter the way the messages are sent to the client
  */
  enum
  {
    SEND_NUM_ROWS= 1,
    SEND_DEFAULTS= 2,
    SEND_EOF= 4
  };

  virtual enum enum_protocol_type type()= 0;

  virtual enum enum_vio_type connection_type()= 0;

  /* Data sending functions */
  virtual bool store_null()= 0;
  virtual bool store_tiny(longlong from)= 0;
  virtual bool store_short(longlong from)= 0;
  virtual bool store_long(longlong from)= 0;
  virtual bool store_longlong(longlong from, bool unsigned_flag)= 0;
  virtual bool store_decimal(const my_decimal *, uint, uint)= 0;
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO *fromcs)= 0;
  virtual bool store(float from, uint32 decimals, String *buffer)= 0;
  virtual bool store(double from, uint32 decimals, String *buffer)= 0;
  virtual bool store(MYSQL_TIME *time, uint precision)= 0;
  virtual bool store_date(MYSQL_TIME *time)= 0;
  virtual bool store_time(MYSQL_TIME *time, uint precision)= 0;
  virtual bool store(Proto_field *field)= 0;
  // Convenience wrappers
  inline  bool store(int from)
  { return store_long((longlong) from); }
  inline  bool store(uint32 from)
  { return store_long((longlong) from); }
  inline  bool store(longlong from)
  { return store_longlong(from, 0); }
  inline  bool store(ulonglong from)
  { return store_longlong((longlong) from, 1); }
  /**
    Send \\0 end terminated string.

    @param from	NullS or \\0 terminated string

    @note In most cases one should use store(from, length, cs) instead of
    this function

    @returns
      false   ok
      true    error
  */
  inline bool store(const char *from, const CHARSET_INFO *fromcs)
  { return from ? store(from, strlen(from), fromcs) : store_null(); }
  inline bool store(String *str)
  { return store((char*) str->ptr(), str->length(), str->charset()); }
  inline bool store(const LEX_STRING &s, const CHARSET_INFO *cs)
  { return store(s.str, s.length, cs); }

  /**
    Returns the client capabilities stored on the protocol.
    The available capabilites are defined in mysql_com.h
  */
  virtual ulong get_client_capabilities()= 0;
  /**
    Checks if the client capabilities include the one
    specified as parameter.

    @returns
      true    if it includes the specified capability
      false   otherwise
  */
  virtual bool has_client_capability(unsigned long client_capability)= 0;

  /**
     Checks if the protocol's connection with the client is still alive.
     It should always return true unless the protocol closed the connection.

     @returns
      true    if the connection is still alive
      false   otherwise
   */
  virtual bool connection_alive()= 0;

   /**
     Result set sending functions

     @details Server uses following schema to send result:
                   ... sending metadata ...
                              | start_result_metadata(...)
                              | start_row()
                              | send_field_metadata(...)
                              | end_row()
               ... same for each field sent ...
                              | end_result_metadata(...)
                              |
                   ... sending result ...
                              | start_row(...)
                              | store_xxx(...)
            ... store_xxx(..) is called for each field ...
                              | end_row(...)
         ... same for each row, until all rows are sent ...
                              | send_ok/eof/error(...)
    However, a protocol implementation might use different schema. For
    example, Protocol_callback ignores start/end_row when metadata is being
    sent.
  */

  virtual void start_row()= 0;
  virtual bool end_row()= 0;
  virtual void abort_row()= 0;
  virtual void end_partial_result_set()= 0;

  /**
    Thread is being shut down, disconnect and free resources

    @param server_shutdown  If false then this is normal thread shutdown. If
                            true then the server is shutting down.
  */
  virtual int shutdown(bool server_shutdown= false)= 0;

  /**
    Returns the read/writing status

    @return
      @retval 1       Read
      @retval 2       Write
      @retval 0       Other(Idle, Killed)
  */
  virtual uint get_rw_status()= 0;
  /**
    Returns if the protocol is compressed or not.

    @return
      @retval false   Not compressed
      @retval true    Compressed
  */
  virtual bool get_compression()= 0;
  /**
    Prepares the server for metadata sending.
    Notifies the client that the metadata sending will start.

    @param num_cols                Number of columns that will be sent
    @param flags                   Flags to alter the metadata sending
                                   Can be any of the following:
                                   SEND_NUM_ROWS, SEND_DEFAULTS, SEND_EOF
    @param resultcs                Charset to convert to

    @return
      @retval false   Ok
      @retval true    An error occurred
  */

  virtual bool start_result_metadata(uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs)= 0;
  /**
    Sends field metadata.

    @param field                   Field metadata to be send to the client
    @param field                   Field to be send to the client
    @param charset                 Field's charset: in case it is different
                                   than the one used by the connection it will
                                   be used to convert the value to
                                   the connection's charset

    @return
      @retval false   The metadata was successfully sent
      @retval true    An error occurred
  */

  virtual bool send_field_metadata(Send_field *field,
                                   const CHARSET_INFO *charset)= 0;
  /**
    Signals the client that the metadata sending is done.
    Clears the server after sending the metadata.

    @return
      @retval false   Ok
      @retval true    An error occurred
  */
  virtual bool end_result_metadata()= 0;

  /**
    Send ok message to the client.

    @param server_status           The server status
    @param statement_warn_count    Total number of warnings
    @param affected_rows           Number of rows changed by statement
    @param last_insert_id          Last insert id (Auto_increment id for first
                                   row if used)
    @param message                 Message to send to the client

    @return
      @retval false The message was successfully sent
      @retval true An error occurred and the messages wasn't sent properly
  */
  virtual bool send_ok(uint server_status, uint statement_warn_count,
                       ulonglong affected_rows, ulonglong last_insert_id,
                       const char *message)= 0;
  /**
    Send eof message to the client.

    @param server_status          The server status
    @param statement_warn_count   Total number of warnings

    @return
      @retval false The message was successfully sent
      @retval true An error occurred and the messages wasn't sent properly
  */
  virtual bool send_eof(uint server_status, uint statement_warn_count)= 0;
  /**
    Send error message to the client.

    @param sql_errno    The error code to send
    @param err          A pointer to the error message
    @param sqlstate     SQL state

    @return
      @retval false The message was successfully sent
      @retval true An error occurred and the messages wasn't sent properly
  */

  virtual bool send_error(uint sql_errno, const char *err_msg,
                          const char *sql_state)= 0;
};

#endif /* PROTOCOL_INCLUDED */
