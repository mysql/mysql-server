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

#include "mrgdef.h"

	/* if flag == HA_PANIC_CLOSE then all misam files are closed */
	/* if flag == HA_PANIC_WRITE then all misam files are unlocked and
	   all changed data in single user misam is written to file */
	/* if flag == HA_PANIC_READ then all misam files that was locked when
	   nisam_panic(HA_PANIC_WRITE) was done is locked. A ni_readinfo() is
	   done for all single user files to get changes in database */


int mrg_panic(
enum ha_panic_function flag)
{
  int error=0;
  LIST *list_element,*next_open;
  MRG_INFO *info;
  DBUG_ENTER("mrg_panic");

  for (list_element=mrg_open_list ; list_element ; list_element=next_open)
  {
    next_open=list_element->next;		/* Save if close */
    info=(MRG_INFO*) list_element->data;
    if (flag == HA_PANIC_CLOSE && mrg_close(info))
      error=my_errno;
  }
  if (mrg_open_list && flag != HA_PANIC_CLOSE)
    DBUG_RETURN(nisam_panic(flag));
  if (!error) DBUG_RETURN(0);
  my_errno=error;
  DBUG_RETURN(-1);
}
