/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
  Preload indexes into key cache
*/

#include "maria_def.h"


/*
  Preload pages of the index file for a table into the key cache

  SYNOPSIS
    maria_preload()
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

int maria_preload(MARIA_HA *info, ulonglong key_map, my_bool ignore_leaves)
{
  uint i;
  ulong length, block_length= 0;
  uchar *buff= NULL;
  MARIA_SHARE* share= info->s;
  uint keys= share->state.header.keys;
  MARIA_KEYDEF *keyinfo= share->keyinfo;
  my_off_t key_file_length= share->state.state.key_file_length;
  my_off_t pos= share->base.keystart;
  DBUG_ENTER("maria_preload");

  if (!keys || !maria_is_any_key_active(key_map) || key_file_length == pos)
    DBUG_RETURN(0);

  block_length= keyinfo[0].block_length;

  /* Check whether all indexes use the same block size */
  for (i= 1 ; i < keys ; i++)
  {
    if (keyinfo[i].block_length != block_length)
      DBUG_RETURN(my_errno= HA_ERR_NON_UNIQUE_BLOCK_SIZE);
  }

  length= info->preload_buff_size/block_length * block_length;
  set_if_bigger(length, block_length);

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
        if (_ma_test_if_nod(buff))
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
