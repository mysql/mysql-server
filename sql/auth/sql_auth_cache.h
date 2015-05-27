#ifndef SQL_USER_CACHE_INCLUDED
#define SQL_USER_CACHE_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "my_global.h"                  // NO_EMBEDDED_ACCESS_CHECKS
#include "my_sys.h"                     // wild_many, wild_one, wild_prefix
#include <string.h>                     // strchr
#include "mysql_com.h"                  // SCRAMBLE_LENGTH
#include "violite.h"                    // SSL_type
#include "hash_filo.h"                  // HASH, hash_filo
#include "records.h"                    // READ_RECORD
#include "partitioned_rwlock.h"         // Partitioned_rwlock

#include "prealloced_array.h"

/* Forward Declarations */
class String;

/* Classes */

class ACL_HOST_AND_IP
{
  char *hostname;
  size_t hostname_length;
  long ip, ip_mask; // Used with masked ip:s

  const char *calc_ip(const char *ip_arg, long *val, char end);

public:
  const char *get_host() const { return hostname; }
  size_t get_host_len() { return hostname_length; }

  bool has_wildcard()
  {
    return (strchr(hostname,wild_many) ||
            strchr(hostname,wild_one)  || ip_mask );
  }

  bool check_allow_all_hosts()
  {
    return (!hostname ||
            (hostname[0] == wild_many && !hostname[1]));
  }

  void update_hostname(const char *host_arg);

  bool compare_hostname(const char *host_arg, const char *ip_arg);

};

class ACL_ACCESS {
public:
  ACL_HOST_AND_IP host;
  ulong sort;
  ulong access;
};

/* ACL_HOST is used if no host is specified */

class ACL_HOST :public ACL_ACCESS
{
public:
  char *db;
};

class ACL_USER :public ACL_ACCESS
{
public:
  USER_RESOURCES user_resource;
  char *user;
  /**
    The salt variable is used as the password hash for
    native_password_authetication.
  */
  uint8 salt[SCRAMBLE_LENGTH + 1];       // scrambled password in binary form
  /**
    In the old protocol the salt_len indicated what type of autnetication
    protocol was used: 0 - no password, 4 - 3.20, 8 - 4.0,  20 - 4.1.1
  */
  uint8 salt_len;
  enum SSL_type ssl_type;
  const char *ssl_cipher, *x509_issuer, *x509_subject;
  LEX_CSTRING plugin;
  LEX_STRING auth_string;
  bool password_expired;
  bool can_authenticate;
  MYSQL_TIME password_last_changed;
  uint password_lifetime;
  bool use_default_password_lifetime;
  /**
    Specifies whether the user account is locked or unlocked.
  */
  bool account_locked;

  ACL_USER *copy(MEM_ROOT *root);
};

class ACL_DB :public ACL_ACCESS
{
public:
  char *user,*db;
};

class ACL_PROXY_USER :public ACL_ACCESS
{
  const char *user;
  ACL_HOST_AND_IP proxied_host;
  const char *proxied_user;
  bool with_grant;

  typedef enum { 
    MYSQL_PROXIES_PRIV_HOST, 
    MYSQL_PROXIES_PRIV_USER, 
    MYSQL_PROXIES_PRIV_PROXIED_HOST,
    MYSQL_PROXIES_PRIV_PROXIED_USER, 
    MYSQL_PROXIES_PRIV_WITH_GRANT,
    MYSQL_PROXIES_PRIV_GRANTOR,
    MYSQL_PROXIES_PRIV_TIMESTAMP } old_acl_proxy_users;
public:
  ACL_PROXY_USER () {};

  void init(const char *host_arg, const char *user_arg,
            const char *proxied_host_arg, const char *proxied_user_arg,
            bool with_grant_arg);

  void init(MEM_ROOT *mem, const char *host_arg, const char *user_arg,
            const char *proxied_host_arg, const char *proxied_user_arg,
            bool with_grant_arg);

  void init(TABLE *table, MEM_ROOT *mem);

  bool get_with_grant() { return with_grant; }
  const char *get_user() { return user; }
  const char *get_proxied_user() { return proxied_user; }
  const char *get_proxied_host() { return proxied_host.get_host(); }
  void set_user(MEM_ROOT *mem, const char *user_arg) 
  { 
    user= user_arg && *user_arg ? strdup_root(mem, user_arg) : NULL;
  }

  bool check_validity(bool check_no_resolve);

  bool matches(const char *host_arg, const char *user_arg, const char *ip_arg,
                const char *proxied_user_arg, bool any_proxy_user);

  inline static bool auth_element_equals(const char *a, const char *b)
  {
    return (a == b || (a != NULL && b != NULL && !strcmp(a,b)));
  }


  bool pk_equals(ACL_PROXY_USER *grant);

  bool granted_on(const char *host_arg, const char *user_arg)
  {
    return (((!user && (!user_arg || !user_arg[0])) ||
             (user && user_arg && !strcmp(user, user_arg))) &&
            ((!host.get_host() && (!host_arg || !host_arg[0])) ||
             (host.get_host() && host_arg && !strcmp(host.get_host(), host_arg))));
  }


  void print_grant(String *str);

  void set_data(ACL_PROXY_USER *grant)
  {
    with_grant= grant->with_grant;
  }

  static int store_pk(TABLE *table,
                      const LEX_CSTRING &host,
                      const LEX_CSTRING &user,
                      const LEX_CSTRING &proxied_host,
                      const LEX_CSTRING &proxied_user);

  static int store_with_grant(TABLE * table,
                              bool with_grant);

  static int store_data_record(TABLE *table,
                               const LEX_CSTRING &host,
                               const LEX_CSTRING &user,
                               const LEX_CSTRING &proxied_host,
                               const LEX_CSTRING &proxied_user,
                               bool with_grant,
                               const char *grantor);
};

#ifndef NO_EMBEDDED_ACCESS_CHECKS

class acl_entry :public hash_filo_element
{
public:
  ulong access;
  uint16 length;
  char key[1];                          // Key will be stored here
};


class GRANT_COLUMN :public Sql_alloc
{
public:
  char *column;
  ulong rights;
  size_t key_length;
  GRANT_COLUMN(String &c,  ulong y);
};


class GRANT_NAME :public Sql_alloc
{
public:
  ACL_HOST_AND_IP host;
  char *db, *user, *tname, *hash_key;
  ulong privs;
  ulong sort;
  size_t key_length;
  GRANT_NAME(const char *h, const char *d,const char *u,
             const char *t, ulong p, bool is_routine);
  GRANT_NAME (TABLE *form, bool is_routine);
  virtual ~GRANT_NAME() {};
  virtual bool ok() { return privs != 0; }
  void set_user_details(const char *h, const char *d,
                        const char *u, const char *t,
                        bool is_routine);
};


class GRANT_TABLE :public GRANT_NAME
{
public:
  ulong cols;
  HASH hash_columns;

  GRANT_TABLE(const char *h, const char *d,const char *u,
              const char *t, ulong p, ulong c);
  explicit GRANT_TABLE(TABLE *form);
  bool init(TABLE *col_privs);
  ~GRANT_TABLE();
  bool ok() { return privs != 0 || cols != 0; }
};


#endif /* NO_EMBEDDED_ACCESS_CHECKS */


/* Data Structures */

#ifndef NO_EMBEDDED_ACCESS_CHECKS
extern MEM_ROOT global_acl_memory;
extern MEM_ROOT memex; 
extern bool initialized;
const size_t ACL_PREALLOC_SIZE = 10U;
extern Prealloced_array<ACL_USER, ACL_PREALLOC_SIZE> *acl_users;
extern Prealloced_array<ACL_PROXY_USER, ACL_PREALLOC_SIZE> *acl_proxy_users;
extern Prealloced_array<ACL_DB, ACL_PREALLOC_SIZE> *acl_dbs;
extern Prealloced_array<ACL_HOST_AND_IP, ACL_PREALLOC_SIZE> *acl_wild_hosts;
extern HASH column_priv_hash, proc_priv_hash, func_priv_hash;
extern hash_filo *acl_cache;
extern HASH acl_check_hosts;
extern bool allow_all_hosts;
extern uint grant_version; /* Version of priv tables */
extern Partitioned_rwlock LOCK_grant;

GRANT_NAME *name_hash_search(HASH *name_hash,
                             const char *host,const char* ip,
                             const char *db,
                             const char *user, const char *tname,
                             bool exact, bool name_tolower);

inline GRANT_NAME * routine_hash_search(const char *host, const char *ip,
                                        const char *db, const char *user,
                                        const char *tname, bool proc,
                                        bool exact)
{
  return (GRANT_TABLE*)
    name_hash_search(proc ? &proc_priv_hash : &func_priv_hash,
                     host, ip, db, user, tname, exact, TRUE);
}

inline GRANT_TABLE * table_hash_search(const char *host, const char *ip,
                                       const char *db, const char *user,
                                       const char *tname, bool exact)
{
  return (GRANT_TABLE*) name_hash_search(&column_priv_hash, host, ip, db,
                                         user, tname, exact, FALSE);
}

inline GRANT_COLUMN * column_hash_search(GRANT_TABLE *t, const char *cname,
                                         size_t length)
{
  return (GRANT_COLUMN*) my_hash_search(&t->hash_columns,
                                        (uchar*) cname, length);
}


#endif /* NO_EMBEDDED_ACCESS_CHECKS */

#endif /* SQL_USER_CACHE_INCLUDED */
