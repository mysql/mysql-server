/* Copyright (C) 2001 MySQL AB

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
  Function to handle quick removal of duplicates
  This code is used when doing multi-table deletes to find the rows in
  reference tables that needs to be deleted.

  The basic idea is as follows:

  Store first all strings in a binary tree, ignoring duplicates.
  When the three uses more memory than 'max_heap_table_size',
  write the tree (in sorted order) out to disk and start with a new tree.
  When all data has been generated, merge the trees (removing any found
  duplicates).

  The unique entries will be returned in sort order, to ensure that we do the
  deletes in disk order.
*/

#include "mysql_priv.h"
#include "sql_sort.h"


Unique::Unique(qsort_cmp2 comp_func, void * comp_func_fixed_arg,
	       uint size, ulong max_in_memory_size_arg)
  :max_in_memory_size(max_in_memory_size_arg),elements(0)
{
  my_b_clear(&file);
  init_tree(&tree, max_in_memory_size / 16, size, comp_func, 0, 0);
  tree.cmp_arg=comp_func_fixed_arg;
  /* If the following fail's the next add will also fail */
  init_dynamic_array(&file_ptrs, sizeof(BUFFPEK), 16, 16);
  max_elements= max_in_memory_size / ALIGN_SIZE(sizeof(TREE_ELEMENT)+size);
}


Unique::~Unique()
{
  close_cached_file(&file);
  delete_tree(&tree);
  delete_dynamic(&file_ptrs);
}


    /* Write tree to disk; clear tree */    
bool Unique::flush()
{
  BUFFPEK file_ptr;
  elements+= tree.elements_in_tree;
  file_ptr.count=tree.elements_in_tree;
  file_ptr.file_pos=my_b_tell(&file);
  if (tree_walk(&tree, (tree_walk_action) unique_write_to_file,
		(void*) this, left_root_right) ||
      insert_dynamic(&file_ptrs, (gptr) &file_ptr))
    return 1;
  delete_tree(&tree);
  return 0;
}


int unique_write_to_file(gptr key, element_count count, Unique *unique)
{
  if (!my_b_inited(&unique->file) && open_cached_file(&unique->file, mysql_tmpdir,TEMP_PREFIX, DISK_BUFFER_SIZE, MYF(MY_WME)))
      return 1;
  return my_b_write(&unique->file, key, unique->tree.size_of_element) ? 1 : 0;
}

int unique_write_to_ptrs(gptr key, element_count count, Unique *unique)
{
  memcpy(unique->record_pointers, key, unique->tree.size_of_element);
  unique->record_pointers+=unique->tree.size_of_element;
  return 0;
}


/*
  Modify the TABLE element so that when one calls init_records()
  the rows will be read in priority order.
*/

bool Unique::get(TABLE *table)
{
  SORTPARAM sort_param;
  table->found_records=elements+tree.elements_in_tree;

  if (!my_b_inited(&file))
  {
    /* Whole tree is in memory;  Don't use disk if you don't need to */
    if ((record_pointers=table->record_pointers= (byte*)
	 my_malloc(tree.size_of_element * tree.elements_in_tree, MYF(0))))
    {
      (void) tree_walk(&tree, (tree_walk_action) unique_write_to_ptrs,
		       this, left_root_right);
      return 0;
    }
  }
  /* Not enough memory; Save the result to file */
  if (flush())
    return 1;

  IO_CACHE *outfile=table->io_cache, tempfile;
  BUFFPEK *file_ptr= (BUFFPEK*) file_ptrs.buffer;
  uint maxbuffer= file_ptrs.elements - 1; // I added -1 .....
  uchar *sort_buffer;
  my_off_t save_pos;
  bool error=1;

  my_b_clear(&tempfile);

      /* Open cached file if it isn't open */
  if (!outfile) outfile= (IO_CACHE *) sql_calloc(sizeof(IO_CACHE));
  if (! my_b_inited(outfile) &&
      open_cached_file(outfile,mysql_tmpdir,TEMP_PREFIX,READ_RECORD_BUFFER,
		       MYF(MY_WME)))
    return 1;
  reinit_io_cache(outfile,WRITE_CACHE,0L,0,0);
  
//  sort_param.keys=elements;
  sort_param.max_rows= elements;
  sort_param.examined_rows=0;
  sort_param.sort_form=table;
  sort_param.sort_length=sort_param.ref_length=tree.size_of_element;
  sort_param.keys= max_in_memory_size / sort_param.sort_length;
  
  if (!(sort_buffer=(uchar*) my_malloc((sort_param.keys+1) * 
				      sort_param.sort_length,
				      MYF(0))))
    return 1;
  sort_param.unique_buff= sort_buffer+(sort_param.keys*
				       sort_param.sort_length);

  /* Merge the buffers to one file, removing duplicates */
  if (merge_many_buff(&sort_param,sort_buffer,file_ptr,&maxbuffer,&tempfile))
    goto err;
  if (flush_io_cache(&tempfile) ||
      reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
    goto err;
  if (merge_buffers(&sort_param, &tempfile, outfile, sort_buffer, file_ptr,
		    file_ptr, file_ptr+maxbuffer,0))
    goto err;                                                                 
  error=0;
err:
  x_free((gptr) sort_buffer);
  close_cached_file(&tempfile);
  if (flush_io_cache(outfile))
    error=1;

  /* Setup io_cache for reading */
  save_pos=outfile->pos_in_file;
  if (reinit_io_cache(outfile,READ_CACHE,0L,0,0))
    error=1;
  outfile->end_of_file=save_pos;
  return error;
}
