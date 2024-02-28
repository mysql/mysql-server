/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/pfs_program.cc
  Statement Digest data structures (implementation).
*/

/*
  This code needs extra visibility in the lexer structures
*/

#include "storage/perfschema/pfs_program.h"

#include <assert.h>
#include <string.h>

#include "my_compiler.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/strings/m_ctype.h"
#include "sql/mysqld_cs.h"  // system_charset_info
#include "sql_string.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_setup_object.h"

LF_HASH program_hash;
static bool program_hash_inited = false;

/**
  Initialize table EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM.
  @param param performance schema sizing
*/
int init_program(const PFS_global_param *param) {
  if (global_program_container.init(param->m_program_sizing)) {
    return 1;
  }

  reset_esms_by_program();
  return 0;
}

/** Cleanup table EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM. */
void cleanup_program() { global_program_container.cleanup(); }

static const uchar *program_hash_get_key(const uchar *entry, size_t *length) {
  const PFS_program *const *typed_entry;
  const PFS_program *program;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_program *const *>(entry);
  assert(typed_entry != nullptr);
  program = *typed_entry;
  assert(program != nullptr);
  *length = sizeof(program->m_key);
  result = &program->m_key;
  return reinterpret_cast<const uchar *>(result);
}

static uint program_hash_func(const LF_HASH *, const uchar *key,
                              size_t key_len [[maybe_unused]]) {
  const PFS_program_key *program_key;
  uint64 nr1;
  uint64 nr2;

  assert(key_len == sizeof(PFS_program_key));
  program_key = reinterpret_cast<const PFS_program_key *>(key);
  assert(program_key != nullptr);

  nr1 = program_key->m_type;
  nr2 = 0;

  program_key->m_schema_name.hash(&nr1, &nr2);

  program_key->m_object_name.hash(&nr1, &nr2);

  return nr1;
}

static int program_hash_cmp_func(const uchar *key1,
                                 size_t key_len1 [[maybe_unused]],
                                 const uchar *key2,
                                 size_t key_len2 [[maybe_unused]]) {
  const PFS_program_key *program_key1;
  const PFS_program_key *program_key2;
  int cmp;

  assert(key_len1 == sizeof(PFS_program_key));
  assert(key_len2 == sizeof(PFS_program_key));
  program_key1 = reinterpret_cast<const PFS_program_key *>(key1);
  program_key2 = reinterpret_cast<const PFS_program_key *>(key2);
  assert(program_key1 != nullptr);
  assert(program_key2 != nullptr);

  if (program_key1->m_type > program_key2->m_type) {
    return +1;
  }

  if (program_key1->m_type < program_key2->m_type) {
    return -1;
  }

  cmp = program_key1->m_schema_name.sort(&program_key2->m_schema_name);
  if (cmp != 0) {
    return cmp;
  }

  cmp = program_key1->m_object_name.sort(&program_key2->m_object_name);
  return cmp;
}

/**
  Initialize the program hash.
  @return 0 on success
*/
int init_program_hash(const PFS_global_param *param) {
  if ((!program_hash_inited) && (param->m_program_sizing != 0)) {
    lf_hash_init3(&program_hash, sizeof(PFS_program *), LF_HASH_UNIQUE,
                  program_hash_get_key, program_hash_func,
                  program_hash_cmp_func, nullptr /* ctor */, nullptr /* dtor */,
                  nullptr /* init */);
    program_hash_inited = true;
  }
  return 0;
}

/** Cleanup the program hash. */
void cleanup_program_hash() {
  if (program_hash_inited) {
    lf_hash_destroy(&program_hash);
    program_hash_inited = false;
  }
}

static void set_program_key(PFS_program_key *key, enum_object_type object_type,
                            const char *schema_name, uint schema_name_length,
                            const char *object_name, uint object_name_length) {
  assert(schema_name_length <= NAME_LEN);
  assert(object_name_length <= NAME_LEN);

  key->m_type = object_type;

  key->m_schema_name.set(schema_name, schema_name_length);

  key->m_object_name.set(object_name, object_name_length);
}

void PFS_program::reset_data() {
  m_sp_stat.reset();
  m_stmt_stat.reset();
}

static void fct_reset_esms_by_program(PFS_program *pfs) { pfs->reset_data(); }

void reset_esms_by_program() {
  global_program_container.apply_all(fct_reset_esms_by_program);
}

static LF_PINS *get_program_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_program_hash_pins == nullptr)) {
    if (!program_hash_inited) {
      return nullptr;
    }
    thread->m_program_hash_pins = lf_hash_get_pins(&program_hash);
  }
  return thread->m_program_hash_pins;
}

PFS_program *find_or_create_program(
    PFS_thread *thread, enum_object_type object_type, const char *object_name,
    uint object_name_length, const char *schema_name, uint schema_name_length) {
  bool is_enabled, is_timed;

  LF_PINS *pins = get_program_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    global_program_container.m_lost++;
    return nullptr;
  }

  /* Prepare program key */
  PFS_program_key key;
  set_program_key(&key, object_type, schema_name, schema_name_length,
                  object_name, object_name_length);

  PFS_program **entry;
  PFS_program *pfs = nullptr;
  uint retry_count = 0;
  constexpr uint retry_max = 3;
  pfs_dirty_state dirty_state;

search:
  entry = reinterpret_cast<PFS_program **>(
      lf_hash_search(&program_hash, pins, &key, sizeof(key)));

  if (entry && (entry != MY_LF_ERRPTR)) {
    /* If record already exists then return its pointer. */
    pfs = *entry;
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  /*
    First time while inserting this record to program array we need to
    find out if it is enabled and timed.
  */
  lookup_setup_object_routine(thread, key.m_type, &key.m_schema_name,
                              &key.m_object_name, &is_enabled, &is_timed);

  /* Else create a new record in program stat array. */
  pfs = global_program_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    /* Do the assignments. */
    pfs->m_key = key;
    pfs->m_enabled = is_enabled;
    pfs->m_timed = is_timed;

    /* Insert this record. */
    const int res = lf_hash_insert(&program_hash, pins, &pfs);

    if (likely(res == 0)) {
      pfs->m_lock.dirty_to_allocated(&dirty_state);
      return pfs;
    }

    global_program_container.dirty_to_free(&dirty_state, pfs);

    if (res > 0) {
      /* Duplicate insert by another thread */
      if (++retry_count > retry_max) {
        /* Avoid infinite loops */
        global_program_container.m_lost++;
        return nullptr;
      }
      goto search;
    }
    /* OOM in lf_hash_insert */
    global_program_container.m_lost++;
    return nullptr;
  }

  return nullptr;
}

void drop_program(PFS_thread *thread, enum_object_type object_type,
                  const char *object_name, uint object_name_length,
                  const char *schema_name, uint schema_name_length) {
  LF_PINS *pins = get_program_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return;
  }

  /* Prepare program key */
  PFS_program_key key;
  set_program_key(&key, object_type, schema_name, schema_name_length,
                  object_name, object_name_length);

  PFS_program **entry;
  entry = reinterpret_cast<PFS_program **>(
      lf_hash_search(&program_hash, pins, &key, sizeof(key)));

  if (entry && (entry != MY_LF_ERRPTR)) {
    PFS_program *pfs = *entry;

    lf_hash_delete(&program_hash, pins, &key, sizeof(key));
    global_program_container.deallocate(pfs);
  }

  lf_hash_search_unpin(pins);
}

void PFS_program::refresh_setup_object_flags(PFS_thread *thread) {
  lookup_setup_object_routine(thread, m_key.m_type, &m_key.m_schema_name,
                              &m_key.m_object_name, &m_enabled, &m_timed);
}
