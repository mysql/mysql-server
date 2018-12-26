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

/**
@file

Low level functions for storing data to be send to the MySQL client.
The actual communication is handled by the net_xxx functions in net_serv.cc
*/

#include "protocol_classic.h"
#include "sql_class.h"                          // THD
#include <stdarg.h>

using std::min;
using std::max;

static const unsigned int PACKET_BUFFER_EXTRA_ALLOC= 1024;
bool net_send_error_packet(THD *, uint, const char *, const char *);
bool net_send_error_packet(NET *, uint, const char *, const char *, bool,
                           ulong, const CHARSET_INFO*);
/* Declared non-static only because of the embedded library. */
bool net_send_ok(THD *, uint, uint, ulonglong, ulonglong, const char *, bool);
/* Declared non-static only because of the embedded library. */
bool net_send_eof(THD *thd, uint server_status, uint statement_warn_count);
#ifndef EMBEDDED_LIBRARY
static bool write_eof_packet(THD *, NET *, uint, uint);
#endif

#ifndef EMBEDDED_LIBRARY
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
  memcpy(to,from,length);
  packet->length((uint) (to+length-(uchar *) packet->ptr()));
  return 0;
}
#endif


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

#ifndef EMBEDDED_LIBRARY
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
#endif


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

#ifndef EMBEDDED_LIBRARY
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
  Return OK to the client.

  The OK packet has the following structure:

  Here 'n' denotes the length of state change information.

  Bytes                Name
  -----                ----
  1                    [00] or [FE] the OK header
                       [FE] is used as header for result set rows
  1-9 (lenenc-int)     affected rows
  1-9 (lenenc-int)     last-insert-id

  if capabilities & CLIENT_PROTOCOL_41 {
    2                  status_flags; Copy of thd->server_status; Can be used
                       by client to check if we are inside a transaction.
    2                  warnings (New in 4.1 protocol)
  } elseif capabilities & CLIENT_TRANSACTIONS {
    2                  status_flags
  }

  if capabilities & CLIENT_ACCEPTS_SERVER_STATUS_CHANGE_INFO {
    1-9(lenenc_str)    info (message); Stored as length of the message string +
                       message.
    if n > 0 {
      1-9 (lenenc_int) total length of session state change
                       information to follow (= n)
      n                session state change information
    }
  }
  else {
    string[EOF]          info (message); Stored as packed length (1-9 bytes) +
                         message. Is not stored if no message.
  }

  @param thd                     Thread handler
  @param server_status           The server status
  @param statement_warn_count    Total number of warnings
  @param affected_rows           Number of rows changed by statement
  @param id                      Auto_increment id for first row (if used)
  @param message                 Message to send to the client
                                 (Used by mysql_status)
  @param eof_indentifier         when true [FE] will be set in OK header
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
  Send eof (= end of result set) to the client.

  The eof packet has the following structure:

  - 254           : Marker (1 byte)
  - warning_count : Stored in 2 bytes; New in 4.1 protocol
  - status_flag   : Stored in 2 bytes;
  For flags like SERVER_MORE_RESULTS_EXISTS.

  Note that the warning count will not be sent if 'no_flush' is set as
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
  @param thd          Thread handler
  @param sql_errno    The error code to send
  @param err          A pointer to the error message
  @param sqlstate     SQL state

  @return
   @retval FALSE The message was successfully sent
   @retval TRUE  An error occurred and the messages wasn't sent properly
*/

bool net_send_error_packet(THD *thd, uint sql_errno, const char *err,
                           const char* sqlstate)
{
  return net_send_error_packet(thd->get_protocol_classic()->get_net(),
                               sql_errno, err, sqlstate, thd->bootstrap,
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
*/

bool net_send_error_packet(NET* net, uint sql_errno, const char *err,
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
#endif /* EMBEDDED_LIBRARY */


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
#ifndef EMBEDDED_LIBRARY
  if (has_client_capability(CLIENT_DEPRECATE_EOF) &&
      (m_thd->get_command() != COM_BINLOG_DUMP &&
       m_thd->get_command() != COM_BINLOG_DUMP_GTID))
    retval= net_send_ok(m_thd, server_status, statement_warn_count, 0, 0, NULL,
                        true);
  else
#endif
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


bool Protocol_classic::flush_net()
{
  return net_flush(&m_thd->net);
}


bool Protocol_classic::write(const uchar *ptr, size_t len)
{
  return my_net_write(&m_thd->net, ptr, len);
}


uchar Protocol_classic::get_error()
{
  return m_thd->net.error;
}


uint Protocol_classic::get_last_errno()
{
  return m_thd->net.last_errno;
}


void Protocol_classic::set_last_errno(uint err)
{
  m_thd->net.last_errno= err;
}


char *Protocol_classic::get_last_error()
{
  return m_thd->net.last_error;
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


void Protocol_classic::set_pkt_nr(uint pkt_nr)
{
  m_thd->net.pkt_nr= pkt_nr;
}


uint Protocol_classic::get_pkt_nr()
{
  return m_thd->net.pkt_nr;
}


String *Protocol_classic::get_packet()
{
  return &m_thd->packet;
}


int Protocol_classic::read_packet()
{
  int ret;
  if ((packet_length= my_net_read(&m_thd->net)) &&
      packet_length != packet_error)
  {
    DBUG_ASSERT(!m_thd->net.error);
    bad_packet= false;
    raw_packet= m_thd->net.read_pos;
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
  switch(cmd)
  {
  case COM_INIT_DB:
  {
    data->com_init_db.db_name= reinterpret_cast<const char*>(raw_packet);
    data->com_init_db.length= packet_length;
    break;
  }
  case COM_REFRESH:
  {
    if (packet_length < 1)
      goto malformed;
    data->com_refresh.options= raw_packet[0];
    break;
  }
  case COM_SHUTDOWN:
  {
    data->com_shutdown.level= packet_length == 0 ?
      SHUTDOWN_DEFAULT : (enum mysql_enum_shutdown_level) raw_packet[0];
    break;
  }
  case COM_PROCESS_KILL:
  {
    if (packet_length < 4)
      goto malformed;
    data->com_kill.id= (ulong) uint4korr(raw_packet);
    break;
  }
  case COM_SET_OPTION:
  {
    if (packet_length < 2)
      goto malformed;
    data->com_set_option.opt_command= uint2korr(raw_packet);
    break;
  }
  case COM_STMT_EXECUTE:
  {
    if (packet_length < 9)
      goto malformed;
    data->com_stmt_execute.stmt_id= uint4korr(raw_packet);
    data->com_stmt_execute.flags= (ulong) raw_packet[4];
    /* stmt_id + 5 bytes of flags */
    /*
      FIXME: params have to be parsed into an array/structure
      by protocol too
    */
    data->com_stmt_execute.params= raw_packet + 9;
    data->com_stmt_execute.params_length= packet_length - 9;
    break;
  }
  case COM_STMT_FETCH:
  {
    if (packet_length < 8)
      goto malformed;
    data->com_stmt_fetch.stmt_id= uint4korr(raw_packet);
    data->com_stmt_fetch.num_rows= uint4korr(raw_packet + 4);
    break;
  }
  case COM_STMT_SEND_LONG_DATA:
  {
#ifndef EMBEDDED_LIBRARY
    if (packet_length < MYSQL_LONG_DATA_HEADER)
      goto malformed;
#endif
    data->com_stmt_send_long_data.stmt_id= uint4korr(raw_packet);
    data->com_stmt_send_long_data.param_number= uint2korr(raw_packet + 4);
    data->com_stmt_send_long_data.longdata= raw_packet + 6;
    data->com_stmt_send_long_data.length= packet_length - 6;
    break;
  }
  case COM_STMT_PREPARE:
  {
    data->com_stmt_prepare.query= reinterpret_cast<const char*>(raw_packet);
    data->com_stmt_prepare.length= packet_length;
    break;
  }
  case COM_STMT_CLOSE:
  {
    if (packet_length < 4)
      goto malformed;

    data->com_stmt_close.stmt_id= uint4korr(raw_packet);
    break;
  }
  case COM_STMT_RESET:
  {
    if (packet_length < 4)
      goto malformed;

    data->com_stmt_reset.stmt_id= uint4korr(raw_packet);
    break;
  }
  case COM_QUERY:
  {
    data->com_query.query= reinterpret_cast<const char*>(raw_packet);
    data->com_query.length= packet_length;
    break;
  }
  case COM_FIELD_LIST:
  {
    /*
      We have name + wildcard in packet, separated by endzero
    */
    data->com_field_list.table_name= raw_packet;
    uint len= data->com_field_list.table_name_length=
        strend((char *)raw_packet) - (char *)raw_packet;
    if (len >= packet_length || len > NAME_LEN)
      goto malformed;
    data->com_field_list.query= raw_packet + len + 1;
    data->com_field_list.query_length= packet_length - len;
    break;
  }
  default:
    break;
  }

  return false;

  malformed:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  bad_packet= true;
  return true;
}

bool Protocol_classic::create_command(COM_DATA *com_data,
                                      enum_server_command cmd,
                                      uchar *pkt, size_t length)
{
  raw_packet= pkt;
  packet_length= length;

  return parse_packet(com_data, cmd);
}

int Protocol_classic::get_command(COM_DATA *com_data, enum_server_command *cmd)
{
  // read packet from the network
  if(int rc= read_packet())
    return rc;

  /*
    'packet_length' contains length of data, as it was stored in packet
    header. In case of malformed header, my_net_read returns zero.
    If packet_length is not zero, my_net_read ensures that the returned
    number of bytes was actually read from network.
    There is also an extra safety measure in my_net_read:
    it sets packet[packet_length]= 0, but only for non-zero packets.
  */
  if (packet_length == 0)                       /* safety */
  {
    /* Initialize with COM_SLEEP packet */
    raw_packet[0]= (uchar) COM_SLEEP;
    packet_length= 1;
  }
  /* Do not rely on my_net_read, extra safety against programming errors. */
  raw_packet[packet_length]= '\0';                  /* safety */

  *cmd= (enum enum_server_command) raw_packet[0];

  if (*cmd >= COM_END)
    *cmd= COM_END;				// Wrong command

  DBUG_ASSERT(packet_length);
  // Skip 'command'
  packet_length--;
  raw_packet++;

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
#ifndef EMBEDDED_LIBRARY
  bool error;
  m_thd->get_stmt_da()->set_overwrite_status(true);
  error= net_flush(&m_thd->net);
  m_thd->get_stmt_da()->set_overwrite_status(false);
  return error;
#else
  return 0;
#endif
}


bool Protocol_classic::get_compression()
{
  return m_thd->net.compress;
}


#ifndef EMBEDDED_LIBRARY
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
#endif /* EMBEDDED_LIBRARY */


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

#ifndef EMBEDDED_LIBRARY

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
#endif


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


int Protocol_classic::shutdown(bool server_shutdown)
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
  Assign OUT-parameters to user variables.

  @param sp_params  List of PS/SP parameters (both input and output).

  @return Error status.
    @retval FALSE Success.
    @retval TRUE  Error.
*/

bool Protocol_text::send_out_parameters(List<Item_param> *sp_params)
{
  DBUG_ASSERT(sp_params->elements == m_thd->lex->prepared_stmt_params.elements);

  List_iterator_fast<Item_param> item_param_it(*sp_params);
  List_iterator_fast<LEX_STRING> user_var_name_it(
    m_thd->lex->prepared_stmt_params);

  while (true)
  {
    Item_param *item_param= item_param_it++;
    LEX_STRING *user_var_name= user_var_name_it++;

    if (!item_param || !user_var_name)
      break;

    if (!item_param->get_out_param_info())
      continue; // It's an IN-parameter.

    Item_func_set_user_var *suv=
      new Item_func_set_user_var(*user_var_name, item_param, false);
    /*
      Item_func_set_user_var is not fixed after construction, call
      fix_fields().
    */
    if (suv->fix_fields(m_thd, NULL))
      return TRUE;

    if (suv->check(FALSE))
      return TRUE;

    if (suv->update())
      return TRUE;
  }

  return FALSE;
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


#ifndef EMBEDDED_LIBRARY
void Protocol_binary::start_row()
{
  if (send_metadata)
    return Protocol_text::start_row();
  packet->length(bit_fields+1);
  memset(const_cast<char*>(packet->ptr()), 0, 1+bit_fields);
  field_pos=0;
}
#endif


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
  Send a result set with OUT-parameter values by means of PS-protocol.

  @param sp_params  List of PS/SP parameters (both input and output).

  @return Error status.
    @retval FALSE Success.
    @retval TRUE  Error.
*/

bool Protocol_binary::send_out_parameters(List<Item_param> *sp_params)
{
  bool ret;
  if (!has_client_capability(CLIENT_PS_MULTI_RESULTS))
  {
    /* The client does not support OUT-parameters. */
    return FALSE;
  }

  List<Item> out_param_lst;

  {
    List_iterator_fast<Item_param> item_param_it(*sp_params);

    while (true)
    {
      Item_param *item_param= item_param_it++;

      if (!item_param)
        break;

      if (!item_param->get_out_param_info())
        continue; // It's an IN-parameter.

      if (out_param_lst.push_back(item_param))
        return TRUE;
    }
  }

  if (!out_param_lst.elements)
    return FALSE;

  /*
    We have to set SERVER_PS_OUT_PARAMS in THD::server_status, because it
    is used in send_result_metadata().
  */

  m_thd->server_status|= SERVER_PS_OUT_PARAMS | SERVER_MORE_RESULTS_EXISTS;

  /* Send meta-data. */
  if (m_thd->send_result_metadata(&out_param_lst,
                                  SEND_NUM_ROWS | SEND_EOF))
    return TRUE;

  /* Send data. */

  this->start_row();

  if (m_thd->send_result_set_row(&out_param_lst))
    return TRUE;

  if (this->end_row())
    return TRUE;

  /* Restore THD::server_status. */
  m_thd->server_status&= ~SERVER_PS_OUT_PARAMS;

  /*
    Reset SERVER_MORE_RESULTS_EXISTS bit, because for sure this is the last
    result set we are sending.
  */

  m_thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

  if (has_client_capability(CLIENT_DEPRECATE_EOF))
    ret= net_send_ok(m_thd,
                     (m_thd->server_status | SERVER_PS_OUT_PARAMS |
                       SERVER_MORE_RESULTS_EXISTS),
                     m_thd->get_stmt_da()->current_statement_cond_count(),
                     0, 0, NULL, true);
  else
    /* In case of old clients send EOF packet. */
    ret= net_send_eof(m_thd, m_thd->server_status, 0);
  return ret ? FALSE : TRUE;
}


/**
  @returns: the file descriptor of the socket.
*/

my_socket Protocol_classic::get_socket()
{
  return get_vio()->mysql_socket.fd;
}
