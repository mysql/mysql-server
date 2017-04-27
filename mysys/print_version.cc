/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**

  @file mysys/print_version.cc
*/

#include "print_version.h"

#include "my_config.h"

#include <stdio.h>
#include <string.h>

#include "m_string.h"
#include "my_sys.h"
#include "mysql_com.h"
#include "mysql_version.h"


#ifdef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX_STR STRINGIFY_ARG(MYSQL_SERVER_SUFFIX)
#else
#define MYSQL_SERVER_SUFFIX_STR MYSQL_SERVER_SUFFIX_DEF
#endif

void print_version()
{
  char version_buffer[SERVER_VERSION_LENGTH];
  strxmov(version_buffer, MYSQL_SERVER_VERSION,
	  MYSQL_SERVER_SUFFIX_STR, NullS);
  printf("%s  Ver %s for %s on %s (%s)\n", my_progname,
	 version_buffer, SYSTEM_TYPE, MACHINE_TYPE, MYSQL_COMPILATION_COMMENT);
}


void print_version_debug()
{
  char version_buffer[SERVER_VERSION_LENGTH];
  strxmov(version_buffer, MYSQL_SERVER_VERSION,
	  MYSQL_SERVER_SUFFIX_STR, NullS);
  printf("%s  Ver %s-debug for %s on %s (%s)\n", my_progname,
	 version_buffer, SYSTEM_TYPE, MACHINE_TYPE, MYSQL_COMPILATION_COMMENT);
}


void print_explicit_version(const char* version)
{
  printf("%s  Ver %s for %s on %s (%s)\n", my_progname,
	 version, SYSTEM_TYPE, MACHINE_TYPE, MYSQL_COMPILATION_COMMENT);
}
