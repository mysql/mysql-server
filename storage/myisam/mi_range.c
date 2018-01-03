/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.


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
  Gives a approximated number of how many records there is between two keys.
  Used when optimizing querries.
 */

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisam/rt_index.h"

static ha_rows _mi_record_pos(MI_INFO *, const uchar *, key_part_map,
                              enum ha_rkey_function);
static double _mi_search_pos(MI_INFO *,MI_KEYDEF *,uchar *, uint,uint,my_off_t);
static uint _mi_keynr(MI_INFO *info,MI_KEYDEF *,uchar *, uchar *,uint *);

/*
  Estimate how many records there is in a given range

  SYNOPSIS
    mi_records_in_range()
    info		MyISAM handler
    inx			Index to use
    min_key		Min key. Is = 0 if no min range
    max_key		Max key. Is = 0 if no max range

  NOTES
    We should ONLY return 0 if there is no rows in range

  RETURN
    HA_POS_ERROR  error (or we can't estimate number of rows)
    number	  Estimated number of rows
*/
  
ha_rows mi_records_in_range(MI_INFO *info, int inx,
                            key_range *min_key, key_range *max_key)
{
  ha_rows start_pos,end_pos,res;
  DBUG_ENTER("mi_records_in_range");

  if ((inx = _mi_check_index(info,inx)) < 0)
    DBUG_RETURN(HA_POS_ERROR);

  if (fast_mi_readinfo(info))
    DBUG_RETURN(HA_POS_ERROR);
  info->update&= (HA_STATE_CHANGED+HA_STATE_ROW_CHANGED);
  if (info->s->concurrent_insert)
    mysql_rwlock_rdlock(&info->s->key_root_lock[inx]);

  switch(info->s->keyinfo[inx].key_alg){
  case HA_KEY_ALG_RTREE:
  {
    uchar * key_buff;
    uint start_key_len;

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
    key_buff= info->lastkey+info->s->base.max_key_length;
    start_key_len= _mi_pack_key(info,inx, key_buff,
                                (uchar*) min_key->key, min_key->keypart_map,
                                (HA_KEYSEG**) 0);
    res= rtree_estimate(info, inx, key_buff, start_key_len,
                        myisam_read_vec[min_key->flag]);
    res= res ? res : 1;                       /* Don't return 0 */
    break;
  }
  case HA_KEY_ALG_BTREE:
  default:
    start_pos= (min_key ?  _mi_record_pos(info, min_key->key,
                                          min_key->keypart_map, min_key->flag)
                        : (ha_rows) 0);
    end_pos=   (max_key ?  _mi_record_pos(info, max_key->key,
                                          max_key->keypart_map, max_key->flag)
                        : info->state->records + (ha_rows) 1);
    res= (end_pos < start_pos ? (ha_rows) 0 :
          (end_pos == start_pos ? (ha_rows) 1 : end_pos-start_pos));
    if (start_pos == HA_POS_ERROR || end_pos == HA_POS_ERROR)
      res=HA_POS_ERROR;
  }

  if (info->s->concurrent_insert)
    mysql_rwlock_unlock(&info->s->key_root_lock[inx]);
  fast_mi_writeinfo(info);

  DBUG_PRINT("info",("records: %ld",(ulong) (res)));
  DBUG_RETURN(res);
}


	/* Find relative position (in records) for key in index-tree */

static ha_rows _mi_record_pos(MI_INFO *info, const uchar *key,
                              key_part_map keypart_map,
			      enum ha_rkey_function search_flag)
{
  uint inx=(uint) info->lastinx, nextflag, key_len;
  MI_KEYDEF *keyinfo=info->s->keyinfo+inx;
  uchar *key_buff;
  double pos;

  DBUG_ENTER("_mi_record_pos");
  DBUG_PRINT("enter",("search_flag: %d",search_flag));
  DBUG_ASSERT(keypart_map);

  key_buff=info->lastkey+info->s->base.max_key_length;
  key_len=_mi_pack_key(info,inx,key_buff,(uchar*) key, keypart_map,
		       (HA_KEYSEG**) 0);
  DBUG_EXECUTE("key",_mi_print_key(DBUG_FILE,keyinfo->seg,
				    (uchar*) key_buff,key_len););
  nextflag=myisam_read_vec[search_flag];
  if (!(nextflag & (SEARCH_FIND | SEARCH_NO_FIND | SEARCH_LAST)))
    key_len=USE_WHOLE_KEY;

  /*
    my_compare.c:ha_compare_text() has a flag 'skip_end_space'.
    This is set in my_compare.c:ha_key_cmp() in dependence on the
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
  pos=_mi_search_pos(info,keyinfo,key_buff,key_len,
		     nextflag | SEARCH_SAVE_BUFF | SEARCH_UPDATE,
		     info->s->state.key_root[inx]);
  if (pos >= 0.0)
  {
    DBUG_PRINT("exit",("pos: %ld",(ulong) (pos*info->state->records)));
    DBUG_RETURN((ulong) (pos*info->state->records+0.5));
  }
  DBUG_RETURN(HA_POS_ERROR);
}


	/* This is a modified version of _mi_search */
	/* Returns offset for key in indextable (decimal 0.0 <= x <= 1.0) */

static double _mi_search_pos(MI_INFO *info,
			     MI_KEYDEF *keyinfo,
			     uchar *key, uint key_len, uint nextflag,
			     my_off_t pos)
{
  int flag;
  uint nod_flag, keynr, max_keynr= 0;
  bool after_key;
  uchar *keypos,*buff;
  double offset;
  DBUG_ENTER("_mi_search_pos");

  if (pos == HA_OFFSET_ERROR)
    DBUG_RETURN(0.5);

  if (!(buff=_mi_fetch_keypage(info,keyinfo,pos,DFLT_INIT_HITS,info->buff,1)))
    goto err;
  flag=(*keyinfo->bin_search)(info,keyinfo,buff,key,key_len,nextflag,
			      &keypos,info->lastkey, &after_key);
  nod_flag=mi_test_if_nod(buff);
  keynr=_mi_keynr(info,keyinfo,buff,keypos,&max_keynr);

  if (flag)
  {
    if (flag == MI_FOUND_WRONG_KEY)
      DBUG_RETURN(-1);				/* error */
    /*
      Didn't found match. keypos points at next (bigger) key
      Try to find a smaller, better matching key.
      Matches keynr + [0-1]
    */
    if (flag > 0 && ! nod_flag)
      offset= 1.0;
    else if ((offset=_mi_search_pos(info,keyinfo,key,key_len,nextflag,
				    _mi_kpos(nod_flag,keypos))) < 0)
      DBUG_RETURN(offset);
  }
  else
  {
    /*
      Found match. Keypos points at the start of the found key
      Matches keynr+1
    */
    offset=1.0;					/* Matches keynr+1 */
    if ((nextflag & SEARCH_FIND) && nod_flag &&
	((keyinfo->flag & (HA_NOSAME | HA_NULL_PART)) != HA_NOSAME ||
	 key_len != USE_WHOLE_KEY))
    {
      /*
        There may be identical keys in the tree. Try to match on of those.
        Matches keynr + [0-1]
      */
      if ((offset=_mi_search_pos(info,keyinfo,key,key_len,SEARCH_FIND,
				 _mi_kpos(nod_flag,keypos))) < 0)
	DBUG_RETURN(offset);			/* Read error */
    }
  }
  DBUG_PRINT("info",("keynr: %d  offset: %g  max_keynr: %d  nod: %d  flag: %d",
		     keynr,offset,max_keynr,nod_flag,flag));
  DBUG_RETURN((keynr+offset)/(max_keynr+1));
err:
  DBUG_PRINT("exit",("Error: %d",my_errno()));
  DBUG_RETURN (-1.0);
}


	/* Get keynummer of current key and max number of keys in nod */

static uint _mi_keynr(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                      uchar *keypos, uint *ret_max_key)
{
  uint nod_flag,keynr,max_key;
  uchar t_buff[MI_MAX_KEY_BUFF],*end;

  end= page+mi_getint(page);
  nod_flag=mi_test_if_nod(page);
  page+=2+nod_flag;

  if (!(keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)))
  {
    *ret_max_key= (uint) (end-page)/(keyinfo->keylength+nod_flag);
    return (uint) (keypos-page)/(keyinfo->keylength+nod_flag);
  }

  max_key=keynr=0;
  t_buff[0]=0;					/* Safety */
  while (page < end)
  {
    if (!(*keyinfo->get_key)(keyinfo,nod_flag,&page,t_buff))
      return 0;					/* Error */
    max_key++;
    if (page == keypos)
      keynr=max_key;
  }
  *ret_max_key=max_key;
  return(keynr);
}
