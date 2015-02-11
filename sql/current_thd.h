/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CURRENT_THD_INCLUDED
#define CURRENT_THD_INCLUDED

#include "my_global.h"
#include "my_thread_local.h"
#include "my_dbug.h"

/*
  THR_THD is a key which will be used to set/get THD* for a thread,
  using my_set_thread_local()/my_get_thread_local().
*/
extern MYSQL_PLUGIN_IMPORT thread_local_key_t THR_THD;
extern bool THR_THD_initialized;

static inline THD * my_thread_get_THR_THD()
{
  DBUG_ASSERT(THR_THD_initialized);
  return (THD*)my_get_thread_local(THR_THD);
}

static inline int my_thread_set_THR_THD(THD *thd)
{
  DBUG_ASSERT(THR_THD_initialized);
  return my_set_thread_local(THR_THD, thd);
}

#if defined(MYSQL_DYNAMIC_PLUGIN) && defined(_WIN32)
extern "C" THD *_current_thd_noinline();
static inline THD *inline_current_thd(void)
{
  return _current_thd_noinline();
}
#else
static inline THD *inline_current_thd(void)
{
  return my_thread_get_THR_THD();
}
#endif  /* MYSQL_DYNAMIC_PLUGIN && _WIN32 */

#define current_thd inline_current_thd()


#endif  // CURRENT_THD_INCLUDED
