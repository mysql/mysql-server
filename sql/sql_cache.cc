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
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>

#define SQL_CACHE_LENGTH 30			// 300 crashes apple gcc.

HASH sql_cache;
static LEX lex_array_static[SQL_CACHE_LENGTH];
LEX * lex_array = lex_array_static;
int last_lex_array_item = SQL_CACHE_LENGTH - 1;

/* Function to return a text string from a LEX struct */
static byte *cache_key(const byte *record, uint *length, my_bool not_used)
{
#ifdef QQ
	LEX *lex=(LEX*) record;
	*length = lex->sql_query_length;
	// *length = strlen(lex->ptr);
	return (byte*) lex->sql_query_text;
	// return (byte*) lex->ptr;
#endif
  return 0;
}

/* At the moment we do not really want to do anything upon delete */
static void free_cache_entry(void *entry)
{
}

/* Initialization of the SQL cache hash -- should be called during
   the bootstrap stage */
bool sql_cache_init(void)
{
  if (query_buff_size)
  {
    VOID(hash_init(&sql_cache, 4096, 0, 0,
		   cache_key,
		   (void (*)(void*)) free_cache_entry,
		   0));
  }
  return 0;
}

/* Clearing the SQL cache hash -- during shutdown */
void sql_cache_free(void)
{
  hash_free(&sql_cache);
}

/* Finds whether the  SQL command is already in the cache, at any case
	establishes correct LEX structure in the THD (either from
	cache or a new one) */

int sql_cache_hit(THD *thd, char *sql, uint length)
{
#ifdef QQ
  LEX *ptr;
  ptr = (LEX *)hash_search(&sql_cache, sql, length);
  if (ptr) {
    fprintf(stderr, "Query `%s' -- hit in the cache (%p)\n", ptr->sql_query_text, ptr);
    thd->lex_ptr = ptr;
    ptr->thd = thd;
  } else {
    thd->lex_ptr = ptr = lex_array + last_lex_array_item--;

    lex_start(thd, (uchar *)sql, length);

    if (hash_insert(&sql_cache, (const byte *)ptr)) {
      fprintf(stderr, "Out of memory during hash_insert?\n");
    }
    fprintf(stderr, "Query `%s' not found in the cache -- insert %p from slot %d\n", thd->lex_ptr->ptr, ptr, last_lex_array_item+1);
    if (!hash_search(&sql_cache, sql, length)) {
      fprintf(stderr, "I just enterred a hash key but it's not where -- what's that?\n");
    } else {
      fprintf(stderr, "Inserted to cache\n");
    }
    return 0;
  }
#endif
  return 1;
}
