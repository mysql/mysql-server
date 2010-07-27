/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @details
  Caching of files with only does (sequential) read or writes of fixed-
  length records. A read isn't allowed to go over file-length. A read is ok
  if it ends at file-length and next read can try to read after file-length
  (and get a EOF-error).
  Possibly use of asyncronic io.
  macros for read and writes for faster io.
  Used instead of FILE when reading or writing whole files.
  This will make mf_rec_cache obsolete.
  One can change info->pos_in_file to a higher value to skip bytes in file if
  also info->rc_pos is set to info->rc_end.
  If called through open_cached_file(), then the temporary file will
  only be created if a write exeeds the file buffer or if one calls
  flush_io_cache().  
*/

#include "sql_priv.h"
#include "sql_class.h"                          // THD
#ifdef HAVE_REPLICATION

extern "C" {

/**
  Read buffered from the net.

  @retval
    1   if can't read requested characters
  @retval
    0   if record read
*/


int _my_b_net_read(register IO_CACHE *info, uchar *Buffer,
		   size_t Count __attribute__((unused)))
{
  ulong read_length;
  NET *net= &(current_thd)->net;
  DBUG_ENTER("_my_b_net_read");

  if (!info->end_of_file)
    DBUG_RETURN(1);	/* because my_b_get (no _) takes 1 byte at a time */
  read_length=my_net_read(net);
  if (read_length == packet_error)
  {
    info->error= -1;
    DBUG_RETURN(1);
  }
  if (read_length == 0)
  {
    info->end_of_file= 0;			/* End of file from client */
    DBUG_RETURN(1);
  }
  /* to set up stuff for my_b_get (no _) */
  info->read_end = (info->read_pos = (uchar*) net->read_pos) + read_length;
  Buffer[0] = info->read_pos[0];		/* length is always 1 */

  /*
    info->request_pos is used by log_loaded_block() to know the size
    of the current block.
    info->pos_in_file is used by log_loaded_block() too.
  */
  info->pos_in_file+= read_length;
  info->request_pos=info->read_pos;

  info->read_pos++;

  DBUG_RETURN(0);
}

} /* extern "C" */
#endif /* HAVE_REPLICATION */


