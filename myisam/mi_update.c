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

/* Update an old row in a MyISAM table */

#include "fulltext.h"
#ifdef	__WIN__
#include <errno.h>
#endif


int mi_update(register MI_INFO *info, const byte *oldrec, byte *newrec)
{
  int flag,key_changed,save_errno;
  reg3 my_off_t pos;
  uint i;
  uchar old_key[MI_MAX_KEY_BUFF],*new_key;
  bool auto_key_changed=0;
  ulonglong changed;
  MYISAM_SHARE *share=info->s;
  ha_checksum old_checksum;
  DBUG_ENTER("mi_update");
  LINT_INIT(new_key);
  LINT_INIT(changed);
  LINT_INIT(old_checksum);

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
  if (_mi_readinfo(info,F_WRLCK,1))
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
    MI_UNIQUEDEF *def=share->uniqueinfo+i;
    if (mi_unique_comp(def, newrec, oldrec,1) &&
	mi_check_unique(info, def, newrec, mi_unique_hash(def, newrec),
			info->lastpos))
    {
      save_errno=my_errno;
      goto err_end;
    }
  }
  if (_mi_mark_file_changed(info))
  {
    save_errno=my_errno;
    goto err_end;
  }

  /* Check which keys changed from the original row */

  new_key=info->lastkey2;
  key_changed=HA_STATE_KEY_CHANGED;	/* We changed current database */
					/* Remove key that didn't change */
  changed=0;
  for (i=0 ; i < share->base.keys ; i++)
  {
    if (((ulonglong) 1 << i) & share->state.key_map)
    {
      /* The following code block is for text searching by SerG */
      if (share->keyinfo[i].flag & HA_FULLTEXT )
      {
	if(_mi_ft_cmp(info,i,oldrec, newrec))
	{
	  if ((int) i == info->lastinx)
	    key_changed|=HA_STATE_WRITTEN;
	  changed|=((ulonglong) 1 << i);
	  if (_mi_ft_del(info,i,(char*) old_key,oldrec,pos))
	    goto err;
	  if (_mi_ft_add(info,i,(char*) new_key,newrec,pos))
	    goto err;
	}
      }
      else
      {
	uint new_length=_mi_make_key(info,i,new_key,newrec,pos);
	uint old_length=_mi_make_key(info,i,old_key,oldrec,pos);
	if (new_length != old_length ||
	    memcmp((byte*) old_key,(byte*) new_key,new_length))
	{
	  if ((int) i == info->lastinx)
	    key_changed|=HA_STATE_WRITTEN;	/* Mark that keyfile changed */
	  changed|=((ulonglong) 1 << i);
	  share->keyinfo[i].version++;
	  if (_mi_ck_delete(info,i,old_key,old_length)) goto err;
	  if (_mi_ck_write(info,i,new_key,new_length)) goto err;
	  if (share->base.auto_key == i+1)
	    auto_key_changed=1;
	}
      }
    }
  }

  if (share->calc_checksum)
    info->checksum=(*share->calc_checksum)(info,newrec);
  if ((*share->update_record)(info,pos,newrec))
    goto err;
  if (auto_key_changed)
    update_auto_increment(info,newrec);
  if (share->calc_checksum)
    share->state.checksum+=(info->checksum - old_checksum);

  info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | HA_STATE_AKTIV |
		 key_changed);
  myisam_log_record(MI_LOG_UPDATE,info,newrec,info->lastpos,0);
  VOID(_mi_writeinfo(info,key_changed ?  WRITEINFO_UPDATE_KEYFILE : 0));
  allow_break();				/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  DBUG_PRINT("error",("key: %d  errno: %d",i,my_errno));
  save_errno=my_errno;
  if (my_errno == HA_ERR_FOUND_DUPP_KEY || my_errno == HA_ERR_RECORD_FILE_FULL)
  {
    info->errkey= (int) i;
    flag=0;
    do
    {
      if (((ulonglong) 1 << i) & changed)
      {
	/* The following code block is for text searching by SerG */
	if (share->keyinfo[i].flag & HA_FULLTEXT)
	{
	  if ((flag++ && _mi_ft_del(info,i,(char*) new_key,newrec,pos)) ||
	      _mi_ft_add(info,i,(char*) old_key,oldrec,pos))
	    break;
	}
	else
	{
	  uint new_length=_mi_make_key(info,i,new_key,newrec,pos);
	  uint old_length= _mi_make_key(info,i,old_key,oldrec,pos);
	  if ((flag++ && _mi_ck_delete(info,i,new_key,new_length)) ||
	      _mi_ck_write(info,i,old_key,old_length))
	    break;
	}
      }
    } while (i-- != 0);
  }
  else
    mi_mark_crashed(info);
  info->update= (HA_STATE_CHANGED | HA_STATE_AKTIV | HA_STATE_ROW_CHANGED |
		 key_changed);

 err_end:
  myisam_log_record(MI_LOG_UPDATE,info,newrec,info->lastpos,my_errno);
  VOID(_mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
  allow_break();				/* Allow SIGHUP & SIGINT */
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
    save_errno=HA_ERR_CRASHED;
  DBUG_RETURN(my_errno=save_errno);
} /* mi_update */
