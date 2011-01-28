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
  virtual const char* enter_cond(mysql_cond_t *cond,
                                 mysql_mutex_t* mutex,
                                 const char* msg)
  {
    m_current_mutex= mutex;
    return NULL;
  }

  virtual void exit_cond(const char* old_msg)
  {
    mysql_mutex_unlock(m_current_mutex);
  }

  virtual int  is_killed() { return 0; }
  virtual THD* get_thd()   { return NULL; }

private:
  mysql_mutex_t *m_current_mutex;
};

#endif  // TEST_MDL_CONTEXT_OWNER_INCLUDED
