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

/* Tar bort ett record fr}n en isam-databas */

#include "isamdef.h"
#ifdef	__WIN__
#include <errno.h>
#endif
#include <assert.h>

static int d_search(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,ulong page,
		    uchar *anc_buff);
static int del(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,uchar *anc_buff,
	       ulong leaf_page,uchar *leaf_buff,uchar *keypos,
	       ulong next_block,uchar *ret_key);
static int underflow(N_INFO *info,N_KEYDEF *keyinfo,uchar *anc_buff,
		     ulong leaf_page, uchar *leaf_buff,uchar *keypos);
static uint remove_key(N_KEYDEF *keyinfo,uint nod_flag,uchar *keypos,
		       uchar *lastkey,uchar *page_end);


int nisam_delete(N_INFO *info,const byte *record)
{
  uint i;
  uchar *old_key;
  int save_errno;
  uint32 lastpos;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("nisam_delete");

	/* Test if record is in datafile */

  if (!(info->update & HA_STATE_AKTIV))
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;		/* No database read */
    DBUG_RETURN(-1);
  }
  if (share->base.options & HA_OPTION_READ_ONLY_DATA)
  {
    my_errno=EACCES;
    DBUG_RETURN(-1);
  }
#ifndef NO_LOCKING
  if (_nisam_readinfo(info,F_WRLCK,1)) DBUG_RETURN(-1);
#endif
  if ((*share->compare_record)(info,record))
    goto err;				/* Fel vid kontroll-l{sning */

	/* Remove all keys from the .ISAM file */

  old_key=info->lastkey+share->base.max_key_length;
  for (i=0 ; i < share->state.keys ; i++ )
  {
    VOID(_nisam_make_key(info,i,old_key,record,info->lastpos));
    if (_nisam_ck_delete(info,i,old_key)) goto err;
  }

  if ((*share->delete_record)(info))
    goto err;				/* Remove record from database */

  info->update= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | HA_STATE_DELETED;
  share->state.records--;

  lastpos= (uint32) info->lastpos;
  nisam_log_command(LOG_DELETE,info,(byte*) &lastpos,sizeof(lastpos),0);
  VOID(_nisam_writeinfo(info,1));
  allow_break();			/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  save_errno=my_errno;
  lastpos= (uint32) info->lastpos;
  nisam_log_command(LOG_DELETE,info,(byte*) &lastpos, sizeof(lastpos),0);
  VOID(_nisam_writeinfo(info,1));
  info->update|=HA_STATE_WRITTEN;	/* Buffer changed */
  allow_break();			/* Allow SIGHUP & SIGINT */
  my_errno=save_errno;
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
    my_errno=HA_ERR_CRASHED;

  DBUG_RETURN(-1);
} /* nisam_delete */


	/* Tar bort en nyckel till isam-nyckelfilen */

int _nisam_ck_delete(register N_INFO *info, uint keynr, uchar *key)
{
  int error;
  uint nod_flag;
  ulong old_root;
  uchar *root_buff;
  N_KEYDEF *keyinfo;
  DBUG_ENTER("_nisam_ck_delete");

  if ((old_root=info->s->state.key_root[keynr]) == NI_POS_ERROR)
  {
    my_errno=HA_ERR_CRASHED;
    DBUG_RETURN(-1);
  }
  keyinfo=info->s->keyinfo+keynr;
  if (!(root_buff= (uchar*) my_alloca((uint) keyinfo->base.block_length+
				      N_MAX_KEY_BUFF*2)))
    DBUG_RETURN(-1);
  if (!_nisam_fetch_keypage(info,keyinfo,old_root,root_buff,0))
  {
    error= -1;
    goto err;
  }
  if ((error=d_search(info,keyinfo,key,old_root,root_buff)) >0)
  {
    if (error == 2)
    {
      DBUG_PRINT("test",("Enlarging of root when deleting"));
      error=_nisam_enlarge_root(info,keynr,key);
    }
    else
    {
      error=0;
      if (getint(root_buff) <= (nod_flag=test_if_nod(root_buff))+3)
      {
	if (nod_flag)
	  info->s->state.key_root[keynr]=_nisam_kpos(nod_flag,
					       root_buff+2+nod_flag);
	else
	  info->s->state.key_root[keynr]= NI_POS_ERROR;
	if (_nisam_dispose(info,keyinfo,old_root))
	  error= -1;
      }
    }
  }
err:
  my_afree((gptr) root_buff);
  DBUG_RETURN(error);
} /* _nisam_ck_delete */


	/* Tar bort en nyckel under root */
	/* Returnerar 1 om nuvarande buffert minskade */
	/* Returnerar 2 om nuvarande buffert |kar */

static int d_search(register N_INFO *info, register N_KEYDEF *keyinfo, uchar *key, ulong page, uchar *anc_buff)
{
  int flag,ret_value,save_flag;
  uint length,nod_flag;
  uchar *leaf_buff,*keypos,*next_keypos;
  ulong leaf_page,next_block;
  uchar lastkey[N_MAX_KEY_BUFF];
  DBUG_ENTER("d_search");
  DBUG_DUMP("page",(byte*) anc_buff,getint(anc_buff));

  flag=(*keyinfo->bin_search)(info,keyinfo,anc_buff,key,0,SEARCH_SAME,&keypos,
			      lastkey);
  nod_flag=test_if_nod(anc_buff);

  leaf_buff=0;
  LINT_INIT(leaf_page);
  if (nod_flag)
  {
    leaf_page=_nisam_kpos(nod_flag,keypos);
    if (!(leaf_buff= (uchar*) my_alloca((uint) keyinfo->base.block_length+
					N_MAX_KEY_BUFF*2)))
    {
      my_errno=ENOMEM;
      DBUG_RETURN(-1);
    }
    if (!_nisam_fetch_keypage(info,keyinfo,leaf_page,leaf_buff,0))
      goto err;
  }

  if (flag != 0)
  {
    if (!nod_flag)
    {
      my_errno=HA_ERR_CRASHED;		/* This should newer happend */
      goto err;
    }
    save_flag=0;
    ret_value=d_search(info,keyinfo,key,leaf_page,leaf_buff);
  }
  else
  {						/* Found key */
    next_keypos=keypos;				/* Find where next block is */
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&next_keypos,lastkey));
    next_block=_nisam_kpos(nod_flag,next_keypos);
    length=getint(anc_buff);
    length-= remove_key(keyinfo,nod_flag,keypos,lastkey,anc_buff+length);
    putint(anc_buff,length,nod_flag);
    if (!nod_flag)
    {						/* On leaf page */
      if (_nisam_write_keypage(info,keyinfo,page,anc_buff))
	DBUG_RETURN(-1);
      DBUG_RETURN(length <= (uint) keyinfo->base.block_length/2);
    }
    save_flag=1;
    ret_value=del(info,keyinfo,key,anc_buff,leaf_page,leaf_buff,keypos,
		  next_block,lastkey);
  }
  if (ret_value >0)
  {
    save_flag=1;
    if (ret_value == 1)
      ret_value= underflow(info,keyinfo,anc_buff,leaf_page,leaf_buff,keypos);
    else
    {				/* This happens only with packed keys */
      DBUG_PRINT("test",("Enlarging of key when deleting"));
      VOID(_nisam_get_last_key(info,keyinfo,anc_buff,lastkey,keypos));
      ret_value=_nisam_insert(info,keyinfo,key,anc_buff,keypos,lastkey,
			   (uchar*) 0,(uchar*) 0,0L);
    }
  }
  if (ret_value == 0 && getint(anc_buff) > keyinfo->base.block_length)
  {
    save_flag=1;
    ret_value=_nisam_splitt_page(info,keyinfo,key,anc_buff,lastkey) | 2;
  }
  if (save_flag)
    ret_value|=_nisam_write_keypage(info,keyinfo,page,anc_buff);
  else
  {
    DBUG_DUMP("page",(byte*) anc_buff,getint(anc_buff));
  }
  my_afree((byte*) leaf_buff);
  DBUG_RETURN(ret_value);
err:
  my_afree((byte*) leaf_buff);
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1);
} /* d_search */


	/* Remove a key that has a page-reference */

static int del(register N_INFO *info, register N_KEYDEF *keyinfo, uchar *key,
	       uchar *anc_buff, ulong leaf_page, uchar *leaf_buff,
	       uchar *keypos,		/* Pos to where deleted key was */
	       ulong next_block,
	       uchar *ret_key)		/* key before keypos in anc_buff */
{
  int ret_value,length;
  uint a_length,nod_flag;
  ulong next_page;
  uchar keybuff[N_MAX_KEY_BUFF],*endpos,*next_buff,*key_start;
  ISAM_SHARE *share=info->s;
  S_PARAM s_temp;
  DBUG_ENTER("del");
  DBUG_PRINT("enter",("leaf_page: %ld   keypos: %lx",leaf_page,keypos));
  DBUG_DUMP("leaf_buff",(byte*) leaf_buff,getint(leaf_buff));

  endpos=leaf_buff+getint(leaf_buff);
  key_start=_nisam_get_last_key(info,keyinfo,leaf_buff,keybuff,endpos);

  if ((nod_flag=test_if_nod(leaf_buff)))
  {
    next_page= _nisam_kpos(nod_flag,endpos);
    if (!(next_buff= (uchar*) my_alloca((uint) keyinfo->base.block_length+
					N_MAX_KEY_BUFF)))
      DBUG_RETURN(-1);
    if (!_nisam_fetch_keypage(info,keyinfo,next_page,next_buff,0))
      ret_value= -1;
    else
    {
      DBUG_DUMP("next_page",(byte*) next_buff,getint(next_buff));
      if ((ret_value=del(info,keyinfo,key,anc_buff,next_page,next_buff,
			 keypos,next_block,ret_key)) >0)
      {
	endpos=leaf_buff+getint(leaf_buff);
	if (ret_value == 1)
	{
	  ret_value=underflow(info,keyinfo,leaf_buff,next_page,
			      next_buff,endpos);
	  if (ret_value == 0 && getint(leaf_buff) > keyinfo->base.block_length)
	  {
	    ret_value=_nisam_splitt_page(info,keyinfo,key,leaf_buff,ret_key) | 2;
	  }
	}
	else
	{
	  DBUG_PRINT("test",("Inserting of key when deleting"));
	  VOID(_nisam_get_last_key(info,keyinfo,leaf_buff,keybuff,endpos));
	  ret_value=_nisam_insert(info,keyinfo,key,leaf_buff,endpos,keybuff,
			       (uchar*) 0,(uchar*) 0,0L);
	}
      }
      if (_nisam_write_keypage(info,keyinfo,leaf_page,leaf_buff))
	goto err;
    }
    my_afree((byte*) next_buff);
    DBUG_RETURN(ret_value);
  }

	/* Remove last key from leaf page */

  putint(leaf_buff,key_start-leaf_buff,nod_flag);
  if (_nisam_write_keypage(info,keyinfo,leaf_page,leaf_buff))
    goto err;

	/* Place last key in ancestor page on deleted key position */

  a_length=getint(anc_buff);
  endpos=anc_buff+a_length;
  VOID(_nisam_get_last_key(info,keyinfo,anc_buff,ret_key,keypos));
  length=_nisam_get_pack_key_length(keyinfo,share->base.key_reflength,
				 keypos == endpos ? (uchar*) 0 : keypos,
				 ret_key,keybuff,&s_temp);
  if (length > 0)
    bmove_upp((byte*) endpos+length,(byte*) endpos,(uint) (endpos-keypos));
  else
    bmove(keypos,keypos-length, (int) (endpos-keypos)+length);
  _nisam_store_key(keyinfo,keypos,&s_temp);
	/* Save pointer to next leaf */
  VOID((*keyinfo->get_key)(keyinfo,share->base.key_reflength,&keypos,ret_key));
  _nisam_kpointer(info,keypos - share->base.key_reflength,next_block);
  putint(anc_buff,a_length+length,share->base.key_reflength);

  DBUG_RETURN( getint(leaf_buff) <= (uint) keyinfo->base.block_length/2 );
err:
  DBUG_RETURN(-1);
} /* del */


	/* Balances adjacent pages if underflow occours */

static int underflow(register N_INFO *info, register N_KEYDEF *keyinfo,
		     uchar *anc_buff,
		     ulong leaf_page,	/* Ancestor page and underflow page */
		     uchar *leaf_buff,
		     uchar *keypos)	/* Position to pos after key */
{
  int t_length;
  uint length,anc_length,buff_length,leaf_length,p_length,s_length,nod_flag;
  ulong next_page;
  uchar anc_key[N_MAX_KEY_BUFF],leaf_key[N_MAX_KEY_BUFF],
	*buff,*endpos,*next_keypos,*half_pos,*temp_pos;
  S_PARAM s_temp;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("underflow");
  DBUG_PRINT("enter",("leaf_page: %ld   keypos: %lx",leaf_page,keypos));
  DBUG_DUMP("anc_buff",(byte*) anc_buff,getint(anc_buff));
  DBUG_DUMP("leaf_buff",(byte*) leaf_buff,getint(leaf_buff));

  buff=info->buff;
  next_keypos=keypos;
  nod_flag=test_if_nod(leaf_buff);
  p_length=2+nod_flag;
  anc_length=getint(anc_buff);
  leaf_length=getint(leaf_buff);
  info->page_changed=1;

  if ((keypos < anc_buff+anc_length && (share->rnd++ & 1)) ||
      keypos == anc_buff+2+share->base.key_reflength)
  {					/* Use page right of anc-page */
    DBUG_PRINT("test",("use right page"));

    VOID((*keyinfo->get_key)(keyinfo,share->base.key_reflength,&next_keypos,
			     buff));
    next_page= _nisam_kpos(share->base.key_reflength,next_keypos);
    if (!_nisam_fetch_keypage(info,keyinfo,next_page,buff,0))
      goto err;
    buff_length=getint(buff);
    DBUG_DUMP("next",(byte*) buff,buff_length);

    /* find keys to make a big key-page */
    bmove((byte*) next_keypos-share->base.key_reflength,(byte*) buff+2,
	  share->base.key_reflength);
    VOID(_nisam_get_last_key(info,keyinfo,anc_buff,anc_key,next_keypos));
    VOID(_nisam_get_last_key(info,keyinfo,leaf_buff,leaf_key,
			  leaf_buff+leaf_length));

    /* merge pages and put parting key from anc_buff between */
    t_length=(int) _nisam_get_pack_key_length(keyinfo,nod_flag,buff+p_length,
					   (leaf_length == nod_flag+2 ?
					    (uchar*) 0 : leaf_key),
					   anc_key,&s_temp);
    length=buff_length-p_length;
    endpos=buff+length+leaf_length+t_length;
    bmove_upp((byte*) endpos, (byte*) buff+buff_length,length);
    memcpy((byte*) buff, (byte*) leaf_buff,(size_t) leaf_length);
    _nisam_store_key(keyinfo,buff+leaf_length,&s_temp);
    buff_length=(uint) (endpos-buff);
    putint(buff,buff_length,nod_flag);

    /* remove key from anc_buff */

    s_length=remove_key(keyinfo,share->base.key_reflength,keypos,anc_key,
			anc_buff+anc_length);
    putint(anc_buff,(anc_length-=s_length),share->base.key_reflength);

    if (buff_length <= keyinfo->base.block_length)
    {						/* Keys in one page */
      memcpy((byte*) leaf_buff,(byte*) buff,(size_t) buff_length);
      if (_nisam_dispose(info,keyinfo,next_page))
       goto err;
    }
    else
    {						/* Page is full */
      VOID(_nisam_get_last_key(info,keyinfo,anc_buff,anc_key,keypos));
      half_pos=_nisam_find_half_pos(info,keyinfo,buff,leaf_key);
      length=(uint) (half_pos-buff);
      memcpy((byte*) leaf_buff,(byte*) buff,(size_t) length);
      putint(leaf_buff,length,nod_flag);
      endpos=anc_buff+anc_length;

      /* Correct new keypointer to leaf_page */
      length=(*keyinfo->get_key)(keyinfo,nod_flag,&half_pos,leaf_key);
      _nisam_kpointer(info,leaf_key+length,next_page);
      /* Save key in anc_buff */
      t_length=(int) _nisam_get_pack_key_length(keyinfo,
						share->base.key_reflength,
						keypos == endpos ?
						(uchar*) 0 : keypos,
						anc_key,leaf_key,&s_temp);
      if (t_length >= 0)
	bmove_upp((byte*) endpos+t_length,(byte*) endpos,
		  (uint) (endpos-keypos));
      else
	bmove(keypos,keypos-t_length,(uint) (endpos-keypos)+t_length);
      _nisam_store_key(keyinfo,keypos,&s_temp);
      putint(anc_buff,(anc_length+=t_length),share->base.key_reflength);

	/* Store new page */
      if (nod_flag)
	bmove((byte*) buff+2,(byte*) half_pos-nod_flag,(size_t) nod_flag);
      VOID((*keyinfo->get_key)(keyinfo,nod_flag,&half_pos,leaf_key));
      t_length=(int) _nisam_get_pack_key_length(keyinfo,nod_flag,(uchar*) 0,
					     (uchar*) 0, leaf_key,&s_temp);
      s_temp.n_length= *half_pos;	/* For _nisam_store_key */
      length=(buff+getint(buff))-half_pos;
      bmove((byte*) buff+p_length+t_length,(byte*) half_pos,(size_t) length);
      _nisam_store_key(keyinfo,buff+p_length,&s_temp);
      putint(buff,length+t_length+p_length,nod_flag);

      if (_nisam_write_keypage(info,keyinfo,next_page,buff))
	goto err;
    }
    if (_nisam_write_keypage(info,keyinfo,leaf_page,leaf_buff))
      goto err;
    DBUG_RETURN(anc_length <= (uint) keyinfo->base.block_length/2);
  }

  DBUG_PRINT("test",("use left page"));

  keypos=_nisam_get_last_key(info,keyinfo,anc_buff,anc_key,keypos);
  next_page= _nisam_kpos(share->base.key_reflength,keypos);
  if (!_nisam_fetch_keypage(info,keyinfo,next_page,buff,0))
      goto err;
  buff_length=getint(buff);
  endpos=buff+buff_length;
  DBUG_DUMP("prev",(byte*) buff,buff_length);

  /* find keys to make a big key-page */
  bmove((byte*) next_keypos - share->base.key_reflength,(byte*) leaf_buff+2,
	share->base.key_reflength);
  next_keypos=keypos;
  VOID((*keyinfo->get_key)(keyinfo,share->base.key_reflength,&next_keypos,
			   anc_key));
  VOID(_nisam_get_last_key(info,keyinfo,buff,leaf_key,endpos));

  /* merge pages and put parting key from anc_buff between */
  t_length=(int) _nisam_get_pack_key_length(keyinfo,nod_flag,
					    leaf_buff+p_length,
					    (leaf_length == nod_flag+2 ?
					     (uchar*) 0 : leaf_key),
					    anc_key,&s_temp);
  if (t_length >= 0)
    bmove((byte*) endpos+t_length,(byte*) leaf_buff+p_length,
	    (size_t) (leaf_length-p_length));
  else						/* We gained space */
    bmove((byte*) endpos,(byte*) leaf_buff+((int) p_length-t_length),
	    (size_t) (leaf_length-p_length+t_length));

  _nisam_store_key(keyinfo,endpos,&s_temp);
  buff_length=buff_length+leaf_length-p_length+t_length;
  putint(buff,buff_length,nod_flag);

  /* remove key from anc_buff */
  s_length=remove_key(keyinfo,share->base.key_reflength,keypos,anc_key,
		      anc_buff+anc_length);
  putint(anc_buff,(anc_length-=s_length),share->base.key_reflength);

  if (buff_length <= keyinfo->base.block_length)
  {						/* Keys in one page */
    if (_nisam_dispose(info,keyinfo,leaf_page))
      goto err;
  }
  else
  {						/* Page is full */
    VOID(_nisam_get_last_key(info,keyinfo,anc_buff,anc_key,keypos));
    endpos=half_pos=_nisam_find_half_pos(info,keyinfo,buff,leaf_key);

    /* Correct new keypointer to leaf_page */
    length=(*keyinfo->get_key)(keyinfo,nod_flag,&half_pos,leaf_key);
    _nisam_kpointer(info,leaf_key+length,leaf_page);
    /* Save key in anc_buff */
    DBUG_DUMP("anc_buff",(byte*) anc_buff,anc_length);
    DBUG_DUMP("key",(byte*) leaf_key,16);

    temp_pos=anc_buff+anc_length;
    t_length=(int) _nisam_get_pack_key_length(keyinfo,
					      share->base.key_reflength,
					      keypos == temp_pos ? (uchar*) 0
					      : keypos,
					      anc_key,leaf_key,&s_temp);
    if (t_length > 0)
      bmove_upp((byte*) temp_pos+t_length,(byte*) temp_pos,
		(uint) (temp_pos-keypos));
    else
      bmove(keypos,keypos-t_length,(uint) (temp_pos-keypos)+t_length);
    _nisam_store_key(keyinfo,keypos,&s_temp);
    putint(anc_buff,(anc_length+=t_length),share->base.key_reflength);

    /* Store new page */
    if (nod_flag)
      bmove((byte*) leaf_buff+2,(byte*) half_pos-nod_flag,(size_t) nod_flag);
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&half_pos,leaf_key));
    t_length=(int) _nisam_get_pack_key_length(keyinfo,nod_flag, (uchar*) 0,
					   (uchar*) 0, leaf_key, &s_temp);
    s_temp.n_length= *half_pos;		/* For _nisam_store_key */
    length=(uint) ((buff+buff_length)-half_pos);
    bmove((byte*) leaf_buff+p_length+t_length,(byte*) half_pos,
	    (size_t) length);
    _nisam_store_key(keyinfo,leaf_buff+p_length,&s_temp);
    putint(leaf_buff,length+t_length+p_length,nod_flag);
    putint(buff,endpos-buff,nod_flag);
    if (_nisam_write_keypage(info,keyinfo,leaf_page,leaf_buff))
	goto err;
  }
  if (_nisam_write_keypage(info,keyinfo,next_page,buff))
    goto err;
  DBUG_RETURN(anc_length <= (uint) keyinfo->base.block_length/2);
err:
  DBUG_RETURN(-1);
} /* underflow */


	/* remove a key from packed buffert */
	/* returns how many chars was removed */

static uint remove_key(N_KEYDEF *keyinfo, uint nod_flag,
		       uchar *keypos,	/* Where key starts */
		       uchar *lastkey,	/* key to be removed */
		       uchar *page_end)	/* End of page */
{
  int r_length,s_length,first,diff_flag;
  uchar *start;
  DBUG_ENTER("remove_key");
  DBUG_PRINT("enter",("keypos: %lx  page_end: %lx",keypos,page_end));

  start=keypos;
  if (!(keyinfo->base.flag & (HA_PACK_KEY | HA_SPACE_PACK_USED)))
    s_length=(int) (keyinfo->base.keylength+nod_flag);
  else
  {					 /* Let keypos point at next key */
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&keypos,lastkey));
    s_length=(keypos-start);
    if (keyinfo->base.flag & HA_PACK_KEY)
    {
      diff_flag= (keyinfo->seg[0].base.flag & HA_SPACE_PACK);
      first= *start;
      if (keypos != page_end && *keypos & 128 && first != 128)
      {					/* Referens length */
	if ((r_length= *keypos++ & 127) == 0)
	{				/* Same key after */
	  if (first & 128)
	    start++;			/* Skipp ref length */
	  if (diff_flag)
	    start+= *start+1;		/* Skipp key length */
	  else
	    start+=keyinfo->seg[0].base.length- (first & 127);
	  s_length=(keypos-start);	/* Remove pointers and next-key-flag */
	}
	else if (! (first & 128))
	{				/* Deleted key was not compressed */
	  if (diff_flag)
	  {
	    *start= (uchar) (r_length+ *keypos);
	    start+=r_length+1;		/* Let ref-part remain */
	    s_length=(keypos-start)+1;	/* Skipp everything between */
	  }
	  else
	  {
	    start+=r_length+1;		/* Let ref-part remain */
	    s_length=(keypos-start);	/* Skipp everything between */
	  }
	}
	else if ((first & 127) < r_length)
	{				/* mid-part of key is used */
	  r_length-=(first & 127);
	  start++;			/* Ref remains the same */
	  if (diff_flag)
	    *start++= (uchar) (*keypos++ + r_length);
	  start+= r_length;
	  s_length=(keypos-start);	/* Skipp everything between */
	}
      }
    }
  }
  bmove((byte*) start,(byte*) start+s_length,
	(uint) (page_end-start-s_length));
  DBUG_RETURN((uint) s_length);
} /* remove_key */
