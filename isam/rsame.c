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

/* L{ser nuvarande record med direktl{sning */
/* Klarar b}de poster l{sta med nyckel och rrnd. */

#include "isamdef.h"

	/* Funktionen ger som resultat:
	   0 = Ok.
	   1 = Posten borttagen
	  -1 = EOF (eller motsvarande: se errno) */


int nisam_rsame(N_INFO *info, byte *record, int inx)


					/* If inx >= 0 find record using key */
{
  DBUG_ENTER("nisam_rsame");

  if (inx >= (int) info->s->state.keys || inx < -1)
  {
    my_errno=HA_ERR_WRONG_INDEX;
    DBUG_RETURN(-1);
  }
  if (info->lastpos == NI_POS_ERROR || info->update & HA_STATE_DELETED)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;	/* No current record */
    DBUG_RETURN(-1);
  }
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  /* L{s record fr}n datafilen */

#ifndef NO_LOCKING
  if (_nisam_readinfo(info,F_RDLCK,1))
    DBUG_RETURN(-1);
#endif

  if (inx >= 0)
  {
    info->lastinx=inx;
    VOID(_nisam_make_key(info,(uint) inx,info->lastkey,record,info->lastpos));
    VOID(_nisam_search(info,info->s->keyinfo+inx,info->lastkey,0,SEARCH_SAME,
		    info->s->state.key_root[inx]));
  }

  if ((*info->read_record)(info,info->lastpos,record) == 0)
    DBUG_RETURN(0);
  if (my_errno == HA_ERR_RECORD_DELETED)
  {
    my_errno=HA_ERR_KEY_NOT_FOUND;
    DBUG_RETURN(1);
  }
  DBUG_RETURN(-1);
} /* nisam_rsame */
