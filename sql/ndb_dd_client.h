/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_DD_CLIENT_H
#define NDB_DD_CLIENT_H

#include "my_inttypes.h"


/*
  Class encapculating the code for accessing the DD
  from ndbcluster

  Handles:
   - locking and releasing MDL(metadata locks)
   - disabling and restoring autocommit
*/
class Ndb_dd_client {
  class THD* const m_thd;
  bool m_mdl_locks_acquired;
  ulonglong m_save_option_bits;

  void disable_autocommit();

public:
  Ndb_dd_client(class THD* thd) :
    m_thd(thd),
    m_mdl_locks_acquired(false),
    m_save_option_bits(0)
  {
    disable_autocommit();
  }

  ~Ndb_dd_client();

  // Metadata lock functions
  bool mdl_locks_acquire(const char* schema_name, const char* table_name);
  bool mdl_locks_acquire_exclusive(const char* schema_name,
                                   const char* table_name);
  void mdl_locks_release();


};


#endif
