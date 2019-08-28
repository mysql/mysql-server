/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef TEST_MDL_CONTEXT_OWNER_INCLUDED
#define TEST_MDL_CONTEXT_OWNER_INCLUDED

#include <mdl.h>
#include <my_pthread.h>

class Test_MDL_context_owner : public MDL_context_owner
{
public:
  Test_MDL_context_owner()
    : m_current_mutex(NULL)
  {}
  virtual void enter_cond(mysql_cond_t *cond,
                          mysql_mutex_t* mutex,
                          const PSI_stage_info *stage,
                          PSI_stage_info *old_stage,
                          const char *src_function,
                          const char *src_file,
                          int src_line)
  {
    m_current_mutex= mutex;
    return;
  }

  virtual void exit_cond(const PSI_stage_info *stage,
                         const char *src_function,
                         const char *src_file,
                         int src_line)
  {
    mysql_mutex_unlock(m_current_mutex);
  }

  virtual int  is_killed() { return 0; }
  virtual THD* get_thd()   { return NULL; }

private:
  mysql_mutex_t *m_current_mutex;
};

#endif  // TEST_MDL_CONTEXT_OWNER_INCLUDED
