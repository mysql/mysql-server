/* Copyright (C) 2000 MySQL AB

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

/* This implements strtol() if needed */

/*
   These includes are mandatory because they check for type sizes and
   functions, especially they handle tricks for Tru64 where 'long' is
   64 bit already and our 'longlong' is just a 'long'.
 */
#include <my_global.h>
#include <m_string.h>

#if !defined(MSDOS) && !defined(HAVE_STRTOL) && !defined(__WIN__)
#include "strto.c"
#endif
