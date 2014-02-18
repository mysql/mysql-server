/* Copyright (c) 2000, 2002-2005, 2007 MySQL AB
   Use is subject to license terms
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "myisamdef.h"

#ifdef HAVE_RTREE_KEYS
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
  DBUG_ENTER("rtree_add_key");

  if (page_size + key_length + info->s->base.rec_reflength <=
      keyinfo->block_length)
  {
    /* split won't be necessary */
    if (nod_flag)
    {
      /* save key */
      DBUG_ASSERT(_mi_kpos(nod_flag, key) < info->state->key_file_length);
      memcpy(rt_PAGE_END(page_buf), key - nod_flag, key_length + nod_flag); 
      page_size += key_length + nod_flag;
    }
    else
    {
      /* save key */
      DBUG_ASSERT(_mi_dpos(info, nod_flag, key + key_length +
                           info->s->base.rec_reflength) <
                  info->state->data_file_length + info->s->base.pack_reclength);
      memcpy(rt_PAGE_END(page_buf), key, key_length + 
                                         info->s->base.rec_reflength);
      page_size += key_length + info->s->base.rec_reflength;
    }
    mi_putint(page_buf, page_size, nod_flag);
    DBUG_RETURN(0);
  }

  DBUG_RETURN((rtree_split_page(info, keyinfo, page_buf, key, key_length,
                                new_page) ? -1 : 1));
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
  DBUG_ENTER("rtree_set_key_mbr");

  if (!_mi_fetch_keypage(info, keyinfo, child_page,
                         DFLT_INIT_HITS, info->buff, 0))
    DBUG_RETURN(-1); /* purecov: inspected */

  DBUG_RETURN(rtree_page_mbr(info, keyinfo->seg, info->buff, key, key_length));
}

#endif /*HAVE_RTREE_KEYS*/
