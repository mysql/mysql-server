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

/* read record through position and fix key-position */
/* As nisam_rsame but supply a position */

#include "isamdef.h"


	/*
	** If inx >= 0 update index pointer
	** Returns one of the following values:
	**  0 = Ok.
	**  1 = Record deleted
	** -1 = EOF (or something similar. More information in my_errno)
	*/

int nisam_rsame_with_pos(N_INFO *info, byte *record, int inx, ulong filepos)
{
  DBUG_ENTER("nisam_rsame_with_pos");

  if (inx >= (int) info->s->state.keys || inx < -1)
  {
    my_errno=HA_ERR_WRONG_INDEX;
    DBUG_RETURN(-1);
  }

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  if ((*info->s->read_rnd)(info,record,filepos,0))
  {
    if (my_errno == HA_ERR_RECORD_DELETED)
    {
      my_errno=HA_ERR_KEY_NOT_FOUND;
      DBUG_RETURN(1);
    }
    DBUG_RETURN(-1);
  }
  info->lastpos=filepos;
  info->lastinx=inx;
  if (inx >= 0)
  {
    VOID(_nisam_make_key(info,(uint) inx,info->lastkey,record,info->lastpos));
    info->update|=HA_STATE_KEY_CHANGED;		/* Don't use indexposition */
  }
  DBUG_RETURN(0);
} /* nisam_rsame_pos */
