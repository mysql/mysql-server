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

int maria_rtree_add_key(MARIA_HA *info, const MARIA_KEY *key,
                        uchar *page_buf, my_off_t page, my_off_t *new_page)
{
  MARIA_SHARE *share= info->s;
  uint page_size= _ma_get_page_used(share, page_buf);
  uint nod_flag= _ma_test_if_nod(share, page_buf);
  uchar *key_pos= rt_PAGE_END(share, page_buf);
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
    page_size+= tot_key_length;
    _ma_store_page_used(share, page_buf, page_size);
    if (share->now_transactional &&
        _ma_log_add(info, page, page_buf, key_pos - page_buf,
                    key_pos, tot_key_length, tot_key_length, 0))
      DBUG_RETURN(-1);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(maria_rtree_split_page(info, key, page, page_buf, new_page)
              ? -1 : 1);
}


/*
  Delete key from the page

  Notes
  key_length is only the data part of the key
*/

int maria_rtree_delete_key(MARIA_HA *info, uchar *page_buf, uchar *key,
                           uint key_length, uint nod_flag, my_off_t page)
{
  MARIA_SHARE *share= info->s;
  uint16 page_size= _ma_get_page_used(share, page_buf);
  uint key_length_with_nod_flag;
  uchar *key_start;

  key_start= key - nod_flag;
  if (!nod_flag)
    key_length+= share->base.rec_reflength;

  memmove(key_start, key + key_length, page_size - key_length -
	  (key - page_buf));
  key_length_with_nod_flag= key_length + nod_flag;
  page_size-= key_length_with_nod_flag;
  _ma_store_page_used(share, page_buf, page_size);
  if (share->now_transactional &&
      _ma_log_delete(info, page, page_buf, key_start, 0,
                     key_length_with_nod_flag))

    return -1;
  return 0;
}


/*
  Calculate and store key MBR into *key.
*/

int maria_rtree_set_key_mbr(MARIA_HA *info, MARIA_KEY *key,
                            my_off_t child_page)
{
  DBUG_ENTER("maria_rtree_set_key_mbr");
  if (!_ma_fetch_keypage(info, key->keyinfo, child_page,
                         PAGECACHE_LOCK_LEFT_UNLOCKED,
                         DFLT_INIT_HITS, info->buff, 0, 0))
    DBUG_RETURN(-1);

  DBUG_RETURN(maria_rtree_page_mbr(info, key->keyinfo->seg,
                                   info->buff, key->data,
                                   key->data_length));
}

#endif /*HAVE_RTREE_KEYS*/
