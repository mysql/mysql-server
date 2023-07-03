/* Copyright (c) 2003, 2023, Oracle and/or its affiliates.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_cache.h"                         /* Query_cache_block */

class Querycache_stream
{
  uchar *cur_data;
  uchar *data_end;
  Query_cache_block *block;
  uint headers_len;
public:
#ifndef NDEBUG
  Query_cache_block *first_block;
  size_t stored_size;
#endif
  Querycache_stream(Query_cache_block *ini_block, uint ini_headers_len) :
    block(ini_block), headers_len(ini_headers_len)    
  {
    cur_data= ((uchar*)block)+headers_len;
    data_end= cur_data + (block->used-headers_len);
#ifndef NDEBUG
    first_block= ini_block;
    stored_size= 0;
#endif
  }
  void use_next_block(bool writing)
  {
    /*
      This shouldn't be called if there is only one block, or to loop
      around to the first block again. That means we're trying to write
      more data than we allocated space for.
    */
    assert(block->next != block);
    assert(block->next != first_block);

    block= block->next;
    /*
      While writing, update the type of each block as we write to it.
      While reading, make sure that the block is of the expected type.
    */
    if (writing)
      block->type= Query_cache_block::RES_CONT;
    else
      assert(block->type == Query_cache_block::RES_CONT);

    cur_data= ((uchar*)block)+headers_len;
    data_end= cur_data + (block->used-headers_len);
  }

  void store_uchar(uchar c);
  void store_short(ushort s);
  void store_int(uint i);
  void store_ll(ulonglong ll);
  void store_str_only(const char *str, size_t str_len);
  void store_str(const char *str, size_t str_len);
  void store_safe_str(const char *str, size_t str_len);

  uchar load_uchar();
  ushort load_short();
  uint load_int();
  ulonglong load_ll();
  void load_str_only(char *buffer, size_t str_len);
  char *load_str(MEM_ROOT *alloc, uint *str_len);
  int load_safe_str(MEM_ROOT *alloc, char **str, uint *str_len);
  int load_column(MEM_ROOT *alloc, char **column);
};

uint emb_count_querycache_size(THD *thd);
int emb_load_querycache_result(THD *thd, Querycache_stream *src);
void emb_store_querycache_result(Querycache_stream *dst, THD* thd);
bool net_send_eof(THD *thd, uint server_status, uint total_warn_count);
