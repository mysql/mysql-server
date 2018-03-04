/* Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Key cache assignments
*/

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"

/*
  Assign pages of the index file for a table to a key cache

  SYNOPSIS
    mi_assign_to_key_cache()
      info          open table
      key_map       map of indexes to assign to the key cache 
      key_cache_ptr pointer to the key cache handle
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

int mi_assign_to_key_cache(MI_INFO *info,
			   ulonglong key_map MY_ATTRIBUTE((unused)),
			   KEY_CACHE *key_cache)
{
  int error= 0;
  MYISAM_SHARE* share= info->s;
  DBUG_ENTER("mi_assign_to_key_cache");
  DBUG_PRINT("enter",("old_key_cache_handle: %p  new_key_cache_handle: %p",
		      share->key_cache, key_cache));

  /*
    Skip operation if we didn't change key cache. This can happen if we
    call this for all open instances of the same table
  */
  if (share->key_cache == key_cache)
    DBUG_RETURN(0);

  /*
    First flush all blocks for the table in the old key cache.
    This is to ensure that the disk is consistent with the data pages
    in memory (which may not be the case if the table uses delayed_key_write)

    Note that some other read thread may still fill in the key cache with
    new blocks during this call and after, but this doesn't matter as
    all threads will start using the new key cache for their next call to
    myisam library and we know that there will not be any changed blocks
    in the old key cache.
  */

  if (flush_key_blocks(share->key_cache, keycache_thread_var(),
                       share->kfile, FLUSH_RELEASE))
  {
    error= my_errno();
    mi_print_error(info->s, HA_ERR_CRASHED);
    mi_mark_crashed(info);		/* Mark that table must be checked */
  }

  /*
    Flush the new key cache for this file.  This is needed to ensure
    that there is no old blocks (with outdated data) left in the new key
    cache from an earlier assign_to_keycache operation

    (This can never fail as there is never any not written data in the
    new key cache)
  */
  (void) flush_key_blocks(key_cache, keycache_thread_var(),
                          share->kfile, FLUSH_RELEASE);

  /*
    ensure that setting the key cache and changing the multi_key_cache
    is done atomicly
  */
  mysql_mutex_lock(&share->intern_lock);
  /*
    Tell all threads to use the new key cache
    This should be seen at the lastes for the next call to an myisam function.
  */
  share->key_cache= key_cache;

  /* store the key cache in the global hash structure for future opens */
  if (multi_key_cache_set((uchar*) share->unique_file_name,
                          share->unique_name_length,
			  share->key_cache))
    error= my_errno();
  mysql_mutex_unlock(&share->intern_lock);
  DBUG_RETURN(error);
}


/*
  Change all MyISAM entries that uses one key cache to another key cache

  SYNOPSIS
    mi_change_key_cache()
    old_key_cache	Old key cache
    new_key_cache	New key cache

  NOTES
    This is used when we delete one key cache.

    To handle the case where some other threads tries to open an MyISAM
    table associated with the to-be-deleted key cache while this operation
    is running, we have to call 'multi_key_cache_change()' from this
    function while we have a lock on the MyISAM table list structure.

    This is safe as long as it's only MyISAM that is using this specific
    key cache.
*/


void mi_change_key_cache(KEY_CACHE *old_key_cache,
			 KEY_CACHE *new_key_cache)
{
  LIST *pos;
  DBUG_ENTER("mi_change_key_cache");

  /*
    Lock list to ensure that no one can close the table while we manipulate it
  */
  mysql_mutex_lock(&THR_LOCK_myisam);
  for (pos=myisam_open_list ; pos ; pos=pos->next)
  {
    MI_INFO *info= (MI_INFO*) pos->data;
    MYISAM_SHARE *share= info->s;
    if (share->key_cache == old_key_cache)
      mi_assign_to_key_cache(info, (ulonglong) ~0, new_key_cache);
  }

  /*
    We have to do the following call while we have the lock on the
    MyISAM list structure to ensure that another thread is not trying to
    open a new table that will be associted with the old key cache
  */
  multi_key_cache_change(old_key_cache, new_key_cache);
  mysql_mutex_unlock(&THR_LOCK_myisam);
  DBUG_VOID_RETURN;
}
