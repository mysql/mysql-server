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

/* Defines that are unique to the embedded version of MySQL */

#ifdef EMBEDDED_LIBRARY

/* Things we don't need in the embedded version of MySQL */

#undef HAVE_PSTACK				/* No stacktrace */
#undef HAVE_DLOPEN				/* No udf functions */
#undef HAVE_OPENSSL
#undef HAVE_VIO
#undef HAVE_ISAM

#define DONT_USE_RAID
#endif /* EMBEDDED_LIBRARY */
