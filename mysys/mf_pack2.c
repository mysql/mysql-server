/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include "mysys_priv.h"
#include <m_string.h>

	/* Pack a filename for output on screen */
	/* Changes long paths to .../ */
	/* Removes pathname and extension */
	/* If not possibly to pack returns '?' in to and returns 1*/

int pack_filename(my_string to, const char *name, size_s max_length)
					/* to may be name */

{
  int i;
  char buff[FN_REFLEN];

  if (strlen(fn_format(to,name,"","",0)) <= max_length)
    return 0;
  if (strlen(fn_format(to,name,"","",8)) <= max_length)
    return 0;
  if (strlen(fn_format(buff,name,".../","",1)) <= max_length)
  {
    VOID(strmov(to,buff));
    return 0;
  }
  for (i= 0 ; i < 3 ; i++)
  {
    if (strlen(fn_format(buff,to,"","", i == 0 ? 2 : i == 1 ? 1 : 3 ))
	<= max_length)
    {
      VOID(strmov(to,buff));
      return 0;
    }
  }
  to[0]='?'; to[1]=0;				/* Can't pack filename */
  return 1;
} /* pack_filename */
