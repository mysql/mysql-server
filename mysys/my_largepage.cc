/* Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file mysys/my_largepage.cc
*/

#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "mysql/psi/mysql_file.h"
#include "mysys/mysys_priv.h"

static uint my_get_large_page_size_int(void);

/* Gets the size of large pages from the OS */

uint my_get_large_page_size(void)
{
  uint size;
  DBUG_ENTER("my_get_large_page_size");

  if (!(size = my_get_large_page_size_int()))
    my_message_local(WARNING_LEVEL, "Failed to determine large page size"); /* purecov: inspected */

  DBUG_RETURN(size);
}

/* Linux-specific function to determine the size of large pages */

uint my_get_large_page_size_int(void)
{
  MYSQL_FILE *f;
  uint size = 0;
  char buf[256];
  DBUG_ENTER("my_get_large_page_size_int");

  if (!(f= mysql_file_fopen(key_file_proc_meminfo, "/proc/meminfo",
                            O_RDONLY, MYF(MY_WME))))
    goto finish;

  while (mysql_file_fgets(buf, sizeof(buf), f))
    if (sscanf(buf, "Hugepagesize: %u kB", &size))
      break;

  mysql_file_fclose(f, MYF(MY_WME));
  
finish:
  DBUG_RETURN(size * 1024);
}
