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

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <mysql.h>

bool Protocol_cursor::send_fields(List<Item> *list, uint flags)
{
  List_iterator_fast<Item> it(*list);
  Item                     *item;
  MYSQL_FIELD              *client_field;
  DBUG_ENTER("Protocol_cursor::send_fields");

  if (prepare_for_send(list))
    return FALSE;

  fields= (MYSQL_FIELD *)alloc_root(alloc, sizeof(MYSQL_FIELD) * field_count);
  if (!fields)
    goto err;

  for (client_field= fields; (item= it++) ; client_field++)
  {
    Send_field server_field;
    item->make_field(&server_field);

    client_field->db=	  strdup_root(alloc, server_field.db_name);
    client_field->table=  strdup_root(alloc, server_field.table_name);
    client_field->name=   strdup_root(alloc, server_field.col_name);
    client_field->org_table= strdup_root(alloc, server_field.org_table_name);
    client_field->org_name=  strdup_root(alloc, server_field.org_col_name);
    client_field->catalog= strdup_root(alloc, "");
    client_field->length= server_field.length;
    client_field->type=   server_field.type;
    client_field->flags= server_field.flags;
    client_field->decimals= server_field.decimals;
    client_field->db_length=		strlen(client_field->db);
    client_field->table_length=		strlen(client_field->table);
    client_field->name_length=		strlen(client_field->name);
    client_field->org_name_length=	strlen(client_field->org_name);
    client_field->org_table_length=	strlen(client_field->org_table);
    client_field->catalog_length=       0;
    client_field->charsetnr=		server_field.charsetnr;
    
    if (INTERNAL_NUM_FIELD(client_field))
      client_field->flags|= NUM_FLAG;

    if (flags & (uint) Protocol::SEND_DEFAULTS)
    {
      char buff[80];
      String tmp(buff, sizeof(buff), default_charset_info), *res;

      if (!(res=item->val_str(&tmp)))
	client_field->def= (char*) "";
      else
	client_field->def= strmake_root(alloc, res->ptr(), res->length());
    }
    else
      client_field->def=0;
    client_field->max_length= 0;
  }

  DBUG_RETURN(FALSE);

err:
  my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES),
             MYF(0));	/* purecov: inspected */
  DBUG_RETURN(TRUE);				/* purecov: inspected */
}


/* Get the length of next field. Change parameter to point at fieldstart */

bool Protocol_cursor::write()
{
  byte *cp= (byte *)packet->ptr();
  byte *end_pos= (byte *)packet->ptr() + packet->length();
  ulong len;
  MYSQL_FIELD *cur_field= fields;
  MYSQL_FIELD *fields_end= fields + field_count;
  MYSQL_ROWS *new_record;
  byte **data_tmp;
  byte *to;

  new_record= (MYSQL_ROWS *)alloc_root(alloc, 
    sizeof(MYSQL_ROWS) + (field_count + 1)*sizeof(char *) + packet->length());
  if (!new_record)
    goto err;
  data_tmp= (byte **)(new_record + 1);
  new_record->data= (char **)data_tmp;

  to= (byte *)data_tmp + (field_count + 1)*sizeof(char *);

  for (; cur_field < fields_end; ++cur_field, ++data_tmp)
  {
    if ((len= net_field_length((uchar **)&cp)) == 0 ||
	len == NULL_LENGTH)
    {
      *data_tmp= 0;
    }
    else
    {
      if ((long)len > (end_pos - cp))
      {
        // TODO error signal      send_error(thd, CR_MALFORMED_PACKET);
	return TRUE;
      }
      *data_tmp= to;
      memcpy(to,(char*) cp,len);
      to[len]=0;
      to+=len+1;
      cp+=len;
      if (cur_field->max_length < len)
	cur_field->max_length=len;
    }
  }
  *data_tmp= 0;

  *prev_record= new_record;
  prev_record= &new_record->next;
  new_record->next= NULL;
  row_count++;
  return FALSE;
 err:
  // TODO error signal      send_error(thd, ER_OUT_OF_RESOURCES);
  return TRUE;
}
