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

/* Read record based on a key */

#include "maria_def.h"
#include "ma_rt_index.h"

/**
  Read a record using key

  @note
  Ordinary search_flag is 0 ; Give error if no record with key
*/

int maria_rkey(MARIA_HA *info, uchar *buf, int inx, const uchar *key_data,
               key_part_map keypart_map, enum ha_rkey_function search_flag)
{
  uchar *key_buff;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo;
  HA_KEYSEG *last_used_keyseg;
  uint32 nextflag;
  MARIA_KEY key;
  ICP_RESULT icp_res= ICP_MATCH;
  DBUG_ENTER("maria_rkey");
  DBUG_PRINT("enter", ("base: 0x%lx  buf: 0x%lx  inx: %d  search_flag: %d",
                       (long) info, (long) buf, inx, search_flag));

  if ((inx = _ma_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->last_key_func= search_flag;
  keyinfo= info->last_key.keyinfo;

  key_buff= info->lastkey_buff+info->s->base.max_key_length;

  if (info->once_flags & USE_PACKED_KEYS)
  {
    info->once_flags&= ~USE_PACKED_KEYS;	/* Reset flag */
    /*
      key is already packed!;  This happens when we are using a MERGE TABLE
      In this key 'key_part_map' is the length of the key !
    */
    bmove(key_buff, key_data, keypart_map);
    key.data=    key_buff;
    key.keyinfo= keyinfo;
    key.data_length= keypart_map;
    key.ref_length= 0;
    key.flag= 0;

    last_used_keyseg= keyinfo->seg + info->last_used_keyseg;
  }
  else
  {
    DBUG_ASSERT(keypart_map);
    /* Save the packed key for later use in the second buffer of lastkey. */
    _ma_pack_key(info, &key, inx, key_buff, key_data,
                 keypart_map, &last_used_keyseg);
    /* Save packed_key_length for use by the MERGE engine. */
    info->pack_key_length= key.data_length;
    info->last_used_keyseg= (uint16) (last_used_keyseg -
                                      keyinfo->seg);
    DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE, &key););
  }

  if (fast_ma_readinfo(info))
    goto err;
  if (share->lock_key_trees)
    mysql_rwlock_rdlock(&keyinfo->root_lock);

  nextflag= maria_read_vec[search_flag] | key.flag;
  if (search_flag != HA_READ_KEY_EXACT)
  {
    /* Assume we will get a read next/previous call after this one */
    nextflag|= SEARCH_SAVE_BUFF;
  }
  switch (keyinfo->key_alg) {
#ifdef HAVE_RTREE_KEYS
  case HA_KEY_ALG_RTREE:
    if (maria_rtree_find_first(info, &key, nextflag) < 0)
    {
      _ma_set_fatal_error(share, HA_ERR_CRASHED);
      info->cur_row.lastpos= HA_OFFSET_ERROR;
    }
    break;
#endif
  case HA_KEY_ALG_BTREE:
  default:
    if (!_ma_search(info, &key, nextflag, info->s->state.key_root[inx]))
    {      
      MARIA_KEY lastkey;
      /*
        Found a key, but it might not be usable. We cannot use rows that
        are inserted by other threads after we got our table lock
        ("concurrent inserts"). The record may not even be present yet.
        Keys are inserted into the index(es) before the record is
        inserted into the data file.

        If index condition is present, it must be either satisfied or 
        not satisfied with an out-of-range condition.
      */
      if ((*share->row_is_visible)(info) && 
          ((icp_res= ma_check_index_cond(info, inx, buf)) != ICP_NO_MATCH))
        break;

      /* The key references a concurrently inserted record. */
      if (search_flag == HA_READ_KEY_EXACT &&
          last_used_keyseg == keyinfo->seg + keyinfo->keysegs)
      {
        /* Simply ignore the key if it matches exactly. (Bug #29838) */
        my_errno= HA_ERR_KEY_NOT_FOUND;
        info->cur_row.lastpos= HA_OFFSET_ERROR;
        break;
      }
      
      lastkey.keyinfo= keyinfo;
      lastkey.data= info->lastkey_buff;
      do
      {
        uint not_used[2];
        /*
          Skip rows that are inserted by other threads since we got
          a lock. Note that this can only happen if we are not
          searching after a full length exact key, because the keys
          are sorted according to position.
        */
        lastkey.data_length= info->last_key.data_length;
        lastkey.ref_length=  info->last_key.ref_length;
        lastkey.flag=        info->last_key.flag;
        if  (_ma_search_next(info, &lastkey, maria_readnext_vec[search_flag],
                             info->s->state.key_root[inx]))
          break;                          /* purecov: inspected */

        /*
          If we are at the last key on the key page, allow writers to
          access the index.
        */
        if (info->int_keypos >= info->int_maxpos &&
            ma_yield_and_check_if_killed(info, inx))
        {
          DBUG_ASSERT(info->cur_row.lastpos == HA_OFFSET_ERROR);
          break;
        }

        /*
          Check that the found key does still match the search.
          _ma_search_next() delivers the next key regardless of its
          value.
        */
        if (!(nextflag & (SEARCH_BIGGER | SEARCH_SMALLER)) &&
            ha_key_cmp(keyinfo->seg, info->last_key.data, key.data,
                       key.data_length, SEARCH_FIND, not_used))
        {
          /* purecov: begin inspected */
          my_errno= HA_ERR_KEY_NOT_FOUND;
          info->cur_row.lastpos= HA_OFFSET_ERROR;
          break;
          /* purecov: end */
        }

      } while (!(*share->row_is_visible)(info) || 
               ((icp_res= ma_check_index_cond(info, inx, buf)) == 0));
    }
    else
    {
      DBUG_ASSERT(info->cur_row.lastpos);
    }
  }
  if (share->lock_key_trees)
    mysql_rwlock_unlock(&keyinfo->root_lock);

  if (info->cur_row.lastpos == HA_OFFSET_ERROR)
  {
    if (icp_res == ICP_OUT_OF_RANGE)
    {
      /* We don't want HA_ERR_END_OF_FILE in this particular case */
      my_errno= HA_ERR_KEY_NOT_FOUND;
    }
    fast_ma_writeinfo(info);
    goto err;
  }
  
  /* Calculate length of the found key;  Used by maria_rnext_same */
  if ((keyinfo->flag & HA_VAR_LENGTH_KEY))
    info->last_rkey_length= _ma_keylength_part(keyinfo, info->lastkey_buff,
					       last_used_keyseg);
  else
    info->last_rkey_length= key.data_length;

  /* Check if we don't want to have record back, only error message */
  if (!buf)
  {
    fast_ma_writeinfo(info);
    DBUG_RETURN(0);
  }
  if (!(*info->read_record)(info, buf, info->cur_row.lastpos))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }

  info->cur_row.lastpos= HA_OFFSET_ERROR;      /* Didn't find row */

err:
  /* Store last used key as a base for read next */
  memcpy(info->last_key.data, key_buff, key.data_length);
  info->last_key.data_length= key.data_length;
  info->last_key.ref_length=  info->s->base.rec_reflength;
  info->last_key.flag= 0;
  /* Create key with rowid 0 */
  bzero((char*) info->last_key.data + info->last_key.data_length,
        info->s->base.rec_reflength);

  if (search_flag == HA_READ_AFTER_KEY)
    info->update|=HA_STATE_NEXT_FOUND;		/* Previous gives last row */
  DBUG_RETURN(my_errno);
} /* _ma_rkey */


/*
  Yield to possible other writers during a index scan.
  Check also if we got killed by the user and if yes, return
  HA_ERR_LOCK_WAIT_TIMEOUT

  return  0  ok
  return  1  Query has been requested to be killed
*/

my_bool ma_yield_and_check_if_killed(MARIA_HA *info, int inx)
{
  MARIA_SHARE *share;
  if (ma_killed(info))
  {
    /* purecov: begin tested */
    /* Mark that we don't have an active row */
    info->cur_row.lastpos= HA_OFFSET_ERROR;
    /* Set error that we where aborted by kill from application */
    my_errno= HA_ERR_ABORTED_BY_USER;
    return 1;
    /* purecov: end */
  }

  if ((share= info->s)->lock_key_trees)
  {
    /* Give writers a chance to access index */
    mysql_rwlock_unlock(&share->keyinfo[inx].root_lock);
    mysql_rwlock_rdlock(&share->keyinfo[inx].root_lock);
  }
  return 0;
}

