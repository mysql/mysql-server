/* Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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
  Preload indexes into key cache
*/

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "storage/myisam/myisamdef.h"

/*
  Preload pages of the index file for a table into the key cache

  SYNOPSIS
    mi_preload()
      info          open table
      map           map of indexes to preload into key cache
      ignore_leaves only non-leaves pages are to be preloaded

  RETURN VALUE
    0 if a success. error code - otherwise.

  NOTES.
    At present pages for all indexes are preloaded.
    In future only pages for indexes specified in the key_map parameter
    of the table will be preloaded.
*/

int mi_preload(MI_INFO *info, ulonglong key_map, bool ignore_leaves) {
  uint i;
  ulong length, block_length = 0;
  uchar *buff = NULL;
  MYISAM_SHARE *share = info->s;
  uint keys = share->state.header.keys;
  MI_KEYDEF *keyinfo = share->keyinfo;
  my_off_t key_file_length = share->state.state.key_file_length;
  my_off_t pos = share->base.keystart;
  DBUG_TRACE;

  if (!keys || !mi_is_any_key_active(key_map) || key_file_length == pos)
    return 0;

  /* Preload into a non initialized key cache should never happen. */
  DBUG_ASSERT(share->key_cache->key_cache_inited);

  block_length = keyinfo[0].block_length;

  if (ignore_leaves) {
    /* Check whether all indexes use the same block size */
    for (i = 1; i < keys; i++) {
      if (keyinfo[i].block_length != block_length) {
        set_my_errno(HA_ERR_NON_UNIQUE_BLOCK_SIZE);
        return HA_ERR_NON_UNIQUE_BLOCK_SIZE;
      }
    }
  } else
    block_length = share->key_cache->key_cache_block_size;

  length = info->preload_buff_size / block_length * block_length;
  set_if_bigger(length, block_length);

  if (!(buff = (uchar *)my_malloc(mi_key_memory_preload_buffer, length,
                                  MYF(MY_WME)))) {
    set_my_errno(HA_ERR_OUT_OF_MEM);
    return HA_ERR_OUT_OF_MEM;
  }

  if (flush_key_blocks(share->key_cache, keycache_thread_var(), share->kfile,
                       FLUSH_RELEASE))
    goto err;

  do {
    /* Read the next block of index file into the preload buffer */
    if ((my_off_t)length > (key_file_length - pos))
      length = (ulong)(key_file_length - pos);
    if (mysql_file_pread(share->kfile, (uchar *)buff, length, pos,
                         MYF(MY_FAE | MY_FNABP)))
      goto err;

    if (ignore_leaves) {
      uchar *end = buff + length;
      do {
        if (mi_test_if_nod(buff)) {
          if (key_cache_insert(share->key_cache, keycache_thread_var(),
                               share->kfile, pos, DFLT_INIT_HITS, (uchar *)buff,
                               block_length))
            goto err;
        }
        pos += block_length;
      } while ((buff += block_length) != end);
      buff = end - length;
    } else {
      if (key_cache_insert(share->key_cache, keycache_thread_var(),
                           share->kfile, pos, DFLT_INIT_HITS, (uchar *)buff,
                           length))
        goto err;
      pos += length;
    }
  } while (pos != key_file_length);

  my_free(buff);
  return 0;

err:
  my_free(buff);
  set_my_errno(errno);
  return errno;
}
