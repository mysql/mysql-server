/* Copyright (C) 2000 MySQL AB & Ramil Kalimullin
   
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

#include "myisamdef.h"

#include "rt_index.h"
#include "rt_key.h"
#include "rt_mbr.h"

/*
  Add key to the page

  RESULT VALUES
    -1 	Error
    0 	Not split
    1	Split
*/

int rtree_add_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key, 
		  uint key_length, uchar *page_buf, my_off_t *new_page)
{
  uint page_size = mi_getint(page_buf);
  uint nod_flag = mi_test_if_nod(page_buf);

  if (page_size + key_length + nod_flag <= keyinfo->block_length)  
  {
    /* split won't be necessary */
    if (nod_flag)
    {
      /* save key */
      memcpy(rt_PAGE_END(page_buf), key - nod_flag, key_length + nod_flag); 
      page_size += key_length + nod_flag;
    }
    else
    {
      /* save key */
      memcpy(rt_PAGE_END(page_buf), key, key_length + 
                                         info->s->base.rec_reflength);
      page_size += key_length + info->s->base.rec_reflength;
    }
    mi_putint(page_buf, page_size, nod_flag);
    return 0;
  }

  return (rtree_split_page(info, keyinfo, page_buf, key, key_length,
			   new_page) ? -1 : 1);
}

/*
  Delete key from the page
*/
int rtree_delete_key(MI_INFO *info, uchar *page_buf, uchar *key, 
		     uint key_length, uint nod_flag)
{
  uint16 page_size = mi_getint(page_buf);
  uchar *key_start;

  key_start= key - nod_flag;
  if (!nod_flag)
    key_length += info->s->base.rec_reflength;

  memmove(key_start, key + key_length, page_size - key_length -
	  (key - page_buf));
  page_size-= key_length + nod_flag;

  mi_putint(page_buf, page_size, nod_flag);
  return 0;
}


/*
  Calculate and store key MBR
*/

int rtree_set_key_mbr(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key, 
		      uint key_length, my_off_t child_page)
{
  if (!_mi_fetch_keypage(info, keyinfo, child_page,
                         DFLT_INIT_HITS, info->buff, 0))
    return -1;

  return rtree_page_mbr(info, keyinfo->seg, info->buff, key, key_length);
}


/*
  Choose non-leaf better key for insertion
*/

uchar *rtree_choose_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key, 
			uint key_length, uchar *page_buf, uint nod_flag)
{
  double increase;
  double best_incr = DBL_MAX;
  double area;
  double best_area;
  uchar *best_key;
  uchar *k = rt_PAGE_FIRST_KEY(page_buf, nod_flag);
  uchar *last = rt_PAGE_END(page_buf);

  LINT_INIT(best_area);
  LINT_INIT(best_key);

  for (; k < last; k = rt_PAGE_NEXT_KEY(k, key_length, nod_flag))
  {
    if ((increase = rtree_area_increase(keyinfo->seg, key, k, key_length, 
                                        &area)) == -1)
      return NULL;
    if (increase < best_incr)
    {
      best_key = k;
      best_area = area;
      best_incr = increase;
    }
    else
    {
      if ((increase == best_incr) && (area < best_area))
      {
        best_key = k;
        best_area = area;
        best_incr = increase;
      }
    }
  }
  return best_key;
}
