/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SQL_NDB_STORED_GRANTS_H
#define SQL_NDB_STORED_GRANTS_H

#include <string>

class THD;
class Thd_ndb;
class Acl_change_notification;
class NdbTransaction;

/*
   All functions return true on success
*/
namespace Ndb_stored_grants {

// Initialize the Ndb_stored_grants component
bool init();

// Setup the Ndb_stored_grants component
bool setup(THD *, Thd_ndb *);

void shutdown(THD *, Thd_ndb *, bool restarting);

bool apply_stored_grants(THD *);

enum class Strategy { ERROR, NONE, STATEMENT, SNAPSHOT };

Strategy handle_local_acl_change(THD *, const class Acl_change_notification *,
                                 std::string *user_list,
                                 bool *schema_dist_use_db,
                                 bool *particpants_must_refresh);

bool update_users_from_snapshot(THD *, std::string user_list);

void maintain_cache(THD *);

NdbTransaction *acquire_snapshot_lock(THD *);

void release_snapshot_lock(NdbTransaction *);

}  // namespace Ndb_stored_grants

#endif
