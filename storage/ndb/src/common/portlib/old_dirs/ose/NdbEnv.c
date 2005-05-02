/* Copyright (C) 2003 MySQL AB

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


#include "NdbEnv.h"
#include <string.h>
#include <stdlib.h>

const char* NdbEnv_GetEnv(const char* name, char * buf, int buflen)
{
  /**
   * All environment variables are associated with a process
   * it's important to read env from the correct process
   * for now read from own process, own block and last the "ose_shell" process.
   *
   * TODO! What process should this be read from in the future?
   *
   */
  PROCESS proc_;
  char* p = NULL;
  /* Look in own process */
  p = get_env(current_process(), (char*)name);
  if (p == NULL){
    /* Look in block process */
    p = get_env(get_bid(current_process()), (char*)name);
    if (p == NULL){
      /* Look in ose_shell process */
      if (hunt("ose_shell", 0, &proc_, NULL)){
	p = get_env(proc_, (char*)name);
      }
    }
  }

  if (p != NULL){
    strncpy(buf, p, buflen);    
    buf[buflen-1] = 0;
    free_buf((union SIGNAL **)&p);
    p = buf;
  }
  return p;
}

