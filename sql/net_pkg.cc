/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


#include "mysql_priv.h"
#include <stdarg.h>

	/* Send a error string to client */

void send_error(THD *thd, uint sql_errno, const char *err)
{
  uint length;
  char buff[MYSQL_ERRMSG_SIZE+2];
  NET *net= &thd->net;
  DBUG_ENTER("send_error");
  DBUG_PRINT("enter",("sql_errno: %d  err: %s", sql_errno,
		      err ? err : net->last_error[0] ?
		      net->last_error : "NULL"));

  query_cache_abort(net);
  if (!err)
  {
    if (sql_errno)
      err=ER(sql_errno);
    else
    {
      if ((err=net->last_error)[0])
	sql_errno=net->last_errno;
      else
      {
	sql_errno=ER_UNKNOWN_ERROR;
	err=ER(sql_errno);	 /* purecov: inspected */
      }
    }
  }
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
    length= (uint) (strmake(buff+2,err,MYSQL_ERRMSG_SIZE-1) - buff);
    err=buff;
  }
  else
  {
    length=(uint) strlen(err);
    set_if_smaller(length,MYSQL_ERRMSG_SIZE-1);
  }
  VOID(net_write_command(net,(uchar) 255, "", 0, (char*) err,length));
  thd->fatal_error=0;			// Error message is given
  DBUG_VOID_RETURN;
}

/*
  Send an error to the client when a connection is forced close
  This is used by mysqld.cc, which doesn't have a THD
*/

void net_send_error(NET *net, uint sql_errno, const char *err)
{
  char buff[2];
  uint length;
  DBUG_ENTER("send_net_error");

  int2store(buff,sql_errno);
  length=(uint) strlen(err);
  set_if_smaller(length,MYSQL_ERRMSG_SIZE-1);
  net_write_command(net,(uchar) 255, buff, 2, err, length);
  DBUG_VOID_RETURN;
}


/*
  Send a warning to the end user

  SYNOPSIS
    send_warning()
    thd			Thread handler
    sql_errno		Warning number (error message)
    err			Error string.  If not set, use ER(sql_errno)

  DESCRIPTION
    Register the warning so that the user can get it with mysql_warnings()
    Send an ok (+ warning count) to the end user.
*/

void send_warning(THD *thd, uint sql_errno, const char *err)
{
  DBUG_ENTER("send_warning");  
  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, sql_errno,
	       err ? err : ER(sql_errno));
  send_ok(thd);
  DBUG_VOID_RETURN;
}


/*
   Write error package and flush to client
   It's a little too low level, but I don't want to use another buffer for
   this
*/

void
net_printf(THD *thd, uint errcode, ...)
{
  va_list args;
  uint length,offset;
  const char *format,*text_pos;
  int head_length= NET_HEADER_SIZE;
  NET *net= &thd->net;
  DBUG_ENTER("net_printf");
  DBUG_PRINT("enter",("message: %u",errcode));

  query_cache_abort(net);	// Safety
  va_start(args,errcode);
  /*
    The following is needed to make net_printf() work with 0 argument for
    errorcode and use the argument after that as the format string. This
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
  offset= net->return_errno ? 2 : 0;
  text_pos=(char*) net->buff+head_length+offset+1;
  (void) vsprintf(my_const_cast(char*) (text_pos),format,args);
  length=(uint) strlen((char*) text_pos);
  if (length >= sizeof(net->last_error))
    length=sizeof(net->last_error)-1;		/* purecov: inspected */
  va_end(args);

  if (net->vio == 0)
  {
    if (thd->bootstrap)
    {
      /* In bootstrap it's ok to print on stderr */
      fprintf(stderr,"ERROR: %d  %s\n",errcode,text_pos);
      thd->fatal_error=1;
    }
    DBUG_VOID_RETURN;
  }

  int3store(net->buff,length+1+offset);
  net->buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
  net->buff[head_length]=(uchar) 255;		// Error package
  if (offset)
    int2store(text_pos-2, errcode);
  VOID(net_real_write(net,(char*) net->buff,length+head_length+1+offset));
  thd->fatal_error=0;			// Error message is given
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

void
send_ok(THD *thd, ha_rows affected_rows, ulonglong id, const char *message)
{
  NET *net= &thd->net;
  if (net->no_send_ok || !net->vio)	// hack for re-parsing queries
    return;

  char buff[MYSQL_ERRMSG_SIZE+10],*pos;
  DBUG_ENTER("send_ok");
  buff[0]=0;					// No fields
  pos=net_store_length(buff+1,(ulonglong) affected_rows);
  pos=net_store_length(pos, (ulonglong) id);
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
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
    pos=net_store_data((char*) pos,message);
  VOID(my_net_write(net,buff,(uint) (pos-buff)));
  VOID(net_flush(net));
  DBUG_VOID_RETURN;
}


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
			For flags like SERVER_STATUS_MORE_RESULTS

    Note that the warning count will not be sent if 'no_flush' is set as
    we don't want to report the warning count until all data is sent to the
    client.
*/    

void
send_eof(THD *thd, bool no_flush)
{
  static char eof_buff[1]= { (char) 254 };	/* Marker for end of fields */
  NET *net= &thd->net;
  DBUG_ENTER("send_eof");
  if (net->vio != 0)
  {
    if (!no_flush && (thd->client_capabilities & CLIENT_PROTOCOL_41))
    {
      char buff[5];
      uint tmp= min(thd->total_warn_count, 65535);
      buff[0]=254;
      int2store(buff+1, tmp);
      int2store(buff+3, 0);			// No flags yet
      VOID(my_net_write(net,buff,5));
      VOID(net_flush(net));
    }
    else
    {
      VOID(my_net_write(net,eof_buff,1));
      if (!no_flush)
	VOID(net_flush(net));
    }
  }
  DBUG_VOID_RETURN;
}


/****************************************************************************
** Store a field length in logical packet
****************************************************************************/

char *
net_store_length(char *pkg, ulonglong length)
{
  uchar *packet=(uchar*) pkg;
  if (length < LL(251))
  {
    *packet=(uchar) length;
    return (char*) packet+1;
  }
  /* 251 is reserved for NULL */
  if (length < LL(65536))
  {
    *packet++=252;
    int2store(packet,(uint) length);
    return (char*) packet+2;
  }
  if (length < LL(16777216))
  {
    *packet++=253;
    int3store(packet,(ulong) length);
    return (char*) packet+3;
  }
  *packet++=254;
  int8store(packet,length);
  return (char*) packet+9;
}

char *
net_store_length(char *pkg, uint length)
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

/* The following will only be used for short strings < 65K */
char *
net_store_data(char *to,const char *from)
{
  uint length=(uint) strlen(from);
  to=net_store_length(to,length);
  memcpy(to,from,length);
  return to+length;
}


char *
net_store_data(char *to,int32 from)
{
  char buff[20];
  uint length=(uint) (int10_to_str(from,buff,10)-buff);
  to=net_store_length(to,length);
  memcpy(to,buff,length);
  return to+length;
}

char *
net_store_data(char *to,longlong from)
{
  char buff[22];
  uint length=(uint) (longlong10_to_str(from,buff,10)-buff);
  to=net_store_length(to,length);
  memcpy(to,buff,length);
  return to+length;
}


bool net_store_null(String *packet)
{
  return packet->append((char) 251);
}

bool
net_store_data(String *packet,const char *from,uint length)
{
  ulong packet_length=packet->length();
  if (packet_length+5+length > packet->alloced_length() &&
      packet->realloc(packet_length+5+length))
    return 1;
  char *to=(char*) net_store_length((char*) packet->ptr()+packet_length,
				    (ulonglong) length);
  memcpy(to,from,length);
  packet->length((uint) (to+length-packet->ptr()));
  return 0;
}

/* The following is only used at short, null terminated data */

bool
net_store_data(String *packet,const char *from)
{
  uint length=(uint) strlen(from);
  uint packet_length=packet->length();
  if (packet_length+5+length > packet->alloced_length() &&
      packet->realloc(packet_length+5+length))
    return 1;
  char *to=(char*) net_store_length((char*) packet->ptr()+packet_length,
				    length);
  memcpy(to,from,length);
  packet->length((uint) (to+length-packet->ptr()));
  return 0;
}


bool
net_store_data(String *packet,uint32 from)
{
  char buff[20];
  return net_store_data(packet,(char*) buff,
			(uint) (int10_to_str(from,buff,10)-buff));
}

bool
net_store_data(String *packet, longlong from)
{
  char buff[22];
  return net_store_data(packet,(char*) buff,
			(uint) (longlong10_to_str(from,buff,10)-buff));
}

bool
net_store_data(String *packet,struct tm *tmp)
{
  char buff[20];
  sprintf(buff,"%04d-%02d-%02d %02d:%02d:%02d",
	  ((int) (tmp->tm_year+1900)) % 10000,
	  (int) tmp->tm_mon+1,
	  (int) tmp->tm_mday,
	  (int) tmp->tm_hour,
	  (int) tmp->tm_min,
	  (int) tmp->tm_sec);
  return net_store_data(packet,(char*) buff,19);
}

bool net_store_data(String* packet, I_List<i_string>* str_list)
{
  char buf[256];
  String tmp(buf, sizeof(buf), default_charset_info);
  tmp.length(0);
  I_List_iterator<i_string> it(*str_list);
  i_string* s;

  while ((s=it++))
  {
    if (tmp.length())
      tmp.append(',');
    tmp.append(s->ptr);
  }

  return net_store_data(packet, (char*)tmp.ptr(), tmp.length());
}

/*
** translate and store data; These are mainly used by the SHOW functions
*/

bool
net_store_data(String *packet,CONVERT *convert, const char *from,uint length)
{
  if (convert)
    return convert->store(packet, from, length);
  return net_store_data(packet,from,length);
}

bool
net_store_data(String *packet, CONVERT *convert, const char *from)
{
  uint length=(uint) strlen(from);
  if (convert)
    return convert->store(packet, from, length);
  return net_store_data(packet,from,length);
}

/*
  Function called by my_net_init() to set some check variables
*/

extern "C" {
void my_net_local_init(NET *net)
{
  net->max_packet=   (uint) global_system_variables.net_buffer_length;
  net->read_timeout= (uint) global_system_variables.net_read_timeout;
  net->write_timeout=(uint) global_system_variables.net_write_timeout;
  net->retry_count=  (uint) global_system_variables.net_retry_count;
  net->max_packet_size= max(global_system_variables.net_buffer_length,
			    global_system_variables.max_allowed_packet);
}
}
