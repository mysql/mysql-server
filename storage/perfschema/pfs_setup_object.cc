/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/pfs_setup_object.cc
  Performance schema setup object (implementation).
*/

#include "storage/perfschema/pfs_setup_object.h"

#include <assert.h>
#include "my_base.h"
#include "my_compiler.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "sql_string.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_stat.h"

/**
  @addtogroup performance_schema_buffers
  @{
*/

uint setup_objects_version = 0;

LF_HASH setup_object_hash;
static bool setup_object_hash_inited = false;

/**
  Initialize the setup object buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_setup_object(const PFS_global_param *param) {
  return global_setup_object_container.init(param->m_setup_object_sizing);
}

/** Cleanup all the setup object buffers. */
void cleanup_setup_object(void) { global_setup_object_container.cleanup(); }

static const uchar *setup_object_hash_get_key(const uchar *entry,
                                              size_t *length) {
  const PFS_setup_object *const *typed_entry;
  const PFS_setup_object *setup_object;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_setup_object *const *>(entry);
  assert(typed_entry != nullptr);
  setup_object = *typed_entry;
  assert(setup_object != nullptr);
  *length = sizeof(setup_object->m_key);
  result = &setup_object->m_key;
  return reinterpret_cast<const uchar *>(result);
}

static bool is_table(enum_object_type object_type) {
  if ((object_type == OBJECT_TYPE_TABLE) ||
      (object_type == OBJECT_TYPE_TEMPORARY_TABLE)) {
    return true;
  }
  return false;
}

static uint setup_object_hash_func(const LF_HASH *, const uchar *key,
                                   size_t key_len [[maybe_unused]]) {
  const PFS_setup_object_key *setup_object_key;
  uint64 nr1;
  uint64 nr2;

  assert(key_len == sizeof(PFS_setup_object_key));
  setup_object_key = reinterpret_cast<const PFS_setup_object_key *>(key);
  assert(setup_object_key != nullptr);

  nr1 = 0;
  nr2 = 0;

  nr1 = setup_object_key->m_object_type;
  setup_object_key->m_schema_name.hash(&nr1, &nr2);

  if (is_table(setup_object_key->m_object_type)) {
    setup_object_key->m_object_name.hash_as_table(&nr1, &nr2);
  } else {
    setup_object_key->m_object_name.hash_as_routine(&nr1, &nr2);
  }

  return nr1;
}

static int setup_object_hash_cmp_func(const uchar *key1,
                                      size_t key_len1 [[maybe_unused]],
                                      const uchar *key2,
                                      size_t key_len2 [[maybe_unused]]) {
  const PFS_setup_object_key *setup_object_key1;
  const PFS_setup_object_key *setup_object_key2;
  int cmp;

  assert(key_len1 == sizeof(PFS_setup_object_key));
  assert(key_len2 == sizeof(PFS_setup_object_key));
  setup_object_key1 = reinterpret_cast<const PFS_setup_object_key *>(key1);
  setup_object_key2 = reinterpret_cast<const PFS_setup_object_key *>(key2);
  assert(setup_object_key1 != nullptr);
  assert(setup_object_key2 != nullptr);

  if (setup_object_key1->m_object_type > setup_object_key2->m_object_type) {
    return +1;
  }

  if (setup_object_key1->m_object_type < setup_object_key2->m_object_type) {
    return -1;
  }

  cmp =
      setup_object_key1->m_schema_name.sort(&setup_object_key2->m_schema_name);
  if (cmp != 0) {
    return cmp;
  }

  if (is_table(setup_object_key1->m_object_type)) {
    cmp = setup_object_key1->m_object_name.sort_as_table(
        &setup_object_key2->m_object_name);
  } else {
    cmp = setup_object_key1->m_object_name.sort_as_routine(
        &setup_object_key2->m_object_name);
  }

  return cmp;
}

/**
  Initialize the setup objects hash.
  @return 0 on success
*/
int init_setup_object_hash(const PFS_global_param *param) {
  if ((!setup_object_hash_inited) && (param->m_setup_object_sizing != 0)) {
    lf_hash_init3(&setup_object_hash, sizeof(PFS_setup_object *),
                  LF_HASH_UNIQUE, setup_object_hash_get_key,
                  setup_object_hash_func, setup_object_hash_cmp_func,
                  nullptr /* ctor */, nullptr /* dtor */, nullptr /* init */);
    setup_object_hash_inited = true;
  }
  return 0;
}

/** Cleanup the setup objects hash. */
void cleanup_setup_object_hash(void) {
  if (setup_object_hash_inited) {
    lf_hash_destroy(&setup_object_hash);
    setup_object_hash_inited = false;
  }
}

static LF_PINS *get_setup_object_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_setup_object_hash_pins == nullptr)) {
    if (!setup_object_hash_inited) {
      return nullptr;
    }
    thread->m_setup_object_hash_pins = lf_hash_get_pins(&setup_object_hash);
  }
  return thread->m_setup_object_hash_pins;
}

static void set_setup_object_key(PFS_setup_object_key *key,
                                 enum_object_type object_type,
                                 const PFS_schema_name *schema,
                                 const PFS_object_name *object) {
  key->m_object_type = object_type;
  key->m_schema_name = *schema;
  key->m_object_name = *object;
}

int insert_setup_object(enum_object_type object_type,
                        const PFS_schema_name *schema,
                        const PFS_object_name *object, bool enabled,
                        bool timed) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_object *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_setup_object_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    set_setup_object_key(&pfs->m_key, object_type, schema, object);
    pfs->m_enabled = enabled;
    pfs->m_timed = timed;

    int res;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    res = lf_hash_insert(&setup_object_hash, pins, &pfs);
    if (likely(res == 0)) {
      setup_objects_version++;
      return 0;
    }

    global_setup_object_container.deallocate(pfs);

    if (res > 0) {
      return HA_ERR_FOUND_DUPP_KEY;
    }
    /* OOM in lf_hash_insert */
    return HA_ERR_OUT_OF_MEM;
  }

  return HA_ERR_RECORD_FILE_FULL;
}

int delete_setup_object(enum_object_type object_type,
                        const PFS_schema_name *schema,
                        const PFS_object_name *object) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_object_key key;
  set_setup_object_key(&key, object_type, schema, object);

  PFS_setup_object **entry;
  entry = reinterpret_cast<PFS_setup_object **>(
      lf_hash_search(&setup_object_hash, pins, &key, sizeof(key)));

  if (entry && (entry != MY_LF_ERRPTR)) {
    PFS_setup_object *pfs = *entry;
    lf_hash_delete(&setup_object_hash, pins, &key, sizeof(key));
    global_setup_object_container.deallocate(pfs);
  }

  lf_hash_search_unpin(pins);

  setup_objects_version++;
  return 0;
}

class Proc_reset_setup_object : public PFS_buffer_processor<PFS_setup_object> {
 public:
  explicit Proc_reset_setup_object(LF_PINS *pins) : m_pins(pins) {}

  void operator()(PFS_setup_object *pfs) override {
    lf_hash_delete(&setup_object_hash, m_pins, &pfs->m_key, sizeof(pfs->m_key));

    global_setup_object_container.deallocate(pfs);
  }

 private:
  LF_PINS *m_pins;
};

int reset_setup_object() {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  Proc_reset_setup_object proc(pins);
  // FIXME: delete helper instead
  global_setup_object_container.apply(proc);

  setup_objects_version++;
  return 0;
}

long setup_object_count() { return setup_object_hash.count; }

static void lookup_setup_object(PFS_thread *thread,
                                enum_object_type object_type,
                                const PFS_schema_name *schema,
                                const PFS_object_name *object, bool *enabled,
                                bool *timed) {
  PFS_setup_object_key key;
  PFS_setup_object **entry;
  PFS_setup_object *pfs;
  int i;

  /*
    The table I/O instrumentation uses "TABLE" and "TEMPORARY TABLE".
    SETUP_OBJECT uses "TABLE" for both concepts.
    There is no way to provide a different setup for:
    - TABLE foo.bar
    - TEMPORARY TABLE foo.bar
  */
  assert(object_type != OBJECT_TYPE_TEMPORARY_TABLE);

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    *enabled = false;
    *timed = false;
    return;
  }

  PFS_schema_name any_schema;
  PFS_object_name any_object;
  any_schema.set("%", 1);

  /*
    In practice, any_object is '%' in both cases,
    but we want to enforce a strict api to make sure
    the proper collation rules are used,
    which depends on the object type.
  */
  if (object_type == OBJECT_TYPE_TABLE) {
    any_object.set_as_table("%", 1);
  } else {
    any_object.set_as_routine("%", 1);
  }

  for (i = 1; i <= 3; i++) {
    switch (i) {
      case 1:
        /* Lookup OBJECT_TYPE + OBJECT_SCHEMA + OBJECT_NAME in SETUP_OBJECTS */
        set_setup_object_key(&key, object_type, schema, object);
        break;
      case 2:
        /* Lookup OBJECT_TYPE + OBJECT_SCHEMA + "%" in SETUP_OBJECTS */
        set_setup_object_key(&key, object_type, schema, &any_object);
        break;
      case 3:
        /* Lookup OBJECT_TYPE + "%" + "%" in SETUP_OBJECTS */
        set_setup_object_key(&key, object_type, &any_schema, &any_object);
        break;
    }
    entry = reinterpret_cast<PFS_setup_object **>(
        lf_hash_search(&setup_object_hash, pins, &key, sizeof(key)));

    if (entry && (entry != MY_LF_ERRPTR)) {
      pfs = *entry;
      *enabled = pfs->m_enabled;
      *timed = pfs->m_timed;
      lf_hash_search_unpin(pins);
      return;
    }

    lf_hash_search_unpin(pins);
  }
  *enabled = false;
  *timed = false;
  return;
}

void lookup_setup_object_table(PFS_thread *thread, enum_object_type object_type,
                               const PFS_schema_name *schema_name,
                               const PFS_table_name *table_name, bool *enabled,
                               bool *timed) {
  PFS_object_name object_name;
  object_name = *table_name;
  lookup_setup_object(thread, object_type, schema_name, &object_name, enabled,
                      timed);
}

void lookup_setup_object_routine(PFS_thread *thread,
                                 enum_object_type object_type,
                                 const PFS_schema_name *schema_name,
                                 const PFS_routine_name *routine_name,
                                 bool *enabled, bool *timed) {
  PFS_object_name object_name;
  object_name = *routine_name;
  lookup_setup_object(thread, object_type, schema_name, &object_name, enabled,
                      timed);
}

/** @} */
