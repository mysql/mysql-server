/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
  Gives a approximated number of how many records there is between two keys.
  Used when optimizing querries.
 */

#include "maria_def.h"
#include "ma_rt_index.h"

static ha_rows _ma_record_pos(MARIA_HA *,const uchar *, key_part_map,
			      enum ha_rkey_function);
static double _ma_search_pos(MARIA_HA *, MARIA_KEY *, uint32, my_off_t);
static uint _ma_keynr(MARIA_PAGE *page, uchar *keypos, uint *ret_max_key);


/**
   @brief Estimate how many records there is in a given range

   @param  info            MARIA handler
   @param  inx             Index to use
   @param  min_key         Min key. Is = 0 if no min range
   @param  max_key         Max key. Is = 0 if no max range

   @note
     We should ONLY return 0 if there is no rows in range

   @return Estimated number of rows or error
     @retval HA_POS_ERROR  error (or we can't estimate number of rows)
     @retval number        Estimated number of rows
*/

ha_rows maria_records_in_range(MARIA_HA *info, int inx, key_range *min_key,
                            key_range *max_key)
{
  ha_rows start_pos,end_pos,res;
  MARIA_SHARE *share= info->s;
  MARIA_KEY key;
  MARIA_KEYDEF *keyinfo;
  DBUG_ENTER("maria_records_in_range");

  if ((inx = _ma_check_index(info,inx)) < 0)
    DBUG_RETURN(HA_POS_ERROR);

  if (fast_ma_readinfo(info))
    DBUG_RETURN(HA_POS_ERROR);
  info->update&= (HA_STATE_CHANGED+HA_STATE_ROW_CHANGED);
  keyinfo= share->keyinfo + inx;
  if (share->lock_key_trees)
    rw_rdlock(&keyinfo->root_lock);

  switch (keyinfo->key_alg) {
#ifdef HAVE_RTREE_KEYS
  case HA_KEY_ALG_RTREE:
  {
    uchar *key_buff;

    /*
      The problem is that the optimizer doesn't support
      RTree keys properly at the moment.
      Hope this will be fixed some day.
      But now NULL in the min_key means that we
      didn't make the task for the RTree key
      and expect BTree functionality from it.
      As it's not able to handle such request
      we return the error.
    */
    if (!min_key)
    {
      res= HA_POS_ERROR;
      break;
    }
    key_buff= info->last_key.data + share->base.max_key_length;
    _ma_pack_key(info, &key, inx, key_buff,
                 min_key->key, min_key->keypart_map,
                 (HA_KEYSEG**) 0);
    res= maria_rtree_estimate(info, &key, maria_read_vec[min_key->flag]);
    res= res ? res : 1;                       /* Don't return 0 */
    break;
  }
#endif
  case HA_KEY_ALG_BTREE:
  default:
    start_pos= (min_key ?
                _ma_record_pos(info, min_key->key, min_key->keypart_map,
                               min_key->flag) :
                (ha_rows) 0);
    end_pos=   (max_key ?
                _ma_record_pos(info, max_key->key, max_key->keypart_map,
                               max_key->flag) :
                info->state->records + (ha_rows) 1);
    res= (end_pos < start_pos ? (ha_rows) 0 :
          (end_pos == start_pos ? (ha_rows) 1 : end_pos-start_pos));
    if (start_pos == HA_POS_ERROR || end_pos == HA_POS_ERROR)
      res=HA_POS_ERROR;
  }

  if (share->lock_key_trees)
    rw_unlock(&keyinfo->root_lock);
  fast_ma_writeinfo(info);

  /**
     @todo LOCK
     If res==0 (no rows), if we need to guarantee repeatability of the search,
     we will need to set a next-key lock in this statement.
     Also SELECT COUNT(*)...
  */

  DBUG_PRINT("info",("records: %ld",(ulong) (res)));
  DBUG_RETURN(res);
}


	/* Find relative position (in records) for key in index-tree */

static ha_rows _ma_record_pos(MARIA_HA *info, const uchar *key_data,
                              key_part_map keypart_map,
			      enum ha_rkey_function search_flag)
{
  uint inx= (uint) info->lastinx;
  uint32 nextflag;
  uchar *key_buff;
  double pos;
  MARIA_KEY key;
  DBUG_ENTER("_ma_record_pos");
  DBUG_PRINT("enter",("search_flag: %d",search_flag));
  DBUG_ASSERT(keypart_map);

  key_buff= info->lastkey_buff+info->s->base.max_key_length;
  _ma_pack_key(info, &key, inx, key_buff, key_data, keypart_map,
		       (HA_KEYSEG**) 0);
  DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE, &key););
  nextflag=maria_read_vec[search_flag];

  /*
    my_handler.c:ha_compare_text() has a flag 'skip_end_space'.
    This is set in my_handler.c:ha_key_cmp() in dependence on the
    compare flags 'nextflag' and the column type.

    TEXT columns are of type HA_KEYTYPE_VARTEXT. In this case the
    condition is skip_end_space= ((nextflag & (SEARCH_FIND |
    SEARCH_UPDATE)) == SEARCH_FIND).

    SEARCH_FIND is used for an exact key search. The combination
    SEARCH_FIND | SEARCH_UPDATE is used in write/update/delete
    operations with a comment like "Not real duplicates", whatever this
    means. From the condition above we can see that 'skip_end_space' is
    always false for these operations. The result is that trailing space
    counts in key comparison and hence, emtpy strings ('', string length
    zero, but not NULL) compare less that strings starting with control
    characters and these in turn compare less than strings starting with
    blanks.

    When estimating the number of records in a key range, we request an
    exact search for the minimum key. This translates into a plain
    SEARCH_FIND flag. Using this alone would lead to a 'skip_end_space'
    compare. Empty strings would be expected above control characters.
    Their keys would not be found because they are located below control
    characters.

    This is the reason that we add the SEARCH_UPDATE flag here. It makes
    the key estimation compare in the same way like key write operations
    do. Olny so we will find the keys where they have been inserted.

    Adding the flag unconditionally does not hurt as it is used in the
    above mentioned condition only. So it can safely be used together
    with other flags.
  */
  pos= _ma_search_pos(info, &key,
                      nextflag | SEARCH_SAVE_BUFF | SEARCH_UPDATE,
                      info->s->state.key_root[inx]);
  if (pos >= 0.0)
  {
    DBUG_PRINT("exit",("pos: %ld",(ulong) (pos*info->state->records)));
    DBUG_RETURN((ulong) (pos*info->state->records+0.5));
  }
  DBUG_RETURN(HA_POS_ERROR);
}


/**
  Find offset for key on index page

  @notes
   Modified version of _ma_search()

  @return
  @retval 0.0 <= x <= 1.0
*/

static double _ma_search_pos(MARIA_HA *info, MARIA_KEY *key,
			     uint32 nextflag, my_off_t pos)
{
  int flag;
  uint keynr, max_keynr;
  my_bool after_key;
  uchar *keypos;
  double offset;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_PAGE page;
  DBUG_ENTER("_ma_search_pos");
  LINT_INIT(max_keynr);

  if (pos == HA_OFFSET_ERROR)
    DBUG_RETURN(0.5);

  if (_ma_fetch_keypage(&page, info, keyinfo, pos,
                        PAGECACHE_LOCK_LEFT_UNLOCKED, DFLT_INIT_HITS,
                        info->buff, 1))
    goto err;
  flag= (*keyinfo->bin_search)(key, &page, nextflag, &keypos,
                               info->lastkey_buff, &after_key);
  keynr= _ma_keynr(&page, keypos, &max_keynr);

  if (flag)
  {
    if (flag == MARIA_FOUND_WRONG_KEY)
      DBUG_RETURN(-1);				/* error */
    /*
      Didn't found match. keypos points at next (bigger) key
      Try to find a smaller, better matching key.
      Matches keynr + [0-1]
    */
    if (flag > 0 && ! page.node)
      offset= 1.0;
    else if ((offset= _ma_search_pos(info, key, nextflag,
                                     _ma_kpos(page.node,keypos))) < 0)
      DBUG_RETURN(offset);
  }
  else
  {
    /*
      Found match. Keypos points at the start of the found key
      Matches keynr+1
    */
    offset=1.0;					/* Matches keynr+1 */
    if ((nextflag & SEARCH_FIND) && page.node &&
	((keyinfo->flag & (HA_NOSAME | HA_NULL_PART)) != HA_NOSAME ||
         (nextflag & (SEARCH_PREFIX | SEARCH_NO_FIND | SEARCH_LAST |
                      SEARCH_PART_KEY))))
    {
      /*
        There may be identical keys in the tree. Try to match on of those.
        Matches keynr + [0-1]
      */
      if ((offset= _ma_search_pos(info, key, SEARCH_FIND,
                                  _ma_kpos(page.node,keypos))) < 0)
	DBUG_RETURN(offset);			/* Read error */
    }
  }
  DBUG_PRINT("info",("keynr: %d  offset: %g  max_keynr: %d  nod: %d  flag: %d",
		     keynr,offset,max_keynr,page.node,flag));
  DBUG_RETURN((keynr+offset)/(max_keynr+1));
err:
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1.0);
}


/* Get keynummer of current key and max number of keys in nod */

static uint _ma_keynr(MARIA_PAGE *page, uchar *keypos, uint *ret_max_key)
{
  uint page_flag, nod_flag, keynr, max_key;
  uchar t_buff[MARIA_MAX_KEY_BUFF], *pos, *end;
  const MARIA_KEYDEF *keyinfo= page->keyinfo;
  MARIA_KEY key;

  page_flag= page->flag;
  nod_flag=  page->node;
  pos= page->buff + page->info->s->keypage_header + nod_flag;
  end= page->buff + page->size;

  if (!(keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)) &&
      ! (page_flag & KEYPAGE_FLAG_HAS_TRANSID))
  {
    *ret_max_key= (uint) (end - pos)/(keyinfo->keylength+nod_flag);
    return (uint) (keypos - pos)/(keyinfo->keylength+nod_flag);
  }

  max_key=keynr=0;
  t_buff[0]=0;					/* Safety */
  key.data= t_buff;
  key.keyinfo= (MARIA_KEYDEF*) keyinfo;

  while (pos < end)
  {
    if (!(pos= (*keyinfo->skip_key)(&key, page_flag, nod_flag, pos)))
    {
      DBUG_ASSERT(0);
      return 0;					/* Error */
    }
    max_key++;
    if (pos == keypos)
      keynr= max_key;
  }
  *ret_max_key=max_key;
  return(keynr);
}
