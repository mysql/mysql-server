/* Copyright (C) 2003 MySQL AB

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

/* 
 * NdbStdio.h - stdio.h for ndb
 *  
 *
 */


#if defined NDB_OSE || defined NDB_SOFTOSE
/* On OSE Delta the snprintf is declare in outfmt.h */
#include <outfmt.h>
#endif

#include <ndb_global.h>

#ifdef NDB_WIN32
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strtok_r(s1, s2, l) strtok(s1, s2)
#endif

