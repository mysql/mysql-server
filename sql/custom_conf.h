/* Copyright (c) 2000, 2006 MySQL AB
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

#ifndef __MYSQL_CUSTOM_BUILD_CONFIG__
#define __MYSQL_CUSTOM_BUILD_CONFIG__

#define MYSQL_PORT		5002
#ifdef __WIN__
#define MYSQL_NAMEDPIPE		"SwSqlServer"
#define MYSQL_SERVICENAME	"SwSqlServer"
#define KEY_SERVICE_PARAMETERS
"SYSTEM\\CurrentControlSet\\Services\\SwSqlServer\\Parameters"
#endif

#endif /* __MYSQL_CUSTOM_BUILD_CONFIG__ */
