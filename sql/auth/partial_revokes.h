/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef PARTIAL_REVOKES_INCLUDED
#define PARTIAL_REVOKES_INCLUDED

#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#include "map_helpers.h"
#include "memory_debugging.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "sql/auth/auth_common.h"
#include "sql/auth/auth_utility.h"

// Forward declarations
class THD;
class ACL_USER;
class Json_array;
class Json_object;
class Restrictions_aggregator;

// Alias declarations
using db_revocations = std::unordered_map<std::string, Access_bitmask>;
using Db_access_map = std::map<std::string, Access_bitmask>;

/**
  Abstract class for ACL restrictions.
*/
class Abstract_restrictions {
 public:
  explicit Abstract_restrictions();
  virtual ~Abstract_restrictions();
  virtual bool is_empty() const = 0;
  virtual size_t size() const = 0;
  virtual void clear() = 0;
};

/**
  DB Restrictions representation in memory.

  Note that an instance of this class is owned by the security context.
  Many of the usage pattern of the security context has complex life cycle, it
  may be using memory allocated through MEM_ROOT. That may lead to an
  unwarranted memory growth in some circumstances. Therefore, we wish to own the
  life cycle of the non POD type members in this class. Please allocate them
  dynamically otherwise you may cause some difficult to find memory leaks.

  @@note : non POD members are allocated when needed but not in constructor to
  avoid unnecessary memory allocations since it is frequently accessed code
  path. Onus is on the user to call the APIs safely that is to make sure that if
  the accessed member in the API is allocated if it was supposed to be.

  DB_restrictions also provides functions to:
  - Manage DB restrictions
  - Status functions
  - Transformation of in memory db restrictions
*/
class DB_restrictions final : public Abstract_restrictions {
 public:
  DB_restrictions();
  ~DB_restrictions() override;

  db_revocations &operator()(void) { return db_restrictions(); }
  DB_restrictions(const DB_restrictions &restrictions);
  DB_restrictions(DB_restrictions &&restrictions) = delete;
  DB_restrictions &operator=(const DB_restrictions &restrictions);
  DB_restrictions &operator=(DB_restrictions &&restrictions);
  bool operator==(const DB_restrictions &restrictions) const;
  void add(const std::string &db_name, const Access_bitmask revoke_privs);
  void add(const DB_restrictions &restrictions);
  bool add(const Json_object &json_object);

  void remove(const std::string &db_name, const Access_bitmask revoke_privs);
  void remove(const Access_bitmask revoke_privs);

  bool find(const std::string &db_name, Access_bitmask &access) const;
  bool is_empty() const override;
  size_t size() const override;
  void clear() override;
  void get_as_json(Json_array &restrictions_array) const;
  const db_revocations &get() const;
  bool has_more_restrictions(const DB_restrictions &, Access_bitmask) const;

 private:
  db_revocations &db_restrictions();
  void remove(const Access_bitmask remove_restrictions,
              Access_bitmask &restrictions_mask) const noexcept;
  db_revocations *create_restrictions_if_needed();
  void copy_restrictions(const DB_restrictions &other);

 private:
  /**
    Database restrictions.
    Dynamically allocating the memory everytime in constructor would be
    expensive because this is frequently accessed code path. Therefore, we shall
    allocate the memory when needed later on.
  */
  db_revocations *m_restrictions = nullptr;
};

inline const db_revocations &DB_restrictions::get() const {
  assert(m_restrictions != nullptr);
  return *m_restrictions;
}

inline db_revocations *DB_restrictions::create_restrictions_if_needed() {
  if (!m_restrictions) {
    m_restrictions = new db_revocations();
  }
  return m_restrictions;
}

inline db_revocations &DB_restrictions::db_restrictions() {
  assert(m_restrictions != nullptr);
  return *m_restrictions;
}

inline void DB_restrictions::copy_restrictions(const DB_restrictions &other) {
  assert(m_restrictions == nullptr);
  if (other.m_restrictions) {
    m_restrictions = new db_revocations(*other.m_restrictions);
  }
}

/**
  Container of all restrictions for a given user.

  Each object created in the MEM_ROOT has to be destroyed manually.
  It will be the client's responsibility that create the objects.
*/
class Restrictions {
 public:
  explicit Restrictions();

  Restrictions(const Restrictions &) = default;
  Restrictions(Restrictions &&);
  Restrictions &operator=(const Restrictions &);
  Restrictions &operator=(Restrictions &&);
  bool has_more_db_restrictions(const Restrictions &, Access_bitmask);

  ~Restrictions();

  const DB_restrictions &db() const;
  void set_db(const DB_restrictions &db_restrictions);
  void clear_db();
  bool is_empty() const;

 private:
  /** Database restrictions */
  DB_restrictions m_db_restrictions;
};

/**
  Factory class that solely creates an object of type Restrictions_aggregator.

  - The concrete implementations of Restrictions_aggregator cannot be created
    directly since their constructors are private. This class is declared as
    friend in those concrete implementations.
  - It also records the CURRENT_USER in the binlog so that partial_revokes can
    be executed on the replica with context of current user
*/
class Restrictions_aggregator_factory {
 public:
  static std::unique_ptr<Restrictions_aggregator> create(
      THD *thd, const ACL_USER *acl_user, const char *db,
      const Access_bitmask rights, bool is_grant_revoke_all_on_db);

  static std::unique_ptr<Restrictions_aggregator> create(
      const Auth_id &grantor, const Auth_id &grantee,
      const Access_bitmask grantor_access, const Access_bitmask grantee_access,
      const DB_restrictions &grantor_restrictions,
      const DB_restrictions &grantee_restrictions,
      const Access_bitmask required_access, Db_access_map *db_map);

 private:
  static Auth_id fetch_grantor(const Security_context *sctx);
  static Auth_id fetch_grantee(const ACL_USER *acl_user);
  static Access_bitmask fetch_grantor_db_access(THD *thd, const char *db);
  static Access_bitmask fetch_grantee_db_access(THD *thd,
                                                const ACL_USER *acl_user,
                                                const char *db);
  static void fetch_grantor_access(const Security_context *sctx, const char *db,
                                   Access_bitmask &global_access,
                                   Restrictions &restrictions);
  static void fetch_grantee_access(const ACL_USER *grantee,
                                   Access_bitmask &access,
                                   Restrictions &restrictions);
};

/**
  Base class to perform aggregation of two restriction lists

  Aggregation is required if all of the following requirements are met:
  1. Partial revocation feature is enabled
  2. GRANT/REVOKE operation
  3. Either grantor or grantee or both have restrictions associated with them

  Task of the aggregator is to evaluate updates required for grantee's
  restriction. Based on restrictions associated with grantor/grantee:
  A. Add additional restrictions
     E.g. - GRANT of a new privileges by a grantor who has restrictions for
            privileges being granted
          - Creation of restrictions through REVOKE
  B. Remove some restrictions
     E.g. - GRANT of existing privileges by a grantor without restrictions
          - REVOKE of existing privileges

*/
class Restrictions_aggregator {
 public:
  virtual ~Restrictions_aggregator();

  /* interface methods which derived classes have to implement */
  virtual bool generate(Abstract_restrictions &restrictions) = 0;
  virtual bool find_if_require_next_level_operation(
      Access_bitmask &rights) const = 0;

 protected:
  Restrictions_aggregator(const Auth_id &grantor, const Auth_id grantee,
                          const Access_bitmask grantor_global_access,
                          const Access_bitmask grantee_global_access,
                          const Access_bitmask requested_access);
  Restrictions_aggregator(const Restrictions_aggregator &) = delete;
  Restrictions_aggregator &operator=(const Restrictions_aggregator &) = delete;
  Restrictions_aggregator(const Restrictions_aggregator &&) = delete;
  Restrictions_aggregator &operator=(const Restrictions_aggregator &&) = delete;

  enum class Status { Error, Warning, Validated, Aggregated, No_op };

  /** Grantor information */
  const Auth_id m_grantor;

  /** Grantee information */
  const Auth_id m_grantee;

  /** Global static privileges of grantor */
  const Access_bitmask m_grantor_global_access;

  /** Global static privileges of grantee */
  const Access_bitmask m_grantee_global_access;

  /** Privileges that are being granted or revoked */
  const Access_bitmask m_requested_access;

  /** Internal status of aggregation process */
  Status m_status;
};

/**
  Restriction aggregator for database restrictions.
  An umbrella class to cover common methods.
  This is ultimately used for privilege aggregation
  in case of GRANT/REVOKE of database level privileges.
*/
class DB_restrictions_aggregator : public Restrictions_aggregator {
 public:
  bool generate(Abstract_restrictions &restrictions) override;

 protected:
  using Status = Restrictions_aggregator::Status;
  DB_restrictions_aggregator(const Auth_id &grantor, const Auth_id grantee,
                             const Access_bitmask grantor_global_access,
                             const Access_bitmask grantee_global_access,
                             const DB_restrictions &grantor_restrictions,
                             const DB_restrictions &grantee_restrictions,
                             const Access_bitmask requested_access,
                             const Security_context *sctx);
  bool find_if_require_next_level_operation(
      Access_bitmask &rights) const override;

  /* Helper methods and members for derived classes */

  bool check_db_access_and_restrictions_collision(
      const Access_bitmask grantee_db_access,
      const Access_bitmask grantee_restrictions,
      const std::string &db_name) noexcept;
  void set_if_db_level_operation(
      const Access_bitmask requested_access,
      const Access_bitmask restrictions_mask) noexcept;
  enum class SQL_OP { SET_ROLE, GLOBAL_GRANT };
  void aggregate_restrictions(SQL_OP sql_op, const Db_access_map *m_db_map,
                              DB_restrictions &restrictions);
  Access_bitmask get_grantee_db_access(const std::string &db_name) const;
  void get_grantee_db_access(const std::string &db_name,
                             Access_bitmask &access) const;

  /** Privileges that needs to be checked further through DB grants */
  Access_bitmask m_privs_not_processed = 0;

  /** Database restrictions for grantor */
  DB_restrictions m_grantor_rl;

  /** Database restrictions for grantee */
  DB_restrictions m_grantee_rl;

  /** Security context of the current user */
  const Security_context *m_sctx;

 private:
  virtual Status validate() = 0;
  virtual void aggregate(DB_restrictions &restrictions) = 0;
};

/**
  Database restriction aggregator for SET ROLE statement.
*/
class DB_restrictions_aggregator_set_role final
    : public DB_restrictions_aggregator {
  DB_restrictions_aggregator_set_role(
      const Auth_id &grantor, const Auth_id grantee,
      const Access_bitmask grantor_global_access,
      const Access_bitmask grantee_global_access,
      const DB_restrictions &grantor_restrictions,
      const DB_restrictions &grantee_restrictions,
      const Access_bitmask requested_access, Db_access_map *db_map);

  Status validate() override;
  void aggregate(DB_restrictions &db_restrictions) override;
  friend class Restrictions_aggregator_factory;

 private:
  Db_access_map *m_db_map;
};

/**
  Restriction aggregator for GRANT statement for GLOBAL privileges.
*/
class DB_restrictions_aggregator_global_grant final
    : public DB_restrictions_aggregator {
  DB_restrictions_aggregator_global_grant(
      const Auth_id &grantor, const Auth_id grantee,
      const Access_bitmask grantor_global_access,
      const Access_bitmask grantee_global_access,
      const DB_restrictions &grantor_restrictions,
      const DB_restrictions &grantee_restrictions,
      const Access_bitmask requested_access, const Security_context *sctx);

  Status validate() override;
  void aggregate(DB_restrictions &restrictions) override;
  friend class Restrictions_aggregator_factory;
};

class DB_restrictions_aggregator_global_revoke
    : public DB_restrictions_aggregator {
 protected:
  DB_restrictions_aggregator_global_revoke(
      const Auth_id &grantor, const Auth_id grantee,
      const Access_bitmask grantor_global_access,
      const Access_bitmask grantee_global_access,
      const DB_restrictions &grantor_restrictions,
      const DB_restrictions &grantee_restrictions,
      const Access_bitmask requested_access, const Security_context *sctx);
  Status validate_if_grantee_rl_not_empty();

 private:
  Status validate() override;
  void aggregate(DB_restrictions &restrictions) override;
  friend class Restrictions_aggregator_factory;
};

/**
  Restriction aggregator for REVOKE statement over GLOBAL privileges.
*/
class DB_restrictions_aggregator_global_revoke_all final
    : public DB_restrictions_aggregator_global_revoke {
  DB_restrictions_aggregator_global_revoke_all(
      const Auth_id &grantor, const Auth_id grantee,
      const Access_bitmask grantor_global_access,
      const Access_bitmask grantee_global_access,
      const DB_restrictions &grantor_restrictions,
      const DB_restrictions &grantee_restrictions,
      const Access_bitmask requested_access, const Security_context *sctx);
  Status validate() override;
  void aggregate(DB_restrictions &restrictions) override;
  friend class Restrictions_aggregator_factory;
};

/**
  Restriction aggregator for GRANT statement over database privileges.
*/
class DB_restrictions_aggregator_db_grant final
    : public DB_restrictions_aggregator {
  DB_restrictions_aggregator_db_grant(
      const Auth_id &grantor, const Auth_id grantee,
      const Access_bitmask grantor_global_access,
      const Access_bitmask grantee_global_access,
      const Access_bitmask grantor_db_access,
      const Access_bitmask grantee_db_access,
      const DB_restrictions &grantor_restrictions,
      const DB_restrictions &grantee_restrictions,
      const Access_bitmask requested_access, bool is_grant_all,
      const std::string &db_name, const Security_context *sctx);

  void aggregate(DB_restrictions &restrictions) override;
  Status validate() override;

  /** Aggregator needs to access class members */
  friend class Restrictions_aggregator_factory;

  /** Grantor's database privileges */
  const Access_bitmask m_grantor_db_access;

  /** Grantee's database privileges */
  const Access_bitmask m_grantee_db_access;

  /** Flag for GRANT ALL ON \<db\>.* TO ... */
  const bool m_is_grant_all;

  /** Target database of GRANT */
  const std::string m_db_name;
};

/**
  Restriction aggregator for REVOKE statement for database privileges.
*/
class DB_restrictions_aggregator_db_revoke final
    : public DB_restrictions_aggregator {
  DB_restrictions_aggregator_db_revoke(
      const Auth_id &grantor, const Auth_id grantee,
      const Access_bitmask grantor_global_access,
      const Access_bitmask grantee_global_access,
      const Access_bitmask grantor_db_access,
      const Access_bitmask grantee_db_access,
      const DB_restrictions &grantor_restrictions,
      const DB_restrictions &grantee_restrictions,
      const Access_bitmask requested_access, bool is_revoke_all,
      const std::string &db_name, const Security_context *sctx);

  void aggregate(DB_restrictions &restrictions) override;
  Status validate() override;

  /** Aggregator needs to access class members */
  friend class Restrictions_aggregator_factory;

  /** Grantor's database privileges */
  const Access_bitmask m_grantor_db_access;

  /** Grantee's database privileges */
  const Access_bitmask m_grantee_db_access;

  /** Flag for GRANT ALL ON \<db\>.* TO ... */
  const bool m_is_revoke_all;

  /** Target database of REVOKE */
  const std::string m_db_name;
};

#endif /* PARTIAL_REVOKES_INCLUDED */
