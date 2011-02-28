/* Copyright (C) 2006 MySQL AB

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

/*
  Key cache assignments
*/

#include "maria_def.h"

/*
  Assign pages of the index file for a table to a key cache

  SYNOPSIS
    maria_assign_to_pagecache()
      info          open table
      key_map       map of indexes to assign to the key cache
      pagecache_ptr pointer to the key cache handle
      assign_lock   Mutex to lock during assignment

  PREREQUESTS
    One must have a READ lock or a WRITE lock on the table when calling
    the function to ensure that there is no other writers to it.

    The caller must also ensure that one doesn't call this function from
    two different threads with the same table.

  NOTES
    At present pages for all indexes must be assigned to the same key cache.
    In future only pages for indexes specified in the key_map parameter
    of the table will be assigned to the specified key cache.

  RETURN VALUE
    0  If a success
    #  Error code
*/

int maria_assign_to_pagecache(MARIA_HA *info,
                              ulonglong key_map __attribute__((unused)),
                              PAGECACHE *pagecache)
{
  int error= 0;
  MARIA_SHARE* share= info->s;
  DBUG_ENTER("maria_assign_to_pagecache");
  DBUG_PRINT("enter",
             ("old_pagecache_handle: 0x%lx  new_pagecache_handle: 0x%lx",
             (long) share->pagecache, (long) pagecache));

  /*
    Skip operation if we didn't change key cache. This can happen if we
    call this for all open instances of the same table
  */
  if (share->pagecache == pagecache)
    DBUG_RETURN(0);

  /*
    First flush all blocks for the table in the old key cache.
    This is to ensure that the disk is consistent with the data pages
    in memory (which may not be the case if the table uses delayed_key_write)

    Note that some other read thread may still fill in the key cache with
    new blocks during this call and after, but this doesn't matter as
    all threads will start using the new key cache for their next call to
    maria library and we know that there will not be any changed blocks
    in the old key cache.
  */

  if (flush_pagecache_blocks(share->pagecache, &share->kfile, FLUSH_RELEASE))
  {
    error= my_errno;
    /* Mark that table must be checked */
    _ma_set_fatal_error(share, error);
  }

  /*
    Flush the new key cache for this file.  This is needed to ensure
    that there is no old blocks (with outdated data) left in the new key
    cache from an earlier assign_to_keycache operation

    (This can never fail as there is never any not written data in the
    new key cache)
  */
  (void) flush_pagecache_blocks(pagecache, &share->kfile, FLUSH_RELEASE);

  /*
    ensure that setting the key cache and changing the multi_pagecache
    is done atomicly
  */
  pthread_mutex_lock(&share->intern_lock);
  /*
    Tell all threads to use the new key cache
    This should be seen at the lastes for the next call to an maria function.
  */
  share->pagecache= pagecache;

  /* store the key cache in the global hash structure for future opens */
  if (multi_pagecache_set((uchar*) share->unique_file_name.str,
                          share->unique_file_name.length,
			  share->pagecache))
    error= my_errno;
  pthread_mutex_unlock(&share->intern_lock);
  DBUG_RETURN(error);
}


/*
  Change all MARIA entries that uses one key cache to another key cache

  SYNOPSIS
    maria_change_pagecache()
    old_pagecache	Old key cache
    new_pagecache	New key cache

  NOTES
    This is used when we delete one key cache.

    To handle the case where some other threads tries to open an MARIA
    table associated with the to-be-deleted key cache while this operation
    is running, we have to call 'multi_pagecache_change()' from this
    function while we have a lock on the MARIA table list structure.

    This is safe as long as it's only MARIA that is using this specific
    key cache.
*/


void maria_change_pagecache(PAGECACHE *old_pagecache,
                            PAGECACHE *new_pagecache)
{
  LIST *pos;
  DBUG_ENTER("maria_change_pagecache");

  /*
    Lock list to ensure that no one can close the table while we manipulate it
  */
  pthread_mutex_lock(&THR_LOCK_maria);
  for (pos=maria_open_list ; pos ; pos=pos->next)
  {
    MARIA_HA *info= (MARIA_HA*) pos->data;
    MARIA_SHARE *share= info->s;
    if (share->pagecache == old_pagecache)
      maria_assign_to_pagecache(info, (ulonglong) ~0, new_pagecache);
  }

  /*
    We have to do the following call while we have the lock on the
    MARIA list structure to ensure that another thread is not trying to
    open a new table that will be associted with the old key cache
  */
  multi_pagecache_change(old_pagecache, new_pagecache);
  pthread_mutex_unlock(&THR_LOCK_maria);
  DBUG_VOID_RETURN;
}
