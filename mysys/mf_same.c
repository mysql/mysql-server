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

/* Kopierar biblioteksstrukturen och extensionen fr}n ett filnamn */

#include "mysys_priv.h"
#include <m_string.h>

        /*
	  Copy directory and/or extension between filenames.
	  (For the meaning of 'flag', check mf_format.c)
	  'to' may be equal to 'name'.
	  Returns 'to'.
	*/

my_string fn_same(char *to, const char *name, int flag)
{
  char dev[FN_REFLEN];
  const char *ext;
  DBUG_ENTER("fn_same");
  DBUG_PRINT("enter",("to: %s  name: %s  flag: %d",to,name,flag));

  if ((ext=strrchr(name+dirname_part(dev,name),FN_EXTCHAR)) == 0)
    ext="";

  DBUG_RETURN(fn_format(to,to,dev,ext,flag));
} /* fn_same */
