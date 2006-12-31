/* Copyright (C) 2005-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H

#if (defined(_SCO_DS) || defined(UNIXWARE_7)) && !defined(SHUT_RDWR)
/*
   SHUT_* functions are defined only if
   "(defined(_XOPEN_SOURCE) && _XOPEN_SOURCE_EXTENDED - 0 >= 1)"
*/
#define SHUT_RDWR 2
#endif

#ifdef __WIN__

#define vsnprintf _vsnprintf
#define snprintf _snprintf

#define SIGKILL 9

/*TODO:  fix this */
#define PROTOCOL_VERSION 10

typedef int pid_t;

#undef popen
#define popen(A,B) _popen(A,B)

#endif /* __WIN__ */

#endif  /* INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H */


