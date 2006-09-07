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

/* Update an old row in a MARIA table */

#include "ma_fulltext.h"
#include "ma_rt_index.h"

int maria_update(register MARIA_HA *info, const byte *oldrec, byte *newrec)
{
  int flag,key_changed,save_errno;
  reg3 my_off_t pos;
  uint i;
  uchar old_key[HA_MAX_KEY_BUFF],*new_key;
  bool auto_key_changed=0;
  ulonglong changed;
  MARIA_SHARE *share=info->s;
  ha_checksum old_checksum;
  DBUG_ENTER("maria_update");
  LINT_INIT(new_key);
  LINT_INIT(changed);
  LINT_INIT(old_checksum);

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
  if (info->state->key_file_length >= share->base.margin_key_file_length)
  {
    DBUG_RETURN(my_errno=HA_ERR_INDEX_FILE_FULL);
  }
  pos=info->lastpos;
  if (_ma_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno);

  if (share->calc_checksum)
    old_checksum=info->checksum=(*share->calc_checksum)(info,oldrec);
  if ((*share->compare_record)(info,oldrec))
  {
    save_errno=my_errno;
    goto err_end;			/* Record has changed */
  }


  /* Calculate and check all unique constraints */
  key_changed=0;
  for (i=0 ; i < share->state.header.uniques ; i++)
  {
    MARIA_UNIQUEDEF *def=share->uniqueinfo+i;
    if (_ma_unique_comp(def, newrec, oldrec,1) &&
	_ma_check_unique(info, def, newrec, _ma_unique_hash(def, newrec),
			info->lastpos))
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

  /* Check which keys changed from the original row */

  new_key=info->lastkey2;
  changed=0;
  for (i=0 ; i < share->base.keys ; i++)
  {
    if (maria_is_key_active(share->state.key_map, i))
    {
      if (share->keyinfo[i].flag & HA_FULLTEXT )
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
	  if (_ma_ft_update(info,i,(char*) old_key,oldrec,newrec,pos))
	    goto err;
	}
      }
      else
      {
	uint new_length= _ma_make_key(info,i,new_key,newrec,pos);
	uint old_length= _ma_make_key(info,i,old_key,oldrec,pos);

        /* The above changed info->lastkey2. Inform maria_rnext_same(). */
        info->update&= ~HA_STATE_RNEXT_SAME;

	if (new_length != old_length ||
	    memcmp((byte*) old_key,(byte*) new_key,new_length))
	{
	  if ((int) i == info->lastinx)
	    key_changed|=HA_STATE_WRITTEN;	/* Mark that keyfile changed */
	  changed|=((ulonglong) 1 << i);
	  share->keyinfo[i].version++;
	  if (share->keyinfo[i].ck_delete(info,i,old_key,old_length)) goto err;
	  if (share->keyinfo[i].ck_insert(info,i,new_key,new_length)) goto err;
	  if (share->base.auto_key == i+1)
	    auto_key_changed=1;
	}
      }
    }
  }
  /*
    If we are running with external locking, we must update the index file
    that something has changed.
  */
  if (changed || !my_disable_locking)
    key_changed|= HA_STATE_CHANGED;

  if (share->calc_checksum)
  {
    info->checksum=(*share->calc_checksum)(info,newrec);
    /* Store new checksum in index file header */
    key_changed|= HA_STATE_CHANGED;
  }
  {
    /*
      Don't update index file if data file is not extended and no status
      information changed
    */
    MARIA_STATUS_INFO state;
    ha_rows org_split;
    my_off_t org_delete_link;

    memcpy((char*) &state, (char*) info->state, sizeof(state));
    org_split=	     share->state.split;
    org_delete_link= share->state.dellink;
    if ((*share->update_record)(info,pos,newrec))
      goto err;
    if (!key_changed &&
	(memcmp((char*) &state, (char*) info->state, sizeof(state)) ||
	 org_split != share->state.split ||
	 org_delete_link != share->state.dellink))
      key_changed|= HA_STATE_CHANGED;		/* Must update index file */
  }
  if (auto_key_changed)
    set_if_bigger(info->s->state.auto_increment,
                  ma_retrieve_auto_increment(info, newrec));
  if (share->calc_checksum)
    info->state->checksum+=(info->checksum - old_checksum);

  info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | HA_STATE_AKTIV |
		 key_changed);
  VOID(_ma_writeinfo(info,key_changed ?  WRITEINFO_UPDATE_KEYFILE : 0));
  allow_break();				/* Allow SIGHUP & SIGINT */
  if (info->invalidator != 0)
  {
    DBUG_PRINT("info", ("invalidator... '%s' (update)", info->filename));
    (*info->invalidator)(info->filename);
    info->invalidator=0;
  }
  DBUG_RETURN(0);

err:
  DBUG_PRINT("error",("key: %d  errno: %d",i,my_errno));
  save_errno=my_errno;
  if (changed)
    key_changed|= HA_STATE_CHANGED;
  if (my_errno == HA_ERR_FOUND_DUPP_KEY || my_errno == HA_ERR_RECORD_FILE_FULL)
  {
    info->errkey= (int) i;
    flag=0;
    do
    {
      if (((ulonglong) 1 << i) & changed)
      {
	if (share->keyinfo[i].flag & HA_FULLTEXT)
	{
	  if ((flag++ && _ma_ft_del(info,i,(char*) new_key,newrec,pos)) ||
	      _ma_ft_add(info,i,(char*) old_key,oldrec,pos))
	    break;
	}
	else
	{
	  uint new_length= _ma_make_key(info,i,new_key,newrec,pos);
	  uint old_length= _ma_make_key(info,i,old_key,oldrec,pos);
	  if ((flag++ && _ma_ck_delete(info,i,new_key,new_length)) ||
	      _ma_ck_write(info,i,old_key,old_length))
	    break;
	}
      }
    } while (i-- != 0);
  }
  else
  {
    maria_print_error(info->s, HA_ERR_CRASHED);
    maria_mark_crashed(info);
  }
  info->update= (HA_STATE_CHANGED | HA_STATE_AKTIV | HA_STATE_ROW_CHANGED |
		 key_changed);

 err_end:
  VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
  allow_break();				/* Allow SIGHUP & SIGINT */
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
  {
    maria_print_error(info->s, HA_ERR_CRASHED);
    save_errno=HA_ERR_CRASHED;
  }
  DBUG_RETURN(my_errno=save_errno);
} /* maria_update */
