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

#include "ma_fulltext.h"
#include "ma_rt_index.h"
#include "trnman.h"
#include "ma_key_recover.h"

static int d_search(MARIA_HA *info, MARIA_KEY *key, uint32 comp_flag,
                    my_off_t page, uchar *anc_buff,
                    MARIA_PINNED_PAGE *anc_page_link);
static int del(MARIA_HA *info, MARIA_KEY *key,
               my_off_t anc_page, uchar *anc_buff, my_off_t leaf_page,
               uchar *leaf_buff, MARIA_PINNED_PAGE *leaf_page_link,
               uchar *keypos, my_off_t next_block, uchar *ret_key);
static int underflow(MARIA_HA *info,MARIA_KEYDEF *keyinfo,
                     my_off_t anc_page, uchar *anc_buff,
		     my_off_t leaf_page, uchar *leaf_buff,
                     MARIA_PINNED_PAGE *leaf_page_link, uchar *keypos);
static uint remove_key(MARIA_KEYDEF *keyinfo, uint page_flag, uint nod_flag,
                       uchar *keypos, uchar *lastkey, uchar *page_end,
		       my_off_t *next_block, MARIA_KEY_PARAM *s_temp);

/* @breif Remove a row from a MARIA table */

int maria_delete(MARIA_HA *info,const uchar *record)
{
  uint i;
  uchar *old_key;
  int save_errno;
  char lastpos[8];
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo;
  DBUG_ENTER("maria_delete");

  /* Test if record is in datafile */
  DBUG_EXECUTE_IF("maria_pretend_crashed_table_on_usage",
                  maria_print_error(share, HA_ERR_CRASHED);
                  DBUG_RETURN(my_errno= HA_ERR_CRASHED););
  DBUG_EXECUTE_IF("my_error_test_undefined_error",
                  maria_print_error(share, INT_MAX);
                  DBUG_RETURN(my_errno= INT_MAX););
  if (!(info->update & HA_STATE_AKTIV))
  {
    DBUG_RETURN(my_errno=HA_ERR_KEY_NOT_FOUND);	/* No database read */
  }
  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    DBUG_RETURN(my_errno=EACCES);
  }
  if (_ma_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno);
  if ((*share->compare_record)(info,record))
    goto err;				/* Error on read-check */

  if (_ma_mark_file_changed(info))
    goto err;

  /* Ensure we don't change the autoincrement value */
  info->last_auto_increment= ~(ulonglong) 0;
  /* Remove all keys from the index file */

  old_key= info->lastkey_buff2;

  for (i=0, keyinfo= share->keyinfo ; i < share->base.keys ; i++, keyinfo++)
  {
    if (maria_is_key_active(share->state.key_map, i))
    {
      keyinfo->version++;
      if (keyinfo->flag & HA_FULLTEXT)
      {
        if (_ma_ft_del(info, i, old_key, record, info->cur_row.lastpos))
          goto err;
      }
      else
      {
        MARIA_KEY key;
        if (keyinfo->ck_delete(info,
                               (*keyinfo->make_key)(info, &key, i, old_key,
                                                    record,
                                                    info->cur_row.lastpos,
                                                    info->cur_row.trid)))
          goto err;
      }
      /* The above changed info->lastkey2. Inform maria_rnext_same(). */
      info->update&= ~HA_STATE_RNEXT_SAME;
    }
  }

  if (share->calc_checksum)
  {
    /*
      We can't use the row based checksum as this doesn't have enough
      precision.
    */
    info->cur_row.checksum= (*share->calc_checksum)(info, record);
  }

  if ((*share->delete_record)(info, record))
    goto err;				/* Remove record from database */

  info->state->checksum-= info->cur_row.checksum;
  info->state->records--;
  info->update= HA_STATE_CHANGED+HA_STATE_DELETED+HA_STATE_ROW_CHANGED;
  share->state.changed|= (STATE_NOT_OPTIMIZED_ROWS | STATE_NOT_MOVABLE |
                          STATE_NOT_ZEROFILLED);

  mi_sizestore(lastpos, info->cur_row.lastpos);
  VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
  allow_break();			/* Allow SIGHUP & SIGINT */
  if (info->invalidator != 0)
  {
    DBUG_PRINT("info", ("invalidator... '%s' (delete)",
                        share->open_file_name.str));
    (*info->invalidator)(share->open_file_name.str);
    info->invalidator=0;
  }
  DBUG_RETURN(0);

err:
  save_errno= my_errno;
  DBUG_ASSERT(save_errno);
  if (!save_errno)
    save_errno= HA_ERR_INTERNAL_ERROR;          /* Should never happen */

  mi_sizestore(lastpos, info->cur_row.lastpos);
  if (save_errno != HA_ERR_RECORD_CHANGED)
  {
    maria_print_error(share, HA_ERR_CRASHED);
    maria_mark_crashed(info);		/* mark table crashed */
  }
  VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
  info->update|=HA_STATE_WRITTEN;	/* Buffer changed */
  allow_break();			/* Allow SIGHUP & SIGINT */
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
  {
    maria_print_error(share, HA_ERR_CRASHED);
    my_errno=HA_ERR_CRASHED;
  }
  DBUG_RETURN(my_errno= save_errno);
} /* maria_delete */


/*
  Remove a key from the btree index

  TODO:
   Change ma_ck_real_delete() to use another buffer for changed keys instead
   of key->data. This would allows us to remove the copying of the key here.
*/

int _ma_ck_delete(register MARIA_HA *info, MARIA_KEY *key)
{
  MARIA_SHARE *share= info->s;
  int res;
  LSN lsn= LSN_IMPOSSIBLE;
  my_off_t new_root= share->state.key_root[key->keyinfo->key_nr];
  uchar key_buff[MARIA_MAX_KEY_BUFF], *save_key_data;
  MARIA_KEY org_key;
  DBUG_ENTER("_ma_ck_delete");

  save_key_data= key->data;
  if (share->now_transactional)
  {
    /* Save original value as the key may change */
    memcpy(key_buff, key->data, key->data_length + key->ref_length);
    org_key= *key;
    key->data= key_buff;
  }

  res= _ma_ck_real_delete(info, key, &new_root);

  key->data= save_key_data;
  if (!res && share->now_transactional)
    res= _ma_write_undo_key_delete(info, &org_key, new_root, &lsn);
  else
  {
    share->state.key_root[key->keyinfo->key_nr]= new_root;
    _ma_fast_unlock_key_del(info);
  }
  _ma_unpin_all_pages_and_finalize_row(info, lsn);
  DBUG_RETURN(res);
} /* _ma_ck_delete */


int _ma_ck_real_delete(register MARIA_HA *info, MARIA_KEY *key,
                       my_off_t *root)
{
  int error;
  uint nod_flag;
  my_off_t old_root;
  uchar *root_buff;
  MARIA_PINNED_PAGE *page_link;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  DBUG_ENTER("_ma_ck_real_delete");

  if ((old_root=*root) == HA_OFFSET_ERROR)
  {
    maria_print_error(info->s, HA_ERR_CRASHED);
    DBUG_RETURN(my_errno=HA_ERR_CRASHED);
  }
  if (!(root_buff= (uchar*)  my_alloca((uint) keyinfo->block_length+
                                       MARIA_MAX_KEY_BUFF*2)))
  {
    DBUG_PRINT("error",("Couldn't allocate memory"));
    DBUG_RETURN(my_errno=ENOMEM);
  }
  DBUG_PRINT("info",("root_page: %ld", (long) old_root));
  if (!_ma_fetch_keypage(info, keyinfo, old_root,
                         PAGECACHE_LOCK_WRITE, DFLT_INIT_HITS, root_buff, 0,
                         &page_link))
  {
    error= -1;
    goto err;
  }
  if ((error= d_search(info, key, (keyinfo->flag & HA_FULLTEXT ?
                                   SEARCH_FIND | SEARCH_UPDATE | SEARCH_INSERT:
                                   SEARCH_SAME),
                       old_root, root_buff, page_link)) > 0)
  {
    if (error == 2)
    {
      DBUG_PRINT("test",("Enlarging of root when deleting"));
      error= _ma_enlarge_root(info, key, root);
    }
    else /* error == 1 */
    {
      uint used_length;
      MARIA_SHARE *share= info->s;
      _ma_get_used_and_nod(share, root_buff, used_length, nod_flag);
      page_link->changed= 1;
      if (used_length <= nod_flag + share->keypage_header + 1)
      {
	error=0;
	if (nod_flag)
	  *root= _ma_kpos(nod_flag, root_buff +share->keypage_header +
                          nod_flag);
	else
	  *root=HA_OFFSET_ERROR;
	if (_ma_dispose(info, old_root, 0))
	  error= -1;
      }
      else
	error= _ma_write_keypage(info,keyinfo, old_root,
                                 PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                 DFLT_INIT_HITS, root_buff);
    }
  }
err:
  my_afree((uchar*) root_buff);
  DBUG_PRINT("exit",("Return: %d",error));
  DBUG_RETURN(error);
} /* _ma_ck_real_delete */


/**
   @brief Remove key below key root

   @param key  Key to delete.  Will contain new key if block was enlarged

   @return
   @retval 0   ok (anc_page is not changed)
   @retval 1   If data on page is too small; In this case anc_buff is not saved
   @retval 2   If data on page is too big
   @retval -1  On errors
*/

static int d_search(MARIA_HA *info, MARIA_KEY *key, uint32 comp_flag,
                    my_off_t anc_page, uchar *anc_buff,
                    MARIA_PINNED_PAGE *anc_page_link)
{
  int flag,ret_value,save_flag;
  uint nod_flag, page_flag;
  my_bool last_key;
  uchar *leaf_buff,*keypos;
  my_off_t leaf_page,next_block;
  uchar lastkey[MARIA_MAX_KEY_BUFF];
  MARIA_PINNED_PAGE *leaf_page_link;
  MARIA_KEY_PARAM s_temp;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  DBUG_ENTER("d_search");
  DBUG_DUMP("page",anc_buff,_ma_get_page_used(share, anc_buff));

  flag=(*keyinfo->bin_search)(key, anc_buff, comp_flag, &keypos, lastkey,
                              &last_key);
  if (flag == MARIA_FOUND_WRONG_KEY)
  {
    DBUG_PRINT("error",("Found wrong key"));
    DBUG_RETURN(-1);
  }
  page_flag= _ma_get_keypage_flag(share, anc_buff);
  nod_flag= _ma_test_if_nod(share, anc_buff);

  if (!flag && (keyinfo->flag & HA_FULLTEXT))
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
        insert_dynamic(info->ft1_to_ft2, (lastkey+off));
      }
      else
      {
        /* we need exact match only if not in ft1->ft2 conversion mode */
        flag=(*keyinfo->bin_search)(key, anc_buff, comp_flag, &keypos,
                                    lastkey, &last_key);
      }
      /* fall through to normal delete */
    }
    else
    {
      /* popular word. two-level tree. going down */
      uint tmp_key_length;
      my_off_t root;
      uchar *kpos=keypos;
      MARIA_KEY tmp_key;

      tmp_key.data=    lastkey;
      tmp_key.keyinfo= keyinfo;

      if (!(tmp_key_length=(*keyinfo->get_key)(&tmp_key, page_flag, nod_flag,
                                               &kpos)))
      {
        maria_print_error(share, HA_ERR_CRASHED);
        my_errno= HA_ERR_CRASHED;
        DBUG_RETURN(-1);
      }
      root= _ma_row_pos_from_key(&tmp_key);
      if (subkeys == -1)
      {
        /* the last entry in sub-tree */
        if (_ma_dispose(info, root, 1))
          DBUG_RETURN(-1);
        /* fall through to normal delete */
      }
      else
      {
        MARIA_KEY word_key;
        keyinfo=&share->ft2_keyinfo;
        /* we'll modify key entry 'in vivo' */
        kpos-=keyinfo->keylength+nod_flag;
        get_key_full_length_rdonly(off, key->data);

        word_key.data=        key->data + off;
        word_key.keyinfo=     &share->ft2_keyinfo;
        word_key.data_length= HA_FT_WLEN;
        word_key.ref_length= 0;
        word_key.flag= 0;
        ret_value= _ma_ck_real_delete(info, &word_key, &root);
        _ma_dpointer(share, kpos+HA_FT_WLEN, root);
        subkeys++;
        ft_intXstore(kpos, subkeys);
        if (!ret_value)
        {
          anc_page_link->changed= 1;
          ret_value= _ma_write_keypage(info, keyinfo, anc_page,
                                       PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                       DFLT_INIT_HITS, anc_buff);
        }
        DBUG_PRINT("exit",("Return: %d",ret_value));
        DBUG_RETURN(ret_value);
      }
    }
  }
  leaf_buff=0;
  LINT_INIT(leaf_page);
  if (nod_flag)
  {
    /* Read left child page */
    leaf_page= _ma_kpos(nod_flag,keypos);
    if (!(leaf_buff= (uchar*) my_alloca((uint) keyinfo->block_length+
                                       MARIA_MAX_KEY_BUFF*2)))
    {
      DBUG_PRINT("error", ("Couldn't allocate memory"));
      my_errno=ENOMEM;
      DBUG_RETURN(-1);
    }
    if (!_ma_fetch_keypage(info,keyinfo,leaf_page,
                           PAGECACHE_LOCK_WRITE, DFLT_INIT_HITS, leaf_buff,
                           0, &leaf_page_link))
      goto err;
  }

  if (flag != 0)
  {
    if (!nod_flag)
    {
      DBUG_PRINT("error",("Didn't find key"));
      maria_print_error(share, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;		/* This should newer happend */
      goto err;
    }
    save_flag=0;
    ret_value= d_search(info, key, comp_flag, leaf_page, leaf_buff,
                        leaf_page_link);
  }
  else
  {						/* Found key */
    uint tmp;
    uint anc_buff_length= _ma_get_page_used(share, anc_buff);
    uint anc_page_flag= _ma_get_keypage_flag(share, anc_buff);

    if (!(tmp= remove_key(keyinfo, anc_page_flag, nod_flag, keypos, lastkey,
                          anc_buff + anc_buff_length,
                          &next_block, &s_temp)))
      goto err;

    anc_page_link->changed= 1;
    anc_buff_length-= tmp;
    _ma_store_page_used(share, anc_buff, anc_buff_length);

    /*
      Log initial changes on pages
      If there is an underflow, there will be more changes logged to the
      page
    */
    if (share->now_transactional &&
        _ma_log_delete(info, anc_page, anc_buff, s_temp.key_pos,
                       s_temp.changed_length, s_temp.move_length))
      DBUG_RETURN(-1);

    if (!nod_flag)
    {						/* On leaf page */
      if (anc_buff_length <= (info->quick_mode ?
                              MARIA_MIN_KEYBLOCK_LENGTH :
                              (uint) keyinfo->underflow_block_length))
      {
        /* Page will be written by caller if we return 1 */
        DBUG_RETURN(1);
      }
      if (_ma_write_keypage(info, keyinfo, anc_page,
                            PAGECACHE_LOCK_LEFT_WRITELOCKED, DFLT_INIT_HITS,
                            anc_buff))
	DBUG_RETURN(-1);
      DBUG_RETURN(0);
    }
    save_flag=1;                         /* Mark that anc_buff is changed */
    ret_value= del(info, key, anc_page, anc_buff,
                   leaf_page, leaf_buff, leaf_page_link,
                   keypos, next_block, lastkey);
  }
  if (ret_value >0)
  {
    save_flag=1;
    if (ret_value == 1)
      ret_value= underflow(info, keyinfo, anc_page, anc_buff,
                           leaf_page, leaf_buff, leaf_page_link, keypos);
    else
    {
      /* This can only happen with variable length keys */
      MARIA_KEY last_key;
      DBUG_PRINT("test",("Enlarging of key when deleting"));

      last_key.data=    lastkey;
      last_key.keyinfo= keyinfo;
      if (!_ma_get_last_key(&last_key, anc_buff, keypos))
	goto err;
      ret_value= _ma_insert(info, key, anc_buff, keypos, anc_page,
                            last_key.data, (my_off_t) 0, (uchar*) 0,
                            (MARIA_PINNED_PAGE*) 0, (uchar*) 0, (my_bool) 0);
    }
  }
  if (ret_value == 0 && _ma_get_page_used(share, anc_buff) >
      (uint) (keyinfo->block_length - KEYPAGE_CHECKSUM_SIZE))
  {
    /* parent buffer got too big ; We have to split the page */
    save_flag=1;
    ret_value= _ma_split_page(info, key, anc_page, anc_buff,
                              (uint) (keyinfo->block_length -
                                      KEYPAGE_CHECKSUM_SIZE),
                              (uchar*) 0, 0, 0, lastkey, 0) | 2;
  }
  if (save_flag && ret_value != 1)
  {
    anc_page_link->changed= 1;
    ret_value|= _ma_write_keypage(info, keyinfo, anc_page,
                                  PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                  DFLT_INIT_HITS, anc_buff);
  }
  else
  {
    DBUG_DUMP("page", anc_buff, _ma_get_page_used(share, anc_buff));
  }
  my_afree(leaf_buff);
  DBUG_PRINT("exit",("Return: %d",ret_value));
  DBUG_RETURN(ret_value);

err:
  my_afree(leaf_buff);
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1);
} /* d_search */


/**
   @brief Remove a key that has a page-reference

   @param info		 Maria handler
   @param key		 Buffer for key to be inserted at upper level
   @param anc_page	 Page address for page where deleted key was
   @param anc_buff       Page buffer (nod) where deleted key was
   @param leaf_page      Page address for nod before the deleted key
   @param leaf_buff      Buffer for leaf_page
   @param leaf_buff_link Pinned page link for leaf_buff
   @param keypos         Pos to where deleted key was on anc_buff
   @param next_block	 Page adress for nod after deleted key
   @param ret_key_buff	 Key before keypos in anc_buff

   @notes
      leaf_buff must be written to disk if retval > 0
      anc_buff  is not updated on disk. Caller should do this

   @return
   @retval < 0   Error
   @retval 0     OK.    leaf_buff is written to disk

   @retval 1     key contains key to upper level (from balance page)
                 leaf_buff has underflow
   @retval 2     key contains key to upper level (from split space)
*/

static int del(MARIA_HA *info, MARIA_KEY *key,
               my_off_t anc_page, uchar *anc_buff,
               my_off_t leaf_page, uchar *leaf_buff,
               MARIA_PINNED_PAGE *leaf_page_link,
	       uchar *keypos, my_off_t next_block, uchar *ret_key_buff)
{
  int ret_value,length;
  uint a_length, page_flag, nod_flag, leaf_length, new_leaf_length;
  my_off_t next_page;
  uchar keybuff[MARIA_MAX_KEY_BUFF],*endpos,*next_buff,*key_start, *prev_key;
  MARIA_KEY_PARAM s_temp;
  MARIA_PINNED_PAGE *next_page_link;
  MARIA_KEY tmp_key;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_KEY ret_key;
  DBUG_ENTER("del");
  DBUG_PRINT("enter",("leaf_page: %ld  keypos: 0x%lx", (long) leaf_page,
		      (ulong) keypos));

  page_flag= _ma_get_keypage_flag(share, leaf_buff);
  _ma_get_used_and_nod_with_flag(share, page_flag, leaf_buff, leaf_length,
                                 nod_flag);
  DBUG_DUMP("leaf_buff", leaf_buff, leaf_length);

  endpos= leaf_buff + leaf_length;
  tmp_key.keyinfo= keyinfo;
  tmp_key.data=    keybuff;

  if (!(key_start= _ma_get_last_key(&tmp_key, leaf_buff, endpos)))
    DBUG_RETURN(-1);

  if (nod_flag)
  {
    next_page= _ma_kpos(nod_flag,endpos);
    if (!(next_buff= (uchar*) my_alloca((uint) keyinfo->block_length+
					MARIA_MAX_KEY_BUFF*2)))
      DBUG_RETURN(-1);
    if (!_ma_fetch_keypage(info, keyinfo, next_page, PAGECACHE_LOCK_WRITE,
                           DFLT_INIT_HITS, next_buff, 0, &next_page_link))
      ret_value= -1;
    else
    {
      DBUG_DUMP("next_page", next_buff, _ma_get_page_used(share, next_buff));
      if ((ret_value= del(info, key, anc_page, anc_buff, next_page,
                          next_buff, next_page_link, keypos, next_block,
                          ret_key_buff)) >0)
      {
        /* Get new length after key was deleted */
	endpos=leaf_buff+_ma_get_page_used(share, leaf_buff);
	if (ret_value == 1)
	{
	  ret_value= underflow(info, keyinfo, leaf_page, leaf_buff, next_page,
                               next_buff, next_page_link, endpos);
	  if (ret_value == 0 &&
              _ma_get_page_used(share, leaf_buff) >
              (uint) (keyinfo->block_length - KEYPAGE_CHECKSUM_SIZE))
	  {
	    ret_value= (_ma_split_page(info, key,
                                       leaf_page, leaf_buff,
                                       (uint) (keyinfo->block_length -
                                               KEYPAGE_CHECKSUM_SIZE),
                                       (uchar*) 0, 0, 0,
                                       ret_key_buff, 0) | 2);
	  }
	}
	else
	{
	  DBUG_PRINT("test",("Inserting of key when deleting"));
	  if (!_ma_get_last_key(&tmp_key, leaf_buff, endpos))
	    goto err;
	  ret_value= _ma_insert(info, key, leaf_buff, endpos,
                                leaf_page, tmp_key.data, (my_off_t) 0,
                                (uchar*) 0, (MARIA_PINNED_PAGE *) 0,
                                (uchar*) 0, 0);
	}
      }
      leaf_page_link->changed= 1;
      /*
        If ret_value <> 0, then leaf_page underflowed and caller will have
        to handle underflow and write leaf_page to disk.
        We can't write it here, as if leaf_page is empty we get an assert
        in _ma_write_keypage.
      */
      if (ret_value == 0 && _ma_write_keypage(info, keyinfo, leaf_page,
                                              PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                              DFLT_INIT_HITS, leaf_buff))
	goto err;
    }
    my_afree(next_buff);
    DBUG_RETURN(ret_value);
  }

  /*
    Remove last key from leaf page
    Note that leaf_page page may only have had one key (can normally only
    happen in quick mode), in which ase it will now temporary have 0 keys
    on it. This will be corrected by the caller as we will return 0.
  */
  new_leaf_length= (uint) (key_start - leaf_buff);
  _ma_store_page_used(share, leaf_buff, new_leaf_length);

  if (share->now_transactional &&
      _ma_log_suffix(info, leaf_page, leaf_buff, leaf_length,
                     new_leaf_length))
    goto err;

  leaf_page_link->changed= 1;                 /* Safety */
  if (new_leaf_length <= (info->quick_mode ? MARIA_MIN_KEYBLOCK_LENGTH :
                          (uint) keyinfo->underflow_block_length))
  {
    /* Underflow, leaf_page will be written by caller */
    ret_value= 1;
  }
  else
  {
    ret_value= 0;
    if (_ma_write_keypage(info, keyinfo, leaf_page,
                          PAGECACHE_LOCK_LEFT_WRITELOCKED, DFLT_INIT_HITS,
                          leaf_buff))
      goto err;
  }

  /* Place last key in ancestor page on deleted key position */
  a_length= _ma_get_page_used(share, anc_buff);
  endpos=anc_buff+a_length;

  ret_key.keyinfo= keyinfo;
  ret_key.data=    ret_key_buff;

  prev_key= 0;
  if (keypos != anc_buff+share->keypage_header + share->base.key_reflength)
  {
    if (!_ma_get_last_key(&ret_key, anc_buff, keypos))
      goto err;
    prev_key= ret_key.data;
  }
  length= (*keyinfo->pack_key)(&tmp_key, share->base.key_reflength,
                               keypos == endpos ? (uchar*) 0 : keypos,
                               prev_key, prev_key,
                               &s_temp);
  if (length > 0)
    bmove_upp(endpos+length,endpos,(uint) (endpos-keypos));
  else
    bmove(keypos,keypos-length, (int) (endpos-keypos)+length);
  (*keyinfo->store_key)(keyinfo,keypos,&s_temp);
  key_start= keypos;
  if (tmp_key.flag & (SEARCH_USER_KEY_HAS_TRANSID |
                      SEARCH_PAGE_KEY_HAS_TRANSID))
    _ma_mark_page_with_transid(share, anc_buff);

  /* Save pointer to next leaf on parent page */
  if (!(*keyinfo->get_key)(&ret_key, page_flag, share->base.key_reflength,
                           &keypos))
    goto err;
  _ma_kpointer(info,keypos - share->base.key_reflength,next_block);
  _ma_store_page_used(share, anc_buff, a_length + length);

  if (share->now_transactional &&
      _ma_log_add(info, anc_page, anc_buff, a_length,
                  key_start, s_temp.changed_length, s_temp.move_length, 1))
    goto err;

  DBUG_RETURN(new_leaf_length <=
              (info->quick_mode ? MARIA_MIN_KEYBLOCK_LENGTH :
               (uint) keyinfo->underflow_block_length));
err:
  DBUG_RETURN(-1);
} /* del */


/**
   @brief Balances adjacent pages if underflow occours

   @fn    underflow()
   @param anc_buff        Anchestor page data
   @param leaf_page       Page number of leaf page
   @param leaf_buff       Leaf page (page that underflowed)
   @param leaf_page_link  Pointer to pin information about leaf page
   @param keypos          Position after current key in anc_buff

   @note
     This function writes redo entries for all changes
     leaf_page is saved to disk
     Caller must save anc_buff

   @return
   @retval  0  ok
   @retval  1  ok, but anc_buff did underflow
   @retval -1  error
 */

static int underflow(register MARIA_HA *info, MARIA_KEYDEF *keyinfo,
		     my_off_t anc_page, uchar *anc_buff,
		     my_off_t leaf_page, uchar *leaf_buff,
                     MARIA_PINNED_PAGE *leaf_page_link,
		     uchar *keypos)
{
  int t_length;
  uint anc_length,buff_length,leaf_length,p_length,s_length,nod_flag;
  uint next_buff_length, new_buff_length, key_reflength;
  uint unchanged_leaf_length, new_leaf_length, new_anc_length;
  uint anc_page_flag, page_flag;
  my_off_t next_page;
  uchar anc_key_buff[MARIA_MAX_KEY_BUFF], leaf_key_buff[MARIA_MAX_KEY_BUFF];
  uchar *buff,*endpos,*next_keypos,*anc_pos,*half_pos,*prev_key;
  uchar *after_key, *anc_end_pos;
  MARIA_KEY_PARAM key_deleted, key_inserted;
  MARIA_SHARE *share= info->s;
  MARIA_PINNED_PAGE *next_page_link;
  my_bool first_key;
  MARIA_KEY tmp_key, anc_key, leaf_key;
  DBUG_ENTER("underflow");
  DBUG_PRINT("enter",("leaf_page: %ld  keypos: 0x%lx",(long) leaf_page,
		      (ulong) keypos));
  DBUG_DUMP("anc_buff", anc_buff,   _ma_get_page_used(share, anc_buff));
  DBUG_DUMP("leaf_buff", leaf_buff, _ma_get_page_used(share, leaf_buff));

  anc_page_flag= _ma_get_keypage_flag(share, anc_buff);
  buff=info->buff;
  info->keyread_buff_used=1;
  next_keypos=keypos;
  nod_flag= _ma_test_if_nod(share, leaf_buff);
  p_length= nod_flag+share->keypage_header;
  anc_length= _ma_get_page_used(share, anc_buff);
  leaf_length= _ma_get_page_used(share, leaf_buff);
  key_reflength=share->base.key_reflength;
  if (share->keyinfo+info->lastinx == keyinfo)
    info->page_changed=1;
  first_key= keypos == anc_buff + share->keypage_header + key_reflength;

  tmp_key.data=  buff;
  anc_key.data=  anc_key_buff;
  leaf_key.data= leaf_key_buff;
  tmp_key.keyinfo= leaf_key.keyinfo= anc_key.keyinfo= keyinfo;

  if ((keypos < anc_buff + anc_length && (info->state->records & 1)) ||
      first_key)
  {
    size_t tmp_length;
    uint next_page_flag;
    /* Use page right of anc-page */
    DBUG_PRINT("test",("use right page"));

    /*
      Calculate position after the current key. Note that keydata itself is
      not used
    */
    if (keyinfo->flag & HA_BINARY_PACK_KEY)
    {
      if (!(next_keypos= _ma_get_key(&tmp_key, anc_buff, keypos)))
	goto err;
    }
    else
    {
      /* Got to end of found key */
      buff[0]=buff[1]=0;	/* Avoid length error check if packed key */
      if (!(*keyinfo->get_key)(&tmp_key, anc_page_flag, key_reflength,
                               &next_keypos))
        goto err;
    }
    next_page= _ma_kpos(key_reflength,next_keypos);
    if (!_ma_fetch_keypage(info,keyinfo, next_page, PAGECACHE_LOCK_WRITE,
                           DFLT_INIT_HITS, buff, 0, &next_page_link))
      goto err;
    next_buff_length= _ma_get_page_used(share, buff);
    next_page_flag=   _ma_get_keypage_flag(share,buff);
    DBUG_DUMP("next", buff, next_buff_length);

    /* find keys to make a big key-page */
    bmove(next_keypos-key_reflength, buff + share->keypage_header,
          key_reflength);

    if (!_ma_get_last_key(&anc_key, anc_buff, next_keypos) ||
	!_ma_get_last_key(&leaf_key, leaf_buff, leaf_buff+leaf_length))
      goto err;

    /* merge pages and put parting key from anc_buff between */
    prev_key= (leaf_length == p_length ? (uchar*) 0 : leaf_key.data);
    t_length= (*keyinfo->pack_key)(&anc_key, nod_flag, buff+p_length,
                                   prev_key, prev_key, &key_inserted);
    tmp_length= next_buff_length - p_length;
    endpos= buff+tmp_length+leaf_length+t_length;
    /* buff will always be larger than before !*/
    bmove_upp(endpos, buff + next_buff_length, tmp_length);
    memcpy(buff, leaf_buff,(size_t) leaf_length);
    (*keyinfo->store_key)(keyinfo, buff+leaf_length, &key_inserted);
    buff_length= (uint) (endpos-buff);
    _ma_store_page_used(share, buff, buff_length);

    /* Set page flag from combination of both key pages and parting key */
    page_flag= (next_page_flag |
                _ma_get_keypage_flag(share, leaf_buff));
    if (anc_key.flag & (SEARCH_USER_KEY_HAS_TRANSID |
                        SEARCH_PAGE_KEY_HAS_TRANSID))
      page_flag|= KEYPAGE_FLAG_HAS_TRANSID;
    _ma_store_keypage_flag(share, buff, page_flag);

    /* remove key from anc_buff */
    if (!(s_length=remove_key(keyinfo, anc_page_flag, key_reflength, keypos,
                              anc_key_buff, anc_buff+anc_length,
                              (my_off_t *) 0, &key_deleted)))
      goto err;

    new_anc_length= anc_length - s_length;
    _ma_store_page_used(share, anc_buff, new_anc_length);

    if (buff_length <= (uint) (keyinfo->block_length - KEYPAGE_CHECKSUM_SIZE))
    {
      /* All keys fitted into one page */
      next_page_link->changed= 1;
      if (_ma_dispose(info, next_page, 0))
       goto err;

      memcpy(leaf_buff, buff, (size_t) buff_length);

      if (share->now_transactional)
      {
        /* Log changes to parent page */
        if (_ma_log_delete(info, anc_page, anc_buff, key_deleted.key_pos,
                           key_deleted.changed_length,
                           key_deleted.move_length))
          goto err;
        /*
          Log changes to leaf page. Data for leaf page is in buff
          which contains original leaf_buff, parting key and next_buff
        */
        if (_ma_log_suffix(info, leaf_page, leaf_buff,
                           leaf_length, buff_length))
          goto err;
      }
    }
    else
    {
      /*
        Balancing didn't free a page, so we have to split 'buff' into two
        pages:
        - Find key in middle of buffer
        - Store everything before key in 'leaf_buff'
        - Pack key into anc_buff at position of deleted key
          Note that anc_buff may overflow! (is handled by caller)
        - Store remaining keys in next_page (buff)
      */
      MARIA_KEY_PARAM anc_key_inserted;

      anc_end_pos= anc_buff + new_anc_length;

      DBUG_PRINT("test",("anc_buff: 0x%lx  anc_end_pos: 0x%lx",
                         (long) anc_buff, (long) anc_end_pos));

      if (!first_key && !_ma_get_last_key(&anc_key, anc_buff, keypos))
	goto err;
      if (!(half_pos= _ma_find_half_pos(info, &leaf_key, nod_flag, buff,
                                        &after_key)))
	goto err;
      new_leaf_length= (uint) (half_pos-buff);
      memcpy(leaf_buff, buff, (size_t) new_leaf_length);
      _ma_store_page_used(share, leaf_buff, new_leaf_length);
      _ma_store_keypage_flag(share, leaf_buff, page_flag);

      /* Correct new keypointer to leaf_page */
      half_pos=after_key;
      _ma_kpointer(info,
                   leaf_key.data+leaf_key.data_length + leaf_key.ref_length,
                   next_page);

      /* Save key in anc_buff */
      prev_key= (first_key  ? (uchar*) 0 : anc_key.data);
      t_length= (*keyinfo->pack_key)(&leaf_key, key_reflength,
                                     (keypos == anc_end_pos ? (uchar*) 0 :
                                      keypos),
                                     prev_key, prev_key, &anc_key_inserted);
      if (t_length >= 0)
	bmove_upp(anc_end_pos+t_length, anc_end_pos,
                  (uint) (anc_end_pos - keypos));
      else
	bmove(keypos,keypos-t_length,(uint) (anc_end_pos-keypos)+t_length);
      (*keyinfo->store_key)(keyinfo,keypos, &anc_key_inserted);
      new_anc_length+= t_length;
      _ma_store_page_used(share, anc_buff, new_anc_length);
      if (leaf_key.flag & (SEARCH_USER_KEY_HAS_TRANSID |
                           SEARCH_PAGE_KEY_HAS_TRANSID))
        _ma_mark_page_with_transid(share, anc_buff);

      /* Store key first in new page */
      if (nod_flag)
	bmove(buff+share->keypage_header, half_pos-nod_flag,
              (size_t) nod_flag);
      if (!(*keyinfo->get_key)(&leaf_key, page_flag, nod_flag, &half_pos))
	goto err;
      t_length=(int) (*keyinfo->pack_key)(&leaf_key, nod_flag, (uchar*) 0,
					  (uchar*) 0, (uchar*) 0,
					  &key_inserted);
      /* t_length will always be > 0 for a new page !*/
      tmp_length= (size_t) ((buff + buff_length) - half_pos);
      bmove(buff+p_length+t_length, half_pos, tmp_length);
      (*keyinfo->store_key)(keyinfo,buff+p_length, &key_inserted);
      new_buff_length= tmp_length + t_length + p_length;
      _ma_store_page_used(share, buff, new_buff_length);
      /* keypage flag is already up to date */

      if (share->now_transactional)
      {
        /*
          Log changes to parent page
          This has one key deleted from it and one key inserted to it at
          keypos

          ma_log_add ensures that we don't log changes that is outside of
          key block size, as the REDO code can't handle that
        */
        if (_ma_log_add(info, anc_page, anc_buff, anc_length,
                        keypos,
                        anc_key_inserted.move_length +
                        max(anc_key_inserted.changed_length -
                            anc_key_inserted.move_length,
                            key_deleted.changed_length),
                        anc_key_inserted.move_length -
                        key_deleted.move_length, 1))
          goto err;

        /*
          Log changes to leaf page.
          This contains original data with new data added at end
        */
        DBUG_ASSERT(leaf_length <= new_leaf_length);
        if (_ma_log_suffix(info, leaf_page, leaf_buff, leaf_length,
                           new_leaf_length))
          goto err;
        /*
          Log changes to next page

          This contains original data with some prefix data deleted and
          some compressed data at start possible extended

          Data in buff was originally:
          org_leaf_buff     [leaf_length]
          separator_key     [buff_key_inserted.move_length]
          next_key_changes  [buff_key_inserted.changed_length -move_length]
          next_page_data    [next_buff_length - p_length -
                            (buff_key_inserted.changed_length -move_length)]

          After changes it's now:
          unpacked_key      [key_inserted.changed_length]
          next_suffix       [next_buff_length - key_inserted.changed_length]

        */
        DBUG_ASSERT(new_buff_length <= next_buff_length);
        if (_ma_log_prefix(info, next_page, buff,
                           key_inserted.changed_length,
                           (int) (new_buff_length - next_buff_length)))
          goto err;
      }
      next_page_link->changed= 1;
      if (_ma_write_keypage(info, keyinfo, next_page,
                            PAGECACHE_LOCK_LEFT_WRITELOCKED, DFLT_INIT_HITS,
                            buff))
	goto err;
    }

    leaf_page_link->changed= 1;
    if (_ma_write_keypage(info, keyinfo, leaf_page,
                          PAGECACHE_LOCK_LEFT_WRITELOCKED, DFLT_INIT_HITS,
                          leaf_buff))
      goto err;
    DBUG_RETURN(new_anc_length <=
                ((info->quick_mode ? MARIA_MIN_KEYBLOCK_LENGTH :
                  (uint) keyinfo->underflow_block_length)));
  }

  DBUG_PRINT("test",("use left page"));

  keypos= _ma_get_last_key(&anc_key, anc_buff, keypos);
  if (!keypos)
    goto err;
  next_page= _ma_kpos(key_reflength,keypos);
  if (!_ma_fetch_keypage(info, keyinfo, next_page, PAGECACHE_LOCK_WRITE,
                         DFLT_INIT_HITS, buff, 0, &next_page_link))
    goto err;
  buff_length= _ma_get_page_used(share, buff);
  endpos= buff + buff_length;
  DBUG_DUMP("prev",buff,buff_length);

  /* find keys to make a big key-page */
  bmove(next_keypos - key_reflength, leaf_buff + share->keypage_header,
        key_reflength);
  next_keypos=keypos;
  if (!(*keyinfo->get_key)(&anc_key, anc_page_flag, key_reflength,
                           &next_keypos))
    goto err;
  if (!_ma_get_last_key(&leaf_key, buff, endpos))
    goto err;

  /* merge pages and put parting key from anc_buff between */
  prev_key= (leaf_length == p_length ? (uchar*) 0 : leaf_key.data);
  t_length=(*keyinfo->pack_key)(&anc_key, nod_flag,
				(leaf_length == p_length ?
                                 (uchar*) 0 : leaf_buff+p_length),
				prev_key, prev_key,
				&key_inserted);
  if (t_length >= 0)
    bmove(endpos+t_length, leaf_buff+p_length,
          (size_t) (leaf_length-p_length));
  else						/* We gained space */
    bmove(endpos,leaf_buff+((int) p_length-t_length),
	  (size_t) (leaf_length-p_length+t_length));
  (*keyinfo->store_key)(keyinfo,endpos, &key_inserted);

  /* Remember for logging how many bytes of leaf_buff that are not changed */
  DBUG_ASSERT((int) key_inserted.changed_length >= key_inserted.move_length);
  unchanged_leaf_length= leaf_length - (key_inserted.changed_length -
                                        key_inserted.move_length);

  new_buff_length= buff_length + leaf_length - p_length + t_length;
  _ma_store_page_used(share, buff, new_buff_length);

  page_flag= (_ma_get_keypage_flag(share, buff) |
              _ma_get_keypage_flag(share, leaf_buff));
  if (anc_key.flag & (SEARCH_USER_KEY_HAS_TRANSID |
                       SEARCH_PAGE_KEY_HAS_TRANSID))
    page_flag|= KEYPAGE_FLAG_HAS_TRANSID;
  _ma_store_keypage_flag(share, buff, page_flag);

  /* remove key from anc_buff */
  if (!(s_length= remove_key(keyinfo, anc_page_flag, key_reflength, keypos,
                             anc_key_buff,
                             anc_buff+anc_length, (my_off_t *) 0,
                             &key_deleted)))
    goto err;

  new_anc_length= anc_length - s_length;
  _ma_store_page_used(share, anc_buff, new_anc_length);

  if (new_buff_length <= (uint) (keyinfo->block_length -
                                 KEYPAGE_CHECKSUM_SIZE))
  {
    /* All keys fitted into one page */
    leaf_page_link->changed= 1;
    if (_ma_dispose(info, leaf_page, 0))
      goto err;

    if (share->now_transactional)
    {
      /* Log changes to parent page */
      if (_ma_log_delete(info, anc_page, anc_buff, key_deleted.key_pos,
                         key_deleted.changed_length, key_deleted.move_length))

        goto err;
      /*
        Log changes to next page. Data for leaf page is in buff
        that contains original leaf_buff, parting key and next_buff
      */
      if (_ma_log_suffix(info, next_page, buff,
                         buff_length, new_buff_length))
        goto err;
    }
  }
  else
  {
    /*
      Balancing didn't free a page, so we have to split 'buff' into two
      pages
      - Find key in middle of buffer (buff)
      - Pack key at half_buff into anc_buff at position of deleted key
        Note that anc_buff may overflow! (is handled by caller)
      - Move everything after middlekey to 'leaf_buff'
      - Shorten buff at 'endpos'
    */
    MARIA_KEY_PARAM anc_key_inserted;
    size_t tmp_length;

    if (keypos == anc_buff + share->keypage_header + key_reflength)
      anc_pos= 0;				/* First key */
    else
    {
      if (!_ma_get_last_key(&anc_key, anc_buff, keypos))
        goto err;
      anc_pos= anc_key.data;
    }
    if (!(endpos= _ma_find_half_pos(info, &leaf_key, nod_flag, buff,
                                    &half_pos)))
      goto err;

    /* Correct new keypointer to leaf_page */
    _ma_kpointer(info,leaf_key.data + leaf_key.data_length +
                 leaf_key.ref_length, leaf_page);

    /* Save key in anc_buff */
    DBUG_DUMP("anc_buff", anc_buff, new_anc_length);
    DBUG_DUMP_KEY("key_to_anc", &leaf_key);
    anc_end_pos= anc_buff + new_anc_length;
    t_length=(*keyinfo->pack_key)(&leaf_key, key_reflength,
				  keypos == anc_end_pos ? (uchar*) 0
				  : keypos,
				  anc_pos, anc_pos,
				  &anc_key_inserted);
    if (t_length >= 0)
      bmove_upp(anc_end_pos+t_length, anc_end_pos,
                (uint) (anc_end_pos-keypos));
    else
      bmove(keypos,keypos-t_length,(uint) (anc_end_pos-keypos)+t_length);
    (*keyinfo->store_key)(keyinfo,keypos, &anc_key_inserted);
    new_anc_length+= t_length;
    _ma_store_page_used(share, anc_buff, new_anc_length);
    if (leaf_key.flag & (SEARCH_USER_KEY_HAS_TRANSID |
                         SEARCH_PAGE_KEY_HAS_TRANSID))
      _ma_mark_page_with_transid(share, anc_buff);

    /* Store first key on new page */
    if (nod_flag)
      bmove(leaf_buff + share->keypage_header, half_pos-nod_flag,
            (size_t) nod_flag);
    if (!(*keyinfo->get_key)(&leaf_key, page_flag, nod_flag, &half_pos))
      goto err;
    DBUG_DUMP_KEY("key_to_leaf", &leaf_key);
    t_length=(*keyinfo->pack_key)(&leaf_key, nod_flag, (uchar*) 0,
				  (uchar*) 0, (uchar*) 0, &key_inserted);
    /* t_length will always be > 0 for a new page !*/
    tmp_length= (size_t) ((buff + new_buff_length) - half_pos);
    DBUG_PRINT("info",("t_length: %d  length: %d",t_length, (int) tmp_length));
    bmove(leaf_buff+p_length+t_length, half_pos, tmp_length);
    (*keyinfo->store_key)(keyinfo,leaf_buff+p_length, &key_inserted);
    new_leaf_length= tmp_length + t_length + p_length;
    _ma_store_page_used(share, leaf_buff, new_leaf_length);
    _ma_store_keypage_flag(share, leaf_buff, page_flag);
    new_buff_length= (uint) (endpos - buff);
    _ma_store_page_used(share, buff, new_buff_length);

    if (share->now_transactional)
    {
      /*
        Log changes to parent page
        This has one key deleted from it and one key inserted to it at
        keypos

        ma_log_add() ensures that we don't log changes that is outside of
        key block size, as the REDO code can't handle that
      */
      if (_ma_log_add(info, anc_page, anc_buff, anc_length,
                      keypos,
                      anc_key_inserted.move_length +
                      max(anc_key_inserted.changed_length -
                          anc_key_inserted.move_length,
                          key_deleted.changed_length),
                      anc_key_inserted.move_length -
                      key_deleted.move_length, 1))
        goto err;

      /*
        Log changes to leaf page.
        This contains original data with new data added first
      */
      DBUG_ASSERT(leaf_length <= new_leaf_length);
      if (_ma_log_prefix(info, leaf_page, leaf_buff,
                         new_leaf_length - unchanged_leaf_length,
                         (int) (new_leaf_length - leaf_length)))
        goto err;
      /*
        Log changes to next page
        This contains original data with some suffix data deleted

      */
      DBUG_ASSERT(new_buff_length <= buff_length);
      if (_ma_log_suffix(info, next_page, buff,
                         buff_length, new_buff_length))
        goto err;
    }

    leaf_page_link->changed= 1;
    if (_ma_write_keypage(info, keyinfo, leaf_page,
                          PAGECACHE_LOCK_LEFT_WRITELOCKED, DFLT_INIT_HITS,
                          leaf_buff))
      goto err;
  }
  next_page_link->changed= 1;
  if (_ma_write_keypage(info, keyinfo, next_page,
                        PAGECACHE_LOCK_LEFT_WRITELOCKED, DFLT_INIT_HITS, buff))
    goto err;

  DBUG_RETURN(new_anc_length <=
              ((info->quick_mode ? MARIA_MIN_KEYBLOCK_LENGTH :
                (uint) keyinfo->underflow_block_length)));

err:
  DBUG_RETURN(-1);
} /* underflow */


/**
  @brief Remove a key from page

  @fn remove_key()
    keyinfo	          Key handle
    nod_flag              Length of node ptr
    keypos	          Where on page key starts
    lastkey	          Buffer for storing keys to be removed
    page_end	          Pointer to end of page
    next_block	          If <> 0 and node-page, this is set to address of
    		          next page
    s_temp	          Information about what changes was done one the page:
    s_temp.key_pos        Start of key
    s_temp.move_length    Number of bytes removed at keypos
    s_temp.changed_length Number of bytes changed at keypos

  @todo
    The current code doesn't handle the case that the next key may be
    packed better against the previous key if there is a case difference

  @return
  @retval 0  error
  @retval #  How many chars was removed
*/

static uint remove_key(MARIA_KEYDEF *keyinfo, uint page_flag, uint nod_flag,
		       uchar *keypos, uchar *lastkey,
		       uchar *page_end, my_off_t *next_block,
                       MARIA_KEY_PARAM *s_temp)
{
  int s_length;
  uchar *start;
  DBUG_ENTER("remove_key");
  DBUG_PRINT("enter", ("keypos: 0x%lx  page_end: 0x%lx",
                       (long) keypos, (long) page_end));

  start= s_temp->key_pos= keypos;
  s_temp->changed_length= 0;
  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)) &&
      !(page_flag & KEYPAGE_FLAG_HAS_TRANSID))
  {
    /* Static length key */
    s_length=(int) (keyinfo->keylength+nod_flag);
    if (next_block && nod_flag)
      *next_block= _ma_kpos(nod_flag,keypos+s_length);
  }
  else
  {
    /* Let keypos point at next key */
    MARIA_KEY key;

    /* Calculate length of key */
    key.keyinfo= keyinfo;
    key.data=    lastkey;
    if (!(*keyinfo->get_key)(&key, page_flag, nod_flag, &keypos))
      DBUG_RETURN(0);				/* Error */

    if (next_block && nod_flag)
      *next_block= _ma_kpos(nod_flag,keypos);
    s_length=(int) (keypos-start);
    if (keypos != page_end)
    {
      if (keyinfo->flag & HA_BINARY_PACK_KEY)
      {
	uchar *old_key= start;
	uint next_length,prev_length,prev_pack_length;

        /* keypos points here on start of next key */
	get_key_length(next_length,keypos);
	get_key_pack_length(prev_length,prev_pack_length,old_key);
	if (next_length > prev_length)
	{
          uint diff= (next_length-prev_length);
	  /* We have to copy data from the current key to the next key */
	  keypos-= diff + prev_pack_length;
	  store_key_length(keypos, prev_length);
          bmove(keypos + prev_pack_length, lastkey + prev_length, diff);
	  s_length=(int) (keypos-start);
          s_temp->changed_length= diff + prev_pack_length;
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
	  {
            /* Next key is based on deleted key */
            uint pack_length;
            uint diff= (next_length-prev_length);

            /* keypos points to data of next key (after key length) */
	    bmove(keypos - diff, lastkey + prev_length, diff);
	    rest_length+= diff;
	    pack_length= prev_length ? get_pack_length(rest_length): 0;
	    keypos-= diff + pack_length + prev_pack_length;
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
            s_temp->changed_length= diff + pack_length + prev_pack_length;
	  }
	}
      }
    }
  }
  end:
  bmove(start, start+s_length, (uint) (page_end-start-s_length));
  s_temp->move_length= s_length;
  DBUG_RETURN((uint) s_length);
} /* remove_key */


/****************************************************************************
  Logging of redos
****************************************************************************/

/**
   @brief log entry where some parts are deleted and some things are changed

   @fn _ma_log_delete()
   @param info		  Maria handler
   @param page	          Pageaddress for changed page
   @param buff		  Page buffer
   @param key_pos         Start of change area
   @param changed_length  How many bytes where changed at key_pos
   @param move_length     How many bytes where deleted at key_pos

*/

my_bool _ma_log_delete(MARIA_HA *info, my_off_t page, const uchar *buff,
                       const uchar *key_pos, uint changed_length,
                       uint move_length)
{
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 9 + 7], *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 3];
  MARIA_SHARE *share= info->s;
  uint translog_parts;
  uint offset= (uint) (key_pos - buff);
  DBUG_ENTER("_ma_log_delete");
  DBUG_PRINT("enter", ("page: %lu  changed_length: %u  move_length: %d",
                       (ulong) page, changed_length, move_length));
  DBUG_ASSERT(share->now_transactional && move_length);
  DBUG_ASSERT(offset + changed_length <= _ma_get_page_used(share, buff));

  /* Store address of new root page */
  page/= share->block_size;
  page_store(log_data + FILEID_STORE_SIZE, page);
  log_pos= log_data+ FILEID_STORE_SIZE + PAGE_STORE_SIZE;
  log_pos[0]= KEY_OP_OFFSET;
  int2store(log_pos+1, offset);
  log_pos[3]= KEY_OP_SHIFT;
  int2store(log_pos+4, -(int) move_length);
  log_pos+= 6;
  translog_parts= 1;
  if (changed_length)
  {
    log_pos[0]= KEY_OP_CHANGE;
    int2store(log_pos+1, changed_length);
    log_pos+= 3;
    translog_parts= 2;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    buff + offset;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= changed_length;
  }

#ifdef EXTRA_DEBUG_KEY_CHANGES
  {
    int page_length= _ma_get_page_used(share, buff);
    ha_checksum crc;
    crc= my_checksum(0, buff + LSN_STORE_SIZE, page_length - LSN_STORE_SIZE);
    log_pos[0]= KEY_OP_CHECK;
    int2store(log_pos+1, page_length);
    int4store(log_pos+3, crc);

    log_array[TRANSLOG_INTERNAL_PARTS + translog_parts].str= log_pos;
    log_array[TRANSLOG_INTERNAL_PARTS + translog_parts].length= 7;
    changed_length+= 7;
    translog_parts++;
  }
#endif

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos - log_data);

  if (translog_write_record(&lsn, LOGREC_REDO_INDEX,
                            info->trn, info,
                            (translog_size_t)
                            log_array[TRANSLOG_INTERNAL_PARTS + 0].length +
                            changed_length,
                            TRANSLOG_INTERNAL_PARTS + translog_parts,
                            log_array, log_data, NULL))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/****************************************************************************
  Logging of undos
****************************************************************************/

int _ma_write_undo_key_delete(MARIA_HA *info, const MARIA_KEY *key,
                              my_off_t new_root, LSN *res_lsn)
{
  MARIA_SHARE *share= info->s;
  uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE +
                 KEY_NR_STORE_SIZE + PAGE_STORE_SIZE], *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
  struct st_msg_to_write_hook_for_undo_key msg;
  enum translog_record_type log_type= LOGREC_UNDO_KEY_DELETE;
  uint keynr= key->keyinfo->key_nr;

  lsn_store(log_data, info->trn->undo_lsn);
  key_nr_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE, keynr);
  log_pos= log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE + KEY_NR_STORE_SIZE;

  /**
    @todo BUG if we had concurrent insert/deletes, reading state's key_root
    like this would be unsafe.
  */
  if (new_root != share->state.key_root[keynr])
  {
    my_off_t page;
    page= ((new_root == HA_OFFSET_ERROR) ? IMPOSSIBLE_PAGE_NO :
           new_root / share->block_size);
    page_store(log_pos, page);
    log_pos+= PAGE_STORE_SIZE;
    log_type= LOGREC_UNDO_KEY_DELETE_WITH_ROOT;
  }

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos - log_data);
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    key->data;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= (key->data_length +
                                                  key->ref_length);

  msg.root= &share->state.key_root[keynr];
  msg.value= new_root;
  /*
    set autoincrement to 1 if this is an auto_increment key
    This is only used if we are now in a rollback of a duplicate key
  */
  msg.auto_increment= share->base.auto_key == keynr + 1;

  return translog_write_record(res_lsn, log_type,
                               info->trn, info,
                               (translog_size_t)
                               (log_array[TRANSLOG_INTERNAL_PARTS + 0].length +
                                log_array[TRANSLOG_INTERNAL_PARTS + 1].length),
                               TRANSLOG_INTERNAL_PARTS + 2, log_array,
                               log_data + LSN_STORE_SIZE, &msg) ? -1 : 0;
}
