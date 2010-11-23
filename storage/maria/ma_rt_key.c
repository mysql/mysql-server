/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin

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

#include "maria_def.h"
#include "trnman.h"
#include "ma_key_recover.h"

#ifdef HAVE_RTREE_KEYS
#include "ma_rt_index.h"
#include "ma_rt_key.h"
#include "ma_rt_mbr.h"

/*
  Add key to the page

  RESULT VALUES
    -1 	Error
    0 	Not split
    1	Split
*/

int maria_rtree_add_key(const MARIA_KEY *key, MARIA_PAGE *page,
                        my_off_t *new_page)
{
  MARIA_HA *info= page->info;
  MARIA_SHARE *share= info->s;
  uint page_size= page->size;
  uint nod_flag=  page->node;
  uchar *key_pos= rt_PAGE_END(page);
  uint tot_key_length= key->data_length + key->ref_length + nod_flag;
  DBUG_ENTER("maria_rtree_add_key");

  if (page_size + tot_key_length <=
      (uint)(key->keyinfo->block_length - KEYPAGE_CHECKSUM_SIZE))
  {
    /* split won't be necessary */
    if (nod_flag)
    {
      DBUG_ASSERT(_ma_kpos(nod_flag, key->data) <
                  info->state->key_file_length);
      /* We don't store reference to row on nod pages for rtree index */
      tot_key_length-= key->ref_length;
    }
    /* save key */
    memcpy(key_pos, key->data - nod_flag, tot_key_length);
    page->size+= tot_key_length;
    page_store_size(share, page);
    if (share->now_transactional &&
        _ma_log_add(page, key_pos - page->buff,
                    key_pos, tot_key_length, tot_key_length, 0,
                    KEY_OP_DEBUG_LOG_ADD_1))
      DBUG_RETURN(-1);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(maria_rtree_split_page(key, page, new_page) ? -1 : 1);
}


/*
  Delete key from the page

  Notes
  key_length is only the data part of the key
*/

int maria_rtree_delete_key(MARIA_PAGE *page, uchar *key, uint key_length)
{
  MARIA_HA *info= page->info;
  MARIA_SHARE *share= info->s;
  uint key_length_with_nod_flag;
  uchar *key_start;

  key_start= key - page->node;
  if (!page->node)
    key_length+= share->base.rec_reflength;

  memmove(key_start, key + key_length, page->size - key_length -
	  (key - page->buff));
  key_length_with_nod_flag= key_length + page->node;
  page->size-= key_length_with_nod_flag;
  page_store_size(share, page);
  if (share->now_transactional &&
      _ma_log_delete(page, key_start, 0, key_length_with_nod_flag,
                     0, KEY_OP_DEBUG_LOG_DEL_CHANGE_RT))
    return -1;
  return 0;
}


/*
  Calculate and store key MBR into *key.
*/

int maria_rtree_set_key_mbr(MARIA_HA *info, MARIA_KEY *key,
                            my_off_t child_page)
{
  MARIA_PAGE page;
  DBUG_ENTER("maria_rtree_set_key_mbr");
  if (_ma_fetch_keypage(&page, info, key->keyinfo, child_page,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        DFLT_INIT_HITS, info->buff, 0))
    DBUG_RETURN(-1);

  DBUG_RETURN(maria_rtree_page_mbr(key->keyinfo->seg,
                                   &page, key->data, key->data_length));
}

#endif /*HAVE_RTREE_KEYS*/
