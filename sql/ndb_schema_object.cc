/*
   Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_schema_object.h"

#include "hash.h"


extern native_mutex_t ndbcluster_mutex;


static uchar *
ndb_schema_objects_get_key(NDB_SCHEMA_OBJECT *schema_object,
                           size_t *length,
                           my_bool)
{
  *length= schema_object->key_length;
  return (uchar*) schema_object->key;
}

class Ndb_schema_objects
{
public:
  HASH m_hash;
  Ndb_schema_objects()
  {
    (void)my_hash_init(&m_hash, &my_charset_bin, 1, 0, 0,
                       (my_hash_get_key)ndb_schema_objects_get_key, 0, 0,
                       PSI_INSTRUMENT_ME);
  }

  ~Ndb_schema_objects()
  {
    my_hash_free(&m_hash);
  }

} ndb_schema_objects;


NDB_SCHEMA_OBJECT *ndb_get_schema_object(const char *key,
                                         bool create_if_not_exists)
{
  NDB_SCHEMA_OBJECT *ndb_schema_object;
  size_t length= strlen(key);
  DBUG_ENTER("ndb_get_schema_object");
  DBUG_PRINT("enter", ("key: '%s'", key));

  native_mutex_lock(&ndbcluster_mutex);
  while (!(ndb_schema_object=
           (NDB_SCHEMA_OBJECT*) my_hash_search(&ndb_schema_objects.m_hash,
                                               (const uchar*) key,
                                               length)))
  {
    if (!create_if_not_exists)
    {
      DBUG_PRINT("info", ("does not exist"));
      break;
    }
    if (!(ndb_schema_object=
          (NDB_SCHEMA_OBJECT*) my_malloc(PSI_INSTRUMENT_ME,
                                         sizeof(*ndb_schema_object) + length + 1,
                                         MYF(MY_WME | MY_ZEROFILL))))
    {
      DBUG_PRINT("info", ("malloc error"));
      break;
    }
    ndb_schema_object->key= (char *)(ndb_schema_object+1);
    memcpy(ndb_schema_object->key, key, length + 1);
    ndb_schema_object->key_length= length;
    if (my_hash_insert(&ndb_schema_objects.m_hash, (uchar*) ndb_schema_object))
    {
      my_free(ndb_schema_object);
      break;
    }
    native_mutex_init(&ndb_schema_object->mutex, MY_MUTEX_INIT_FAST);
    bitmap_init(&ndb_schema_object->slock_bitmap, ndb_schema_object->slock,
                sizeof(ndb_schema_object->slock)*8, FALSE);
    // Expect answer from all other nodes by default(those
    // who are not subscribed will be filtered away by
    // the Coordinator which keep track of that stuff)
    bitmap_set_all(&ndb_schema_object->slock_bitmap);
    break;
  }
  if (ndb_schema_object)
  {
    ndb_schema_object->use_count++;
    DBUG_PRINT("info", ("use_count: %d", ndb_schema_object->use_count));
  }
  native_mutex_unlock(&ndbcluster_mutex);
  DBUG_RETURN(ndb_schema_object);
}


void
ndb_free_schema_object(NDB_SCHEMA_OBJECT **ndb_schema_object)
{
  DBUG_ENTER("ndb_free_schema_object");
  DBUG_PRINT("enter", ("key: '%s'", (*ndb_schema_object)->key));

  native_mutex_lock(&ndbcluster_mutex);
  if (!--(*ndb_schema_object)->use_count)
  {
    DBUG_PRINT("info", ("use_count: %d", (*ndb_schema_object)->use_count));
    my_hash_delete(&ndb_schema_objects.m_hash, (uchar*) *ndb_schema_object);
    native_mutex_destroy(&(*ndb_schema_object)->mutex);
    my_free(*ndb_schema_object);
    *ndb_schema_object= 0;
  }
  else
  {
    DBUG_PRINT("info", ("use_count: %d", (*ndb_schema_object)->use_count));
  }
  native_mutex_unlock(&ndbcluster_mutex);
  DBUG_VOID_RETURN;
}
