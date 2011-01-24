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

/**
   Update an old row in a MARIA table
*/

int maria_update(register MARIA_HA *info, const uchar *oldrec, uchar *newrec)
{
  int flag,key_changed,save_errno;
  reg3 my_off_t pos;
  uint i;
  uchar old_key_buff[MARIA_MAX_KEY_BUFF],*new_key_buff;
  my_bool auto_key_changed= 0;
  ulonglong changed;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo;
  DBUG_ENTER("maria_update");
  LINT_INIT(new_key_buff);
  LINT_INIT(changed);

  DBUG_EXECUTE_IF("maria_pretend_crashed_table_on_usage",
                  maria_print_error(info->s, HA_ERR_CRASHED);
                  DBUG_RETURN(my_errno= HA_ERR_CRASHED););
  if (!(info->update & HA_STATE_AKTIV))
  {
    DBUG_RETURN(my_errno=HA_ERR_KEY_NOT_FOUND);
  }
  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    DBUG_RETURN(my_errno=EACCES);
  }
  if (share->state.state.key_file_length >= share->base.margin_key_file_length)
  {
    DBUG_RETURN(my_errno=HA_ERR_INDEX_FILE_FULL);
  }
  pos= info->cur_row.lastpos;
  if (_ma_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno);

  if ((*share->compare_record)(info,oldrec))
  {
    save_errno= my_errno;
    DBUG_PRINT("warning", ("Got error from compare record"));
    goto err_end;			/* Record has changed */
  }

  /* Calculate and check all unique constraints */
  key_changed=0;
  for (i=0 ; i < share->state.header.uniques ; i++)
  {
    MARIA_UNIQUEDEF *def=share->uniqueinfo+i;
    if (_ma_unique_comp(def, newrec, oldrec,1) &&
	_ma_check_unique(info, def, newrec, _ma_unique_hash(def, newrec),
                         pos))
    {
      save_errno=my_errno;
      goto err_end;
    }
  }
  if (_ma_mark_file_changed(info))
  {
    save_errno=my_errno;
    goto err_end;
  }

  /* Ensure we don't try to restore auto_increment if it doesn't change */
  info->last_auto_increment= ~(ulonglong) 0;

  /* Check which keys changed from the original row */

  new_key_buff= info->lastkey_buff2;
  changed=0;
  for (i=0, keyinfo= share->keyinfo ; i < share->base.keys ; i++, keyinfo++)
  {
    if (maria_is_key_active(share->state.key_map, i))
    {
      if (keyinfo->flag & HA_FULLTEXT )
      {
	if (_ma_ft_cmp(info,i,oldrec, newrec))
	{
	  if ((int) i == info->lastinx)
	  {
	  /*
	    We are changeing the index we are reading on.  Mark that
	    the index data has changed and we need to do a full search
	    when doing read-next
	  */
	    key_changed|=HA_STATE_WRITTEN;
	  }
	  changed|=((ulonglong) 1 << i);
	  if (_ma_ft_update(info,i,old_key_buff,oldrec,newrec,pos))
	    goto err;
	}
      }
      else
      {
        MARIA_KEY new_key, old_key;

        (*keyinfo->make_key)(info,&new_key, i, new_key_buff, newrec,
                             pos, info->trn->trid);
        (*keyinfo->make_key)(info,&old_key, i, old_key_buff,
                             oldrec, pos, info->cur_row.trid);

        /* The above changed info->lastkey2. Inform maria_rnext_same(). */
        info->update&= ~HA_STATE_RNEXT_SAME;

	if (new_key.data_length != old_key.data_length ||
	    memcmp(old_key.data, new_key.data, new_key.data_length))
	{
	  if ((int) i == info->lastinx)
	    key_changed|=HA_STATE_WRITTEN;	/* Mark that keyfile changed */
	  changed|=((ulonglong) 1 << i);
	  keyinfo->version++;
	  if (keyinfo->ck_delete(info,&old_key))
            goto err;
	  if (keyinfo->ck_insert(info,&new_key))
            goto err;
	  if (share->base.auto_key == i+1)
	    auto_key_changed=1;
	}
      }
    }
  }

  if (share->calc_checksum)
  {
    /*
      We can't use the row based checksum as this doesn't have enough
      precision (one byte, while the table's is more bytes).
      At least _ma_check_unique() modifies the 'newrec' record, so checksum
      has to be computed _after_ it. Nobody apparently modifies 'oldrec'.
      We need to pass the old row's checksum down to (*update_record)(), we do
      this via info->new_row.checksum (not intuitive but existing code
      mandated that cur_row is the new row).
      If (*update_record)() fails, table will be marked corrupted so no need
      to revert the live checksum change.
    */
    info->cur_row.checksum= (*share->calc_checksum)(info, newrec);
    info->new_row.checksum= (*share->calc_checksum)(info, oldrec);
    info->state->checksum+= info->cur_row.checksum - info->new_row.checksum;
  }

  if ((*share->update_record)(info, pos, oldrec, newrec))
    goto err;

  if (auto_key_changed & !share->now_transactional)
  {
    const HA_KEYSEG *keyseg= share->keyinfo[share->base.auto_key-1].seg;
    const uchar *key= newrec + keyseg->start;
    set_if_bigger(share->state.auto_increment,
                  ma_retrieve_auto_increment(key, keyseg->type));
  }

  /*
    We can't yet have HA_STATE_AKTIV here, as block_record dosn't support it
  */
  info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | key_changed);
  share->state.changed|= STATE_NOT_MOVABLE | STATE_NOT_ZEROFILLED;
  info->state->changed= 1;

  /*
    Every Maria function that updates Maria table must end with
    call to _ma_writeinfo(). If operation (second param of
    _ma_writeinfo()) is not 0 it sets share->changed to 1, that is
    flags that data has changed. If operation is 0, this function
    equals to no-op in this case.

    ma_update() must always pass !0 value as operation, since even if
    there is no index change there could be data change.
  */
  VOID(_ma_writeinfo(info, WRITEINFO_UPDATE_KEYFILE));
  allow_break();				/* Allow SIGHUP & SIGINT */
  if (info->invalidator != 0)
  {
    DBUG_PRINT("info", ("invalidator... '%s' (update)",
                        share->open_file_name.str));
    (*info->invalidator)(share->open_file_name.str);
    info->invalidator=0;
  }
  DBUG_RETURN(0);

err:
  DBUG_PRINT("error",("key: %d  errno: %d",i,my_errno));
  save_errno= my_errno;
  DBUG_ASSERT(save_errno);
  if (!save_errno)
    save_errno= HA_ERR_INTERNAL_ERROR;          /* Should never happen */

  if (my_errno == HA_ERR_FOUND_DUPP_KEY || my_errno == HA_ERR_OUT_OF_MEM ||
      my_errno == HA_ERR_RECORD_FILE_FULL)
  {
    info->errkey= (int) i;
    flag=0;
    do
    {
      if (((ulonglong) 1 << i) & changed)
      {
	if (share->keyinfo[i].flag & HA_FULLTEXT)
	{
	  if ((flag++ && _ma_ft_del(info,i,new_key_buff,newrec,pos)) ||
	      _ma_ft_add(info,i,old_key_buff,oldrec,pos))
          {
            _ma_set_fatal_error(share, my_errno);
	    break;
          }
	}
	else
	{
          MARIA_KEY new_key, old_key;
          (*share->keyinfo[i].make_key)(info, &new_key, i, new_key_buff,
                                        newrec, pos,
                                        info->trn->trid);
          (*share->keyinfo[i].make_key)(info, &old_key, i, old_key_buff,
                                        oldrec, pos, info->cur_row.trid);
	  if ((flag++ && _ma_ck_delete(info, &new_key)) ||
	      _ma_ck_write(info, &old_key))
          {
            _ma_set_fatal_error(share, my_errno);
	    break;
          }
	}
      }
    } while (i-- != 0);
  }
  else
    _ma_set_fatal_error(share, save_errno);

  info->update= (HA_STATE_CHANGED | HA_STATE_AKTIV | HA_STATE_ROW_CHANGED |
		 key_changed);

 err_end:
  VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
  allow_break();				/* Allow SIGHUP & SIGINT */
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
    _ma_set_fatal_error(share, HA_ERR_CRASHED);
  DBUG_RETURN(my_errno=save_errno);
} /* maria_update */
