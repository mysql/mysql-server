/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "protocol.h"

#include "messages.h"

#include <mysql_com.h>
#include <m_string.h>


static char eof_buff[1]= { (char) 254 };        /* Marker for end of fields */
static const char ERROR_PACKET_CODE= (char) 255;


int net_send_ok(struct st_net *net, unsigned long connection_id,
                const char *message)
{
  /*
            The format of a packet
    1                             packet type code
    1-9                           affected rows count
    1-9                           connection id
    2                             thread return status
    2                             warning count
    1-9 + message length          message to send (isn't stored if no message)
  */
  Buffer buff;
  char *pos= buff.buffer;

  /* check that we have space to hold mandatory fields */
  buff.reserve(0, 23);

  enum { OK_PACKET_CODE= 0 };
  *pos++= OK_PACKET_CODE;
  pos= net_store_length(pos, (ulonglong) 0);
  pos= net_store_length(pos, (ulonglong) connection_id);
  int2store(pos, *net->return_status);
  pos+= 2;
  /* We don't support warnings, so store 0 for total warning count */
  int2store(pos, 0);
  pos+= 2;

  uint position= pos - buff.buffer; /* we might need it for message */

  if (message != NULL)
  {
    buff.reserve(position, 9 + strlen(message));
    store_to_protocol_packet(&buff, message, &position);
  }

  return my_net_write(net, buff.buffer, position) || net_flush(net);
}


int net_send_error(struct st_net *net, uint sql_errno)
{
  const char *err= message(sql_errno);
  char buff[1 +                                 // packet type code
            2 +                                 // sql error number
            1 + SQLSTATE_LENGTH +               // sql state
            MYSQL_ERRMSG_SIZE];                 // message
  char *pos= buff;

  *pos++= ERROR_PACKET_CODE;
  int2store(pos, sql_errno);
  pos+= 2;
  /* The first # is to make the protocol backward compatible */
  *pos++= '#';
  memcpy(pos, errno_to_sqlstate(sql_errno), SQLSTATE_LENGTH);
  pos+= SQLSTATE_LENGTH;
  pos= strmake(pos, err, MYSQL_ERRMSG_SIZE - 1) + 1;
  return my_net_write(net, buff, pos - buff) || net_flush(net);
}


int net_send_error_323(struct st_net *net, uint sql_errno)
{
  const char *err= message(sql_errno);
  char buff[1 +                                 // packet type code
            2 +                                 // sql error number
            MYSQL_ERRMSG_SIZE];                 // message
  char *pos= buff;

  *pos++= ERROR_PACKET_CODE;
  int2store(pos, sql_errno);
  pos+= 2;
  pos= strmake(pos, err, MYSQL_ERRMSG_SIZE - 1) + 1;
  return my_net_write(net, buff, pos - buff) || net_flush(net);
}

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


int store_to_protocol_packet(Buffer *buf, const char *string, uint *position,
                             uint string_len)
{
  uint currpos;

  /* reserve max amount of bytes needed to store length */
  if (buf->reserve(*position, 9))
    goto err;
  currpos= (net_store_length(buf->buffer + *position,
                             (ulonglong) string_len) - buf->buffer);
  if (buf->append(currpos, string, string_len))
    goto err;
  *position= *position + string_len + (currpos - *position);

  return 0;
err:
  return 1;
}


int store_to_protocol_packet(Buffer *buf, const char *string, uint *position)
{
  uint string_len;

  string_len= strlen(string);
  return store_to_protocol_packet(buf, string, position, string_len);
}


int send_eof(struct st_net *net)
{
  uchar buff[1 + /* eof packet code */
	     2 + /* warning count */
	     2]; /* server status */

  buff[0]=254;
  int2store(buff+1, 0);
  int2store(buff+3, 0);
  return my_net_write(net, (char*) buff, sizeof buff);
}

int send_fields(struct st_net *net, LIST *fields)
{
  LIST *tmp= fields;
  Buffer send_buff;
  char small_buff[4];
  uint position= 0;
  NAME_WITH_LENGTH *field;

  /* send the number of fileds */
  net_store_length(small_buff, (uint) list_length(fields));
  if (my_net_write(net, small_buff, (uint) 1))
    goto err;

  while (tmp)
  {
    position= 0;
    field= (NAME_WITH_LENGTH *) tmp->data;

    store_to_protocol_packet(&send_buff,
                             (char*) "", &position);    /* catalog name */
    store_to_protocol_packet(&send_buff,
                             (char*) "", &position);    /* db name */
    store_to_protocol_packet(&send_buff,
                             (char*) "", &position);    /* table name */
    store_to_protocol_packet(&send_buff,
                             (char*) "", &position);    /* table name alias */
    store_to_protocol_packet(&send_buff,
                             field->name, &position);   /* column name */
    store_to_protocol_packet(&send_buff,
                             field->name, &position);   /* column name alias */
    send_buff.reserve(position, 12);
    if (send_buff.is_error())
      goto err;
    send_buff.buffer[position++]= 12;
    int2store(send_buff.buffer + position, 1);          /* charsetnr */
    int4store(send_buff.buffer + position + 2,
              field->length);                           /* field length */
    send_buff.buffer[position+6]= (char) FIELD_TYPE_STRING;    /* type */
    int2store(send_buff.buffer + position + 7, 0);      /* flags */
    send_buff.buffer[position + 9]= (char) 0;           /* decimals */
    send_buff.buffer[position + 10]= 0;
    send_buff.buffer[position + 11]= 0;
    position+= 12;
    if (my_net_write(net, send_buff.buffer, (uint) position+1))
      goto err;
    tmp= list_rest(tmp);
  }

  if (my_net_write(net, eof_buff, 1))
    goto err;
  return 0;

err:
  return 1;
}
