/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_event_data.h"

#include <table.h>


Ndb_event_data::Ndb_event_data(NDB_SHARE *the_share) :
  shadow_table(NULL),
  share(the_share),
  pk_bitmap(NULL)
{
  ndb_value[0]= NULL;
  ndb_value[1]= NULL;
}


Ndb_event_data::~Ndb_event_data()
{
  if (shadow_table)
    closefrm(shadow_table, 1);
  shadow_table= NULL;

  delete pk_bitmap;
  pk_bitmap = NULL;

  free_root(&mem_root, MYF(0));
  share= NULL;
  /*
    ndbvalue[] allocated with my_multi_malloc -> only
    first pointer need to be freed
  */
  my_free(ndb_value[0]);
}


void Ndb_event_data::print(const char* where, FILE* file) const
{
  fprintf(file,
          "%s shadow_table: %p '%s.%s'\n",
          where,
          shadow_table, shadow_table->s->db.str,
          shadow_table->s->table_name.str);

  // Print stats for the MEM_ROOT where Ndb_event_data
  // has allocated the shadow_table etc.
  {
    USED_MEM *mem_block;
    size_t mem_root_used = 0;
    size_t mem_root_size = 0;

    /* iterate through (partially) free blocks */
    for (mem_block= mem_root.free; mem_block; mem_block= mem_block->next)
    {
      const size_t block_used =
          mem_block->size - // Size of block
          ALIGN_SIZE(sizeof(USED_MEM)) - // Size of header
          mem_block->left; // What's unused in block
      mem_root_used += block_used;
      mem_root_size += mem_block->size;
    }

    /* iterate through the used blocks */
    for (mem_block= mem_root.used; mem_block; mem_block= mem_block->next)
    {
      const size_t block_used =
          mem_block->size - // Size of block
          ALIGN_SIZE(sizeof(USED_MEM)) - // Size of header
          mem_block->left; // What's unused in block
      mem_root_used += block_used;
      mem_root_size += mem_block->size;
    }
    fprintf(file, "  - mem_root size: %lu\n", mem_root_size);
    fprintf(file, "  - mem_root used: %lu\n", mem_root_used);
  }
}

/*
 * While writing an UPDATE_ROW event to the binlog, a bitmap is
 * used to indicate which columns should be written. An
 * UPDATE_ROW event contains 2 versions of the row: a Before Image
 * of the row before the update was done, and an After Image of
 * the row after the update. Column bitmaps are used to decide
 * which columns will be written to both images. The Before
 * Image and After Image can contain different columns.
 *
 * For the binlog formats UPDATED_ONLY_USE_UPDATE_MINIMAL and
 * FULL_USE_UPDATE_MINIMAL, it is necessary to write only primary
 * key columns to the Before Image, and to remove all primary key
 * columns from the After Image. A bitmap of primary key columns is
 * created for this purpose.
 */
void Ndb_event_data::init_pk_bitmap()
{
  if (shadow_table->s->primary_key == MAX_KEY)
  {
    // Table without pk, no need for pk_bitmap since minimal is full
    return;
  }
  pk_bitmap = new MY_BITMAP();
  bitmap_init(pk_bitmap, pk_bitbuf, shadow_table->s->fields, FALSE);
  KEY* key = shadow_table->key_info + shadow_table->s->primary_key;
  KEY_PART_INFO* key_part_info = key->key_part;
  const uint key_parts = key->user_defined_key_parts;
  for (uint i = 0; i < key_parts; i++, key_part_info++)
  {
    bitmap_set_bit(pk_bitmap, key_part_info->fieldnr - 1);
  }
  assert(!bitmap_is_clear_all(pk_bitmap));
}

/*
 * Modify the column bitmaps generated for UPDATE_ROW as per
 * the MINIMAL binlog format type. Expected arguments:
 *
 * @before: empty bitmap to be populated with PK columns
 * @after: bitmap with updated cols, if ndb_log_updated_only=TRUE
 *         bitmap with all cols, if ndb_log_updated_only=FALSE
 *
 * If no PK is defined, bitmaps revert to default behaviour:
 *  - before and after bitmaps are identical
 *  - bitmaps contain all/updated cols as per ndb_log_updated_only
 */
void Ndb_event_data::generate_minimal_bitmap(MY_BITMAP *before, MY_BITMAP *after)
{
  if (pk_bitmap)
  {
    assert(!bitmap_is_clear_all(pk_bitmap));
    // set Before Image to contain only primary keys
    bitmap_copy(before, pk_bitmap);
    // remove primary keys from After Image
    bitmap_subtract(after, pk_bitmap);
  }
  else
  {
    // no usable PK bitmap, set Before Image = After Image
    bitmap_copy(before, after);
  }
}
