/* Copyright (C) 2011 Monty Program Ab

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

/* This file is to be include first in all files in the string directory */

#include <my_global.h>		/* Define standar vars */
#include "m_string.h"		/* Exernal defintions of string functions */

/*
  We can't use the original DBUG_ASSERT() (which includes _db_flush())
  in the strings library as libdbug is compiled after the the strings
  library and we don't want to have strings depending on libdbug which
  depends on mysys and strings.
*/

#if !defined(DBUG_OFF)
#undef DBUG_ASSERT
#define DBUG_ASSERT(A) assert(A)
#endif
