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
  When the tree uses more memory than 'max_heap_table_size',
  write the tree (in sorted order) out to disk and start with a new tree.
  When all data has been generated, merge the trees (removing any found
  duplicates).

  The unique entries will be returned in sort order, to ensure that we do the
  deletes in disk order.
*/

#include "mysql_priv.h"
#include "sql_sort.h"


int unique_write_to_file(gptr key, element_count count, Unique *unique)
{
  /*
    Use unique->size (size of element stored in the tree) and not 
    unique->tree.size_of_element. The latter is different from unique->size 
    when tree implementation chooses to store pointer to key in TREE_ELEMENT
    (instead of storing the element itself there)
  */
  return my_b_write(&unique->file, (byte*) key,
		    unique->size) ? 1 : 0;
}

int unique_write_to_ptrs(gptr key, element_count count, Unique *unique)
{
  memcpy(unique->record_pointers, key, unique->size);
  unique->record_pointers+=unique->size;
  return 0;
}

Unique::Unique(qsort_cmp2 comp_func, void * comp_func_fixed_arg,
	       uint size_arg, ulong max_in_memory_size_arg)
  :max_in_memory_size(max_in_memory_size_arg), size(size_arg), elements(0)
{
  my_b_clear(&file);
  init_tree(&tree, max_in_memory_size / 16, 0, size, comp_func, 0, NULL,
	    comp_func_fixed_arg);
  /* If the following fail's the next add will also fail */
  my_init_dynamic_array(&file_ptrs, sizeof(BUFFPEK), 16, 16);
  max_elements= max_in_memory_size / ALIGN_SIZE(sizeof(TREE_ELEMENT)+size);
  open_cached_file(&file, mysql_tmpdir,TEMP_PREFIX, DISK_BUFFER_SIZE,
		   MYF(MY_WME));
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


/*
  Clear the tree and the file.
  You must call reset() if you want to reuse Unique after walk().
*/

void
Unique::reset()
{
  reset_tree(&tree);
  /*
    If elements != 0, some trees were stored in the file (see how
    flush() works). Note, that we can not count on my_b_tell(&file) == 0
    here, because it can return 0 right after walk(), and walk() does not
    reset any Unique member.
  */
  if (elements)
  {
    reset_dynamic(&file_ptrs);
    reinit_io_cache(&file, WRITE_CACHE, 0L, 0, 1);
  }
  elements= 0;
}
        
/*
  The comparison function, passed to queue_init() in merge_walk() must
  use comparison function of Uniques::tree, but compare members of struct
  BUFFPEK.
*/

struct BUFFPEK_COMPARE_CONTEXT
{
  qsort_cmp2 key_compare;
  void *key_compare_arg;
};

C_MODE_START

static int buffpek_compare(void *arg, byte *key_ptr1, byte *key_ptr2)
{
  BUFFPEK_COMPARE_CONTEXT *ctx= (BUFFPEK_COMPARE_CONTEXT *) arg;
  return ctx->key_compare(ctx->key_compare_arg,
                          *((byte **) key_ptr1), *((byte **)key_ptr2));
}

C_MODE_END


/*
  DESCRIPTION
    Function is very similar to merge_buffers, but instead of writing sorted 
    unique keys to the output file, it invokes walk_action for each key.
    This saves I/O if you need to pass through all unique keys only once.
  SYNOPSIS
    merge_walk()
  All params are 'IN' (but see comment for begin, end):
    merge_buffer       buffer to perform cached piece-by-piece loading
                       of trees; initially the buffer is empty
    merge_buffer_size  size of merge_buffer. Must be aligned with
                       key_length
    key_length         size of tree element; key_length * (end - begin)
                       must be less or equal than merge_buffer_size.
    begin              pointer to BUFFPEK struct for the first tree.
    end                pointer to BUFFPEK struct for the last tree;
                       end > begin and [begin, end) form a consecutive
                       range. BUFFPEKs structs in that range are used and
                       overwritten in merge_walk().
    walk_action        element visitor. Action is called for each unique
                       key.
    walk_action_arg    argument to walk action. Passed to it on each call.
    compare            elements comparison function
    compare_arg        comparison function argument
    file               file with all trees dumped. Trees in the file
                       must contain sorted unique values. Cache must be
                       initialized in read mode.
  RETURN VALUE
    0     ok
    <> 0  error
*/

static bool merge_walk(uchar *merge_buffer, uint merge_buffer_size,
                       uint key_length, BUFFPEK *begin, BUFFPEK *end,
                       tree_walk_action walk_action, void *walk_action_arg,
                       qsort_cmp2 compare, void *compare_arg,
                       IO_CACHE *file)
{
  BUFFPEK_COMPARE_CONTEXT compare_context = { compare, compare_arg };
  QUEUE queue;
  if (end <= begin ||
      merge_buffer_size < key_length * (end - begin + 1) ||
      init_queue(&queue, end - begin, offsetof(BUFFPEK, key), 0,
                 buffpek_compare, &compare_context))
    return 1;
  /* we need space for one key when a piece of merge buffer is re-read */
  merge_buffer_size-= key_length;
  uchar *save_key_buff= merge_buffer + merge_buffer_size;
  uint max_key_count_per_piece= merge_buffer_size/(end-begin)/key_length;
  /* if piece_size is aligned reuse_freed_buffer will always hit */
  uint piece_size= max_key_count_per_piece * key_length;
  uint bytes_read;               /* to hold return value of read_to_buffer */
  BUFFPEK *top;
  int res= 1;
  /*
    Invariant: queue must contain top element from each tree, until a tree
    is not completely walked through.
    Here we're forcing the invariant, inserting one element from each tree
    to the queue.
  */
  for (top= begin; top != end; ++top)
  {
    top->base= merge_buffer + (top - begin) * piece_size;
    top->max_keys= max_key_count_per_piece;
    bytes_read= read_to_buffer(file, top, key_length);
    if (bytes_read == (uint) (-1))
      goto end;
    DBUG_ASSERT(bytes_read);
    queue_insert(&queue, (byte *) top);
  }
  top= (BUFFPEK *) queue_top(&queue);
  while (queue.elements > 1)
  {
    /*
      Every iteration one element is removed from the queue, and one is
      inserted by the rules of the invariant. If two adjacent elements on
      the top of the queue are not equal, biggest one is unique, because all
      elements in each tree are unique. Action is applied only to unique
      elements.
    */
    void *old_key= top->key;
    /*
      read next key from the cache or from the file and push it to the
      queue; this gives new top.
    */
    top->key+= key_length;
    if (--top->mem_count)
      queue_replaced(&queue);
    else /* next piece should be read */
    {
      /* save old_key not to overwrite it in read_to_buffer */
      memcpy(save_key_buff, old_key, key_length);
      old_key= save_key_buff;
      bytes_read= read_to_buffer(file, top, key_length);
      if (bytes_read == (uint) (-1))
        goto end;
      else if (bytes_read > 0)      /* top->key, top->mem_count are reset */
        queue_replaced(&queue);     /* in read_to_buffer */
      else
      {
        /*
          Tree for old 'top' element is empty: remove it from the queue and
          give all its memory to the nearest tree.
        */
        queue_remove(&queue, 0);
        reuse_freed_buff(&queue, top, key_length);
      }
    }
    top= (BUFFPEK *) queue_top(&queue);
    /* new top has been obtained; if old top is unique, apply the action */
    if (compare(compare_arg, old_key, top->key))
    {
      if (walk_action(old_key, 1, walk_action_arg))
        goto end;
    }
  }
  /*
    Applying walk_action to the tail of the last tree: this is safe because
    either we had only one tree in the beginning, either we work with the
    last tree in the queue.
  */
  do
  {
    do
    {
      if (walk_action(top->key, 1, walk_action_arg))
        goto end;
      top->key+= key_length;
    }
    while (--top->mem_count);
    bytes_read= read_to_buffer(file, top, key_length);
    if (bytes_read == (uint) (-1))
      goto end;
  }
  while (bytes_read);
  res= 0;
end:
  delete_queue(&queue);
  return res;
}


/*
  DESCRIPTION
    Walks consecutively through all unique elements:
    if all elements are in memory, then it simply invokes 'tree_walk', else
    all flushed trees are loaded to memory piece-by-piece, pieces are
    sorted, and action is called for each unique value.
    Note: so as merging resets file_ptrs state, this method can change
    internal Unique state to undefined: if you want to reuse Unique after
    walk() you must call reset() first!
  SYNOPSIS
    Unique:walk()
  All params are 'IN':
    action  function-visitor, typed in include/my_tree.h
            function is called for each unique element
    arg     argument for visitor, which is passed to it on each call
  RETURN VALUE
    0    OK
    <> 0 error
 */

bool Unique::walk(tree_walk_action action, void *walk_action_arg)
{
  if (elements == 0)                       /* the whole tree is in memory */
    return tree_walk(&tree, action, walk_action_arg, left_root_right);

  /* flush current tree to the file to have some memory for merge buffer */
  if (flush())
    return 1;
  if (flush_io_cache(&file) || reinit_io_cache(&file, READ_CACHE, 0L, 0, 0))
    return 1;
  uchar *merge_buffer= (uchar *) my_malloc(max_in_memory_size, MYF(0));
  if (merge_buffer == 0)
    return 1;
  int res= merge_walk(merge_buffer, max_in_memory_size, size,
                      (BUFFPEK *) file_ptrs.buffer,
                      (BUFFPEK *) file_ptrs.buffer + file_ptrs.elements,
                      action, walk_action_arg,
                      tree.compare, tree.custom_arg, &file);
  x_free(merge_buffer);
  return res;
}

/*
  Modify the TABLE element so that when one calls init_records()
  the rows will be read in priority order.
*/

bool Unique::get(TABLE *table)
{
  SORTPARAM sort_param;
  table->sort.found_records=elements+tree.elements_in_tree;

  if (my_b_tell(&file) == 0)
  {
    /* Whole tree is in memory;  Don't use disk if you don't need to */
    if ((record_pointers=table->sort.record_pointers= (byte*)
	 my_malloc(size * tree.elements_in_tree, MYF(0))))
    {
      (void) tree_walk(&tree, (tree_walk_action) unique_write_to_ptrs,
		       this, left_root_right);
      return 0;
    }
  }
  /* Not enough memory; Save the result to file && free memory used by tree */
  if (flush())
    return 1;

  IO_CACHE *outfile=table->sort.io_cache;
  BUFFPEK *file_ptr= (BUFFPEK*) file_ptrs.buffer;
  uint maxbuffer= file_ptrs.elements - 1;
  uchar *sort_buffer;
  my_off_t save_pos;
  bool error=1;

      /* Open cached file if it isn't open */
  outfile=table->sort.io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE), 
                                MYF(MY_ZEROFILL));

  if (!outfile || ! my_b_inited(outfile) &&
      open_cached_file(outfile,mysql_tmpdir,TEMP_PREFIX,READ_RECORD_BUFFER,
		       MYF(MY_WME)))
    return 1;
  reinit_io_cache(outfile,WRITE_CACHE,0L,0,0);

  bzero((char*) &sort_param,sizeof(sort_param));
  sort_param.max_rows= elements;
  sort_param.sort_form=table;
  sort_param.rec_length= sort_param.sort_length= sort_param.ref_length=
    size;
  sort_param.keys= max_in_memory_size / sort_param.sort_length;
  sort_param.not_killable=1;

  if (!(sort_buffer=(uchar*) my_malloc((sort_param.keys+1) * 
				       sort_param.sort_length,
				       MYF(0))))
    return 1;
  sort_param.unique_buff= sort_buffer+(sort_param.keys*
				       sort_param.sort_length);

  /* Merge the buffers to one file, removing duplicates */
  if (merge_many_buff(&sort_param,sort_buffer,file_ptr,&maxbuffer,&file))
    goto err;
  if (flush_io_cache(&file) ||
      reinit_io_cache(&file,READ_CACHE,0L,0,0))
    goto err;
  if (merge_buffers(&sort_param, &file, outfile, sort_buffer, file_ptr,
		    file_ptr, file_ptr+maxbuffer,0))
    goto err;                                                                 
  error=0;
err:
  x_free((gptr) sort_buffer);
  if (flush_io_cache(outfile))
    error=1;

  /* Setup io_cache for reading */
  save_pos=outfile->pos_in_file;
  if (reinit_io_cache(outfile,READ_CACHE,0L,0,0))
    error=1;
  outfile->end_of_file=save_pos;
  return error;
}
