/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_stored_grants.h"

#include <algorithm>  // std::find()
#include <mutex>
#include <unordered_set>

#include "m_string.h"
#include "mysql/components/my_service.h"
#include "mysql/components/services/dynamic_privilege.h"
#include "sql/auth/acl_change_notification.h"
#include "sql/mem_root_array.h"
#include "sql/sql_lex.h"
#include "sql/statement/ed_connection.h"
#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_mysql_services.h"
#include "storage/ndb/plugin/ndb_retry.h"
#include "storage/ndb/plugin/ndb_rpl_filter.h"
#include "storage/ndb/plugin/ndb_sql_metadata_table.h"
#include "storage/ndb/plugin/ndb_thd.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"
#include "string_with_len.h"

using ChangeNotice = const Acl_change_notification;

//
// Private implementation
//
namespace {

/* Static file-scope data */
static Ndb_sql_metadata_api metadata_table;
static std::unordered_set<std::string> local_granted_users;
static std::mutex local_granted_users_mutex;

/* Utility functions */

bool op_grants_or_revokes_ndb_storage(ChangeNotice *notice) {
  for (const ChangeNotice::Priv priv : notice->get_dynamic_privilege_list())
    if (!native_strncasecmp(priv.c_str(), "NDB_STORED_USER", priv.length()))
      return true;
  return false;
}

/* Internal interfaces */
class Buffer {
 public:
  static int getType(const char *);
  static const NdbOperation *writeTuple(const char *, NdbTransaction *);
  static const NdbOperation *deleteTuple(const char *, NdbTransaction *);
  static const NdbOperation *readTupleExclusive(const char *, char *,
                                                NdbTransaction *);
};

/* ThreadContext is stack-allocated and lives for the duration of one call
   into the public API. It will hold the local_granted_users_mutex for its
   whole life cycle. Its mem_root arena allocator will release memory when
   the ThreadContext goes out of scope.
*/
class ThreadContext : public Ndb_local_connection {
 public:
  ThreadContext(THD *);
  ~ThreadContext();

  void apply_current_snapshot();
  void write_status_message_to_server_log();
  int build_cache_of_ndb_users();
  Ndb_stored_grants::Strategy handle_change(ChangeNotice *);
  void deserialize_users(std::string &);
  bool cache_was_rebuilt() const { return m_rebuilt_cache; }
  void serialize_snapshot_user_list(std::string *out_str);
  void consider_all_local_users_for_drop();
  void handle_dropped_users();

  /* NDB Transactions */
  bool read_snapshot();
  const NdbError *read_snapshot(NdbTransaction *);
  bool write_snapshot();
  const NdbError *write_snapshot(NdbTransaction *);

 private:
  /* Methods */
  int get_user_lists_for_statement(ChangeNotice *);
  int handle_rename_user();
  int update_users(const Mem_root_array<std::string> &);
  int drop_users(ChangeNotice *, const Mem_root_array<std::string> &);
  void update_user(std::string user);
  void drop_user(std::string user, bool revoke = false);

  bool get_local_user(const std::string &) const;
  int get_grants_for_user(std::string);
  bool show_create_user(std::string, std::string &, bool use_hex = true);
  void get_create_user(std::string, int);
  void create_user(std::string &, std::string &);

  char *getBuffer(size_t);
  char *Row(uint type, std::string name, uint seq, unsigned int *note,
            std::string sql);
  char *Row(std::string sql);
  char *Key(uint type, std::string name, uint seq);
  char *Key(const char *);

  /* ThreadContext "is a" Ndb_local_connection (by inheritance).
     It owns a single Execute Direct connection, which is able to run a
     SQL statement. Running a second statement will cause all results from
     the first to become invalid. To help the programmer get this right,
     you are required to explicitly close the connection each time you are
     finished using a set of results.
  */
  bool exec_sql(const std::string &statement);
  void close() { m_closed = true; }

  /* Data */
  MEM_ROOT mem_root;
  Thd_ndb *m_thd_ndb;
  bool m_closed;
  bool m_rebuilt_cache;
  size_t m_applied_users;
  size_t m_applied_grants;

  Mem_root_array<char *> m_read_keys;
  Mem_root_array<unsigned short> m_grant_count;
  Mem_root_array<char *> m_current_rows;
  Mem_root_array<char *> m_delete_users;
  Mem_root_array<std::string> m_statement_users;
  Mem_root_array<std::string> m_intersection;
  Mem_root_array<std::string> m_extra_grants;
  Mem_root_array<std::string> m_users_in_snapshot;

  /* Constants */
  /* 3250 is just over 1/4 of the full row buffer size */
  static constexpr size_t MallocBlockSize = 3250;

  static constexpr int SCAN_RESULT_READY = 0;
  static constexpr int SCAN_FINISHED = 1;
  static constexpr int SCAN_CACHE_EMPTY = 2;

  /* Stored in the type column in ndb_sql_metadata */
  static constexpr uint TYPE_USER = Ndb_sql_metadata_api::TYPE_USER;
  static constexpr uint TYPE_GRANT = Ndb_sql_metadata_api::TYPE_GRANT;
  static_assert(TYPE_USER < TYPE_GRANT, "(for ordered index scan)");
};

/* Buffer Private Implementation */

int Buffer::getType(const char *data) {
  unsigned short val = 0;
  metadata_table.getType(data, &val);
  return val;
}

const NdbOperation *Buffer::writeTuple(const char *data, NdbTransaction *tx) {
  return tx->writeTuple(metadata_table.keyNdbRecord(), data,
                        metadata_table.rowNdbRecord(), data);
}

const NdbOperation *Buffer::deleteTuple(const char *data, NdbTransaction *tx) {
  return tx->deleteTuple(metadata_table.keyNdbRecord(), data,
                         metadata_table.keyNdbRecord());
}

const NdbOperation *Buffer::readTupleExclusive(const char *key, char *result,
                                               NdbTransaction *tx) {
  return tx->readTuple(metadata_table.keyNdbRecord(), key,
                       metadata_table.noteNdbRecord(), result,
                       NdbOperation::LM_Exclusive);
}

/* ThreadContext */

ThreadContext::ThreadContext(THD *thd)
    : Ndb_local_connection(thd),
      mem_root(PSI_NOT_INSTRUMENTED, MallocBlockSize),
      m_thd_ndb(get_thd_ndb(thd)),
      m_closed(true),
      m_rebuilt_cache(false),
      m_applied_users(0),
      m_applied_grants(0),
      m_read_keys(&mem_root),
      m_grant_count(&mem_root),
      m_current_rows(&mem_root),
      m_delete_users(&mem_root),
      m_statement_users(&mem_root),
      m_intersection(&mem_root),
      m_extra_grants(&mem_root),
      m_users_in_snapshot(&mem_root) {
  local_granted_users_mutex.lock();
}

ThreadContext::~ThreadContext() { local_granted_users_mutex.unlock(); }

char *ThreadContext::getBuffer(size_t size) {
  return static_cast<char *>(mem_root.Alloc(size));
}

char *ThreadContext::Row(uint type, std::string name, uint seq, uint *note,
                         std::string sql) {
  char *buffer = getBuffer(metadata_table.getRowSize());

  metadata_table.setType(buffer, type);
  metadata_table.setName(buffer, name);
  metadata_table.setSeq(buffer, seq);
  metadata_table.setNote(buffer, note);
  metadata_table.setSql(buffer, sql);

  return buffer;
}

char *ThreadContext::Key(uint type, std::string name, uint seq) {
  char *buffer = getBuffer(metadata_table.getKeySize());

  metadata_table.setType(buffer, type);
  metadata_table.setName(buffer, name);
  metadata_table.setSeq(buffer, seq);

  return buffer;
}

char *ThreadContext::Key(const char *key) {
  char *buffer = getBuffer(metadata_table.getKeySize());
  memcpy(buffer, key, metadata_table.getKeySize());

  return buffer;
}

void ThreadContext::serialize_snapshot_user_list(std::string *out_str) {
  int i = 0;
  for (std::string s : m_users_in_snapshot) {
    if (i++) out_str->append(",");
    out_str->append(s);
  }
}

/* deserialize_users()
   Sets m_read_keys to a set of buffers that can be used in NdbScanFilter
*/
void ThreadContext::deserialize_users(std::string &str) {
  unsigned long nfound = 0;

  for (size_t pos = 0; pos < str.length();) {
    /* Find the 4th quote mark in 'user'@'host' */
    size_t end = pos;
    for (int i = 0; i < 3; i++) {
      end = str.find('\'', end + 1);
      if (end == std::string::npos) return;
    }
    size_t len = end + 1 - pos;
    std::string user = str.substr(pos, len);
    if (get_local_user(user)) nfound++;
    m_users_in_snapshot.push_back(user);
    {
      char *buf = getBuffer(len + 4);
      metadata_table.packName(buf, user);
      m_read_keys.push_back(buf);
    }
    pos = end + 2;
  }
  /* As an optimization, prefer a complete snapshot refresh to a partial
     refresh of n users if n is greater than half. */
  if (nfound > local_granted_users.size() / 2) {
    ndb_log_verbose(9, "deserialize_users() choosing complete refresh");
    m_read_keys.clear();
  }
}

/* returns false on success */
bool ThreadContext::exec_sql(const std::string &statement) {
  // Disable rpl_filter as otherwise the non-updating query fail in the applier
  Ndb_rpl_filter_disable disable_filter(m_thd);

  assert(m_closed);
  /* execute_query_iso() returns false on success */
  if (execute_query_iso(statement, nullptr)) {
    // Query failed
    return true;
  }

  if (get_results() == nullptr) {
    // Query reported sucess but no result set, this indicates failure
    ndb_log_error("No result set for query '%s'", statement.c_str());
    assert(false);
  }

  m_closed = false;
  return m_closed;
}

/* Run SHOW CREATE USER, and place the result SQL in result.
   Return true on success.
*/
bool ThreadContext::show_create_user(std::string user, std::string &result,
                                     bool use_hex) {
  std::string statement("SHOW CREATE USER " + user);
  int exec_return_val;

  {
    bool saved_val = m_thd->variables.print_identified_with_as_hex;
    m_thd->variables.print_identified_with_as_hex = use_hex;
    exec_return_val = exec_sql(statement);
    m_thd->variables.print_identified_with_as_hex = saved_val;
  }

  if (exec_return_val) {
    ndb_log_error("Failed SHOW CREATE USER for %s", user.c_str());
    return false;
  }

  List<Ed_row> results = *get_results();

  if (results.elements != 1) {
    ndb_log_error("%s returned %d rows", statement.c_str(), results.elements);
    close();
    return false;
  }

  Ed_row *result_row = results[0];
  const MYSQL_LEX_STRING *result_sql = result_row->get_column(0);
  result.assign(result_sql->str, result_sql->length);
  close();
  return true;
}

/* Run SHOW CREATE USER, create a row, and push it to m_current_rows
 */
void ThreadContext::get_create_user(std::string user, int ngrants) {
  std::string show_create;
  if (show_create_user(user, show_create)) {
    unsigned int note = ngrants;
    char *row = Row(TYPE_USER, user, 0, &note, show_create);
    m_current_rows.push_back(row);
  }
  return;
}

int ThreadContext::get_grants_for_user(std::string user) {
  if (exec_sql("SHOW GRANTS FOR " + user)) return 0;

  List<Ed_row> results = *get_results();
  uint n = results.elements;
  ndb_log_verbose(9, "SHOW GRANTS FOR %s returned %d rows", user.c_str(), n);

  for (uint seq = 0; seq < n; seq++) {
    Ed_row *result_row = results[seq];
    const MYSQL_LEX_STRING *result_sql = result_row->get_column(0);
    char *row = Row(TYPE_GRANT, user, seq, nullptr,
                    std::string(result_sql->str, result_sql->length));
    m_current_rows.push_back(row);
  }
  close();
  return n;
}

bool log_message_on_error(bool retry_result, const NdbError &ndb_err) {
  if (ndb_err.code) {
    if (retry_result)
      ndb_log_info("Error %d, %s [Transaction succeeded on retry]",
                   ndb_err.code, ndb_err.message);
    else
      ndb_log_error("Error %d, %s [Transaction failed]", ndb_err.code,
                    ndb_err.message);
  }
  return retry_result;
}

/*
 * Read snapshot stored in NDB
 */
const NdbError *scan_snapshot(NdbTransaction *tx, ThreadContext *context) {
  return context->read_snapshot(tx);
}

bool ThreadContext::read_snapshot() {
  NdbError ndb_err;
  bool r;
  r = ndb_trans_retry(m_thd_ndb->ndb, m_thd, ndb_err,
                      std::function<decltype(scan_snapshot)>(scan_snapshot),
                      this);
  return log_message_on_error(r, ndb_err);
}

/* read_snapshot()
   On success, m_current_rows will be populated with rows read.
*/
const NdbError *ThreadContext::read_snapshot(NdbTransaction *tx) {
  using ScanOptions = NdbScanOperation::ScanOptions;
  ScanOptions scan_options;
  scan_options.optionsPresent = ScanOptions::SO_SCANFLAGS;
  scan_options.scan_flags = NdbScanOperation::ScanFlag::SF_OrderBy;

  /* Partial scans.
     This is just a performance optimization. It should be possible to disable
     this block of code and still pass the test suite.
  */
  NdbInterpretedCode filter_code(*metadata_table.rowNdbRecord());
  if (m_read_keys.size()) {
    scan_options.optionsPresent |= ScanOptions::SO_INTERPRETED;
    scan_options.interpretedCode = &filter_code;
    NdbScanFilter filter(&filter_code);
    filter.begin(NdbScanFilter::OR);
    for (char *user : m_read_keys) {
      filter.cmp(NdbScanFilter::COND_EQ, 1, user);
    }
    filter.end();
  }

  NdbIndexScanOperation *scan =
      tx->scanIndex(metadata_table.orderedNdbRecord(),  // scan key
                    metadata_table.rowNdbRecord(),      // row record
                    NdbOperation::LM_Read,              // lock mode
                    nullptr,                            // result mask
                    nullptr,                            // index bounds
                    &scan_options, 0);
  if (!scan) return &tx->getNdbError();

  char *lo_key = Key(TYPE_USER, "", 0);
  char *hi_key = Key(TYPE_GRANT, "", 0);
  scan->setBound(metadata_table.orderedNdbRecord(),
                 {
                     lo_key, 1, true,  // From (1 key part, inclusive)
                     hi_key, 1, true,  // To (1 key part, inclusive)
                     0                 // range number
                 });

  if (tx->execute(NoCommit) != 0) return &tx->getNdbError();

  static constexpr bool force = true;
  const NdbError *error = nullptr;
  bool done = false;
  bool fetch = false;

  m_current_rows.clear();

  while (!done) {
    char *row = getBuffer(metadata_table.getRowSize());
    int result = scan->nextResultCopyOut(row, fetch, force);
    switch (result) {
      case SCAN_RESULT_READY:
        m_current_rows.push_back(row);
        fetch = false;
        break;
      case SCAN_FINISHED:
        done = true;
        scan->close();
        break;
      case SCAN_CACHE_EMPTY:
        fetch = true;
        break;
      default:
        error = &scan->getNdbError();
        scan->close();
        done = true;
    }
  }

  ndb_log_verbose(9, "Ndb_stored_grants::snapshot_fetch, read %zu rows",
                  m_current_rows.size());
  return error;
}

bool ThreadContext::get_local_user(const std::string &name) const {
  auto it = local_granted_users.find(name);
  return (it != local_granted_users.end());
}

inline bool blacklisted(std::string user) {
  /* Reserved User Accounts should not be stored */
  return !(user.compare("'mysql.sys'@'localhost'") &&
           user.compare("'mysql.infoschema'@'localhost'") &&
           user.compare("'mysql.session'@'localhost'"));
}

/* build_cache_of_ndb_users()
   This query selects only those users that have been directly granted
   NDB_STORED_USER, not those granted it transitively via a role. This
   entails that the direct grant is required -- a limitation which must
   be documented. If there were a table in information_schema analogous to
   mysql.role_edges, we could solve this problem by issuing a JOIN query
   of user_privileges and role_edges. For now, though, living with the
   documented limitation is preferable to relying on the mysql table.
*/
int ThreadContext::build_cache_of_ndb_users() {
  int n = 0;
  local_granted_users.clear();
  if (!exec_sql("SELECT grantee FROM information_schema.user_privileges "
                "WHERE privilege_type='NDB_STORED_USER'")) {
    List<Ed_row> results = *get_results();
    n = results.elements;
    for (Ed_row result : results) {
      const MYSQL_LEX_STRING *result_user = result.get_column(0);
      std::string user(result_user->str, result_user->length);
      if (!blacklisted(user)) local_granted_users.insert(user);
    }
    close();
  }
  m_rebuilt_cache = true;
  return n;
}

/* The quoting and formatting here must be identical to that in
   the information_schema.user_privileges table.
   See fill_schema_user_privileges() in auth/sql_authorization.cc
*/
static void format_user(std::string &str, const ChangeNotice::User user) {
  str.append("'").append(user.name).append("'@'").append(user.host).append("'");
}

const NdbError *store_snapshot(NdbTransaction *tx, ThreadContext *ctx) {
  return ctx->write_snapshot(tx);
}

/* write_snapshot()

   m_read_keys holds a list of USER records to read.
   m_grant_count holds the number of grants that will be stored for each user.
   m_read_keys and m_grant_count are in one-to-one correspondence.
   Any extraneous old grants for a user above m_grant_count will be deleted.

   m_current_rows holds a set of USER and GRANT records to be written.
   m_delete_users holds a set of USER records to be deleted.

   After execute(), m_current_rows, m_read_keys, m_grant_count, and
   m_delete_users are all cleared.
*/
const NdbError *ThreadContext::write_snapshot(NdbTransaction *tx) {
  Mem_root_array<char *> read_results(&mem_root);
  Mem_root_array<const NdbOperation *> read_ops(&mem_root);

  /* When updating users, it may be necessary to delete some extra grants */
  if (m_read_keys.size()) {
    for (char *row : m_read_keys) {
      char *result = getBuffer(metadata_table.getNoteSize());
      read_results.push_back(result);
      const NdbOperation *op = Buffer::readTupleExclusive(row, result, tx);
      read_ops.push_back(op);
    }

    if (tx->execute(NoCommit)) return &tx->getNdbError();

    assert(m_read_keys.size() == m_grant_count.size());
    for (size_t i = 0; i < m_read_keys.size(); i++) {
      unsigned int n_stored_grants;
      const NdbError &op_error = read_ops[i]->getNdbError();
      if (op_error.code != 0) {
        ndb_log_error("Error %d, %s [reading user record]", op_error.code,
                      op_error.message);
        continue;
      }
      if (!metadata_table.getNote(read_results[i], &n_stored_grants)) {
        ndb_log_error("Unexpected NULL in ndb_sql_metadata table");
        continue;
      }

      ndb_log_verbose(9, "Deleting extra grants -- old %d, new %d",
                      n_stored_grants, m_grant_count[i]);
      for (int n = n_stored_grants - 1; n >= m_grant_count[i]; n--) {
        char *key = Key(m_read_keys[i]);          // Make a copy of the key,
        metadata_table.setType(key, TYPE_GRANT);  // ... modify it,
        metadata_table.setSeq(key, n);
        Buffer::deleteTuple(key, tx);  // ... and use it to delete the GRANT.
      }
    }
  }

  /* Apply the updates stored in m_current_rows */
  for (char *row : m_current_rows) {
    Buffer::writeTuple(row, tx);
  }

  /* Delete user records for DROP USER */
  for (char *key : m_delete_users) {
    Buffer::deleteTuple(key, tx);
  }

  bool r = tx->execute(Commit);

  m_current_rows.clear();
  m_read_keys.clear();
  m_grant_count.clear();
  m_delete_users.clear();

  return r ? &tx->getNdbError() : nullptr;
}

bool ThreadContext::write_snapshot() {
  NdbError ndb_err;
  bool r;
  r = ndb_trans_retry(m_thd_ndb->ndb, m_thd, ndb_err,
                      std::function<decltype(store_snapshot)>(store_snapshot),
                      this);
  return log_message_on_error(r, ndb_err);
}

void ThreadContext::update_user(std::string user) {
  int ngrants = get_grants_for_user(user);
  if (ngrants) {
    get_create_user(user, ngrants);
    if (local_granted_users.count(user)) {
      m_read_keys.push_back(Key(TYPE_USER, user, 0));
      m_grant_count.push_back(ngrants);
    }
    m_users_in_snapshot.push_back(user);
  }
}

int ThreadContext::update_users(const Mem_root_array<std::string> &list) {
  for (std::string user : list) update_user(user);
  return list.size();
}

void ThreadContext::drop_user(std::string user, bool is_revoke) {
  unsigned int zero = 0;
  if (is_revoke) {
    std::string statement("REVOKE NDB_STORED_USER ON *.* FROM " + user);
    m_current_rows.push_back(Row(TYPE_USER, user, 0, &zero, statement));
  } else {
    m_delete_users.push_back(Key(TYPE_USER, user, 0));
  }
  m_read_keys.push_back(Key(TYPE_USER, user, 0));
  m_grant_count.push_back(0);
  m_users_in_snapshot.push_back(user);
}

int ThreadContext::drop_users(ChangeNotice *notice,
                              const Mem_root_array<std::string> &list) {
  for (std::string user : list) {
    assert(local_granted_users.count(user));
    drop_user(user, (notice->get_operation() != SQLCOM_DROP_USER));
  }
  return list.size();
}

/* Stored in the snapshot is a CREATE USER statement. This statement has come
   from SHOW CREATE USER, so its exact format is known.

   If the user already exists locally and the local SHOW CREATE USER exactly
   matches the snapshot, then return without doing anything, so that the
   last_mod timestamp on the user's password does not get unnecessarily reset.

   Otherwise, try to apply the statement as-is. This can fail for several
   reasons (some of which are tested in the apply_stored_grants test case),
   but if might succeed.

   If running the CREATE USER statement failed, it must be rewritten as
   several statements. The final result is:
      CREATE USER IF NOT EXISTS user@host  IDENTIFIED BY RANDOM PASSWORD;
      REVOKE ALL ON *.* FROM user@host;
      ALTER USER user@host ...;   [CLEAR RESOURCE LIMITS]
      ALTER USER user@host ...;   [SET VALUES FROM SHOW CREATE USER]
      ALTER USER user@host DEFAULT ROLE ... ;

  The DEFAULT ROLE statement is pushed to the end of a list and run later,
  after some other statement in the snapshot possibly creates the named role.
*/
void ThreadContext::create_user(std::string &name, std::string &statement) {
  const std::string create_user_if_ne("CREATE USER IF NOT EXISTS ");
  const std::string random_pass(" IDENTIFIED BY RANDOM PASSWORD");
  const std::string alter_user("ALTER USER ");
  const std::string revoke_all("REVOKE ALL ON *.* FROM ");
  const std::string set_resource_defaults(
      " WITH MAX_QUERIES_PER_HOUR 0 MAX_UPDATES_PER_HOUR 0 "
      " MAX_CONNECTIONS_PER_HOUR 0 MAX_USER_CONNECTIONS 0");

  const bool exists_in_cache = get_local_user(name);

  if (exists_in_cache) {
    /* Compare the snapshot user to the local one. Before comparing, we must
       detect whether the password hash stored in the snapshot used hex encoding
       or a plain string.  In statement.find(), 36 characters skips past
       "CREATE USER `a`@`%` IDENTIFIED WITH".
    */
    std::string show_create;
    bool is_hex = (statement.find(" AS 0x", 36) != std::string::npos);
    if (show_create_user(name, show_create, is_hex) && show_create == statement)
      return;  // Current SHOW CREATE USER already matches snapshot
  } else {
    local_granted_users.insert(name);
  }

  ndb_log_info("From stored snapshot, adding NDB stored user: %s",
               name.c_str());

  /* Try to run the CREATE USER statement stored in the snapshot.
     If this succeeds, we are done; but there are several reasons it might fail.
  */
  if (try_create_user(statement) == false) return;

  /* Create the user with a random password (unless the user already exists) */
  run_acl_statement(create_user_if_ne + name + random_pass);

  /* Revoke any privileges the user may have had prior to this snapshot. */
  run_acl_statement(revoke_all + name);

  /* Clear resource limits (this is not included in SHOW CREATE USER) */
  run_acl_statement(alter_user + name + set_resource_defaults);

  /* Rewrite CREATE to ALTER */
  statement = statement.replace(0, 6, "ALTER");

  /* Statement may have a DEFAULT ROLE clause */
  size_t default_role_pos = statement.find(" DEFAULT ROLE ");  // length 14
  if (default_role_pos == std::string::npos) {
    run_acl_statement(statement);
    return;
  }

  /* Locate the part between DEFAULT ROLE and REQUIRE */
  size_t require_pos = statement.find(" REQUIRE ", default_role_pos + 14);
  assert(require_pos != std::string::npos);
  size_t role_clause_len = require_pos - default_role_pos;

  /*  Set default role. The role has not yet been granted, so this statement
      is stored on a list, to be executed after the user's grants.
  */
  m_extra_grants.push_back(alter_user + name +
                           statement.substr(default_role_pos, role_clause_len));

  /* Run the rest of the statement. */
  run_acl_statement(statement.erase(default_role_pos, role_clause_len));
}

/* Apply the snapshot in m_current_rows,
   removing each applied user from m_users_in_snapshot.
 */
void ThreadContext::apply_current_snapshot() {
  for (const char *row : m_current_rows) {
    unsigned int note = 0;
    size_t str_length = 0;
    const char *str_start = nullptr;
    bool is_null;

    int type = Buffer::getType(row);
    metadata_table.getName(row, &str_length, &str_start);
    std::string name(str_start, str_length);
    metadata_table.getSql(row, &str_length, &str_start);
    std::string statement(str_start, str_length);

    switch (type) {
      case TYPE_USER:
        m_applied_users++;
        m_users_in_snapshot.erase_value(name);
        is_null = !metadata_table.getNote(row, &note);
        if (is_null) {
          ndb_log_error("Unexpected NULL in ndb_sql_metadata table");
        }
        if (note > 0) {
          create_user(name, statement);
        } else {
          /* REVOKE NDB_STORED_USER ON *.* FROM user, or 8.0.18 DROP USER */
          if (get_local_user(name)) {
            run_acl_statement(statement);
          }
        }
        break;
      case TYPE_GRANT:
        m_applied_grants++;
        run_acl_statement(statement);
        break;
      default:
        /* These records should have come from a bounded index scan */
        assert(false);
        break;
    }  // switch()
  }    // for()

  /* Extra DEFAULT ROLE statements added by create_user() */
  for (std::string grant : m_extra_grants) run_acl_statement(grant);
}

/* After apply_current_snapshot() has iteratively removed users from
   m_users_in_snapshot, any user remaining there must be dropped.
*/
void ThreadContext::handle_dropped_users() {
  const std::string drop("DROP USER IF EXISTS ");

  for (std::string user : m_users_in_snapshot) {
    ndb_log_info("Dropping user %s not present in stored snapshot",
                 user.c_str());
    run_acl_statement(drop + user);
  }
}

/* At server startup time, any local user with NDB_STORED_USER may have
   been dropped while the server was down, so m_users_in_snapshot is
   initialized with the whole list of local users.
*/
void ThreadContext::consider_all_local_users_for_drop() {
  for (std::string user : local_granted_users)
    m_users_in_snapshot.push_back(user);
}

void ThreadContext::write_status_message_to_server_log() {
  ndb_log_info("From NDB stored grants, applied %zu grant%s for %zu user%s.",
               m_applied_grants, (m_applied_grants == 1 ? "" : "s"),
               m_applied_users, (m_applied_users == 1 ? "" : "s"));
}

/* Fetch the list of users named in the SQL statement into m_statement_users.
   Compute the intersection of m_statement_users and local_granted_users,
   and store this in m_intersection.
   Return the number of elements in m_statement_users.
*/
int ThreadContext::get_user_lists_for_statement(ChangeNotice *notice) {
  assert(m_statement_users.size() == 0);
  assert(m_intersection.size() == 0);

  for (const ChangeNotice::User notice_user : notice->get_user_list()) {
    std::string user;
    format_user(user, notice_user);
    m_statement_users.push_back(user);
    if (local_granted_users.count(user)) m_intersection.push_back(user);
  }
  return m_statement_users.size();
}

/* The server has executed a RENAME USER statement. The server guarantees that
   the statement does not attempt to rename a role (see ER_RENAME_ROLE).
   Determine which stored users were affected, and must be dropped from or
   updated in the snapshot, then build a snapshot update in memory.
*/
int ThreadContext::handle_rename_user() {
  struct status {
    bool lhs;    // User first seen on left hand side of RENAME
    bool rhs;    // User first seen on right hand side of RENAME
    bool drop;   // User may be dropped as a result of RENAME
    bool known;  // User has NDB_STORED_USER
  };

  /* Initialize the map */
  std::unordered_map<std::string, struct status> user_map;
  for (std::string user : m_statement_users)
    user_map[user] = {0, 0, 0, (bool)local_granted_users.count(user)};

  /* Process the RENAME operations one pair at a time */
  for (unsigned int i = 0; i < m_statement_users.size(); i += 2) {
    struct status &from_status = user_map[m_statement_users[i]];
    from_status.drop = 1;
    if (!from_status.rhs) from_status.lhs = 1;

    struct status &to_status = user_map[m_statement_users[i + 1]];
    to_status.drop = 0;
    if (!to_status.lhs) to_status.rhs = 1;
    to_status.known = from_status.known;
  }

  /* Handle each user. Dropped users that originally appeared on RHS were just
     temporary placeholders; those that originally appeared on LHS should
     actually be dropped.
  */
  for (auto pair : user_map) {
    const std::string &user = pair.first;
    const struct status &status = pair.second;
    if (status.known) {
      if (status.drop) {
        if (status.lhs) drop_user(user);
      } else {
        update_user(user);
      }
    }
  }

  return m_users_in_snapshot.size();
}

/* Handle a local ACL change notification.
   Update the snapshot stored in NDB, and the local cache of stored users.
   Determine a strategy for distributing the change to schema dist participants.
*/
Ndb_stored_grants::Strategy ThreadContext::handle_change(ChangeNotice *notice) {
  const Mem_root_array<std::string> *drop_list = nullptr;
  const Mem_root_array<std::string> *update_list = nullptr;

  /* get_user_lists_for_statement() sets m_statement_users and m_intersection */
  int n_users_in_statement = get_user_lists_for_statement(notice);
  int n_changed_users = 0;
  bool rebuild_local_cache = true;
  bool dist_as_snapshot = false;

  const enum_sql_command operation = notice->get_operation();
  if (operation == SQLCOM_RENAME_USER) {
    n_changed_users = handle_rename_user();
  } else if (operation == SQLCOM_REVOKE_ALL ||
             op_grants_or_revokes_ndb_storage(notice)) {
    /* Handle GRANT or REVOKE that includes the NDB_STORED_USER privilege. */
    if (operation == SQLCOM_GRANT) {
      ndb_log_verbose(9, "This statement grants NDB_STORED_USER");
      update_list = &m_statement_users;
      dist_as_snapshot = true;
    } else {
      /* REVOKE ALL or REVOKE NDB_STORED_USER */
      drop_list = &m_intersection;
    }
  } else if (operation == SQLCOM_DROP_USER) {
    /* DROP user or role. DROP ROLE can have a cascading effect upon the grants
       of other users, so this requires a full snapshot update. */
    if (m_intersection.size()) {
      drop_list = &m_intersection;
      m_statement_users.clear();
      for (std::string user : local_granted_users)
        if (!std::find(drop_list->begin(), drop_list->end(), user))
          m_statement_users.push_back(user);
      update_list = &m_statement_users;
    }
  } else {
    /* ALTER USER, SET PASSWORD, or GRANT or REVOKE of misc. privileges */
    rebuild_local_cache = false;
    update_list = &m_intersection;
  }

  /* drop_users() will DROP USER or REVOKE NDB_STORED_USER, as appropriate */
  if (drop_list) {
    n_changed_users += drop_users(notice, *drop_list);
  }

  /* Update users in snapshot */
  if (update_list) {
    n_changed_users += update_users(*update_list);
  }

  /* If statement did not affect any distributed users, do not distribute it */
  if (n_changed_users == 0) return Ndb_stored_grants::Strategy::NONE;

  /* The set of users known to be stored in NDB may have changed */
  if (rebuild_local_cache) build_cache_of_ndb_users();

  /* Write snapshot, and handle error */
  if (!write_snapshot()) return Ndb_stored_grants::Strategy::ERROR;

  /* Distribute the whole SQL statement when possible. */
  if ((n_changed_users == n_users_in_statement) && !dist_as_snapshot)
    return Ndb_stored_grants::Strategy::STATEMENT;

  return Ndb_stored_grants::Strategy::SNAPSHOT;
}

}  // anonymous namespace

//
// Public interface
//

bool Ndb_stored_grants::init() {
  const Ndb_mysql_services services;

  // Register the NDB_STORED_USER dynamic privilege
  // NOTE! This privilege is never unregistered
  my_service<SERVICE_TYPE(dynamic_privilege_register)> service(
      "dynamic_privilege_register.mysql_server", services);
  if (!service.is_valid() ||
      service->register_privilege(STRING_WITH_LEN("NDB_STORED_USER"))) {
    return false;
  }
  return true;
}

/* setup() is run as part of binlog setup.
 */
bool Ndb_stored_grants::setup(THD *thd, Thd_ndb *thd_ndb) {
  ThreadContext context(thd);

  if (metadata_table.isRestarting()) {
    ndb_log_info("Ndb_stored_grants::setup() -- after deferred shutdown");
    metadata_table.clear(thd_ndb->ndb->getDictionary());
  } else if (metadata_table.isInitialized()) {
    ndb_log_info("Ndb_stored_grants::setup() -- no op");
    return true;
  } else {
    ndb_log_info("Ndb_stored_grants::setup() -- normal setup");
  }

  /* Create or upgrade the ndb_sql_metadata table.
     If this fails, create_or_upgrade() will log an error message,
     and we return false, which will cause the whole binlog setup
     routine to be retried.
  */
  Ndb_sql_metadata_table sql_metadata_table(thd_ndb);
  if (!sql_metadata_table.create_or_upgrade(thd, true)) return false;

  metadata_table.setup(thd_ndb->ndb->getDictionary(),
                       sql_metadata_table.get_table());
  const NdbError &err = metadata_table.initializeSnapshotLock(thd_ndb->ndb);
  if (err.status != NdbError::Success) {
    ndb_log_error("ndb_stored_grants initalizeSnapshotLock failure: %d %s",
                  err.code, err.message);
    return false;
  }

  return true;
}

void Ndb_stored_grants::shutdown(THD *thd, Thd_ndb *thd_ndb, bool restarting) {
  if (!(thd && thd_ndb)) {
    ndb_log_info("Ndb_stored_grants::shutdown() -- no op");
    return;
  }

  ThreadContext context(thd);
  if (restarting) {
    ndb_log_info("Ndb_stored_grants::shutdown() -- deferred");
    metadata_table.setRestarting();
  } else {
    ndb_log_info("Ndb_stored_grants::shutdown() -- normal shutdown");
    metadata_table.clear(thd_ndb->ndb->getDictionary());
  }
}

bool Ndb_stored_grants::apply_stored_grants(THD *thd) {
  ThreadContext context(thd);

  if (!metadata_table.isInitialized()) {
    ndb_log_error("stored grants: not initialized.");
    return false;
  }

  if (!context.read_snapshot()) return false;

  (void)context.build_cache_of_ndb_users();

  context.consider_all_local_users_for_drop();
  context.apply_current_snapshot();
  context.write_status_message_to_server_log();
  context.handle_dropped_users();
  return true;  // success
}

Ndb_stored_grants::Strategy Ndb_stored_grants::handle_local_acl_change(
    THD *thd, const Acl_change_notification *notice, std::string *user_list,
    bool *schema_dist_use_db, bool *must_refresh) {
  ThreadContext context(thd);

  if (notice == nullptr) {
    ndb_log_error("stored grants: no Acl_change_notification");
    return Strategy::ERROR;
  }

  if (!metadata_table.isInitialized()) {
    ndb_log_error("stored grants: not intialized.");
    return Strategy::ERROR;
  }

  /* Do not distribute CREATE USER statements -- a newly created user
     or role is certain not to have the NDB_STORED_USER privilege.
  */
  const enum_sql_command operation = notice->get_operation();
  if (operation == SQLCOM_CREATE_USER) return Strategy::NONE;

  Strategy strategy = context.handle_change(notice);

  /* Set flags for caller.
   */
  if (strategy == Strategy::STATEMENT) {
    *must_refresh = context.cache_was_rebuilt();
    *schema_dist_use_db =
        (operation == SQLCOM_GRANT || operation == SQLCOM_REVOKE);
  } else if (strategy == Strategy::SNAPSHOT) {
    context.serialize_snapshot_user_list(user_list);
  }

  return strategy;
}

void Ndb_stored_grants::maintain_cache(THD *thd) {
  ThreadContext context(thd);
  context.build_cache_of_ndb_users();
}

bool Ndb_stored_grants::update_users_from_snapshot(THD *thd,
                                                   std::string users) {
  ThreadContext context(thd);

  if (!metadata_table.isInitialized()) {
    ndb_log_error("stored grants: not intialized.");
    return false;
  }

  context.deserialize_users(users);
  if (!context.read_snapshot()) return false;

  (void)context.build_cache_of_ndb_users();
  context.apply_current_snapshot();
  context.handle_dropped_users();
  return true;  // success
}

NdbTransaction *Ndb_stored_grants::acquire_snapshot_lock(THD *thd) {
  NdbTransaction *transaction;
  Ndb *ndb = get_thd_ndb(thd)->ndb;
  const NdbError &err = metadata_table.acquireSnapshotLock(ndb, transaction);
  if (err.status != NdbError::Success)
    ndb_log_error("acquire_snapshot_lock: Error %d '%s'", err.code,
                  err.message);
  return transaction;
}

void Ndb_stored_grants::release_snapshot_lock(NdbTransaction *transaction) {
  metadata_table.releaseSnapshotLock(transaction);
}
