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

/* L{ser f|rsta posten som har samma isam-nyckel */

#include "isamdef.h"

	/*
	   L{ser f|rsta posten med samma isamnyckel som f|reg}ende l{sning.
	   Man kan ha gjort write, update eller delete p} f|reg}ende post.
	   OBS! [ven om man {ndrade isamnyckeln p} f|reg}ende post l{ses
	   posten i avseende p} f|reg}ende isam-nyckel-l{sning !!
	*/

int nisam_rfirst(N_INFO *info, byte *buf, int inx)
{
  DBUG_ENTER("nisam_rfirst");
  info->lastpos= NI_POS_ERROR;
  info->update|= HA_STATE_PREV_FOUND;
  DBUG_RETURN(nisam_rnext(info,buf,inx));
} /* nisam_rfirst */
