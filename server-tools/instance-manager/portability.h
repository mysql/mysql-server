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

#define DFLT_CONFIG_FILE_NAME "my.ini"
#define DFLT_MYSQLD_PATH      "mysqld"
#define DFLT_PASSWD_FILE_EXT  ".passwd"
#define DFLT_PID_FILE_EXT     ".pid"
#define DFLT_SOCKET_FILE_EXT  ".sock"

typedef int pid_t;

#undef popen
#define popen(A,B) _popen(A,B)

#define NEWLINE "\r\n"
#define NEWLINE_LEN 2

const char CR = '\r';
const char LF = '\n';

#else /* ! __WIN__ */

#define NEWLINE "\n"
#define NEWLINE_LEN 1

const char LF = '\n';

#endif /* __WIN__ */

#endif  /* INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H */


