#ifndef MYSQLD_SUFFIX_INCLUDED
#define MYSQLD_SUFFIX_INCLUDED

/* Copyright (c) 2000, 2004, 2006, 2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file

  Set MYSQL_SERVER_SUFFIX_STR.

  The following code is quite ugly as there is no portable way to easily set a
  string to the value of a macro
*/

#ifdef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX_STR STRINGIFY_ARG(MYSQL_SERVER_SUFFIX)
#else
#define MYSQL_SERVER_SUFFIX_STR MYSQL_SERVER_SUFFIX_DEF
#endif
#endif /* MYSQLD_SUFFIX_INCLUDED */
