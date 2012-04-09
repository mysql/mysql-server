/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "NdbPoolImpl.hpp"

NdbMutex *NdbPool::pool_mutex = NULL;
NdbPool *the_pool = NULL;

NdbPool*
NdbPool::create_instance(Ndb_cluster_connection* cc,
			 Uint32 max_ndb_obj,
                         Uint32 no_conn_obj,
                         Uint32 init_no_ndb_objects)
{
  if (!initPoolMutex()) {
    return NULL;
  }
  NdbMutex_Lock(pool_mutex);
  NdbPool* a_pool;
  if (the_pool != NULL) {
    a_pool = NULL;
  } else {
    the_pool = new NdbPool(cc, max_ndb_obj, no_conn_obj);
    if (!the_pool->init(init_no_ndb_objects)) {
      delete the_pool;
      the_pool = NULL;
    }
    a_pool = the_pool;
  }
  NdbMutex* temp = pool_mutex;
  if (a_pool == NULL) {
    pool_mutex = NULL;
  }
  NdbMutex_Unlock(pool_mutex);
  if (a_pool == NULL) {
    NdbMutex_Destroy(temp);
  }
  return a_pool;
}

void
NdbPool::drop_instance()
{
  if (pool_mutex == NULL) {
    return;
  }
  NdbMutex_Lock(pool_mutex);
  the_pool->release_all();
  delete the_pool;
  the_pool = NULL;
  NdbMutex* temp = pool_mutex;
  NdbMutex_Unlock(temp);
  NdbMutex_Destroy(temp);
}

bool
NdbPool::initPoolMutex()
{
  bool ret_result = false;
  if (pool_mutex == NULL) {
    pool_mutex = NdbMutex_Create();
    ret_result = ((pool_mutex == NULL) ? false : true);
  }
  return ret_result;
}

NdbPool::NdbPool(Ndb_cluster_connection* cc,
		 Uint32 max_no_objects,
                 Uint32 no_conn_objects)
{
  if (no_conn_objects > 1024) {
    no_conn_objects = 1024;
  }
  if (max_no_objects > MAX_NDB_OBJECTS) {
    max_no_objects = MAX_NDB_OBJECTS;
  } else if (max_no_objects == 0) {
    max_no_objects = 1;
  }
  m_max_ndb_objects = max_no_objects;
  m_no_of_conn_objects = no_conn_objects;
  m_no_of_objects = 0;
  m_waiting = 0;
  m_pool_reference = NULL;
  m_hash_entry = NULL;
  m_first_free = NULL_POOL;
  m_first_not_in_use = NULL_POOL;
  m_last_free = NULL_POOL;
  input_pool_cond = NULL;
  output_pool_cond = NULL;
  m_output_queue = 0;
  m_input_queue = 0;
  m_signal_count = 0;
  m_cluster_connection = cc;
}

NdbPool::~NdbPool()
{
  NdbCondition_Destroy(input_pool_cond);
  NdbCondition_Destroy(output_pool_cond);
}

void
NdbPool::release_all()
{
  int i;
  for (i = 0; i < m_max_ndb_objects + 1; i++) {
    if (m_pool_reference[i].ndb_reference != NULL) {
      assert(m_pool_reference[i].in_use);
      assert(m_pool_reference[i].free_entry);
      delete m_pool_reference[i].ndb_reference;
    }
  }
  delete [] m_pool_reference;
  delete [] m_hash_entry;
  m_pool_reference = NULL;
  m_hash_entry = NULL;
}

bool
NdbPool::init(Uint32 init_no_objects)
{
  bool ret_result = false;
  int i;
  do {
    input_pool_cond = NdbCondition_Create();
    output_pool_cond = NdbCondition_Create();
    if (input_pool_cond == NULL || output_pool_cond == NULL) {
      break;
    }
    if (init_no_objects > m_max_ndb_objects) {
      init_no_objects = m_max_ndb_objects;
    }
    if (init_no_objects == 0) {
      init_no_objects = 1;
    }
    m_pool_reference = new NdbPool::POOL_STRUCT[m_max_ndb_objects + 1];
    m_hash_entry     = new Uint8[POOL_HASH_TABLE_SIZE];

    if ((m_pool_reference == NULL) || (m_hash_entry == NULL)) {
      delete [] m_pool_reference;
      delete [] m_hash_entry;
      break;
    }
    for (i = 0; i < m_max_ndb_objects + 1; i++) {
      m_pool_reference[i].ndb_reference = NULL;
      m_pool_reference[i].in_use = false;
      m_pool_reference[i].next_free_object = i+1;
      m_pool_reference[i].prev_free_object = i-1;
      m_pool_reference[i].next_db_object = NULL_POOL;
      m_pool_reference[i].prev_db_object = NULL_POOL;
    }
    for (i = 0; i < POOL_HASH_TABLE_SIZE; i++) {
      m_hash_entry[i] = NULL_HASH;
    }
    m_pool_reference[m_max_ndb_objects].next_free_object = NULL_POOL;
    m_pool_reference[1].prev_free_object = NULL_POOL;
    m_first_not_in_use = 1;
    m_no_of_objects = init_no_objects;
    for (i = init_no_objects; i > 0 ; i--) {
      Uint32 fake_id;
      if (!allocate_ndb(fake_id, (const char*)NULL, (const char*)NULL)) {
        release_all();
        break;
      }
    }
    ret_result = true;
    break;
  } while (1);
  return ret_result;
}

/*
Get an Ndb object.
Input:
hint_id: 0 = no hint, otherwise a hint of which Ndb object the thread
         used the last time.
a_db_name: NULL = don't check for database specific Ndb  object, otherwise
           a hint of which database is preferred.
Output:
hint_id: Returns id of Ndb object returned
Return value: Ndb object pointer
*/
Ndb*
NdbPool::get_ndb_object(Uint32 &hint_id,
                        const char* a_catalog_name,
                        const char* a_schema_name)
{
  Ndb* ret_ndb = NULL;
  Uint32 hash_entry = compute_hash(a_schema_name);
  NdbMutex_Lock(pool_mutex);
  while (1) {
    /*
    We start by checking if we can use the hinted Ndb object.
    */
    if ((ret_ndb = get_hint_ndb(hint_id, hash_entry)) != NULL) {
      break;
    }
    /*
    The hinted Ndb object was not free. We need to allocate another object.
    We start by checking for a free Ndb object connected to the same database.
    */
    if (a_schema_name && (ret_ndb = get_db_hash(hint_id,
                                                hash_entry,
                                                a_catalog_name,
                                                a_schema_name))) {
      break;
    }
    /*
    No Ndb object connected to the preferred database was found.
    We look for a free Ndb object in general.
    */
    if ((ret_ndb = get_free_list(hint_id, hash_entry)) != NULL) {
      break;
    }
    /*
    No free Ndb object was found. If we haven't allocated objects up until the
    maximum number yet then we can allocate a new Ndb object here.
    */
    if (m_no_of_objects < m_max_ndb_objects) {
      if (allocate_ndb(hint_id, a_catalog_name, a_schema_name)) {
        assert((ret_ndb = get_hint_ndb(hint_id, hash_entry)) != NULL);
        break;
      }
    }
    /*
    We need to wait until an Ndb object becomes
    available.
    */
    if ((ret_ndb = wait_free_ndb(hint_id)) != NULL) {
      break;
    }
    /*
    Not even after waiting were we able to get hold of an Ndb object. We 
    return NULL to indicate this problem.
    */
    ret_ndb = NULL;
    break;
  }
  NdbMutex_Unlock(pool_mutex);
  if (ret_ndb != NULL) {
    /*
    We need to set the catalog and schema name of the Ndb object before
    returning it to the caller.
    */
    ret_ndb->setCatalogName(a_catalog_name);
    ret_ndb->setSchemaName(a_schema_name);
  }
  return ret_ndb;
}

void
NdbPool::return_ndb_object(Ndb* returned_ndb, Uint32 id)
{
  NdbMutex_Lock(pool_mutex);
  assert(id <= m_max_ndb_objects);
  assert(id != 0);
  assert(returned_ndb == m_pool_reference[id].ndb_reference);
  bool wait_cond = m_waiting;
  if (wait_cond) {
    NdbCondition* pool_cond;
    if (m_signal_count > 0) {
      pool_cond = output_pool_cond;
      m_signal_count--;
    } else {
      pool_cond = input_pool_cond;
    }
    add_wait_list(id);
    NdbMutex_Unlock(pool_mutex);
    NdbCondition_Signal(pool_cond);
  } else {
    add_free_list(id);
    add_db_hash(id);
    NdbMutex_Unlock(pool_mutex);
  }
}

bool
NdbPool::allocate_ndb(Uint32 &id,
                      const char* a_catalog_name,
                      const char* a_schema_name)
{
  Ndb* a_ndb;
  if (m_first_not_in_use == NULL_POOL) {
    return false;
  }
  if (a_schema_name) {
    a_ndb = new Ndb(m_cluster_connection, a_schema_name, a_catalog_name);
  } else {
    a_ndb = new Ndb(m_cluster_connection, "");
  }
  if (a_ndb == NULL) {
    return false;
  }
  a_ndb->init(m_no_of_conn_objects);
  m_no_of_objects++;

  id = m_first_not_in_use;
  Uint32 allocated_id = m_first_not_in_use;
  m_first_not_in_use = m_pool_reference[allocated_id].next_free_object;

  m_pool_reference[allocated_id].ndb_reference = a_ndb;
  m_pool_reference[allocated_id].in_use = true;
  m_pool_reference[allocated_id].free_entry = false;

  add_free_list(allocated_id);
  add_db_hash(allocated_id);
  return true;
}

void
NdbPool::add_free_list(Uint32 id)
{
  assert(!m_pool_reference[id].free_entry);
  assert(m_pool_reference[id].in_use);
  m_pool_reference[id].free_entry = true;
  m_pool_reference[id].next_free_object = m_first_free;
  m_pool_reference[id].prev_free_object = (Uint8)NULL_POOL;
  m_first_free = (Uint8)id;
  if (m_last_free == (Uint8)NULL_POOL) {
    m_last_free = (Uint8)id;
  }
}

void
NdbPool::add_db_hash(Uint32 id)
{
  Ndb* t_ndb = m_pool_reference[id].ndb_reference;
  const char* schema_name = t_ndb->getSchemaName();
  Uint32 hash_entry = compute_hash(schema_name);
  Uint8 next_db_entry = m_hash_entry[hash_entry];
  m_pool_reference[id].next_db_object = next_db_entry;
  m_pool_reference[id].prev_db_object = (Uint8)NULL_HASH;
  m_hash_entry[hash_entry] = (Uint8)id;
}

Ndb*
NdbPool::get_free_list(Uint32 &id, Uint32 hash_entry)
{
  if (m_first_free == NULL_POOL) {
    return NULL;
  }
  id = m_first_free;
  Ndb* ret_ndb = get_hint_ndb(m_first_free, hash_entry);
  assert(ret_ndb != NULL);
  return ret_ndb;
}

Ndb*
NdbPool::get_db_hash(Uint32 &id,
                     Uint32 hash_entry,
                     const char *a_catalog_name,
                     const char *a_schema_name)
{
  Uint32 entry_id = m_hash_entry[hash_entry];
  bool found = false;
  while (entry_id != NULL_HASH) {
    Ndb* t_ndb = m_pool_reference[entry_id].ndb_reference;
    const char *a_ndb_catalog_name = t_ndb->getCatalogName();
    if (strcmp(a_catalog_name, a_ndb_catalog_name) == 0) {
      const char *a_ndb_schema_name = t_ndb->getSchemaName();
      if (strcmp(a_schema_name, a_ndb_schema_name) == 0) {
        found = true;
        break;
      }
    }
    entry_id = m_pool_reference[entry_id].next_db_object;
  }
  if (found) {
    id = entry_id;
    Ndb* ret_ndb = get_hint_ndb(entry_id, hash_entry);
    assert(ret_ndb != NULL);
    return ret_ndb;
  }
  return NULL;
}

Ndb*
NdbPool::get_hint_ndb(Uint32 hint_id, Uint32 hash_entry)
{
  Ndb* ret_ndb = NULL;
  do {
    if ((hint_id != 0) &&
        (hint_id <= m_max_ndb_objects) &&
        (m_pool_reference[hint_id].in_use) &&
        (m_pool_reference[hint_id].free_entry)) {
      ret_ndb = m_pool_reference[hint_id].ndb_reference;
      if (ret_ndb != NULL) {
        break;
      } else {
        assert(false);
      }
    }
    return NULL;
  } while (1);
  /*
  This is where we remove the entry from the free list and from the db hash
  table.
  */
  remove_free_list(hint_id);
  remove_db_hash(hint_id, hash_entry);
  return ret_ndb;
}

void
NdbPool::remove_free_list(Uint32 id)
{
  Uint16 next_free_entry = m_pool_reference[id].next_free_object;
  Uint16 prev_free_entry = m_pool_reference[id].prev_free_object;
  if (prev_free_entry == (Uint8)NULL_POOL) {
    m_first_free = next_free_entry;
  } else {
    m_pool_reference[prev_free_entry].next_free_object = next_free_entry;
  }
  if (next_free_entry == (Uint8)NULL_POOL) {
    m_last_free = prev_free_entry;
  } else {
    m_pool_reference[next_free_entry].prev_free_object = prev_free_entry;
  }
  m_pool_reference[id].next_free_object = NULL_POOL;
  m_pool_reference[id].prev_free_object = NULL_POOL;
  m_pool_reference[id].free_entry = false;
}

void
NdbPool::remove_db_hash(Uint32 id, Uint32 hash_entry)
{
  Uint16 next_free_entry = m_pool_reference[id].next_db_object;
  Uint16 prev_free_entry = m_pool_reference[id].prev_db_object;
  if (prev_free_entry == (Uint8)NULL_HASH) {
    m_hash_entry[hash_entry] = (Uint8)next_free_entry;
  } else {
    m_pool_reference[prev_free_entry].next_db_object = next_free_entry;
  }
  if (next_free_entry == (Uint8)NULL_HASH) {
    ;
  } else {
    m_pool_reference[next_free_entry].prev_db_object = prev_free_entry;
  }
  m_pool_reference[id].next_db_object = NULL_HASH;
  m_pool_reference[id].prev_db_object = NULL_HASH;
}

Uint32
NdbPool::compute_hash(const char *a_schema_name)
{
  Uint32 len = Uint32(strlen(a_schema_name));
  Uint32 h = 147;
  for (Uint32 i = 0; i < len; i++) {
    Uint32 c = a_schema_name[i];
    h = (h << 5) + h + c;
  }
  h &= (POOL_HASH_TABLE_SIZE - 1);
  return h;
}

Ndb*
NdbPool::wait_free_ndb(Uint32 &id)
{
  int res;
  int time_out = 3500;
  do {
    NdbCondition* tmp = input_pool_cond;
    m_waiting++;
    m_input_queue++;
    time_out -= 500;
    res = NdbCondition_WaitTimeout(input_pool_cond, pool_mutex, time_out);
    if (tmp == input_pool_cond) {
      m_input_queue--;
    } else {
      m_output_queue--;
      if (m_output_queue == 0) {
        switch_condition_queue();
      }
    }
    m_waiting--;
  } while (res == 0 && m_first_wait == NULL_POOL);
  if (res != 0 && m_first_wait == NULL_POOL) {
    return NULL;
  }
  id = m_first_wait;
  remove_wait_list();
  assert(m_waiting != 0 || m_first_wait == NULL_POOL);
  return m_pool_reference[id].ndb_reference;
}

void
NdbPool::remove_wait_list()
{
  Uint32 id = m_first_wait;
  m_first_wait = m_pool_reference[id].next_free_object;
  m_pool_reference[id].next_free_object = NULL_POOL;
  m_pool_reference[id].prev_free_object = NULL_POOL;
  m_pool_reference[id].free_entry = false;
}

void
NdbPool::add_wait_list(Uint32 id)
{
  m_pool_reference[id].next_free_object = m_first_wait;
  m_first_wait = id;
}

void
NdbPool::switch_condition_queue()
{
  m_signal_count = m_input_queue;
  Uint16 move_queue = m_input_queue;
  m_input_queue = m_output_queue;
  m_output_queue = move_queue;

  NdbCondition* move_cond = input_pool_cond;
  input_pool_cond = output_pool_cond;
  output_pool_cond = move_cond;
}

