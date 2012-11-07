/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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
  The actual communction is handled by the net_xxx functions in net_serv.cc
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "sql_priv.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "protocol.h"
#include "sql_class.h"                          // THD
#include <stdarg.h>

static const unsigned int PACKET_BUFFER_EXTRA_ALLOC= 1024;
/* Declared non-static only because of the embedded library. */
bool net_send_error_packet(THD *, uint, const char *, const char *);
/* Declared non-static only because of the embedded library. */
bool net_send_ok(THD *, uint, uint, ulonglong, ulonglong, const char *);
/* Declared non-static only because of the embedded library. */
bool net_send_eof(THD *thd, uint server_status, uint statement_warn_count);
#ifndef EMBEDDED_LIBRARY
static bool write_eof_packet(THD *, NET *, uint, uint);
#endif

#ifndef EMBEDDED_LIBRARY
bool Protocol::net_store_data(const uchar *from, size_t length)
#else
bool Protocol_binary::net_store_data(const uchar *from, size_t length)
#endif
{
  ulong packet_length=packet->length();
  /* 
     The +9 comes from that strings of length longer than 16M require
     9 bytes to be stored (see net_store_length).
  */
  if (packet_length+9+length > packet->alloced_length() &&
      packet->realloc(packet_length+9+length))
    return 1;
  uchar *to= net_store_length((uchar*) packet->ptr()+packet_length, length);
  memcpy(to,from,length);
  packet->length((uint) (to+length-(uchar*) packet->ptr()));
  return 0;
}




/*
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
bool Protocol::net_store_data(const uchar *from, size_t length,
                              CHARSET_INFO *from_cs, CHARSET_INFO *to_cs)
{
  uint dummy_errors;
  /* Calculate maxumum possible result length */
  uint conv_length= to_cs->mbmaxlen * length / from_cs->mbminlen;
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
    return (convert->copy((const char*) from, length, from_cs,
                          to_cs, &dummy_errors) ||
            net_store_data((const uchar*) convert->ptr(), convert->length()));
  }

  ulong packet_length= packet->length();
  ulong new_length= packet_length + conv_length + 1;

  if (new_length > packet->alloced_length() && packet->realloc(new_length))
    return 1;

  char *length_pos= (char*) packet->ptr() + packet_length;
  char *to= length_pos + 1;

  to+= copy_and_convert(to, conv_length, to_cs,
                        (const char*) from, length, from_cs, &dummy_errors);

  net_store_length((uchar*) length_pos, to - length_pos - 1);
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

bool net_send_error(THD *thd, uint sql_errno, const char *err,
                    const char* sqlstate)
{
  bool error;
  DBUG_ENTER("net_send_error");

  DBUG_ASSERT(!thd->spcont);
  DBUG_ASSERT(sql_errno);
  DBUG_ASSERT(err);

  DBUG_PRINT("enter",("sql_errno: %d  err: %s", sql_errno, err));

  if (sqlstate == NULL)
    sqlstate= mysql_errno_to_sqlstate(sql_errno);

  /*
    It's one case when we can push an error even though there
    is an OK or EOF already.
  */
  thd->stmt_da->can_overwrite_status= TRUE;

  /* Abort multi-result sets */
  thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

  error= net_send_error_packet(thd, sql_errno, err, sqlstate);

  thd->stmt_da->can_overwrite_status= FALSE;

  DBUG_RETURN(error);
}

/**
  Return ok to the client.

  The ok packet has the following structure:

  - 0               : Marker (1 byte)
  - affected_rows	: Stored in 1-9 bytes
  - id		: Stored in 1-9 bytes
  - server_status	: Copy of thd->server_status;  Can be used by client
  to check if we are inside an transaction.
  New in 4.0 protocol
  - warning_count	: Stored in 2 bytes; New in 4.1 protocol
  - message		: Stored as packed length (1-9 bytes) + message.
  Is not stored if no message.

  @param thd		   Thread handler
  @param server_status     The server status
  @param statement_warn_count  Total number of warnings
  @param affected_rows	   Number of rows changed by statement
  @param id		   Auto_increment id for first row (if used)
  @param message	   Message to send to the client (Used by mysql_status)
 
  @return
    @retval FALSE The message was successfully sent
    @retval TRUE An error occurred and the messages wasn't sent properly

*/

#ifndef EMBEDDED_LIBRARY
bool
net_send_ok(THD *thd,
            uint server_status, uint statement_warn_count,
            ulonglong affected_rows, ulonglong id, const char *message)
{
  NET *net= &thd->net;
  uchar buff[MYSQL_ERRMSG_SIZE+10],*pos;
  bool error= FALSE;
  DBUG_ENTER("net_send_ok");

  if (! net->vio)	// hack for re-parsing queries
  {
    DBUG_PRINT("info", ("vio present: NO"));
    DBUG_RETURN(FALSE);
  }

  buff[0]=0;					// No fields
  pos=net_store_length(buff+1,affected_rows);
  pos=net_store_length(pos, id);
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    DBUG_PRINT("info",
	       ("affected_rows: %lu  id: %lu  status: %u  warning_count: %u",
		(ulong) affected_rows,		
		(ulong) id,
		(uint) (server_status & 0xffff),
		(uint) statement_warn_count));
    int2store(pos, server_status);
    pos+=2;

    /* We can only return up to 65535 warnings in two bytes */
    uint tmp= min(statement_warn_count, 65535);
    int2store(pos, tmp);
    pos+= 2;
  }
  else if (net->return_status)			// For 4.0 protocol
  {
    int2store(pos, server_status);
    pos+=2;
  }
  thd->stmt_da->can_overwrite_status= TRUE;

  if (message && message[0])
    pos= net_store_data(pos, (uchar*) message, strlen(message));
  error= my_net_write(net, buff, (size_t) (pos-buff));
  if (!error)
    error= net_flush(net);


  thd->stmt_da->can_overwrite_status= FALSE;
  DBUG_PRINT("info", ("OK sent, so no more error sending allowed"));

  DBUG_RETURN(error);
}

static uchar eof_buff[1]= { (uchar) 254 };      /* Marker for end of fields */

/**
  Send eof (= end of result set) to the client.

  The eof packet has the following structure:

  - 254		: Marker (1 byte)
  - warning_count	: Stored in 2 bytes; New in 4.1 protocol
  - status_flag	: Stored in 2 bytes;
  For flags like SERVER_MORE_RESULTS_EXISTS.

  Note that the warning count will not be sent if 'no_flush' is set as
  we don't want to report the warning count until all data is sent to the
  client.

  @param thd		Thread handler
  @param server_status The server status
  @param statement_warn_count Total number of warnings

  @return
    @retval FALSE The message was successfully sent
    @retval TRUE An error occurred and the message wasn't sent properly
*/    

bool
net_send_eof(THD *thd, uint server_status, uint statement_warn_count)
{
  NET *net= &thd->net;
  bool error= FALSE;
  DBUG_ENTER("net_send_eof");
  /* Set to TRUE if no active vio, to work well in case of --init-file */
  if (net->vio != 0)
  {
    thd->stmt_da->can_overwrite_status= TRUE;
    error= write_eof_packet(thd, net, server_status, statement_warn_count);
    if (!error)
      error= net_flush(net);
    thd->stmt_da->can_overwrite_status= FALSE;
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
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    uchar buff[5];
    /*
      Don't send warn count during SP execution, as the warn_list
      is cleared between substatements, and mysqltest gets confused
    */
    uint tmp= min(statement_warn_count, 65535);
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
  @param thd Thread handler
  @param sql_errno The error code to send
  @param err A pointer to the error message

  @return
   @retval FALSE The message was successfully sent
   @retval TRUE  An error occurred and the messages wasn't sent properly
*/

bool net_send_error_packet(THD *thd, uint sql_errno, const char *err,
                           const char* sqlstate)

{
  NET *net= &thd->net;
  uint length;
  /*
    buff[]: sql_errno:2 + ('#':1 + SQLSTATE_LENGTH:5) + MYSQL_ERRMSG_SIZE:512
  */
  uint error;
  char converted_err[MYSQL_ERRMSG_SIZE];
  char buff[2+1+SQLSTATE_LENGTH+MYSQL_ERRMSG_SIZE], *pos;

  DBUG_ENTER("send_error_packet");

  if (net->vio == 0)
  {
    if (thd->bootstrap)
    {
      /* In bootstrap it's ok to print on stderr */
      fprintf(stderr,"ERROR: %d  %s\n",sql_errno,err);
    }
    DBUG_RETURN(FALSE);
  }

  int2store(buff,sql_errno);
  pos= buff+2;
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    /* The first # is to make the protocol backward compatible */
    buff[2]= '#';
    pos= strmov(buff+3, sqlstate);
  }

  convert_error_message(converted_err, sizeof(converted_err),
                        thd->variables.character_set_results,
                        err, strlen(err), system_charset_info, &error);
  /* Converted error message is always null-terminated. */
  length= (uint) (strmake(pos, converted_err, MYSQL_ERRMSG_SIZE - 1) - buff);

  DBUG_RETURN(net_write_command(net,(uchar) 255, (uchar*) "", 0, (uchar*) buff,
                                length));
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

static uchar *net_store_length_fast(uchar *packet, uint length)
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

/**
  Send the status of the current statement execution over network.

  @param  thd   in fact, carries two parameters, NET for the transport and
                Diagnostics_area as the source of status information.

  In MySQL, there are two types of SQL statements: those that return
  a result set and those that return status information only.

  If a statement returns a result set, it consists of 3 parts:
  - result set meta-data
  - variable number of result set rows (can be 0)
  - followed and terminated by EOF or ERROR packet

  Once the  client has seen the meta-data information, it always
  expects an EOF or ERROR to terminate the result set. If ERROR is
  received, the result set rows are normally discarded (this is up
  to the client implementation, libmysql at least does discard them).
  EOF, on the contrary, means "successfully evaluated the entire
  result set". Since we don't know how many rows belong to a result
  set until it's evaluated, EOF/ERROR is the indicator of the end
  of the row stream. Note, that we can not buffer result set rows
  on the server -- there may be an arbitrary number of rows. But
  we do buffer the last packet (EOF/ERROR) in the Diagnostics_area and
  delay sending it till the very end of execution (here), to be able to
  change EOF to an ERROR if commit failed or some other error occurred
  during the last cleanup steps taken after execution.

  A statement that does not return a result set doesn't send result
  set meta-data either. Instead it returns one of:
  - OK packet
  - ERROR packet.
  Similarly to the EOF/ERROR of the previous statement type, OK/ERROR
  packet is "buffered" in the diagnostics area and sent to the client
  in the end of statement.

  @note This method defines a template, but delegates actual 
  sending of data to virtual Protocol::send_{ok,eof,error}. This
  allows for implementation of protocols that "intercept" ok/eof/error
  messages, and store them in memory, etc, instead of sending to
  the client.

  @pre  The diagnostics area is assigned or disabled. It can not be empty
        -- we assume that every SQL statement or COM_* command
        generates OK, ERROR, or EOF status.

  @post The status information is encoded to protocol format and sent to the
        client.

  @return We conventionally return void, since the only type of error
          that can happen here is a NET (transport) error, and that one
          will become visible when we attempt to read from the NET the
          next command.
          Diagnostics_area::is_sent is set for debugging purposes only.
*/

void Protocol::end_statement()
{
  DBUG_ENTER("Protocol::end_statement");
  DBUG_ASSERT(! thd->stmt_da->is_sent);
  bool error= FALSE;

  /* Can not be true, but do not take chances in production. */
  if (thd->stmt_da->is_sent)
    DBUG_VOID_RETURN;

  switch (thd->stmt_da->status()) {
  case Diagnostics_area::DA_ERROR:
    /* The query failed, send error to log and abort bootstrap. */
    error= send_error(thd->stmt_da->sql_errno(),
                      thd->stmt_da->message(),
                      thd->stmt_da->get_sqlstate());
    break;
  case Diagnostics_area::DA_EOF:
    error= send_eof(thd->server_status,
                    thd->stmt_da->statement_warn_count());
    break;
  case Diagnostics_area::DA_OK:
    error= send_ok(thd->server_status,
                   thd->stmt_da->statement_warn_count(),
                   thd->stmt_da->affected_rows(),
                   thd->stmt_da->last_insert_id(),
                   thd->stmt_da->message());
    break;
  case Diagnostics_area::DA_DISABLED:
    break;
  case Diagnostics_area::DA_EMPTY:
  default:
    DBUG_ASSERT(0);
    error= send_ok(thd->server_status, 0, 0, 0, NULL);
    break;
  }
  if (!error)
    thd->stmt_da->is_sent= TRUE;
  DBUG_VOID_RETURN;
}


/**
  A default implementation of "OK" packet response to the client.

  Currently this implementation is re-used by both network-oriented
  protocols -- the binary and text one. They do not differ
  in their OK packet format, which allows for a significant simplification
  on client side.
*/

bool Protocol::send_ok(uint server_status, uint statement_warn_count,
                       ulonglong affected_rows, ulonglong last_insert_id,
                       const char *message)
{
  DBUG_ENTER("Protocol::send_ok");
  const bool retval= 
    net_send_ok(thd, server_status, statement_warn_count,
                affected_rows, last_insert_id, message);
  DBUG_RETURN(retval);
}


/**
  A default implementation of "EOF" packet response to the client.

  Binary and text protocol do not differ in their EOF packet format.
*/

bool Protocol::send_eof(uint server_status, uint statement_warn_count)
{
  DBUG_ENTER("Protocol::send_eof");
  const bool retval= net_send_eof(thd, server_status, statement_warn_count);
  DBUG_RETURN(retval);
}


/**
  A default implementation of "ERROR" packet response to the client.

  Binary and text protocol do not differ in ERROR packet format.
*/

bool Protocol::send_error(uint sql_errno, const char *err_msg,
                          const char *sql_state)
{
  DBUG_ENTER("Protocol::send_error");
  const bool retval= net_send_error_packet(thd, sql_errno, err_msg, sql_state);
  DBUG_RETURN(retval);
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

uchar *net_store_data(uchar *to,int32 from)
{
  char buff[20];
  uint length=(uint) (int10_to_str(from,buff,10)-buff);
  to=net_store_length_fast(to,length);
  memcpy(to,buff,length);
  return to+length;
}

uchar *net_store_data(uchar *to,longlong from)
{
  char buff[22];
  uint length=(uint) (longlong10_to_str(from,buff,10)-buff);
  to=net_store_length_fast(to,length);
  memcpy(to,buff,length);
  return to+length;
}


/*****************************************************************************
  Default Protocol functions
*****************************************************************************/

void Protocol::init(THD *thd_arg)
{
  thd=thd_arg;
  packet= &thd->packet;
  convert= &thd->convert_buffer;
#ifndef DBUG_OFF
  field_types= 0;
#endif
}

/**
  Finish the result set with EOF packet, as is expected by the client,
  if there is an error evaluating the next row and a continue handler
  for the error.
*/

void Protocol::end_partial_result_set(THD *thd_arg)
{
  net_send_eof(thd_arg, thd_arg->server_status, 0 /* no warnings, we're inside SP */);
}


bool Protocol::flush()
{
#ifndef EMBEDDED_LIBRARY
  bool error;
  thd->stmt_da->can_overwrite_status= TRUE;
  error= net_flush(&thd->net);
  thd->stmt_da->can_overwrite_status= FALSE;
  return error;
#else
  return 0;
#endif
}

#ifndef EMBEDDED_LIBRARY

/**
  Send name and type of result to client.

  Sum fields has table name empty and field_name.

  @param THD		Thread data object
  @param list	        List of items to send to client
  @param flag	        Bit mask with the following functions:
                        - 1 send number of rows
                        - 2 send default values
                        - 4 don't write eof packet

  @retval
    0	ok
  @retval
    1	Error  (Note that in this case the error is not sent to the
    client)
*/
bool Protocol::send_result_set_metadata(List<Item> *list, uint flags)
{
  List_iterator_fast<Item> it(*list);
  Item *item;
  uchar buff[MAX_FIELD_WIDTH];
  String tmp((char*) buff,sizeof(buff),&my_charset_bin);
  Protocol_text prot(thd);
  String *local_packet= prot.storage_packet();
  CHARSET_INFO *thd_charset= thd->variables.character_set_results;
  DBUG_ENTER("send_result_set_metadata");

  if (flags & SEND_NUM_ROWS)
  {				// Packet with number of elements
    uchar *pos= net_store_length(buff, list->elements);
    if (my_net_write(&thd->net, buff, (size_t) (pos-buff)))
      DBUG_RETURN(1);
  }

#ifndef DBUG_OFF
  field_types= (enum_field_types*) thd->alloc(sizeof(field_types) *
					      list->elements);
  uint count= 0;
#endif

  while ((item=it++))
  {
    char *pos;
    CHARSET_INFO *cs= system_charset_info;
    Send_field field;
    item->make_field(&field);

    /* Keep things compatible for old clients */
    if (field.type == MYSQL_TYPE_VARCHAR)
      field.type= MYSQL_TYPE_VAR_STRING;

    prot.prepare_for_resend();

    if (thd->client_capabilities & CLIENT_PROTOCOL_41)
    {
      if (prot.store(STRING_WITH_LEN("def"), cs, thd_charset) ||
	  prot.store(field.db_name, (uint) strlen(field.db_name),
		     cs, thd_charset) ||
	  prot.store(field.table_name, (uint) strlen(field.table_name),
		     cs, thd_charset) ||
	  prot.store(field.org_table_name, (uint) strlen(field.org_table_name),
		     cs, thd_charset) ||
	  prot.store(field.col_name, (uint) strlen(field.col_name),
		     cs, thd_charset) ||
	  prot.store(field.org_col_name, (uint) strlen(field.org_col_name),
		     cs, thd_charset) ||
	  local_packet->realloc(local_packet->length()+12))
	goto err;
      /* Store fixed length fields */
      pos= (char*) local_packet->ptr()+local_packet->length();
      *pos++= 12;				// Length of packed fields
      /* inject a NULL to test the client */
      DBUG_EXECUTE_IF("poison_rs_fields", pos[-1]= 0xfb;);
      if (item->charset_for_protocol() == &my_charset_bin || thd_charset == NULL)
      {
        /* No conversion */
        int2store(pos, item->charset_for_protocol()->number);
        int4store(pos+2, field.length);
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
        max_length= (field.type >= MYSQL_TYPE_TINY_BLOB &&
                     field.type <= MYSQL_TYPE_BLOB) ?
                     field.length / item->collation.collation->mbminlen :
                     field.length / item->collation.collation->mbmaxlen;
        field_length= char_to_byte_length_safe(max_length,
                                               thd_charset->mbmaxlen);
        int4store(pos + 2, field_length);
      }
      pos[6]= field.type;
      int2store(pos+7,field.flags);
      pos[9]= (char) field.decimals;
      pos[10]= 0;				// For the future
      pos[11]= 0;				// For the future
      pos+= 12;
    }
    else
    {
      if (prot.store(field.table_name, (uint) strlen(field.table_name),
		     cs, thd_charset) ||
	  prot.store(field.col_name, (uint) strlen(field.col_name),
		     cs, thd_charset) ||
	  local_packet->realloc(local_packet->length()+10))
	goto err;
      pos= (char*) local_packet->ptr()+local_packet->length();
      pos[0]=3;
      int3store(pos+1,field.length);
      pos[4]=1;
      pos[5]=field.type;
      pos[6]=3;
      int2store(pos+7,field.flags);
      pos[9]= (char) field.decimals;
      pos+= 10;
    }
    local_packet->length((uint) (pos - local_packet->ptr()));
    if (flags & SEND_DEFAULTS)
      item->send(&prot, &tmp);			// Send default value
    if (prot.write())
      DBUG_RETURN(1);
#ifndef DBUG_OFF
    field_types[count++]= field.type;
#endif
  }

  if (flags & SEND_EOF)
  {
    /*
      Mark the end of meta-data result set, and store thd->server_status,
      to show that there is no cursor.
      Send no warning information, as it will be sent at statement end.
    */
    if (write_eof_packet(thd, &thd->net, thd->server_status,
                         thd->warning_info->statement_warn_count()))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(prepare_for_send(list->elements));

err:
  my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES),
             MYF(0));	/* purecov: inspected */
  DBUG_RETURN(1);				/* purecov: inspected */
}


bool Protocol::write()
{
  DBUG_ENTER("Protocol::write");
  DBUG_RETURN(my_net_write(&thd->net, (uchar*) packet->ptr(),
                           packet->length()));
}
#endif /* EMBEDDED_LIBRARY */


/**
  Send one result set row.

  @param row_items a collection of column values for that row

  @return Error status.
    @retval TRUE  Error.
    @retval FALSE Success.
*/

bool Protocol::send_result_set_row(List<Item> *row_items)
{
  char buffer[MAX_FIELD_WIDTH];
  String str_buffer(buffer, sizeof (buffer), &my_charset_bin);
  List_iterator_fast<Item> it(*row_items);

  DBUG_ENTER("Protocol::send_result_set_row");

  for (Item *item= it++; item; item= it++)
  {
    if (item->send(this, &str_buffer))
    {
      // If we're out of memory, reclaim some, to help us recover.
      this->free();
      DBUG_RETURN(TRUE);
    }
    /* Item::send() may generate an error. If so, abort the loop. */
    if (thd->is_error())
      DBUG_RETURN(TRUE);

    /*
      Reset str_buffer to its original state, as it may have been altered in
      Item::send().
    */
    str_buffer.set(buffer, sizeof(buffer), &my_charset_bin);
  }

  DBUG_RETURN(FALSE);
}


/**
  Send \\0 end terminated string.

  @param from	NullS or \\0 terminated string

  @note
    In most cases one should use store(from, length) instead of this function

  @retval
    0		ok
  @retval
    1		error
*/

bool Protocol::store(const char *from, CHARSET_INFO *cs)
{
  if (!from)
    return store_null();
  uint length= strlen(from);
  return store(from, length, cs);
}


/**
  Send a set of strings as one long string with ',' in between.
*/

bool Protocol::store(I_List<i_string>* str_list)
{
  char buf[256];
  String tmp(buf, sizeof(buf), &my_charset_bin);
  uint32 len;
  I_List_iterator<i_string> it(*str_list);
  i_string* s;

  tmp.length(0);
  while ((s=it++))
  {
    tmp.append(s->ptr);
    tmp.append(',');
  }
  if ((len= tmp.length()))
    len--;					// Remove last ','
  return store((char*) tmp.ptr(), len,  tmp.charset());
}

/****************************************************************************
  Functions to handle the simple (default) protocol where everything is
  This protocol is the one that is used by default between the MySQL server
  and client when you are not using prepared statements.

  All data are sent as 'packed-string-length' followed by 'string-data'
****************************************************************************/

#ifndef EMBEDDED_LIBRARY
void Protocol_text::prepare_for_resend()
{
  packet->length(0);
#ifndef DBUG_OFF
  field_pos= 0;
#endif
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

bool Protocol::store_string_aux(const char *from, size_t length,
                                CHARSET_INFO *fromcs, CHARSET_INFO *tocs)
{
  /* 'tocs' is set 0 when client issues SET character_set_results=NULL */
  if (tocs && !my_charset_same(fromcs, tocs) &&
      fromcs != &my_charset_bin &&
      tocs != &my_charset_bin)
  {
    /* Store with conversion */
    return net_store_data((uchar*) from, length, fromcs, tocs);
  }
  /* Store without conversion */
  return net_store_data((uchar*) from, length);
}


bool Protocol_text::store(const char *from, size_t length,
                          CHARSET_INFO *fromcs, CHARSET_INFO *tocs)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DECIMAL ||
              field_types[field_pos] == MYSQL_TYPE_BIT ||
              field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL ||
	      (field_types[field_pos] >= MYSQL_TYPE_ENUM &&
	       field_types[field_pos] <= MYSQL_TYPE_GEOMETRY));
  field_pos++;
#endif
  return store_string_aux(from, length, fromcs, tocs);
}


bool Protocol_text::store(const char *from, size_t length,
                          CHARSET_INFO *fromcs)
{
  CHARSET_INFO *tocs= this->thd->variables.character_set_results;
#ifndef DBUG_OFF
  DBUG_PRINT("info", ("Protocol_text::store field %u (%u): %.*s", field_pos,
                      field_count, (int) length, (length == 0 ? "" : from)));
  DBUG_ASSERT(field_pos < field_count);
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DECIMAL ||
              field_types[field_pos] == MYSQL_TYPE_BIT ||
              field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL ||
              field_types[field_pos] == MYSQL_TYPE_NEWDATE ||
	      (field_types[field_pos] >= MYSQL_TYPE_ENUM &&
	       field_types[field_pos] <= MYSQL_TYPE_GEOMETRY));
  field_pos++;
#endif
  return store_string_aux(from, length, fromcs, tocs);
}


bool Protocol_text::store_tiny(longlong from)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 || field_types[field_pos] == MYSQL_TYPE_TINY);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((uchar*) buff,
			(size_t) (int10_to_str((int) from, buff, -10) - buff));
}


bool Protocol_text::store_short(longlong from)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_YEAR ||
	      field_types[field_pos] == MYSQL_TYPE_SHORT);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((uchar*) buff,
			(size_t) (int10_to_str((int) from, buff, -10) -
                                  buff));
}


bool Protocol_text::store_long(longlong from)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
              field_types[field_pos] == MYSQL_TYPE_INT24 ||
              field_types[field_pos] == MYSQL_TYPE_LONG);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((uchar*) buff,
			(size_t) (int10_to_str((long int)from, buff,
                                               (from <0)?-10:10)-buff));
}


bool Protocol_text::store_longlong(longlong from, bool unsigned_flag)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_LONGLONG);
  field_pos++;
#endif
  char buff[22];
  return net_store_data((uchar*) buff,
			(size_t) (longlong10_to_str(from,buff,
                                                    unsigned_flag ? 10 : -10)-
                                  buff));
}


bool Protocol_text::store_decimal(const my_decimal *d)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
              field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL);
  field_pos++;
#endif
  char buff[DECIMAL_MAX_STR_LENGTH];
  String str(buff, sizeof(buff), &my_charset_bin);
  (void) my_decimal2string(E_DEC_FATAL_ERROR, d, 0, 0, 0, &str);
  return net_store_data((uchar*) str.ptr(), str.length());
}


bool Protocol_text::store(float from, uint32 decimals, String *buffer)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_FLOAT);
  field_pos++;
#endif
  buffer->set_real((double) from, decimals, thd->charset());
  return net_store_data((uchar*) buffer->ptr(), buffer->length());
}


bool Protocol_text::store(double from, uint32 decimals, String *buffer)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DOUBLE);
  field_pos++;
#endif
  buffer->set_real(from, decimals, thd->charset());
  return net_store_data((uchar*) buffer->ptr(), buffer->length());
}


bool Protocol_text::store(Field *field)
{
  if (field->is_null())
    return store_null();
#ifndef DBUG_OFF
  field_pos++;
#endif
  char buff[MAX_FIELD_WIDTH];
  String str(buff,sizeof(buff), &my_charset_bin);
  CHARSET_INFO *tocs= this->thd->variables.character_set_results;
#ifndef DBUG_OFF
  TABLE *table= field->table;
  my_bitmap_map *old_map= 0;
  if (table->file)
    old_map= dbug_tmp_use_all_columns(table, table->read_set);
#endif

  field->val_str(&str);
#ifndef DBUG_OFF
  if (old_map)
    dbug_tmp_restore_column_map(table->read_set, old_map);
#endif

  return store_string_aux(str.ptr(), str.length(), str.charset(), tocs);
}


/**
  @todo
    Second_part format ("%06") needs to change when 
    we support 0-6 decimals for time.
*/

bool Protocol_text::store(MYSQL_TIME *tm)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DATETIME ||
	      field_types[field_pos] == MYSQL_TYPE_TIMESTAMP);
  field_pos++;
#endif
  char buff[40];
  uint length;
  length= sprintf(buff, "%04d-%02d-%02d %02d:%02d:%02d",
                  (int) tm->year, (int) tm->month,
                  (int) tm->day, (int) tm->hour,
                  (int) tm->minute, (int) tm->second);
  if (tm->second_part)
    length+= sprintf(buff+length, ".%06d", (int) tm->second_part);
  return net_store_data((uchar*) buff, length);
}


bool Protocol_text::store_date(MYSQL_TIME *tm)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DATE);
  field_pos++;
#endif
  char buff[MAX_DATE_STRING_REP_LENGTH];
  size_t length= my_date_to_str(tm, buff);
  return net_store_data((uchar*) buff, length);
}


/**
  @todo 
    Second_part format ("%06") needs to change when 
    we support 0-6 decimals for time.
*/

bool Protocol_text::store_time(MYSQL_TIME *tm)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_TIME);
  field_pos++;
#endif
  char buff[40];
  uint length;
  uint day= (tm->year || tm->month) ? 0 : tm->day;
  length= sprintf(buff, "%s%02ld:%02d:%02d", tm->neg ? "-" : "",
                  (long) day*24L+(long) tm->hour, (int) tm->minute,
                  (int) tm->second);
  if (tm->second_part)
    length+= sprintf(buff+length, ".%06d", (int) tm->second_part);
  return net_store_data((uchar*) buff, length);
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
  DBUG_ASSERT(sp_params->elements ==
              thd->lex->prepared_stmt_params.elements);

  List_iterator_fast<Item_param> item_param_it(*sp_params);
  List_iterator_fast<LEX_STRING> user_var_name_it(thd->lex->prepared_stmt_params);

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
    if (suv->fix_fields(thd, NULL))
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

bool Protocol_binary::prepare_for_send(uint num_columns)
{
  Protocol::prepare_for_send(num_columns);
  bit_fields= (field_count+9)/8;
  return packet->alloc(bit_fields+1);

  /* prepare_for_resend will be called after this one */
}


void Protocol_binary::prepare_for_resend()
{
  packet->length(bit_fields+1);
  bzero((uchar*) packet->ptr(), 1+bit_fields);
  field_pos=0;
}


bool Protocol_binary::store(const char *from, size_t length,
                            CHARSET_INFO *fromcs)
{
  CHARSET_INFO *tocs= thd->variables.character_set_results;
  field_pos++;
  return store_string_aux(from, length, fromcs, tocs);
}

bool Protocol_binary::store(const char *from, size_t length,
                            CHARSET_INFO *fromcs, CHARSET_INFO *tocs)
{
  field_pos++;
  return store_string_aux(from, length, fromcs, tocs);
}

bool Protocol_binary::store_null()
{
  uint offset= (field_pos+2)/8+1, bit= (1 << ((field_pos+2) & 7));
  /* Room for this as it's allocated in prepare_for_send */
  char *to= (char*) packet->ptr()+offset;
  *to= (char) ((uchar) *to | (uchar) bit);
  field_pos++;
  return 0;
}


bool Protocol_binary::store_tiny(longlong from)
{
  char buff[1];
  field_pos++;
  buff[0]= (uchar) from;
  return packet->append(buff, sizeof(buff), PACKET_BUFFER_EXTRA_ALLOC);
}


bool Protocol_binary::store_short(longlong from)
{
  field_pos++;
  char *to= packet->prep_append(2, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int2store(to, (int) from);
  return 0;
}


bool Protocol_binary::store_long(longlong from)
{
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int4store(to, from);
  return 0;
}


bool Protocol_binary::store_longlong(longlong from, bool unsigned_flag)
{
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int8store(to, from);
  return 0;
}

bool Protocol_binary::store_decimal(const my_decimal *d)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
              field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL);
  field_pos++;
#endif
  char buff[DECIMAL_MAX_STR_LENGTH];
  String str(buff, sizeof(buff), &my_charset_bin);
  (void) my_decimal2string(E_DEC_FATAL_ERROR, d, 0, 0, 0, &str);
  return store(str.ptr(), str.length(), str.charset());
}

bool Protocol_binary::store(float from, uint32 decimals, String *buffer)
{
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  float4store(to, from);
  return 0;
}


bool Protocol_binary::store(double from, uint32 decimals, String *buffer)
{
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  float8store(to, from);
  return 0;
}


bool Protocol_binary::store(Field *field)
{
  /*
    We should not increment field_pos here as send_binary() will call another
    protocol function to do this for us
  */
  if (field->is_null())
    return store_null();
  return field->send_binary(this);
}


bool Protocol_binary::store(MYSQL_TIME *tm)
{
  char buff[12],*pos;
  uint length;
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
  tm->hour= tm->minute= tm->second=0;
  tm->second_part= 0;
  return Protocol_binary::store(tm);
}


bool Protocol_binary::store_time(MYSQL_TIME *tm)
{
  char buff[13], *pos;
  uint length;
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
  if (!(thd->client_capabilities & CLIENT_PS_MULTI_RESULTS))
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
    is used in send_result_set_metadata().
  */

  thd->server_status|= SERVER_PS_OUT_PARAMS | SERVER_MORE_RESULTS_EXISTS;

  /* Send meta-data. */
  if (send_result_set_metadata(&out_param_lst, SEND_NUM_ROWS | SEND_EOF))
    return TRUE;

  /* Send data. */

  prepare_for_resend();

  if (send_result_set_row(&out_param_lst))
    return TRUE;

  if (write())
    return TRUE;

  /* Restore THD::server_status. */
  thd->server_status&= ~SERVER_PS_OUT_PARAMS;

  /*
    Reset SERVER_MORE_RESULTS_EXISTS bit, because this is the last packet
    for sure.
  */
  thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

  /* Send EOF-packet. */
  net_send_eof(thd, thd->server_status, 0);

  return FALSE;
}
