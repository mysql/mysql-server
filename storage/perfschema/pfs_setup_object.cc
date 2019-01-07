/* Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
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
  DBUG_ASSERT(typed_entry != NULL);
  setup_object = *typed_entry;
  DBUG_ASSERT(setup_object != NULL);
  *length = setup_object->m_key.m_key_length;
  result = setup_object->m_key.m_hash_key;
  return reinterpret_cast<const uchar *>(result);
}

/**
  Initialize the setup objects hash.
  @return 0 on success
*/
int init_setup_object_hash(const PFS_global_param *param) {
  if ((!setup_object_hash_inited) && (param->m_setup_object_sizing != 0)) {
    lf_hash_init(&setup_object_hash, sizeof(PFS_setup_object *), LF_HASH_UNIQUE,
                 0, 0, setup_object_hash_get_key, &my_charset_bin);
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
  if (unlikely(thread->m_setup_object_hash_pins == NULL)) {
    if (!setup_object_hash_inited) {
      return NULL;
    }
    thread->m_setup_object_hash_pins = lf_hash_get_pins(&setup_object_hash);
  }
  return thread->m_setup_object_hash_pins;
}

static void set_setup_object_key(PFS_setup_object_key *key,
                                 enum_object_type object_type,
                                 const char *schema, uint schema_length,
                                 const char *object, uint object_length) {
  DBUG_ASSERT(schema_length <= NAME_LEN);
  DBUG_ASSERT(object_length <= NAME_LEN);

  char *ptr = &key->m_hash_key[0];
  ptr[0] = (char)object_type;
  ptr++;
  memcpy(ptr, schema, schema_length);
  ptr += schema_length;
  ptr[0] = 0;
  ptr++;
  memcpy(ptr, object, object_length);
  ptr += object_length;
  ptr[0] = 0;
  ptr++;
  key->m_key_length = ptr - &key->m_hash_key[0];
}

int insert_setup_object(enum_object_type object_type, const String *schema,
                        const String *object, bool enabled, bool timed) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_object *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_setup_object_container.allocate(&dirty_state);
  if (pfs != NULL) {
    set_setup_object_key(&pfs->m_key, object_type, schema->ptr(),
                         schema->length(), object->ptr(), object->length());
    pfs->m_schema_name = &pfs->m_key.m_hash_key[1];
    pfs->m_schema_name_length = schema->length();
    pfs->m_object_name = pfs->m_schema_name + pfs->m_schema_name_length + 1;
    pfs->m_object_name_length = object->length();
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

int delete_setup_object(enum_object_type object_type, const String *schema,
                        const String *object) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_object_key key;
  set_setup_object_key(&key, object_type, schema->ptr(), schema->length(),
                       object->ptr(), object->length());

  PFS_setup_object **entry;
  entry = reinterpret_cast<PFS_setup_object **>(lf_hash_search(
      &setup_object_hash, pins, key.m_hash_key, key.m_key_length));

  if (entry && (entry != MY_LF_ERRPTR)) {
    PFS_setup_object *pfs = *entry;
    lf_hash_delete(&setup_object_hash, pins, key.m_hash_key, key.m_key_length);
    global_setup_object_container.deallocate(pfs);
  }

  lf_hash_search_unpin(pins);

  setup_objects_version++;
  return 0;
}

class Proc_reset_setup_object : public PFS_buffer_processor<PFS_setup_object> {
 public:
  Proc_reset_setup_object(LF_PINS *pins) : m_pins(pins) {}

  virtual void operator()(PFS_setup_object *pfs) {
    lf_hash_delete(&setup_object_hash, m_pins, pfs->m_key.m_hash_key,
                   pfs->m_key.m_key_length);

    global_setup_object_container.deallocate(pfs);
  }

 private:
  LF_PINS *m_pins;
};

int reset_setup_object() {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    return HA_ERR_OUT_OF_MEM;
  }

  Proc_reset_setup_object proc(pins);
  // FIXME: delete helper instead
  global_setup_object_container.apply(proc);

  setup_objects_version++;
  return 0;
}

long setup_object_count() { return setup_object_hash.count; }

void lookup_setup_object(PFS_thread *thread, enum_object_type object_type,
                         const char *schema_name, int schema_name_length,
                         const char *object_name, int object_name_length,
                         bool *enabled, bool *timed) {
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
  DBUG_ASSERT(object_type != OBJECT_TYPE_TEMPORARY_TABLE);

  LF_PINS *pins = get_setup_object_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    *enabled = false;
    *timed = false;
    return;
  }

  for (i = 1; i <= 3; i++) {
    switch (i) {
      case 1:
        /* Lookup OBJECT_TYPE + OBJECT_SCHEMA + OBJECT_NAME in SETUP_OBJECTS */
        set_setup_object_key(&key, object_type, schema_name, schema_name_length,
                             object_name, object_name_length);
        break;
      case 2:
        /* Lookup OBJECT_TYPE + OBJECT_SCHEMA + "%" in SETUP_OBJECTS */
        set_setup_object_key(&key, object_type, schema_name, schema_name_length,
                             "%", 1);
        break;
      case 3:
        /* Lookup OBJECT_TYPE + "%" + "%" in SETUP_OBJECTS */
        set_setup_object_key(&key, object_type, "%", 1, "%", 1);
        break;
    }
    entry = reinterpret_cast<PFS_setup_object **>(lf_hash_search(
        &setup_object_hash, pins, key.m_hash_key, key.m_key_length));

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

/** @} */
