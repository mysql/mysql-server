/* Copyright (C) 2004-2008 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   Copyright (C) 2008-2009 Sun Microsystems, Inc.

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

/* Write a row to a MARIA table */

#include "ma_fulltext.h"
#include "ma_rt_index.h"
#include "trnman.h"
#include "ma_key_recover.h"
#include "ma_blockrec.h"

	/* Functions declared in this file */

static int w_search(MARIA_HA *info, uint32 comp_flag,
                    MARIA_KEY *key, my_off_t page,
		    MARIA_PAGE *father_page, uchar *father_keypos,
		    my_bool insert_last);
static int _ma_balance_page(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
			    MARIA_KEY *key, MARIA_PAGE *curr_page,
                            MARIA_PAGE *father_page,
                            uchar *father_key_pos, MARIA_KEY_PARAM *s_temp);
static uchar *_ma_find_last_pos(MARIA_KEY *int_key,
                                MARIA_PAGE *page, uchar **after_key);
static my_bool _ma_ck_write_tree(register MARIA_HA *info, MARIA_KEY *key);
static my_bool _ma_ck_write_btree(register MARIA_HA *info, MARIA_KEY *key);
static my_bool _ma_ck_write_btree_with_log(MARIA_HA *, MARIA_KEY *, my_off_t *,
                                           uint32);
static my_bool _ma_log_split(MARIA_PAGE *page, uint org_length,
                             uint new_length,
                             const uchar *key_pos,
                             uint key_length, int move_length,
                             enum en_key_op prefix_or_suffix,
                             const uchar *data, uint data_length,
                             uint changed_length);
static my_bool _ma_log_del_prefix(MARIA_PAGE *page,
                                  uint org_length, uint new_length,
                                  const uchar *key_pos, uint key_length,
                                  int move_length);
static my_bool _ma_log_key_middle(MARIA_PAGE *page,
                                  uint new_length,
                                  uint data_added_first,
                                  uint data_changed_first,
                                  uint data_deleted_last,
                                  const uchar *key_pos,
                                  uint key_length, int move_length);

/*
  @brief Default handler for returing position to new row

  @note
    This is only called for non transactional tables and not for block format
    which is why we use info->state here.
*/

MARIA_RECORD_POS _ma_write_init_default(MARIA_HA *info,
                                        const uchar *record
                                        __attribute__((unused)))
{
  return ((info->s->state.dellink != HA_OFFSET_ERROR &&
           !info->append_insert_at_end) ?
          info->s->state.dellink :
          info->state->data_file_length);
}

my_bool _ma_write_abort_default(MARIA_HA *info __attribute__((unused)))
{
  return 0;
}


/* Write new record to a table */

int maria_write(MARIA_HA *info, uchar *record)
{
  MARIA_SHARE *share= info->s;
  uint i;
  int save_errno;
  MARIA_RECORD_POS filepos;
  uchar *buff;
  my_bool lock_tree= share->lock_key_trees;
  my_bool fatal_error;
  MARIA_KEYDEF *keyinfo;
  DBUG_ENTER("maria_write");
  DBUG_PRINT("enter",("index_file: %d  data_file: %d",
                      share->kfile.file, info->dfile.file));

  DBUG_EXECUTE_IF("maria_pretend_crashed_table_on_usage",
                  maria_print_error(info->s, HA_ERR_CRASHED);
                  DBUG_RETURN(my_errno= HA_ERR_CRASHED););
  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    DBUG_RETURN(my_errno=EACCES);
  }
  if (_ma_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno);

  if (share->base.reloc == (ha_rows) 1 &&
      share->base.records == (ha_rows) 1 &&
      share->state.state.records == (ha_rows) 1)
  {						/* System file */
    my_errno=HA_ERR_RECORD_FILE_FULL;
    goto err2;
  }
  if (share->state.state.key_file_length >= share->base.margin_key_file_length)
  {
    my_errno=HA_ERR_INDEX_FILE_FULL;
    goto err2;
  }
  if (_ma_mark_file_changed(share))
    goto err2;

  /* Calculate and check all unique constraints */

  if (share->state.header.uniques)
  {
    for (i=0 ; i < share->state.header.uniques ; i++)
    {
      MARIA_UNIQUEDEF *def= share->uniqueinfo + i;
      ha_checksum unique_hash= _ma_unique_hash(share->uniqueinfo+i,record);
      if (maria_is_key_active(share->state.key_map, def->key))
      {
        if (_ma_check_unique(info, def, record,
                             unique_hash, HA_OFFSET_ERROR))
          goto err2;
      }
      else
        maria_unique_store(record+ share->keyinfo[def->key].seg->start,
                           unique_hash);
    }
  }

  /* Ensure we don't try to restore auto_increment if it doesn't change */
  info->last_auto_increment= ~(ulonglong) 0;

  if ((info->opt_flag & OPT_NO_ROWS))
    filepos= HA_OFFSET_ERROR;
  else
  {
    /*
      This may either calculate a record or, or write the record and return
      the record id
    */
    if ((filepos= (*share->write_record_init)(info, record)) ==
        HA_OFFSET_ERROR)
      goto err2;
  }

  /* Write all keys to indextree */
  buff= info->lastkey_buff2;
  for (i=0, keyinfo= share->keyinfo ; i < share->base.keys ; i++, keyinfo++)
  {
    MARIA_KEY int_key;
    if (maria_is_key_active(share->state.key_map, i))
    {
      my_bool local_lock_tree= (lock_tree &&
                                !(info->bulk_insert &&
                                  is_tree_inited(&info->bulk_insert[i])));
      if (local_lock_tree)
      {
	mysql_rwlock_wrlock(&keyinfo->root_lock);
	keyinfo->version++;
      }
      if (keyinfo->flag & HA_FULLTEXT )
      {
        if (_ma_ft_add(info,i, buff,record,filepos))
        {
	  if (local_lock_tree)
	    mysql_rwlock_unlock(&keyinfo->root_lock);
          DBUG_PRINT("error",("Got error: %d on write",my_errno));
          goto err;
        }
      }
      else
      {
        while (keyinfo->ck_insert(info,
                                  (*keyinfo->make_key)(info, &int_key, i,
                                                       buff, record, filepos,
                                                       info->trn->trid)))
        {
          TRN *blocker;
          DBUG_PRINT("error",("Got error: %d on write",my_errno));
          /*
            explicit check to filter out temp tables, they aren't
            transactional and don't have a proper TRN so the code
            below doesn't work for them.
            Also, filter out non-thread maria use, and table modified in
            the same transaction.
            At last, filter out non-dup-unique errors.
          */
          if (!local_lock_tree)
            goto err;
          if (info->dup_key_trid == info->trn->trid ||
              my_errno != HA_ERR_FOUND_DUPP_KEY)
          {
	    mysql_rwlock_unlock(&keyinfo->root_lock);
            goto err;
          }
          /* Different TrIDs: table must be transactional */
          DBUG_ASSERT(share->base.born_transactional);
          /*
            If transactions are disabled, and dup_key_trid is different from
            our TrID, it must be ALTER TABLE with dup_key_trid==0 (no
            transaction). ALTER TABLE does have MARIA_HA::TRN not dummy but
            puts TrID=0 in rows/keys.
          */
          DBUG_ASSERT(share->now_transactional ||
                      (info->dup_key_trid == 0));
          blocker= trnman_trid_to_trn(info->trn, info->dup_key_trid);
          /*
            if blocker TRN was not found, it means that the conflicting
            transaction was committed long time ago. It could not be
            aborted, as it would have to wait on the key tree lock
            to remove the conflicting key it has inserted.
          */
          if (!blocker || blocker->commit_trid != ~(TrID)0)
          { /* committed */
            if (blocker)
              mysql_mutex_unlock(& blocker->state_lock);
            mysql_rwlock_unlock(&keyinfo->root_lock);
            goto err;
          }
          mysql_rwlock_unlock(&keyinfo->root_lock);
          {
            /* running. now we wait */
            WT_RESOURCE_ID rc;
            int res;
            const char *old_proc_info; 

            rc.type= &ma_rc_dup_unique;
            /* TODO savepoint id when we'll have them */
            rc.value= (intptr)blocker;
            res= wt_thd_will_wait_for(info->trn->wt, blocker->wt, & rc);
            if (res != WT_OK)
            {
              mysql_mutex_unlock(& blocker->state_lock);
              my_errno= HA_ERR_LOCK_DEADLOCK;
              goto err;
            }
            old_proc_info= proc_info_hook(0,
                                          "waiting for a resource",
                                          __func__, __FILE__, __LINE__);
            res= wt_thd_cond_timedwait(info->trn->wt, & blocker->state_lock);
            proc_info_hook(0, old_proc_info, __func__, __FILE__, __LINE__);

            mysql_mutex_unlock(& blocker->state_lock);
            if (res != WT_OK)
            {
              my_errno= res == WT_TIMEOUT ? HA_ERR_LOCK_WAIT_TIMEOUT
                                          : HA_ERR_LOCK_DEADLOCK;
              goto err;
            }
          }
          mysql_rwlock_wrlock(&keyinfo->root_lock);
#ifndef MARIA_CANNOT_ROLLBACK
          keyinfo->version++;
#endif
        }
      }

      /* The above changed info->lastkey2. Inform maria_rnext_same(). */
      info->update&= ~HA_STATE_RNEXT_SAME;

      if (local_lock_tree)
        mysql_rwlock_unlock(&keyinfo->root_lock);
    }
  }
  if (share->calc_write_checksum)
    info->cur_row.checksum= (*share->calc_write_checksum)(info,record);
  if (filepos != HA_OFFSET_ERROR)
  {
    if ((*share->write_record)(info,record))
      goto err;
    info->state->checksum+= info->cur_row.checksum;
  }
  if (!share->now_transactional)
  {
    if (share->base.auto_key != 0)
    {
      const HA_KEYSEG *keyseg= share->keyinfo[share->base.auto_key-1].seg;
      const uchar *key= record + keyseg->start;
      set_if_bigger(share->state.auto_increment,
                    ma_retrieve_auto_increment(key, keyseg->type));
    }
  }
  info->state->records++;
  info->update= (HA_STATE_CHANGED | HA_STATE_AKTIV | HA_STATE_WRITTEN |
		 HA_STATE_ROW_CHANGED);
  info->row_changes++;
  share->state.changed|= STATE_NOT_MOVABLE | STATE_NOT_ZEROFILLED;
  info->state->changed= 1;

  info->cur_row.lastpos= filepos;
  _ma_writeinfo(info, WRITEINFO_UPDATE_KEYFILE);
  if (info->invalidator != 0)
  {
    DBUG_PRINT("info", ("invalidator... '%s' (update)",
                        share->open_file_name.str));
    (*info->invalidator)(share->open_file_name.str);
    info->invalidator=0;
  }

  /*
    Update status of the table. We need to do so after each row write
    for the log tables, as we want the new row to become visible to
    other threads as soon as possible. We don't lock mutex here
    (as it is required by pthread memory visibility rules) as (1) it's
    not critical to use outdated share->is_log_table value (2) locking
    mutex here for every write is too expensive.
  */
  if (share->is_log_table)
    _ma_update_status((void*) info);

  DBUG_RETURN(0);

err:
  save_errno= my_errno;
  fatal_error= 0;
  if (my_errno == HA_ERR_FOUND_DUPP_KEY ||
      my_errno == HA_ERR_RECORD_FILE_FULL ||
      my_errno == HA_ERR_LOCK_DEADLOCK ||
      my_errno == HA_ERR_LOCK_WAIT_TIMEOUT ||
      my_errno == HA_ERR_NULL_IN_SPATIAL ||
      my_errno == HA_ERR_OUT_OF_MEM)
  {
    if (info->bulk_insert)
    {
      uint j;
      for (j=0 ; j < share->base.keys ; j++)
        maria_flush_bulk_insert(info, j);
    }
    info->errkey= (int) i;
    /*
      We delete keys in the reverse order of insertion. This is the order that
      a rollback would do and is important for CLR_ENDs generated by
      _ma_ft|ck_delete() and write_record_abort() to work (with any other
      order they would cause wrong jumps in the chain).
    */
    while ( i-- > 0)
    {
      if (maria_is_key_active(share->state.key_map, i))
      {
	my_bool local_lock_tree= (lock_tree &&
                                  !(info->bulk_insert &&
                                    is_tree_inited(&info->bulk_insert[i])));
        keyinfo= share->keyinfo + i;
	if (local_lock_tree)
	  mysql_rwlock_wrlock(&keyinfo->root_lock);
        /**
           @todo RECOVERY BUG
           The key deletes below should generate CLR_ENDs
        */
	if (keyinfo->flag & HA_FULLTEXT)
        {
          if (_ma_ft_del(info,i,buff,record,filepos))
	  {
	    if (local_lock_tree)
	      mysql_rwlock_unlock(&keyinfo->root_lock);
            break;
	  }
        }
        else
	{
	  MARIA_KEY key;
	  if (keyinfo->ck_delete(info,
                                 (*keyinfo->make_key)(info, &key, i, buff,
                                                      record,
                                                      filepos,
                                                      info->trn->trid)))
	  {
	    if (local_lock_tree)
	      mysql_rwlock_unlock(&keyinfo->root_lock);
	    break;
	  }
	}
	if (local_lock_tree)
	  mysql_rwlock_unlock(&keyinfo->root_lock);
      }
    }
  }
  else
    fatal_error= 1;

  if ((*share->write_record_abort)(info))
    fatal_error= 1;
  if (fatal_error)
  {
    maria_print_error(info->s, HA_ERR_CRASHED);
    maria_mark_crashed(info);
  }

  info->update= (HA_STATE_CHANGED | HA_STATE_WRITTEN | HA_STATE_ROW_CHANGED);
  my_errno=save_errno;
err2:
  save_errno=my_errno;
  DBUG_ASSERT(save_errno);
  if (!save_errno)
    save_errno= HA_ERR_INTERNAL_ERROR;          /* Should never happen */
  DBUG_PRINT("error", ("got error: %d", save_errno));
  _ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);
  DBUG_RETURN(my_errno=save_errno);
} /* maria_write */


/*
  Write one key to btree

  TODO
    Remove this function and have bulk insert change keyinfo->ck_insert
    to point to the right function
*/

my_bool _ma_ck_write(MARIA_HA *info, MARIA_KEY *key)
{
  DBUG_ENTER("_ma_ck_write");

  if (info->bulk_insert &&
      is_tree_inited(&info->bulk_insert[key->keyinfo->key_nr]))
  {
    DBUG_RETURN(_ma_ck_write_tree(info, key));
  }
  DBUG_RETURN(_ma_ck_write_btree(info, key));
} /* _ma_ck_write */


/**********************************************************************
  Insert key into btree (normal case)
**********************************************************************/

static my_bool _ma_ck_write_btree(MARIA_HA *info, MARIA_KEY *key)
{
  my_bool error;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  my_off_t  *root= &info->s->state.key_root[keyinfo->key_nr];
  DBUG_ENTER("_ma_ck_write_btree");

  error= _ma_ck_write_btree_with_log(info, key, root,
                                     keyinfo->write_comp_flag | key->flag);
  if (info->ft1_to_ft2)
  {
    if (!error)
      error= _ma_ft_convert_to_ft2(info, key);
    delete_dynamic(info->ft1_to_ft2);
    my_free(info->ft1_to_ft2);
    info->ft1_to_ft2=0;
  }
  DBUG_RETURN(error);
} /* _ma_ck_write_btree */


/**
  @brief Write a key to the b-tree

  @retval 1   error
  @retval 0    ok
*/

static my_bool _ma_ck_write_btree_with_log(MARIA_HA *info, MARIA_KEY *key,
                                           my_off_t *root, uint32 comp_flag)
{
  MARIA_SHARE *share= info->s;
  LSN lsn= LSN_IMPOSSIBLE;
  int error;
  my_off_t new_root= *root;
  uchar key_buff[MARIA_MAX_KEY_BUFF];
  MARIA_KEY org_key; /* Set/used when now_transactional=TRUE */
  my_bool transactional= share->now_transactional;
  DBUG_ENTER("_ma_ck_write_btree_with_log");
  
  LINT_INIT_STRUCT(org_key);

  if (transactional)
  {
    /* Save original value as the key may change */
    org_key= *key;
    memcpy(key_buff, key->data, key->data_length + key->ref_length);
  }

  error= _ma_ck_real_write_btree(info, key, &new_root, comp_flag);
  if (!error && transactional)
  {
    /* Log the original value */
    *key= org_key;
    key->data= key_buff;
    error= _ma_write_undo_key_insert(info, key, root, new_root, &lsn);
  }
  else
  {
    *root= new_root;
    _ma_fast_unlock_key_del(info);
  }
  _ma_unpin_all_pages_and_finalize_row(info, lsn);

  DBUG_RETURN(error != 0);
} /* _ma_ck_write_btree_with_log */


/**
  @brief Write a key to the b-tree

  @retval 1   error
  @retval 0    ok
*/

my_bool _ma_ck_real_write_btree(MARIA_HA *info, MARIA_KEY *key, my_off_t *root,
                            uint32 comp_flag)
{
  int error;
  DBUG_ENTER("_ma_ck_real_write_btree");

  /* key_length parameter is used only if comp_flag is SEARCH_FIND */
  if (*root == HA_OFFSET_ERROR ||
      (error= w_search(info, comp_flag, key, *root, (MARIA_PAGE *) 0,
                       (uchar*) 0, 1)) > 0)
    error= _ma_enlarge_root(info, key, root);
  DBUG_RETURN(error != 0);
} /* _ma_ck_real_write_btree */


/**
  @brief Make a new root with key as only pointer

  @retval 1   error
  @retval 0    ok
*/

my_bool _ma_enlarge_root(MARIA_HA *info, MARIA_KEY *key, my_off_t *root)
{
  uint t_length, nod_flag;
  MARIA_KEY_PARAM s_temp;
  MARIA_SHARE *share= info->s;
  MARIA_PINNED_PAGE tmp_page_link, *page_link= &tmp_page_link;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_PAGE page;
  my_bool res= 0;
  DBUG_ENTER("_ma_enlarge_root");

  page.info=    info;
  page.keyinfo= keyinfo;
  page.buff=    info->buff;
  page.flag=    0;

  nod_flag= (*root != HA_OFFSET_ERROR) ?  share->base.key_reflength : 0;
  /* Store pointer to prev page if nod */
  _ma_kpointer(info, page.buff + share->keypage_header, *root);
  t_length= (*keyinfo->pack_key)(key, nod_flag, (uchar*) 0,
                                 (uchar*) 0, (uchar*) 0, &s_temp);
  page.size= share->keypage_header + t_length + nod_flag;

  bzero(page.buff, share->keypage_header);
  _ma_store_keynr(share, page.buff, keyinfo->key_nr);
  if (nod_flag)
    page.flag|= KEYPAGE_FLAG_ISNOD;
  if (key->flag & (SEARCH_USER_KEY_HAS_TRANSID | SEARCH_PAGE_KEY_HAS_TRANSID))
    page.flag|= KEYPAGE_FLAG_HAS_TRANSID;
  (*keyinfo->store_key)(keyinfo, page.buff + share->keypage_header +
                        nod_flag, &s_temp);

  /* Mark that info->buff was used */
  info->keyread_buff_used= info->page_changed= 1;
  if ((page.pos= _ma_new(info, PAGECACHE_PRIORITY_HIGH, &page_link)) ==
      HA_OFFSET_ERROR)
    DBUG_RETURN(1);
  *root= page.pos;

  page_store_info(share, &page);

  /*
    Clear unitialized part of page to avoid valgrind/purify warnings
    and to get a clean page that is easier to compress and compare with
    pages generated with redo
  */
  bzero(page.buff + page.size, share->block_size - page.size);

  if (share->now_transactional && _ma_log_new(&page, 1))
    res= 1;

  if (_ma_write_keypage(&page, page_link->write_lock,
                        PAGECACHE_PRIORITY_HIGH))
    res= 1;

  DBUG_RETURN(res);
} /* _ma_enlarge_root */


/*
  Search after a position for a key and store it there

  TODO:
  Change this to use pagecache directly instead of creating a copy
  of the page. To do this, we must however change write-key-on-page
  algorithm to not overwrite the buffer but instead store any overflow
  key in a separate buffer.

  @return
  @retval -1   error
  @retval 0    ok
  @retval > 0  Key should be stored in higher tree
*/

static int w_search(register MARIA_HA *info, uint32 comp_flag, MARIA_KEY *key,
                    my_off_t page_pos,
                    MARIA_PAGE *father_page, uchar *father_keypos,
		    my_bool insert_last)
{
  int error,flag;
  uchar *temp_buff,*keypos;
  uchar keybuff[MARIA_MAX_KEY_BUFF];
  my_bool was_last_key;
  my_off_t next_page, dup_key_pos;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_PAGE page;
  DBUG_ENTER("w_search");
  DBUG_PRINT("enter", ("page: %lu", (ulong) (page_pos/keyinfo->block_length)));

  if (!(temp_buff= (uchar*) my_alloca((uint) keyinfo->block_length+
				      MARIA_MAX_KEY_BUFF*2)))
    DBUG_RETURN(-1);
  if (_ma_fetch_keypage(&page, info, keyinfo, page_pos, PAGECACHE_LOCK_WRITE,
                        DFLT_INIT_HITS, temp_buff, 0))
    goto err;

  flag= (*keyinfo->bin_search)(key, &page, comp_flag, &keypos,
                               keybuff, &was_last_key);
  if (flag == 0)
  {
    MARIA_KEY tmp_key;
    /* get position to record with duplicated key */

    tmp_key.keyinfo= keyinfo;
    tmp_key.data= keybuff;

    if ((*keyinfo->get_key)(&tmp_key, page.flag, page.node, &keypos))
      dup_key_pos= _ma_row_pos_from_key(&tmp_key);
    else
      dup_key_pos= HA_OFFSET_ERROR;

    if (keyinfo->flag & HA_FULLTEXT)
    {
      uint off;
      int  subkeys;

      get_key_full_length_rdonly(off, keybuff);
      subkeys=ft_sintXkorr(keybuff+off);
      comp_flag=SEARCH_SAME;
      if (subkeys >= 0)
      {
        /* normal word, one-level tree structure */
        flag=(*keyinfo->bin_search)(key, &page, comp_flag,
                                    &keypos, keybuff, &was_last_key);
      }
      else
      {
        /* popular word. two-level tree. going down */
        my_off_t root=dup_key_pos;
        keyinfo= &share->ft2_keyinfo;
        get_key_full_length_rdonly(off, key);
        key+=off;
        /* we'll modify key entry 'in vivo' */
        keypos-= keyinfo->keylength + page.node;
        error= _ma_ck_real_write_btree(info, key, &root, comp_flag);
        _ma_dpointer(share, keypos+HA_FT_WLEN, root);
        subkeys--; /* should there be underflow protection ? */
        DBUG_ASSERT(subkeys < 0);
        ft_intXstore(keypos, subkeys);
        if (!error)
        {
          page_mark_changed(info, &page);
          if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                                DFLT_INIT_HITS))
            goto err;
        }
        my_afree(temp_buff);
        DBUG_RETURN(error);
      }
    }
    else /* not HA_FULLTEXT, normal HA_NOSAME key */
    {
      /*
        TODO
        When the index will support true versioning - with multiple
        identical values in the UNIQUE index, invisible to each other -
        the following should be changed to "continue inserting keys, at the
        end (of the row or statement) wait". We need to wait on *all*
        unique conflicts at once, not one-at-a-time, because we need to
        know all blockers in advance, otherwise we'll have incomplete wait-for
        graph.
      */
      /*
        transaction that has inserted the conflicting key may be in progress.
        the caller will wait for it to be committed or aborted.
      */
      info->dup_key_trid= _ma_trid_from_key(&tmp_key);
      info->dup_key_pos= dup_key_pos;
      my_errno= HA_ERR_FOUND_DUPP_KEY;
      DBUG_PRINT("warning",
                 ("Duplicate key. dup_key_trid: %lu  pos %lu  visible: %d",
                  (ulong) info->dup_key_trid,
                  (ulong) info->dup_key_pos,
                  info->trn ? trnman_can_read_from(info->trn,
                                                   info->dup_key_trid) : 2));
      goto err;
    }
  }
  if (flag == MARIA_FOUND_WRONG_KEY)
    goto err;
  if (!was_last_key)
    insert_last=0;
  next_page= _ma_kpos(page.node, keypos);
  if (next_page == HA_OFFSET_ERROR ||
      (error= w_search(info, comp_flag, key, next_page,
                       &page, keypos, insert_last)) > 0)
  {
    error= _ma_insert(info, key, &page, keypos, keybuff,
                      father_page, father_keypos, insert_last);
    if (error < 0)
      goto err;
    page_mark_changed(info, &page);
    if (_ma_write_keypage(&page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                          DFLT_INIT_HITS))
      goto err;
  }
  my_afree(temp_buff);
  DBUG_RETURN(error);
err:
  my_afree(temp_buff);
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN(-1);
} /* w_search */


/*
  Insert new key.

  SYNOPSIS
    _ma_insert()
    info                        Open table information.
    keyinfo                     Key definition information.
    key                         New key
    anc_page                    Key page (beginning)
    key_pos                     Position in key page where to insert.
    key_buff                    Copy of previous key if keys where packed.
    father_page                 position of parent key page in file.
    father_key_pos              position in parent key page for balancing.
    insert_last                 If to append at end of page.

  DESCRIPTION
    Insert new key at right of key_pos.
    Note that caller must save anc_buff

    This function writes log records for all changed pages
    (Including anc_buff and father page)

  RETURN
    < 0         Error.
    0           OK
    1           If key contains key to upper level (from balance page)
    2           If key contains key to upper level (from split space)
*/

int _ma_insert(register MARIA_HA *info, MARIA_KEY *key,
               MARIA_PAGE *anc_page, uchar *key_pos, uchar *key_buff,
               MARIA_PAGE *father_page, uchar *father_key_pos,
               my_bool insert_last)
{
  uint a_length, nod_flag, org_anc_length;
  int t_length;
  uchar *endpos, *prev_key, *anc_buff;
  MARIA_KEY_PARAM s_temp;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  DBUG_ENTER("_ma_insert");
  DBUG_PRINT("enter",("key_pos: 0x%lx", (ulong) key_pos));
  DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE, key););

  /*
    Note that anc_page->size can be bigger then block_size in case of
    delete key that caused increase of page length
  */
  org_anc_length= a_length= anc_page->size;
  nod_flag= anc_page->node;

  anc_buff= anc_page->buff;
  endpos= anc_buff+ a_length;
  prev_key= (key_pos == anc_buff + share->keypage_header + nod_flag ?
             (uchar*) 0 : key_buff);
  t_length= (*keyinfo->pack_key)(key, nod_flag,
                                 (key_pos == endpos ? (uchar*) 0 : key_pos),
                                 prev_key, prev_key, &s_temp);
#ifndef DBUG_OFF
  if (prev_key && (keyinfo->flag & (HA_BINARY_PACK_KEY | HA_PACK_KEY)))
  {
    DBUG_DUMP("prev_key", prev_key, _ma_keylength(keyinfo,prev_key));
  }
  if (keyinfo->flag & HA_PACK_KEY)
  {
    DBUG_PRINT("test",("t_length: %d  ref_len: %d",
		       t_length,s_temp.ref_length));
    DBUG_PRINT("test",("n_ref_len: %d  n_length: %d  key_pos: 0x%lx",
		       s_temp.n_ref_length, s_temp.n_length, (long) s_temp.key));
  }
#endif
  if (t_length > 0)
  {
    if (t_length >= keyinfo->maxlength*2+MARIA_INDEX_OVERHEAD_SIZE)
    {
      _ma_set_fatal_error(share, HA_ERR_CRASHED);
      DBUG_RETURN(-1);
    }
    bmove_upp(endpos+t_length, endpos, (uint) (endpos-key_pos));
  }
  else
  {
    if (-t_length >= keyinfo->maxlength*2+MARIA_INDEX_OVERHEAD_SIZE)
    {
      _ma_set_fatal_error(share, HA_ERR_CRASHED);
      DBUG_RETURN(-1);
    }
    bmove(key_pos,key_pos-t_length,(uint) (endpos-key_pos)+t_length);
  }
  (*keyinfo->store_key)(keyinfo,key_pos,&s_temp);
  a_length+=t_length;

  if (key->flag & (SEARCH_USER_KEY_HAS_TRANSID | SEARCH_PAGE_KEY_HAS_TRANSID))
  {
    _ma_mark_page_with_transid(share, anc_page);
  }
  anc_page->size= a_length;
  page_store_size(share, anc_page);

  /*
    Check if the new key fits totally into the the page
    (anc_buff is big enough to contain a full page + one key)
  */
  if (a_length <= share->max_index_block_size)
  {
    if (share->max_index_block_size - a_length < 32 &&
        (keyinfo->flag & HA_FULLTEXT) && key_pos == endpos &&
        share->base.key_reflength <= share->rec_reflength &&
        share->options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD))
    {
      /*
        Normal word. One-level tree. Page is almost full.
        Let's consider converting.
        We'll compare 'key' and the first key at anc_buff
      */
      const uchar *a= key->data;
      const uchar *b= anc_buff + share->keypage_header + nod_flag;
      uint alen, blen, ft2len= share->ft2_keyinfo.keylength;
      /* the very first key on the page is always unpacked */
      DBUG_ASSERT((*b & 128) == 0);
#if HA_FT_MAXLEN >= 127
      blen= mi_uint2korr(b); b+=2;
      When you enable this code, as part of the MyISAM->Maria merge of
ChangeSet@1.2562, 2008-04-09 07:41:40+02:00, serg@janus.mylan +9 -0
  restore ft2 functionality, fix bugs.
      Then this will enable two-level fulltext index, which is not totally
      recoverable yet.
      So remove this text and inform Guilhem so that he fixes the issue.
#else
      blen= *b++;
#endif
      get_key_length(alen,a);
      DBUG_ASSERT(info->ft1_to_ft2==0);
      if (alen == blen &&
          ha_compare_text(keyinfo->seg->charset, a, alen,
                          b, blen, 0, 0) == 0)
      {
        /* Yup. converting */
        info->ft1_to_ft2=(DYNAMIC_ARRAY *)
          my_malloc(sizeof(DYNAMIC_ARRAY), MYF(MY_WME));
        my_init_dynamic_array(info->ft1_to_ft2, ft2len, 300, 50);

        /*
          Now, adding all keys from the page to dynarray
          if the page is a leaf (if not keys will be deleted later)
        */
        if (!nod_flag)
        {
          /*
            Let's leave the first key on the page, though, because
            we cannot easily dispatch an empty page here
          */
          b+=blen+ft2len+2;
          for (a=anc_buff+a_length ; b < a ; b+=ft2len+2)
            insert_dynamic(info->ft1_to_ft2, b);

          /* fixing the page's length - it contains only one key now */
          anc_page->size= share->keypage_header + blen + ft2len + 2;
          page_store_size(share, anc_page);
        }
        /* the rest will be done when we're back from recursion */
      }
    }
    else
    {
      if (share->now_transactional && 
          _ma_log_add(anc_page, org_anc_length,
                      key_pos, s_temp.changed_length, t_length, 1,
                      KEY_OP_DEBUG_LOG_ADD_1))
        DBUG_RETURN(-1);
    }
    DBUG_RETURN(0);				/* There is room on page */
  }
  /* Page is full */
  if (nod_flag)
    insert_last=0;
  /*
    TODO:
    Remove 'born_transactional' here.
    The only reason for having it here is that the current
    _ma_balance_page_ can't handle variable length keys.
  */
  if (!(keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)) &&
      father_page && !insert_last && !info->quick_mode &&
      !info->s->base.born_transactional)
  {
    s_temp.key_pos= key_pos;
    page_mark_changed(info, father_page);
    DBUG_RETURN(_ma_balance_page(info, keyinfo, key, anc_page,
                                 father_page, father_key_pos,
                                 &s_temp));
  }
  DBUG_RETURN(_ma_split_page(info, key, anc_page,
                             min(org_anc_length,
                                 info->s->max_index_block_size),
                             key_pos, s_temp.changed_length, t_length,
                             key_buff, insert_last));
} /* _ma_insert */


/**
  @brief split a full page in two and assign emerging item to key

  @fn _ma_split_page()
    info	     Maria handler
    keyinfo	     Key handler
    key		     Buffer for middle key
    split_page       Page that should be split
    org_split_length Original length of split_page before key was inserted
    inserted_key_pos Address in buffer where key was inserted
    changed_length   Number of bytes changed at 'inserted_key_pos'
    move_length	     Number of bytes buffer was moved when key was inserted
    key_buff	     Key buffer to use for temporary storage of key
    insert_last_key  If we are insert key on rightmost key page

  @note
    split_buff is not stored on disk    (caller has to do this)

  @return
  @retval 2   ok  (Middle key up from _ma_insert())
  @retval -1  error
*/

int _ma_split_page(MARIA_HA *info, MARIA_KEY *key, MARIA_PAGE *split_page,
                   uint org_split_length,
                   uchar *inserted_key_pos, uint changed_length,
                   int move_length,
                   uchar *key_buff, my_bool insert_last_key)
{
  uint length,a_length,key_ref_length,t_length,nod_flag,key_length;
  uint page_length, split_length, page_flag;
  uchar *key_pos,*pos, *after_key;
  MARIA_KEY_PARAM s_temp;
  MARIA_PINNED_PAGE tmp_page_link, *page_link= &tmp_page_link;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  MARIA_KEY tmp_key;
  MARIA_PAGE new_page;
  int res;
  DBUG_ENTER("_ma_split_page");

  LINT_INIT(after_key);
  DBUG_DUMP("buff", split_page->buff, split_page->size);

  info->page_changed=1;			/* Info->buff is used */
  info->keyread_buff_used=1;
  page_flag= split_page->flag;
  nod_flag=  split_page->node;
  key_ref_length= share->keypage_header + nod_flag;

  new_page.info= info;
  new_page.buff= info->buff;
  new_page.keyinfo= keyinfo;

  tmp_key.data=   key_buff;
  tmp_key.keyinfo= keyinfo;
  if (insert_last_key)
    key_pos= _ma_find_last_pos(&tmp_key, split_page, &after_key);
  else
    key_pos= _ma_find_half_pos(&tmp_key, split_page, &after_key);
  if (!key_pos)
    DBUG_RETURN(-1);

  key_length= tmp_key.data_length + tmp_key.ref_length;
  split_length= (uint) (key_pos - split_page->buff);
  a_length= split_page->size;
  split_page->size= split_length;
  page_store_size(share, split_page);

  key_pos=after_key;
  if (nod_flag)
  {
    DBUG_PRINT("test",("Splitting nod"));
    pos=key_pos-nod_flag;
    memcpy(new_page.buff + share->keypage_header, pos, (size_t) nod_flag);
  }

  /* Move middle item to key and pointer to new page */
  if ((new_page.pos= _ma_new(info, PAGECACHE_PRIORITY_HIGH, &page_link)) ==
      HA_OFFSET_ERROR)
    DBUG_RETURN(-1);

  _ma_copy_key(key, &tmp_key);
  _ma_kpointer(info, key->data + key_length, new_page.pos);

  /* Store new page */
  if (!(*keyinfo->get_key)(&tmp_key, page_flag, nod_flag, &key_pos))
    DBUG_RETURN(-1);

  t_length=(*keyinfo->pack_key)(&tmp_key, nod_flag, (uchar *) 0,
				(uchar*) 0, (uchar*) 0, &s_temp);
  length=(uint) ((split_page->buff + a_length) - key_pos);
  memcpy(new_page.buff + key_ref_length + t_length, key_pos,
	 (size_t) length);
  (*keyinfo->store_key)(keyinfo,new_page.buff+key_ref_length,&s_temp);
  page_length= length + t_length + key_ref_length;

  bzero(new_page.buff, share->keypage_header);
  /* Copy KEYFLAG_FLAG_ISNODE and KEYPAGE_FLAG_HAS_TRANSID from parent page */
  new_page.flag= page_flag;
  new_page.size= page_length;
  page_store_info(share, &new_page);

  /* Copy key number */
  new_page.buff[share->keypage_header - KEYPAGE_USED_SIZE -
                KEYPAGE_KEYID_SIZE - KEYPAGE_FLAG_SIZE]=
    split_page->buff[share->keypage_header - KEYPAGE_USED_SIZE -
                     KEYPAGE_KEYID_SIZE - KEYPAGE_FLAG_SIZE];

  res= 2;                                       /* Middle key up */
  if (share->now_transactional && _ma_log_new(&new_page, 0))
    res= -1;

  /*
    Clear unitialized part of page to avoid valgrind/purify warnings
    and to get a clean page that is easier to compress and compare with
    pages generated with redo
  */
  bzero(new_page.buff + page_length, share->block_size - page_length);

  if (_ma_write_keypage(&new_page, page_link->write_lock,
                        DFLT_INIT_HITS))
    res= -1;

  /* Save changes to split pages */
  if (share->now_transactional &&
      _ma_log_split(split_page, org_split_length, split_length,
                    inserted_key_pos, changed_length, move_length,
                    KEY_OP_NONE, (uchar*) 0, 0, 0))
    res= -1;

  DBUG_DUMP_KEY("middle_key", key);
  DBUG_RETURN(res);
} /* _ma_split_page */


/*
  Calculate how to much to move to split a page in two

  Returns pointer to start of key.
  key will contain the key.
  after_key will contain the position to where the next key starts
*/

uchar *_ma_find_half_pos(MARIA_KEY *key, MARIA_PAGE *ma_page,
                         uchar **after_key)
{
  uint keys, length, key_ref_length, page_flag, nod_flag;
  uchar *page, *end, *lastpos;
  MARIA_HA *info= ma_page->info;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  DBUG_ENTER("_ma_find_half_pos");

  nod_flag= ma_page->node;
  key_ref_length= share->keypage_header + nod_flag;
  page_flag= ma_page->flag;
  length=    ma_page->size - key_ref_length;
  page=      ma_page->buff+ key_ref_length;        /* Point to first key */

  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)) && !(page_flag & KEYPAGE_FLAG_HAS_TRANSID))
  {
    key_ref_length=   keyinfo->keylength+nod_flag;
    key->data_length= keyinfo->keylength - info->s->rec_reflength;
    key->ref_length=  info->s->rec_reflength;
    key->flag= 0;
    keys=length/(key_ref_length*2);
    end=page+keys*key_ref_length;
    *after_key=end+key_ref_length;
    memcpy(key->data, end, key_ref_length);
    DBUG_RETURN(end);
  }

  end=page+length/2-key_ref_length;		/* This is aprox. half */
  key->data[0]= 0;                               /* Safety */
  do
  {
    lastpos=page;
    if (!(length= (*keyinfo->get_key)(key, page_flag, nod_flag, &page)))
      DBUG_RETURN(0);
  } while (page < end);
  *after_key= page;
  DBUG_PRINT("exit",("returns: 0x%lx  page: 0x%lx  half: 0x%lx",
                     (long) lastpos, (long) page, (long) end));
  DBUG_RETURN(lastpos);
} /* _ma_find_half_pos */


/**
  Find second to last key on leaf page

  @notes
  Used to split buffer at last key.  In this case the next to last
  key will be moved to parent page and last key will be on it's own page.
  
  @TODO
  Add one argument for 'last key value' to get_key so that one can
  do the loop without having to copy the found key the whole time

  @return
  @retval Pointer to the start of the key before the last key
  @retval int_key will contain the last key
*/

static uchar *_ma_find_last_pos(MARIA_KEY *int_key, MARIA_PAGE *ma_page,
                                uchar **after_key)
{
  uint keys, length, key_ref_length, page_flag;
  uchar *page, *end, *lastpos, *prevpos;
  uchar key_buff[MARIA_MAX_KEY_BUFF];
  MARIA_HA *info= ma_page->info;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= int_key->keyinfo;
  MARIA_KEY tmp_key;
  DBUG_ENTER("_ma_find_last_pos");

  key_ref_length= share->keypage_header;
  page_flag= ma_page->flag;
  length= ma_page->size - key_ref_length;
  page=   ma_page->buff + key_ref_length;

  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)) && !(page_flag & KEYPAGE_FLAG_HAS_TRANSID))
  {
    keys= length / keyinfo->keylength - 2;
    length= keyinfo->keylength;
    int_key->data_length= length - info->s->rec_reflength;
    int_key->ref_length=  info->s->rec_reflength;
    int_key->flag= 0;
    end=page+keys*length;
    *after_key=end+length;
    memcpy(int_key->data, end, length);
    DBUG_RETURN(end);
  }

  end=page+length-key_ref_length;
  lastpos=page;
  tmp_key.data= key_buff;
  tmp_key.keyinfo= int_key->keyinfo;
  key_buff[0]= 0;                               /* Safety */

  /* We know that there are at least 2 keys on the page */

  if (!(length=(*keyinfo->get_key)(&tmp_key, page_flag, 0, &page)))
  {
    _ma_set_fatal_error(share, HA_ERR_CRASHED);
    DBUG_RETURN(0);
  }

  do
  {
    prevpos=lastpos; lastpos=page;
    int_key->data_length= tmp_key.data_length;
    int_key->ref_length=  tmp_key.ref_length;
    int_key->flag=        tmp_key.flag;
    memcpy(int_key->data, key_buff, length);		/* previous key */
    if (!(length=(*keyinfo->get_key)(&tmp_key, page_flag, 0, &page)))
    {
      _ma_set_fatal_error(share, HA_ERR_CRASHED);
      DBUG_RETURN(0);
    }
  } while (page < end);

  *after_key=lastpos;
  DBUG_PRINT("exit",("returns: 0x%lx  page: 0x%lx  end: 0x%lx",
                     (long) prevpos,(long) page,(long) end));
  DBUG_RETURN(prevpos);
} /* _ma_find_last_pos */


/**
  @brief Balance page with static size keys with page on right/left

  @param key 	Middle key will be stored here

  @notes
    Father_buff will always be changed
    Caller must handle saving of curr_buff

  @return
  @retval  0   Balance was done (father buff is saved)
  @retval  1   Middle key up    (father buff is not saved)
  @retval  -1  Error
*/

static int _ma_balance_page(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
			    MARIA_KEY *key, MARIA_PAGE *curr_page,
                            MARIA_PAGE *father_page,
                            uchar *father_key_pos, MARIA_KEY_PARAM *s_temp)
{
  MARIA_PINNED_PAGE tmp_page_link, *new_page_link= &tmp_page_link;
  MARIA_SHARE *share= info->s;
  my_bool right;
  uint k_length,father_length,father_keylength,nod_flag,curr_keylength;
  uint right_length,left_length,new_right_length,new_left_length,extra_length;
  uint keys, tmp_length, extra_buff_length;
  uchar *pos, *extra_buff, *parting_key;
  uchar tmp_part_key[MARIA_MAX_KEY_BUFF];
  MARIA_PAGE next_page, extra_page, *left_page, *right_page;
  DBUG_ENTER("_ma_balance_page");

  k_length= keyinfo->keylength;
  father_length= father_page->size;
  father_keylength= k_length + share->base.key_reflength;
  nod_flag= curr_page->node;
  curr_keylength= k_length+nod_flag;
  info->page_changed=1;

  if ((father_key_pos != father_page->buff+father_length &&
       (info->state->records & 1)) ||
      father_key_pos == father_page->buff+ share->keypage_header +
      share->base.key_reflength)
  {
    right=1;
    next_page.pos= _ma_kpos(share->base.key_reflength,
                            father_key_pos+father_keylength);
    left_page=  curr_page;
    right_page= &next_page;
    DBUG_PRINT("info", ("use right page: %lu",
                        (ulong) (next_page.pos / keyinfo->block_length)));
  }
  else
  {
    right=0;
    father_key_pos-=father_keylength;
    next_page.pos= _ma_kpos(share->base.key_reflength,father_key_pos);
    left_page=  &next_page;
    right_page= curr_page;
    DBUG_PRINT("info", ("use left page: %lu",
                        (ulong) (next_page.pos / keyinfo->block_length)));
  }					/* father_key_pos ptr to parting key */

  if (_ma_fetch_keypage(&next_page, info, keyinfo, next_page.pos,
                        PAGECACHE_LOCK_WRITE,
                        DFLT_INIT_HITS, info->buff, 0))
    goto err;
  page_mark_changed(info, &next_page);
  DBUG_DUMP("next", next_page.buff, next_page.size);

  /* Test if there is room to share keys */
  left_length= left_page->size;
  right_length= right_page->size;
  keys= ((left_length+right_length-share->keypage_header*2-nod_flag*2)/
         curr_keylength);

  if ((right ? right_length : left_length) + curr_keylength <=
      share->max_index_block_size)
  {
    /* Enough space to hold all keys in the two buffers ; Balance bufferts */
    new_left_length= share->keypage_header+nod_flag+(keys/2)*curr_keylength;
    new_right_length=share->keypage_header+nod_flag+(((keys+1)/2)*
                                                       curr_keylength);
    left_page->size=  new_left_length;
    page_store_size(share, left_page);
    right_page->size= new_right_length;
    page_store_size(share, right_page);

    DBUG_PRINT("info", ("left_length: %u -> %u  right_length: %u -> %u",
                        left_length, new_left_length,
                        right_length, new_right_length));
    if (left_length < new_left_length)
    {
      uint length;
      DBUG_PRINT("info", ("move keys to end of buff"));

      /* Move keys right_page -> left_page */
      pos= left_page->buff+left_length;
      memcpy(pos,father_key_pos, (size_t) k_length);
      memcpy(pos+k_length, right_page->buff + share->keypage_header,
	     (size_t) (length=new_left_length - left_length - k_length));
      pos= right_page->buff + share->keypage_header + length;
      memcpy(father_key_pos, pos, (size_t) k_length);
      bmove(right_page->buff + share->keypage_header,
            pos + k_length, new_right_length);

      if (share->now_transactional)
      {
        if (right)
        {
          /*
            Log changes to page on left
            The original page is on the left and stored in left_page->buff
            We have on the page the newly inserted key and data
            from buff added last on the page
          */
          if (_ma_log_split(curr_page,
                            left_length - s_temp->move_length,
                            new_left_length,
                            s_temp->key_pos, s_temp->changed_length,
                            s_temp->move_length,
                            KEY_OP_ADD_SUFFIX,
                            curr_page->buff + left_length,
                            new_left_length - left_length,
                            new_left_length - left_length+ k_length))
            goto err;
          /*
            Log changes to page on right
            This contains the original data with some keys deleted from
            start of page
          */
          if (_ma_log_prefix(&next_page, 0,
                             ((int) new_right_length - (int) right_length),
                             KEY_OP_DEBUG_LOG_PREFIX_3))
            goto err;
        }
        else
        {
          /*
            Log changes to page on right (the original page) which is in buff
            Data is removed from start of page
            The inserted key may be in buff or moved to curr_buff
          */
          if (_ma_log_del_prefix(curr_page,
                                 right_length - s_temp->changed_length,
                                 new_right_length,
                                 s_temp->key_pos, s_temp->changed_length,
                                 s_temp->move_length))
            goto err;
          /*
            Log changes to page on left, which has new data added last
          */
          if (_ma_log_suffix(&next_page, left_length, new_left_length))
            goto err;
        }
      }
    }
    else
    {
      uint length;
      DBUG_PRINT("info", ("move keys to start of right_page"));

      bmove_upp(right_page->buff + new_right_length,
                right_page->buff + right_length,
		right_length - share->keypage_header);
      length= new_right_length -right_length - k_length;
      memcpy(right_page->buff + share->keypage_header + length, father_key_pos,
             (size_t) k_length);
      pos= left_page->buff + new_left_length;
      memcpy(father_key_pos, pos, (size_t) k_length);
      memcpy(right_page->buff + share->keypage_header, pos+k_length,
             (size_t) length);

      if (share->now_transactional)
      {
        if (right)
        {
          /*
            Log changes to page on left
            The original page is on the left and stored in curr_buff
            The page is shortened from end and the key may be on the page
          */
          if (_ma_log_split(curr_page,
                            left_length - s_temp->move_length,
                            new_left_length,
                            s_temp->key_pos, s_temp->changed_length,
                            s_temp->move_length,
                            KEY_OP_NONE, (uchar*) 0, 0, 0))
            goto err;
          /*
            Log changes to page on right
            This contains the original data, with some data from cur_buff
            added first
          */
          if (_ma_log_prefix(&next_page,
                             (uint) (new_right_length - right_length),
                             (int) (new_right_length - right_length),
                             KEY_OP_DEBUG_LOG_PREFIX_4))
            goto err;
        }
        else
        {
          /*
            Log changes to page on right (the original page) which is in buff
            We have on the page the newly inserted key and data
            from buff added first on the page
          */
          uint diff_length= new_right_length - right_length;
          if (_ma_log_split(curr_page,
                            left_length - s_temp->move_length,
                            new_right_length,
                            s_temp->key_pos + diff_length,
                            s_temp->changed_length,
                            s_temp->move_length,
                            KEY_OP_ADD_PREFIX,
                            curr_page->buff + share->keypage_header,
                            diff_length, diff_length + k_length))
            goto err;
          /*
            Log changes to page on left, which is shortened from end
          */
          if (_ma_log_suffix(&next_page, left_length, new_left_length))
            goto err;
        }
      }
    }

    /* Log changes to father (one level up) page */

    if (share->now_transactional &&
        _ma_log_change(father_page, father_key_pos, k_length,
                       KEY_OP_DEBUG_FATHER_CHANGED_1))
      goto err;

    /*
      next_page_link->changed is marked as true above and fathers
      page_link->changed is marked as true in caller
    */
    if (_ma_write_keypage(&next_page, PAGECACHE_LOCK_LEFT_WRITELOCKED,
                          DFLT_INIT_HITS) ||
        _ma_write_keypage(father_page,
                          PAGECACHE_LOCK_LEFT_WRITELOCKED, DFLT_INIT_HITS))
      goto err;
    DBUG_RETURN(0);
  }

  /* left_page and right_page are full, lets split and make new nod */

  extra_buff= info->buff+share->base.max_key_block_length;
  new_left_length= new_right_length= (share->keypage_header + nod_flag +
                                      (keys+1) / 3 * curr_keylength);
  extra_page.info=    info;
  extra_page.keyinfo= keyinfo;
  extra_page.buff=    extra_buff;

  /*
    5 is the minum number of keys we can have here. This comes from
    the fact that each full page can store at least 2 keys and in this case
    we have a 'split' key, ie 2+2+1 = 5
  */
  if (keys == 5)				/* Too few keys to balance */
    new_left_length-=curr_keylength;
  extra_length= (nod_flag + left_length + right_length -
                 new_left_length - new_right_length - curr_keylength);
  extra_buff_length= extra_length + share->keypage_header;
  DBUG_PRINT("info",("left_length: %d  right_length: %d  new_left_length: %d  new_right_length: %d  extra_length: %d",
                     left_length, right_length,
                     new_left_length, new_right_length,
                     extra_length));

  left_page->size= new_left_length;
  page_store_size(share, left_page);
  right_page->size= new_right_length;
  page_store_size(share, right_page);

  bzero(extra_buff, share->keypage_header);
  extra_page.flag= nod_flag ? KEYPAGE_FLAG_ISNOD : 0;
  extra_page.size= extra_buff_length;
  page_store_info(share, &extra_page);

  /* Copy key number */
  extra_buff[share->keypage_header - KEYPAGE_USED_SIZE - KEYPAGE_KEYID_SIZE -
             KEYPAGE_FLAG_SIZE]= keyinfo->key_nr;

  /* move first largest keys to new page  */
  pos= right_page->buff + right_length-extra_length;
  memcpy(extra_buff + share->keypage_header, pos, extra_length);
  /* Zero old data from buffer */
  bzero(extra_buff + extra_buff_length,
        share->block_size - extra_buff_length);

  /* Save new parting key between buff and extra_buff */
  memcpy(tmp_part_key, pos-k_length,k_length);
  /* Make place for new keys */
  bmove_upp(right_page->buff + new_right_length, pos - k_length,
            right_length - extra_length - k_length - share->keypage_header);
  /* Copy keys from left page */
  pos= left_page->buff + new_left_length;
  memcpy(right_page->buff + share->keypage_header, pos + k_length,
         (size_t) (tmp_length= left_length - new_left_length - k_length));
  /* Copy old parting key */
  parting_key= right_page->buff + share->keypage_header + tmp_length;
  memcpy(parting_key, father_key_pos, (size_t) k_length);

  /* Move new parting keys up to caller */
  memcpy((right ? key->data : father_key_pos),pos,(size_t) k_length);
  memcpy((right ? father_key_pos : key->data),tmp_part_key, k_length);

  if ((extra_page.pos= _ma_new(info, DFLT_INIT_HITS, &new_page_link))
      == HA_OFFSET_ERROR)
    goto err;
  _ma_kpointer(info,key->data+k_length, extra_page.pos);
  /* This is safe as long we are using not keys with transid */
  key->data_length= k_length - info->s->rec_reflength;
  key->ref_length= info->s->rec_reflength;

  if (right)
  {
    /*
      Page order according to key values:
      orignal_page (curr_page = left_page), next_page (buff), extra_buff

      Move page positions so that we store data in extra_page where
      next_page was and next_page will be stored at the new position
    */
    swap_variables(my_off_t, extra_page.pos, next_page.pos);
  } 

  if (share->now_transactional)
  {
    if (right)
    {
      /*
        left_page is shortened,
        right_page is getting new keys at start and shortened from end.
        extra_page is new page

        Note that extra_page (largest key parts) will be stored at the
        place of the original 'right' page (next_page) and right page
        will be stored at the new page position

        This makes the log entries smaller as right_page contains all
        data to generate the data extra_buff
      */

      /*
        Log changes to page on left (page shortened page at end)
      */
      if (_ma_log_split(curr_page,
                        left_length - s_temp->move_length, new_left_length,
                        s_temp->key_pos, s_temp->changed_length,
                        s_temp->move_length,
                        KEY_OP_NONE, (uchar*) 0, 0, 0))
        goto err;
      /*
        Log changes to right page (stored at next page)
        This contains the last 'extra_buff' from 'buff'
      */
      if (_ma_log_prefix(&extra_page,
                         0, (int) (extra_buff_length - right_length),
                         KEY_OP_DEBUG_LOG_PREFIX_5))
        goto err;

      /*
        Log changes to middle page, which is stored at the new page
        position
      */
      if (_ma_log_new(&next_page, 0))
        goto err;
    }
    else
    {
      /*
        Log changes to page on right (the original page) which is in buff
        This contains the original data, with some data from curr_buff
        added first and shortened at end
      */
      int data_added_first= left_length - new_left_length;
      if (_ma_log_key_middle(right_page,
                             new_right_length,
                             data_added_first,
                             data_added_first,
                             extra_length,
                             s_temp->key_pos,
                             s_temp->changed_length,
                             s_temp->move_length))
        goto err;

      /* Log changes to page on left, which is shortened from end */
      if (_ma_log_suffix(left_page, left_length, new_left_length))
        goto err;

      /* Log change to rightmost (new) page */
      if (_ma_log_new(&extra_page, 0))
        goto err;
    }

    /* Log changes to father (one level up) page */
    if (share->now_transactional &&
        _ma_log_change(father_page, father_key_pos, k_length,
                       KEY_OP_DEBUG_FATHER_CHANGED_2))
      goto err;
  }

  if (_ma_write_keypage(&next_page,
                        (right ? new_page_link->write_lock :
                         PAGECACHE_LOCK_LEFT_WRITELOCKED),
                        DFLT_INIT_HITS) ||
      _ma_write_keypage(&extra_page,
                        (!right ? new_page_link->write_lock :
                         PAGECACHE_LOCK_LEFT_WRITELOCKED),
                        DFLT_INIT_HITS))
    goto err;

  DBUG_RETURN(1);				/* Middle key up */

err:
  DBUG_RETURN(-1);
} /* _ma_balance_page */


/**********************************************************************
 *                Bulk insert code                                    *
 **********************************************************************/

typedef struct {
  MARIA_HA *info;
  uint keynr;
} bulk_insert_param;


static my_bool _ma_ck_write_tree(register MARIA_HA *info, MARIA_KEY *key)
{
  my_bool error;
  uint keynr= key->keyinfo->key_nr;
  DBUG_ENTER("_ma_ck_write_tree");

  /* Store ref_length as this is always constant */
  info->bulk_insert_ref_length= key->ref_length;
  error= tree_insert(&info->bulk_insert[keynr], key->data,
                     key->data_length + key->ref_length,
                     info->bulk_insert[keynr].custom_arg) == 0;
  DBUG_RETURN(error);
} /* _ma_ck_write_tree */


/* typeof(_ma_keys_compare)=qsort_cmp2 */

static int keys_compare(bulk_insert_param *param, uchar *key1, uchar *key2)
{
  uint not_used[2];
  return ha_key_cmp(param->info->s->keyinfo[param->keynr].seg,
                    key1, key2, USE_WHOLE_KEY, SEARCH_SAME,
                    not_used);
}


static int keys_free(uchar *key, TREE_FREE mode, bulk_insert_param *param)
{
  /*
    Probably I can use info->lastkey here, but I'm not sure,
    and to be safe I'd better use local lastkey.
  */
  MARIA_SHARE *share= param->info->s;
  uchar lastkey[MARIA_MAX_KEY_BUFF];
  uint keylen;
  MARIA_KEYDEF *keyinfo= share->keyinfo + param->keynr;
  MARIA_KEY tmp_key;

  switch (mode) {
  case free_init:
    if (share->lock_key_trees)
    {
      mysql_rwlock_wrlock(&keyinfo->root_lock);
      keyinfo->version++;
    }
    return 0;
  case free_free:
    /* Note: keylen doesn't contain transid lengths */
    keylen= _ma_keylength(keyinfo, key);
    tmp_key.data=        lastkey;
    tmp_key.keyinfo=     keyinfo;
    tmp_key.data_length= keylen - share->rec_reflength;
    tmp_key.ref_length=  param->info->bulk_insert_ref_length;
    tmp_key.flag= (param->info->bulk_insert_ref_length ==
                   share->rec_reflength ? 0 : SEARCH_USER_KEY_HAS_TRANSID);
    /*
      We have to copy key as ma_ck_write_btree may need the buffer for
      copying middle key up if tree is growing
    */
    memcpy(lastkey, key, tmp_key.data_length + tmp_key.ref_length);
    return _ma_ck_write_btree(param->info, &tmp_key);
  case free_end:
    if (share->lock_key_trees)
      mysql_rwlock_unlock(&keyinfo->root_lock);
    return 0;
  }
  return 1;
}


int maria_init_bulk_insert(MARIA_HA *info, ulong cache_size, ha_rows rows)
{
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *key=share->keyinfo;
  bulk_insert_param *params;
  uint i, num_keys, total_keylength;
  ulonglong key_map;
  DBUG_ENTER("_ma_init_bulk_insert");
  DBUG_PRINT("enter",("cache_size: %lu", cache_size));

  DBUG_ASSERT(!info->bulk_insert &&
	      (!rows || rows >= MARIA_MIN_ROWS_TO_USE_BULK_INSERT));

  maria_clear_all_keys_active(key_map);
  for (i=total_keylength=num_keys=0 ; i < share->base.keys ; i++)
  {
    if (! (key[i].flag & HA_NOSAME) && (share->base.auto_key != i + 1) &&
        maria_is_key_active(share->state.key_map, i))
    {
      num_keys++;
      maria_set_key_active(key_map, i);
      total_keylength+=key[i].maxlength+TREE_ELEMENT_EXTRA_SIZE;
    }
  }

  if (num_keys==0 ||
      num_keys * MARIA_MIN_SIZE_BULK_INSERT_TREE > cache_size)
    DBUG_RETURN(0);

  if (rows && rows*total_keylength < cache_size)
    cache_size= (ulong)rows;
  else
    cache_size/=total_keylength*16;

  info->bulk_insert=(TREE *)
    my_malloc((sizeof(TREE)*share->base.keys+
               sizeof(bulk_insert_param)*num_keys),MYF(0));

  if (!info->bulk_insert)
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  params=(bulk_insert_param *)(info->bulk_insert+share->base.keys);
  for (i=0 ; i < share->base.keys ; i++)
  {
    if (maria_is_key_active(key_map, i))
    {
      params->info=info;
      params->keynr=i;
      /* Only allocate a 16'th of the buffer at a time */
      init_tree(&info->bulk_insert[i],
                cache_size * key[i].maxlength,
                cache_size * key[i].maxlength, 0,
		(qsort_cmp2)keys_compare, 0,
		(tree_element_free) keys_free, (void *)params++);
    }
    else
     info->bulk_insert[i].root=0;
  }

  DBUG_RETURN(0);
}

void maria_flush_bulk_insert(MARIA_HA *info, uint inx)
{
  if (info->bulk_insert)
  {
    if (is_tree_inited(&info->bulk_insert[inx]))
      reset_tree(&info->bulk_insert[inx]);
  }
}

void maria_end_bulk_insert(MARIA_HA *info)
{
  DBUG_ENTER("maria_end_bulk_insert");
  if (info->bulk_insert)
  {
    uint i;
    for (i=0 ; i < info->s->base.keys ; i++)
    {
      if (is_tree_inited(&info->bulk_insert[i]))
      {
        if (info->s->deleting)
          reset_free_element(&info->bulk_insert[i]);
        delete_tree(&info->bulk_insert[i]);
      }
    }
    my_free(info->bulk_insert);
    info->bulk_insert= 0;
  }
  DBUG_VOID_RETURN;
}


/****************************************************************************
  Dedicated functions that generate log entries
****************************************************************************/


int _ma_write_undo_key_insert(MARIA_HA *info, const MARIA_KEY *key,
                              my_off_t *root, my_off_t new_root, LSN *res_lsn)
{
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  uchar log_data[LSN_STORE_SIZE + FILEID_STORE_SIZE +
                 KEY_NR_STORE_SIZE];
  const uchar *key_value;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
  struct st_msg_to_write_hook_for_undo_key msg;
  uint key_length;

  /* Save if we need to write a clr record */
  lsn_store(log_data, info->trn->undo_lsn);
  key_nr_store(log_data + LSN_STORE_SIZE + FILEID_STORE_SIZE,
               keyinfo->key_nr);
  key_length= key->data_length + key->ref_length;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    key->data;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= key_length;

  msg.root= root;
  msg.value= new_root;
  msg.auto_increment= 0;
  key_value= key->data;
  if (share->base.auto_key == ((uint) keyinfo->key_nr + 1))
  {
    const HA_KEYSEG *keyseg= keyinfo->seg;
    uchar reversed[MARIA_MAX_KEY_BUFF];
    if (keyseg->flag & HA_SWAP_KEY)
    {
      /* We put key from log record to "data record" packing format... */
      const uchar *key_ptr= key->data, *key_end= key->data + keyseg->length;
      uchar *to= reversed + keyseg->length;
      do
      {
        *--to= *key_ptr++;
      } while (key_ptr != key_end);
      key_value= to;
    }
    /* ... so that we can read it with: */
    msg.auto_increment=
      ma_retrieve_auto_increment(key_value, keyseg->type);
    /* and write_hook_for_undo_key_insert() will pick this. */
  }

  return translog_write_record(res_lsn, LOGREC_UNDO_KEY_INSERT,
                               info->trn, info,
                               (translog_size_t)
                               log_array[TRANSLOG_INTERNAL_PARTS + 0].length +
                               key_length,
                               TRANSLOG_INTERNAL_PARTS + 2, log_array,
                               log_data + LSN_STORE_SIZE, &msg) ? -1 : 0;
}


/**
  @brief Log creation of new page

  @note
    We don't have to store the page_length into the log entry as we can
    calculate this from the length of the log entry

  @retval 1   error
  @retval 0    ok
*/

my_bool _ma_log_new(MARIA_PAGE *ma_page, my_bool root_page)
{
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2 + KEY_NR_STORE_SIZE
                 +1];
  uint page_length;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
  MARIA_HA *info= ma_page->info;
  MARIA_SHARE *share= info->s;
  my_off_t page= ma_page->pos / share->block_size;
  DBUG_ENTER("_ma_log_new");
  DBUG_PRINT("enter", ("page: %lu", (ulong) page));

  DBUG_ASSERT(share->now_transactional);

  /* Store address of new root page */
  page_store(log_data + FILEID_STORE_SIZE, page);

  /* Store link to next unused page */
  if (info->key_del_used == 2)
    page= 0;                                    /* key_del not changed */
  else
    page= ((share->key_del_current == HA_OFFSET_ERROR) ? IMPOSSIBLE_PAGE_NO :
           share->key_del_current / share->block_size);

  page_store(log_data + FILEID_STORE_SIZE + PAGE_STORE_SIZE, page);
  key_nr_store(log_data + FILEID_STORE_SIZE + PAGE_STORE_SIZE*2,
               ma_page->keyinfo->key_nr);
  log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE*2 + KEY_NR_STORE_SIZE]=
    (uchar) root_page;

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);

  page_length= ma_page->size - LSN_STORE_SIZE;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=   ma_page->buff + LSN_STORE_SIZE;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= page_length;

  /* Remember new page length for future log entires for same page */
  ma_page->org_size= ma_page->size;

  if (translog_write_record(&lsn, LOGREC_REDO_INDEX_NEW_PAGE,
                            info->trn, info,
                            (translog_size_t)
                            (sizeof(log_data) + page_length),
                            TRANSLOG_INTERNAL_PARTS + 2, log_array,
                            log_data, NULL))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/**
   @brief
   Log when some part of the key page changes
*/

my_bool _ma_log_change(MARIA_PAGE *ma_page, const uchar *key_pos, uint length,
                       enum en_key_debug debug_marker __attribute__((unused)))
{
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 2 + 6 + 7], *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 4];
  uint offset= (uint) (key_pos - ma_page->buff), translog_parts;
  MARIA_HA *info= ma_page->info;
  my_off_t page= ma_page->pos / info->s->block_size;
  DBUG_ENTER("_ma_log_change");
  DBUG_PRINT("enter", ("page: %lu  length: %u", (ulong) page, length));

  DBUG_ASSERT(info->s->now_transactional);
  DBUG_ASSERT(offset + length <= ma_page->size);
  DBUG_ASSERT(ma_page->org_size == ma_page->size);

  /* Store address of new root page */
  page= ma_page->pos / info->s->block_size;
  page_store(log_data + FILEID_STORE_SIZE, page);
  log_pos= log_data+ FILEID_STORE_SIZE + PAGE_STORE_SIZE;

#ifdef EXTRA_DEBUG_KEY_CHANGES
  (*log_pos++)= KEY_OP_DEBUG;
  (*log_pos++)= debug_marker;
#endif

  log_pos[0]= KEY_OP_OFFSET;
  int2store(log_pos+1, offset);
  log_pos[3]= KEY_OP_CHANGE;
  int2store(log_pos+4, length);
  log_pos+= 6;

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (log_pos - log_data);
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    key_pos;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= length;
  translog_parts= 2;

  _ma_log_key_changes(ma_page,
                      log_array + TRANSLOG_INTERNAL_PARTS + translog_parts,
                      log_pos, &length, &translog_parts);

  if (translog_write_record(&lsn, LOGREC_REDO_INDEX,
                            info->trn, info,
                            (translog_size_t) (log_pos - log_data) + length,
                            TRANSLOG_INTERNAL_PARTS + translog_parts,
                            log_array, log_data, NULL))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/**
   @brief Write log entry for page splitting

   @fn     _ma_log_split()
   @param
     ma_page		Page that is changed
     org_length	        Original length of page. Can be bigger than block_size
                        for block that overflowed
     new_length		New length of page
     key_pos		Where key is inserted on page (may be 0 if no key)
     key_length		Number of bytes changed at key_pos
     move_length	Number of bytes moved at key_pos to make room for key
     prefix_or_suffix   KEY_OP_NONE	    Ignored
   			KEY_OP_ADD_PREFIX   Add data to start of page
			KEY_OP_ADD_SUFFIX   Add data to end of page
     data		What data was added
     data_length	Number of bytes added first or last
     changed_length	Number of bytes changed first or last.

   @note
     Write log entry for page that has got a key added to the page under
     one and only one of the following senarios:
     - Page is shortened from end
     - Data is added to end of page
     - Data added at front of page
*/

static my_bool _ma_log_split(MARIA_PAGE *ma_page,
                             uint org_length, uint new_length,
                             const uchar *key_pos, uint key_length,
                             int move_length, enum en_key_op prefix_or_suffix,
                             const uchar *data, uint data_length,
                             uint changed_length)
{
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 2 + 2 + 3+3+3+3+3+2 +7];
  uchar *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 6];
  uint offset= (uint) (key_pos - ma_page->buff);
  uint translog_parts, extra_length;
  MARIA_HA *info= ma_page->info; 
  my_off_t page= ma_page->pos / info->s->block_size;
  DBUG_ENTER("_ma_log_split");
  DBUG_PRINT("enter", ("page: %lu  org_length: %u  new_length: %u",
                       (ulong) page, org_length, new_length));

  DBUG_ASSERT(changed_length >= data_length);
  DBUG_ASSERT(org_length <= info->s->max_index_block_size);
  DBUG_ASSERT(new_length == ma_page->size);
  DBUG_ASSERT(org_length == ma_page->org_size);

  log_pos= log_data + FILEID_STORE_SIZE;
  page_store(log_pos, page);
  log_pos+= PAGE_STORE_SIZE;

#ifdef EXTRA_DEBUG_KEY_CHANGES
  (*log_pos++)= KEY_OP_DEBUG;
  (*log_pos++)= KEY_OP_DEBUG_LOG_SPLIT;
#endif

  /* Store keypage_flag */
  *log_pos++= KEY_OP_SET_PAGEFLAG;
  *log_pos++= ma_page->buff[KEYPAGE_TRANSFLAG_OFFSET];

  if (new_length <= offset || !key_pos)
  {
    /*
      Page was split before inserted key. Write redo entry where
      we just cut current page at page_length
    */
    uint length_offset= org_length - new_length;
    log_pos[0]= KEY_OP_DEL_SUFFIX;
    int2store(log_pos+1, length_offset);
    log_pos+= 3;
    translog_parts= 1;
    extra_length= 0;
    DBUG_ASSERT(data_length == 0);
  }
  else
  {
    /* Key was added to page which was split after the inserted key */
    uint max_key_length;

    /*
      Handle case when split happened directly after the newly inserted key.
    */
    max_key_length= new_length - offset;
    extra_length= min(key_length, max_key_length);
    if (offset + move_length > new_length)
    {
      /* This is true when move_length includes changes for next packed key */
      move_length= new_length - offset;
    }

    if ((int) new_length < (int) (org_length + move_length + data_length))
    {
      /* Shorten page */
      uint diff= org_length + move_length + data_length - new_length;
      log_pos[0]= KEY_OP_DEL_SUFFIX;
      int2store(log_pos + 1, diff);
      log_pos+= 3;
      DBUG_ASSERT(data_length == 0);            /* Page is shortened */
      DBUG_ASSERT(offset <= org_length - diff);
    }
    else
    {
      DBUG_ASSERT(new_length == org_length + move_length + data_length);
      DBUG_ASSERT(offset <= org_length);
    }

    log_pos[0]= KEY_OP_OFFSET;
    int2store(log_pos+1, offset);
    log_pos+= 3;

    if (move_length)
    {
      log_pos[0]= KEY_OP_SHIFT;
      int2store(log_pos+1, move_length);
      log_pos+= 3;
    }

    log_pos[0]= KEY_OP_CHANGE;
    int2store(log_pos+1, extra_length);
    log_pos+= 3;

    /* Point to original inserted key data */
    if (prefix_or_suffix == KEY_OP_ADD_PREFIX)
      key_pos+= data_length;

    translog_parts= 2;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    key_pos;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= extra_length;
  }

  if (data_length)
  {
    /* Add prefix or suffix */
    log_pos[0]= prefix_or_suffix;
    int2store(log_pos+1, data_length);
    log_pos+= 3;
    if (prefix_or_suffix == KEY_OP_ADD_PREFIX)
    {
      int2store(log_pos+1, changed_length);
      log_pos+= 2;
      data_length= changed_length;
    }
    log_array[TRANSLOG_INTERNAL_PARTS + translog_parts].str=    data;
    log_array[TRANSLOG_INTERNAL_PARTS + translog_parts].length= data_length;
    translog_parts++;
    extra_length+= data_length;
  }

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                         log_data);

  _ma_log_key_changes(ma_page,
                      log_array + TRANSLOG_INTERNAL_PARTS + translog_parts,
                      log_pos, &extra_length, &translog_parts);
  /* Remember new page length for future log entires for same page */
  ma_page->org_size= ma_page->size;

  DBUG_RETURN(translog_write_record(&lsn, LOGREC_REDO_INDEX,
                                    info->trn, info,
                                    (translog_size_t)
                                    log_array[TRANSLOG_INTERNAL_PARTS +
                                              0].length + extra_length,
                                    TRANSLOG_INTERNAL_PARTS + translog_parts,
                                    log_array, log_data, NULL));
}


/**
   @brief
   Write log entry for page that has got a key added to the page
   and page is shortened from start of page

   @fn _ma_log_del_prefix()
   @param info		Maria handler
   @param page		Page number
   @param buff		Page buffer
   @param org_length	Length of buffer when read
   @param new_length	Final length
   @param key_pos	Where on page buffer key was added. This is position
			before prefix was removed
   @param key_length    How many bytes was changed at 'key_pos'
   @param move_length   How many bytes was moved up when key was added

   @return
   @retval  0  ok
   @retval  1  error
*/

static my_bool _ma_log_del_prefix(MARIA_PAGE *ma_page,
                                  uint org_length, uint new_length,
                                  const uchar *key_pos, uint key_length,
                                  int move_length)
{
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 2 + 2 + 12 + 7];
  uchar *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 4];
  uint offset= (uint) (key_pos - ma_page->buff);
  uint diff_length= org_length + move_length - new_length;
  uint translog_parts, extra_length;
  MARIA_HA *info= ma_page->info;
  my_off_t page= ma_page->pos / info->s->block_size;
  DBUG_ENTER("_ma_log_del_prefix");
  DBUG_PRINT("enter", ("page: %lu  org_length: %u  new_length: %u",
                       (ulong) page, org_length, new_length));

  DBUG_ASSERT((int) diff_length > 0);
  DBUG_ASSERT(ma_page->org_size == org_length);
  DBUG_ASSERT(ma_page->size == new_length);

  log_pos= log_data + FILEID_STORE_SIZE;
  page_store(log_pos, page);
  log_pos+= PAGE_STORE_SIZE;

  translog_parts= 1;
  extra_length= 0;

#ifdef EXTRA_DEBUG_KEY_CHANGES
  *log_pos++= KEY_OP_DEBUG;
  *log_pos++= KEY_OP_DEBUG_LOG_DEL_PREFIX;
#endif

  /* Store keypage_flag */
  *log_pos++= KEY_OP_SET_PAGEFLAG;
  *log_pos++= ma_page->buff[KEYPAGE_TRANSFLAG_OFFSET];

  if (offset < diff_length + info->s->keypage_header)
  {
    /*
      Key is not anymore on page. Move data down, but take into account that
      the original page had grown with 'move_length bytes'
    */
    DBUG_ASSERT(offset + key_length <= diff_length + info->s->keypage_header);

    log_pos[0]= KEY_OP_DEL_PREFIX;
    int2store(log_pos+1, diff_length - move_length);
    log_pos+= 3;
  }
  else
  {
    /*
      Correct position to key, as data before key has been delete and key
      has thus been moved down
    */
    offset-= diff_length;
    key_pos-= diff_length;

    /* Move data down */
    log_pos[0]= KEY_OP_DEL_PREFIX;
    int2store(log_pos+1, diff_length);
    log_pos+= 3;

    log_pos[0]= KEY_OP_OFFSET;
    int2store(log_pos+1, offset);
    log_pos+= 3;

    if (move_length)
    {
      log_pos[0]= KEY_OP_SHIFT;
      int2store(log_pos+1, move_length);
      log_pos+= 3;
    }
    log_pos[0]= KEY_OP_CHANGE;
    int2store(log_pos+1, key_length);
    log_pos+= 3;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    key_pos;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= key_length;
    translog_parts= 2;
    extra_length= key_length;
  }
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                         log_data);
  _ma_log_key_changes(ma_page,
                      log_array + TRANSLOG_INTERNAL_PARTS + translog_parts,
                      log_pos, &extra_length, &translog_parts);
  /* Remember new page length for future log entires for same page */
  ma_page->org_size= ma_page->size;

  DBUG_RETURN(translog_write_record(&lsn, LOGREC_REDO_INDEX,
                                    info->trn, info,
                                    (translog_size_t)
                                    log_array[TRANSLOG_INTERNAL_PARTS +
                                              0].length + extra_length,
                                    TRANSLOG_INTERNAL_PARTS + translog_parts,
                                    log_array, log_data, NULL));
}


/**
   @brief
   Write log entry for page that has got data added first and
   data deleted last. Old changed key may be part of page
*/

static my_bool _ma_log_key_middle(MARIA_PAGE *ma_page,
                                  uint new_length,
                                  uint data_added_first,
                                  uint data_changed_first,
                                  uint data_deleted_last,
                                  const uchar *key_pos,
                                  uint key_length, int move_length)
{
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 2 + 2 + 3+5+3+3+3 + 7];
  uchar *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 6];
  uint key_offset;
  uint translog_parts, extra_length;
  MARIA_HA *info= ma_page->info;
  my_off_t page= ma_page->pos / info->s->block_size;
  DBUG_ENTER("_ma_log_key_middle");
  DBUG_PRINT("enter", ("page: %lu", (ulong) page));

  DBUG_ASSERT(ma_page->size == new_length);

  /* new place of key after changes */
  key_pos+= data_added_first;
  key_offset= (uint) (key_pos - ma_page->buff);
  if (key_offset < new_length)
  {
    /* key is on page; Calculate how much of the key is there */
    uint max_key_length= new_length - key_offset;
    if (max_key_length < key_length)
    {
      /* Key is last on page */
      key_length= max_key_length;
      move_length= 0;
    }
    /*
      Take into account that new data was added as part of original key
      that also needs to be removed from page
    */
    data_deleted_last+= move_length;
  }

  /* First log changes to page */
  log_pos= log_data + FILEID_STORE_SIZE;
  page_store(log_pos, page);
  log_pos+= PAGE_STORE_SIZE;

#ifdef EXTRA_DEBUG_KEY_CHANGES
  *log_pos++= KEY_OP_DEBUG;
  *log_pos++= KEY_OP_DEBUG_LOG_MIDDLE;
#endif

  /* Store keypage_flag */
  *log_pos++= KEY_OP_SET_PAGEFLAG;
  *log_pos++= ma_page->buff[KEYPAGE_TRANSFLAG_OFFSET];

  log_pos[0]= KEY_OP_DEL_SUFFIX;
  int2store(log_pos+1, data_deleted_last);
  log_pos+= 3;

  log_pos[0]= KEY_OP_ADD_PREFIX;
  int2store(log_pos+1, data_added_first);
  int2store(log_pos+3, data_changed_first);
  log_pos+= 5;

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                         log_data);
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    (ma_page->buff +
                                                  info->s->keypage_header);
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= data_changed_first;
  translog_parts= 2;
  extra_length= data_changed_first;

  /* If changed key is on page, log those changes too */

  if (key_offset < new_length)
  {
    uchar *start_log_pos= log_pos;

    log_pos[0]= KEY_OP_OFFSET;
    int2store(log_pos+1, key_offset);
    log_pos+= 3;
    if (move_length)
    {
      log_pos[0]= KEY_OP_SHIFT;
      int2store(log_pos+1, move_length);
      log_pos+= 3;
    }
    log_pos[0]= KEY_OP_CHANGE;
    int2store(log_pos+1, key_length);
    log_pos+= 3;

    log_array[TRANSLOG_INTERNAL_PARTS + 2].str=    start_log_pos;
    log_array[TRANSLOG_INTERNAL_PARTS + 2].length= (uint) (log_pos -
                                                           start_log_pos);

    log_array[TRANSLOG_INTERNAL_PARTS + 3].str=    key_pos;
    log_array[TRANSLOG_INTERNAL_PARTS + 3].length= key_length;
    translog_parts+=2;
    extra_length+= (uint) (log_array[TRANSLOG_INTERNAL_PARTS + 2].length +
                           key_length);
  }

  _ma_log_key_changes(ma_page,
                      log_array + TRANSLOG_INTERNAL_PARTS + translog_parts,
                      log_pos, &extra_length, &translog_parts);
  /* Remember new page length for future log entires for same page */
  ma_page->org_size= ma_page->size;

  DBUG_RETURN(translog_write_record(&lsn, LOGREC_REDO_INDEX,
                                    info->trn, info,
                                    (translog_size_t)
                                    (log_array[TRANSLOG_INTERNAL_PARTS +
                                               0].length + extra_length),
                                    TRANSLOG_INTERNAL_PARTS + translog_parts,
                                    log_array, log_data, NULL));
}


#ifdef NOT_NEEDED

/**
   @brief
   Write log entry for page that has got data added first and
   data deleted last
*/

static my_bool _ma_log_middle(MARIA_PAGE *ma_page,
                              uint data_added_first, uint data_changed_first,
                              uint data_deleted_last)
{
  LSN lsn;
  LEX_STRING log_array[TRANSLOG_INTERNAL_PARTS + 4];
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 3 + 5 + 7], *log_pos;
  MARIA_HA *info= ma_page->info;
  my_off_t page= ma_page->page / info->s->block_size;
  uint translog_parts, extra_length;
  DBUG_ENTER("_ma_log_middle");
  DBUG_PRINT("enter", ("page: %lu", (ulong) page));

  DBUG_ASSERT(ma_page->org_size + data_added_first - data_deleted_last ==
              ma_page->size);

  log_pos= log_data + FILEID_STORE_SIZE;
  page_store(log_pos, page);
  log_pos+= PAGE_STORE_SIZE;

  log_pos[0]= KEY_OP_DEL_PREFIX;
  int2store(log_pos+1, data_deleted_last);
  log_pos+= 3;

  log_pos[0]= KEY_OP_ADD_PREFIX;
  int2store(log_pos+1, data_added_first);
  int2store(log_pos+3, data_changed_first);
  log_pos+= 5;

  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= (uint) (log_pos -
                                                         log_data);

  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    ((char*) buff +
                                                  info->s->keypage_header);
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= data_changed_first;
  translog_parts= 2;
  extra_length= data_changed_first;

  _ma_log_key_changes(ma_page,
                      log_array + TRANSLOG_INTERNAL_PARTS + translog_parts,
                      log_pos, &extra_length, &translog_parts);
  /* Remember new page length for future log entires for same page */
  ma_page->org_size= ma_page->size;

  DBUG_RETURN(translog_write_record(&lsn, LOGREC_REDO_INDEX,
                                    info->trn, info,
                                    (translog_size_t)
                                    log_array[TRANSLOG_INTERNAL_PARTS +
                                              0].length + extra_length,
                                    TRANSLOG_INTERNAL_PARTS + translog_parts,
                                    log_array, log_data, NULL));
}
#endif
