/* Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef TEST_MDL_CONTEXT_OWNER_INCLUDED
#define TEST_MDL_CONTEXT_OWNER_INCLUDED

#include <mdl.h>

class Test_MDL_context_owner : public MDL_context_owner
{
public:
  Test_MDL_context_owner()
  {}
  virtual void enter_cond(mysql_cond_t *cond,
                          mysql_mutex_t* mutex,
                          const PSI_stage_info *stage,
                          PSI_stage_info *old_stage,
                          const char *src_function,
                          const char *src_file,
                          int src_line)
  {
  }

  virtual void exit_cond(const PSI_stage_info *stage,
                         const char *src_function,
                         const char *src_file,
                         int src_line)
  {
  }

  virtual int  is_killed() { return 0; }
  virtual bool is_connected() { return true; }
  virtual THD* get_thd()   { return NULL; }

  virtual bool notify_hton_pre_acquire_exclusive(const MDL_key *mdl_key,
                                                 bool *victimized)
  { return false; }
  virtual void notify_hton_post_release_exclusive(const MDL_key *mdl_key)
  { }

  virtual uint get_rand_seed() { return 0; }
};

#endif  // TEST_MDL_CONTEXT_OWNER_INCLUDED
