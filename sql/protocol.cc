/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Low level functions for storing data to be send to the MySQL client
  The actual communction is handled by the net_xxx functions in net_serv.cc
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sp_rcontext.h"
#include <stdarg.h>

static const unsigned int PACKET_BUFFER_EXTRA_ALLOC= 1024;
static void write_eof_packet(THD *thd, NET *net);
void net_send_error_packet(THD *thd, uint sql_errno, const char *err);

#ifndef EMBEDDED_LIBRARY
bool Protocol::net_store_data(const char *from, uint length)
#else
bool Protocol_prep::net_store_data(const char *from, uint length)
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
  char *to=(char*) net_store_length((char*) packet->ptr()+packet_length,
				    (ulonglong) length);
  memcpy(to,from,length);
  packet->length((uint) (to+length-packet->ptr()));
  return 0;
}


	/* Send a error string to client */

void net_send_error(THD *thd, uint sql_errno, const char *err)
{
  NET *net= &thd->net;
  bool generate_warning= thd->killed != THD::KILL_CONNECTION;
  DBUG_ENTER("net_send_error");
  DBUG_PRINT("enter",("sql_errno: %d  err: %s", sql_errno,
		      err ? err : net->last_error[0] ?
		      net->last_error : "NULL"));

  if (net && net->no_send_error)
  {
    thd->clear_error();
    DBUG_PRINT("info", ("sending error messages prohibited"));
    DBUG_VOID_RETURN;
  }
  if (thd->spcont && thd->spcont->find_handler(sql_errno,
                                               MYSQL_ERROR::WARN_LEVEL_ERROR))
  {
    if (! thd->spcont->found_handler_here())
      thd->net.report_error= 1; /* Make "select" abort correctly */ 
    DBUG_VOID_RETURN;
  }
  thd->query_error=  1; // needed to catch query errors during replication
  if (!err)
  {
    if (sql_errno)
      err=ER(sql_errno);
    else
    {
      if ((err=net->last_error)[0])
      {
	sql_errno=net->last_errno;
        generate_warning= 0;            // This warning has already been given
      }
      else
      {
	sql_errno=ER_UNKNOWN_ERROR;
	err=ER(sql_errno);	 /* purecov: inspected */
      }
    }
  }

  if (generate_warning)
  {
    /* Error that we have not got with my_error() */
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, sql_errno, err);
  }

  net_send_error_packet(thd, sql_errno, err);

  thd->is_fatal_error=0;			// Error message is given
  thd->net.report_error= 0;

  /* Abort multi-result sets */
  thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;
  DBUG_VOID_RETURN;
}

/*
   Write error package and flush to client
   It's a little too low level, but I don't want to use another buffer for
   this
*/

void
net_printf_error(THD *thd, uint errcode, ...)
{
  va_list args;
  uint length,offset;
  const char *format;
#ifndef EMBEDDED_LIBRARY
  const char *text_pos;
  int head_length= NET_HEADER_SIZE;
#else
  char text_pos[1024];
#endif
  NET *net= &thd->net;

  DBUG_ENTER("net_printf_error");
  DBUG_PRINT("enter",("message: %u",errcode));

  if (net && net->no_send_error)
  {
    thd->clear_error();
    DBUG_PRINT("info", ("sending error messages prohibited"));
    DBUG_VOID_RETURN;
  }

  if (thd->spcont && thd->spcont->find_handler(errcode,
                                               MYSQL_ERROR::WARN_LEVEL_ERROR))
  {
    if (! thd->spcont->found_handler_here())
      thd->net.report_error= 1; /* Make "select" abort correctly */ 
    DBUG_VOID_RETURN;
  }
  thd->query_error=  1; // needed to catch query errors during replication
#ifndef EMBEDDED_LIBRARY
  query_cache_abort(net);	// Safety
#endif
  va_start(args,errcode);
  /*
    The following is needed to make net_printf_error() work with 0 argument
    for errorcode and use the argument after that as the format string. This
    is useful for rare errors that are not worth the hassle to put in
    errmsg.sys, but at the same time, the message is not fixed text
  */
  if (errcode)
    format= ER(errcode);
  else
  {
    format=va_arg(args,char*);
    errcode= ER_UNKNOWN_ERROR;
  }
  offset= (net->return_errno ?
	   ((thd->client_capabilities & CLIENT_PROTOCOL_41) ?
	    2+SQLSTATE_LENGTH+1 : 2) : 0);
#ifndef EMBEDDED_LIBRARY
  text_pos=(char*) net->buff + head_length + offset + 1;
  length= (uint) ((char*)net->buff_end - text_pos);
#else
  length=sizeof(text_pos)-1;
#endif
  length=my_vsnprintf(my_const_cast(char*) (text_pos),
                      min(length, sizeof(net->last_error)),
                      format,args);
  va_end(args);

  /* Replication slave relies on net->last_* to see if there was error */
  net->last_errno= errcode;
  strmake(net->last_error, text_pos, sizeof(net->last_error)-1);

#ifndef EMBEDDED_LIBRARY
  if (net->vio == 0)
  {
    if (thd->bootstrap)
    {
      /*
	In bootstrap it's ok to print on stderr
	This may also happen when we get an error from a slave thread
      */
      fprintf(stderr,"ERROR: %d  %s\n",errcode,text_pos);
      thd->fatal_error();
    }
    DBUG_VOID_RETURN;
  }

  int3store(net->buff,length+1+offset);
  net->buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
  net->buff[head_length]=(uchar) 255;		// Error package
  if (offset)
  {
    uchar *pos= net->buff+head_length+1;
    int2store(pos, errcode);
    if (thd->client_capabilities & CLIENT_PROTOCOL_41)
    {
      pos[2]= '#';      /* To make the protocol backward compatible */
      memcpy(pos+3, mysql_errno_to_sqlstate(errcode), SQLSTATE_LENGTH);
    }
  }
  VOID(net_real_write(net,(char*) net->buff,length+head_length+1+offset));
#else
  net->last_errno= errcode;
  strmake(net->last_error, text_pos, length);
  strmake(net->sqlstate, mysql_errno_to_sqlstate(errcode), SQLSTATE_LENGTH);
#endif
  if (thd->killed != THD::KILL_CONNECTION)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, errcode,
                 text_pos ? text_pos : ER(errcode));
  thd->is_fatal_error=0;			// Error message is given
  DBUG_VOID_RETURN;
}

/*
  Return ok to the client.

  SYNOPSIS
    send_ok()
    thd			Thread handler
    affected_rows	Number of rows changed by statement
    id			Auto_increment id for first row (if used)
    message		Message to send to the client (Used by mysql_status)

  DESCRIPTION
    The ok packet has the following structure

    0			Marker (1 byte)
    affected_rows	Stored in 1-9 bytes
    id			Stored in 1-9 bytes
    server_status	Copy of thd->server_status;  Can be used by client
			to check if we are inside an transaction
			New in 4.0 protocol
    warning_count	Stored in 2 bytes; New in 4.1 protocol
    message		Stored as packed length (1-9 bytes) + message
			Is not stored if no message

   If net->no_send_ok return without sending packet
*/    

#ifndef EMBEDDED_LIBRARY
void
send_ok(THD *thd, ha_rows affected_rows, ulonglong id, const char *message)
{
  NET *net= &thd->net;
  char buff[MYSQL_ERRMSG_SIZE+10],*pos;
  DBUG_ENTER("send_ok");

  if (net->no_send_ok || !net->vio)	// hack for re-parsing queries
  {
    DBUG_PRINT("info", ("no send ok: %s, vio present: %s",
                        (net->no_send_ok ? "YES" : "NO"),
                        (net->vio ? "YES" : "NO")));
    DBUG_VOID_RETURN;
  }

  buff[0]=0;					// No fields
  pos=net_store_length(buff+1,(ulonglong) affected_rows);
  pos=net_store_length(pos, (ulonglong) id);
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    DBUG_PRINT("info",
	       ("affected_rows: %lu  id: %lu  status: %u  warning_count: %u",
		(ulong) affected_rows,		
		(ulong) id,
		(uint) (thd->server_status & 0xffff),
		(uint) thd->total_warn_count));
    int2store(pos,thd->server_status);
    pos+=2;

    /* We can only return up to 65535 warnings in two bytes */
    uint tmp= min(thd->total_warn_count, 65535);
    int2store(pos, tmp);
    pos+= 2;
  }
  else if (net->return_status)			// For 4.0 protocol
  {
    int2store(pos,thd->server_status);
    pos+=2;
  }
  if (message)
    pos=net_store_data((char*) pos, message, strlen(message));
  VOID(my_net_write(net,buff,(uint) (pos-buff)));
  VOID(net_flush(net));
  /* We can't anymore send an error to the client */
  thd->net.report_error= 0;
  thd->net.no_send_error= 1;
  DBUG_PRINT("info", ("OK sent, so no more error sending allowed"));

  DBUG_VOID_RETURN;
}

static char eof_buff[1]= { (char) 254 };        /* Marker for end of fields */

/*
  Send eof (= end of result set) to the client

  SYNOPSIS
    send_eof()
    thd			Thread handler
    no_flush		Set to 1 if there will be more data to the client,
			like in send_fields().

  DESCRIPTION
    The eof packet has the following structure

    254			Marker (1 byte)
    warning_count	Stored in 2 bytes; New in 4.1 protocol
    status_flag		Stored in 2 bytes;
			For flags like SERVER_MORE_RESULTS_EXISTS

    Note that the warning count will not be sent if 'no_flush' is set as
    we don't want to report the warning count until all data is sent to the
    client.
*/    

void
send_eof(THD *thd)
{
  NET *net= &thd->net;
  DBUG_ENTER("send_eof");
  if (net->vio != 0 && !net->no_send_eof)
  {
    write_eof_packet(thd, net);
    VOID(net_flush(net));
    thd->net.no_send_error= 1;
    DBUG_PRINT("info", ("EOF sent, so no more error sending allowed"));
  }
  DBUG_VOID_RETURN;
}


/*
  Format EOF packet according to the current protocol and
  write it to the network output buffer.
*/

static void write_eof_packet(THD *thd, NET *net)
{
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    uchar buff[5];
    /*
      Don't send warn count during SP execution, as the warn_list
      is cleared between substatements, and mysqltest gets confused
    */
    uint tmp= (thd->spcont ? 0 : min(thd->total_warn_count, 65535));
    buff[0]= 254;
    int2store(buff+1, tmp);
    /*
      The following test should never be true, but it's better to do it
      because if 'is_fatal_error' is set the server is not going to execute
      other queries (see the if test in dispatch_command / COM_QUERY)
    */
    if (thd->is_fatal_error)
      thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;
    int2store(buff+3, thd->server_status);
    VOID(my_net_write(net, (char*) buff, 5));
  }
  else
    VOID(my_net_write(net, eof_buff, 1));
}

/*
    Please client to send scrambled_password in old format.
  SYNOPSYS
    send_old_password_request()
    thd thread handle
     
  RETURN VALUE
    0  ok
   !0  error
*/

bool send_old_password_request(THD *thd)
{
  NET *net= &thd->net;
  return my_net_write(net, eof_buff, 1) || net_flush(net);
}


void net_send_error_packet(THD *thd, uint sql_errno, const char *err)
{
  NET *net= &thd->net;
  uint length;
  char buff[MYSQL_ERRMSG_SIZE+2], *pos;

  DBUG_ENTER("send_error_packet");

  if (net->vio == 0)
  {
    if (thd->bootstrap)
    {
      /* In bootstrap it's ok to print on stderr */
      fprintf(stderr,"ERROR: %d  %s\n",sql_errno,err);
    }
    DBUG_VOID_RETURN;
  }

  if (net->return_errno)
  {				// new client code; Add errno before message
    int2store(buff,sql_errno);
    pos= buff+2;
    if (thd->client_capabilities & CLIENT_PROTOCOL_41)
    {
      /* The first # is to make the protocol backward compatible */
      buff[2]= '#';
      pos= strmov(buff+3, mysql_errno_to_sqlstate(sql_errno));
    }
    length= (uint) (strmake(pos, err, MYSQL_ERRMSG_SIZE-1) - buff);
    err=buff;
  }
  else
  {
    length=(uint) strlen(err);
    set_if_smaller(length,MYSQL_ERRMSG_SIZE-1);
  }
  VOID(net_write_command(net,(uchar) 255, "", 0, (char*) err,length));
  DBUG_VOID_RETURN;
}

#endif /* EMBEDDED_LIBRARY */

/*
  Faster net_store_length when we know that length is less than 65536.
  We keep a separate version for that range because it's widely used in
  libmysql.
  uint is used as agrument type because of MySQL type conventions:
  uint for 0..65536
  ulong for 0..4294967296
  ulonglong for bigger numbers.
*/

char *net_store_length(char *pkg, uint length)
{
  uchar *packet=(uchar*) pkg;
  if (length < 251)
  {
    *packet=(uchar) length;
    return (char*) packet+1;
  }
  *packet++=252;
  int2store(packet,(uint) length);
  return (char*) packet+2;
}


/****************************************************************************
  Functions used by the protocol functions (like send_ok) to store strings
  and numbers in the header result packet.
****************************************************************************/

/* The following will only be used for short strings < 65K */

char *net_store_data(char *to,const char *from, uint length)
{
  to=net_store_length(to,length);
  memcpy(to,from,length);
  return to+length;
}

char *net_store_data(char *to,int32 from)
{
  char buff[20];
  uint length=(uint) (int10_to_str(from,buff,10)-buff);
  to=net_store_length(to,length);
  memcpy(to,buff,length);
  return to+length;
}

char *net_store_data(char *to,longlong from)
{
  char buff[22];
  uint length=(uint) (longlong10_to_str(from,buff,10)-buff);
  to=net_store_length(to,length);
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


bool Protocol::flush()
{
#ifndef EMBEDDED_LIBRARY
  return net_flush(&thd->net);
#else
  return 0;
#endif
}

/*
  Send name and type of result to client.

  SYNOPSIS
    send_fields()
    THD		Thread data object
    list	List of items to send to client
    flag	Bit mask with the following functions:
		1 send number of rows
		2 send default values
                4 don't write eof packet

  DESCRIPTION
    Sum fields has table name empty and field_name.

  RETURN VALUES
    0	ok
    1	Error  (Note that in this case the error is not sent to the client)
*/

#ifndef EMBEDDED_LIBRARY
bool Protocol::send_fields(List<Item> *list, uint flags)
{
  List_iterator_fast<Item> it(*list);
  Item *item;
  char buff[80];
  String tmp((char*) buff,sizeof(buff),&my_charset_bin);
  Protocol_simple prot(thd);
  String *local_packet= prot.storage_packet();
  CHARSET_INFO *thd_charset= thd->variables.character_set_results;
  DBUG_ENTER("send_fields");

  if (flags & SEND_NUM_ROWS)
  {				// Packet with number of elements
    char *pos=net_store_length(buff, (uint) list->elements);
    (void) my_net_write(&thd->net, buff,(uint) (pos-buff));
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
      if (item->collation.collation == &my_charset_bin || thd_charset == NULL)
      {
        /* No conversion */
        int2store(pos, field.charsetnr);
        int4store(pos+2, field.length);
      }
      else
      {
        /* With conversion */
        uint max_char_len;
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
        */
        max_char_len= (field.type >= (int) MYSQL_TYPE_TINY_BLOB &&
                      field.type <= (int) MYSQL_TYPE_BLOB) ?
                      field.length / item->collation.collation->mbminlen :
                      field.length / item->collation.collation->mbmaxlen;
        int4store(pos+2, max_char_len * thd_charset->mbmaxlen);
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

#ifdef TO_BE_DELETED_IN_6
      if (!(thd->client_capabilities & CLIENT_LONG_FLAG))
      {
	pos[0]=3;
	int3store(pos+1,field.length);
	pos[4]=1;
	pos[5]=field.type;
	pos[6]=2;
	pos[7]= (char) field.flags;
	pos[8]= (char) field.decimals;
	pos+= 9;
      }
      else
#endif
      {
	pos[0]=3;
	int3store(pos+1,field.length);
	pos[4]=1;
	pos[5]=field.type;
	pos[6]=3;
	int2store(pos+7,field.flags);
	pos[9]= (char) field.decimals;
	pos+= 10;
      }
    }
    local_packet->length((uint) (pos - local_packet->ptr()));
    if (flags & SEND_DEFAULTS)
      item->send(&prot, &tmp);			// Send default value
    if (prot.write())
      break;					/* purecov: inspected */
#ifndef DBUG_OFF
    field_types[count++]= field.type;
#endif
  }

  if (flags & SEND_EOF)
    write_eof_packet(thd, &thd->net);
  DBUG_RETURN(prepare_for_send(list));

err:
  my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES),
             MYF(0));	/* purecov: inspected */
  DBUG_RETURN(1);				/* purecov: inspected */
}


bool Protocol::write()
{
  DBUG_ENTER("Protocol::write");
  DBUG_RETURN(my_net_write(&thd->net, packet->ptr(), packet->length()));
}
#endif /* EMBEDDED_LIBRARY */


/*
  Send \0 end terminated string

  SYNOPSIS
    store()
    from	NullS or \0 terminated string

  NOTES
    In most cases one should use store(from, length) instead of this function

  RETURN VALUES
    0		ok
    1		error
*/

bool Protocol::store(const char *from, CHARSET_INFO *cs)
{
  if (!from)
    return store_null();
  uint length= strlen(from);
  return store(from, length, cs);
}


/*
  Send a set of strings as one long string with ',' in between
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
void Protocol_simple::prepare_for_resend()
{
  packet->length(0);
#ifndef DBUG_OFF
  field_pos= 0;
#endif
}

bool Protocol_simple::store_null()
{
#ifndef DBUG_OFF
  field_pos++;
#endif
  char buff[1];
  buff[0]= (char)251;
  return packet->append(buff, sizeof(buff), PACKET_BUFFER_EXTRA_ALLOC);
}
#endif


/*
  Auxilary function to convert string to the given character set
  and store in network buffer.
*/

bool Protocol::store_string_aux(const char *from, uint length,
                                CHARSET_INFO *fromcs, CHARSET_INFO *tocs)
{
  /* 'tocs' is set 0 when client issues SET character_set_results=NULL */
  if (tocs && !my_charset_same(fromcs, tocs) &&
      fromcs != &my_charset_bin &&
      tocs != &my_charset_bin)
  {
    uint dummy_errors;
    return convert->copy(from, length, fromcs, tocs, &dummy_errors) ||
           net_store_data(convert->ptr(), convert->length());
  }
  return net_store_data(from, length);
}


bool Protocol_simple::store(const char *from, uint length,
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


bool Protocol_simple::store(const char *from, uint length,
			    CHARSET_INFO *fromcs)
{
  CHARSET_INFO *tocs= this->thd->variables.character_set_results;
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


bool Protocol_simple::store_tiny(longlong from)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 || field_types[field_pos] == MYSQL_TYPE_TINY);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((char*) buff,
			(uint) (int10_to_str((int) from,buff, -10)-buff));
}


bool Protocol_simple::store_short(longlong from)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_YEAR ||
	      field_types[field_pos] == MYSQL_TYPE_SHORT);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((char*) buff,
			(uint) (int10_to_str((int) from,buff, -10)-buff));
}


bool Protocol_simple::store_long(longlong from)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
              field_types[field_pos] == MYSQL_TYPE_INT24 ||
              field_types[field_pos] == MYSQL_TYPE_LONG);
  field_pos++;
#endif
  char buff[20];
  return net_store_data((char*) buff,
			(uint) (int10_to_str((long int)from,buff, (from <0)?-10:10)-buff));
}


bool Protocol_simple::store_longlong(longlong from, bool unsigned_flag)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_LONGLONG);
  field_pos++;
#endif
  char buff[22];
  return net_store_data((char*) buff,
			(uint) (longlong10_to_str(from,buff,
						  unsigned_flag ? 10 : -10)-
				buff));
}


bool Protocol_simple::store_decimal(const my_decimal *d)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
              field_types[field_pos] == MYSQL_TYPE_NEWDECIMAL);
  field_pos++;
#endif
  char buff[DECIMAL_MAX_STR_LENGTH];
  String str(buff, sizeof(buff), &my_charset_bin);
  (void) my_decimal2string(E_DEC_FATAL_ERROR, d, 0, 0, 0, &str);
  return net_store_data(str.ptr(), str.length());
}


bool Protocol_simple::store(float from, uint32 decimals, String *buffer)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_FLOAT);
  field_pos++;
#endif
  buffer->set((double) from, decimals, thd->charset());
  return net_store_data((char*) buffer->ptr(), buffer->length());
}


bool Protocol_simple::store(double from, uint32 decimals, String *buffer)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DOUBLE);
  field_pos++;
#endif
  buffer->set(from, decimals, thd->charset());
  return net_store_data((char*) buffer->ptr(), buffer->length());
}


bool Protocol_simple::store(Field *field)
{
  if (field->is_null())
    return store_null();
#ifndef DBUG_OFF
  field_pos++;
#endif
  char buff[MAX_FIELD_WIDTH];
  String str(buff,sizeof(buff), &my_charset_bin);
  CHARSET_INFO *tocs= this->thd->variables.character_set_results;

  field->val_str(&str);
  return store_string_aux(str.ptr(), str.length(), str.charset(), tocs);
}


/*
   TODO:
        Second_part format ("%06") needs to change when 
        we support 0-6 decimals for time.
*/


bool Protocol_simple::store(TIME *tm)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DATETIME ||
	      field_types[field_pos] == MYSQL_TYPE_TIMESTAMP);
  field_pos++;
#endif
  char buff[40];
  uint length;
  length= my_sprintf(buff,(buff, "%04d-%02d-%02d %02d:%02d:%02d",
			   (int) tm->year,
			   (int) tm->month,
			   (int) tm->day,
			   (int) tm->hour,
			   (int) tm->minute,
			   (int) tm->second));
  if (tm->second_part)
    length+= my_sprintf(buff+length,(buff+length, ".%06d", (int)tm->second_part));
  return net_store_data((char*) buff, length);
}


bool Protocol_simple::store_date(TIME *tm)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DATE);
  field_pos++;
#endif
  char buff[MAX_DATE_STRING_REP_LENGTH];
  int length= my_date_to_str(tm, buff);
  return net_store_data(buff, (uint) length);
}


/*
   TODO:
        Second_part format ("%06") needs to change when 
        we support 0-6 decimals for time.
*/

bool Protocol_simple::store_time(TIME *tm)
{
#ifndef DBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_TIME);
  field_pos++;
#endif
  char buff[40];
  uint length;
  uint day= (tm->year || tm->month) ? 0 : tm->day;
  length= my_sprintf(buff,(buff, "%s%02ld:%02d:%02d",
			   tm->neg ? "-" : "",
			   (long) day*24L+(long) tm->hour,
			   (int) tm->minute,
			   (int) tm->second));
  if (tm->second_part)
    length+= my_sprintf(buff+length,(buff+length, ".%06d", (int)tm->second_part));
  return net_store_data((char*) buff, length);
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

bool Protocol_prep::prepare_for_send(List<Item> *item_list)
{
  Protocol::prepare_for_send(item_list);
  bit_fields= (field_count+9)/8;
  if (packet->alloc(bit_fields+1))
    return 1;
  /* prepare_for_resend will be called after this one */
  return 0;
}


void Protocol_prep::prepare_for_resend()
{
  packet->length(bit_fields+1);
  bzero((char*) packet->ptr(), 1+bit_fields);
  field_pos=0;
}


bool Protocol_prep::store(const char *from, uint length, CHARSET_INFO *fromcs)
{
  CHARSET_INFO *tocs= thd->variables.character_set_results;
  field_pos++;
  return store_string_aux(from, length, fromcs, tocs);
}

bool Protocol_prep::store(const char *from,uint length,
			  CHARSET_INFO *fromcs, CHARSET_INFO *tocs)
{
  field_pos++;
  return store_string_aux(from, length, fromcs, tocs);
}

bool Protocol_prep::store_null()
{
  uint offset= (field_pos+2)/8+1, bit= (1 << ((field_pos+2) & 7));
  /* Room for this as it's allocated in prepare_for_send */
  char *to= (char*) packet->ptr()+offset;
  *to= (char) ((uchar) *to | (uchar) bit);
  field_pos++;
  return 0;
}


bool Protocol_prep::store_tiny(longlong from)
{
  char buff[1];
  field_pos++;
  buff[0]= (uchar) from;
  return packet->append(buff, sizeof(buff), PACKET_BUFFER_EXTRA_ALLOC);
}


bool Protocol_prep::store_short(longlong from)
{
  field_pos++;
  char *to= packet->prep_append(2, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int2store(to, (int) from);
  return 0;
}


bool Protocol_prep::store_long(longlong from)
{
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int4store(to, from);
  return 0;
}


bool Protocol_prep::store_longlong(longlong from, bool unsigned_flag)
{
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  int8store(to, from);
  return 0;
}

bool Protocol_prep::store_decimal(const my_decimal *d)
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

bool Protocol_prep::store(float from, uint32 decimals, String *buffer)
{
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  float4store(to, from);
  return 0;
}


bool Protocol_prep::store(double from, uint32 decimals, String *buffer)
{
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFER_EXTRA_ALLOC);
  if (!to)
    return 1;
  float8store(to, from);
  return 0;
}


bool Protocol_prep::store(Field *field)
{
  /*
    We should not increment field_pos here as send_binary() will call another
    protocol function to do this for us
  */
  if (field->is_null())
    return store_null();
  return field->send_binary(this);
}


bool Protocol_prep::store(TIME *tm)
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

bool Protocol_prep::store_date(TIME *tm)
{
  tm->hour= tm->minute= tm->second=0;
  tm->second_part= 0;
  return Protocol_prep::store(tm);
}


bool Protocol_prep::store_time(TIME *tm)
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
