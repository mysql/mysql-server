/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
  @section ndbPool              Pooling of NDB objects
  This class implements pooling of NDB objects to support MySQL, ODBC and
  any application with a great number of threads.

  The general idea is to let the NdbPool class administer all Ndb objects.
  When a thread needs a Ndb object it request a Ndb object from the Pool.
  This interface contains some hints to ensure that the proper Ndb object
  is returned.

  The object contains an array of references to all Ndb objects together with
  an indication of whether the object is free or not.

  The idea is that the thread should keep track of the Ndb object it used the
  last time. If this object is still free it will simply get this object
  back. If the number of threads do not exceed the number of Ndb objects this
  will always be successful. In certain situations the number of threads will
  be much greater than the number of Ndb objects. In this situation the Pool
  will attempt to provide an object that is attached to the same schema
  as the thread is connected to. If this is not possible it will attempt to
  get any free Ndb object. If not even this is possible the Pool will wait
  until an Ndb object becomes free. If an Ndb object becomes available in
  time it will deliver this Ndb object. In the worst case the call will
  time-out and return NULL to indicate no free Ndb object was found in time.

  The implementation uses an array of structs which contain a reference to a
  Ndb object, whether it is in use currently and a number of references to
  set up linked lists of
  1) Free objects on a schema
  2) Free objects in LIFO order

  Usage:
  The class is a singleton.
  The first step is to call create_instance(..). If successful this will
  create the NdbPool object and return a reference to it. When completed
  drop_instance is called to remove the NdbPool object and all memory and
  other resources attached to it.

  After initialising the NdbPool object all threads can now start using the
  NdbPool. There are two methods in normal usage mode. The first
  get_ndb_object gets a Ndb object and the second return_ndb_object returns
  an Ndb object. The user of the NdbPool must keep track of the identity
  of the Ndb object. The idea is that this identity can also be used to
  find the object quickly again unless another thread have taken it. If the
  user wants any Ndb object it requests identity 0 which means any here.

  When constructing the NdbPool one can set the number of NdbConnection
  objects which are allowed in all Ndb objects. For use in synchronous
  applications such as the MySQL server 4 objects should be enough. When
  using the NdbPool for asynchronous applications one should use 1024 to
  enable a high level of parallelism. It is also possible to set the
  maximum number of Ndb objects in the pool and the initial number of
  Ndb objects allocated.
*/

#ifndef NdbPool_H
#define NdbPool_H

#include <NdbCondition.h>
#include <NdbMutex.h>
#include <Ndb.hpp>
#include <NdbOut.hpp>

class NdbPool {
#define NULL_POOL 0
#define NULL_HASH 0xFF
#define POOL_HASH_TABLE_SIZE 32
#define MAX_NDB_OBJECTS 240
  struct POOL_STRUCT {
    Ndb *ndb_reference;
    bool in_use;
    bool free_entry;
    Uint16 next_free_object;
    Uint16 prev_free_object;
    Uint16 next_db_object;
    Uint16 prev_db_object;
  };

 public:
  static NdbPool *create_instance(Ndb_cluster_connection *,
                                  Uint32 max_ndb_objects = 240,
                                  Uint32 no_conn_obj = 4,
                                  Uint32 init_no_ndb_objects = 8);
  static void drop_instance();
  Ndb *get_ndb_object(Uint32 &hint_id, const char *a_catalog_name,
                      const char *a_schema_name);
  void return_ndb_object(Ndb *returned_object, Uint32 id);

 private:
  bool init(Uint32 initial_no_of_ndb_objects = 8);
  void release_all();
  static bool initPoolMutex();
  NdbPool(Ndb_cluster_connection *, Uint32 max_no_of_ndb_objects,
          Uint32 no_conn_objects);
  ~NdbPool();
  /*
  We have three lists:
  1) A list for entries not in use
  2) A list for free entries
  3) A hash table with schema name and database name as key

  These lists are all initialised in the init call.
  The list for entries not in use is very simple since the current
  implementation have not yet any handling of dropping Ndb objects
  until all Ndb objects are dropped.
  */
  void add_free_list(Uint32 id);
  void remove_free_list(Uint32 id);
  Ndb *get_free_list(Uint32 &id, Uint32 hash_entry);

  void add_db_hash(Uint32 id);
  void remove_db_hash(Uint32 id, Uint32 hash_entry);
  Ndb *get_db_hash(Uint32 &id, Uint32 hash_entry, const char *a_catalog_name,
                   const char *a_schema_name);

  bool allocate_ndb(Uint32 &id, const char *a_catalog_name,
                    const char *a_schema_name);
  Ndb *get_hint_ndb(Uint32 id, Uint32 hash_entry);
  Ndb *wait_free_ndb(Uint32 &id);
  Uint32 compute_hash(const char *a_schema_name);
  void add_wait_list(Uint32 id);
  void remove_wait_list();
  void switch_condition_queue();

  static NdbMutex *pool_mutex;
  struct NdbCondition *input_pool_cond;
  struct NdbCondition *output_pool_cond;

  POOL_STRUCT *m_pool_reference;
  Uint8 *m_hash_entry;

  bool m_inited;
  Uint32 m_no_of_conn_objects;

  Uint16 m_no_of_objects;
  Uint16 m_max_ndb_objects;
  Uint16 m_first_free;
  Uint16 m_last_free;
  Uint16 m_first_not_in_use;
  Uint16 m_waiting;
  Uint16 m_first_wait;
  Uint16 m_input_queue;
  Uint16 m_output_queue;
  Uint16 m_signal_count;

  Ndb_cluster_connection *m_cluster_connection;
};
#endif
