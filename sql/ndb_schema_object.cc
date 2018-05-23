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

// Implements the functions declared in ndb_schema_object.h
#include "sql/ndb_schema_object.h"

#include <mutex>
#include <string>
#include "map_helpers.h"
#include "mysql/service_mysql_alloc.h"
#include "template_utils.h"

class Ndb_schema_objects
{
public:
  // Mutex protecting the unordered map
  std::mutex m_lock;
  malloc_unordered_map<std::string, NDB_SCHEMA_OBJECT *> m_hash;
  Ndb_schema_objects()
    : m_hash(PSI_INSTRUMENT_ME)
  {
  }
} ndb_schema_objects;


NDB_SCHEMA_OBJECT *ndb_get_schema_object(const char *db, const char* table_name,
                                         bool create_if_not_exists)
{
  NDB_SCHEMA_OBJECT *ndb_schema_object;
  DBUG_ENTER("ndb_get_schema_object");
  DBUG_PRINT("enter", ("db: '%s', table_name: '%s'", db, table_name));

  std::string key;
  key.append("./").append(db).append("/").append(table_name);
  DBUG_PRINT("info", ("key: '%s'", key.c_str()));

  std::lock_guard<std::mutex> lock_hash(ndb_schema_objects.m_lock);

  while (!(ndb_schema_object =
               find_or_nullptr(ndb_schema_objects.m_hash, key.c_str())))
  {
    if (!create_if_not_exists)
    {
      DBUG_PRINT("info", ("does not exist"));
      break;
    }
    if (!(ndb_schema_object=
          (NDB_SCHEMA_OBJECT*) my_malloc(PSI_INSTRUMENT_ME,
                                         sizeof(*ndb_schema_object) +
                                         key.length() + 1,
                                         MYF(MY_WME | MY_ZEROFILL))))
    {
      DBUG_PRINT("info", ("malloc error"));
      break;
    }
    ndb_schema_object->key= (char *)(ndb_schema_object+1);
    memcpy(ndb_schema_object->key, key.c_str(), key.length() + 1);
    ndb_schema_object->key_length= key.length();
    ndb_schema_objects.m_hash.emplace(key.c_str(), ndb_schema_object);
    mysql_mutex_init(PSI_INSTRUMENT_ME, &ndb_schema_object->mutex,
                     MY_MUTEX_INIT_FAST);
    mysql_cond_init(PSI_INSTRUMENT_ME, &ndb_schema_object->cond);
    bitmap_init(&ndb_schema_object->slock_bitmap, ndb_schema_object->slock,
                sizeof(ndb_schema_object->slock)*8, false);
    //slock_bitmap is initially cleared due to 'ZEROFILL-malloc'
    break;
  }
  if (ndb_schema_object)
  {
    ndb_schema_object->use_count++;
    DBUG_PRINT("info", ("use_count: %d", ndb_schema_object->use_count));
  }

  DBUG_RETURN(ndb_schema_object);
}


void
ndb_free_schema_object(NDB_SCHEMA_OBJECT **ndb_schema_object)
{
  DBUG_ENTER("ndb_free_schema_object");
  DBUG_PRINT("enter", ("key: '%s'", (*ndb_schema_object)->key));

  std::lock_guard<std::mutex> lock_hash(ndb_schema_objects.m_lock);

  if (!--(*ndb_schema_object)->use_count)
  {
    DBUG_PRINT("info", ("use_count: %d", (*ndb_schema_object)->use_count));
    ndb_schema_objects.m_hash.erase((*ndb_schema_object)->key);
    mysql_cond_destroy(&(*ndb_schema_object)->cond);
    mysql_mutex_destroy(&(*ndb_schema_object)->mutex);
    my_free(*ndb_schema_object);
    *ndb_schema_object= NULL;
  }
  else
  {
    DBUG_PRINT("info", ("use_count: %d", (*ndb_schema_object)->use_count));
  }
  DBUG_VOID_RETURN;
}

//static
void NDB_SCHEMA_OBJECT::check_waiters(const MY_BITMAP &new_participants)
{
  std::lock_guard<std::mutex> lock_hash(ndb_schema_objects.m_lock);

  for (const auto &key_and_value : ndb_schema_objects.m_hash)
  {
    NDB_SCHEMA_OBJECT *schema_object = key_and_value.second;
    schema_object->check_waiter(new_participants);
  }
}

void
NDB_SCHEMA_OBJECT::check_waiter(const MY_BITMAP &new_participants)
{
  mysql_mutex_lock(&mutex);
  bitmap_intersect(&slock_bitmap, &new_participants);
  mysql_mutex_unlock(&mutex);

  // Wakeup waiting Client
  mysql_cond_signal(&cond);
}
