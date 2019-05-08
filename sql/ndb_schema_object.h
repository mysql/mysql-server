/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_SCHEMA_OBJECT_H
#define NDB_SCHEMA_OBJECT_H

#include <string>

#include "my_bitmap.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"

/*
  Used for communication between the schema distribution Client(which often is
  in a user thread) performing a schema operation and the schema distribution
  Coordinator(which is running as part of the binlog thread).

  The schema distribution Client creates a NDB_SCHEMA_OBJECT before writing
  the schema operation to NDB, then it waits on the NDB_SCHEMA_OBJECT to be
  woken up when the schema operation is completed.

  The schema distribution Coordinator receives new events for the schema
  operation and will update the NDB_SCHEMA_OBJECT with replies and results from
  the other nodes in the cluster. Finally, all other MySQL Servers have replied
  and the schema distribution Client can continue.
*/
class NDB_SCHEMA_OBJECT {
  // String used when storing the NDB_SCHEMA_OBJECT in the list of
  // active NDB_SCHEMA_OBJECTs
  const std::string m_key;

  // Use counter controlling lifecycle of the NDB_SCHEMA_OBJECT
  // Normally there are only two users(the Client and the Coordinator)
  uint m_use_count{0};

  NDB_SCHEMA_OBJECT() = delete;
  NDB_SCHEMA_OBJECT(const NDB_SCHEMA_OBJECT&) = delete;
  NDB_SCHEMA_OBJECT(const char* key, uint slock_bits);
  ~NDB_SCHEMA_OBJECT();

  void check_waiter(const MY_BITMAP &new_participants);
public:

  mysql_mutex_t mutex; // Protects "slock_bitmap" and "cond"
  mysql_cond_t cond;   // Signal/wait for slock_bitmap changes

  // Bitmap which keep track of MySQL Servers participating in the schema
  // operation. When the bitmap is cleared the operation has completed on all.
  MY_BITMAP slock_bitmap;

  // Return bitmap bits as hexadecimal string
  std::string slock_bitmap_to_string() const;

  // Check if NDB_SCHEMA_OBJECTs should wakeup due to new participant status
  static void check_waiters(const MY_BITMAP &new_participants);

  /**
    @brief Get NDB_SCHEMA_OBJECT to be used for communication between Client
           and Coordinator. The Client is usually the one to create an instance
           while the Coordinator simple uses it.

           The parameters "db", "table_name", "id" and "version" identifies
           which object the communication is about

    @param db          Part 1 of key, normally used for database
    @param table_name  Part 2 of key, normally used for table
    @param id          Part 3 of key, normally used for id
    @param version     Part 4 of key, normally used for version
    @param participants Number of participants to dimension for. This parameter
                        must be provided when create_if_not_exists is true.
    @param create_if_not_exists Allow a new NDB_SCHEMA_OBJECT if one doesn't
                                exist.

    @return pointer to NDB_SCHEMA_OBJECT if it existed already or was created
    @return nullptr if NDB_SCHEMA_OBJECT didn't exist
  */
  static NDB_SCHEMA_OBJECT *get(const char *db, const char *table_name,
                                uint32 id, uint32 version,
                                uint participants = 0,
                                bool create_if_not_exists = false);

  /**
     @brief Release NDB_SCHEMA_OBJECT which has been acquired with get()

     @param ndb_schema_object pointer to NDB_SCHEMA_OBJECT to release
   */
  static void release(NDB_SCHEMA_OBJECT *ndb_schema_object);
};

#endif
