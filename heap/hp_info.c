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

/* Returns info about database status */

#include "heapdef.h"


byte *heap_position(HP_INFO *info)
{
  return ((info->update & HA_STATE_AKTIV) ? info->current_ptr :
	  (HEAP_PTR) 0);
}


#ifdef WANT_OLD_HEAP_VERSION

/*
  The following should NOT be used anymore as this can't be used together with
   heap_rkey()
*/

ulong heap_position_old(HP_INFO *info)
{
  return ((info->update & HA_STATE_AKTIV) ? info->current_record :
	  (ulong) ~0L);
}

#endif /* WANT_OLD_HEAP_CODE */

/* Note that heap_info does NOT return information about the
   current position anymore;  Use heap_position instead */

int heap_info(reg1 HP_INFO *info,reg2 HEAPINFO *x,int flag)
{
  DBUG_ENTER("heap_info");
  x->records	 = info->s->records;
  x->deleted	 = info->s->deleted;
  x->reclength	 = info->s->reclength;
  x->data_length = info->s->data_length;
  x->index_length= info->s->index_length;
  x->max_records = info->s->max_records;
  x->errkey	 = info->errkey;
  DBUG_RETURN(0);
} /* heap_info */
