/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*  File   : strcat.c
    Author : Richard A. O'Keefe.
    Updated: 10 April 1984
    Defines: strcat()

    strcat(s, t) concatenates t on the end of s.  There  had  better  be
    enough  room  in  the  space s points to; strcat has no way to tell.
    Note that strcat has to search for the end of s, so if you are doing
    a lot of concatenating it may be better to use strmov, e.g.
	strmov(strmov(strmov(strmov(s,a),b),c),d)
    rather than
	strcat(strcat(strcat(strcpy(s,a),b),c),d).
    strcat returns the old value of s.
*/

#include "strings.h"

char *strcat(register char *s, register const char *t)
{
	char *save;

	for (save = s; *s++; ) ;
	for (--s; *s++ = *t++; ) ;
	return save;
    }
