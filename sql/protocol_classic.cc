/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  Low level functions for storing data to be send to the MySQL client.
  The actual communication is handled by the net_xxx functions in net_serv.cc
*/

/**
  @page page_protocol_basics Protocol Basics

  This is a description of the basic building blocks used by the MySQL protocol:
  - @subpage page_protocol_basic_data_types
  - @subpage page_protocol_basic_packets
  - @subpage page_protocol_basic_response_packets
  - @subpage page_protocol_basic_character_set
*/


/**
  @page page_protocol_basic_data_types Basic Data Types

  The protocol has a few basic types that are used throughout the protocol:
  - @subpage page_protocol_basic_dt_integers
  - @subpage page_protocol_basic_dt_strings
*/

/**
  @page page_protocol_basic_dt_integers Integer Types

  The MySQL %Protocol has a set of possible encodings for integers.

  @section sect_protocol_basic_dt_int_fixed Protocol::FixedLengthInteger

  Fixed-Length Integer Types
  ============================

  A fixed-length unsigned integer stores its value in a series of 
  bytes with the least significant byte first.

  The MySQL uses the following fixed-length unsigned integer variants:
  - @anchor a_protocol_type_int1 int\<1>:
    1 byte @ref sect_protocol_basic_dt_int_fixed.
  - @anchor a_protocol_type_int2 int\<2\>:
    2 byte @ref sect_protocol_basic_dt_int_fixed.  See int2store()
  - @anchor a_protocol_type_int3 int\<3\>:
    3 byte @ref sect_protocol_basic_dt_int_fixed.  See int3store()
  - @anchor a_protocol_type_int4 int\<4\>:
    4 byte @ref sect_protocol_basic_dt_int_fixed.  See int4store()
  - @anchor a_protocol_type_int6 int\<6\>:
    6 byte @ref sect_protocol_basic_dt_int_fixed.  See int6store()
  - @anchor a_protocol_type_int8 int\<8\>:
    8 byte @ref sect_protocol_basic_dt_int_fixed.  See int8store()

  See int3store() for an example.


  @section sect_protocol_basic_dt_int_le Protocol::LengthEncodedInteger

  Length-Encoded Integer Type
  ==============================

  An integer that consumes 1, 3, 4, or 9 bytes, depending on its numeric value

  To convert a number value into a length-encoded integer:

  Greater or equal |     Lower than | Stored as
  -----------------|----------------|-------------------------
                 0 |            251 | `1-byte integer`
               251 | 2<sup>16</sup> | `0xFC + 2-byte integer`
    2<sup>16</sup> | 2<sup>24</sup> | `0xFD + 3-byte integer`
    2<sup>24</sup> | 2<sup>64</sup> | `0xFE + 8-byte integer`

   Similarly, to convert a length-encoded integer into its numeric value
   check the first byte.

   @warning
   If the first byte of a packet is a length-encoded integer and
   its byte value is `0xFE`, you must check the length of the packet to
   verify that it has enough space for a 8-byte integer.
   If not, it may be an EOF_Packet instead.
*/

/**
  @page page_protocol_basic_dt_strings String Types

  Strings are sequences of bytes and appear in a few forms in the protocol.

  @section sect_protocol_basic_dt_string_fix Protocol::FixedLengthString

  Fixed-length strings have a known, hardcoded length.

  An example is the sql-state of the @ref page_protocol_basic_err_packet
  which is always 5 bytes long.

  @section sect_protocol_basic_dt_string_null Protocol::NullTerminatedString

  Strings that are terminated by a `00` byte.

  @section sect_protocol_basic_dt_string_var Protocol::VariableLengthString

  The length of the string is determined by another field or is calculated
  at runtime

  @section sect_protocol_basic_dt_string_le Protocol::LengthEncodedString

  A length encoded string is a string that is prefixed with length encoded
  integer describing the length of the string.

  It is a special case of @ref sect_protocol_basic_dt_string_var

  @section sect_protocol_basic_dt_string_eof Protocol::RestOfPacketString

  If a string is the last component of a packet, its length can be calculated
  from the overall packet length minus the current position.
*/

/**
  @page page_protocol_basic_response_packets Generic Response Packets

  For most commands the client sends to the server, the server returns one
  of these packets in response:
  - @subpage page_protocol_basic_ok_packet
  - @subpage page_protocol_basic_err_packet
  - @subpage page_protocol_basic_eof_packet
*/

/**
@page page_protocol_command_phase %Command Phase

@todo Document it.
*/

/**
  @page page_protocol_connection_lifecycle Connection Lifecycle

  The MySQL protocol is a stateful protocol. When a connection is established
  the server initiates a \ref page_protocol_connection_phase. Once that is
  performed the connection enters the \ref page_protocol_command_phase. The
  \ref page_protocol_command_phase ends when the connection terminates.

  Further reading:
  - @subpage page_protocol_connection_phase
  - @subpage page_protocol_command_phase
*/

/**
  @page page_protocol_basic_character_set Character Set

  MySQL has a very flexible character set support as documented in
  [Character Set Support](http://dev.mysql.com/doc/refman/5.7/en/charset.html).
  The list of character sets and their IDs can be queried as follows:

<pre>
  SELECT id, collation_name FROM information_schema.collations ORDER BY id;
  +----+-------------------+
  | id | collation_name    |
  +----+-------------------+
  |  1 | big5_chinese_ci   |
  |  2 | latin2_czech_cs   |
  |  3 | dec8_swedish_ci   |
  |  4 | cp850_general_ci  |
  |  5 | latin1_german1_ci |
  |  6 | hp8_english_ci    |
  |  7 | koi8r_general_ci  |
  |  8 | latin1_swedish_ci |
  |  9 | latin2_general_ci |
  | 10 | swe7_swedish_ci   |
  +----+-------------------+
</pre>

  The following table shows a few common character sets.

  Number |  Hex  | Character Set Name
  -------|-------|-------------------
       8 |  0x08 | @ref my_charset_latin1 "latin1_swedish_ci"
      33 |  0x21 | @ref my_charset_utf8_general_ci "utf8_general_ci"
      63 |  0x3f | @ref my_charset_bin "binary"


  @anchor a_protocol_character_set Protocol::CharacterSet
  ----------------------

  A character set is defined in the protocol as a integer.
  Fields:
     - charset_nr (2) -- number of the character set and collation
*/

/**
  @defgroup group_cs Client/Server Protocol

  Client/server protocol related structures,
  macros, globals and functions
*/


#include "protocol_classic.h"

#include <string.h>
#include <algorithm>

#include "decimal.h"
#include "field.h"
#include "item.h"
#include "item_func.h"                          // Item_func_set_user_var
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "my_time.h"
#include "mysql/com_data.h"
#include "mysql/psi/mysql_socket.h"
#include "mysqld.h"                             // global_system_variables
#include "mysqld_error.h"
#include "session_tracker.h"
#include "sql_class.h"                          // THD
#include "sql_error.h"
#include "sql_lex.h"
#include "sql_list.h"
#include "sql_prepare.h"                        // Prepared_statement
#include "system_variables.h"


using std::min;
using std::max;

static const unsigned int PACKET_BUFFER_EXTRA_ALLOC= 1024;
static bool net_send_error_packet(NET *, uint, const char *, const char *, bool,
                                  ulong, const CHARSET_INFO*);
static bool write_eof_packet(THD *, NET *, uint, uint);

ulong get_ps_param_len(enum enum_field_types type, uchar *packet,
                       ulong packet_len, ulong *header_len, bool *err);
bool Protocol_classic::net_store_data(const uchar *from, size_t length)
{
  size_t packet_length=packet->length();
  /*
     The +9 comes from that strings of length longer than 16M require
     9 bytes to be stored (see net_store_length).
  */
  if (packet_length+9+length > packet->alloced_length() &&
      packet->mem_realloc(packet_length+9+length))
    return 1;
  uchar *to= net_store_length((uchar *) packet->ptr()+packet_length, length);
  if (length > 0)
    memcpy(to,from,length);
  packet->length((uint) (to+length-(uchar *) packet->ptr()));
  return 0;
}


/**
  net_store_data() - extended version with character set conversion.
  
  It is optimized for short strings whose length after
  conversion is garanteed to be less than 251, which accupies
  exactly one byte to store length. It allows not to use
  the "convert" member as a temporary buffer, conversion
  is done directly to the "packet" member.
  The limit 251 is good enough to optimize send_result_set_metadata()
  because column, table, database names fit into this limit.
*/

bool Protocol_classic::net_store_data(const uchar *from, size_t length,
                                      const CHARSET_INFO *from_cs,
                                      const CHARSET_INFO *to_cs)
{
  uint dummy_errors;
  /* Calculate maxumum possible result length */
  size_t conv_length= to_cs->mbmaxlen * length / from_cs->mbminlen;
  if (conv_length > 250)
  {
    /*
      For strings with conv_length greater than 250 bytes
      we don't know how many bytes we will need to store length: one or two,
      because we don't know result length until conversion is done.
      For example, when converting from utf8 (mbmaxlen=3) to latin1,
      conv_length=300 means that the result length can vary between 100 to 300.
      length=100 needs one byte, length=300 needs to bytes.
      
      Thus conversion directly to "packet" is not worthy.
      Let's use "convert" as a temporary buffer.
    */
    return (convert->copy((const char *) from, length, from_cs,
        to_cs, &dummy_errors) ||
        net_store_data((const uchar *) convert->ptr(), convert->length()));
  }

  size_t packet_length= packet->length();
  size_t new_length= packet_length + conv_length + 1;

  if (new_length > packet->alloced_length() && packet->mem_realloc(new_length))
    return 1;

  char *length_pos= (char *) packet->ptr() + packet_length;
  char *to= length_pos + 1;

  to+= copy_and_convert(to, conv_length, to_cs,
      (const char *) from, length, from_cs, &dummy_errors);

  net_store_length((uchar *) length_pos, to - length_pos - 1);
  packet->length((uint) (to - packet->ptr()));
  return 0;
}


/**
  Send a error string to client.

  Design note:

  net_printf_error and net_send_error are low-level functions
  that shall be used only when a new connection is being
  established or at server startup.

  For SIGNAL/RESIGNAL and GET DIAGNOSTICS functionality it's
  critical that every error that can be intercepted is issued in one
  place only, my_message_sql.

  @param thd Thread handler
  @param sql_errno The error code to send
  @param err A pointer to the error message

  @return
    @retval FALSE The message was sent to the client
    @retval TRUE An error occurred and the message wasn't sent properly
*/

bool net_send_error(THD *thd, uint sql_errno, const char *err)
{
  bool error;
  DBUG_ENTER("net_send_error");

  DBUG_ASSERT(!thd->sp_runtime_ctx);
  DBUG_ASSERT(sql_errno);
  DBUG_ASSERT(err);

  DBUG_PRINT("enter",("sql_errno: %d  err: %s", sql_errno, err));

  /*
    It's one case when we can push an error even though there
    is an OK or EOF already.
  */
  thd->get_stmt_da()->set_overwrite_status(true);

  /* Abort multi-result sets */
  thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

  error= net_send_error_packet(thd, sql_errno, err,
    mysql_errno_to_sqlstate(sql_errno));

  thd->get_stmt_da()->set_overwrite_status(false);

  DBUG_RETURN(error);
}


/**
  Send a error string to client using net struct.
  This is used initial connection handling code.

  @param net        Low-level net struct
  @param sql_errno  The error code to send
  @param err        A pointer to the error message

  @return
    @retval FALSE The message was sent to the client
    @retval TRUE  An error occurred and the message wasn't sent properly
*/

bool net_send_error(NET *net, uint sql_errno, const char *err)
{
  DBUG_ENTER("net_send_error");

  DBUG_ASSERT(sql_errno && err);

  DBUG_PRINT("enter",("sql_errno: %d  err: %s", sql_errno, err));

  bool error=
    net_send_error_packet(net, sql_errno, err,
                          mysql_errno_to_sqlstate(sql_errno), false, 0,
                          global_system_variables.character_set_results);

  DBUG_RETURN(error);
}


/**
  @page page_protocol_basic_ok_packet OK_Packet

  An OK packet is sent from the server to the client to signal successful
  completion of a command. As of MySQL 5.7.5, OK packes are also used to
  indicate EOF, and EOF packets are deprecated.

  if ::CLIENT_PROTOCOL_41 is set, the packet contains a warning count.

  <table>
  <caption>The Payload of an OK Packet</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>header</td>
      <td>`0x00` or `0xFE` the OK packet header</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_int_le "int&lt;lenenc&gt;"</td>
      <td>affected_rows</td>
      <td>affected rows</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_int_le "int&lt;lenenc&gt;"</td>
      <td>last_insert_id</td>
      <td>last insert-id</td></tr>
  <tr><td colspan="3">if capabilities @& ::CLIENT_PROTOCOL_41 {</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>status_flags</td>
      <td>@ref SERVER_STATUS_flags_enum</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>warnings</td>
      <td>number of warnings</td></tr>
  <tr><td colspan="3">} else if capabilities @& ::CLIENT_TRANSACTIONS {</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>status_flags</td>
      <td>@ref SERVER_STATUS_flags_enum</td></tr>
  <tr><td colspan="3">}</td></tr>
  <tr><td colspan="3">if capabilities @& ::CLIENT_SESSION_TRACK</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_le "string&lt;lenenc&gt;"</td>
      <td>info</td>
      <td>human readable status information</td></tr>
  <tr><td colspan="3">  if status_flags @& ::SERVER_SESSION_STATE_CHANGED {</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_le "string&lt;lenenc&gt;"</td>
      <td>session state info</td>
      <td>@anchor a_protocol_basic_ok_packet_sessinfo
          @ref sect_protocol_basic_ok_packet_sessinfo</td></tr>
  <tr><td colspan="3">  }</td></tr>
  <tr><td colspan="3">} else {</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_eof "string&lt;EOF&gt;"</td>
      <td>info</td>
      <td>human readable status information</td></tr>
  <tr><td colspan="3">}</td></tr>
  </table>

  These rules distinguish whether the packet represents OK or EOF:
  - OK: header = 0 and length of packet > 7
  - EOF: header = 0xfe and length of packet < 9

  To ensure backward compatibility between old (prior to 5.7.5) and
  new (5.7.5 and up) versions of MySQL, new clients advertise
  the ::CLIENT_DEPRECATE_EOF flag:
  - Old clients do not know about this flag and do not advertise it.
    Consequently, the server does not send OK packets that represent EOF.
    (Old servers never do this, anyway. New servers recognize the absence
    of the flag to mean they should not.)
  - New clients advertise this flag. Old servers do not know this flag and
    do not send OK packets that represent EOF. New servers recognize the flag
    and can send OK packets that represent EOF.

  Example
  =======

  OK with ::CLIENT_PROTOCOL_41. 0 affected rows, last-insert-id was 0,
  AUTOCOMMIT enabled, 0 warnings. No further info.

  ~~~~~~~~~~~~~~~~~~~~~
  07 00 00 02 00 00 00 02    00 00 00
  ~~~~~~~~~~~~~~~~~~~~~

  @section sect_protocol_basic_ok_packet_sessinfo Session State Information

  State-change information is sent in the OK packet as a array of state-change
  blocks which are made up of:

  <table>
  <caption>Layout of Session State Information</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>type</td>
      <td>type of data. See enum_session_state_type</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_le "string&lt;lenenc&gt;"</td>
      <td>data</td>
      <td>data of the changed session info</td></tr>
  </table>

  Interpretation of the data field depends on the type value:

  @subsection sect_protocol_basic_ok_packet_sessinfo_SESSION_TRACK_SYSTEM_VARIABLES SESSION_TRACK_SYSTEM_VARIABLES

  <table>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_le "string&lt;lenenc&gt;"</td>
      <td>name</td>
      <td>name of the changed system variable</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_le "string&lt;lenenc&gt;"</td>
      <td>value</td>
      <td>value of the changed system variable</td></tr>
  </table>

  Example:

  After a SET autocommit = OFF statement:
  <table><tr>
  <td>
  ~~~~~~~~~~~~~~~~~~~~~
  00 0f1 0a 61 75 74 6f 63   6f 6d 6d 69 74 03 4f 46 46
  ~~~~~~~~~~~~~~~~~~~~~
  </td><td>
  ~~~~~~~~~~~~~~~~~~~~~
  ....autocommit.OFF
  ~~~~~~~~~~~~~~~~~~~~~
  </td></tr></table>

  @subsection sect_protocol_basic_ok_packet_sessinfo_SESSION_TRACK_SCHEMA SESSION_TRACK_SCHEMA

  <table>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_le "string&lt;lenenc&gt;"</td>
      <td>name</td>
      <td>name of the changed system variable</td></tr>
  </table>

  Example:

  After a USE test statement:

  <table><tr>
  <td>
  ~~~~~~~~~~~~~~~~~~~~~
  01 05 04 74 65 73 74
  ~~~~~~~~~~~~~~~~~~~~~
  </td><td>
  ~~~~~~~~~~~~~~~~~~~~~
  ...test
  ~~~~~~~~~~~~~~~~~~~~~
  </td></tr></table>

  @subsection sect_protocol_basic_ok_packet_sessinfo_SESSION_TRACK_STATE_CHANGE SESSION_TRACK_STATE_CHANGE

  A flag byte that indicates whether session state changes occurred.
  This flag is represented as an ASCII value.

  <table>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_le "string&lt;lenenc&gt;"</td>
  <td>is_tracked</td>
  <td>`0x31` ("1") if state tracking got enabled.</td></tr>
  </table>

  Example:

  After a SET SESSION session_track_state_change = 1 statement:

  <table><tr>
  <td>
  ~~~~~~~~~~~~~~~~~~~~~
  03 02 01 31
  ~~~~~~~~~~~~~~~~~~~~~
  </td><td>
  ~~~~~~~~~~~~~~~~~~~~~
  ...1
  ~~~~~~~~~~~~~~~~~~~~~
  </td></tr></table>

  See also net_send_ok()
*/

/**
  Return OK to the client.

  See @ref page_protocol_basic_ok_packet for the OK packet structure.

  @param thd                     Thread handler
  @param server_status           The server status
  @param statement_warn_count    Total number of warnings
  @param affected_rows           Number of rows changed by statement
  @param id                      Auto_increment id for first row (if used)
  @param message                 Message to send to the client
                                 (Used by mysql_status)
  @param eof_identifier          when true [FE] will be set in OK header
                                 else [00] will be used

  @return
    @retval FALSE The message was successfully sent
    @retval TRUE An error occurred and the messages wasn't sent properly
*/

bool
net_send_ok(THD *thd,
            uint server_status, uint statement_warn_count,
            ulonglong affected_rows, ulonglong id, const char *message,
            bool eof_identifier)
{
  Protocol *protocol= thd->get_protocol();
  NET *net= thd->get_protocol_classic()->get_net();
  uchar buff[MYSQL_ERRMSG_SIZE + 10];
  uchar *pos, *start;

  /*
    To be used to manage the data storage in case session state change
    information is present.
  */
  String store;
  bool state_changed= false;

  bool error= FALSE;
  DBUG_ENTER("net_send_ok");

  if (! net->vio)	// hack for re-parsing queries
  {
    DBUG_PRINT("info", ("vio present: NO"));
    DBUG_RETURN(FALSE);
  }

  start= buff;

  /*
    Use 0xFE packet header if eof_identifier is true
    unless we are talking to old client
  */
  if (eof_identifier &&
      (protocol->has_client_capability(CLIENT_DEPRECATE_EOF)))
    buff[0]= 254;
  else
    buff[0]= 0;

  /* affected rows */
  pos= net_store_length(buff + 1, affected_rows);

  /* last insert id */
  pos= net_store_length(pos, id);

  if (protocol->has_client_capability(CLIENT_SESSION_TRACK) &&
      thd->session_tracker.enabled_any() &&
      thd->session_tracker.changed_any())
  {
    server_status |= SERVER_SESSION_STATE_CHANGED;
    state_changed= true;
  }

  if (protocol->has_client_capability(CLIENT_PROTOCOL_41))
  {
    DBUG_PRINT("info",
        ("affected_rows: %lu  id: %lu  status: %u  warning_count: %u",
            (ulong) affected_rows,
            (ulong) id,
            (uint) (server_status & 0xffff),
            (uint) statement_warn_count));
    /* server status */
    int2store(pos, server_status);
    pos+= 2;

    /* warning count: we can only return up to 65535 warnings in two bytes. */
    uint tmp= min(statement_warn_count, 65535U);
    int2store(pos, tmp);
    pos+= 2;
  }
  else if (net->return_status)			// For 4.0 protocol
  {
    int2store(pos, server_status);
    pos+=2;
  }

  thd->get_stmt_da()->set_overwrite_status(true);

  if (protocol->has_client_capability(CLIENT_SESSION_TRACK))
  {
    /* the info field */
    if (state_changed || (message && message[0]))
      pos= net_store_data(pos, (uchar*) message, message ? strlen(message) : 0);
    /* session state change information */
    if (unlikely(state_changed))
    {
      store.set_charset(thd->variables.collation_database);

      /*
        First append the fields collected so far. In case of malloc, memory
        for message is also allocated here.
      */
      store.append((const char *)start, (pos - start), MYSQL_ERRMSG_SIZE);

      /* .. and then the state change information. */
      thd->session_tracker.store(thd, store);

      start= (uchar *) store.ptr();
      pos= start+store.length();
    }
  }
  else if (message && message[0])
  {
    /* the info field, if there is a message to store */
    pos= net_store_data(pos, (uchar*) message, strlen(message));
  }

  /* OK packet length will be restricted to 16777215 bytes */
  if (((size_t) (pos - start)) > MAX_PACKET_LENGTH)
  {
    net->error= 1;
    net->last_errno= ER_NET_OK_PACKET_TOO_LARGE;
    my_error(ER_NET_OK_PACKET_TOO_LARGE, MYF(0));
    DBUG_PRINT("info", ("OK packet too large"));
    DBUG_RETURN(1);
  }
  error= my_net_write(net, start, (size_t) (pos - start));
  if (!error)
    error= net_flush(net);

  thd->get_stmt_da()->set_overwrite_status(false);
  DBUG_PRINT("info", ("OK sent, so no more error sending allowed"));

  DBUG_RETURN(error);
}

static uchar eof_buff[1]= { (uchar) 254 };      /* Marker for end of fields */


/**
  @page page_protocol_basic_eof_packet EOF_Packet

  If ::CLIENT_PROTOCOL_41 is enabled, the EOF packet contains a
  warning count and status flags.

  @note
  In the MySQL client/server protocol, the
  @ref page_protocol_basic_eof_packet and
  @ref page_protocol_basic_ok_packet packets serve
  the same purpose, to mark the end of a query execution result.
  Due to changes in MySQL 5.7 in
  the @ref page_protocol_basic_ok_packet packets (such as session
  state tracking), and to avoid repeating the changes in
  the @ref page_protocol_basic_eof_packet packet, the
  @ref page_protocol_basic_ok_packet is deprecated as of MySQL 5.7.5.

  @warning
  The @ref page_protocol_basic_eof_packet packet may appear in places where
  a @ref sect_protocol_basic_dt_int_le "Protocol::LengthEncodedInteger"
  may appear. You must check whether the packet length is less than 9 to
  make sure that it is a @ref page_protocol_basic_eof_packet packet.

  <table>
  <caption>The Payload of an EOF Packet</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>header</td>
      <td>`0xFE` EOF packet header</td></tr>
  <tr><td colspan="3">if capabilities @& ::CLIENT_PROTOCOL_41 {</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>warnings</td>
      <td>number of warnings</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>status_flags</td>
      <td>@ref SERVER_STATUS_flags_enum</td></tr>
  </table>

  Example:

  A MySQL 4.1 EOF packet with: 0 warnings, AUTOCOMMIT enabled.

  <table><tr>
  <td>
  ~~~~~~~~~~~~~~~~~~~~~
  05 00 00 05 fe 00 00 02 00
  ~~~~~~~~~~~~~~~~~~~~~
  </td><td>
  ~~~~~~~~~~~~~~~~~~~~~
  ..........
  ~~~~~~~~~~~~~~~~~~~~~
  </td></tr></table>

  @sa net_send_eof().
*/

/**
  Send eof (= end of result set) to the client.

  See @ref page_protocol_basic_eof_packet packet for the structure
  of the packet.

  note
  The warning count will not be sent if 'no_flush' is set as
  we don't want to report the warning count until all data is sent to the
  client.

  @param thd                    Thread handler
  @param server_status          The server status
  @param statement_warn_count   Total number of warnings

  @return
    @retval FALSE The message was successfully sent
    @retval TRUE An error occurred and the message wasn't sent properly
*/

bool
net_send_eof(THD *thd, uint server_status, uint statement_warn_count)
{
  NET *net= thd->get_protocol_classic()->get_net();
  bool error= FALSE;
  DBUG_ENTER("net_send_eof");
  /* Set to TRUE if no active vio, to work well in case of --init-file */
  if (net->vio != 0)
  {
    thd->get_stmt_da()->set_overwrite_status(true);
    error= write_eof_packet(thd, net, server_status, statement_warn_count);
    if (!error)
      error= net_flush(net);
    thd->get_stmt_da()->set_overwrite_status(false);
    DBUG_PRINT("info", ("EOF sent, so no more error sending allowed"));
  }
  DBUG_RETURN(error);
}


/**
  Format EOF packet according to the current protocol and
  write it to the network output buffer.

  See also @ref page_protocol_basic_err_packet

  @param thd The thread handler
  @param net The network handler
  @param server_status The server status
  @param statement_warn_count The number of warnings


  @return
    @retval FALSE The message was sent successfully
    @retval TRUE An error occurred and the messages wasn't sent properly
*/

static bool write_eof_packet(THD *thd, NET *net,
                             uint server_status,
                             uint statement_warn_count)
{
  bool error;
  Protocol *protocol= thd->get_protocol();
  if (protocol->has_client_capability(CLIENT_PROTOCOL_41))
  {
    uchar buff[5];
    /*
      Don't send warn count during SP execution, as the warn_list
      is cleared between substatements, and mysqltest gets confused
    */
    uint tmp= min(statement_warn_count, 65535U);
    buff[0]= 254;
    int2store(buff+1, tmp);
    /*
      The following test should never be true, but it's better to do it
      because if 'is_fatal_error' is set the server is not going to execute
      other queries (see the if test in dispatch_command / COM_QUERY)
    */
    if (thd->is_fatal_error)
      server_status&= ~SERVER_MORE_RESULTS_EXISTS;
    int2store(buff + 3, server_status);
    error= my_net_write(net, buff, 5);
  }
  else
    error= my_net_write(net, eof_buff, 1);

  return error;
}


/**
  @page page_protocol_basic_err_packet ERR_Packet

  This packet signals that an error occurred. It contains a SQL state value
  if ::CLIENT_PROTOCOL_41 is enabled.

  Error texts cannot exceed ::MYSQL_ERRMSG_SIZE

  <table>
  <caption>The Payload of an ERR Packet</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
  <td>header</td>
  <td>`0xFF` ERR packet header</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
  <td>error_code</td>
  <td>error-code</td></tr>
  <tr><td colspan="3">if capabilities @& ::CLIENT_PROTOCOL_41 {</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_fix "string[1]"</td>
  <td>sql_state_marker</td>
  <td># marker of the SQL state</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_fix "string[5]"</td>
  <td>sql_state</td>
  <td>SQL state</td></tr>
  <tr><td colspan="3">  }</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_eof "string&lt;EOF&gt;"</td>
  <td>error_message</td>
  <td>human readable error message</td></tr>
  </table>

  Example:

  <table><tr>
  <td>
  ~~~~~~~~~~~~~~~~~~~~~
  17 00 00 01 ff 48 04 23    48 59 30 30 30 4e 6f 20
  74 61 62 6c 65 73 20 75    73 65 64
  ~~~~~~~~~~~~~~~~~~~~~
  </td><td>
  ~~~~~~~~~~~~~~~~~~~~~
  .....H.#HY000No
  tables used
  ~~~~~~~~~~~~~~~~~~~~~
  </td></tr></table>

  @sa net_send_error_packet()
*/


/**
  @param thd          Thread handler
  @param sql_errno    The error code to send
  @param err          A pointer to the error message
  @param sqlstate     SQL state

  @return
   @retval FALSE The message was successfully sent
   @retval TRUE  An error occurred and the messages wasn't sent properly

  See also @ref page_protocol_basic_err_packet
*/

bool net_send_error_packet(THD *thd, uint sql_errno, const char *err,
                           const char* sqlstate)
{
  return net_send_error_packet(thd->get_protocol_classic()->get_net(),
                               sql_errno, err, sqlstate,
                               thd->is_bootstrap_system_thread(),
                               thd->get_protocol()->get_client_capabilities(),
                               thd->variables.character_set_results);
}


/**
  @param net                    Low-level NET struct
  @param sql_errno              The error code to send
  @param err                    A pointer to the error message
  @param sqlstate               SQL state
  @param bootstrap              Server is started in bootstrap mode
  @param client_capabilities    Client capabilities flag
  @param character_set_results  Char set info

  @return
   @retval FALSE The message was successfully sent
   @retval TRUE  An error occurred and the messages wasn't sent properly

  See also @ref page_protocol_basic_err_packet
*/

static bool net_send_error_packet(NET* net, uint sql_errno, const char *err,
                                  const char* sqlstate, bool bootstrap,
                                  ulong client_capabilities,
                                  const CHARSET_INFO* character_set_results)
{
  uint length;
  /*
    buff[]: sql_errno:2 + ('#':1 + SQLSTATE_LENGTH:5) + MYSQL_ERRMSG_SIZE:512
  */
  uint error;
  char converted_err[MYSQL_ERRMSG_SIZE];
  char buff[2+1+SQLSTATE_LENGTH+MYSQL_ERRMSG_SIZE], *pos;

  DBUG_ENTER("net_send_error_packet");

  if (net->vio == 0)
  {
    if (bootstrap)
    {
      /* In bootstrap it's ok to print on stderr */
      my_message_local(ERROR_LEVEL, "%d  %s", sql_errno, err);
    }
    DBUG_RETURN(FALSE);
  }

  int2store(buff,sql_errno);
  pos= buff+2;
  if (client_capabilities & CLIENT_PROTOCOL_41)
  {
    /* The first # is to make the protocol backward compatible */
    buff[2]= '#';
    pos= my_stpcpy(buff+3, sqlstate);
  }

  convert_error_message(converted_err, sizeof(converted_err),
                        character_set_results, err,
                        strlen(err), system_charset_info, &error);
  /* Converted error message is always null-terminated. */
  length= (uint) (strmake(pos, converted_err, MYSQL_ERRMSG_SIZE - 1) - buff);

  DBUG_RETURN(net_write_command(net,(uchar) 255, (uchar *) "", 0,
              (uchar *) buff, length));
}


/**
  Faster net_store_length when we know that length is less than 65536.
  We keep a separate version for that range because it's widely used in
  libmysql.

  uint is used as agrument type because of MySQL type conventions:
    - uint for 0..65536
    - ulong for 0..4294967296
    - ulonglong for bigger numbers.
*/

static uchar *net_store_length_fast(uchar *packet, size_t length)
{
  if (length < 251)
  {
    *packet=(uchar) length;
    return packet+1;
  }
  *packet++=252;
  int2store(packet,(uint) length);
  return packet+2;
}


/****************************************************************************
  Functions used by the protocol functions (like net_send_ok) to store
  strings and numbers in the header result packet.
****************************************************************************/

/* The following will only be used for short strings < 65K */

uchar *net_store_data(uchar *to, const uchar *from, size_t length)
{
  to=net_store_length_fast(to,length);
  if (length > 0)
    memcpy(to,from,length);
  return to+length;
}


uchar *net_store_data(uchar *to, int32 from)
{
  char buff[20];
  uint length= (uint)(int10_to_str(from, buff, 10) - buff);
  to= net_store_length_fast(to, length);
  memcpy(to, buff, length);
  return to + length;
}


uchar *net_store_data(uchar *to, longlong from)
{
  char buff[22];
  uint length= (uint)(longlong10_to_str(from, buff, 10) - buff);
  to= net_store_length_fast(to, length);
  memcpy(to, buff, length);
  return to + length;
}


/*****************************************************************************
  Protocol_classic functions
*****************************************************************************/

void Protocol_classic::init(THD *thd_arg)
{
  m_thd= thd_arg;
  packet= &m_thd->packet;
  convert= &m_thd->convert_buffer;
#ifndef DBUG_OFF
  field_types= 0;
#endif
}


/**
  A default implementation of "OK" packet response to the client.

  Currently this implementation is re-used by both network-oriented
  protocols -- the binary and text one. They do not differ
  in their OK packet format, which allows for a significant simplification
  on client side.
*/

bool
Protocol_classic::send_ok(uint server_status, uint statement_warn_count,
                          ulonglong affected_rows, ulonglong last_insert_id,
                          const char *message)
{
  DBUG_ENTER("Protocol_classic::send_ok");
  const bool retval=
    net_send_ok(m_thd, server_status, statement_warn_count,
                affected_rows, last_insert_id, message, false);
  DBUG_RETURN(retval);
}


/**
  A default implementation of "EOF" packet response to the client.

  Binary and text protocol do not differ in their EOF packet format.
*/

bool Protocol_classic::send_eof(uint server_status, uint statement_warn_count)
{
  DBUG_ENTER("Protocol_classic::send_eof");
  bool retval;
  /*
    Normally end of statement reply is signaled by OK packet, but in case
    of binlog dump request an EOF packet is sent instead. Also, old clients
    expect EOF packet instead of OK
  */
  if (has_client_capability(CLIENT_DEPRECATE_EOF) &&
      (m_thd->get_command() != COM_BINLOG_DUMP &&
       m_thd->get_command() != COM_BINLOG_DUMP_GTID))
    retval= net_send_ok(m_thd, server_status, statement_warn_count, 0, 0, NULL,
                        true);
  else
    retval= net_send_eof(m_thd, server_status, statement_warn_count);
  DBUG_RETURN(retval);
}


/**
  A default implementation of "ERROR" packet response to the client.

  Binary and text protocol do not differ in ERROR packet format.
*/

bool Protocol_classic::send_error(uint sql_errno, const char *err_msg,
                                  const char *sql_state)
{
  DBUG_ENTER("Protocol_classic::send_error");
  const bool retval= net_send_error_packet(m_thd, sql_errno, err_msg, sql_state);
  DBUG_RETURN(retval);
}


void Protocol_classic::set_read_timeout(ulong read_timeout)
{
  my_net_set_read_timeout(&m_thd->net, read_timeout);
}


void Protocol_classic::set_write_timeout(ulong write_timeout)
{
  my_net_set_write_timeout(&m_thd->net, write_timeout);
}


// NET interaction functions
bool Protocol_classic::init_net(Vio *vio)
{
  return my_net_init(&m_thd->net, vio);
}

void Protocol_classic::claim_memory_ownership()
{
  net_claim_memory_ownership(&m_thd->net);
}

void Protocol_classic::end_net()
{
  DBUG_ASSERT(m_thd->net.buff);
  net_end(&m_thd->net);
  m_thd->net.vio= NULL;
}


bool Protocol_classic::write(const uchar *ptr, size_t len)
{
  return my_net_write(&m_thd->net, ptr, len);
}


uchar Protocol_classic::get_error()
{
  return m_thd->net.error;
}


void Protocol_classic::wipe_net()
{
  memset(&m_thd->net, 0, sizeof(m_thd->net));
}


void Protocol_classic::set_max_packet_size(ulong max_packet_size)
{
  m_thd->net.max_packet_size= max_packet_size;
}


NET *Protocol_classic::get_net()
{
  return &m_thd->net;
}


Vio *Protocol_classic::get_vio()
{
  return m_thd->net.vio;
}

void Protocol_classic::set_vio(Vio *vio)
{
  m_thd->net.vio= vio;
}


void Protocol_classic::set_output_pkt_nr(uint pkt_nr)
{
  m_thd->net.pkt_nr= pkt_nr;
}


uint Protocol_classic::get_output_pkt_nr()
{
  return m_thd->net.pkt_nr;
}


String *Protocol_classic::get_output_packet()
{
  return &m_thd->packet;
}


int Protocol_classic::read_packet()
{
  int ret;
  if ((input_packet_length= my_net_read(&m_thd->net)) &&
       input_packet_length != packet_error)
  {
    DBUG_ASSERT(!m_thd->net.error);
    bad_packet= false;
    input_raw_packet= m_thd->net.read_pos;
    return 0;
  }
  else if (m_thd->net.error == 3)
    ret= 1;
  else
    ret= -1;
  bad_packet= true;
  return ret;
}


bool Protocol_classic::parse_packet(union COM_DATA *data,
                                    enum_server_command cmd)
{
  DBUG_ENTER("Protocol_classic::parse_packet");
  switch(cmd)
  {
  case COM_INIT_DB:
  {
    data->com_init_db.db_name= reinterpret_cast<const char*>(input_raw_packet);
    data->com_init_db.length= input_packet_length;
    break;
  }
  case COM_REFRESH:
  {
    if (input_packet_length < 1)
      goto malformed;
    data->com_refresh.options= input_raw_packet[0];
    break;
  }
  case COM_PROCESS_KILL:
  {
    if (input_packet_length < 4)
      goto malformed;
    data->com_kill.id= (ulong) uint4korr(input_raw_packet);
    break;
  }
  case COM_SET_OPTION:
  {
    if (input_packet_length < 2)
      goto malformed;
    data->com_set_option.opt_command= uint2korr(input_raw_packet);
    break;
  }
  case COM_STMT_EXECUTE:
  {
    if (input_packet_length < 9)
      goto malformed;
    uchar *read_pos= input_raw_packet;
    size_t packet_left= input_packet_length;

    // Get the statement id
    data->com_stmt_execute.stmt_id= uint4korr(read_pos);
    read_pos+= 4;
    packet_left-= 4;
    // Get execution flags
    data->com_stmt_execute.open_cursor= static_cast<bool>(*read_pos);
    read_pos+= 5;
    packet_left-= 5;
    DBUG_PRINT("info", ("stmt %lu", data->com_stmt_execute.stmt_id));
    DBUG_PRINT("info", ("Flags %lu", data->com_stmt_execute.open_cursor));

    // Get the statement by id
    Prepared_statement *stmt=
      m_thd->stmt_map.find(data->com_stmt_execute.stmt_id);
    data->com_stmt_execute.parameter_count= 0;

    /*
      If no statement found there's no need to generate error.
      It will be generated in sql_parse.cc which will check again for the id.
    */
    if (!stmt || stmt->param_count < 1)
      break;

    uint param_count= stmt->param_count;
    data->com_stmt_execute.parameters=
        static_cast<PS_PARAM *>(m_thd->alloc(param_count * sizeof(PS_PARAM)));
    if (!data->com_stmt_execute.parameters)
      goto malformed;              /* purecov: inspected */

    /* Then comes the null bits */
    const uint null_bits_packet_len= (param_count + 7) / 8;
    if (packet_left < null_bits_packet_len)
      goto malformed;
    unsigned char *null_bits= read_pos;
    read_pos += null_bits_packet_len;
    packet_left-= null_bits_packet_len;

    PS_PARAM *params= data->com_stmt_execute.parameters;

    /* Then comes the types byte. If set, new types are provided */
    bool has_new_types= static_cast<bool>(*read_pos++);
    data->com_stmt_execute.has_new_types= has_new_types;
    if (has_new_types)
    {
      DBUG_PRINT("info", ("Types provided"));
      --packet_left;

      for (uint i= 0; i < param_count; ++i)
      {
        if (packet_left < 2)
          goto malformed;

        ushort type_code= sint2korr(read_pos);
        read_pos+= 2;
        packet_left-= 2;

        const uint signed_bit= 1 << 15;
        params[i].type=
          static_cast<enum enum_field_types>(type_code & ~signed_bit);
        params[i].unsigned_type= static_cast<bool>(type_code & signed_bit);
        DBUG_PRINT("info", ("type=%u", (uint) params[i].type));
        DBUG_PRINT("info", ("flags=%u", (uint) params[i].unsigned_type));
      }
    }
    /*
      No check for packet_left here or in case of only long data
      we will return malformed, although the packet will be correct
    */

    /* Here comes the real data */
    for (uint i= 0; i < param_count; ++i)
    {
      params[i].null_bit=
          static_cast<bool> (null_bits[i / 8] & (1 << (i & 7)));
      // Check if parameter is null
      if (params[i].null_bit)
      {
        DBUG_PRINT("info", ("null param"));
        params[i].value= nullptr;
        params[i].length= 0;
        data->com_stmt_execute.parameter_count++;
        continue;
      }
      enum enum_field_types type= has_new_types ? params[i].type :
                                  stmt->param_array[i]->data_type();
      if (stmt->param_array[i]->state == Item_param::LONG_DATA_VALUE)
      {
        DBUG_PRINT("info", ("long data"));
        if (!((type >= MYSQL_TYPE_TINY_BLOB) && (type <= MYSQL_TYPE_STRING)))
          goto malformed;
        data->com_stmt_execute.parameter_count++;

        continue;
      }

      bool buffer_underrun= false;
      ulong header_len;

      // Set parameter length.
      params[i].length= get_ps_param_len(type, read_pos, packet_left,
                                         &header_len, &buffer_underrun);
      if (buffer_underrun)
        goto malformed;

      // Set parameter value
      params[i].value= header_len + read_pos;
      read_pos+= (header_len + params[i].length);
      packet_left-= (header_len + params[i].length);
      data->com_stmt_execute.parameter_count++;
      DBUG_PRINT("info", ("param len %ul", (uint) params[i].length));
    }
    DBUG_PRINT("info", ("param count %ul",
                        (uint) data->com_stmt_execute.parameter_count));
    break;
  }
  case COM_STMT_FETCH:
  {
    if (input_packet_length < 8)
      goto malformed;
    data->com_stmt_fetch.stmt_id= uint4korr(input_raw_packet);
    data->com_stmt_fetch.num_rows= uint4korr(input_raw_packet + 4);
    break;
  }
  case COM_STMT_SEND_LONG_DATA:
  {
    if (input_packet_length < MYSQL_LONG_DATA_HEADER)
      goto malformed;
    data->com_stmt_send_long_data.stmt_id= uint4korr(input_raw_packet);
    data->com_stmt_send_long_data.param_number=
      uint2korr(input_raw_packet + 4);
    data->com_stmt_send_long_data.longdata= input_raw_packet + 6;
    data->com_stmt_send_long_data.length= input_packet_length - 6;
    break;
  }
  case COM_STMT_PREPARE:
  {
    data->com_stmt_prepare.query=
      reinterpret_cast<const char*>(input_raw_packet);
    data->com_stmt_prepare.length= input_packet_length;
    break;
  }
  case COM_STMT_CLOSE:
  {
    if (input_packet_length < 4)
      goto malformed;

    data->com_stmt_close.stmt_id= uint4korr(input_raw_packet);
    break;
  }
  case COM_STMT_RESET:
  {
    if (input_packet_length < 4)
      goto malformed;

    data->com_stmt_reset.stmt_id= uint4korr(input_raw_packet);
    break;
  }
  case COM_QUERY:
  {
    data->com_query.query= reinterpret_cast<const char*>(input_raw_packet);
    data->com_query.length= input_packet_length;
    break;
  }
  case COM_FIELD_LIST:
  {
    /*
      We have name + wildcard in packet, separated by endzero
    */
    ulong len= strend((char *)input_raw_packet) - (char *)input_raw_packet;

    if (len >= input_packet_length || len > NAME_LEN)
      goto malformed;

    data->com_field_list.table_name= input_raw_packet;
    data->com_field_list.table_name_length= len;

    data->com_field_list.query= input_raw_packet + len + 1;
    data->com_field_list.query_length= input_packet_length - len;
    break;
  }
  default:
    break;
  }

  DBUG_RETURN(false);

malformed:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  bad_packet= true;
  DBUG_RETURN(true);
}

bool Protocol_classic::create_command(COM_DATA *com_data,
                                      enum_server_command cmd,
                                      uchar *pkt, size_t length)
{
  input_raw_packet= pkt;
  input_packet_length= length;

  return parse_packet(com_data, cmd);
}

int Protocol_classic::get_command(COM_DATA *com_data, enum_server_command *cmd)
{
  // read packet from the network
  if(int rc= read_packet())
    return rc;

  /*
    'input_packet_length' contains length of data, as it was stored in packet
    header. In case of malformed header, my_net_read returns zero.
    If input_packet_length is not zero, my_net_read ensures that the returned
    number of bytes was actually read from network.
    There is also an extra safety measure in my_net_read:
    it sets packet[input_packet_length]= 0, but only for non-zero packets.
  */
  if (input_packet_length == 0)                       /* safety */
  {
    /* Initialize with COM_SLEEP packet */
    input_raw_packet[0]= (uchar) COM_SLEEP;
    input_packet_length= 1;
  }
  /* Do not rely on my_net_read, extra safety against programming errors. */
  input_raw_packet[input_packet_length]= '\0';        /* safety */

  *cmd= (enum enum_server_command) (uchar) input_raw_packet[0];

  if (*cmd >= COM_END)
    *cmd= COM_END;				// Wrong command

  DBUG_ASSERT(input_packet_length);
  // Skip 'command'
  input_packet_length--;
  input_raw_packet++;

  return parse_packet(com_data, *cmd);
}

uint Protocol_classic::get_rw_status()
{
  return m_thd->net.reading_or_writing;
}

/**
  Finish the result set with EOF packet, as is expected by the client,
  if there is an error evaluating the next row and a continue handler
  for the error.
*/

void Protocol_classic::end_partial_result_set()
{
  net_send_eof(m_thd, m_thd->server_status,
               0 /* no warnings, we're inside SP */);
}


bool Protocol_classic::flush()
{
  return net_flush(&m_thd->net);
}


bool Protocol_classic::store_ps_status(ulong stmt_id, uint column_count,
                                       uint param_count, ulong cond_count)
{
  DBUG_ENTER("Protocol_classic::store_ps_status");

  uchar buff[12];
  buff[0]= 0;                                   /* OK packet indicator */
  int4store(buff + 1, stmt_id);
  int2store(buff + 5, column_count);
  int2store(buff + 7, param_count);
  buff[9]= 0;                                   // Guard against a 4.1 client
  uint16 tmp= min(static_cast<uint16>(cond_count),
                  std::numeric_limits<uint16>::max());
  int2store(buff + 10, tmp);

  DBUG_RETURN(my_net_write(&m_thd->net, buff, sizeof(buff)));
}


bool Protocol_classic::get_compression()
{
  return m_thd->net.compress;
}


bool
Protocol_classic::start_result_metadata(uint num_cols, uint flags,
                                        const CHARSET_INFO *cs)
{
  DBUG_ENTER("Protocol_classic::start_result_metadata");
  DBUG_PRINT("info", ("num_cols %u, flags %u", num_cols, flags));
  result_cs= (CHARSET_INFO *) cs;
  send_metadata= true;
  field_count= num_cols;
  sending_flags= flags;
  if (flags & Protocol::SEND_NUM_ROWS)
  {
    ulonglong tmp;
    uchar *pos = net_store_length((uchar *) &tmp, num_cols);
    my_net_write(&m_thd->net, (uchar *) &tmp, (size_t) (pos - ((uchar *) &tmp)));
  }
#ifndef DBUG_OFF
  field_types= (enum_field_types*) m_thd->alloc(sizeof(field_types) * num_cols);
  count= 0;
#endif

  DBUG_RETURN(false);
}


bool
Protocol_classic::end_result_metadata()
{
  DBUG_ENTER("Protocol_classic::end_result_metadata");
  DBUG_PRINT("info", ("num_cols %u, flags %u", field_count, sending_flags));
  send_metadata= false;
  if (sending_flags & SEND_EOF)
  {
    /* if it is new client do not send EOF packet */
    if (!(has_client_capability(CLIENT_DEPRECATE_EOF)))
    {
      /*
        Mark the end of meta-data result set, and store m_thd->server_status,
        to show that there is no cursor.
        Send no warning information, as it will be sent at statement end.
      */
      if (write_eof_packet(m_thd, &m_thd->net, m_thd->server_status,
            m_thd->get_stmt_da()->current_statement_cond_count()))
      {
        DBUG_RETURN(true);
      }
    }
  }
  DBUG_RETURN(false);
}


bool Protocol_classic::send_field_metadata(Send_field *field,
                                           const CHARSET_INFO *item_charset)
{
  DBUG_ENTER("Protocol_classic::send_field_metadata");
  char *pos;
  const CHARSET_INFO *cs= system_charset_info;
  const CHARSET_INFO *thd_charset= m_thd->variables.character_set_results;

  /* Keep things compatible for old clients */
  if (field->type == MYSQL_TYPE_VARCHAR)
    field->type= MYSQL_TYPE_VAR_STRING;

  send_metadata= true;
  if (has_client_capability(CLIENT_PROTOCOL_41))
  {
    if (store(STRING_WITH_LEN("def"), cs) ||
        store(field->db_name, strlen(field->db_name), cs) ||
        store(field->table_name, strlen(field->table_name), cs) ||
        store(field->org_table_name, strlen(field->org_table_name), cs) ||
        store(field->col_name, strlen(field->col_name), cs) ||
        store(field->org_col_name, strlen(field->org_col_name), cs) ||
        packet->mem_realloc(packet->length() + 12))
    {
      send_metadata= false;
      return true;
    }
    /* Store fixed length fields */
    pos= (char *) packet->ptr() + packet->length();
    *pos++= 12;        // Length of packed fields
    /* inject a NULL to test the client */
    DBUG_EXECUTE_IF("poison_rs_fields", pos[-1]= (char) 0xfb;);
    if (item_charset == &my_charset_bin || thd_charset == NULL)
    {
      /* No conversion */
      int2store(pos, item_charset->number);
      int4store(pos + 2, field->length);
    }
    else
    {
      /* With conversion */
      uint32 field_length, max_length;
      int2store(pos, thd_charset->number);
      /*
        For TEXT/BLOB columns, field_length describes the maximum data
        length in bytes. There is no limit to the number of characters
        that a TEXT column can store, as long as the data fits into
        the designated space.
        For the rest of textual columns, field_length is evaluated as
        char_count * mbmaxlen, where character count is taken from the
        definition of the column. In other words, the maximum number
        of characters here is limited by the column definition.

        When one has a LONG TEXT column with a single-byte
        character set, and the connection character set is multi-byte, the
        client may get fields longer than UINT_MAX32, due to
        <character set column> -> <character set connection> conversion.
        In that case column max length does not fit into the 4 bytes
        reserved for it in the protocol.
      */
      max_length= (field->type >= MYSQL_TYPE_TINY_BLOB &&
                   field->type <= MYSQL_TYPE_BLOB) ?
                   field->length / item_charset->mbminlen :
                   field->length / item_charset->mbmaxlen;
      field_length= char_to_byte_length_safe(max_length, thd_charset->mbmaxlen);
      int4store(pos + 2, field_length);
    }
    pos[6]= field->type;
    int2store(pos + 7, field->flags);
    pos[9]= (char) field->decimals;
    pos[10]= 0;        // For the future
    pos[11]= 0;        // For the future
    pos+= 12;
  }
  else
  {
    if (store(field->table_name, strlen(field->table_name), cs) ||
        store(field->col_name, strlen(field->col_name), cs) ||
        packet->mem_realloc(packet->length() + 10))
    {
      send_metadata= false;
      return true;
    }
    pos= (char *) packet->ptr() + packet->length();
    pos[0]= 3;
    int3store(pos + 1, field->length);
    pos[4]= 1;
    pos[5]= field->type;
    pos[6]= 3;
    int2store(pos + 7, field->flags);
    pos[9]= (char) field->decimals;
    pos+= 10;
  }
  packet->length((uint) (pos - packet->ptr()));

#ifndef DBUG_OFF
  // TODO: this should be protocol-dependent, as it records incorrect type
  // for binary protocol
  // Text protocol sends fields as varchar
  field_types[count++]= field->field ? MYSQL_TYPE_VAR_STRING : field->type;
#endif
  DBUG_RETURN(false);
}


bool Protocol_classic::end_row()
{
  DBUG_ENTER("Protocol_classic::end_row");
  if (m_thd->get_protocol()->connection_alive())
    DBUG_RETURN(my_net_write(&m_thd->net, (uchar *) packet->ptr(),
                             packet->length()));
  DBUG_RETURN(0);
}


/**
  Send a set of strings as one long string with ',' in between.
*/

bool store(Protocol *prot, I_List<i_string>* str_list)
{
  char buf[256];
  String tmp(buf, sizeof(buf), &my_charset_bin);
  size_t len;
  I_List_iterator<i_string> it(*str_list);
  i_string* s;

  tmp.length(0);
  while ((s=it++))
  {
    tmp.append(s->ptr);
    tmp.append(',');
  }
  if ((len= tmp.length()))
    len--;                         // Remove last ','
  return prot->store((char *) tmp.ptr(), len,  tmp.charset());
}

/****************************************************************************
  Functions to handle the simple (default) protocol where everything is
  This protocol is the one that is used by default between the MySQL server
  and client when you are not using prepared statements.

  All data are sent as 'packed-string-length' followed by 'string-data'
****************************************************************************/

bool Protocol_classic::connection_alive()
{
  return m_thd->net.vio != NULL;
}

void Protocol_text::start_row()
{
#ifndef DBUG_OFF
  field_pos= 0;
#endif
  packet->length(0);
}


bool Protocol_text::store_null()
{
#ifndef DBUG_OFF
  field_pos++;
#endif
  char buff[1];
  buff[0]= (char)251;
  return packet->append(buff, sizeof(buff), PACKET_BUFFER_EXTRA_ALLOC);
}


/**
  Auxilary function to convert string to the given character set
  and store in network buffer.
*/

bool Protocol_classic::store_string_aux(const char *from, size_t length,
                                        const CHARSET_INFO *fromcs,
                                        const CHARSET_INFO *tocs)
{
  /* 'tocs' is set 0 when client issues SET character_set_results=NULL */
  if (tocs && !my_charset_same(fromcs, tocs) &&
      fromcs != &my_charset_bin &&
      tocs != &my_charset_bin)
  {
    /* Store with conversion */
    return net_store_data((uchar *) from, length, fromcs, tocs);
  }
  /* Store without conversion */
  return net_store_data((uchar *) from, length);
}


SSL_handle Protocol_classic::get_ssl()
{
  return
#ifdef HAVE_OPENSSL
    m_thd->net.vio ? (SSL *) m_thd->net.vio->ssl_arg :
#endif
      NULL;
}


int Protocol_classic::shutdown(bool)
{
  return m_thd->net.vio ? vio_shutdown(m_thd->net.vio) : 0;
}


bool Protocol_text::store(const char *from, size_t length,
                          const CHARSET_INFO *fromcs,
                          const CHARSET_INFO *tocs)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
      field_types[field_pos] == MYSQL_TYPE_DECIMAL ||
      field_types[field_pos] == MYSQL_TYPE_BIT ||
      field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL ||
      field_types[field_pos] == MYSQL_TYPE_NEWDATE ||
      field_types[field_pos] == MYSQL_TYPE_JSON ||
      (field_types[field_pos] >= MYSQL_TYPE_ENUM &&
           field_types[field_pos] <= MYSQL_TYPE_GEOMETRY));
  if(!send_metadata) field_pos++;
#endif
  return store_string_aux(from, length, fromcs, tocs);
}


bool Protocol_text::store_tiny(longlong from)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
              field_types[field_pos] == MYSQL_TYPE_TINY);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((uchar *) buff,
    (size_t) (int10_to_str((int) from, buff, -10) - buff));
}


bool Protocol_text::store_short(longlong from)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_YEAR ||
    field_types[field_pos] == MYSQL_TYPE_SHORT);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((uchar *) buff,
    (size_t) (int10_to_str((int) from, buff, -10) - buff));
}


bool Protocol_text::store_long(longlong from)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_INT24 ||
    field_types[field_pos] == MYSQL_TYPE_LONG);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((uchar *) buff,
    (size_t) (int10_to_str((long int)from, buff,
                           (from < 0) ? -10 : 10) - buff));
}


bool Protocol_text::store_longlong(longlong from, bool unsigned_flag)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_LONGLONG);
  field_pos++;
#endif
  char buff[22];
  return net_store_data((uchar *) buff,
    (size_t) (longlong10_to_str(from, buff,
                                unsigned_flag ? 10 : -10)-
                                buff));
}


bool Protocol_text::store_decimal(const my_decimal *d, uint prec, uint dec)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL);
  field_pos++;
#endif
  char buff[DECIMAL_MAX_STR_LENGTH + 1];
  String str(buff, sizeof(buff), &my_charset_bin);
  (void) my_decimal2string(E_DEC_FATAL_ERROR, d, prec, dec, '0', &str);
  return net_store_data((uchar *) str.ptr(), str.length());
}


bool Protocol_text::store(float from, uint32 decimals, String *buffer)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_FLOAT);
  field_pos++;
#endif
  buffer->set_real((double) from, decimals, m_thd->charset());
  return net_store_data((uchar *) buffer->ptr(), buffer->length());
}


bool Protocol_text::store(double from, uint32 decimals, String *buffer)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_DOUBLE);
  field_pos++;
#endif
  buffer->set_real(from, decimals, m_thd->charset());
  return net_store_data((uchar *) buffer->ptr(), buffer->length());
}


bool Protocol_text::store(Proto_field *field)
{
  return field->send_text(this);
}


/**
  @todo
  Second_part format ("%06") needs to change when
  we support 0-6 decimals for time.
*/

bool Protocol_text::store(MYSQL_TIME *tm, uint decimals)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    is_temporal_type_with_date_and_time(field_types[field_pos]));
  field_pos++;
#endif
  char buff[MAX_DATE_STRING_REP_LENGTH];
  size_t length= my_datetime_to_str(tm, buff, decimals);
  return net_store_data((uchar *) buff, length);
}


bool Protocol_text::store_date(MYSQL_TIME *tm)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_DATE);
  field_pos++;
#endif
  char buff[MAX_DATE_STRING_REP_LENGTH];
  size_t length= my_date_to_str(tm, buff);
  return net_store_data((uchar *) buff, length);
}


bool Protocol_text::store_time(MYSQL_TIME *tm, uint decimals)
{
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(send_metadata || field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_TIME);
  field_pos++;
#endif
  char buff[MAX_DATE_STRING_REP_LENGTH];
  size_t length= my_time_to_str(tm, buff, decimals);
  return net_store_data((uchar *) buff, length);
}


/**
  Sends OUT-parameters by writing the values to the protocol.


  @param parameters       List of PS/SP parameters (both input and output).
  @param is_sql_prepare  If it's an sql prepare then
                         text protocol wil be used.

  @return Error status.
    @retval false Success.
    @retval true  Error.
*/
bool Protocol_binary::send_parameters(List<Item_param> *parameters,
                                      bool is_sql_prepare)
{
  if (is_sql_prepare)
    return Protocol_text::send_parameters(parameters, is_sql_prepare);

  List_iterator_fast<Item_param> item_param_it(*parameters);

  if (!has_client_capability(CLIENT_PS_MULTI_RESULTS))
    // The client does not support OUT-parameters.
    return false;

  List<Item> out_param_lst;
  Item_param *item_param;
  while ((item_param= item_param_it++))
  {
    // Skip it as it's just an IN-parameter.
    if (!item_param->get_out_param_info())
      continue;

    if (out_param_lst.push_back(item_param))
      return true;                 /* purecov: inspected */
  }

  // Empty list
  if (!out_param_lst.elements)
    return false;

  /*
    We have to set SERVER_PS_OUT_PARAMS in THD::server_status, because it
    is used in send_result_metadata().
  */
  m_thd->server_status|= SERVER_PS_OUT_PARAMS | SERVER_MORE_RESULTS_EXISTS;

  // Send meta-data.
  if (m_thd->send_result_metadata(&out_param_lst,
                                  Protocol::SEND_NUM_ROWS|Protocol::SEND_EOF))
    return true;

  // Send data.
  start_row();
  if (m_thd->send_result_set_row(&out_param_lst))
    return true;
  if (end_row())
    return true;

  // Restore THD::server_status.
  m_thd->server_status&= ~SERVER_PS_OUT_PARAMS;
  m_thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

  if (has_client_capability(CLIENT_DEPRECATE_EOF))
    return net_send_ok(m_thd,
                       (m_thd->server_status | SERVER_PS_OUT_PARAMS |
                        SERVER_MORE_RESULTS_EXISTS),
                       m_thd->get_stmt_da()->current_statement_cond_count(),
                       0, 0, nullptr, true);
  else
    /*
      In case of old clients send EOF packet.
      @ref page_protocol_basic_eof_packet is deprecated as of MySQL 5.7.5.
    */
    return send_eof(m_thd->server_status, 0);
}


/**
  Sets OUT-parameters to user variables.

  @param parameters  List of PS/SP parameters (both input and output).

  @return Error status.
    @retval false Success.
    @retval true  Error.
*/
bool Protocol_text::send_parameters(List<Item_param> *parameters, bool)
{
  List_iterator_fast<Item_param> item_param_it(*parameters);
  List_iterator_fast<LEX_STRING> user_var_name_it(
    m_thd->lex->prepared_stmt_params);

  Item_param *item_param;
  LEX_STRING *user_var_name;
  while ((item_param= item_param_it++) && (user_var_name= user_var_name_it++))
  {
    // Skip if it as it's just an IN-parameter.
    if (!item_param->get_out_param_info())
      continue;

    Item_func_set_user_var *suv=
      new Item_func_set_user_var(*user_var_name, item_param, false);
    /*
      Item_func_set_user_var is not fixed after construction,
      call fix_fields().
    */
    if (suv->fix_fields(m_thd, nullptr))
      return true;

    if (suv->check(false))
      return true;

    if (suv->update())
      return true;
  }

  return false;
}


/****************************************************************************
  Functions to handle the binary protocol used with prepared statements

  Data format:

    [ok:1]                            reserved ok packet
    [null_field:(field_count+7+2)/8]  reserved to send null data. The size is
                                      calculated using:
                                      bit_fields= (field_count+7+2)/8;
                                      2 bits are reserved for identifying type
                                      of package.
    [[length]data]                    data field (the length applies only for
                                      string/binary/time/timestamp fields and
                                      rest of them are not sent as they have
                                      the default length that client understands
                                      based on the field type
    [..]..[[length]data]              data
****************************************************************************/
bool Protocol_binary::start_result_metadata(uint num_cols, uint flags,
                                            const CHARSET_INFO *result_cs)
{
  bit_fields= (num_cols + 9) / 8;
  packet->alloc(bit_fields+1);
  return Protocol_classic::start_result_metadata(num_cols, flags, result_cs);
}


void Protocol_binary::start_row()
{
  if (send_metadata)
    return Protocol_text::start_row();
  packet->length(bit_fields+1);
  memset(const_cast<char*>(packet->ptr()), 0, 1+bit_fields);
  field_pos=0;
}


bool Protocol_binary::store(const char *from, size_t length,
                            const CHARSET_INFO *fromcs,
                            const CHARSET_INFO *tocs)
{
  if(send_metadata)
    return Protocol_text::store(from, length, fromcs, tocs);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_DECIMAL ||
    field_types[field_pos] == MYSQL_TYPE_BIT ||
    field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL ||
    field_types[field_pos] == MYSQL_TYPE_NEWDATE ||
    field_types[field_pos] == MYSQL_TYPE_JSON ||
    (field_types[field_pos] >= MYSQL_TYPE_ENUM &&
      field_types[field_pos] <= MYSQL_TYPE_GEOMETRY));
#endif
  field_pos++;
  return store_string_aux(from, length, fromcs, tocs);
}


bool Protocol_binary::store_null()
{
  if(send_metadata)
    return Protocol_text::store_null();
  uint offset= (field_pos+2)/8+1, bit= (1 << ((field_pos+2) & 7));
  /* Room for this as it's allocated in prepare_for_send */
  char *to= (char *) packet->ptr()+offset;
  *to= (char) ((uchar) *to | (uchar) bit);
  field_pos++;
  return 0;
}


bool Protocol_binary::store_tiny(longlong from)
{
  if(send_metadata)
    return Protocol_text::store_tiny(from);
  char buff[1];
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_TINY ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  field_pos++;
  buff[0]= (uchar) from;
  return packet->append(buff, sizeof(buff), PACKET_BUFFER_EXTRA_ALLOC);
}


bool Protocol_binary::store_short(longlong from)
{
  if(send_metadata)
    return Protocol_text::store_short(from);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_YEAR ||
    field_types[field_pos] == MYSQL_TYPE_SHORT ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  field_pos++;
  char *to= packet->prep_append(2, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int2store(to, (int) from);
  return 0;
}


bool Protocol_binary::store_long(longlong from)
{
  if(send_metadata)
    return Protocol_text::store_long(from);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_INT24 ||
    field_types[field_pos] == MYSQL_TYPE_LONG ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int4store(to, static_cast<uint32>(from));
  return 0;
}


bool Protocol_binary::store_longlong(longlong from, bool unsigned_flag)
{
  if(send_metadata)
    return Protocol_text::store_longlong(from, unsigned_flag);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_LONGLONG ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int8store(to, from);
  return 0;
}


bool Protocol_binary::store_decimal(const my_decimal *d, uint prec, uint dec)
{
  if(send_metadata)
    return Protocol_text::store_decimal(d, prec, dec);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
  // store() will increment the field_pos counter
#endif
  char buff[DECIMAL_MAX_STR_LENGTH + 1];
  String str(buff, sizeof(buff), &my_charset_bin);
  (void) my_decimal2string(E_DEC_FATAL_ERROR, d, prec, dec, '0', &str);
  return store(str.ptr(), str.length(), str.charset(), result_cs);
}


bool Protocol_binary::store(float from, uint32 decimals, String *buffer)
{
  if(send_metadata)
    return Protocol_text::store(from, decimals, buffer);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_FLOAT ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  float4store(to, from);
  return 0;
}


bool Protocol_binary::store(double from, uint32 decimals, String *buffer)
{
  if(send_metadata)
    return Protocol_text::store(from, decimals, buffer);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0
    || field_types[field_pos] == MYSQL_TYPE_DOUBLE ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  float8store(to, from);
  return 0;
}


bool Protocol_binary::store(Proto_field *field)
{
  if(send_metadata)
    return Protocol_text::store(field);
  return field->send_binary(this);
}


bool Protocol_binary::store(MYSQL_TIME *tm, uint precision)
{
  if(send_metadata)
    return Protocol_text::store(tm, precision);

#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_DATE ||
    is_temporal_type_with_date_and_time(field_types[field_pos]) ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  char buff[12],*pos;
  size_t length;
  field_pos++;
  pos= buff+1;

  int2store(pos, tm->year);
  pos[2]= (uchar) tm->month;
  pos[3]= (uchar) tm->day;
  pos[4]= (uchar) tm->hour;
  pos[5]= (uchar) tm->minute;
  pos[6]= (uchar) tm->second;
  int4store(pos+7, tm->second_part);
  if (tm->second_part)
    length=11;
  else if (tm->hour || tm->minute || tm->second)
    length=7;
  else if (tm->year || tm->month || tm->day)
    length=4;
  else
    length=0;
  buff[0]=(char) length;			// Length is stored first
  return packet->append(buff, length+1, PACKET_BUFFER_EXTRA_ALLOC);
}


bool Protocol_binary::store_date(MYSQL_TIME *tm)
{
  if(send_metadata)
    return Protocol_text::store_date(tm);
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_DATE ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  tm->hour= tm->minute= tm->second=0;
  tm->second_part= 0;
  return Protocol_binary::store(tm, 0);
}


bool Protocol_binary::store_time(MYSQL_TIME *tm, uint precision)
{
  if(send_metadata)
    return Protocol_text::store_time(tm, precision);
  char buff[13], *pos;
  size_t length;
#ifndef DBUG_OFF
  // field_types check is needed because of the embedded protocol
  DBUG_ASSERT(field_types == 0 ||
    field_types[field_pos] == MYSQL_TYPE_TIME ||
    field_types[field_pos] == MYSQL_TYPE_VAR_STRING);
#endif
  field_pos++;
  pos= buff+1;
  pos[0]= tm->neg ? 1 : 0;
  if (tm->hour >= 24)
  {
    /* Fix if we come from Item::send */
    uint days= tm->hour/24;
    tm->hour-= days*24;
    tm->day+= days;
  }
  int4store(pos+1, tm->day);
  pos[5]= (uchar) tm->hour;
  pos[6]= (uchar) tm->minute;
  pos[7]= (uchar) tm->second;
  int4store(pos+8, tm->second_part);
  if (tm->second_part)
    length=12;
  else if (tm->hour || tm->minute || tm->second || tm->day)
    length=8;
  else
    length=0;
  buff[0]=(char) length;			// Length is stored first
  return packet->append(buff, length+1, PACKET_BUFFER_EXTRA_ALLOC);
}


/**
  @returns: the file descriptor of the socket.
*/

my_socket Protocol_classic::get_socket()
{
  return get_vio()->mysql_socket.fd;
}


/**
  Read the length of the parameter data and return it back to
  the caller.

  @param packet             a pointer to the data
  @param packet_left_len    remaining packet length
  @param header_len         size of the header stored at the beginning of the
                            packet and used to specify the length of the data.

  @return
    Length of data piece.
*/

static ulong
get_param_length(uchar *packet, ulong packet_left_len, ulong *header_len)
{
  if (packet_left_len < 1)
  {
    *header_len= 0;
    return 0;
  }
  if (*packet < 251)
  {
    *header_len= 1;
    return (ulong) *packet;
  }
  if (packet_left_len < 3)
  {
    *header_len= 0;
    return 0;
  }
  if (*packet == 252)
  {
    *header_len= 3;
    return (ulong) uint2korr(packet + 1);
  }
  if (packet_left_len < 4)
  {
    *header_len= 0;
    return 0;
  }
  if (*packet == 253)
  {
    *header_len= 4;
    return (ulong) uint3korr(packet + 1);
  }
  if (packet_left_len < 5)
  {
    *header_len= 0;
    return 0;
  }
  *header_len= 9; // Must be 254 when here
  /*
    In our client-server protocol all numbers bigger than 2^24
    stored as 8 bytes with uint8korr. Here we always know that
    parameter length is less than 2^4 so don't look at the second
    4 bytes. But still we need to obey the protocol hence 9 in the
    assignment above.
  */
  return (ulong) uint4korr(packet + 1);
}


/**
  Returns the length of the encoded data

   @param[in]  type          parameter data type
   @param[in]  packet        network buffer
   @param[in]  packet_len    number of bytes left in packet
   @param[out] header_len    the size of the header(bytes to be skiped)
   @param[out] err           boolean to store if an error occurred
*/
ulong get_ps_param_len(enum enum_field_types type, uchar *packet,
                       ulong packet_len, ulong *header_len, bool *err)
{
  DBUG_ENTER("get_ps_param_len");
  *header_len= 0;

  switch (type)
  {
    case MYSQL_TYPE_TINY:
      *err= (packet_len < 1);
      DBUG_RETURN(1);
    case MYSQL_TYPE_SHORT:
      *err= (packet_len < 2);
      DBUG_RETURN(2);
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_LONG:
      *err= (packet_len < 4);
      DBUG_RETURN(4);
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_LONGLONG:
      *err= (packet_len < 8);
      DBUG_RETURN(8);
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    {
      ulong param_length= get_param_length(packet, packet_len, header_len);
      /* in case of error ret is 0 and header size is 0 */
      *err= ((!param_length && !*header_len) ||
          (packet_len < *header_len + param_length));
      DBUG_PRINT("info", ("ret=%lu ", param_length));
      DBUG_RETURN(param_length);
    }
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    default:
    {
      ulong param_length= get_param_length(packet, packet_len, header_len);
      /* in case of error ret is 0 and header size is 0 */
      *err= (!param_length && !*header_len);
      if (param_length > packet_len - *header_len)
        param_length= packet_len - *header_len;
      DBUG_PRINT("info", ("ret=%lu", param_length));
      DBUG_RETURN(param_length);
    }
  }
}
