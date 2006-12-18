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
  Gives a approximated number of how many records there is between two keys.
  Used when optimizing querries.
 */

#include "maria_def.h"
#include "ma_rt_index.h"

static ha_rows _ma_record_pos(MARIA_HA *info,const byte *key,uint key_len,
			      enum ha_rkey_function search_flag);
static double _ma_search_pos(MARIA_HA *info,MARIA_KEYDEF *keyinfo,uchar *key,
			     uint key_len,uint nextflag,my_off_t pos);
static uint _ma_keynr(MARIA_HA *info,MARIA_KEYDEF *keyinfo,uchar *page,
		      uchar *keypos,uint *ret_max_key);


/*
  Estimate how many records there is in a given range

  SYNOPSIS
    maria_records_in_range()
    info		MARIA handler
    inx			Index to use
    min_key		Min key. Is = 0 if no min range
    max_key		Max key. Is = 0 if no max range

  NOTES
    We should ONLY return 0 if there is no rows in range

  RETURN
    HA_POS_ERROR  error (or we can't estimate number of rows)
    number	  Estimated number of rows
*/


ha_rows maria_records_in_range(MARIA_HA *info, int inx, key_range *min_key,
                            key_range *max_key)
{
  ha_rows start_pos,end_pos,res;
  DBUG_ENTER("maria_records_in_range");

  if ((inx = _ma_check_index(info,inx)) < 0)
    DBUG_RETURN(HA_POS_ERROR);

  if (fast_ma_readinfo(info))
    DBUG_RETURN(HA_POS_ERROR);
  info->update&= (HA_STATE_CHANGED+HA_STATE_ROW_CHANGED);
  if (info->s->concurrent_insert)
    rw_rdlock(&info->s->key_root_lock[inx]);

  switch(info->s->keyinfo[inx].key_alg){
#ifdef HAVE_RTREE_KEYS
  case HA_KEY_ALG_RTREE:
  {
    uchar * key_buff;
    uint start_key_len;

    key_buff= info->lastkey+info->s->base.max_key_length;
    start_key_len= _ma_pack_key(info,inx, key_buff,
                                (uchar*) min_key->key, min_key->length,
                                (HA_KEYSEG**) 0);
    res= maria_rtree_estimate(info, inx, key_buff, start_key_len,
                        maria_read_vec[min_key->flag]);
    res= res ? res : 1;                       /* Don't return 0 */
    break;
  }
#endif
  case HA_KEY_ALG_BTREE:
  default:
    start_pos= (min_key ?
                _ma_record_pos(info, min_key->key, min_key->length,
                               min_key->flag) :
                (ha_rows) 0);
    end_pos=   (max_key ?
                _ma_record_pos(info, max_key->key, max_key->length,
                               max_key->flag) :
                info->state->records+ (ha_rows) 1);
    res= (end_pos < start_pos ? (ha_rows) 0 :
          (end_pos == start_pos ? (ha_rows) 1 : end_pos-start_pos));
    if (start_pos == HA_POS_ERROR || end_pos == HA_POS_ERROR)
      res=HA_POS_ERROR;
  }

  if (info->s->concurrent_insert)
    rw_unlock(&info->s->key_root_lock[inx]);
  fast_ma_writeinfo(info);

  DBUG_PRINT("info",("records: %ld",(ulong) (res)));
  DBUG_RETURN(res);
}


	/* Find relative position (in records) for key in index-tree */

static ha_rows _ma_record_pos(MARIA_HA *info, const byte *key, uint key_len,
			      enum ha_rkey_function search_flag)
{
  uint inx=(uint) info->lastinx, nextflag;
  MARIA_KEYDEF *keyinfo=info->s->keyinfo+inx;
  uchar *key_buff;
  double pos;

  DBUG_ENTER("_ma_record_pos");
  DBUG_PRINT("enter",("search_flag: %d",search_flag));

  if (key_len == 0)
    key_len=USE_WHOLE_KEY;
  key_buff=info->lastkey+info->s->base.max_key_length;
  key_len= _ma_pack_key(info,inx,key_buff,(uchar*) key,key_len,
		       (HA_KEYSEG**) 0);
  DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE,keyinfo->seg,
				    (uchar*) key_buff,key_len););
  nextflag=maria_read_vec[search_flag];
  if (!(nextflag & (SEARCH_FIND | SEARCH_NO_FIND | SEARCH_LAST)))
    key_len=USE_WHOLE_KEY;

  pos= _ma_search_pos(info,keyinfo,key_buff,key_len,
		     nextflag | SEARCH_SAVE_BUFF,
		     info->s->state.key_root[inx]);
  if (pos >= 0.0)
  {
    DBUG_PRINT("exit",("pos: %ld",(ulong) (pos*info->state->records)));
    DBUG_RETURN((ulong) (pos*info->state->records+0.5));
  }
  DBUG_RETURN(HA_POS_ERROR);
}


	/* This is a modified version of _ma_search */
	/* Returns offset for key in indextable (decimal 0.0 <= x <= 1.0) */

static double _ma_search_pos(register MARIA_HA *info,
			     register MARIA_KEYDEF *keyinfo,
			     uchar *key, uint key_len, uint nextflag,
			     register my_off_t pos)
{
  int flag;
  uint nod_flag,keynr,max_keynr;
  my_bool after_key;
  uchar *keypos,*buff;
  double offset;
  DBUG_ENTER("_ma_search_pos");

  if (pos == HA_OFFSET_ERROR)
    DBUG_RETURN(0.5);

  if (!(buff= _ma_fetch_keypage(info,keyinfo,pos,DFLT_INIT_HITS,info->buff,1)))
    goto err;
  flag=(*keyinfo->bin_search)(info,keyinfo,buff,key,key_len,nextflag,
			      &keypos,info->lastkey, &after_key);
  nod_flag=_ma_test_if_nod(buff);
  keynr= _ma_keynr(info,keyinfo,buff,keypos,&max_keynr);

  if (flag)
  {
    if (flag == MARIA_FOUND_WRONG_KEY)
      DBUG_RETURN(-1);				/* error */
    /*
      Didn't found match. keypos points at next (bigger) key
      Try to find a smaller, better matching key.
      Matches keynr + [0-1]
    */
    if (flag > 0 && ! nod_flag)
      offset= 1.0;
    else if ((offset= _ma_search_pos(info,keyinfo,key,key_len,nextflag,
				    _ma_kpos(nod_flag,keypos))) < 0)
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
      if ((offset= _ma_search_pos(info,keyinfo,key,key_len,SEARCH_FIND,
				 _ma_kpos(nod_flag,keypos))) < 0)
	DBUG_RETURN(offset);			/* Read error */
    }
  }
  DBUG_PRINT("info",("keynr: %d  offset: %g  max_keynr: %d  nod: %d  flag: %d",
		     keynr,offset,max_keynr,nod_flag,flag));
  DBUG_RETURN((keynr+offset)/(max_keynr+1));
err:
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1.0);
}


	/* Get keynummer of current key and max number of keys in nod */

static uint _ma_keynr(MARIA_HA *info, register MARIA_KEYDEF *keyinfo, uchar *page,
                      uchar *keypos, uint *ret_max_key)
{
  uint nod_flag,keynr,max_key;
  uchar t_buff[HA_MAX_KEY_BUFF],*end;

  end= page+maria_getint(page);
  nod_flag=_ma_test_if_nod(page);
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
