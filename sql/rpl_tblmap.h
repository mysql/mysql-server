/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TABLE_MAPPING_H
#define TABLE_MAPPING_H

/* Forward declarations */
#ifndef MYSQL_CLIENT
struct st_table;
typedef st_table TABLE;
#else
class Table_map_log_event;
typedef Table_map_log_event TABLE;
void free_table_map_log_event(TABLE *table);
#endif


/*
  CLASS table_mapping

  RESPONSIBILITIES
    The table mapping is used to map table id's to table pointers

  COLLABORATION
    RELAY_LOG    For mapping table id:s to tables when receiving events.
 */

/*
  Guilhem to Mats:
  in the table_mapping class, the memory is allocated and never freed (until
  destruction). So this is a good candidate for allocating inside a MEM_ROOT:
  it gives the efficient allocation in chunks (like in expand()). So I have
  introduced a MEM_ROOT.

  Note that inheriting from Sql_alloc had no effect: it has effects only when
  "ptr= new table_mapping" is called, and this is never called. And it would
  then allocate from thd->mem_root which is a highly volatile object (reset
  from example after executing each query, see dispatch_command(), it has a
  free_root() at end); as the table_mapping object is supposed to live longer
  than a query, it was dangerous.
  A dedicated MEM_ROOT needs to be used, see below.
*/

class table_mapping {

private:
  MEM_ROOT m_mem_root;

public:

  enum enum_error {
      ERR_NO_ERROR = 0,
      ERR_LIMIT_EXCEEDED,
      ERR_MEMORY_ALLOCATION
  };

  table_mapping();
  ~table_mapping();

  TABLE* get_table(ulong table_id);

  int       set_table(ulong table_id, TABLE* table);
  int       remove_table(ulong table_id);
  void      clear_tables();
  ulong     count() const { return m_table_ids.records; }

private:
  /*
    This is a POD (Plain Old Data).  Keep it that way (we apply offsetof() to
    it, which only works for PODs)
  */
  struct entry { 
    ulong table_id;
    union {
      TABLE *table;
      entry *next;
    };
  };

  entry *find_entry(ulong table_id)
  {
    return (entry *)hash_search(&m_table_ids,
				(uchar*)&table_id,
				sizeof(table_id));
  }
  int expand();

  /*
    Head of the list of free entries; "free" in the sense that it's an
    allocated entry free for use, NOT in the sense that it's freed
    memory.
  */
  entry *m_free;

  /* Correspondance between an id (a number) and a TABLE object */
  HASH m_table_ids;
};

#endif
