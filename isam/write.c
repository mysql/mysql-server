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

/* Skriver ett record till en isam-databas */

#include "isamdef.h"
#ifdef	__WIN__
#include <errno.h>
#endif

	/* Functions declared in this file */

static int w_search(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,
		    ulong pos, uchar *father_buff, uchar *father_keypos,
		    ulong father_page);
static int _nisam_balance_page(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,
			    uchar *curr_buff,uchar *father_buff,
			    uchar *father_keypos,ulong father_page);


	/* Write new record to database */

int nisam_write(N_INFO *info, const byte *record)
{
  uint i;
  ulong filepos;
  uchar *buff;
  DBUG_ENTER("nisam_write");
  DBUG_PRINT("enter",("isam: %d  data: %d",info->s->kfile,info->dfile));

  if (info->s->base.options & HA_OPTION_READ_ONLY_DATA)
  {
    my_errno=EACCES;
    DBUG_RETURN(-1);
  }
#ifndef NO_LOCKING
  if (_nisam_readinfo(info,F_WRLCK,1)) DBUG_RETURN(-1);
#endif
  dont_break();				/* Dont allow SIGHUP or SIGINT */
#if !defined(NO_LOCKING) && defined(USE_RECORD_LOCK)
  if (!info->locked && my_lock(info->dfile,F_WRLCK,0L,F_TO_EOF,
			       MYF(MY_SEEK_NOT_DONE) | info->lock_wait))
    goto err;
#endif
  filepos= ((info->s->state.dellink != NI_POS_ERROR) ?
	    info->s->state.dellink :
	    info->s->state.data_file_length);

  if (info->s->base.reloc == 1L && info->s->base.records == 1L &&
      info->s->state.records == 1L)
  {						/* System file */
    my_errno=HA_ERR_RECORD_FILE_FULL;
    goto err2;
  }
  if (info->s->state.key_file_length >=
      info->s->base.max_key_file_length -
      info->s->blocksize* INDEX_BLOCK_MARGIN *info->s->state.keys)
  {
    my_errno=HA_ERR_INDEX_FILE_FULL;
    goto err2;
  }

	/* Write all keys to indextree */
  buff=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->state.keys ; i++)
  {
    VOID(_nisam_make_key(info,i,buff,record,filepos));
    if (_nisam_ck_write(info,i,buff)) goto err;
  }

  if ((*info->s->write_record)(info,record))
    goto err;

  info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED |HA_STATE_AKTIV |
		 HA_STATE_WRITTEN);
  info->s->state.records++;
  info->lastpos=filepos;
  nisam_log_record(LOG_WRITE,info,record,filepos,0);
  VOID(_nisam_writeinfo(info,1));
  allow_break();				/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  if (my_errno == HA_ERR_FOUND_DUPP_KEY || my_errno == HA_ERR_RECORD_FILE_FULL)
  {
    info->errkey= (int) i;
    while ( i-- > 0)
    {
      VOID(_nisam_make_key(info,i,buff,record,filepos));
      if (_nisam_ck_delete(info,i,buff))
	break;
    }
  }
  info->update=(HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | HA_STATE_WRITTEN);
err2:
  nisam_log_record(LOG_WRITE,info,record,filepos,my_errno);
  VOID(_nisam_writeinfo(info,1));
  allow_break();			/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(-1);
} /* nisam_write */


	/* Write one key to btree */

int _nisam_ck_write(register N_INFO *info, uint keynr, uchar *key)
{
  int error;
  DBUG_ENTER("_nisam_ck_write");

  if ((error=w_search(info,info->s->keyinfo+keynr,key,
		      info->s->state.key_root[keynr], (uchar *) 0, (uchar*) 0,
		      0L)) > 0)
    error=_nisam_enlarge_root(info,keynr,key);
  DBUG_RETURN(error);
} /* _nisam_ck_write */


	/* Make a new root with key as only pointer */

int _nisam_enlarge_root(register N_INFO *info, uint keynr, uchar *key)
{
  uint t_length,nod_flag;
  reg2 N_KEYDEF *keyinfo;
  S_PARAM s_temp;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("_nisam_enlarge_root");

  info->page_changed=1;
  nod_flag= (share->state.key_root[keynr] != NI_POS_ERROR) ?
    share->base.key_reflength : 0;
  _nisam_kpointer(info,info->buff+2,share->state.key_root[keynr]); /* if nod */
  keyinfo=share->keyinfo+keynr;
  t_length=_nisam_get_pack_key_length(keyinfo,nod_flag,(uchar*) 0,(uchar*) 0,
				   key,&s_temp);
  putint(info->buff,t_length+2+nod_flag,nod_flag);
  _nisam_store_key(keyinfo,info->buff+2+nod_flag,&s_temp);
  if ((share->state.key_root[keynr]= _nisam_new(info,keyinfo)) ==
      NI_POS_ERROR ||
      _nisam_write_keypage(info,keyinfo,share->state.key_root[keynr],info->buff))
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
} /* _nisam_enlarge_root */


	/* S|ker reda p} vart nyckeln skall s{ttas och placerar den dit */
	/* Returnerar -1 om fel ; 0 om ok.  1 om nyckel propagerar upp}t */

static int w_search(register N_INFO *info, register N_KEYDEF *keyinfo,
		    uchar *key, ulong page, uchar *father_buff,
		    uchar *father_keypos, ulong father_page)
{
  int error,flag;
  uint comp_flag,nod_flag;
  uchar *temp_buff,*keypos;
  uchar keybuff[N_MAX_KEY_BUFF];
  DBUG_ENTER("w_search");
  DBUG_PRINT("enter",("page: %ld",page));

  if (page == NI_POS_ERROR)
    DBUG_RETURN(1);				/* No key, make new */

  if (keyinfo->base.flag & HA_SORT_ALLOWS_SAME)
    comp_flag=SEARCH_BIGGER;			/* Put after same key */
  else if (keyinfo->base.flag & HA_NOSAME)
    comp_flag=SEARCH_FIND;			/* No dupplicates */
  else
    comp_flag=SEARCH_SAME;			/* Keys in rec-pos order */

  if (!(temp_buff= (uchar*) my_alloca((uint) keyinfo->base.block_length+
				      N_MAX_KEY_BUFF)))
    DBUG_RETURN(-1);
  if (!_nisam_fetch_keypage(info,keyinfo,page,temp_buff,0))
    goto err;

  flag=(*keyinfo->bin_search)(info,keyinfo,temp_buff,key,0,comp_flag,&keypos,
			      keybuff);
  nod_flag=test_if_nod(temp_buff);
  if (flag == 0)
  {
    my_errno=HA_ERR_FOUND_DUPP_KEY;
	/* get position to record with dupplicated key */
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&keypos,keybuff));
    info->dupp_key_pos=_nisam_dpos(info,test_if_nod(temp_buff),keypos);
    my_afree((byte*) temp_buff);
    DBUG_RETURN(-1);
  }
  if ((error=w_search(info,keyinfo,key,_nisam_kpos(nod_flag,keypos),
		      temp_buff,keypos,page)) >0)
  {
    error=_nisam_insert(info,keyinfo,key,temp_buff,keypos,keybuff,father_buff,
		     father_keypos,father_page);
    if (_nisam_write_keypage(info,keyinfo,page,temp_buff))
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

int _nisam_insert(register N_INFO *info, register N_KEYDEF *keyinfo,
	       uchar *key, uchar *anc_buff, uchar *key_pos, uchar *key_buff,
	       uchar *father_buff, uchar *father_key_pos, ulong father_page)
{
  uint a_length,t_length,nod_flag;
  uchar *endpos;
  int key_offset;
  S_PARAM s_temp;
  DBUG_ENTER("_nisam_insert");
  DBUG_PRINT("enter",("key_pos: %lx",key_pos));
  DBUG_EXECUTE("key",_nisam_print_key(DBUG_FILE,keyinfo->seg,key););

  nod_flag=test_if_nod(anc_buff);
  a_length=getint(anc_buff);
  endpos= anc_buff+ a_length;
  t_length=_nisam_get_pack_key_length(keyinfo,nod_flag,
				   (key_pos == endpos ? (uchar*) 0 : key_pos),
				   (key_pos == anc_buff+2+nod_flag ?
				    (uchar*) 0 : key_buff),key,&s_temp);
#ifndef DBUG_OFF
  if (key_pos != anc_buff+2+nod_flag)
    DBUG_DUMP("prev_key",(byte*) key_buff,_nisam_keylength(keyinfo,key_buff));
  if (keyinfo->base.flag & HA_PACK_KEY)
  {
    DBUG_PRINT("test",("t_length: %d  ref_len: %d",
		       t_length,s_temp.ref_length));
    DBUG_PRINT("test",("n_ref_len: %d  n_length: %d  key: %lx",
		       s_temp.n_ref_length,s_temp.n_length,s_temp.key));
  }
#endif
  key_offset = (uint)(endpos-key_pos);
  if((int) t_length < 0)
    key_offset += (int) t_length;
  if (key_offset < 0)
  {
    DBUG_PRINT("error",("Found a bug: negative key_offset %d\n", key_offset));
    DBUG_RETURN(-1);
  }
  if ((int) t_length >= 0)		/* t_length is almost always > 0 */
    bmove_upp((byte*) endpos+t_length,(byte*) endpos,(uint)key_offset );
  else
  {
    /* This may happen if a key was deleted and the next key could be
       compressed better than before */
    DBUG_DUMP("anc_buff",(byte*) anc_buff,a_length);
    
    bmove(key_pos,key_pos - (int) t_length,(uint)key_offset);
  }
  _nisam_store_key(keyinfo,key_pos,&s_temp);
  a_length+=t_length;
  putint(anc_buff,a_length,nod_flag);
  if (a_length <= keyinfo->base.block_length)
    DBUG_RETURN(0);				/* There is room on page */

  /* Page is full */

  if (!(keyinfo->base.flag & (HA_PACK_KEY | HA_SPACE_PACK_USED)) &&
      father_buff)
    DBUG_RETURN(_nisam_balance_page(info,keyinfo,key,anc_buff,father_buff,
				 father_key_pos,father_page));
  DBUG_RETURN(_nisam_splitt_page(info,keyinfo,key,anc_buff,key_buff));
} /* _nisam_insert */


	/* splitt a full page in two and assign emerging item to key */

int _nisam_splitt_page(register N_INFO *info, register N_KEYDEF *keyinfo,
		    uchar *key, uchar *buff, uchar *key_buff)
{
  uint length,a_length,key_ref_length,t_length,nod_flag;
  uchar *key_pos,*pos;
  ulong new_pos;
  S_PARAM s_temp;
  DBUG_ENTER("ni_splitt_page");
  DBUG_DUMP("buff",(byte*) buff,getint(buff));

  nod_flag=test_if_nod(buff);
  key_ref_length=2+nod_flag;
  key_pos=_nisam_find_half_pos(info,keyinfo,buff,key_buff);
  length=(uint) (key_pos-buff);
  a_length=getint(buff);
  putint(buff,length,nod_flag);
  info->page_changed=1;

	/* Correct new page pointer */
  VOID((*keyinfo->get_key)(keyinfo,nod_flag,&key_pos,key_buff));
  if (nod_flag)
  {
    DBUG_PRINT("test",("Splitting nod"));
    pos=key_pos-nod_flag;
    memcpy((byte*) info->buff+2,(byte*) pos,(size_t) nod_flag);
  }

	/* Move midle item to key and pointer to new page */
  if ((new_pos=_nisam_new(info,keyinfo)) == NI_POS_ERROR)
    DBUG_RETURN(-1);
  _nisam_kpointer(info,_nisam_move_key(keyinfo,key,key_buff),new_pos);

	/* Store new page */
  VOID((*keyinfo->get_key)(keyinfo,nod_flag,&key_pos,key_buff));
  t_length=_nisam_get_pack_key_length(keyinfo,nod_flag,(uchar *) 0, (uchar*) 0,
				   key_buff, &s_temp);
  s_temp.n_length= *key_pos;			/* Needed by ni_store_key */
  length=(uint) ((buff+a_length)-key_pos);
  memcpy((byte*) info->buff+key_ref_length+t_length,(byte*) key_pos,
	 (size_t) length);
  _nisam_store_key(keyinfo,info->buff+key_ref_length,&s_temp);
  putint(info->buff,length+t_length+key_ref_length,nod_flag);

  if (_nisam_write_keypage(info,keyinfo,new_pos,info->buff))
    DBUG_RETURN(-1);
  DBUG_DUMP("key",(byte*) key,_nisam_keylength(keyinfo,key));
  DBUG_RETURN(2);				/* Middle key up */
} /* _nisam_splitt_page */


	/* find out how much more room a key will take */

#ifdef QQ
uint _nisam_get_pack_key_length(N_KEYDEF *keyinfo, uint nod_flag, uchar *key_pos, uchar *key_buff, uchar *key, S_PARAM *s_temp)

					/* If nod: Length of nod-pointer */
					/* Position to pos after key in buff */
					/* Last key before current key */
					/* Current key */
					/* How next key will be packed */
{
  reg1 N_KEYSEG *keyseg;
  int length;
  uint key_length,ref_length,n_length,diff_flag,same_length;
  uchar *start,*end,*key_end;

  s_temp->key=key;
  if (!(keyinfo->base.flag & HA_PACK_KEY))
    return (s_temp->totlength=_nisam_keylength(keyinfo,key)+nod_flag);
  s_temp->ref_length=s_temp->n_ref_length=s_temp->n_length=0;
  s_temp->prev_length=0;

  same_length=0; keyseg=keyinfo->seg;
  key_length=_nisam_keylength(keyinfo,key)+nod_flag;

  if (keyseg->base.flag & HA_SPACE_PACK)
  {
    diff_flag=1;
    end=key_end= key+ *key+1;
    if (key_buff)
    {
      if (*key == *key_buff && *key)
	same_length=1;			/* Don't use key-pack if length == 0 */
      else if (*key > *key_buff)
	end=key+ *key_buff+1;
      key_buff++;
    }
    key++;
  }
  else
  {
    diff_flag=0;
    key_end=end= key+keyseg->base.length;
  }

  start=key;
  if (key_buff)
    while (key < end && *key == *key_buff)
    {
      key++; key_buff++;
    }

  s_temp->key=key; s_temp->key_length= (uint) (key_end-key);

  if (same_length && key == key_end)
  {
    s_temp->ref_length=128;
    length=(int) key_length-(int)(key_end-start); /* Same as prev key */
    if (key_pos)
    {
      s_temp->n_length= *key_pos;
      key_pos=0;				/* Can't combine with next */
    }
  }
  else
  {
    if (start != key)
    {						/* Starts as prev key */
      s_temp->ref_length= (uint) (key-start)+128;
      length=(int) (1+key_length-(uint) (key-start));
    }
    else
      length=(int) (key_length+ (1-diff_flag));		/* Not packed key */
  }
  s_temp->totlength=(uint) length;

  DBUG_PRINT("test",("tot_length: %d  length: %d  uniq_key_length: %d",
		     key_length,length,s_temp->key_length));

	/* If something after that is not 0 length test if we can combine */

  if (key_pos && (n_length= *key_pos))
  {
    key_pos++;
    ref_length=0;
    if (n_length & 128)
    {
      if ((ref_length=n_length & 127))
	if (diff_flag)
	  n_length= *key_pos++;			/* Length of key-part */
	else
	  n_length=keyseg->base.length - ref_length;
    }
    else
      if (*start == *key_pos && diff_flag && start != key_end)
	length++;				/* One new pos for ref.len */

    DBUG_PRINT("test",("length: %d  key_pos: %lx",length,key_pos));
    if (n_length != 128)
    {						/* Not same key after */
      key=start+ref_length;
      while (n_length > 0 && key < key_end && *key == *key_pos)
      {
	key++; key_pos++;
	ref_length++;
	n_length--;
	length--;				/* We gained one char */
      }

      if (n_length == 0 && diff_flag)
      {
	n_length=128;				/* Same as prev key */
	length--;				/* We don't need key-length */
      }
      else if (ref_length)
	s_temp->n_ref_length=ref_length | 128;
    }
    s_temp->n_length=n_length;
  }
  return (uint) length;
} /* _nisam_get_pack_key_length */

#else

uint
_nisam_get_pack_key_length(N_KEYDEF *keyinfo,
			uint nod_flag,  /* If nod: Length of nod-pointer */
			uchar *key_pos, /* Position to pos after key in buff */
			uchar *key_buff,/* Last key before current key */
			uchar *key,	/* Current key */
			S_PARAM *s_temp/* How next key will be packed */
			)
{
  reg1 N_KEYSEG *keyseg;
  int length;
  uint key_length,ref_length,n_length,diff_flag,same_length,org_key_length=0;
  uchar *start,*end,*key_end;

  s_temp->key=key;
  if (!(keyinfo->base.flag & HA_PACK_KEY))
    return (s_temp->totlength=_nisam_keylength(keyinfo,key)+nod_flag);
  s_temp->ref_length=s_temp->n_ref_length=s_temp->n_length=0;

  same_length=0; keyseg=keyinfo->seg;
  key_length=_nisam_keylength(keyinfo,key)+nod_flag;
  s_temp->prev_key=key_buff;

  if (keyseg->base.flag & HA_SPACE_PACK)
  {
    diff_flag=1;
    end=key_end= key+ *key+1;
    if (key_buff)
    {
      org_key_length= (uint) *key_buff;
      if (*key == *key_buff && *key)
	same_length=1;			/* Don't use key-pack if length == 0 */
      else if (*key > *key_buff)
	end=key+ org_key_length+1;
      key_buff++;
    }
    key++;
  }
  else
  {
    diff_flag=0;
    key_end=end= key+(org_key_length=keyseg->base.length);
  }

  start=key;
  if (key_buff)
    while (key < end && *key == *key_buff)
    {
      key++; key_buff++;
    }

  s_temp->key=key; s_temp->key_length= (uint) (key_end-key);

  if (same_length && key == key_end)
  {
    s_temp->ref_length=128;
    length=(int) key_length-(int)(key_end-start); /* Same as prev key */
    if (key_pos)
    {						/* Can't combine with next */
      s_temp->n_length= *key_pos;		/* Needed by _nisam_store_key */
      key_pos=0;
    }
  }
  else
  {
    if (start != key)
    {						/* Starts as prev key */
      s_temp->ref_length= (uint) (key-start)+128;
      length=(int) (1+key_length-(uint) (key-start));
    }
    else
      length=(int) (key_length+ (1-diff_flag));		/* Not packed key */
  }
  s_temp->totlength=(uint) length;
  s_temp->prev_length=0;
  DBUG_PRINT("test",("tot_length: %d  length: %d  uniq_key_length: %d",
		     key_length,length,s_temp->key_length));

	/* If something after that is not 0 length test if we can combine */

  if (key_pos && (n_length= *key_pos++))
  {
    if (n_length == 128)
    {
      /*
	We put a different key between two identical keys
	Extend next key to have same prefix as this key
      */
      if (s_temp->ref_length)
      {					/* make next key longer */
	s_temp->part_of_prev_key= s_temp->ref_length;
	s_temp->prev_length=      org_key_length - (s_temp->ref_length-128);
	s_temp->n_length= 	  s_temp->prev_length;
	s_temp->prev_key+=        diff_flag + (s_temp->ref_length - 128);
	length+=		  s_temp->prev_length+diff_flag;
      }
      else
      {						/* Can't use prev key */
	s_temp->part_of_prev_key=0;
	s_temp->prev_length= org_key_length;
	s_temp->n_length=    org_key_length;
	s_temp->prev_key+=   diff_flag;		/* To start of key */
	length+=	     org_key_length;
      }
      return (uint) length;
    }

    if (n_length & 128)
    {
      ref_length=n_length & 127;
      if (diff_flag)				/* If SPACE_PACK */
	n_length= *key_pos++;			/* Length of key-part */
      else
	n_length=keyseg->base.length - ref_length;

      /* Test if new keys has fewer characters that match the previous key */
      if (!s_temp->ref_length)
      {						/* Can't use prev key */
	s_temp->part_of_prev_key=	0;
	s_temp->prev_length= 		ref_length;
	s_temp->n_length=		n_length+ref_length;
	s_temp->prev_key+=		diff_flag;	/* To start of key */
	return (uint) length+ref_length-diff_flag;
      }
      if (ref_length+128 > s_temp->ref_length)
      {
	/* We must copy characters from the original key to the next key */
	s_temp->part_of_prev_key= s_temp->ref_length;
	s_temp->prev_length=	  ref_length+128 - s_temp->ref_length;
	s_temp->n_length=	  n_length + s_temp->prev_length;
	s_temp->prev_key+= 	  diff_flag + s_temp->ref_length -128;
	return (uint) length + s_temp->prev_length;
      }
    }
    else
    {
      ref_length=0;
      if (*start == *key_pos && diff_flag && start != key_end)
	length++;				/* One new pos for ref.len */
    }
    DBUG_PRINT("test",("length: %d  key_pos: %lx",length,key_pos));

    key=start+ref_length;
    while (n_length > 0 && key < key_end && *key == *key_pos)
    {
      key++; key_pos++;
      ref_length++;
      n_length--;
      length--;				/* We gained one char */
    }

    if (n_length == 0 && diff_flag)
    {
      n_length=128;				/* Same as prev key */
      length--;				/* We don't need key-length */
    }
    else if (ref_length)
      s_temp->n_ref_length=ref_length | 128;
    s_temp->n_length=n_length;
  }
  return (uint) length;
} /* _nisam_get_pack_key_length */
#endif


	/* store a key in page-buffert */

void _nisam_store_key(N_KEYDEF *keyinfo, register uchar *key_pos,
		   register S_PARAM *s_temp)
{
  uint length;
  uchar *start;

  if (! (keyinfo->base.flag & HA_PACK_KEY))
  {
    memcpy((byte*) key_pos,(byte*) s_temp->key,(size_t) s_temp->totlength);
    return;
  }
  start=key_pos;
  if ((*key_pos=(uchar) s_temp->ref_length))
    key_pos++;
  if (s_temp->ref_length == 0 ||
      (s_temp->ref_length > 128 &&
       (keyinfo->seg[0].base.flag & HA_SPACE_PACK)))
    *key_pos++= (uchar) s_temp->key_length;
  bmove((byte*) key_pos,(byte*) s_temp->key,
	(length=s_temp->totlength-(uint) (key_pos-start)));
  key_pos+=length;

  if (s_temp->prev_length)
  {
    /* Extend next key because new key didn't have same prefix as prev key */
    if (s_temp->part_of_prev_key)
      *key_pos++ = s_temp->part_of_prev_key;
    if (keyinfo->seg[0].base.flag & HA_SPACE_PACK)
      *key_pos++= s_temp->n_length;
    memcpy(key_pos, s_temp->prev_key, s_temp->prev_length);
    return;
  }

  if ((*key_pos = (uchar) s_temp->n_ref_length))
  {
    if (! (keyinfo->seg[0].base.flag & HA_SPACE_PACK))
      return;					/* Don't save keylength */
    key_pos++;					/* Store ref for next key */
  }
  *key_pos = (uchar) s_temp->n_length;
  return;
} /* _nisam_store_key */


	/* Calculate how to much to move to split a page in two */
	/* Returns pointer and key for get_key() to get mid key */
	/* There is at last 2 keys after pointer in buff */

uchar *_nisam_find_half_pos(N_INFO *info, N_KEYDEF *keyinfo, uchar *page, uchar *key)
{
  uint keys,length,key_ref_length,nod_flag;
  uchar *end,*lastpos;
  DBUG_ENTER("_nisam_find_half_pos");

  nod_flag=test_if_nod(page);
  key_ref_length=2+nod_flag;
  length=getint(page)-key_ref_length;
  page+=key_ref_length;
  if (!(keyinfo->base.flag & (HA_PACK_KEY | HA_SPACE_PACK_USED)))
  {
    keys=(length/(keyinfo->base.keylength+nod_flag))/2;
    DBUG_RETURN(page+keys*(keyinfo->base.keylength+nod_flag));
  }

  end=page+length/2-key_ref_length;		/* This is aprox. half */
  *key='\0';
  do
  {
    lastpos=page;
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&page,key));
   } while (page < end);

   DBUG_PRINT("exit",("returns: %lx  page: %lx  half: %lx",lastpos,page,end));
   DBUG_RETURN(lastpos);
} /* _nisam_find_half_pos */


	/* Balance page with not packed keys with page on right/left */
	/* returns 0 if balance was done */

static int _nisam_balance_page(register N_INFO *info, N_KEYDEF *keyinfo,
			    uchar *key, uchar *curr_buff, uchar *father_buff,
			    uchar *father_key_pos, ulong father_page)
{
  my_bool right;
  uint k_length,father_length,father_keylength,nod_flag,curr_keylength,
       right_length,left_length,new_right_length,new_left_length,extra_length,
       length,keys;
  uchar *pos,*buff,*extra_buff;
  ulong next_page,new_pos;
  byte tmp_part_key[N_MAX_KEY_BUFF];
  DBUG_ENTER("_nisam_balance_page");

  k_length=keyinfo->base.keylength;
  father_length=getint(father_buff);
  father_keylength=k_length+info->s->base.key_reflength;
  nod_flag=test_if_nod(curr_buff);
  curr_keylength=k_length+nod_flag;
  info->page_changed=1;

  if ((father_key_pos != father_buff+father_length && (info->s->rnd++ & 1)) ||
      father_key_pos == father_buff+2+info->s->base.key_reflength)
  {
    right=1;
    next_page= _nisam_kpos(info->s->base.key_reflength,
			father_key_pos+father_keylength);
    buff=info->buff;
    DBUG_PRINT("test",("use right page: %lu",next_page));
  }
  else
  {
    right=0;
    father_key_pos-=father_keylength;
    next_page= _nisam_kpos(info->s->base.key_reflength,father_key_pos);
					/* Fix that curr_buff is to left */
    buff=curr_buff; curr_buff=info->buff;
    DBUG_PRINT("test",("use left page: %lu",next_page));
  }					/* father_key_pos ptr to parting key */

  if (!_nisam_fetch_keypage(info,keyinfo,next_page,info->buff,0))
    goto err;
  DBUG_DUMP("next",(byte*) info->buff,getint(info->buff));

	/* Test if there is room to share keys */

  left_length=getint(curr_buff);
  right_length=getint(buff);
  keys=(left_length+right_length-4-nod_flag*2)/curr_keylength;

  if ((right ? right_length : left_length) + curr_keylength <=
      keyinfo->base.block_length)
  {						/* Merge buffs */
    new_left_length=2+nod_flag+(keys/2)*curr_keylength;
    new_right_length=2+nod_flag+((keys+1)/2)*curr_keylength;
    putint(curr_buff,new_left_length,nod_flag);
    putint(buff,new_right_length,nod_flag);

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

    if (_nisam_write_keypage(info,keyinfo,next_page,info->buff) ||
	_nisam_write_keypage(info,keyinfo,father_page,father_buff))
      goto err;
    DBUG_RETURN(0);
  }

	/* curr_buff[] and buff[] are full, lets splitt and make new nod */

  extra_buff=info->buff+info->s->base.max_block;
  new_left_length=new_right_length=2+nod_flag+(keys+1)/3*curr_keylength;
  if (keys == 5)				/* Too few keys to balance */
    new_left_length-=curr_keylength;
  extra_length=nod_flag+left_length+right_length-new_left_length-new_right_length-curr_keylength;
  DBUG_PRINT("info",("left_length: %d  right_length: %d  new_left_length: %d  new_right_length: %d  extra_length: %d",
		     left_length, right_length,
		     new_left_length, new_right_length,
		     extra_length));
  putint(curr_buff,new_left_length,nod_flag);
  putint(buff,new_right_length,nod_flag);
  putint(extra_buff,extra_length+2,nod_flag);

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

  /* Move new parting keys up */
  memcpy((byte*) (right ? key : father_key_pos),pos, (size_t) k_length);
  memcpy((byte*) (right ? father_key_pos : key), tmp_part_key, k_length);

  if ((new_pos=_nisam_new(info,keyinfo)) == NI_POS_ERROR)
    goto err;
  _nisam_kpointer(info,key+k_length,new_pos);
  if (_nisam_write_keypage(info,keyinfo,(right ? new_pos : next_page),
			info->buff) ||
      _nisam_write_keypage(info,keyinfo,(right ? next_page : new_pos),extra_buff))
    goto err;

  DBUG_RETURN(1);				/* Middle key up */

err:
  DBUG_RETURN(-1);
} /* _nisam_balance_page */
