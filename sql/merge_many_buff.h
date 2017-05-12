/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MERGE_MANY_BUFF_INCLUDED
#define MERGE_MANY_BUFF_INCLUDED

#include <algorithm>

#include "my_dbug.h"
#include "my_sys.h"                             // open_cached_file
#include "mysqld.h"                             // mysql_tmpdir
#include "sql_base.h"                           // TEMP_PREFIX
#include "sql_sort.h"                           // Sort_buffer

class THD;

/**
  Merges buffers to make < MERGEBUFF2 buffers.

  @param thd
  @param param        Sort parameters.
  @param sort_buffer  The main memory buffer.
  @param chunk_array  Array of chunk descriptors to merge.
  @param [out] p_num_chunks The number of chunks left in the output file.
  @param [out] t_file Where to store the result.

  @returns   false if success, true if error
*/

template<typename Merge_param>
bool merge_many_buff(THD *thd, Merge_param *param, Sort_buffer sort_buffer,
                     Merge_chunk_array chunk_array,
                     size_t *p_num_chunks, IO_CACHE *t_file)
{
  IO_CACHE t_file2;
  DBUG_ENTER("merge_many_buff");

  size_t num_chunks= chunk_array.size();
  *p_num_chunks= num_chunks;

  if (num_chunks <= MERGEBUFF2)
    DBUG_RETURN(false);				/* purecov: inspected */

  if (flush_io_cache(t_file) ||
      open_cached_file(&t_file2, mysql_tmpdir, TEMP_PREFIX, DISK_BUFFER_SIZE,
                       MYF(MY_WME)))
    DBUG_RETURN(true);				/* purecov: inspected */

  IO_CACHE *from_file= t_file;
  IO_CACHE *to_file= &t_file2;

  while (num_chunks > MERGEBUFF2)
  {
    if (reinit_io_cache(from_file, READ_CACHE, 0L, 0, 0))
      goto cleanup;
    if (reinit_io_cache(to_file, WRITE_CACHE, 0L, 0, 0))
      goto cleanup;
    Merge_chunk *last_chunk= chunk_array.begin();
    uint i;
    for (i= 0 ; i < num_chunks - MERGEBUFF * 3U / 2U; i+= MERGEBUFF)
    {
      if (merge_buffers(thd,
                        param,
                        from_file,
                        to_file,
                        sort_buffer,
                        last_chunk++,
                        Merge_chunk_array(&chunk_array[i], MERGEBUFF),
                        0))
      goto cleanup;
    }
    if (merge_buffers(thd,
                      param,
                      from_file,
                      to_file,
                      sort_buffer,
                      last_chunk++,
                      Merge_chunk_array(&chunk_array[i], num_chunks - i),
                      0))
      break;					/* purecov: inspected */
    if (flush_io_cache(to_file))
      break;					/* purecov: inspected */

    std::swap(from_file, to_file);
    setup_io_cache(from_file);
    setup_io_cache(to_file);
    num_chunks= last_chunk - chunk_array.begin();
  }
cleanup:
  close_cached_file(to_file);			// This holds old result
  if (to_file == t_file)
  {
    *t_file= t_file2;				// Copy result file
    setup_io_cache(t_file);
  }

  *p_num_chunks= num_chunks;
  DBUG_RETURN(num_chunks > MERGEBUFF2);  /* Return true if interrupted */
} /* merge_many_buff */


#endif  // MERGE_MANY_BUFF_INCLUDED
