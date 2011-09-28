/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include "common.h"


// Client-side logging function

void error_log_vprint(error_log_level::type level,
                        const char *fmt, va_list args)
{
  const char *level_string= "";
  int   log_level= get_log_level();

  switch (level)
  {
  case error_log_level::INFO:    
    if (3 > log_level)
      return;
    level_string= "Note"; 
    break;
  case error_log_level::WARNING: 
    if (2 > log_level)
      return;
    level_string= "Warning"; 
    break;
  case error_log_level::ERROR:   
    if (1 > log_level)
      return;
    level_string= "ERROR";
    break;
  }

  fprintf(stderr, "Windows Authentication Plugin %s: ", level_string);
  vfprintf(stderr, fmt, args);
  fputc('\n', stderr);
  fflush(stderr);
}


// Trivial implementation of log-level setting storage.

void set_log_level(unsigned int level)
{
  opt_auth_win_log_level= level;
}


unsigned int  get_log_level(void)
{
  return opt_auth_win_log_level;
}
