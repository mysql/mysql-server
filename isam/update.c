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

/* Uppdaterare nuvarande record i en pisam-databas */

#include "isamdef.h"
#ifdef	__WIN__
#include <errno.h>
#endif

	/* Updaterar senaste l{sta record i databasen */

int nisam_update(register N_INFO *info, const byte *oldrec, const byte *newrec)
{
  int flag,key_changed,save_errno;
  reg3 ulong pos;
  uint i,length;
  uchar old_key[N_MAX_KEY_BUFF],*new_key;
  DBUG_ENTER("nisam_update");

  LINT_INIT(save_errno);
  if (!(info->update & HA_STATE_AKTIV))
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;
    DBUG_RETURN(-1);
  }
  if (info->s->base.options & HA_OPTION_READ_ONLY_DATA)
  {
    my_errno=EACCES;
    DBUG_RETURN(-1);
  }
  pos=info->lastpos;
#ifndef NO_LOCKING
  if (_nisam_readinfo(info,F_WRLCK,1)) DBUG_RETURN(-1);
#endif
  if ((*info->s->compare_record)(info,oldrec))
  {
    save_errno=my_errno;
    goto err_end;			/* Record has changed */
  }
  if (info->s->state.key_file_length >=
      info->s->base.max_key_file_length -
      info->s->blocksize* INDEX_BLOCK_MARGIN *info->s->state.keys)
  {
    save_errno=HA_ERR_INDEX_FILE_FULL;
    goto err_end;
  }

	/* Flyttar de element i isamfilen som m}ste flyttas */

  new_key=info->lastkey+info->s->base.max_key_length;
  key_changed=HA_STATE_KEY_CHANGED;	/* We changed current database */
					/* Remove key that didn't change */
  for (i=0 ; i < info->s->state.keys ; i++)
  {
    length=_nisam_make_key(info,i,new_key,newrec,pos);
    if (length != _nisam_make_key(info,i,old_key,oldrec,pos) ||
	memcmp((byte*) old_key,(byte*) new_key,length))
    {
      if ((int) i == info->lastinx)
	key_changed|=HA_STATE_WRITTEN;		/* Mark that keyfile changed */
      if (_nisam_ck_delete(info,i,old_key)) goto err;
      if (_nisam_ck_write(info,i,new_key)) goto err;
    }
  }

  if ((*info->s->update_record)(info,pos,newrec))
    goto err;

  info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | HA_STATE_AKTIV |
		 key_changed);
  nisam_log_record(LOG_UPDATE,info,newrec,info->lastpos,0);
  VOID(_nisam_writeinfo(info,test(key_changed)));
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
      length=_nisam_make_key(info,i,new_key,newrec,pos);
      if (length != _nisam_make_key(info,i,old_key,oldrec,pos) ||
	  memcmp((byte*) old_key,(byte*) new_key,length))
      {
	if ((flag++ && _nisam_ck_delete(info,i,new_key)) ||
	    _nisam_ck_write(info,i,old_key))
	  break;
      }
    } while (i-- != 0);
  }
  info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | HA_STATE_AKTIV |
		 key_changed);
 err_end:
  nisam_log_record(LOG_UPDATE,info,newrec,info->lastpos,save_errno);
  VOID(_nisam_writeinfo(info,1));
  allow_break();				/* Allow SIGHUP & SIGINT */
  my_errno=(save_errno == HA_ERR_KEY_NOT_FOUND) ? HA_ERR_CRASHED : save_errno;
  DBUG_RETURN(-1);
} /* nisam_update */
