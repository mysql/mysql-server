/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

#include "myisamdef.h"

static ha_rows _mi_record_pos(MI_INFO *info,const byte *key,uint key_len,
			      enum ha_rkey_function search_flag);
static double _mi_search_pos(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,
			     uint key_len,uint nextflag,my_off_t pos);
static uint _mi_keynr(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *page,
		      uchar *keypos,uint *ret_max_key);


	/* If start_key = 0 assume read from start */
	/* If end_key = 0 assume read to end */
	/* Returns HA_POS_ERROR on error */

ha_rows mi_records_in_range(MI_INFO *info, int inx, const byte *start_key,
			    uint start_key_len,
			    enum ha_rkey_function start_search_flag,
			    const byte *end_key, uint end_key_len,
			    enum ha_rkey_function end_search_flag)
{
  ha_rows start_pos,end_pos;
  DBUG_ENTER("mi_records_in_range");

  if ((inx = _mi_check_index(info,inx)) < 0)
    DBUG_RETURN(HA_POS_ERROR);

  if (_mi_readinfo(info,F_RDLCK,1))
    DBUG_RETURN(HA_POS_ERROR);
  info->update&= (HA_STATE_CHANGED+HA_STATE_ROW_CHANGED);
  if (info->s->concurrent_insert)
    rw_rdlock(&info->s->key_root_lock[inx]);
  start_pos= (start_key ?
	      _mi_record_pos(info,start_key,start_key_len,start_search_flag) :
	      (ha_rows) 0);
  end_pos=   (end_key ?
	      _mi_record_pos(info,end_key,end_key_len,end_search_flag) :
	      info->state->records+ (ha_rows) 1);
  if (info->s->concurrent_insert)
    rw_unlock(&info->s->key_root_lock[inx]);
  VOID(_mi_writeinfo(info,0));
  if (start_pos == HA_POS_ERROR || end_pos == HA_POS_ERROR)
    DBUG_RETURN(HA_POS_ERROR);
  DBUG_PRINT("info",("records: %ld",(ulong) (end_pos-start_pos)));
  DBUG_RETURN(end_pos < start_pos ? (ha_rows) 0 :
	      (end_pos == start_pos ? (ha_rows) 1 : end_pos-start_pos));
}


	/* Find relative position (in records) for key in index-tree */

static ha_rows _mi_record_pos(MI_INFO *info, const byte *key, uint key_len,
			      enum ha_rkey_function search_flag)
{
  uint inx=(uint) info->lastinx;
  MI_KEYDEF *keyinfo=info->s->keyinfo+inx;
  uchar *key_buff;
  double pos;

  DBUG_ENTER("_mi_record_pos");
  DBUG_PRINT("enter",("search_flag: %d",search_flag));

  if (key_len == 0)
    key_len=USE_WHOLE_KEY;
  key_buff=info->lastkey+info->s->base.max_key_length;
  key_len=_mi_pack_key(info,inx,key_buff,(uchar*) key,key_len);
  DBUG_EXECUTE("key",_mi_print_key(DBUG_FILE,keyinfo->seg,
				    (uchar*) key_buff,key_len););
  pos=_mi_search_pos(info,keyinfo,key_buff,key_len,
		     myisam_read_vec[search_flag] | SEARCH_SAVE_BUFF,
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

static double _mi_search_pos(register MI_INFO *info,
			     register MI_KEYDEF *keyinfo,
			     uchar *key, uint key_len, uint nextflag,
			     register my_off_t pos)
{
  int flag;
  uint nod_flag,keynr,max_keynr;
  my_bool after_key;
  uchar *keypos,*buff;
  double offset;
  DBUG_ENTER("_mi_search_pos");

  if (pos == HA_OFFSET_ERROR)
    DBUG_RETURN(0.5);

  if (!(buff=_mi_fetch_keypage(info,keyinfo,pos,info->buff,1)))
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
    ** Didn't found match. keypos points at next (bigger) key
    *  Try to find a smaller, better matching key.
    ** Matches keynr + [0-1]
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
    ** Found match. Keypos points at the start of the found key
    ** Matches keynr+1
    */
    offset=1.0;					/* Matches keynr+1 */
    if (nextflag & SEARCH_FIND &&
	((keyinfo->flag & (HA_NOSAME | HA_NULL_PART)) != HA_NOSAME ||
	 key_len) && nod_flag)
    {
      /*
      ** There may be identical keys in the tree. Try to match on of those.
      ** Matches keynr + [0-1]
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
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1.0);
}


	/* Get keynummer of current key and max number of keys in nod */

static uint _mi_keynr(MI_INFO *info, register MI_KEYDEF *keyinfo, uchar *page, uchar *keypos, uint *ret_max_key)
{
  uint nod_flag,keynr,max_key;
  uchar t_buff[MI_MAX_KEY_BUFF],*end;

  end= page+mi_getint(page);
  nod_flag=mi_test_if_nod(page);
  page+=2+nod_flag;

  if (!(keyinfo->flag & (HA_VAR_LENGTH_KEY| HA_BINARY_PACK_KEY)))
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
