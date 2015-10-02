/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Remove a row from a MyISAM table */

#include "fulltext.h"
#include "rt_index.h"

static int d_search(MI_INFO *info,MI_KEYDEF *keyinfo,uint comp_flag,
                    uchar *key,uint key_length,my_off_t page,uchar *anc_buff);
static int del(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,uchar *anc_buff,
	       my_off_t leaf_page,uchar *leaf_buff,uchar *keypos,
	       my_off_t next_block,uchar *ret_key);
static int underflow(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *anc_buff,
		     my_off_t leaf_page,uchar *leaf_buff,uchar *keypos);
static uint remove_key(MI_KEYDEF *keyinfo,uint nod_flag,uchar *keypos,
		       uchar *lastkey,uchar *page_end,
		       my_off_t *next_block);
static int _mi_ck_real_delete(MI_INFO *info,MI_KEYDEF *keyinfo,
			      uchar *key, uint key_length, my_off_t *root);


int mi_delete(MI_INFO *info,const uchar *record)
{
  uint i;
  uchar *old_key;
  int save_errno;
  char lastpos[8];

  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_delete");

	/* Test if record is in datafile */

  DBUG_EXECUTE_IF("myisam_pretend_crashed_table_on_usage",
                  mi_print_error(info->s, HA_ERR_CRASHED);
                  set_my_errno(HA_ERR_CRASHED);
                  DBUG_RETURN(HA_ERR_CRASHED););
  DBUG_EXECUTE_IF("my_error_test_undefined_error",
                  mi_print_error(info->s, INT_MAX);
                  set_my_errno(INT_MAX);
                  DBUG_RETURN(INT_MAX););
  if (!(info->update & HA_STATE_AKTIV))
  {
    set_my_errno(HA_ERR_KEY_NOT_FOUND);
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);	/* No database read */
  }
  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    set_my_errno(EACCES);
    DBUG_RETURN(EACCES);
  }
  if (_mi_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno());
  if (info->s->calc_checksum)
    info->checksum=(*info->s->calc_checksum)(info,record);
  if ((*share->compare_record)(info,record))
    goto err;				/* Error on read-check */

  if (_mi_mark_file_changed(info))
    goto err;

	/* Remove all keys from the .ISAM file */

  old_key=info->lastkey2;
  for (i=0 ; i < share->base.keys ; i++ )
  {
    if (mi_is_key_active(info->s->state.key_map, i))
    {
      info->s->keyinfo[i].version++;
      if (info->s->keyinfo[i].flag & HA_FULLTEXT )
      {
        if (_mi_ft_del(info,i, old_key,record,info->lastpos))
          goto err;
      }
      else
      {
        if (info->s->keyinfo[i].ck_delete(info,i,old_key,
                _mi_make_key(info,i,old_key,record,info->lastpos)))
          goto err;
      }
    }
  }

  if ((*share->delete_record)(info))
    goto err;				/* Remove record from database */
  info->state->checksum-=info->checksum;

  info->update= HA_STATE_CHANGED+HA_STATE_DELETED+HA_STATE_ROW_CHANGED;
  info->state->records--;

  mi_sizestore(lastpos,info->lastpos);
  myisam_log_command(MI_LOG_DELETE,info,(uchar*) lastpos,sizeof(lastpos),0);
  (void) _mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);

  if (info->invalidator != 0)
  {
    DBUG_PRINT("info", ("invalidator... '%s' (delete)", info->filename));
    (*info->invalidator)(info->filename);
    info->invalidator=0;
  }
  DBUG_RETURN(0);

err:
  save_errno=my_errno();
  mi_sizestore(lastpos,info->lastpos);
  myisam_log_command(MI_LOG_DELETE,info,(uchar*) lastpos, sizeof(lastpos),0);
  if (save_errno != HA_ERR_RECORD_CHANGED)
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    mi_mark_crashed(info);		/* mark table crashed */
  }
  (void) _mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);
  info->update|=HA_STATE_WRITTEN;	/* Buffer changed */
  set_my_errno(save_errno);
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    set_my_errno(HA_ERR_CRASHED);
  }

  DBUG_RETURN(my_errno());
} /* mi_delete */


	/* Remove a key from the btree index */

int _mi_ck_delete(MI_INFO *info, uint keynr, uchar *key,
		  uint key_length)
{
  return _mi_ck_real_delete(info, info->s->keyinfo+keynr, key, key_length,
                            &info->s->state.key_root[keynr]);
} /* _mi_ck_delete */


static int _mi_ck_real_delete(MI_INFO *info, MI_KEYDEF *keyinfo,
			      uchar *key, uint key_length, my_off_t *root)
{
  int error;
  uint nod_flag;
  my_off_t old_root;
  uchar *root_buff;
  DBUG_ENTER("_mi_ck_real_delete");

  if ((old_root=*root) == HA_OFFSET_ERROR)
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    set_my_errno(HA_ERR_CRASHED);
    DBUG_RETURN(HA_ERR_CRASHED);
  }
  if (!(root_buff= (uchar*) my_alloca((uint) keyinfo->block_length+
				      MI_MAX_KEY_BUFF*2)))
  {
    DBUG_PRINT("error",("Couldn't allocate memory"));
    set_my_errno(ENOMEM);
    DBUG_RETURN(ENOMEM);
  }
  DBUG_PRINT("info",("root_page: %ld", (long) old_root));
  if (!_mi_fetch_keypage(info,keyinfo,old_root,DFLT_INIT_HITS,root_buff,0))
  {
    error= -1;
    goto err;
  }
  if ((error=d_search(info,keyinfo,
                      (keyinfo->flag & HA_FULLTEXT ? SEARCH_FIND | SEARCH_UPDATE
                                                   : SEARCH_SAME),
                       key,key_length,old_root,root_buff)) >0)
  {
    if (error == 2)
    {
      DBUG_PRINT("test",("Enlarging of root when deleting"));
      error=_mi_enlarge_root(info,keyinfo,key,root);
    }
    else /* error == 1 */
    {
      if (mi_getint(root_buff) <= (nod_flag=mi_test_if_nod(root_buff))+3)
      {
	error=0;
	if (nod_flag)
	  *root=_mi_kpos(nod_flag,root_buff+2+nod_flag);
	else
	  *root=HA_OFFSET_ERROR;
	if (_mi_dispose(info,keyinfo,old_root,DFLT_INIT_HITS))
	  error= -1;
      }
      else
	error=_mi_write_keypage(info,keyinfo,old_root,
                                DFLT_INIT_HITS,root_buff);
    }
  }
err:
  DBUG_PRINT("exit",("Return: %d",error));
  DBUG_RETURN(error);
} /* _mi_ck_real_delete */


	/*
	** Remove key below key root
	** Return values:
	** 1 if there are less buffers;  In this case anc_buff is not saved
	** 2 if there are more buffers
	** -1 on errors
	*/

static int d_search(MI_INFO *info, MI_KEYDEF *keyinfo,
                    uint comp_flag, uchar *key, uint key_length,
                    my_off_t page, uchar *anc_buff)
{
  int flag,ret_value,save_flag;
  uint length,nod_flag,search_key_length;
  my_bool last_key;
  uchar *leaf_buff,*keypos;
  my_off_t leaf_page= 0, next_block;
  uchar lastkey[MI_MAX_KEY_BUFF];
  DBUG_ENTER("d_search");
  DBUG_DUMP("page",(uchar*) anc_buff,mi_getint(anc_buff));

  search_key_length= (comp_flag & SEARCH_FIND) ? key_length : USE_WHOLE_KEY;
  flag=(*keyinfo->bin_search)(info,keyinfo,anc_buff,key, search_key_length,
                              comp_flag, &keypos, lastkey, &last_key);
  if (flag == MI_FOUND_WRONG_KEY)
  {
    DBUG_PRINT("error",("Found wrong key"));
    DBUG_RETURN(-1);
  }
  nod_flag=mi_test_if_nod(anc_buff);

  if (!flag && keyinfo->flag & HA_FULLTEXT)
  {
    uint off;
    int  subkeys;

    get_key_full_length_rdonly(off, lastkey);
    subkeys=ft_sintXkorr(lastkey+off);
    DBUG_ASSERT(info->ft1_to_ft2==0 || subkeys >=0);
    comp_flag=SEARCH_SAME;
    if (subkeys >= 0)
    {
      /* normal word, one-level tree structure */
      if (info->ft1_to_ft2)
      {
        /* we're in ft1->ft2 conversion mode. Saving key data */
        if (insert_dynamic(info->ft1_to_ft2, (lastkey+off)))
        {
          DBUG_PRINT("error",("Out of memory"));
          DBUG_RETURN(-1);
        }
      }
      else
      {
        /* we need exact match only if not in ft1->ft2 conversion mode */
        flag=(*keyinfo->bin_search)(info,keyinfo,anc_buff,key,USE_WHOLE_KEY,
                                    comp_flag, &keypos, lastkey, &last_key);
      }
      /* fall through to normal delete */
    }
    else
    {
      /* popular word. two-level tree. going down */
      uint tmp_key_length;
      my_off_t root;
      uchar *kpos=keypos;

      if (!(tmp_key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&kpos,lastkey)))
      {
        mi_print_error(info->s, HA_ERR_CRASHED);
        set_my_errno(HA_ERR_CRASHED);
        DBUG_RETURN(-1);
      }
      root=_mi_dpos(info,nod_flag,kpos);
      if (subkeys == -1)
      {
        /* the last entry in sub-tree */
        if (_mi_dispose(info, keyinfo, root,DFLT_INIT_HITS))
          DBUG_RETURN(-1);
        /* fall through to normal delete */
      }
      else
      {
        keyinfo=&info->s->ft2_keyinfo;
        kpos-=keyinfo->keylength+nod_flag; /* we'll modify key entry 'in vivo' */
        get_key_full_length_rdonly(off, key);
        key+=off;
        ret_value=_mi_ck_real_delete(info, &info->s->ft2_keyinfo,
            key, HA_FT_WLEN, &root);
        _mi_dpointer(info, kpos+HA_FT_WLEN, root);
        subkeys++;
        ft_intXstore(kpos, subkeys);
        if (!ret_value)
          ret_value=_mi_write_keypage(info,keyinfo,page,
                                      DFLT_INIT_HITS,anc_buff);
        DBUG_PRINT("exit",("Return: %d",ret_value));
        DBUG_RETURN(ret_value);
      }
    }
  }
  leaf_buff=0;
  leaf_page= 0;
  if (nod_flag)
  {
    leaf_page=_mi_kpos(nod_flag,keypos);
    if (!(leaf_buff= (uchar*) my_alloca((uint) keyinfo->block_length+
					MI_MAX_KEY_BUFF*2)))
    {
      DBUG_PRINT("error",("Couldn't allocate memory"));
      set_my_errno(ENOMEM);
      DBUG_PRINT("exit",("Return: %d",-1));
      DBUG_RETURN(-1);
    }
    if (!_mi_fetch_keypage(info,keyinfo,leaf_page,DFLT_INIT_HITS,leaf_buff,0))
      goto err;
  }

  if (flag != 0)
  {
    if (!nod_flag)
    {
      DBUG_PRINT("error",("Didn't find key"));
      mi_print_error(info->s, HA_ERR_CRASHED);
      set_my_errno(HA_ERR_CRASHED);		/* This should newer happend */
      goto err;
    }
    save_flag=0;
    ret_value=d_search(info,keyinfo,comp_flag,key,key_length,
                       leaf_page,leaf_buff);
  }
  else
  {						/* Found key */
    uint tmp;
    length=mi_getint(anc_buff);
    if (!(tmp= remove_key(keyinfo,nod_flag,keypos,lastkey,anc_buff+length,
                          &next_block)))
      goto err;

    length-= tmp;

    mi_putint(anc_buff,length,nod_flag);
    if (!nod_flag)
    {						/* On leaf page */
      if (_mi_write_keypage(info,keyinfo,page,DFLT_INIT_HITS,anc_buff))
      {
        DBUG_PRINT("exit",("Return: %d",-1));
	DBUG_RETURN(-1);
      }
      /* Page will be update later if we return 1 */
      DBUG_RETURN(MY_TEST(length <= (info->quick_mode ? MI_MIN_KEYBLOCK_LENGTH :
                                     (uint) keyinfo->underflow_block_length)));
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
      if (!_mi_get_last_key(info,keyinfo,anc_buff,lastkey,keypos,&length))
      {
	goto err;
      }
      ret_value=_mi_insert(info,keyinfo,key,anc_buff,keypos,lastkey,
			   (uchar*) 0,(uchar*) 0,(my_off_t) 0,(my_bool) 0);
    }
  }
  if (ret_value == 0 && mi_getint(anc_buff) > keyinfo->block_length)
  {
    save_flag=1;
    ret_value=_mi_split_page(info,keyinfo,key,anc_buff,lastkey,0) | 2;
  }
  if (save_flag && ret_value != 1)
    ret_value|=_mi_write_keypage(info,keyinfo,page,DFLT_INIT_HITS,anc_buff);
  else
  {
    DBUG_DUMP("page",(uchar*) anc_buff,mi_getint(anc_buff));
  }
  DBUG_PRINT("exit",("Return: %d",ret_value));
  DBUG_RETURN(ret_value);

err:
  DBUG_PRINT("exit",("Error: %d",my_errno()));
  DBUG_RETURN (-1);
} /* d_search */


	/* Remove a key that has a page-reference */

static int del(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
	       uchar *anc_buff, my_off_t leaf_page, uchar *leaf_buff,
	       uchar *keypos,		/* Pos to where deleted key was */
	       my_off_t next_block,
	       uchar *ret_key)		/* key before keypos in anc_buff */
{
  int ret_value,length;
  uint a_length,nod_flag,tmp;
  my_off_t next_page;
  uchar keybuff[MI_MAX_KEY_BUFF],*endpos,*next_buff,*key_start, *prev_key;
  MYISAM_SHARE *share=info->s;
  MI_KEY_PARAM s_temp;
  DBUG_ENTER("del");
  DBUG_PRINT("enter",("leaf_page: %ld  keypos: 0x%lx", (long) leaf_page,
		      (ulong) keypos));
  DBUG_DUMP("leaf_buff",(uchar*) leaf_buff,mi_getint(leaf_buff));

  endpos=leaf_buff+mi_getint(leaf_buff);
  if (!(key_start=_mi_get_last_key(info,keyinfo,leaf_buff,keybuff,endpos,
				   &tmp)))
    DBUG_RETURN(-1);

  if ((nod_flag=mi_test_if_nod(leaf_buff)))
  {
    next_page= _mi_kpos(nod_flag,endpos);
    if (!(next_buff= (uchar*) my_alloca((uint) keyinfo->block_length+
					MI_MAX_KEY_BUFF*2)))
      DBUG_RETURN(-1);
    if (!_mi_fetch_keypage(info,keyinfo,next_page,DFLT_INIT_HITS,next_buff,0))
      ret_value= -1;
    else
    {
      DBUG_DUMP("next_page",(uchar*) next_buff,mi_getint(next_buff));
      if ((ret_value=del(info,keyinfo,key,anc_buff,next_page,next_buff,
			 keypos,next_block,ret_key)) >0)
      {
	endpos=leaf_buff+mi_getint(leaf_buff);
	if (ret_value == 1)
	{
	  ret_value=underflow(info,keyinfo,leaf_buff,next_page,
			      next_buff,endpos);
	  if (ret_value == 0 && mi_getint(leaf_buff) > keyinfo->block_length)
	  {
	    ret_value=_mi_split_page(info,keyinfo,key,leaf_buff,ret_key,0) | 2;
	  }
	}
	else
	{
	  DBUG_PRINT("test",("Inserting of key when deleting"));
	  if (!_mi_get_last_key(info,keyinfo,leaf_buff,keybuff,endpos,
				&tmp))
	    goto err;
	  ret_value=_mi_insert(info,keyinfo,key,leaf_buff,endpos,keybuff,
			       (uchar*) 0,(uchar*) 0,(my_off_t) 0,0);
	}
      }
      if (_mi_write_keypage(info,keyinfo,leaf_page,DFLT_INIT_HITS,leaf_buff))
	goto err;
    }
    DBUG_RETURN(ret_value);
  }

	/* Remove last key from leaf page */

  mi_putint(leaf_buff,key_start-leaf_buff,nod_flag);
  if (_mi_write_keypage(info,keyinfo,leaf_page,DFLT_INIT_HITS,leaf_buff))
    goto err;

	/* Place last key in ancestor page on deleted key position */

  a_length=mi_getint(anc_buff);
  endpos=anc_buff+a_length;
  if (keypos != anc_buff+2+share->base.key_reflength &&
      !_mi_get_last_key(info,keyinfo,anc_buff,ret_key,keypos,&tmp))
    goto err;
  prev_key=(keypos == anc_buff+2+share->base.key_reflength ?
	    0 : ret_key);
  length=(*keyinfo->pack_key)(keyinfo,share->base.key_reflength,
			      keypos == endpos ? (uchar*) 0 : keypos,
			      prev_key, prev_key,
			      keybuff,&s_temp);
  if (length > 0)
    memmove(keypos + length, keypos, (size_t) (endpos - keypos));
  else
    memmove(keypos, keypos - length, (int) (endpos - keypos) + length);
  (*keyinfo->store_key)(keyinfo,keypos,&s_temp);
  /* Save pointer to next leaf */
  if (!(*keyinfo->get_key)(keyinfo,share->base.key_reflength,&keypos,ret_key))
    goto err;
  _mi_kpointer(info,keypos - share->base.key_reflength,next_block);
  mi_putint(anc_buff,a_length+length,share->base.key_reflength);

  DBUG_RETURN( mi_getint(leaf_buff) <=
	       (info->quick_mode ? MI_MIN_KEYBLOCK_LENGTH :
		(uint) keyinfo->underflow_block_length));
err:
  DBUG_RETURN(-1);
} /* del */


	/* Balances adjacent pages if underflow occours */

static int underflow(MI_INFO *info, MI_KEYDEF *keyinfo,
		     uchar *anc_buff,
		     my_off_t leaf_page,/* Ancestor page and underflow page */
		     uchar *leaf_buff,
		     uchar *keypos)	/* Position to pos after key */
{
  int t_length;
  uint length,anc_length,buff_length,leaf_length,p_length,s_length,nod_flag,
       key_reflength,key_length;
  my_off_t next_page;
  uchar anc_key[MI_MAX_KEY_BUFF],leaf_key[MI_MAX_KEY_BUFF],
        *buff,*endpos,*next_keypos,*anc_pos,*half_pos,*temp_pos,*prev_key,
        *after_key;
  MI_KEY_PARAM s_temp;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("underflow");
  DBUG_PRINT("enter",("leaf_page: %ld  keypos: 0x%lx",(long) leaf_page,
		      (ulong) keypos));
  DBUG_DUMP("anc_buff",(uchar*) anc_buff,mi_getint(anc_buff));
  DBUG_DUMP("leaf_buff",(uchar*) leaf_buff,mi_getint(leaf_buff));

  buff=info->buff;
  info->buff_used=1;
  next_keypos=keypos;
  nod_flag=mi_test_if_nod(leaf_buff);
  p_length=nod_flag+2;
  anc_length=mi_getint(anc_buff);
  leaf_length=mi_getint(leaf_buff);
  key_reflength=share->base.key_reflength;
  if (info->s->keyinfo+info->lastinx == keyinfo)
    info->page_changed=1;

  if ((keypos < anc_buff+anc_length && (info->state->records & 1)) ||
      keypos == anc_buff+2+key_reflength)
  {					/* Use page right of anc-page */
    DBUG_PRINT("test",("use right page"));

    if (keyinfo->flag & HA_BINARY_PACK_KEY)
    {
      if (!(next_keypos=_mi_get_key(info, keyinfo,
				    anc_buff, buff, keypos, &length)))
	goto err;
    }
    else
    {
      /* Got to end of found key */
      buff[0]=buff[1]=0;	/* Avoid length error check if packed key */
      if (!(*keyinfo->get_key)(keyinfo,key_reflength,&next_keypos,
			       buff))
      goto err;
    }
    next_page= _mi_kpos(key_reflength,next_keypos);
    if (!_mi_fetch_keypage(info,keyinfo,next_page,DFLT_INIT_HITS,buff,0))
      goto err;
    buff_length=mi_getint(buff);
    DBUG_DUMP("next",(uchar*) buff,buff_length);

    /* find keys to make a big key-page */
    memmove((uchar*) next_keypos - key_reflength, (uchar*) buff + 2,
            key_reflength);
    if (!_mi_get_last_key(info,keyinfo,anc_buff,anc_key,next_keypos,&length)
	|| !_mi_get_last_key(info,keyinfo,leaf_buff,leaf_key,
			     leaf_buff+leaf_length,&length))
      goto err;

    /* merge pages and put parting key from anc_buff between */
    prev_key=(leaf_length == p_length ? (uchar*) 0 : leaf_key);
    t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,buff+p_length,
				  prev_key, prev_key,
				  anc_key, &s_temp);
    length=buff_length-p_length;
    endpos=buff+length+leaf_length+t_length;
    /* buff will always be larger than before !*/
    memmove(endpos - length, buff + buff_length - length, length);
    memcpy((uchar*) buff, (uchar*) leaf_buff,(size_t) leaf_length);
    (*keyinfo->store_key)(keyinfo,buff+leaf_length,&s_temp);
    buff_length=(uint) (endpos-buff);
    mi_putint(buff,buff_length,nod_flag);

    /* remove key from anc_buff */

    if (!(s_length=remove_key(keyinfo,key_reflength,keypos,anc_key,
                              anc_buff+anc_length,(my_off_t *) 0)))
      goto err;

    anc_length-=s_length;
    mi_putint(anc_buff,anc_length,key_reflength);

    if (buff_length <= keyinfo->block_length)
    {						/* Keys in one page */
      memcpy((uchar*) leaf_buff,(uchar*) buff,(size_t) buff_length);
      if (_mi_dispose(info,keyinfo,next_page,DFLT_INIT_HITS))
       goto err;
    }
    else
    {						/* Page is full */
      endpos=anc_buff+anc_length;
      DBUG_PRINT("test",("anc_buff: 0x%lx  endpos: 0x%lx",
                         (long) anc_buff, (long) endpos));
      if (keypos != anc_buff+2+key_reflength &&
	  !_mi_get_last_key(info,keyinfo,anc_buff,anc_key,keypos,&length))
	goto err;
      if (!(half_pos=_mi_find_half_pos(nod_flag, keyinfo, buff, leaf_key,
				       &key_length, &after_key)))
	goto err;
      length=(uint) (half_pos-buff);
      memcpy((uchar*) leaf_buff,(uchar*) buff,(size_t) length);
      mi_putint(leaf_buff,length,nod_flag);

      /* Correct new keypointer to leaf_page */
      half_pos=after_key;
      _mi_kpointer(info,leaf_key+key_length,next_page);
      /* Save key in anc_buff */
      prev_key=(keypos == anc_buff+2+key_reflength ? (uchar*) 0 : anc_key),
      t_length=(*keyinfo->pack_key)(keyinfo,key_reflength,
				    (keypos == endpos ? (uchar*) 0 :
				     keypos),
				    prev_key, prev_key,
				    leaf_key, &s_temp);
      if (t_length >= 0)
        memmove(keypos + t_length, keypos, (size_t) (endpos - keypos));
      else
	memmove(keypos, keypos - t_length, (uint) (endpos - keypos) + t_length);
      (*keyinfo->store_key)(keyinfo,keypos,&s_temp);
      mi_putint(anc_buff,(anc_length+=t_length),key_reflength);

	/* Store key first in new page */
      if (nod_flag)
	memmove((uchar*) buff + 2, (uchar*) half_pos - nod_flag, (size_t) nod_flag);
      if (!(*keyinfo->get_key)(keyinfo,nod_flag,&half_pos,leaf_key))
	goto err;
      t_length=(int) (*keyinfo->pack_key)(keyinfo, nod_flag, (uchar*) 0,
					  (uchar*) 0, (uchar *) 0,
					  leaf_key, &s_temp);
      /* t_length will always be > 0 for a new page !*/
      length=(uint) ((buff+mi_getint(buff))-half_pos);
      memmove((uchar*) buff + p_length + t_length, (uchar*) half_pos, (size_t) length);
      (*keyinfo->store_key)(keyinfo,buff+p_length,&s_temp);
      mi_putint(buff,length+t_length+p_length,nod_flag);

      if (_mi_write_keypage(info,keyinfo,next_page,DFLT_INIT_HITS,buff))
	goto err;
    }
    if (_mi_write_keypage(info,keyinfo,leaf_page,DFLT_INIT_HITS,leaf_buff))
      goto err;
    DBUG_RETURN(anc_length <= ((info->quick_mode ? MI_MIN_BLOCK_LENGTH :
				(uint) keyinfo->underflow_block_length)));
  }

  DBUG_PRINT("test",("use left page"));

  keypos=_mi_get_last_key(info,keyinfo,anc_buff,anc_key,keypos,&length);
  if (!keypos)
    goto err;
  next_page= _mi_kpos(key_reflength,keypos);
  if (!_mi_fetch_keypage(info,keyinfo,next_page,DFLT_INIT_HITS,buff,0))
      goto err;
  buff_length=mi_getint(buff);
  endpos=buff+buff_length;
  DBUG_DUMP("prev",(uchar*) buff,buff_length);

  /* find keys to make a big key-page */
  memmove((uchar*) next_keypos - key_reflength, (uchar*) leaf_buff + 2,
          key_reflength);
  next_keypos=keypos;
  if (!(*keyinfo->get_key)(keyinfo,key_reflength,&next_keypos,
			   anc_key))
    goto err;
  if (!_mi_get_last_key(info,keyinfo,buff,leaf_key,endpos,&length))
    goto err;

  /* merge pages and put parting key from anc_buff between */
  prev_key=(leaf_length == p_length ? (uchar*) 0 : leaf_key);
  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,
				(leaf_length == p_length ?
                                 (uchar*) 0 : leaf_buff+p_length),
				prev_key, prev_key,
				anc_key, &s_temp);
  if (t_length >= 0)
    memmove((uchar*) endpos + t_length, (uchar*) leaf_buff + p_length,
	    (size_t) (leaf_length - p_length));
  else						/* We gained space */
    memmove((uchar*) endpos, (uchar*) leaf_buff + ((int) p_length - t_length),
            (size_t) (leaf_length - p_length + t_length));

  (*keyinfo->store_key)(keyinfo,endpos,&s_temp);
  buff_length=buff_length+leaf_length-p_length+t_length;
  mi_putint(buff,buff_length,nod_flag);

  /* remove key from anc_buff */
  if (!(s_length= remove_key(keyinfo,key_reflength,keypos,anc_key,
                             anc_buff+anc_length,(my_off_t *) 0)))
    goto err;

  anc_length-=s_length;
  mi_putint(anc_buff,anc_length,key_reflength);

  if (buff_length <= keyinfo->block_length)
  {						/* Keys in one page */
    if (_mi_dispose(info,keyinfo,leaf_page,DFLT_INIT_HITS))
      goto err;
  }
  else
  {						/* Page is full */
    if (keypos == anc_buff+2+key_reflength)
      anc_pos=0;				/* First key */
    else if (!_mi_get_last_key(info,keyinfo,anc_buff,anc_pos=anc_key,keypos,
			       &length))
      goto err;
    endpos=_mi_find_half_pos(nod_flag,keyinfo,buff,leaf_key,
			     &key_length, &half_pos);
    if (!endpos)
      goto err;
    _mi_kpointer(info,leaf_key+key_length,leaf_page);
    /* Save key in anc_buff */
    DBUG_DUMP("anc_buff",(uchar*) anc_buff,anc_length);
    DBUG_DUMP("key_to_anc",(uchar*) leaf_key,key_length);

    temp_pos=anc_buff+anc_length;
    t_length=(*keyinfo->pack_key)(keyinfo,key_reflength,
				  keypos == temp_pos ? (uchar*) 0
				  : keypos,
				  anc_pos, anc_pos,
				  leaf_key,&s_temp);
    if (t_length > 0)
      memmove(keypos + t_length, keypos, (size_t) (temp_pos - keypos));
    else
      memmove(keypos, keypos - t_length,
              (uint) (temp_pos - keypos) + t_length);
    (*keyinfo->store_key)(keyinfo,keypos,&s_temp);
    mi_putint(anc_buff,(anc_length+=t_length),key_reflength);

    /* Store first key on new page */
    if (nod_flag)
      memmove((uchar*) leaf_buff + 2,
              (uchar*) half_pos - nod_flag, (size_t) nod_flag);
    if (!(length=(*keyinfo->get_key)(keyinfo,nod_flag,&half_pos,leaf_key)))
      goto err;
    DBUG_DUMP("key_to_leaf",(uchar*) leaf_key,length);
    t_length=(*keyinfo->pack_key)(keyinfo,nod_flag, (uchar*) 0,
				  (uchar*) 0, (uchar*) 0, leaf_key, &s_temp);
    length=(uint) ((buff+buff_length)-half_pos);
    DBUG_PRINT("info",("t_length: %d  length: %d",t_length,(int) length));
    memmove((uchar*) leaf_buff + p_length + t_length,
            (uchar*) half_pos, (size_t) length);
    (*keyinfo->store_key)(keyinfo,leaf_buff+p_length,&s_temp);
    mi_putint(leaf_buff,length+t_length+p_length,nod_flag);
    if (_mi_write_keypage(info,keyinfo,leaf_page,DFLT_INIT_HITS,leaf_buff))
      goto err;
    mi_putint(buff,endpos-buff,nod_flag);
  }
  if (_mi_write_keypage(info,keyinfo,next_page,DFLT_INIT_HITS,buff))
    goto err;
  DBUG_RETURN(anc_length <= (uint) keyinfo->block_length/2);

err:
  DBUG_RETURN(-1);
} /* underflow */


	/*
	  remove a key from packed buffert
	  The current code doesn't handle the case that the next key may be
	  packed better against the previous key if there is a case difference
	  returns how many chars was removed or 0 on error
	*/

static uint remove_key(MI_KEYDEF *keyinfo, uint nod_flag,
		       uchar *keypos,	/* Where key starts */
		       uchar *lastkey,	/* key to be removed */
		       uchar *page_end, /* End of page */
		       my_off_t *next_block)	/* ptr to next block */
{
  int s_length;
  uchar *start;
  DBUG_ENTER("remove_key");
  DBUG_PRINT("enter",("keypos: 0x%lx  page_end: 0x%lx",(long) keypos, (long) page_end));

  start=keypos;
  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)))
  {
    s_length=(int) (keyinfo->keylength+nod_flag);
    if (next_block && nod_flag)
      *next_block= _mi_kpos(nod_flag,keypos+s_length);
  }
  else
  {					 /* Let keypos point at next key */
    /* Calculate length of key */
    if (!(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,lastkey))
      DBUG_RETURN(0);				/* Error */

    if (next_block && nod_flag)
      *next_block= _mi_kpos(nod_flag,keypos);
    s_length=(int) (keypos-start);
    if (keypos != page_end)
    {
      if (keyinfo->flag & HA_BINARY_PACK_KEY)
      {
	uchar *old_key=start;
	uint next_length,prev_length,prev_pack_length;
	get_key_length(next_length,keypos);
	get_key_pack_length(prev_length,prev_pack_length,old_key);
	if (next_length > prev_length)
	{
	  /* We have to copy data from the current key to the next key */
	  memmove(keypos - next_length + prev_length, lastkey + prev_length,
              next_length - prev_length);
	  keypos-=(next_length-prev_length)+prev_pack_length;
	  store_key_length(keypos,prev_length);
	  s_length=(int) (keypos-start);
	}
      }
      else
      {
	/* Check if a variable length first key part */
	if ((keyinfo->seg->flag & HA_PACK_KEY) && *keypos & 128)
	{
	  /* Next key is packed against the current one */
	  uint next_length,prev_length,prev_pack_length,lastkey_length,
	    rest_length;
	  if (keyinfo->seg[0].length >= 127)
	  {
	    if (!(prev_length=mi_uint2korr(start) & 32767))
	      goto end;
	    next_length=mi_uint2korr(keypos) & 32767;
	    keypos+=2;
	    prev_pack_length=2;
	  }
	  else
	  {
	    if (!(prev_length= *start & 127))
	      goto end;				/* Same key as previous*/
	    next_length= *keypos & 127;
	    keypos++;
	    prev_pack_length=1;
	  }
	  if (!(*start & 128))
	    prev_length=0;			/* prev key not packed */
	  if (keyinfo->seg[0].flag & HA_NULL_PART)
	    lastkey++;				/* Skip null marker */
	  get_key_length(lastkey_length,lastkey);
	  if (!next_length)			/* Same key after */
	  {
	    next_length=lastkey_length;
	    rest_length=0;
	  }
	  else
	    get_key_length(rest_length,keypos);

	  if (next_length >= prev_length)
	  {		/* Key after is based on deleted key */
	    uint pack_length;
	    uint tmp= next_length - prev_length;
	    memmove(keypos - tmp, lastkey + next_length - tmp, tmp);
	    rest_length+=tmp;
	    pack_length= prev_length ? get_pack_length(rest_length): 0;
	    keypos-=tmp+pack_length+prev_pack_length;
	    s_length=(int) (keypos-start);
	    if (prev_length)			/* Pack against prev key */
	    {
	      *keypos++= start[0];
	      if (prev_pack_length == 2)
		*keypos++= start[1];
	      store_key_length(keypos,rest_length);
	    }
	    else
	    {
	      /* Next key is not packed anymore */
	      if (keyinfo->seg[0].flag & HA_NULL_PART)
	      {
		rest_length++;			/* Mark not null */
	      }
	      if (prev_pack_length == 2)
	      {
		mi_int2store(keypos,rest_length);
	      }
	      else
		*keypos= rest_length;
	    }
	  }
	}
      }
    }
  }
  end:
  memmove((uchar*) start, (uchar*) start + s_length,
          (uint) (page_end - start - s_length));
  DBUG_RETURN((uint) s_length);
} /* remove_key */
