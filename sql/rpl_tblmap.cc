/* Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql_priv.h"
#include "my_global.h"                          // HAVE_REPLICATION

#ifdef HAVE_REPLICATION

#include "rpl_tblmap.h"
#ifndef MYSQL_CLIENT
#include "table.h"
#endif

#ifdef MYSQL_CLIENT
#define MAYBE_TABLE_NAME(T) ("")
#else
#define MAYBE_TABLE_NAME(T) ((T) ? (T)->s->table_name.str : "<>")
#endif
#define TABLE_ID_HASH_SIZE 32
#define TABLE_ID_CHUNK 256

table_mapping::table_mapping()
  : m_free(0)
{
  /*
    No "free_element" function for entries passed here, as the entries are
    allocated in a MEM_ROOT (freed as a whole in the destructor), they cannot
    be freed one by one.
    Note that below we don't test if my_hash_init() succeeded. This
    constructor is called at startup only.
  */
  (void) my_hash_init(&m_table_ids,&my_charset_bin,TABLE_ID_HASH_SIZE,
		   offsetof(entry,table_id),sizeof(ulonglong),
		   0,0,0);
  /* We don't preallocate any block, this is consistent with m_free=0 above */
  init_alloc_root(&m_mem_root, TABLE_ID_HASH_SIZE*sizeof(entry), 0);
}

table_mapping::~table_mapping()
{
#ifdef MYSQL_CLIENT
  clear_tables();
#endif
  my_hash_free(&m_table_ids);
  free_root(&m_mem_root, MYF(0));
}

TABLE* table_mapping::get_table(ulonglong table_id)
{
  DBUG_ENTER("table_mapping::get_table(ulonglong)");
  DBUG_PRINT("enter", ("table_id: %llu", table_id));
  entry *e= find_entry(table_id);
  if (e) 
  {
    DBUG_PRINT("info", ("tid %llu -> table 0x%lx (%s)",
			table_id, (long) e->table,
			MAYBE_TABLE_NAME(e->table)));
    DBUG_RETURN(e->table);
  }

  DBUG_PRINT("info", ("tid %llu is not mapped!", table_id));
  DBUG_RETURN(NULL);
}

/*
  Called when we are out of table id entries. Creates TABLE_ID_CHUNK
  new entries, chain them and attach them at the head of the list of free
  (free for use) entries.
*/
int table_mapping::expand()
{
  /*
    If we wanted to use "tmp= new (&m_mem_root) entry[TABLE_ID_CHUNK]",
    we would have to make "entry" derive from Sql_alloc but then it would not
    be a POD anymore and we want it to be (see rpl_tblmap.h). So we allocate
    in C.
  */
  entry *tmp= (entry *)alloc_root(&m_mem_root, TABLE_ID_CHUNK*sizeof(entry));
  if (tmp == NULL)
    return ERR_MEMORY_ALLOCATION; // Memory allocation failed

  /* Find the end of this fresh new array of free entries */
  entry *e_end= tmp+TABLE_ID_CHUNK-1;
  for (entry *e= tmp; e < e_end; e++)
    e->next= e+1;
  e_end->next= m_free;
  m_free= tmp;
  return 0;
}

int table_mapping::set_table(ulonglong table_id, TABLE* table)
{
  DBUG_ENTER("table_mapping::set_table(ulong,TABLE*)");
  DBUG_PRINT("enter", ("table_id: %llu  table: 0x%lx (%s)",
		       table_id, 
		       (long) table, MAYBE_TABLE_NAME(table)));
  entry *e= find_entry(table_id);
  if (e == 0)
  {
    if (m_free == 0 && expand())
      DBUG_RETURN(ERR_MEMORY_ALLOCATION); // Memory allocation failed      
    e= m_free;
    m_free= m_free->next;
  }
  else
  {
#ifdef MYSQL_CLIENT
    free_table_map_log_event(e->table);
#endif
    my_hash_delete(&m_table_ids,(uchar *)e);
  }
  e->table_id= table_id;
  e->table= table;
  if (my_hash_insert(&m_table_ids,(uchar *)e))
  {
    /* we add this entry to the chain of free (free for use) entries */
    e->next= m_free;
    m_free= e;
    DBUG_RETURN(ERR_MEMORY_ALLOCATION);
  }

  DBUG_PRINT("info", ("tid %llu -> table 0x%lx (%s)",
		      table_id, (long) e->table,
		      MAYBE_TABLE_NAME(e->table)));
  DBUG_RETURN(0);		// All OK
}

int table_mapping::remove_table(ulonglong table_id)
{
  entry *e= find_entry(table_id);
  if (e)
  {
    my_hash_delete(&m_table_ids,(uchar *)e);
    /* we add this entry to the chain of free (free for use) entries */
    e->next= m_free;
    m_free= e;
    return 0;			// All OK
  }
  return 1;			// No table to remove
}

/*
  Puts all entries into the list of free-for-use entries (does not free any
  memory), and empties the hash.
*/
void table_mapping::clear_tables()
{
  DBUG_ENTER("table_mapping::clear_tables()");
  for (uint i= 0; i < m_table_ids.records; i++)
  {
    entry *e= (entry *)my_hash_element(&m_table_ids, i);
#ifdef MYSQL_CLIENT
    free_table_map_log_event(e->table);
#endif
    e->next= m_free;
    m_free= e;
  }
  my_hash_reset(&m_table_ids);
  DBUG_VOID_RETURN;
}

#endif
