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

void send_error(NET *net, uint sql_errno, const char *err)
{
  uint length;
  char buff[MYSQL_ERRMSG_SIZE+2];
  THD *thd=current_thd;
  DBUG_ENTER("send_error");
  DBUG_PRINT("enter",("sql_errno: %d  err: %s", sql_errno,
		      err ? err : net->last_error[0] ?
		      net->last_error : "NULL")); 

  if (thd)
    thd->query_error = 1; // needed to catch query errors during replication
  if (!err)
  {
    if (sql_errno)
      err=ER(sql_errno);
    else if (!err)
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
    if (thd && thd->bootstrap)
    {
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
    length=strlen(err);
    set_if_smaller(length,MYSQL_ERRMSG_SIZE);
  }
  VOID(net_write_command(net,(uchar) 255,(char*) err,length));
  if (thd)
    thd->fatal_error=0;			// Error message is given
  DBUG_VOID_RETURN;
}

/**
** write error package and flush to client
** It's a little too low level, but I don't want to allow another buffer
*/
/* VARARGS3 */

void
net_printf(NET *net, uint errcode, ...)
{
  va_list args;
  uint length,offset;
  const char *format,*text_pos;
  int head_length= NET_HEADER_SIZE;
  THD *thd=current_thd;
  DBUG_ENTER("net_printf");
  DBUG_PRINT("enter",("message: %u",errcode));

  if(thd) thd->query_error = 1;
  // if we are here, something is wrong :-)
  
  va_start(args,errcode);
  format=ER(errcode);
  offset= net->return_errno ? 2 : 0;
  text_pos=(char*) net->buff+head_length+offset+1;
  (void) vsprintf(my_const_cast(char*) (text_pos),format,args);
  length=strlen((char*) text_pos);
  if (length >= sizeof(net->last_error))
    length=sizeof(net->last_error)-1;		/* purecov: inspected */
  va_end(args);

  if (net->vio == 0)
  {
    if (thd && thd->bootstrap)
    {
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
  if (thd)
    thd->fatal_error=0;			// Error message is given
  DBUG_VOID_RETURN;
}


void
send_ok(NET *net,ha_rows affected_rows,ulonglong id,const char *message)
{
  if(net->no_send_ok)
    return;
  
  char buff[MYSQL_ERRMSG_SIZE+10],*pos;
  DBUG_ENTER("send_ok");
  buff[0]=0;					// No fields
  pos=net_store_length(buff+1,(ulonglong) affected_rows);
  pos=net_store_length(pos, (ulonglong) id);
  if (net->return_status)
  {
    int2store(pos,*net->return_status);
    pos+=2;
  }
  if (message)
    pos=net_store_data((char*) pos,message);
  if (net->vio != 0)
  {
    VOID(my_net_write(net,buff,(uint) (pos-buff)));
    VOID(net_flush(net));
  }
  DBUG_VOID_RETURN;
}

void
send_eof(NET *net,bool no_flush)
{
  static char eof_buff[1]= { (char) 254 };	/* Marker for end of fields */
  DBUG_ENTER("send_eof");
  if (net->vio != 0)
  {
    VOID(my_net_write(net,eof_buff,1));
    if (!no_flush)
      VOID(net_flush(net));
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
  uint length=strlen(from);
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
  uint length=strlen(from);
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
  String tmp(buf, sizeof(buf));
  tmp.length(0);
  I_List_iterator<i_string> it(*str_list);
  i_string* s;

  while((s=it++))
    {
      if(tmp.length())
	tmp.append(',');
      tmp.append(s->ptr);
    }

  return net_store_data(packet, (char*)tmp.ptr(), tmp.length());
}
