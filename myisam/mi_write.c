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

/* Write a row to a MyISAM table */

#include "fulltext.h"
#ifdef	__WIN__
#include <errno.h>
#endif

#define MAX_POINTER_LENGTH 8

	/* Functions declared in this file */

static int w_search(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,
		    uint key_length, my_off_t pos, uchar *father_buff,
		    uchar *father_keypos, my_off_t father_page,
		    my_bool insert_last);
static int _mi_balance_page(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,
			    uchar *curr_buff,uchar *father_buff,
			    uchar *father_keypos,my_off_t father_page);
static uchar *_mi_find_last_pos(MI_KEYDEF *keyinfo, uchar *page,
				uchar *key, uint *return_key_length,
				uchar **after_key);
int _mi_ck_write_tree(register MI_INFO *info, uint keynr, uchar *key,
		      uint key_length);
int _mi_ck_write_btree(register MI_INFO *info, uint keynr, uchar *key,
		       uint key_length);

	/* Write new record to database */

int mi_write(MI_INFO *info, byte *record)
{
  MYISAM_SHARE *share=info->s;
  uint i;
  int save_errno;
  my_off_t filepos;
  uchar *buff;
  my_bool lock_tree= share->concurrent_insert;
  DBUG_ENTER("mi_write");
  DBUG_PRINT("enter",("isam: %d  data: %d",info->s->kfile,info->dfile));

  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    DBUG_RETURN(my_errno=EACCES);
  }
  if (_mi_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno);
  dont_break();				/* Dont allow SIGHUP or SIGINT */
#if !defined(NO_LOCKING) && defined(USE_RECORD_LOCK)
  if (!info->locked && my_lock(info->dfile,F_WRLCK,0L,F_TO_EOF,
			       MYF(MY_SEEK_NOT_DONE) | info->lock_wait))
    goto err;
#endif
  filepos= ((share->state.dellink != HA_OFFSET_ERROR) ?
	    share->state.dellink :
	    info->state->data_file_length);

  if (share->base.reloc == (ha_rows) 1 &&
      share->base.records == (ha_rows) 1 &&
      info->state->records == (ha_rows) 1)
  {						/* System file */
    my_errno=HA_ERR_RECORD_FILE_FULL;
    goto err2;
  }
  if (info->state->key_file_length >= share->base.margin_key_file_length)
  {
    my_errno=HA_ERR_INDEX_FILE_FULL;
    goto err2;
  }
  if (_mi_mark_file_changed(info))
    goto err2;

  /* Calculate and check all unique constraints */
  for (i=0 ; i < share->state.header.uniques ; i++)
  {
    if (mi_check_unique(info,share->uniqueinfo+i,record,
		     mi_unique_hash(share->uniqueinfo+i,record),
		     HA_OFFSET_ERROR))
      goto err2;
  }

	/* Write all keys to indextree */

  buff=info->lastkey2;
  for (i=0 ; i < share->base.keys ; i++)
  {
    if (((ulonglong) 1 << i) & share->state.key_map)
    {
      bool local_lock_tree= (lock_tree &&
			     !(info->bulk_insert &&
			       is_tree_inited(& info->bulk_insert[i])));
      if (local_lock_tree)
      {
	rw_wrlock(&share->key_root_lock[i]);
	share->keyinfo[i].version++;
      }
      if (share->keyinfo[i].flag & HA_FULLTEXT )
      {
        if (_mi_ft_add(info,i,(char*) buff,record,filepos))
        {
	  if (local_lock_tree)
	    rw_unlock(&share->key_root_lock[i]);
          DBUG_PRINT("error",("Got error: %d on write",my_errno));
          goto err;
        }
      }
      else
      {
	uint key_length=_mi_make_key(info,i,buff,record,filepos);
	if (_mi_ck_write(info,i,buff,key_length))
	{
	  if (local_lock_tree)
	    rw_unlock(&share->key_root_lock[i]);
	  DBUG_PRINT("error",("Got error: %d on write",my_errno));
	  goto err;
	}
      }
      if (local_lock_tree)
	rw_unlock(&share->key_root_lock[i]);
    }
  }
  if (share->calc_checksum)
    info->checksum=(*share->calc_checksum)(info,record);
  if (!(info->opt_flag & OPT_NO_ROWS))
  {
    if ((*share->write_record)(info,record))
      goto err;
    share->state.checksum+=info->checksum;
  }
  if (share->base.auto_key)
    update_auto_increment(info,record);
  info->update= (HA_STATE_CHANGED | HA_STATE_AKTIV | HA_STATE_WRITTEN |
		 HA_STATE_ROW_CHANGED);
  info->state->records++;
  info->lastpos=filepos;
  myisam_log_record(MI_LOG_WRITE,info,record,filepos,0);
  VOID(_mi_writeinfo(info, WRITEINFO_UPDATE_KEYFILE));
  allow_break();				/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  save_errno=my_errno;
  if (my_errno == HA_ERR_FOUND_DUPP_KEY || my_errno == HA_ERR_RECORD_FILE_FULL)
  {
    if (info->bulk_insert)
    {
      uint j;
      for (j=0 ; j < share->base.keys ; j++)
      {
        if (is_tree_inited(& info->bulk_insert[j]))
        {
          reset_tree(& info->bulk_insert[j]);
        }
      }
    }
    info->errkey= (int) i;
    while ( i-- > 0)
    {
      if (((ulonglong) 1 << i) & share->state.key_map)
      {
	bool local_lock_tree= (lock_tree &&
			       !(info->bulk_insert &&
				 is_tree_inited(& info->bulk_insert[i])));
	if (local_lock_tree)
	  rw_wrlock(&share->key_root_lock[i]);
	if (share->keyinfo[i].flag & HA_FULLTEXT)
        {
          if (_mi_ft_del(info,i,(char*) buff,record,filepos))
	  {
	    if (local_lock_tree)
	      rw_unlock(&share->key_root_lock[i]);
            break;
	  }
        }
        else
	{
	  uint key_length=_mi_make_key(info,i,buff,record,filepos);
	  if (_mi_ck_delete(info,i,buff,key_length))
	  {
	    if (local_lock_tree)
	      rw_unlock(&share->key_root_lock[i]);
	    break;
	  }
	}
	if (local_lock_tree)
	  rw_unlock(&share->key_root_lock[i]);
      }
    }
  }
  else
    mi_mark_crashed(info);
  info->update= (HA_STATE_CHANGED | HA_STATE_WRITTEN | HA_STATE_ROW_CHANGED);
  my_errno=save_errno;
err2:
  save_errno=my_errno;
  myisam_log_record(MI_LOG_WRITE,info,record,filepos,my_errno);
  VOID(_mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
  allow_break();			/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(my_errno=save_errno);
} /* mi_write */


	/* Write one key to btree */

int _mi_ck_write(MI_INFO *info, uint keynr, uchar *key, uint key_length)
{
  DBUG_ENTER("_mi_ck_write");

  if (info->bulk_insert && is_tree_inited(& info->bulk_insert[keynr]))
  {
    DBUG_RETURN(_mi_ck_write_tree(info, keynr, key, key_length));
  }
  else
  {
    DBUG_RETURN(_mi_ck_write_btree(info, keynr, key, key_length));
  }
} /* _mi_ck_write */


/**********************************************************************
 *                Normal insert code                                  *
 **********************************************************************/

int _mi_ck_write_btree(register MI_INFO *info, uint keynr, uchar *key,
		       uint key_length)
{
  int error;
  DBUG_ENTER("_mi_ck_write_btree");

  if (info->s->state.key_root[keynr] == HA_OFFSET_ERROR ||
      (error=w_search(info,info->s->keyinfo+keynr,key, key_length,
		      info->s->state.key_root[keynr], (uchar *) 0, (uchar*) 0,
		      (my_off_t) 0, 1)) > 0)
    error=_mi_enlarge_root(info,keynr,key);
  DBUG_RETURN(error);
} /* _mi_ck_write_btree */


	/* Make a new root with key as only pointer */

int _mi_enlarge_root(register MI_INFO *info, uint keynr, uchar *key)
{
  uint t_length,nod_flag;
  reg2 MI_KEYDEF *keyinfo;
  MI_KEY_PARAM s_temp;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_enlarge_root");

  nod_flag= (share->state.key_root[keynr] != HA_OFFSET_ERROR) ?
    share->base.key_reflength : 0;
  _mi_kpointer(info,info->buff+2,share->state.key_root[keynr]); /* if nod */
  keyinfo=share->keyinfo+keynr;
  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,(uchar*) 0,
				(uchar*) 0, (uchar*) 0, key,&s_temp);
  mi_putint(info->buff,t_length+2+nod_flag,nod_flag);
  (*keyinfo->store_key)(keyinfo,info->buff+2+nod_flag,&s_temp);
  info->buff_used=info->page_changed=1;		/* info->buff is used */
  if ((share->state.key_root[keynr]= _mi_new(info,keyinfo)) ==
      HA_OFFSET_ERROR ||
      _mi_write_keypage(info,keyinfo,share->state.key_root[keynr],info->buff))
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
} /* _mi_enlarge_root */


	/*
	  Search after a position for a key and store it there
	  Returns -1 = error
		   0  = ok
		   1  = key should be stored in higher tree
	*/

static int w_search(register MI_INFO *info, register MI_KEYDEF *keyinfo,
		    uchar *key, uint key_length, my_off_t page,
		    uchar *father_buff,
		    uchar *father_keypos, my_off_t father_page,
		    my_bool insert_last)
{
  int error,flag;
  uint comp_flag,nod_flag, search_key_length;
  uchar *temp_buff,*keypos;
  uchar keybuff[MI_MAX_KEY_BUFF];
  my_bool was_last_key;
  my_off_t next_page;
  DBUG_ENTER("w_search");
  DBUG_PRINT("enter",("page: %ld",page));

  search_key_length=USE_WHOLE_KEY;
  if (keyinfo->flag & HA_SORT_ALLOWS_SAME)
    comp_flag=SEARCH_BIGGER;			/* Put after same key */
  else if (keyinfo->flag & HA_NOSAME)
  {
    comp_flag=SEARCH_FIND | SEARCH_UPDATE;	/* No dupplicates */
    search_key_length= key_length;
  }
  else
    comp_flag=SEARCH_SAME;			/* Keys in rec-pos order */

  if (!(temp_buff= (uchar*) my_alloca((uint) keyinfo->block_length+
				      MI_MAX_KEY_BUFF*2)))
    DBUG_RETURN(-1);
  if (!_mi_fetch_keypage(info,keyinfo,page,temp_buff,0))
    goto err;

  flag=(*keyinfo->bin_search)(info,keyinfo,temp_buff,key,search_key_length,
			      comp_flag, &keypos, keybuff, &was_last_key);
  nod_flag=mi_test_if_nod(temp_buff);
  if (flag == 0)
  {
    uint tmp_key_length;
    my_errno=HA_ERR_FOUND_DUPP_KEY;
	/* get position to record with duplicated key */
    tmp_key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,keybuff);
    if (tmp_key_length)
      info->dupp_key_pos=_mi_dpos(info,0,keybuff+tmp_key_length);
    else
      info->dupp_key_pos= HA_OFFSET_ERROR;
    my_afree((byte*) temp_buff);
    DBUG_RETURN(-1);
  }
  if (flag == MI_FOUND_WRONG_KEY)
    DBUG_RETURN(-1);
  if (!was_last_key)
    insert_last=0;
  next_page=_mi_kpos(nod_flag,keypos);
  if (next_page == HA_OFFSET_ERROR ||
      (error=w_search(info,keyinfo,key,key_length,next_page,
		      temp_buff, keypos, page, insert_last)) >0)
  {
    error=_mi_insert(info,keyinfo,key,temp_buff,keypos,keybuff,father_buff,
		     father_keypos,father_page, insert_last);
    if (_mi_write_keypage(info,keyinfo,page,temp_buff))
      goto err;
  }
  my_afree((byte*) temp_buff);
  DBUG_RETURN(error);
err:
  my_afree((byte*) temp_buff);
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1);
} /* w_search */


	/* Insert new key at right of key_pos */
	/* Returns 2 if key contains key to upper level */

int _mi_insert(register MI_INFO *info, register MI_KEYDEF *keyinfo,
	       uchar *key, uchar *anc_buff, uchar *key_pos, uchar *key_buff,
               uchar *father_buff, uchar *father_key_pos, my_off_t father_page,
	       my_bool insert_last)

{
  uint a_length,nod_flag;
  int t_length;
  uchar *endpos, *prev_key;
  MI_KEY_PARAM s_temp;
  DBUG_ENTER("_mi_insert");
  DBUG_PRINT("enter",("key_pos: %lx",key_pos));
  DBUG_EXECUTE("key",_mi_print_key(DBUG_FILE,keyinfo->seg,key,USE_WHOLE_KEY););

  nod_flag=mi_test_if_nod(anc_buff);
  a_length=mi_getint(anc_buff);
  endpos= anc_buff+ a_length;
  prev_key=(key_pos == anc_buff+2+nod_flag ? (uchar*) 0 : key_buff);
  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,
				(key_pos == endpos ? (uchar*) 0 : key_pos),
				prev_key, prev_key,
				key,&s_temp);
#ifndef DBUG_OFF
  if (key_pos != anc_buff+2+nod_flag && (keyinfo->flag &
					 (HA_BINARY_PACK_KEY | HA_PACK_KEY)))
    DBUG_DUMP("prev_key",(byte*) key_buff,_mi_keylength(keyinfo,key_buff));
  if (keyinfo->flag & HA_PACK_KEY)
  {
    DBUG_PRINT("test",("t_length: %d  ref_len: %d",
		       t_length,s_temp.ref_length));
    DBUG_PRINT("test",("n_ref_len: %d  n_length: %d  key: %lx",
		       s_temp.n_ref_length,s_temp.n_length,s_temp.key));
  }
#endif
  if (t_length > 0)
  {
    if (t_length >= keyinfo->maxlength*2+MAX_POINTER_LENGTH)
    {
      my_errno=HA_ERR_CRASHED;
      DBUG_RETURN(-1);
    }
    bmove_upp((byte*) endpos+t_length,(byte*) endpos,(uint) (endpos-key_pos));
  }
  else
  {
    if (-t_length >= keyinfo->maxlength*2+MAX_POINTER_LENGTH)
    {
      my_errno=HA_ERR_CRASHED;
      DBUG_RETURN(-1);
    }
    bmove(key_pos,key_pos-t_length,(uint) (endpos-key_pos)+t_length);
  }
  (*keyinfo->store_key)(keyinfo,key_pos,&s_temp);
  a_length+=t_length;
  mi_putint(anc_buff,a_length,nod_flag);
  if (a_length <= keyinfo->block_length)
    DBUG_RETURN(0);				/* There is room on page */

  /* Page is full */
  if (nod_flag)
    insert_last=0;
  if (!(keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)) &&
      father_buff && !insert_last)
    DBUG_RETURN(_mi_balance_page(info,keyinfo,key,anc_buff,father_buff,
				 father_key_pos,father_page));
  DBUG_RETURN(_mi_split_page(info,keyinfo,key,anc_buff,key_buff, insert_last));
} /* _mi_insert */


	/* split a full page in two and assign emerging item to key */

int _mi_split_page(register MI_INFO *info, register MI_KEYDEF *keyinfo,
		   uchar *key, uchar *buff, uchar *key_buff,
		   my_bool insert_last_key)
{
  uint length,a_length,key_ref_length,t_length,nod_flag,key_length;
  uchar *key_pos,*pos, *after_key;
  my_off_t new_pos;
  MI_KEY_PARAM s_temp;
  DBUG_ENTER("mi_split_page");
  DBUG_DUMP("buff",(byte*) buff,mi_getint(buff));

  if (info->s->keyinfo+info->lastinx == keyinfo)
    info->page_changed=1;			/* Info->buff is used */
  info->buff_used=1;
  nod_flag=mi_test_if_nod(buff);
  key_ref_length=2+nod_flag;
  if (insert_last_key)
    key_pos=_mi_find_last_pos(keyinfo,buff,key_buff, &key_length, &after_key);
  else
    key_pos=_mi_find_half_pos(nod_flag,keyinfo,buff,key_buff, &key_length,
			      &after_key);
  if (!key_pos)
    DBUG_RETURN(-1);
  length=(uint) (key_pos-buff);
  a_length=mi_getint(buff);
  mi_putint(buff,length,nod_flag);

  key_pos=after_key;
  if (nod_flag)
  {
    DBUG_PRINT("test",("Splitting nod"));
    pos=key_pos-nod_flag;
    memcpy((byte*) info->buff+2,(byte*) pos,(size_t) nod_flag);
  }

	/* Move middle item to key and pointer to new page */
  if ((new_pos=_mi_new(info,keyinfo)) == HA_OFFSET_ERROR)
    DBUG_RETURN(-1);
  _mi_kpointer(info,_mi_move_key(keyinfo,key,key_buff),new_pos);

	/* Store new page */
  if (!(*keyinfo->get_key)(keyinfo,nod_flag,&key_pos,key_buff))
    DBUG_RETURN(-1);
  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,(uchar *) 0,
				(uchar*) 0, (uchar*) 0,
				key_buff, &s_temp);
  length=(uint) ((buff+a_length)-key_pos);
  memcpy((byte*) info->buff+key_ref_length+t_length,(byte*) key_pos,
	 (size_t) length);
  (*keyinfo->store_key)(keyinfo,info->buff+key_ref_length,&s_temp);
  mi_putint(info->buff,length+t_length+key_ref_length,nod_flag);

  if (_mi_write_keypage(info,keyinfo,new_pos,info->buff))
    DBUG_RETURN(-1);
  DBUG_DUMP("key",(byte*) key,_mi_keylength(keyinfo,key));
  DBUG_RETURN(2);				/* Middle key up */
} /* _mi_split_page */


	/*
	  Calculate how to much to move to split a page in two
	  Returns pointer to start of key.
	  key will contain the key.
	  return_key_length will contain the length of key
	  after_key will contain the position to where the next key starts
	*/

uchar *_mi_find_half_pos(uint nod_flag, MI_KEYDEF *keyinfo, uchar *page,
			 uchar *key, uint *return_key_length,
			 uchar **after_key)
{
  uint keys,length,key_ref_length;
  uchar *end,*lastpos;
  DBUG_ENTER("_mi_find_half_pos");

  key_ref_length=2+nod_flag;
  length=mi_getint(page)-key_ref_length;
  page+=key_ref_length;
  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)))
  {
    key_ref_length=keyinfo->keylength+nod_flag;
    keys=length/(key_ref_length*2);
    *return_key_length=keyinfo->keylength;
    end=page+keys*key_ref_length;
    *after_key=end+key_ref_length;
    memcpy(key,end,key_ref_length);
    DBUG_RETURN(end);
  }

  end=page+length/2-key_ref_length;		/* This is aprox. half */
  *key='\0';
  do
  {
    lastpos=page;
    if (!(length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,key)))
      DBUG_RETURN(0);
  } while (page < end);
  *return_key_length=length;
  *after_key=page;
  DBUG_PRINT("exit",("returns: %lx  page: %lx  half: %lx",lastpos,page,end));
  DBUG_RETURN(lastpos);
} /* _mi_find_half_pos */


	/*
	  Split buffer at last key
	  Returns pointer to the start of the key before the last key
	  key will contain the last key
	*/

static uchar *_mi_find_last_pos(MI_KEYDEF *keyinfo, uchar *page,
				uchar *key, uint *return_key_length,
				uchar **after_key)
{
  uint keys,length,last_length,key_ref_length;
  uchar *end,*lastpos,*prevpos;
  uchar key_buff[MI_MAX_KEY_BUFF];
  DBUG_ENTER("_mi_find_last_pos");

  key_ref_length=2;
  length=mi_getint(page)-key_ref_length;
  page+=key_ref_length;
  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)))
  {
    keys=length/keyinfo->keylength-2;
    *return_key_length=length=keyinfo->keylength;
    end=page+keys*length;
    *after_key=end+length;
    memcpy(key,end,length);
    DBUG_RETURN(end);
  }

  LINT_INIT(prevpos);
  LINT_INIT(last_length);
  end=page+length-key_ref_length;
  *key='\0';
  length=0;
  lastpos=page;
  while (page < end)
  {
    prevpos=lastpos; lastpos=page;
    last_length=length;
    memcpy(key, key_buff, length);		/* previous key */
    if (!(length=(*keyinfo->get_key)(keyinfo,0,&page,key_buff)))
    {
      my_errno=HA_ERR_CRASHED;
      DBUG_RETURN(0);
    }
  }
  *return_key_length=last_length;
  *after_key=lastpos;
  DBUG_PRINT("exit",("returns: %lx  page: %lx  end: %lx",prevpos,page,end));
  DBUG_RETURN(prevpos);
} /* _mi_find_last_pos */


	/* Balance page with not packed keys with page on right/left */
	/* returns 0 if balance was done */

static int _mi_balance_page(register MI_INFO *info, MI_KEYDEF *keyinfo,
			    uchar *key, uchar *curr_buff, uchar *father_buff,
			    uchar *father_key_pos, my_off_t father_page)
{
  my_bool right;
  uint k_length,father_length,father_keylength,nod_flag,curr_keylength,
       right_length,left_length,new_right_length,new_left_length,extra_length,
       length,keys;
  uchar *pos,*buff,*extra_buff;
  my_off_t next_page,new_pos;
  byte tmp_part_key[MI_MAX_KEY_BUFF];
  DBUG_ENTER("_mi_balance_page");

  k_length=keyinfo->keylength;
  father_length=mi_getint(father_buff);
  father_keylength=k_length+info->s->base.key_reflength;
  nod_flag=mi_test_if_nod(curr_buff);
  curr_keylength=k_length+nod_flag;
  info->page_changed=1;

  if ((father_key_pos != father_buff+father_length && (info->s->rnd++ & 1)) ||
      father_key_pos == father_buff+2+info->s->base.key_reflength)
  {
    right=1;
    next_page= _mi_kpos(info->s->base.key_reflength,
			father_key_pos+father_keylength);
    buff=info->buff;
    DBUG_PRINT("test",("use right page: %lu",next_page));
  }
  else
  {
    right=0;
    father_key_pos-=father_keylength;
    next_page= _mi_kpos(info->s->base.key_reflength,father_key_pos);
					/* Fix that curr_buff is to left */
    buff=curr_buff; curr_buff=info->buff;
    DBUG_PRINT("test",("use left page: %lu",next_page));
  }					/* father_key_pos ptr to parting key */

  if (!_mi_fetch_keypage(info,keyinfo,next_page,info->buff,0))
    goto err;
  DBUG_DUMP("next",(byte*) info->buff,mi_getint(info->buff));

	/* Test if there is room to share keys */

  left_length=mi_getint(curr_buff);
  right_length=mi_getint(buff);
  keys=(left_length+right_length-4-nod_flag*2)/curr_keylength;

  if ((right ? right_length : left_length) + curr_keylength <=
      keyinfo->block_length)
  {						/* Merge buffs */
    new_left_length=2+nod_flag+(keys/2)*curr_keylength;
    new_right_length=2+nod_flag+((keys+1)/2)*curr_keylength;
    mi_putint(curr_buff,new_left_length,nod_flag);
    mi_putint(buff,new_right_length,nod_flag);

    if (left_length < new_left_length)
    {						/* Move keys buff -> leaf */
      pos=curr_buff+left_length;
      memcpy((byte*) pos,(byte*) father_key_pos, (size_t) k_length);
      memcpy((byte*) pos+k_length, (byte*) buff+2,
	     (size_t) (length=new_left_length - left_length - k_length));
      pos=buff+2+length;
      memcpy((byte*) father_key_pos,(byte*) pos,(size_t) k_length);
      bmove((byte*) buff+2,(byte*) pos+k_length,new_right_length);
    }
    else
    {						/* Move keys -> buff */

      bmove_upp((byte*) buff+new_right_length,(byte*) buff+right_length,
		right_length-2);
      length=new_right_length-right_length-k_length;
      memcpy((byte*) buff+2+length,father_key_pos,(size_t) k_length);
      pos=curr_buff+new_left_length;
      memcpy((byte*) father_key_pos,(byte*) pos,(size_t) k_length);
      memcpy((byte*) buff+2,(byte*) pos+k_length,(size_t) length);
    }

    if (_mi_write_keypage(info,keyinfo,next_page,info->buff) ||
	_mi_write_keypage(info,keyinfo,father_page,father_buff))
      goto err;
    DBUG_RETURN(0);
  }

	/* curr_buff[] and buff[] are full, lets split and make new nod */

  extra_buff=info->buff+info->s->base.max_key_block_length;
  new_left_length=new_right_length=2+nod_flag+(keys+1)/3*curr_keylength;
  if (keys == 5)				/* Too few keys to balance */
    new_left_length-=curr_keylength;
  extra_length=nod_flag+left_length+right_length-
    new_left_length-new_right_length-curr_keylength;
  DBUG_PRINT("info",("left_length: %d  right_length: %d  new_left_length: %d  new_right_length: %d  extra_length: %d",
		     left_length, right_length,
		     new_left_length, new_right_length,
		     extra_length));
  mi_putint(curr_buff,new_left_length,nod_flag);
  mi_putint(buff,new_right_length,nod_flag);
  mi_putint(extra_buff,extra_length+2,nod_flag);

  /* move first largest keys to new page  */
  pos=buff+right_length-extra_length;
  memcpy((byte*) extra_buff+2,pos,(size_t) extra_length);
  /* Save new parting key */
  memcpy(tmp_part_key, pos-k_length,k_length);
  /* Make place for new keys */
  bmove_upp((byte*) buff+new_right_length,(byte*) pos-k_length,
	    right_length-extra_length-k_length-2);
  /* Copy keys from left page */
  pos= curr_buff+new_left_length;
  memcpy((byte*) buff+2,(byte*) pos+k_length,
	 (size_t) (length=left_length-new_left_length-k_length));
  /* Copy old parting key */
  memcpy((byte*) buff+2+length,father_key_pos,(size_t) k_length);

  /* Move new parting keys up to caller */
  memcpy((byte*) (right ? key : father_key_pos),pos,(size_t) k_length);
  memcpy((byte*) (right ? father_key_pos : key),tmp_part_key, k_length);

  if ((new_pos=_mi_new(info,keyinfo)) == HA_OFFSET_ERROR)
    goto err;
  _mi_kpointer(info,key+k_length,new_pos);
  if (_mi_write_keypage(info,keyinfo,(right ? new_pos : next_page),
			info->buff) ||
      _mi_write_keypage(info,keyinfo,(right ? next_page : new_pos),extra_buff))
    goto err;

  DBUG_RETURN(1);				/* Middle key up */

err:
  DBUG_RETURN(-1);
} /* _mi_balance_page */

/**********************************************************************
 *                Bulk insert code                                    *
 **********************************************************************/

typedef struct {
  MI_INFO *info;
  uint keynr;
} bulk_insert_param;

int _mi_ck_write_tree(register MI_INFO *info, uint keynr, uchar *key,
		      uint key_length)
{
  int error;
  DBUG_ENTER("_mi_ck_write_tree");

  error= tree_insert(& info->bulk_insert[keynr], key,
         key_length + info->s->rec_reflength) ? 0 : HA_ERR_OUT_OF_MEM ;

  DBUG_RETURN(error);
} /* _mi_ck_write_tree */


/* typeof(_mi_keys_compare)=qsort_cmp2 */
static int keys_compare(bulk_insert_param *param, uchar *key1, uchar *key2)
{
  uint not_used;
  return _mi_key_cmp(param->info->s->keyinfo[param->keynr].seg,
              key1, key2, USE_WHOLE_KEY, SEARCH_SAME, &not_used);
}


static int keys_free(uchar *key, TREE_FREE mode, bulk_insert_param *param)
{
  /* probably I can use info->lastkey here, but I'm not sure,
     and to be safe I'd better use local lastkey.
     Monty, feel free to comment on this */
  uchar lastkey[MI_MAX_KEY_BUFF];
  uint keylen;
  MI_KEYDEF *keyinfo;

  switch (mode) {
    case free_init:
      if (param->info->s->concurrent_insert)
      {
        rw_wrlock(&param->info->s->key_root_lock[param->keynr]);
        param->info->s->keyinfo[param->keynr].version++;
      }
      return 0;
    case free_free:
      keyinfo=param->info->s->keyinfo+param->keynr;
      keylen=_mi_keylength(keyinfo, key);
      memcpy(lastkey, key, keylen);
      return _mi_ck_write_btree(param->info,param->keynr,lastkey, 
                 keylen - param->info->s->rec_reflength);
    case free_end:
      if (param->info->s->concurrent_insert)
        rw_unlock(&param->info->s->key_root_lock[param->keynr]);
      return 0;
  }
  return -1;
}

int _mi_init_bulk_insert(MI_INFO *info)
{
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF *key=share->keyinfo;
  bulk_insert_param *params;
  uint i, num_keys;
  ulonglong key_map=0;

  if (info->bulk_insert)
    return 0;
  
  for (i=num_keys=0 ; i < share->base.keys ; i++)
  {
    if (!(key[i].flag & HA_NOSAME) && share->base.auto_key != i+1
        && test(share->state.key_map & ((ulonglong) 1 << i)))
    {
      num_keys++;
      key_map |=((ulonglong) 1 << i);
    }
  }

  if (!num_keys)
    return 0;
  
  info->bulk_insert=(TREE *)
    my_malloc((sizeof(TREE)*share->base.keys+
               sizeof(bulk_insert_param)*num_keys),MYF(0));

  if (!info->bulk_insert)
    return HA_ERR_OUT_OF_MEM;

  params=(bulk_insert_param *)(info->bulk_insert+share->base.keys);
  for (i=0 ; i < share->base.keys ; i++,key++)
  {
    if (test(key_map & ((ulonglong) 1 << i)))
    {
      params->info=info;
      params->keynr=i;
      init_tree(& info->bulk_insert[i], 0, 
		myisam_bulk_insert_tree_size / num_keys, 0,
		(qsort_cmp2)keys_compare, 0,
		(tree_element_free) keys_free, (void *)params++);
    }
    else
     info->bulk_insert[i].root=0; 
  }

  return 0;
}
