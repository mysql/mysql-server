/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_share.h"

#include <iostream>
#include <sstream>
#include <unordered_set>

#include "m_string.h"
#include "sql/sql_class.h"
#include "sql/sql_table.h"
#include "sql/strfunc.h"
#include "sql/table.h"
#include "storage/ndb/include/ndbapi/NdbEventOperation.hpp"
#include "storage/ndb/plugin/ndb_conflict.h"
#include "storage/ndb/plugin/ndb_event_data.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_name_util.h"
#include "storage/ndb/plugin/ndb_require.h"
#include "storage/ndb/plugin/ndb_table_map.h"

struct CHARSET_INFO;

extern Ndb *g_ndb;

// Protecting the list of NDB_SHARE's as well as each shares refcount and list
// of references
static mysql_mutex_t shares_mutex;

// List of NDB_SHARE's which correspond to an open table.
static std::unique_ptr<collation_unordered_map<std::string, NDB_SHARE *>>
    ndb_shares;

// List of NDB_SHARE's which have been dropped, they are kept in this list
// until all references to them have been released.
static std::unordered_set<NDB_SHARE *> dropped_shares;

// Format the key used for storing NDB_SHARE's in the list of open tables
static inline void format_key(char *buf, size_t buf_size, const char *db_name,
                              const char *table_name) {
  // Format key in filesystem format, detect temporary named tables and use
  // FN_IS_TMP since those doesn't need to be prefixed with db name or
  // translated to filesystem format.
  // NOTE! This function is still using filesystem format in order to be
  // compatible with the NDB_SHARE_KEY format that's used as key in the list of
  // open NDB_SHARE's.
  build_table_filename(buf, buf_size, db_name, table_name, "",
                       ndb_name_is_temp(table_name) ? FN_IS_TMP : 0);
}

NDB_SHARE::NDB_SHARE(const char *key_arg) {
  /* Allocates enough space for key, db, and table_name */
  key = NDB_SHARE::create_key(key_arg);

  db = NDB_SHARE::key_get_db_name(key);
  table_name = NDB_SHARE::key_get_table_name(key);

  thr_lock_init(&lock);
  mysql_mutex_init(PSI_INSTRUMENT_ME, &mutex, MY_MUTEX_INIT_FAST);

#ifndef NDEBUG
  refs = new Ndb_share_references();
#endif

  // Turn on special flag for mysql.ndb_apply_status table
  if (strcmp(db, "mysql") == 0 && strcmp(table_name, "ndb_apply_status") == 0) {
    DBUG_PRINT("info", ("Setting FLAG_TABLE_IS_APPLY_STATUS"));
    flags |= FLAG_TABLE_IS_APPLY_STATUS;
  }

  // Initialize to preserve same behaviour as zerofilling malloc
  // in prior versions
  tuple_id_range.m_first_tuple_id = 0;
  tuple_id_range.m_last_tuple_id = 0;
  tuple_id_range.m_highest_seen = 0;
}

NDB_SHARE *NDB_SHARE::create(const char *key) {
  if (DBUG_EVALUATE_IF("ndb_share_create_fail1", true, false)) {
    // Simulate failure to create NDB_SHARE
    return nullptr;
  }

  return new (std::nothrow) NDB_SHARE(key);
}

NDB_SHARE::~NDB_SHARE() {
  thr_lock_delete(&lock);
  mysql_mutex_destroy(&mutex);

  // ndb_index_stat_free() should have cleaned up:
  assert(index_stat_list == nullptr);

  teardown_conflict_fn(g_ndb, m_cfn_share);

#ifndef NDEBUG
  assert(m_use_count == 0);
  assert(refs->check_empty());
  delete refs;
#endif

  // Release memory for the variable length strings held by
  // key but also referenced by db, table_name and shadow_table->db etc.
  free_key(key);
}

void NDB_SHARE::destroy(NDB_SHARE *share) { delete share; }

/*
  Struct holding dynamic length strings for NDB_SHARE. The type is
  opaque to the user of NDB_SHARE and should
  only be accessed using NDB_SHARE accessor functions.

  All the strings are zero terminated.

  Layout:
  size_t key_length
  "key"\0
  "db\0"
  "table_name\0"
*/
struct NDB_SHARE_KEY {
  size_t m_key_length;
  char m_buffer[1];
};

NDB_SHARE_KEY *NDB_SHARE::create_key(const char *new_key) {
  const size_t new_key_length = strlen(new_key);

  char db_name_buf[FN_HEADLEN];
  ndb_set_dbname(new_key, db_name_buf);
  const size_t db_name_len = strlen(db_name_buf);

  char table_name_buf[FN_HEADLEN];
  ndb_set_tabname(new_key, table_name_buf);
  const size_t table_name_len = strlen(table_name_buf);

  // Calculate total size needed for the variable length strings
  const size_t size = sizeof(NDB_SHARE_KEY) + new_key_length + db_name_len + 1 +
                      table_name_len + 1;

  NDB_SHARE_KEY *allocated_key = (NDB_SHARE_KEY *)my_malloc(
      PSI_INSTRUMENT_ME, size, MYF(MY_WME | ME_FATALERROR));

  allocated_key->m_key_length = new_key_length;

  // Copy key into the buffer
  char *buf_ptr = allocated_key->m_buffer;
  my_stpcpy(buf_ptr, new_key);
  buf_ptr += new_key_length + 1;

  // Copy db_name into the buffer
  my_stpcpy(buf_ptr, db_name_buf);
  buf_ptr += db_name_len + 1;

  // Copy table_name into the buffer
  my_stpcpy(buf_ptr, table_name_buf);
  buf_ptr += table_name_len;

  // Check that writing has not occurred beyond end of allocated memory
  assert(buf_ptr < reinterpret_cast<char *>(allocated_key) + size);

  DBUG_PRINT("info", ("size: %lu", (unsigned long)size));
  DBUG_PRINT("info",
             ("new_key: '%s', %lu", new_key, (unsigned long)new_key_length));
  DBUG_PRINT("info",
             ("db_name: '%s', %lu", db_name_buf, (unsigned long)db_name_len));
  DBUG_PRINT("info", ("table_name: '%s', %lu", table_name_buf,
                      (unsigned long)table_name_len));

  return allocated_key;
}

void NDB_SHARE::free_key(NDB_SHARE_KEY *key) { my_free(key); }

std::string NDB_SHARE::key_get_key(NDB_SHARE_KEY *key) {
  assert(key->m_key_length == strlen(key->m_buffer));
  return key->m_buffer;
}

char *NDB_SHARE::key_get_db_name(NDB_SHARE_KEY *key) {
  char *buf_ptr = key->m_buffer;
  // Step past the key string and it's zero terminator
  buf_ptr += key->m_key_length + 1;
  return buf_ptr;
}

char *NDB_SHARE::key_get_table_name(NDB_SHARE_KEY *key) {
  char *buf_ptr = key_get_db_name(key);
  const size_t db_name_len = strlen(buf_ptr);
  // Step past the db name string and it's zero terminator
  buf_ptr += db_name_len + 1;
  return buf_ptr;
}

size_t NDB_SHARE::key_length() const {
  assert(key->m_key_length == strlen(key->m_buffer));
  return key->m_key_length;
}

const char *NDB_SHARE::key_string() const {
  assert(strlen(key->m_buffer) == key->m_key_length);
  return key->m_buffer;
}

const char *NDB_SHARE::share_state_string() const {
  switch (state) {
    case NSS_INITIAL:
      return "NSS_INITIAL";
    case NSS_DROPPED:
      return "NSS_DROPPED";
  }
  assert(false);
  return "<unknown>";
}

void NDB_SHARE::free_share(NDB_SHARE **share) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&shares_mutex);

  if (!(*share)->decrement_use_count()) {
    // No one is using the NDB_SHARE anymore, release it
    NDB_SHARE::real_free_share(share);
  }
}

NDB_SHARE *NDB_SHARE::acquire_or_create_reference(const char *db_name,
                                                  const char *table_name,
                                                  const char *reference) {
  DBUG_TRACE;

  char key[FN_REFLEN + 1];
  format_key(key, sizeof(key) - 1, db_name, table_name);

  DBUG_PRINT("enter", ("key: '%s'", key));

  mysql_mutex_lock(&shares_mutex);

  NDB_SHARE *share = acquire_reference_impl(key);
  if (share) {
    share->refs_insert(reference);
    DBUG_PRINT("NDB_SHARE",
               ("'%s', reference: '%s', use_count: %u", share->key_string(),
                reference, share->use_count()));
    mysql_mutex_unlock(&shares_mutex);
    return share;
  }

  // No NDB_SHARE existed, create one
  share = NDB_SHARE::create(key);
  if (share == nullptr) {
    DBUG_PRINT("error", ("failed to create NDB_SHARE"));
    mysql_mutex_unlock(&shares_mutex);
    return nullptr;
  }

  // Insert the new share in list of open shares
  ndb_shares->emplace(key, share);

  // Add share refcount from 'ndb_shares', the list owns a reference
  share->increment_use_count();
  share->refs_insert("ndb_shares");

  // Add refcount for returned 'share'.
  share->increment_use_count();
  share->refs_insert(reference);

  mysql_mutex_unlock(&shares_mutex);

  return share;
}

NDB_SHARE *NDB_SHARE::create_for_handler(const char *db_name,
                                         const char *table_name,
                                         const ha_ndbcluster *reference) {
  char key[FN_REFLEN + 1];
  format_key(key, sizeof(key) - 1, db_name, table_name);

  mysql_mutex_lock(&shares_mutex);

  NDB_SHARE *share = NDB_SHARE::create(key);
  if (share == nullptr)
    ndb_log_error("failed to create NDB_SHARE for key: %s", key);
  else {
    // Insert the new share in list of open shares
    ndb_shares->emplace(key, share);

    // Add share refcount from 'ndb_shares', the list owns a reference
    share->increment_use_count();
    share->refs_insert("ndb_shares");

    // Add refcount for returned 'share'.
    share->increment_use_count();
    share->refs_insert(reference);
  }

  mysql_mutex_unlock(&shares_mutex);
  return share;
}

NDB_SHARE *NDB_SHARE::acquire_for_handler(const char *db_name,
                                          const char *table_name,
                                          const ha_ndbcluster *reference) {
  DBUG_TRACE;

  char key[FN_REFLEN + 1];
  format_key(key, sizeof(key) - 1, db_name, table_name);

  mysql_mutex_lock(&shares_mutex);
  NDB_SHARE *share = acquire_reference_impl(key);
  if (share) {
    share->refs_insert(reference);
    DBUG_PRINT("NDB_SHARE",
               ("'%s' reference: 'ha_ndbcluster(%p)', "
                "use_count: %u",
                share->key_string(), reference, share->use_count()));
  }
  mysql_mutex_unlock(&shares_mutex);

  return share;
}

void NDB_SHARE::release_for_handler(NDB_SHARE *share,
                                    const ha_ndbcluster *reference) {
  DBUG_TRACE;

  mysql_mutex_lock(&shares_mutex);

  DBUG_PRINT("NDB_SHARE", ("release '%s', reference: 'ha_ndbcluster(%p)', "
                           "use_count: %u",
                           share->key_string(), reference, share->use_count()));

  share->refs_erase(reference);
  NDB_SHARE::free_share(&share);
  mysql_mutex_unlock(&shares_mutex);
}

/*
  Acquire another reference using existing share reference.
*/

NDB_SHARE *NDB_SHARE::acquire_reference_on_existing(NDB_SHARE *share,
                                                    const char *reference) {
  mysql_mutex_lock(&shares_mutex);

  // Should already be referenced
  assert(share->use_count() > 0);
  // Number of references should match use_count
  assert(share->use_count() == share->refs->size());

  share->increment_use_count();
  share->refs_insert(reference);

  DBUG_PRINT("NDB_SHARE", ("'%s', reference: '%s', use_count: %u",
                           share->key_string(), reference, share->use_count()));

  mysql_mutex_unlock(&shares_mutex);
  return share;
}

/*
  Acquire reference using db and table name
*/
NDB_SHARE *NDB_SHARE::acquire_reference(const char *db_name,
                                        const char *table_name,
                                        const char *reference) {
  char key[FN_REFLEN + 1];
  format_key(key, sizeof(key) - 1, db_name, table_name);

  mysql_mutex_lock(&shares_mutex);

  NDB_SHARE *share = acquire_reference_impl(key);
  if (share) {
    share->refs_insert(reference);
    DBUG_PRINT("NDB_SHARE",
               ("'%s', reference: '%s', use_count: %u", share->key_string(),
                reference, share->use_count()));
  }

  mysql_mutex_unlock(&shares_mutex);
  return share;
}

void NDB_SHARE::release_reference(NDB_SHARE *share, const char *reference) {
  mysql_mutex_lock(&shares_mutex);

  DBUG_PRINT("NDB_SHARE", ("release '%s', reference: '%s', use_count: %u",
                           share->key_string(), reference, share->use_count()));

  share->refs_erase(reference);
  NDB_SHARE::free_share(&share);

  mysql_mutex_unlock(&shares_mutex);
}

#ifndef NDEBUG

bool NDB_SHARE::Ndb_share_references::check_empty() const {
  if (size() == 0) {
    // There are no references, all OK
    return true;
  }

  ndb_log_error(
      "Consistency check of NDB_SHARE references failed, the list "
      "of references should be empty at this time");

  std::string s;
  debug_print(s);
  ndb_log_error("%s", s.c_str());
  abort();
  return false;
}

void NDB_SHARE::Ndb_share_references::debug_print(
    std::string &out, const char *line_separator) const {
  std::stringstream ss;

  // Print the handler list
  {
    const char *separator = "";
    ss << "  handlers: " << handlers.size() << " [ ";
    for (const auto &key : handlers) {
      ss << separator << "'" << key << "'";
      separator = ",";
    }
    ss << " ]";
  }
  ss << ", " << line_separator;

  // Print the strings list
  {
    const char *separator = "";
    ss << "  strings: " << strings.size() << " [ ";
    for (const auto &key : strings) {
      ss << separator << "'" << key.c_str() << "'";
      separator = ",";
    }
    ss << " ]";
  }
  ss << ", " << line_separator;

  out = ss.str();
}

#endif

void NDB_SHARE::debug_print(std::string &out,
                            const char *line_separator) const {
  std::stringstream ss;
  ss << "NDB_SHARE { " << line_separator << "  db: '" << db << "',"
     << line_separator << "  table_name: '" << table_name << "', "
     << line_separator << "  key: '" << key_string() << "', " << line_separator
     << "  use_count: " << use_count() << ", " << line_separator
     << "  state: " << share_state_string() << ", " << line_separator
     << "  op: " << op << ", " << line_separator;

#ifndef NDEBUG
  std::string refs_string;
  refs->debug_print(refs_string, line_separator);
  ss << refs_string.c_str();

  // There should be as many refs as the use_count says
  assert(use_count() == refs->size());
#endif

  ss << "}";

  out = ss.str();
}

void NDB_SHARE::debug_print_shares(std::string &out) {
  std::stringstream ss;
  ss << "ndb_shares {"
     << "\n";

  for (const auto &key_and_value : *ndb_shares) {
    const NDB_SHARE *share = key_and_value.second;
    std::string s;
    share->debug_print(s, "\n");
    ss << s << "\n";
  }

  ss << "}"
     << "\n";

  out = ss.str();
}

uint NDB_SHARE::decrement_use_count() {
  ndbcluster::ndbrequire(m_use_count > 0);
  return --m_use_count;
}

void NDB_SHARE::print_remaining_open_shares() {
  mysql_mutex_lock(&shares_mutex);
  if (!ndb_shares->empty()) {
    std::string s;
    NDB_SHARE::debug_print_shares(s);
    std::cerr << s << std::endl;
  }
  mysql_mutex_unlock(&shares_mutex);
}

int NDB_SHARE::rename_share(NDB_SHARE *share, NDB_SHARE_KEY *new_key) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("share->key: '%s'", share->key_string()));
  DBUG_PRINT("enter",
             ("new_key: '%s'", NDB_SHARE::key_get_key(new_key).c_str()));

  mysql_mutex_lock(&shares_mutex);

  // Make sure that no NDB_SHARE with new_key already exists
  if (find_or_nullptr(*ndb_shares, NDB_SHARE::key_get_key(new_key))) {
    // Dump the list of open NDB_SHARE's since new_key already exists
    ndb_log_error(
        "INTERNAL ERROR: Found existing NDB_SHARE for "
        "new key: '%s' while renaming: '%s'",
        NDB_SHARE::key_get_key(new_key).c_str(), share->key_string());
    std::string s;
    NDB_SHARE::debug_print_shares(s);
    std::cerr << s << std::endl;
    abort();
  }

  /* Update the share hash key. */
  NDB_SHARE_KEY *old_key = share->key;
  share->key = new_key;
  ndb_shares->erase(NDB_SHARE::key_get_key(old_key));
  ndb_shares->emplace(NDB_SHARE::key_get_key(new_key), share);

  // Make sure that NDB_SHARE with old key does not exist
  assert(find_or_nullptr(*ndb_shares, NDB_SHARE::key_get_key(old_key)) ==
         nullptr);
  // Make sure that NDB_SHARE with new key does exist
  assert(find_or_nullptr(*ndb_shares, NDB_SHARE::key_get_key(new_key)));

  DBUG_PRINT("info", ("setting db and table_name to point at new key"));
  share->db = NDB_SHARE::key_get_db_name(share->key);
  share->table_name = NDB_SHARE::key_get_table_name(share->key);

  if (share->op) {
    Ndb_event_data *event_data =
        Ndb_event_data::get_event_data(share->op->getCustomData());
    if (event_data && event_data->shadow_table) {
      if (!ndb_name_is_temp(share->table_name)) {
        DBUG_PRINT("info", ("Renaming shadow table"));
        // Allocate new strings for db and table_name for shadow_table
        // in event_data's MEM_ROOT(where the shadow_table itself is allocated)
        // NOTE! This causes a slight memory leak since the already existing
        // strings are not release until the mem_root is eventually
        // released.
        lex_string_strmake(&event_data->mem_root,
                           &event_data->shadow_table->s->db, share->db,
                           strlen(share->db));
        lex_string_strmake(&event_data->mem_root,
                           &event_data->shadow_table->s->table_name,
                           share->table_name, strlen(share->table_name));
      } else {
        DBUG_PRINT("info", ("Name is temporary, skip rename of shadow table"));
        // don't rename the shadow table here, it's used by injector and all
        // events might not have been processed. It will be dropped anyway
      }
    }
  }
  mysql_mutex_unlock(&shares_mutex);
  return 0;
}

/**
  Acquire NDB_SHARE for key

  Returns share for key, and increases the refcount on the share.

  @param key      The key for NDB_SHARE to acquire
*/

NDB_SHARE *NDB_SHARE::acquire_reference_impl(const char *key) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("key: '%s'", key));

  if (DBUG_EVALUATE_IF("ndb_share_acquire_fail1", true, false)) {
    // Simulate failure to acquire NDB_SHARE
    return nullptr;
  }

  mysql_mutex_assert_owner(&shares_mutex);

  auto it = ndb_shares->find(key);
  if (it == ndb_shares->end()) {
    DBUG_PRINT("error", ("%s does not exist", key));
    return nullptr;
  }

  NDB_SHARE *share = it->second;

  // Add refcount for returned 'share'.
  share->increment_use_count();

  return share;
}

void NDB_SHARE::initialize(CHARSET_INFO *charset) {
  mysql_mutex_init(PSI_INSTRUMENT_ME, &shares_mutex, MY_MUTEX_INIT_FAST);
  ndb_shares.reset(new collation_unordered_map<std::string, NDB_SHARE *>(
      charset, PSI_INSTRUMENT_ME));
}

void NDB_SHARE::deinitialize() {
  mysql_mutex_lock(&shares_mutex);

  // There should not be any NDB_SHARE's left -> crash after logging in debug
  const class Debug_require {
    const bool m_required_val;

   public:
    Debug_require(bool val) : m_required_val(val) {}
    ~Debug_require() { assert(m_required_val); }
  } shares_remaining(ndb_shares->empty() && dropped_shares.empty());

  // Drop remaining open shares, drop one NDB_SHARE after the other
  // until open tables list is empty
  while (!ndb_shares->empty()) {
    NDB_SHARE *share = ndb_shares->begin()->second;
    ndb_log_error("Still open NDB_SHARE '%s', use_count: %d, state: %s",
                  share->key_string(), share->use_count(),
                  share->share_state_string());
    // If last ref, share is destroyed immediately, else moved to list of
    // dropped shares
    NDB_SHARE::mark_share_dropped_impl(&share);
  }

  // Release remaining dropped shares, release one NDB_SHARE after the other
  // until dropped list is empty
  while (!dropped_shares.empty()) {
    NDB_SHARE *share = *dropped_shares.begin();
    ndb_log_error("Not freed NDB_SHARE '%s', use_count: %d, state: %s",
                  share->key_string(), share->use_count(),
                  share->share_state_string());
    NDB_SHARE::real_free_share(&share);
  }

  mysql_mutex_unlock(&shares_mutex);

  mysql_mutex_destroy(&shares_mutex);
}

void NDB_SHARE::release_extra_share_references() {
  mysql_mutex_lock(&shares_mutex);
  while (!ndb_shares->empty()) {
    NDB_SHARE *share = ndb_shares->begin()->second;
    /*
      The share kept by the server has not been freed, free it
      Will also take it out of _open_tables list
    */
    assert(share->use_count() > 0);
    assert(share->state != NSS_DROPPED);
    NDB_SHARE::mark_share_dropped_impl(&share);
  }
  mysql_mutex_unlock(&shares_mutex);
}

void NDB_SHARE::real_free_share(NDB_SHARE **share_ptr) {
  NDB_SHARE *share = *share_ptr;
  DBUG_TRACE;
  mysql_mutex_assert_owner(&shares_mutex);

  // Share must already be marked as dropped
  ndbcluster::ndbrequire(share->state == NSS_DROPPED);

  // Share must be in dropped list
  ndbcluster::ndbrequire(dropped_shares.find(share) != dropped_shares.end());

  // Remove share from dropped list
  ndbcluster::ndbrequire(dropped_shares.erase(share));

  // Remove shares reference from 'dropped_shares'
  share->refs_erase("dropped_shares");

  NDB_SHARE::destroy(share);
}

extern void ndb_index_stat_free(NDB_SHARE *);

void NDB_SHARE::mark_share_dropped_impl(NDB_SHARE **share_ptr) {
  NDB_SHARE *share = *share_ptr;
  DBUG_TRACE;
  mysql_mutex_assert_owner(&shares_mutex);

  if (share->state == NSS_DROPPED) {
    // The NDB_SHARE was already marked as dropped
    return;
  }

  // The NDB_SHARE should not have any event operation, it
  // should have been removed already _before_ marking the NDB_SHARE
  // as dropped.
  assert(share->have_event_operation() == false);

  // The index_stat is not needed anymore, free it.
  ndb_index_stat_free(share);

  // Mark share as dropped
  share->state = NSS_DROPPED;

  // Remove share from list of open shares
  ndbcluster::ndbrequire(ndb_shares->erase(share->key_string()));

  // Remove reference from list of open shares and decrement use count
  share->refs_erase("ndb_shares");
  share->decrement_use_count();

  // Destroy the NDB_SHARE if no one is using it, this is normally a special
  // case for shutdown code path. In all other cases the caller will hold
  // reference to the share.
  if (share->use_count() == 0) {
    NDB_SHARE::destroy(share);
    return;
  }

  // Someone is still using the NDB_SHARE, insert it into the list of dropped
  // to keep track of it until all references has been released
  dropped_shares.emplace(share);

#ifndef NDEBUG
  std::string s;
  share->debug_print(s, "\n");
  std::cerr << "dropped_share: " << s << std::endl;
#endif

  // Share is referenced by 'dropped_shares'
  share->refs_insert("dropped_shares");
  // NOTE! The refcount has not been incremented
}

void NDB_SHARE::mark_share_dropped_and_release(NDB_SHARE *share,
                                               const char *reference) {
  mysql_mutex_lock(&shares_mutex);
  mark_share_dropped_impl(&share);

  DBUG_PRINT("NDB_SHARE", ("release '%s', reference: '%s', use_count: %u",
                           share->key_string(), reference, share->use_count()));

  share->refs_erase(reference);
  NDB_SHARE::free_share(&share);

  // If this was the last share ref, it is now deleted. If there are more
  // references, the share will remain in the list of dropped until
  // remaining references are released.

  mysql_mutex_unlock(&shares_mutex);
}

#ifndef NDEBUG
void NDB_SHARE::dbg_check_shares_update() {
  ndb_log_info("dbug_check_shares open:");
  for (const auto &key_and_value : *ndb_shares) {
    const NDB_SHARE *share = key_and_value.second;
    ndb_log_info("  %s.%s: state: %s(%u) use_count: %u", share->db,
                 share->table_name, share->share_state_string(),
                 (unsigned)share->state, share->use_count());
    assert(share->state != NSS_DROPPED);
  }

  ndb_log_info("dbug_check_shares dropped:");
  for (const NDB_SHARE *share : dropped_shares) {
    ndb_log_info("  %s.%s: state: %s(%u) use_count: %u", share->db,
                 share->table_name, share->share_state_string(),
                 (unsigned)share->state, share->use_count());
    assert(share->state == NSS_DROPPED);
  }

  /**
   * Only shares in mysql database may be open...
   */
  for (const auto &key_and_value : *ndb_shares) {
    const NDB_SHARE *share = key_and_value.second;
    assert(strcmp(share->db, "mysql") == 0);
  }

  /**
   * Only shares in mysql database may be in dropped list...
   */
  for (const NDB_SHARE *share : dropped_shares) {
    assert(strcmp(share->db, "mysql") == 0);
  }
}
#endif

bool NDB_SHARE::install_event_op(NdbEventOperation *new_op, bool replace_op) {
  // Require mutex to be held
  mysql_mutex_assert_owner(&mutex);

  // Only allow installing op with properly setup custom data
  ndbcluster::ndbrequire(
      Ndb_event_data::check_custom_data(new_op->getCustomData(), this));

  // Only allow replacing op unless "replace_op" (which is used by inplace alter
  // who will replace the old op with new in an atomic fashion)
  ndbcluster::ndbrequire(op == nullptr || (op && replace_op));

  // Don't allow installing op in other states than NSS_INITIAL. This will for
  // example detect when a user thread tries to install event op on a share
  // which has been "dropped" (by the binlog thread) during CREATE TABLE.
  if (state != NSS_INITIAL) {
    return false;
  }

  op = new_op;
  return true;
}
