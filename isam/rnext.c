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

/* L{ser n{sta post med samma isam-nyckel */

#include "isamdef.h"

	/*
	   L{ser n{sta post med samma isamnyckel som f|reg}ende l{sning.
	   Man kan ha gjort write, update eller delete p} f|reg}ende post.
	   OBS! [ven om man {ndrade isamnyckeln p} f|reg}ende post l{ses
	   posten i avseende p} f|reg}ende isam-nyckel-l{sning !!
	*/

int nisam_rnext(N_INFO *info, byte *buf, int inx)
{
  int error;
  uint flag;
  DBUG_ENTER("nisam_rnext");

  if ((inx = _nisam_check_index(info,inx)) < 0)
    DBUG_RETURN(-1);
  flag=SEARCH_BIGGER;				/* Read next */
  if (info->lastpos == NI_POS_ERROR && info->update & HA_STATE_PREV_FOUND)
    flag=0;					/* Read first */

#ifndef NO_LOCKING
  if (_nisam_readinfo(info,F_RDLCK,1)) DBUG_RETURN(-1);
#endif
  if (!flag)
    error=_nisam_search_first(info,info->s->keyinfo+inx,
			   info->s->state.key_root[inx]);
  else if (_nisam_test_if_changed(info) == 0)
    error=_nisam_search_next(info,info->s->keyinfo+inx,info->lastkey,flag,
			  info->s->state.key_root[inx]);
  else
    error=_nisam_search(info,info->s->keyinfo+inx,info->lastkey,0,flag,
		     info->s->state.key_root[inx]);

	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED |
		  HA_STATE_BUFF_SAVED);
  info->update|= HA_STATE_NEXT_FOUND;

  if (error && my_errno == HA_ERR_KEY_NOT_FOUND)
    my_errno=HA_ERR_END_OF_FILE;
  if ((*info->read_record)(info,info->lastpos,buf) >=0)
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(-1);
} /* nisam_rnext */
