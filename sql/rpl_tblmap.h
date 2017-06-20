/* Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_MAPPING_H
#define TABLE_MAPPING_H

#include <stddef.h>
#include <sys/types.h>

#include "map_helpers.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "template_utils.h"

/* Forward declarations */
#ifdef MYSQL_SERVER
struct TABLE;
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

  TABLE* get_table(ulonglong table_id);

  int       set_table(ulonglong table_id, TABLE* table);
  int       remove_table(ulonglong table_id);
  void      clear_tables();
  ulong     count() const { return m_table_ids.size(); }

private:

  struct entry { 
    ulonglong table_id;
    union {
      TABLE *table;
      entry *next;
    };
  };

  int expand();

  /*
    Head of the list of free entries; "free" in the sense that it's an
    allocated entry free for use, NOT in the sense that it's freed
    memory.
  */
  entry *m_free;

  /*
    Correspondence between an id (a number) and a TABLE object.

    No destructor for entries passed here, as the entries are allocated in a
    MEM_ROOT (freed as a whole in the destructor), they cannot be freed one by
    one.
  */
  malloc_unordered_map<ulonglong, entry *> m_table_ids;
};

#endif
