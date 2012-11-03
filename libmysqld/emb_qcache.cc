/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "my_global.h"                          // HAVE_*

#ifdef HAVE_QUERY_CACHE
#include <mysql.h>
#include "emb_qcache.h"
#include "embedded_priv.h"
#include "sql_class.h"                          // THD

void Querycache_stream::store_uchar(uchar c)
{
  if (data_end == cur_data)
    use_next_block(TRUE);
  *(cur_data++)= c;
#ifndef DBUG_OFF
  stored_size++;
#endif
}

void Querycache_stream::store_short(ushort s)
{
#ifndef DBUG_OFF
  stored_size+= 2;
#endif
  if (data_end - cur_data > 1)
  {
    int2store(cur_data, s);
    cur_data+= 2;
    return;
  }
  if (data_end == cur_data)
  {
    use_next_block(TRUE);
    int2store(cur_data, s);
    cur_data+= 2;
    return;
  }
  *cur_data= ((uchar *)(&s))[0];
  use_next_block(TRUE);
  *(cur_data++)= ((uchar *)(&s))[1];
}

void Querycache_stream::store_int(uint i)
{
#ifndef DBUG_OFF
  stored_size+= 4;
#endif
  size_t rest_len= data_end - cur_data;
  if (rest_len > 3)
  {
    int4store(cur_data, i);
    cur_data+= 4;
    return;
  }
  if (!rest_len)
  {
    use_next_block(TRUE);
    int4store(cur_data, i);
    cur_data+= 4;
    return;
  }
  char buf[4];
  int4store(buf, i);
  memcpy(cur_data, buf, rest_len);
  use_next_block(TRUE);
  memcpy(cur_data, buf+rest_len, 4-rest_len);
  cur_data+= 4-rest_len;
}

void Querycache_stream::store_ll(ulonglong ll)
{
#ifndef DBUG_OFF
  stored_size+= 8;
#endif
  size_t rest_len= data_end - cur_data;
  if (rest_len > 7)
  {
    int8store(cur_data, ll);
    cur_data+= 8;
    return;
  }
  if (!rest_len)
  {
    use_next_block(TRUE);
    int8store(cur_data, ll);
    cur_data+= 8;
    return;
  }
  memcpy(cur_data, &ll, rest_len);
  use_next_block(TRUE);
  memcpy(cur_data, ((uchar*)&ll)+rest_len, 8-rest_len);
  cur_data+= 8-rest_len;
}

void Querycache_stream::store_str_only(const char *str, uint str_len)
{
#ifndef DBUG_OFF
  stored_size+= str_len;
#endif
  do
  {
    size_t rest_len= data_end - cur_data;
    if (rest_len >= str_len)
    {
      memcpy(cur_data, str, str_len);
      cur_data+= str_len;
      return;
    }
    memcpy(cur_data, str, rest_len);
    use_next_block(TRUE);
    str_len-= rest_len;
    str+= rest_len;
  } while(str_len);
}

void Querycache_stream::store_str(const char *str, uint str_len)
{
  store_int(str_len);
  store_str_only(str, str_len);
}

void Querycache_stream::store_safe_str(const char *str, uint str_len)
{
  if (str)
  {
    store_int(str_len+1);
    store_str_only(str, str_len);
  }
  else
    store_int(0);
}

uchar Querycache_stream::load_uchar()
{
  if (cur_data == data_end)
    use_next_block(FALSE);
  return *(cur_data++);
}

ushort Querycache_stream::load_short()
{
  ushort result;
  if (data_end-cur_data > 1)
  {
    result= uint2korr(cur_data);
    cur_data+= 2;
    return result;
  }
  if (data_end == cur_data)
  {
    use_next_block(FALSE);
    result= uint2korr(cur_data);
    cur_data+= 2;
    return result;
  }
  ((uchar*)&result)[0]= *cur_data;
  use_next_block(FALSE);
  ((uchar*)&result)[1]= *(cur_data++);
  return result;
}

uint Querycache_stream::load_int()
{
  int result;
  size_t rest_len= data_end - cur_data;
  if (rest_len > 3)
  {
    result= uint4korr(cur_data);
    cur_data+= 4;
    return result;
  }
  if (!rest_len)
  {
    use_next_block(FALSE);
    result= uint4korr(cur_data);
    cur_data+= 4;
    return result;
  }
  char buf[4], *buf_p= buf;
  memcpy(buf, cur_data, rest_len);
  use_next_block(FALSE);
  memcpy(buf+rest_len, cur_data, 4-rest_len);
  cur_data+= 4-rest_len;
  result= uint4korr(buf_p);
  return result;
}

ulonglong Querycache_stream::load_ll()
{
  ulonglong result;
  size_t rest_len= data_end - cur_data;
  if (rest_len > 7)
  {
    result= uint8korr(cur_data);
    cur_data+= 8;
    return result;
  }
  if (!rest_len)
  {
    use_next_block(FALSE);
    result= uint8korr(cur_data);
    cur_data+= 8;
    return result;
  }
  memcpy(&result, cur_data, rest_len);
  use_next_block(FALSE);
  memcpy(((uchar*)&result)+rest_len, cur_data, 8-rest_len);
  cur_data+= 8-rest_len;
  return result;
}

void Querycache_stream::load_str_only(char *buffer, uint str_len)
{
  do
  {
    size_t rest_len= data_end - cur_data;
    if (rest_len >= str_len)
    {
      memcpy(buffer, cur_data, str_len);
      cur_data+= str_len;
      buffer+= str_len;
      break;
    }
    memcpy(buffer, cur_data, rest_len);
    use_next_block(FALSE);
    str_len-= rest_len;
    buffer+= rest_len;
  } while(str_len);
  *buffer= 0;
}

char *Querycache_stream::load_str(MEM_ROOT *alloc, uint *str_len)
{
  char *result;
  *str_len= load_int();
  if (!(result= (char*) alloc_root(alloc, *str_len + 1)))
    return 0;
  load_str_only(result, *str_len);
  return result;
}

int Querycache_stream::load_safe_str(MEM_ROOT *alloc, char **str, uint *str_len)
{
  if (!(*str_len= load_int()))
  {
    *str= NULL;
    return 0;
  }
  (*str_len)--;
  if (!(*str= (char*) alloc_root(alloc, *str_len + 1)))
    return 1;
  load_str_only(*str, *str_len);
  return 0;
}

int Querycache_stream::load_column(MEM_ROOT *alloc, char** column)
{
  int len;
  if (!(len = load_int()))
  {
    *column= NULL;
    return 0;
  }
  len--;
  if (!(*column= (char *)alloc_root(alloc, len + sizeof(uint) + 1)))
    return 1;
  *((uint*)*column)= len;
  (*column)+= sizeof(uint);
  load_str_only(*column, len);
  return 1;
}

uint emb_count_querycache_size(THD *thd)
{
  uint result= 0;
  MYSQL_FIELD *field;
  MYSQL_FIELD *field_end;
  MYSQL_ROWS *cur_row;
  my_ulonglong n_rows;
  MYSQL_DATA *data= thd->first_data;

  while (data->embedded_info->next)
    data= data->embedded_info->next;
  field= data->embedded_info->fields_list;
  field_end= field + data->fields;
  
  if (!field)
    return result;
  *data->embedded_info->prev_ptr= NULL; // this marks the last record
  cur_row= data->data;
  n_rows= data->rows;
  /* n_fields + n_rows + field_info * n_fields */
  result+= (uint) (4+8 + 42*data->fields);

  for(; field < field_end; field++)
  {
    result+= field->name_length + field->table_length +
      field->org_name_length + field->org_table_length + field->db_length +
      field->catalog_length;
    if (field->def)
      result+= field->def_length;
  }
  
  if (thd->protocol == &thd->protocol_binary)
  {
    result+= (uint) (4*n_rows);
    for (; cur_row; cur_row=cur_row->next)
      result+= cur_row->length;
  }
  else
  {
    result+= (uint) (4*n_rows*data->fields);
    for (; cur_row; cur_row=cur_row->next)
    {
      MYSQL_ROW col= cur_row->data;
      MYSQL_ROW col_end= col + data->fields;
      for (; col < col_end; col++)
        if (*col)
          result+= *(uint *)((*col) - sizeof(uint));
    }
  }
  return result;
}

void emb_store_querycache_result(Querycache_stream *dst, THD *thd)
{
  MYSQL_FIELD *field;
  MYSQL_FIELD *field_end;
  MYSQL_ROWS *cur_row;
  my_ulonglong n_rows;
  MYSQL_DATA *data= thd->first_data;

  DBUG_ENTER("emb_store_querycache_result");

  while (data->embedded_info->next)
    data= data->embedded_info->next;
  field= data->embedded_info->fields_list;
  field_end= field + data->fields;

  if (!field)
    DBUG_VOID_RETURN;

  *data->embedded_info->prev_ptr= NULL; // this marks the last record
  cur_row= data->data;
  n_rows= data->rows;

  dst->store_int((uint)data->fields);
  dst->store_ll((ulonglong)n_rows);

  for(; field < field_end; field++)
  {
    dst->store_int((uint)field->length);
    dst->store_int((uint)field->max_length);
    dst->store_uchar((uchar)field->type);
    dst->store_short((ushort)field->flags);
    dst->store_short((ushort)field->charsetnr);
    dst->store_uchar((uchar)field->decimals);
    dst->store_str(field->name, field->name_length);
    dst->store_str(field->table, field->table_length);
    dst->store_str(field->org_name, field->org_name_length);
    dst->store_str(field->org_table, field->org_table_length);
    dst->store_str(field->db, field->db_length);
    dst->store_str(field->catalog, field->catalog_length);
    dst->store_safe_str(field->def, field->def_length);
  }
  
  if (thd->protocol == &thd->protocol_binary)
  {
    for (; cur_row; cur_row=cur_row->next)
      dst->store_str((char *) cur_row->data, cur_row->length);
  }
  else
  {
    for (; cur_row; cur_row=cur_row->next)
    {
      MYSQL_ROW col= cur_row->data;
      MYSQL_ROW col_end= col + data->fields;
      for (; col < col_end; col++)
      {
        uint len= *col ? *(uint *)((*col) - sizeof(uint)) : 0;
        dst->store_safe_str(*col, len);
      }
    }
  }
  DBUG_ASSERT(emb_count_querycache_size(thd) == dst->stored_size);
  DBUG_VOID_RETURN;
}

int emb_load_querycache_result(THD *thd, Querycache_stream *src)
{
  MYSQL_DATA *data= thd->alloc_new_dataset();
  MYSQL_FIELD *field;
  MYSQL_FIELD *field_end;
  MEM_ROOT *f_alloc;
  MYSQL_ROWS *row, *end_row;
  MYSQL_ROWS **prev_row;
  ulonglong rows;
  MYSQL_ROW columns;
  DBUG_ENTER("emb_load_querycache_result");

  if (!data)
    goto err;
  init_alloc_root(&data->alloc, 8192,0);
  f_alloc= &data->alloc;

  data->fields= src->load_int();
  rows= src->load_ll();

  if (!(field= (MYSQL_FIELD *)
        alloc_root(f_alloc,data->fields*sizeof(MYSQL_FIELD))))
    goto err;
  data->embedded_info->fields_list= field;
  for(field_end= field+data->fields; field < field_end; field++)
  {
    field->length= src->load_int();
    field->max_length= (unsigned int)src->load_int();
    field->type= (enum enum_field_types)src->load_uchar();
    field->flags= (unsigned int)src->load_short();
    field->charsetnr= (unsigned int)src->load_short();
    field->decimals= src->load_uchar();

    if (!(field->name= src->load_str(f_alloc, &field->name_length))          ||
        !(field->table= src->load_str(f_alloc,&field->table_length))         ||
        !(field->org_name= src->load_str(f_alloc, &field->org_name_length))  ||
        !(field->org_table= src->load_str(f_alloc, &field->org_table_length))||
        !(field->db= src->load_str(f_alloc, &field->db_length))              ||
        !(field->catalog= src->load_str(f_alloc, &field->catalog_length))    ||
        src->load_safe_str(f_alloc, &field->def, &field->def_length))
      goto err;
  }
  
  data->rows= rows;
  if (!rows)
    goto return_ok;
  if (thd->protocol == &thd->protocol_binary)
  {
    uint length;
    row= (MYSQL_ROWS *)alloc_root(&data->alloc,
                                  (size_t) (rows * sizeof(MYSQL_ROWS)));
    end_row= row + rows;
    data->data= row;

    for (prev_row= &row->next; row < end_row; prev_row= &row->next, row++)
    {
      *prev_row= row;
      row->data= (MYSQL_ROW) src->load_str(&data->alloc, &length);
      row->length= length;
    }
  }
  else
  {
    row= (MYSQL_ROWS *)alloc_root(&data->alloc,
        (uint) (rows * sizeof(MYSQL_ROWS) +
          rows*(data->fields+1)*sizeof(char*)));
    end_row= row + rows;
    columns= (MYSQL_ROW)end_row;

    data->data= row;

    for (prev_row= &row->next; row < end_row; prev_row= &row->next, row++)
    {
      *prev_row= row;
      row->data= columns;
      MYSQL_ROW col_end= columns + data->fields;
      for (; columns < col_end; columns++)
        src->load_column(&data->alloc, columns);

      *(columns++)= NULL;
    }
  }
  *prev_row= NULL;
  data->embedded_info->prev_ptr= prev_row;
return_ok:
  net_send_eof(thd, thd->server_status,
               thd->get_stmt_da()->current_statement_cond_count());
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

#endif /*HAVE_QUERY_CACHE*/

