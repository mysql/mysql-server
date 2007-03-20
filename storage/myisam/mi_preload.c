/* Copyright (C) 2003, 2005 MySQL AB

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
  Preload indexes into key cache
*/

#include "myisamdef.h"


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

int mi_preload(MI_INFO *info, ulonglong key_map, my_bool ignore_leaves)
{
  uint i;
  ulong length, block_length= 0;
  uchar *buff= NULL;
  MYISAM_SHARE* share= info->s;
  uint keys= share->state.header.keys;
  MI_KEYDEF *keyinfo= share->keyinfo;
  my_off_t key_file_length= share->state.state.key_file_length;
  my_off_t pos= share->base.keystart;
  DBUG_ENTER("mi_preload");

  if (!keys || !mi_is_any_key_active(key_map) || key_file_length == pos)
    DBUG_RETURN(0);

  block_length= keyinfo[0].block_length;

  /* Check whether all indexes use the same block size */
  for (i= 1 ; i < keys ; i++)
  {
#if !defined(INGO_TEST_LOADIDX_OFF)
    /* Allow non-IGNORE-LEAVES index loading even with different block sizes. */
    if (ignore_leaves && (keyinfo[i].block_length != block_length))
      DBUG_RETURN(my_errno= HA_ERR_NON_UNIQUE_BLOCK_SIZE);
    set_if_bigger(block_length, keyinfo[i].block_length);
#else
    if (keyinfo[i].block_length != block_length)
      DBUG_RETURN(my_errno= HA_ERR_NON_UNIQUE_BLOCK_SIZE);
#endif
  }

#if !defined(INGO_TEST_LOADIDX_OFF)
  /* Align non-IGNORE-LEAVES index loads. */
  if (!ignore_leaves)
  {
    /* Round up to the next multiple of key_cache_block_size. */
    length= ((info->preload_buff_size +
              share->key_cache->key_cache_block_size - 1) /
             share->key_cache->key_cache_block_size *
             share->key_cache->key_cache_block_size);
    /* Round down to the next multiple of key_cache_block_size. */
    pos= (share->base.keystart / share->key_cache->key_cache_block_size *
          share->key_cache->key_cache_block_size);
  }
  else
  {
    length= info->preload_buff_size/block_length * block_length;
    set_if_bigger(length, block_length);
  }
#else
  length= info->preload_buff_size/block_length * block_length;
  set_if_bigger(length, block_length);
#endif

  if (!(buff= (uchar *) my_malloc(length, MYF(MY_WME))))
    DBUG_RETURN(my_errno= HA_ERR_OUT_OF_MEM);

  if (flush_key_blocks(share->key_cache,share->kfile, FLUSH_RELEASE))
    goto err;

  do
  {
    /* Read the next block of index file into the preload buffer */
    if ((my_off_t) length > (key_file_length-pos))
      length= (ulong) (key_file_length-pos);
    if (my_pread(share->kfile, (byte*) buff, length, pos, MYF(MY_FAE|MY_FNABP)))
      goto err;

    if (ignore_leaves)
    {
      uchar *end= buff+length;
      do
      {
        if (mi_test_if_nod(buff))
        {
          if (key_cache_insert(share->key_cache,
                               share->kfile, pos, DFLT_INIT_HITS,
                              (byte*) buff, block_length))
	    goto err;
	}
        pos+= block_length;
      }
      while ((buff+= block_length) != end);
      buff= end-length;
    }
    else
    {
      if (key_cache_insert(share->key_cache,
                           share->kfile, pos, DFLT_INIT_HITS,
                           (byte*) buff, length))
	goto err;
      pos+= length;
    }
  }
  while (pos != key_file_length);

  my_free((char*) buff, MYF(0));
  DBUG_RETURN(0);

err:
  my_free((char*) buff, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN(my_errno= errno);
}

