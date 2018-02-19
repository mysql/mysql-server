/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_show.h"                   /* append_identifier */
#include "log.h"                        /* sql_print_warning */
#include "sql_base.h"                   /* MYSQL_LOCK_IGNORE_TIMEOUT */
#include "key.h"                        /* key_copy, key_cmp_if_same */
                                        /* key_restore */

#include "auth_internal.h"
#include "sql_auth_cache.h"
#include "sql_authentication.h"
#include "sql_time.h"
#include "sql_plugin.h"                         // lock_plugin_data etc.
#include "debug_sync.h"
#include "sql_user_table.h"

#define INVALID_DATE "0000-00-00 00:00:00"

#include <algorithm>
#include <functional>
using std::min;

struct ACL_internal_schema_registry_entry
{
  const LEX_STRING *m_name;
  const ACL_internal_schema_access *m_access;
};
/**
  Internal schema registered.
  Currently, this is only:
  - performance_schema
  - information_schema,
  This can be reused later for:
  - mysql
*/
static ACL_internal_schema_registry_entry registry_array[2];
static uint m_registry_array_size= 0;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
MEM_ROOT global_acl_memory;
MEM_ROOT memex;
Prealloced_array<ACL_USER, ACL_PREALLOC_SIZE> *acl_users= NULL;
Prealloced_array<ACL_PROXY_USER, ACL_PREALLOC_SIZE> *acl_proxy_users= NULL;
Prealloced_array<ACL_DB, ACL_PREALLOC_SIZE> *acl_dbs= NULL;
Prealloced_array<ACL_HOST_AND_IP, ACL_PREALLOC_SIZE> *acl_wild_hosts= NULL;

HASH column_priv_hash, proc_priv_hash, func_priv_hash;
hash_filo *acl_cache;
HASH acl_check_hosts;

bool initialized=0;
bool allow_all_hosts=1;
uint grant_version=0; /* Version of priv tables */
my_bool validate_user_plugins= TRUE;
/**
  Flag to track if rwlocks in ACL subsystem were initialized.
  Necessary because acl_free() can be called in some error scenarios
  without prior call to acl_init().
*/
bool rwlocks_initialized= false;

const uint LOCK_GRANT_PARTITIONS= 32;
Partitioned_rwlock LOCK_grant;

#define FIRST_NON_YN_FIELD 26

#define IP_ADDR_STRLEN (3 + 1 + 3 + 1 + 3 + 1 + 3)
#define ACL_KEY_LENGTH (IP_ADDR_STRLEN + 1 + NAME_LEN + \
                        1 + USERNAME_LENGTH + 1)

#endif /* NO_EMBEDDED_ACCESS_CHECKS */

/**
  Add an internal schema to the registry.
  @param name the schema name
  @param access the schema ACL specific rules
*/
void ACL_internal_schema_registry::register_schema
  (const LEX_STRING &name, const ACL_internal_schema_access *access)
{
  DBUG_ASSERT(m_registry_array_size < array_elements(registry_array));

  /* Not thread safe, and does not need to be. */
  registry_array[m_registry_array_size].m_name= &name;
  registry_array[m_registry_array_size].m_access= access;
  m_registry_array_size++;
}


/**
  Search per internal schema ACL by name.
  @param name a schema name
  @return per schema rules, or NULL
*/
const ACL_internal_schema_access *
ACL_internal_schema_registry::lookup(const char *name)
{
  DBUG_ASSERT(name != NULL);

  uint i;

  for (i= 0; i<m_registry_array_size; i++)
  {
    if (my_strcasecmp(system_charset_info, registry_array[i].m_name->str,
                      name) == 0)
      return registry_array[i].m_access;
  }
  return NULL;
}


const char *
ACL_HOST_AND_IP::calc_ip(const char *ip_arg, long *val, char end)
{
  long ip_val,tmp;
  if (!(ip_arg=str2int(ip_arg,10,0,255,&ip_val)) || *ip_arg != '.')
    return 0;
  ip_val<<=24;
  if (!(ip_arg=str2int(ip_arg+1,10,0,255,&tmp)) || *ip_arg != '.')
    return 0;
  ip_val+=tmp<<16;
  if (!(ip_arg=str2int(ip_arg+1,10,0,255,&tmp)) || *ip_arg != '.')
    return 0;
  ip_val+=tmp<<8;
  if (!(ip_arg=str2int(ip_arg+1,10,0,255,&tmp)) || *ip_arg != end)
    return 0;
  *val=ip_val+tmp;
  return ip_arg;
}

/**
  @brief Update the hostname. Updates ip and ip_mask accordingly.

  @param host_arg Value to be stored
 */
void
ACL_HOST_AND_IP::update_hostname(const char *host_arg)
{
  hostname=(char*) host_arg;     // This will not be modified!
  hostname_length= hostname ? strlen( hostname ) : 0;
  if (!host_arg ||
      (!(host_arg=(char*) calc_ip(host_arg,&ip,'/')) ||
       !(host_arg=(char*) calc_ip(host_arg+1,&ip_mask,'\0'))))
  {
    ip= ip_mask=0;               // Not a masked ip
  }
}

/*
   @brief Comparing of hostnames

   @param  host_arg    Hostname to be compared with
   @param  ip_arg      IP address to be compared with

   @notes
   A hostname may be of type:
   1) hostname   (May include wildcards);   monty.pp.sci.fi
   2) ip     (May include wildcards);   192.168.0.0
   3) ip/netmask                        192.168.0.0/255.255.255.0
   A net mask of 0.0.0.0 is not allowed.

   @return
   true   if matched
   false  if not matched
 */

bool
ACL_HOST_AND_IP::compare_hostname(const char *host_arg, const char *ip_arg)
{
  long tmp;
  if (ip_mask && ip_arg && calc_ip(ip_arg,&tmp,'\0'))
  {
    return (tmp & ip_mask) == ip;
  }
  return (!hostname ||
      (host_arg &&
       !wild_case_compare(system_charset_info, host_arg, hostname)) ||
      (ip_arg && !wild_compare(ip_arg, hostname, 0)));
}

ACL_USER *
ACL_USER::copy(MEM_ROOT *root)
{
  ACL_USER *dst= (ACL_USER *) alloc_root(root, sizeof(ACL_USER));
  if (!dst)
    return 0;
  *dst= *this;
  dst->user= safe_strdup_root(root, user);
  dst->ssl_cipher= safe_strdup_root(root, ssl_cipher);
  dst->x509_issuer= safe_strdup_root(root, x509_issuer);
  dst->x509_subject= safe_strdup_root(root, x509_subject);
  /*
     If the plugin is built in we don't need to reallocate the name of the
     plugin.
   */
  if (auth_plugin_is_built_in(dst->plugin.str))
    dst->plugin= plugin;
  else
  {
    dst->plugin.str= strmake_root(root, plugin.str, plugin.length);
    dst->plugin.length= plugin.length;
  }
  dst->auth_string.str= safe_strdup_root(root, auth_string.str);
  dst->host.update_hostname(safe_strdup_root(root, host.get_host()));
  return dst;
}

void
ACL_PROXY_USER::init(const char *host_arg, const char *user_arg,
                     const char *proxied_host_arg,
                     const char *proxied_user_arg, bool with_grant_arg)
{
  user= (user_arg && *user_arg) ? user_arg : NULL;
  host.update_hostname ((host_arg && *host_arg) ? host_arg : NULL);
  proxied_user= (proxied_user_arg && *proxied_user_arg) ? 
    proxied_user_arg : NULL;
  proxied_host.update_hostname ((proxied_host_arg && *proxied_host_arg) ?
      proxied_host_arg : NULL);
  with_grant= with_grant_arg;
  sort= get_sort(4, host.get_host(), user,
      proxied_host.get_host(), proxied_user);
}

void
ACL_PROXY_USER::init(MEM_ROOT *mem, const char *host_arg, const char *user_arg,
                     const char *proxied_host_arg,
                     const char *proxied_user_arg, bool with_grant_arg)
{
  init ((host_arg && *host_arg) ? strdup_root (mem, host_arg) : NULL,
      (user_arg && *user_arg) ? strdup_root (mem, user_arg) : NULL,
      (proxied_host_arg && *proxied_host_arg) ? 
      strdup_root (mem, proxied_host_arg) : NULL,
      (proxied_user_arg && *proxied_user_arg) ? 
      strdup_root (mem, proxied_user_arg) : NULL,
      with_grant_arg);
}

void
ACL_PROXY_USER::init(TABLE *table, MEM_ROOT *mem)
{
  init (get_field(mem, table->field[MYSQL_PROXIES_PRIV_HOST]),
        get_field(mem, table->field[MYSQL_PROXIES_PRIV_USER]),
        get_field(mem, table->field[MYSQL_PROXIES_PRIV_PROXIED_HOST]),
        get_field(mem, table->field[MYSQL_PROXIES_PRIV_PROXIED_USER]),
                  table->field[MYSQL_PROXIES_PRIV_WITH_GRANT]->val_int() != 0);
}

bool
ACL_PROXY_USER::check_validity(bool check_no_resolve)
{
  if (check_no_resolve && 
      (hostname_requires_resolving(host.get_host()) ||
       hostname_requires_resolving(proxied_host.get_host())))
  {
    sql_print_warning("'proxies_priv' entry '%s@%s %s@%s' "
                      "ignored in --skip-name-resolve mode.",
                      proxied_user ? proxied_user : "",
                      proxied_host.get_host() ? proxied_host.get_host() : "",
                      user ? user : "",
                      host.get_host() ? host.get_host() : "");
  }
  return FALSE;
}

bool
ACL_PROXY_USER::matches(const char *host_arg, const char *user_arg,
                        const char *ip_arg, const char *proxied_user_arg,
						bool any_proxy_user)
{
  DBUG_ENTER("ACL_PROXY_USER::matches");
  DBUG_PRINT("info", ("compare_hostname(%s,%s,%s) &&"
             "compare_hostname(%s,%s,%s) &&"
             "wild_compare (%s,%s) &&"
             "wild_compare (%s,%s)",
             host.get_host() ? host.get_host() : "<NULL>",
             host_arg ? host_arg : "<NULL>",
             ip_arg ? ip_arg : "<NULL>",
             proxied_host.get_host() ? proxied_host.get_host() : "<NULL>",
             host_arg ? host_arg : "<NULL>",
             ip_arg ? ip_arg : "<NULL>",
             user_arg ? user_arg : "<NULL>",
             user ? user : "<NULL>",
             proxied_user_arg ? proxied_user_arg : "<NULL>",
             proxied_user ? proxied_user : "<NULL>"));
  DBUG_RETURN(host.compare_hostname(host_arg, ip_arg) &&
              proxied_host.compare_hostname(host_arg, ip_arg) &&
              (!user ||
               (user_arg && !wild_compare(user_arg, user, TRUE))) &&
              (any_proxy_user || !proxied_user || 
               (proxied_user && !wild_compare(proxied_user_arg, proxied_user,
                                              TRUE))));
}

bool
ACL_PROXY_USER::pk_equals(ACL_PROXY_USER *grant)
{
  DBUG_ENTER("pk_equals");
  DBUG_PRINT("info", ("strcmp(%s,%s) &&"
             "strcmp(%s,%s) &&"
             "wild_compare (%s,%s) &&"
             "wild_compare (%s,%s)",
             user ? user : "<NULL>",
             grant->user ? grant->user : "<NULL>",
             proxied_user ? proxied_user : "<NULL>",
             grant->proxied_user ? grant->proxied_user : "<NULL>",
             host.get_host() ? host.get_host() : "<NULL>",
             grant->host.get_host() ? grant->host.get_host() : "<NULL>",
             proxied_host.get_host() ? proxied_host.get_host() : "<NULL>",
             grant->proxied_host.get_host() ? 
             grant->proxied_host.get_host() : "<NULL>"));

  DBUG_RETURN(auth_element_equals(user, grant->user) &&
              auth_element_equals(proxied_user, grant->proxied_user) &&
              auth_element_equals(host.get_host(), grant->host.get_host()) &&
              auth_element_equals(proxied_host.get_host(), 
                                  grant->proxied_host.get_host()));
}

void
ACL_PROXY_USER::print_grant(String *str)
{
  str->append(STRING_WITH_LEN("GRANT PROXY ON '"));
  if (proxied_user)
    str->append(proxied_user, strlen(proxied_user));
  str->append(STRING_WITH_LEN("'@'"));
  if (proxied_host.get_host())
    str->append(proxied_host.get_host(), strlen(proxied_host.get_host()));
  str->append(STRING_WITH_LEN("' TO '"));
  if (user)
    str->append(user, strlen(user));
  str->append(STRING_WITH_LEN("'@'"));
  if (host.get_host())
    str->append(host.get_host(), strlen(host.get_host()));
  str->append(STRING_WITH_LEN("'"));
  if (with_grant)
    str->append(STRING_WITH_LEN(" WITH GRANT OPTION"));
}

int
ACL_PROXY_USER::store_pk(TABLE *table,
                         const LEX_CSTRING &host,
                         const LEX_CSTRING &user,
                         const LEX_CSTRING &proxied_host,
                         const LEX_CSTRING &proxied_user)
{
  DBUG_ENTER("ACL_PROXY_USER::store_pk");
  DBUG_PRINT("info", ("host=%s, user=%s, proxied_host=%s, proxied_user=%s",
                      host.str ? host.str : "<NULL>",
                      user.str ? user.str : "<NULL>",
                      proxied_host.str ? proxied_host.str : "<NULL>",
                      proxied_user.str ? proxied_user.str : "<NULL>"));
  if (table->field[MYSQL_PROXIES_PRIV_HOST]->store(host.str,
                                                   host.length,
                                                   system_charset_info))
    DBUG_RETURN(TRUE);
  if (table->field[MYSQL_PROXIES_PRIV_USER]->store(user.str,
                                                   user.length,
                                                   system_charset_info))
    DBUG_RETURN(TRUE);
  if (table->field[MYSQL_PROXIES_PRIV_PROXIED_HOST]->store(proxied_host.str,
                                                           proxied_host.length,
                                                           system_charset_info))
    DBUG_RETURN(TRUE);
  if (table->field[MYSQL_PROXIES_PRIV_PROXIED_USER]->store(proxied_user.str,
                                                           proxied_user.length,
                                                           system_charset_info))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}

int
ACL_PROXY_USER::store_with_grant(TABLE * table,
                                 bool with_grant)
{
  DBUG_ENTER("ACL_PROXY_USER::store_with_grant");
  DBUG_PRINT("info", ("with_grant=%s", with_grant ? "TRUE" : "FALSE"));
  if (table->field[MYSQL_PROXIES_PRIV_WITH_GRANT]->store(with_grant ? 1 : 0,
                                                         TRUE))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}

int
ACL_PROXY_USER::store_data_record(TABLE *table,
                                  const LEX_CSTRING &host,
                                  const LEX_CSTRING &user,
                                  const LEX_CSTRING &proxied_host,
                                  const LEX_CSTRING &proxied_user,
                                  bool with_grant,
                                  const char *grantor)
{
  DBUG_ENTER("ACL_PROXY_USER::store_pk");
  if (store_pk(table,  host, user, proxied_host, proxied_user))
    DBUG_RETURN(TRUE);
  if (store_with_grant(table, with_grant))
    DBUG_RETURN(TRUE);
  if (table->field[MYSQL_PROXIES_PRIV_GRANTOR]->store(grantor, 
                                                      strlen(grantor),
                                                      system_charset_info))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}


int wild_case_compare(CHARSET_INFO *cs, const char *str,const char *wildstr)
{
  int flag;
  DBUG_ENTER("wild_case_compare");
  DBUG_PRINT("enter",("str: '%s'  wildstr: '%s'",str,wildstr));
  while (*wildstr)
  {
    while (*wildstr && *wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == wild_prefix && wildstr[1])
        wildstr++;
      if (my_toupper(cs, *wildstr++) !=
          my_toupper(cs, *str++)) DBUG_RETURN(1);
    }
    if (! *wildstr ) DBUG_RETURN (*str != 0);
    if (*wildstr++ == wild_one)
    {
      if (! *str++) DBUG_RETURN (1);    /* One char; skip */
    }
    else
    {                                           /* Found '*' */
      if (!*wildstr) DBUG_RETURN(0);            /* '*' as last char: OK */
      flag=(*wildstr != wild_many && *wildstr != wild_one);
      do
      {
        if (flag)
        {
          char cmp;
          if ((cmp= *wildstr) == wild_prefix && wildstr[1])
            cmp=wildstr[1];
          cmp=my_toupper(cs, cmp);
          while (*str && my_toupper(cs, *str) != cmp)
            str++;
          if (!*str) DBUG_RETURN (1);
        }
        if (wild_case_compare(cs, str,wildstr) == 0) DBUG_RETURN (0);
      } while (*str++);
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN (*str != '\0');
}


/*
  Return a number which, if sorted 'desc', puts strings in this order:
    no wildcards
    strings containg wildcards and non-wildcard characters
    single muilt-wildcard character('%')
    empty string
*/

ulong get_sort(uint count,...)
{
  va_list args;
  va_start(args,count);
  ulong sort=0;

  /* Should not use this function with more than 4 arguments for compare. */
  DBUG_ASSERT(count <= 4);

  while (count--)
  {
    char *start, *str= va_arg(args,char*);
    uint chars= 0;
    uint wild_pos= 0;

    /*
      wild_pos
        0                            if string is empty
        1                            if string is a single muilt-wildcard
                                     character('%')
        first wildcard position + 1  if string containg wildcards and
                                     non-wildcard characters
    */

    if ((start= str))
    {
      for (; *str ; str++)
      {
        if (*str == wild_prefix && str[1])
          str++;
        else if (*str == wild_many || *str == wild_one)
        {
          wild_pos= (uint) (str - start) + 1;
          if (!(wild_pos == 1 && *str == wild_many && *(++str) == '\0'))
            wild_pos++;
          break;
        }
        chars= 128;                             // Marker that chars existed
      }
    }
    sort= (sort << 8) + (wild_pos ? min(wild_pos, 127U) : chars);
  }
  va_end(args);
  return sort;
}


/**
  Check if the given host name needs to be resolved or not.
  Host name has to be resolved if it actually contains *name*.

  For example:
    192.168.1.1               --> FALSE
    192.168.1.0/255.255.255.0 --> FALSE
    %                         --> FALSE
    192.168.1.%               --> FALSE
    AB%                       --> FALSE

    AAAAFFFF                  --> TRUE (Hostname)
    AAAA:FFFF:1234:5678       --> FALSE
    ::1                       --> FALSE

  This function does not check if the given string is a valid host name or
  not. It assumes that the argument is a valid host name.

  @param hostname   the string to check.

  @return a flag telling if the argument needs to be resolved or not.
  @retval TRUE the argument is a host name and needs to be resolved.
  @retval FALSE the argument is either an IP address, or a patter and
          should not be resolved.
*/

bool hostname_requires_resolving(const char *hostname)
{

  /* called only for --skip-name-resolve */
  DBUG_ASSERT(specialflag & SPECIAL_NO_RESOLVE);

  if (!hostname)
    return FALSE;

  /*
    If the string contains any of {':', '%', '_', '/'}, it is definitely
    not a host name:
      - ':' means that the string is an IPv6 address;
      - '%' or '_' means that the string is a pattern;
      - '/' means that the string is an IPv4 network address;
  */

  for (const char *p= hostname; *p; ++p)
  {
    switch (*p) {
      case ':':
      case '%':
      case '_':
      case '/':
        return FALSE;
    }
  }

  /*
    Now we have to tell a host name (ab.cd, 12.ab) from an IPv4 address
    (12.34.56.78). The assumption is that if the string contains only
    digits and dots, it is an IPv4 address. Otherwise -- a host name.
  */

  for (const char *p= hostname; *p; ++p)
  {
    if (*p != '.' && !my_isdigit(&my_charset_latin1, *p))
      return TRUE; /* a "letter" has been found. */
  }

  return FALSE; /* all characters are either dots or digits. */
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS


static uchar* get_key_column(GRANT_COLUMN *buff, size_t *length,
                             my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length=buff->key_length;
  return (uchar*) buff->column;
}


uchar* get_grant_table(GRANT_NAME *buff, size_t *length,
                       my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length=buff->key_length;
  return (uchar*) buff->hash_key;
}


GRANT_COLUMN::GRANT_COLUMN(String &c,  ulong y) :rights (y)
{
  column= (char*) memdup_root(&memex,c.ptr(), key_length=c.length());
}


void GRANT_NAME::set_user_details(const char *h, const char *d,
                                  const char *u, const char *t,
                                  bool is_routine)
{
  /* Host given by user */
  host.update_hostname(strdup_root(&memex, h));
  if (db != d)
  {
    db= strdup_root(&memex, d);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, db);
  }
  user = strdup_root(&memex,u);
  sort=  get_sort(3,host.get_host(),db,user);
  if (tname != t)
  {
    tname= strdup_root(&memex, t);
    if (lower_case_table_names || is_routine)
      my_casedn_str(files_charset_info, tname);
  }
  key_length= strlen(d) + strlen(u)+ strlen(t)+3;
  hash_key=   (char*) alloc_root(&memex,key_length);
  my_stpcpy(my_stpcpy(my_stpcpy(hash_key,user)+1,db)+1,tname);
}

GRANT_NAME::GRANT_NAME(const char *h, const char *d,const char *u,
                       const char *t, ulong p, bool is_routine)
  :db(0), tname(0), privs(p)
{
  set_user_details(h, d, u, t, is_routine);
}

GRANT_TABLE::GRANT_TABLE(const char *h, const char *d,const char *u,
                         const char *t, ulong p, ulong c)
  :GRANT_NAME(h,d,u,t,p, FALSE), cols(c)
{
  (void) my_hash_init2(&hash_columns,4,system_charset_info,
                   0,0,0, (my_hash_get_key) get_key_column,0,0,
                   key_memory_acl_memex);
}


GRANT_NAME::GRANT_NAME(TABLE *form, bool is_routine)
{
  host.update_hostname(get_field(&memex, form->field[0]));
  db=    get_field(&memex,form->field[1]);
  user=  get_field(&memex,form->field[2]);
  if (!user)
    user= (char*) "";
  sort=  get_sort(3, host.get_host(), db, user);
  tname= get_field(&memex,form->field[3]);
  if (!db || !tname) {
    /* Wrong table row; Ignore it */
    privs= 0;
    return;                                     /* purecov: inspected */
  }
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, db);
  }
  if (lower_case_table_names || is_routine)
  {
    my_casedn_str(files_charset_info, tname);
  }
  key_length= (strlen(db) + strlen(user) + strlen(tname) + 3);
  hash_key=   (char*) alloc_root(&memex, key_length);
  my_stpcpy(my_stpcpy(my_stpcpy(hash_key,user)+1,db)+1,tname);

  if (form->field[MYSQL_TABLES_PRIV_FIELD_TABLE_PRIV])
  {
    privs = (ulong) form->field[MYSQL_TABLES_PRIV_FIELD_TABLE_PRIV]->val_int();
    privs = fix_rights_for_table(privs);
  }
}


GRANT_TABLE::GRANT_TABLE(TABLE *form)
  :GRANT_NAME(form, false)
{
  if (!db || !tname)
  {
    /* Wrong table row; Ignore it */
    my_hash_clear(&hash_columns);               /* allow for destruction */
    cols= 0;
    return;
  }

  if (form->field[MYSQL_TABLES_PRIV_FIELD_COLUMN_PRIV])
  {
    cols= (ulong) form->field[MYSQL_TABLES_PRIV_FIELD_COLUMN_PRIV]->val_int();
    cols =  fix_rights_for_column(cols);
  }
  else
    cols= 0;

  (void) my_hash_init2(&hash_columns,4,system_charset_info,
                   0,0,0, (my_hash_get_key) get_key_column,0,0,
                   key_memory_acl_memex);
}


GRANT_TABLE::~GRANT_TABLE()
{
  my_hash_free(&hash_columns);
}


bool GRANT_TABLE::init(TABLE *col_privs)
{
  int error;

  if (cols)
  {
    uchar key[MAX_KEY_LENGTH];
    uint key_prefix_len;

    if (!col_privs->key_info)
    {
      my_error(ER_MISSING_KEY, MYF(0), col_privs->s->db.str,
               col_privs->s->table_name.str);
      return true;
    }

    KEY_PART_INFO *key_part= col_privs->key_info->key_part;
    col_privs->field[0]->store(host.get_host(),
                               host.get_host() ? host.get_host_len() : 0,
                               system_charset_info);
    col_privs->field[1]->store(db, strlen(db), system_charset_info);
    col_privs->field[2]->store(user, strlen(user), system_charset_info);
    col_privs->field[3]->store(tname, strlen(tname), system_charset_info);

    key_prefix_len= (key_part[0].store_length +
                     key_part[1].store_length +
                     key_part[2].store_length +
                     key_part[3].store_length);
    key_copy(key, col_privs->record[0], col_privs->key_info, key_prefix_len);
    col_privs->field[4]->store("", 0, &my_charset_latin1);

    error= col_privs->file->ha_index_init(0, 1);
    if (error)
    {
      acl_print_ha_error(col_privs, error);
      return true;
    }

    error=
      col_privs->file->ha_index_read_map(col_privs->record[0], (uchar*) key,
                                         (key_part_map)15, HA_READ_KEY_EXACT);
    DBUG_EXECUTE_IF("se_error_grant_table_init_read",
                    error= HA_ERR_LOCK_WAIT_TIMEOUT;);
    if (error)
    {
      bool ret= false;
      cols= 0;
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        acl_print_ha_error(col_privs, error);
        ret= true;
      }
      col_privs->file->ha_index_end();
      return ret;
    }

    do
    {
      String *res,column_name;
      GRANT_COLUMN *mem_check;
      /* As column name is a string, we don't have to supply a buffer */
      res= col_privs->field[4]->val_str(&column_name);
      ulong priv= (ulong) col_privs->field[6]->val_int();
      if (!(mem_check= new GRANT_COLUMN(*res,
                                        fix_rights_for_column(priv))) ||
            my_hash_insert(&hash_columns, (uchar *) mem_check))
      {
        /* Don't use this entry */
        col_privs->file->ha_index_end();
        return true;
      }

      error= col_privs->file->ha_index_next(col_privs->record[0]);
      DBUG_EXECUTE_IF("se_error_grant_table_init_read_next",
                      error= HA_ERR_LOCK_WAIT_TIMEOUT;);
      if (error && error != HA_ERR_END_OF_FILE)
      {
        acl_print_ha_error(col_privs, error);
        col_privs->file->ha_index_end();
        return true;
      }
    }
    while (!error && !key_cmp_if_same(col_privs,key,0,key_prefix_len));
    col_privs->file->ha_index_end();
  }

  return false;
}

/*
  Find first entry that matches the current user
*/

ACL_USER *
find_acl_user(const char *host, const char *user, my_bool exact)
{
  DBUG_ENTER("find_acl_user");
  DBUG_PRINT("enter",("host: '%s'  user: '%s'",host,user));

  mysql_mutex_assert_owner(&acl_cache->lock);

  if (likely(acl_users))
  {
    for (ACL_USER *acl_user= acl_users->begin();
         acl_user != acl_users->end(); ++acl_user)
    {
      DBUG_PRINT("info",("strcmp('%s','%s'), compare_hostname('%s','%s'),",
                         user, acl_user->user ? acl_user->user : "",
                         host,
                         acl_user->host.get_host() ? acl_user->host.get_host() :
                         ""));
      if ((!acl_user->user && !user[0]) ||
          (acl_user->user && !strcmp(user,acl_user->user)))
      {
        if (exact ? !my_strcasecmp(system_charset_info, host,
                                   acl_user->host.get_host() ?
                                   acl_user->host.get_host() : "") :
            acl_user->host.compare_hostname(host,host))
        {
          DBUG_RETURN(acl_user);
        }
      }
    }
  }
  DBUG_RETURN(0);
}


/*
  Find user in ACL

  SYNOPSIS
    is_acl_user()
    host                 host name
    user                 user name

  RETURN
   FALSE  user not fond
   TRUE   there are such user
*/

bool is_acl_user(const char *host, const char *user)
{
  bool res;

  /* --skip-grants */
  if (!initialized)
    return TRUE;

  mysql_mutex_lock(&acl_cache->lock);
  res= find_acl_user(host, user, TRUE) != NULL;
  mysql_mutex_unlock(&acl_cache->lock);
  return res;
}


/**
  Validate if a user can proxy as another user

  @thd                     current thread
  @param user              the logged in user (proxy user)
  @param authenticated_as  the effective user a plugin is trying to 
                           impersonate as (proxied user)
  @return                  proxy user definition
    @retval NULL           proxy user definition not found or not applicable
    @retval non-null       the proxy user data
*/

ACL_PROXY_USER *
acl_find_proxy_user(const char *user, const char *host, const char *ip,
                    char *authenticated_as, bool *proxy_used)
{
  /* if the proxied and proxy user are the same return OK */
  DBUG_ENTER("acl_find_proxy_user");
  DBUG_PRINT("info", ("user=%s host=%s ip=%s authenticated_as=%s",
                      user, host, ip, authenticated_as));

  if (!strcmp(authenticated_as, user))
  {
    DBUG_PRINT ("info", ("user is the same as authenticated_as"));
    DBUG_RETURN (NULL);
  }

  bool find_any = check_proxy_users && !*authenticated_as;

  if(!find_any)
    *proxy_used= TRUE; 
  for (ACL_PROXY_USER *proxy= acl_proxy_users->begin();
       proxy != acl_proxy_users->end(); ++proxy)
  {
	if (proxy->matches(host, user, ip, authenticated_as, find_any))
	{
      DBUG_PRINT("info", ("proxy matched=%s@%s",
		proxy->get_proxied_user(),
		proxy->get_proxied_host()));
      if (!find_any)
	  {
        DBUG_PRINT("info", ("returning specific match as authenticated_as was specified"));
        *proxy_used = TRUE;
        DBUG_RETURN(proxy);
      }
      else
      {
        // we never use anonymous users when mapping
        // proxy users for internal plugins:
        if (strcmp(proxy->get_proxied_user() ?
          proxy->get_proxied_user() : "", ""))
        {
          if (find_acl_user(
            proxy->get_proxied_host(),
            proxy->get_proxied_user(),
            TRUE))
          {
            DBUG_PRINT("info", ("setting proxy_used to true, as \
              find_all search matched real user=%s host=%s",
              proxy->get_proxied_user(),
              proxy->get_proxied_host()));
            *proxy_used = TRUE;
            strcpy(authenticated_as, proxy->get_proxied_user());
          }
          else
          {
            DBUG_PRINT("info", ("skipping match because ACL user \
              does not exist, looking for next match to map"));
          }
          if (*proxy_used)
          {
            DBUG_PRINT("info", ("returning matching user"));
            DBUG_RETURN(proxy);
          }
        }
      }
	}
  }
  DBUG_PRINT("info", ("No matching users found, returning null"));
  DBUG_RETURN(NULL);
}


static uchar* acl_entry_get_key(acl_entry *entry, size_t *length,
                                my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length=(uint) entry->length;
  return (uchar*) entry->key;
}


static uchar* check_get_key(ACL_USER *buff, size_t *length,
                            my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length=buff->host.get_host_len();
  return (uchar*) buff->host.get_host();
}


/*
  Get privilege for a host, user and db combination

  as db_is_pattern changes the semantics of comparison,
  acl_cache is not used if db_is_pattern is set.
*/

ulong acl_get(const char *host, const char *ip,
              const char *user, const char *db, my_bool db_is_pattern)
{
  ulong host_access= ~(ulong)0, db_access= 0;
  size_t key_length, copy_length;
  char key[ACL_KEY_LENGTH],*tmp_db,*end;
  acl_entry *entry;
  DBUG_ENTER("acl_get");

  copy_length= (strlen(ip ? ip : "") +
                strlen(user ? user : "") +
                strlen(db ? db : "")) + 2; /* Added 2 at the end to avoid
                                              buffer overflow at strmov()*/
  /*
    Make sure that my_stpcpy() operations do not result in buffer overflow.
  */
  if (copy_length >= ACL_KEY_LENGTH)
    DBUG_RETURN(0);

  mysql_mutex_lock(&acl_cache->lock);
  end=my_stpcpy((tmp_db=my_stpcpy(my_stpcpy(key, ip ? ip : "")+1,user)+1),db);
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, tmp_db);
    db=tmp_db;
  }
  key_length= (size_t) (end-key);
  if (!db_is_pattern && (entry=(acl_entry*) acl_cache->search((uchar*) key,
                                                              key_length)))
  {
    db_access=entry->access;
    mysql_mutex_unlock(&acl_cache->lock);
    DBUG_PRINT("exit", ("access: 0x%lx", db_access));
    DBUG_RETURN(db_access);
  }

  /*
    Check if there are some access rights for database and user
  */
  for (ACL_DB *acl_db= acl_dbs->begin(); acl_db != acl_dbs->end(); ++acl_db)
  {
    if (!acl_db->user || !strcmp(user,acl_db->user))
    {
      if (acl_db->host.compare_hostname(host,ip))
      {
        if (!acl_db->db || !wild_compare(db,acl_db->db,db_is_pattern))
        {
          db_access=acl_db->access;
          if (acl_db->host.get_host())
            goto exit;                          // Fully specified. Take it
          break; /* purecov: tested */
        }
      }
    }
  }
  if (!db_access)
    goto exit;                                  // Can't be better

exit:
  /* Save entry in cache for quick retrieval */
  if (!db_is_pattern &&
      (entry= (acl_entry*) my_malloc(key_memory_acl_cache,
                                     sizeof(acl_entry)+key_length,
                                     MYF(0))))
  {
    entry->access=(db_access & host_access);
    entry->length=key_length;
    memcpy((uchar*) entry->key,key,key_length);
    acl_cache->add(entry);
  }
  mysql_mutex_unlock(&acl_cache->lock);
  DBUG_PRINT("exit", ("access: 0x%lx", db_access & host_access));
  DBUG_RETURN(db_access & host_access);
}


/**
  Check if the user is allowed to change password

 @param thd THD
 @param host Hostname for the user
 @param user User name
 @param new_password new password

 new_password cannot be NULL

 @return Error status
   @retval 0 OK
   @retval 1 ERROR; In this case the error is sent to the client.
*/

/*
  Check if there are any possible matching entries for this host

  NOTES
    All host names without wild cards are stored in a hash table,
    entries with wildcards are stored in a dynamic array
*/

static void init_check_host(void)
{
  DBUG_ENTER("init_check_host");
  if (acl_wild_hosts != NULL)
    acl_wild_hosts->clear();
  else
    acl_wild_hosts=
      new Prealloced_array<ACL_HOST_AND_IP, ACL_PREALLOC_SIZE>(key_memory_acl_mem);

  size_t acl_users_size= acl_users ? acl_users->size() : 0;

  (void) my_hash_init(&acl_check_hosts,system_charset_info,
                      acl_users_size, 0, 0,
                      (my_hash_get_key) check_get_key, 0, 0,
                      key_memory_acl_mem);
  if (acl_users_size && !allow_all_hosts)
  {
    for (ACL_USER *acl_user= acl_users->begin();
         acl_user != acl_users->end(); ++acl_user)
    {
      if (acl_user->host.has_wildcard())
      {                                         // Has wildcard
        ACL_HOST_AND_IP *acl= NULL;
        for (acl= acl_wild_hosts->begin(); acl != acl_wild_hosts->end(); ++acl)
        {                                       // Check if host already exists
          if (!my_strcasecmp(system_charset_info,
                             acl_user->host.get_host(), acl->get_host()))
            break;                              // already stored
        }
        if (acl == acl_wild_hosts->end())       // If new
          acl_wild_hosts->push_back(acl_user->host);
      }
      else if (!my_hash_search(&acl_check_hosts,(uchar*)
                               acl_user->host.get_host(),
                               strlen(acl_user->host.get_host())))
      {
        if (my_hash_insert(&acl_check_hosts,(uchar*) acl_user))
        {                                       // End of memory
          allow_all_hosts=1;                    // Should never happen
          DBUG_VOID_RETURN;
        }
      }
    }
  }
  acl_wild_hosts->shrink_to_fit();
  freeze_size(&acl_check_hosts.array);
  DBUG_VOID_RETURN;
}


/*
  Rebuild lists used for checking of allowed hosts

  We need to rebuild 'acl_check_hosts' and 'acl_wild_hosts' after adding,
  dropping or renaming user, since they contain pointers to elements of
  'acl_user' array, which are invalidated by drop operation, and use
  ACL_USER::host::hostname as a key, which is changed by rename.
*/
void rebuild_check_host(void)
{
  delete acl_wild_hosts;
  acl_wild_hosts= NULL;
  my_hash_free(&acl_check_hosts);
  init_check_host();
}


/*
  Gets user credentials without authentication and resource limit checks.

  SYNOPSIS
    acl_getroot()
      sctx               Context which should be initialized
      user               user name
      host               host name
      ip                 IP
      db                 current data base name

  RETURN
    FALSE  OK
    TRUE   Error
*/

bool acl_getroot(Security_context *sctx, char *user, char *host,
                 char *ip, const char *db)
{
  int res= 1;
  ACL_USER *acl_user= 0;
  DBUG_ENTER("acl_getroot");

  DBUG_PRINT("enter", ("Host: '%s', Ip: '%s', User: '%s', db: '%s'",
                       (host ? host : "(NULL)"), (ip ? ip : "(NULL)"),
                       user, (db ? db : "(NULL)")));
  sctx->set_user_ptr(user, user ? strlen(user) : 0);
  sctx->set_host_ptr(host, host ? strlen(host) : 0);
  sctx->set_ip_ptr(ip, ip? strlen(ip) : 0);
  sctx->set_host_or_ip_ptr();

  if (!initialized)
  {
    /*
      here if mysqld's been started with --skip-grant-tables option.
    */
    sctx->skip_grants();
    DBUG_RETURN(FALSE);
  }

  mysql_mutex_lock(&acl_cache->lock);

  sctx->set_master_access(0);
  sctx->set_db_access(0);
  sctx->assign_priv_user("", 0);
  sctx->assign_priv_host("", 0);

  /*
     Find acl entry in user database.
     This is specially tailored to suit the check we do for CALL of
     a stored procedure; user is set to what is actually a
     priv_user, which can be ''.
  */
  for (ACL_USER *acl_user_tmp= acl_users->begin();
       acl_user_tmp != acl_users->end(); ++acl_user_tmp)
  {
    if ((!acl_user_tmp->user && !user[0]) ||
        (acl_user_tmp->user && strcmp(user, acl_user_tmp->user) == 0))
    {
      if (acl_user_tmp->host.compare_hostname(host, ip))
      {
        acl_user= acl_user_tmp;
        res= 0;
        break;
      }
    }
  }

  if (acl_user)
  {
    for (ACL_DB *acl_db= acl_dbs->begin(); acl_db != acl_dbs->end(); ++acl_db)
    {
      if (!acl_db->user ||
          (user && user[0] && !strcmp(user, acl_db->user)))
      {
        if (acl_db->host.compare_hostname(host, ip))
        {
          if (!acl_db->db || (db && !wild_compare(db, acl_db->db, 0)))
          {
            sctx->set_db_access(acl_db->access);
            break;
          }
        }
      }
    }
    sctx->set_master_access(acl_user->access);
    sctx->assign_priv_user(user, user ? strlen(user) : 0);

    sctx->assign_priv_host(acl_user->host.get_host(),
                           acl_user->host.get_host() ?
                           strlen(acl_user->host.get_host()) : 0);

    sctx->set_password_expired(acl_user->password_expired);
  }
  mysql_mutex_unlock(&acl_cache->lock);
  DBUG_RETURN(res);
}


namespace {

class ACL_compare :
  public std::binary_function<ACL_ACCESS, ACL_ACCESS, bool>
{
public:
  bool operator()(const ACL_ACCESS &a, const ACL_ACCESS &b)
  {
    return a.sort > b.sort;
  }
};

} // namespace


/**
  Convert scrambled password to binary form, according to scramble type, 
  Binary form is stored in user.salt.
  
  @param acl_user The object where to store the salt
   
  Despite the name of the function it is used when loading ACLs from disk
  to store the password hash in the ACL_USER object.
  Note that it works only for native and "old" mysql authentication built-in
  plugins.
  
  Assumption : user's authentication plugin information is available.

  @return Password hash validation
    @retval false Hash is of suitable length
    @retval true Hash is of wrong length or format
*/

bool set_user_salt(ACL_USER *acl_user)
{
  bool result= false;
  plugin_ref plugin= NULL;

  plugin= my_plugin_lock_by_name(0, acl_user->plugin,
                                 MYSQL_AUTHENTICATION_PLUGIN);
  if (plugin)
  {
    st_mysql_auth *auth= (st_mysql_auth *) plugin_decl(plugin)->info;
    result=  auth->set_salt(acl_user->auth_string.str,
                            acl_user->auth_string.length,
                            acl_user->salt,
                            &acl_user->salt_len);
    plugin_unlock(0, plugin);
  }
  return result;
}

/**
  Iterate over the user records and check for irregularities.
  Currently this includes :
   - checking if the plugin referenced is present.
   - if there's sha256 users and there's neither SSL nor RSA configured
*/
static void
validate_user_plugin_records()
{
  DBUG_ENTER("validate_user_plugin_records");
  if (!validate_user_plugins)
    DBUG_VOID_RETURN;

  lock_plugin_data();
  for (ACL_USER *acl_user= acl_users->begin();
       acl_user != acl_users->end(); ++acl_user)
  {
    struct st_plugin_int *plugin;

    if (acl_user->plugin.length)
    {
      /* rule 1 : plugin does exit */
      if (!auth_plugin_is_built_in(acl_user->plugin.str))
      {
        plugin= plugin_find_by_type(acl_user->plugin,
                                    MYSQL_AUTHENTICATION_PLUGIN);

        if (!plugin)
        {
          sql_print_warning("The plugin '%.*s' used to authenticate "
                            "user '%s'@'%.*s' is not loaded."
                            " Nobody can currently login using this account.",
                            (int) acl_user->plugin.length, acl_user->plugin.str,
                            acl_user->user,
                            static_cast<int>(acl_user->host.get_host_len()),
                            acl_user->host.get_host());
        }
      }
      if (acl_user->plugin.str == sha256_password_plugin_name.str &&
          rsa_auth_status() && !ssl_acceptor_fd)
      {
          sql_print_warning("The plugin '%s' is used to authenticate "
                            "user '%s'@'%.*s', "
#if !defined(HAVE_YASSL)
                            "but neither SSL nor RSA keys are "
#else
                            "but no SSL is "
#endif
                            "configured. "
                            "Nobody can currently login using this account.",
                            sha256_password_plugin_name.str,
                            acl_user->user,
                            static_cast<int>(acl_user->host.get_host_len()),
                            acl_user->host.get_host());
      }
    }
  }
  unlock_plugin_data();
  DBUG_VOID_RETURN;
}


/*
  Initialize structures responsible for user/db-level privilege checking and
  load privilege information for them from tables in the 'mysql' database.

  SYNOPSIS
    acl_init()
      dont_read_acl_tables  TRUE if we want to skip loading data from
                            privilege tables and disable privilege checking.

  NOTES
    This function is mostly responsible for preparatory steps, main work
    on initialization and grants loading is done in acl_reload().

  RETURN VALUES
    0   ok
    1   Could not initialize grant's
*/

my_bool acl_init(bool dont_read_acl_tables)
{
  THD  *thd;
  my_bool return_val;
  DBUG_ENTER("acl_init");

  acl_cache= new hash_filo(key_memory_acl_cache,
                           ACL_CACHE_SIZE, 0, 0,
                           (my_hash_get_key) acl_entry_get_key,
                           (my_hash_free_key) my_free,
                           &my_charset_utf8_bin);

  LOCK_grant.init(LOCK_GRANT_PARTITIONS
#ifdef HAVE_PSI_INTERFACE
                  , key_rwlock_LOCK_grant
#endif
                  );
  rwlocks_initialized= true;

  /*
    cache built-in native authentication plugins,
    to avoid hash searches and a global mutex lock on every connect
  */
  native_password_plugin= my_plugin_lock_by_name(0,
           native_password_plugin_name, MYSQL_AUTHENTICATION_PLUGIN);
  if (!native_password_plugin)
    DBUG_RETURN(1);

  if (dont_read_acl_tables)
  {
    DBUG_RETURN(0); /* purecov: tested */
  }

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd=new THD))
    DBUG_RETURN(1); /* purecov: inspected */
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  /*
    It is safe to call acl_reload() since acl_* arrays and hashes which
    will be freed there are global static objects and thus are initialized
    by zeros at startup.
  */
  return_val= acl_reload(thd);

  thd->release_resources();
  delete thd;

  DBUG_RETURN(return_val);
}


/*
  Initialize structures responsible for user/db-level privilege checking
  and load information about grants from open privilege tables.

  SYNOPSIS
    acl_load()
      thd     Current thread
      tables  List containing open "mysql.host", "mysql.user",
              "mysql.db" and "mysql.proxies_priv" tables in that order.

  RETURN VALUES
    FALSE  Success
    TRUE   Error
*/

static my_bool acl_load(THD *thd, TABLE_LIST *tables)
{
  TABLE *table;
  READ_RECORD read_record_info;
  my_bool return_val= TRUE;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  char tmp_name[NAME_LEN+1];
  sql_mode_t old_sql_mode= thd->variables.sql_mode;
  bool password_expired= false;
  bool super_users_with_empty_plugin= false;
  Acl_load_user_table_schema_factory user_table_schema_factory;
  Acl_load_user_table_schema *table_schema = NULL;
  bool is_old_db_layout= false;
  DBUG_ENTER("acl_load");

  DBUG_EXECUTE_IF("wl_9262_set_max_length_hostname",
                    thd->security_context()->assign_priv_host(
                      "oh_my_gosh_this_is_a_long_"
                      "hostname_look_at_it_it_has_60"
                      "_char", 60);
                    thd->security_context()->assign_host(
                      "oh_my_gosh_this_is_a_long_"
                      "hostname_look_at_it_it_has_60"
                      "_char", 60);
                    thd->security_context()->set_host_or_ip_ptr();
                    );

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  grant_version++; /* Privileges updated */

  
  acl_cache->clear(1);                          // Clear locked hostname cache

  init_sql_alloc(key_memory_acl_mem,
                 &global_acl_memory, ACL_ALLOC_BLOCK_SIZE, 0);
  /*
    Prepare reading from the mysql.user table
  */
  if (init_read_record(&read_record_info, thd, table=tables[0].table,
                       NULL, 1, 1, FALSE))
    goto end;
  table->use_all_columns();
  acl_users->clear();
  /*
   We need to check whether we are working with old database layout. This
   might be the case for instance when we are running mysql_upgrade.
  */
  if (user_table_schema_factory.user_table_schema_check(table))
  {
    table_schema= user_table_schema_factory.get_user_table_schema(table);
    is_old_db_layout= user_table_schema_factory.is_old_user_table_schema(table);
  }
  else
  {
    sql_print_error("[FATAL] mysql.user table is damaged. "
                    "Please run mysql_upgrade.");
    end_read_record(&read_record_info);
    goto end;
  }

  allow_all_hosts=0;
  int read_rec_errcode;
  while (!(read_rec_errcode= read_record_info.read_record(&read_record_info)))
  {
    password_expired= false;
    /* Reading record from mysql.user */
    ACL_USER user;
    memset(&user, 0, sizeof(user));

    /*
      All accounts can authenticate per default. This will change when
      we add a new field to the user table.

      Currently this flag is only set to false when authentication is attempted
      using an unknown user name.
    */
    user.can_authenticate= true;

    /*
      Account is unlocked by default.
    */
    user.account_locked= false;

    user.host.update_hostname(get_field(&global_acl_memory,
                                      table->field[table_schema->host_idx()]));
    user.user= get_field(&global_acl_memory,
                         table->field[table_schema->user_idx()]);
    if (check_no_resolve && hostname_requires_resolving(user.host.get_host()))
    {
      sql_print_warning("'user' entry '%s@%s' "
                        "ignored in --skip-name-resolve mode.",
                        user.user ? user.user : "",
                        user.host.get_host() ? user.host.get_host() : "");
    }

    /* Read password from authentication_string field */
    if (table->s->fields > table_schema->authentication_string_idx())
      user.auth_string.str=
        get_field(&global_acl_memory,
                  table->field[table_schema->authentication_string_idx()]);
    else
    {
      sql_print_error("Fatal error: mysql.user table is damaged. "
                      "Please run mysql_upgrade.");

      end_read_record(&read_record_info);
      goto end;
    }
    if(user.auth_string.str)
      user.auth_string.length= strlen(user.auth_string.str);
    else
      user.auth_string= EMPTY_STR;

    {
      uint next_field;
      user.access= get_access(table, table_schema->select_priv_idx(),
                              &next_field) & GLOBAL_ACLS;
      /*
        if it is pre 5.0.1 privilege table then map CREATE privilege on
        CREATE VIEW & SHOW VIEW privileges
      */
      if (table->s->fields <= 31 && (user.access & CREATE_ACL))
        user.access|= (CREATE_VIEW_ACL | SHOW_VIEW_ACL);

      /*
        if it is pre 5.0.2 privilege table then map CREATE/ALTER privilege on
        CREATE PROCEDURE & ALTER PROCEDURE privileges
      */
      if (table->s->fields <= 33 && (user.access & CREATE_ACL))
        user.access|= CREATE_PROC_ACL;
      if (table->s->fields <= 33 && (user.access & ALTER_ACL))
        user.access|= ALTER_PROC_ACL;

      /*
        pre 5.0.3 did not have CREATE_USER_ACL
      */
      if (table->s->fields <= 36 && (user.access & GRANT_ACL))
        user.access|= CREATE_USER_ACL;


      /*
        if it is pre 5.1.6 privilege table then map CREATE privilege on
        CREATE|ALTER|DROP|EXECUTE EVENT
      */
      if (table->s->fields <= 37 && (user.access & SUPER_ACL))
        user.access|= EVENT_ACL;

      /*
        if it is pre 5.1.6 privilege then map TRIGGER privilege on CREATE.
      */
      if (table->s->fields <= 38 && (user.access & SUPER_ACL))
        user.access|= TRIGGER_ACL;

      user.sort= get_sort(2,user.host.get_host(),user.user);

      /* Starting from 4.0.2 we have more fields */
      if (table->s->fields >= 31)
      {
        char *ssl_type=
          get_field(thd->mem_root, table->field[table_schema->ssl_type_idx()]);
        if (!ssl_type)
          user.ssl_type=SSL_TYPE_NONE;
        else if (!strcmp(ssl_type, "ANY"))
          user.ssl_type=SSL_TYPE_ANY;
        else if (!strcmp(ssl_type, "X509"))
          user.ssl_type=SSL_TYPE_X509;
        else  /* !strcmp(ssl_type, "SPECIFIED") */
          user.ssl_type=SSL_TYPE_SPECIFIED;

        user.ssl_cipher= 
          get_field(&global_acl_memory,
                    table->field[table_schema->ssl_cipher_idx()]);
        user.x509_issuer=
          get_field(&global_acl_memory,
                    table->field[table_schema->x509_issuer_idx()]);
        user.x509_subject=
          get_field(&global_acl_memory,
                    table->field[table_schema->x509_subject_idx()]);

        char *ptr= get_field(thd->mem_root,
                             table->field[table_schema->max_questions_idx()]);
        user.user_resource.questions=ptr ? atoi(ptr) : 0;
        ptr= get_field(thd->mem_root,
                       table->field[table_schema->max_updates_idx()]);
        user.user_resource.updates=ptr ? atoi(ptr) : 0;
        ptr= get_field(thd->mem_root,
                       table->field[table_schema->max_connections_idx()]);
        user.user_resource.conn_per_hour= ptr ? atoi(ptr) : 0;
        if (user.user_resource.questions || user.user_resource.updates ||
            user.user_resource.conn_per_hour)
          mqh_used=1;

        if (table->s->fields > table_schema->max_user_connections_idx())
        {
          /* Starting from 5.0.3 we have max_user_connections field */
          ptr= get_field(thd->mem_root,
                         table->field[table_schema->max_user_connections_idx()]);
          user.user_resource.user_conn= ptr ? atoi(ptr) : 0;
        }

        if (table->s->fields >= 41)
        {
          /* We may have plugin & auth_String fields */
          const char *tmpstr=
            get_field(&global_acl_memory,
                      table->field[table_schema->plugin_idx()]);

          /* In case we are working with 5.6 db layout we need to make server
             aware of Password field and that the plugin column can be null.
             In case when plugin column is null we use native password plugin
             if we can.
          */
          if (is_old_db_layout && (tmpstr == NULL || strlen(tmpstr) == 0 ||
              my_strcasecmp(system_charset_info, tmpstr,
                            native_password_plugin_name.str) == 0))
          {
            char *password= get_field(&global_acl_memory,
                                    table->field[table_schema->password_idx()]);

            //We only support native hash, we do not support pre 4.1 hashes
            plugin_ref native_plugin= NULL;
            native_plugin= my_plugin_lock_by_name(0,
                                       native_password_plugin_name,
                                       MYSQL_AUTHENTICATION_PLUGIN);
            if (native_plugin)
            {
              uint password_len= password ? strlen(password) : 0;
              st_mysql_auth *auth=
                (st_mysql_auth *) plugin_decl(native_plugin)->info;
              if (auth->validate_authentication_string(password,
                                                       password_len) == 0)
              {
                //auth_string takes precedence over password
                if (user.auth_string.length == 0)
                {
                  user.auth_string.str= password;
                  user.auth_string.length= password_len;
                }
                if (tmpstr == NULL || strlen(tmpstr) == 0)
                  tmpstr= native_password_plugin_name.str;
              }
              else
              {
                if ((user.access & SUPER_ACL) && !super_users_with_empty_plugin
                    && (tmpstr == NULL || strlen(tmpstr) == 0))
                  super_users_with_empty_plugin= true;

                sql_print_warning("User entry '%s'@'%s' has a deprecated "
                "pre-4.1 password. The user will be ignored and no one can "
                "login with this user anymore.",
                user.user ? user.user : "",
                user.host.get_host() ? user.host.get_host() : "");
                plugin_unlock(0, native_plugin);
                continue;
              }
              plugin_unlock(0, native_plugin);
            }
          }

          /*
            Check if the plugin string is blank or null.
            If it is, the user will be skipped.
          */
          if(tmpstr == NULL || strlen(tmpstr) == 0)
          {
            if ((user.access & SUPER_ACL) && !super_users_with_empty_plugin)
                      super_users_with_empty_plugin= true;
            sql_print_warning("User entry '%s'@'%s' has an empty plugin "
      			"value. The user will be ignored and no one can login "
      			"with this user anymore.",
      			user.user ? user.user : "",
      			user.host.get_host() ? user.host.get_host() : "");
            continue;
          }
          /*
            By comparing the plugin with the built in plugins it is possible
            to optimize the string allocation and comparision.
          */
          if (my_strcasecmp(system_charset_info, tmpstr,
                            native_password_plugin_name.str) == 0)
            user.plugin= native_password_plugin_name;
#if defined(HAVE_OPENSSL)
          else
            if (my_strcasecmp(system_charset_info, tmpstr,
                              sha256_password_plugin_name.str) == 0)
              user.plugin= sha256_password_plugin_name;
#endif
          else
            {
              user.plugin.str= tmpstr;
              user.plugin.length= strlen(tmpstr);
            }
        }

        /* Validate the hash string. */
        plugin_ref plugin= NULL;
        plugin= my_plugin_lock_by_name(0, user.plugin,
                                       MYSQL_AUTHENTICATION_PLUGIN);
        if (plugin)
        {
          st_mysql_auth *auth= (st_mysql_auth *) plugin_decl(plugin)->info;
          if (auth->validate_authentication_string(user.auth_string.str,
                                                   user.auth_string.length))
          {
            sql_print_warning("Found invalid password for user: '%s@%s'; "
                              "Ignoring user", user.user ? user.user : "",
                              user.host.get_host() ? user.host.get_host() : "");
            plugin_unlock(0, plugin);
            continue;
          }
          plugin_unlock(0, plugin);
        }

        if (table->s->fields > table_schema->password_expired_idx())
        {
          char *tmpstr= get_field(&global_acl_memory,
                           table->field[table_schema->password_expired_idx()]);
          if (tmpstr && (*tmpstr == 'Y' || *tmpstr == 'y'))
          {
            user.password_expired= true;

            if (!auth_plugin_supports_expiration(user.plugin.str))
            {
              sql_print_warning("'user' entry '%s@%s' has the password ignore "
                                "flag raised, but its authentication plugin "
                                "doesn't support password expiration. "
                                "The user id will be ignored.",
                                user.user ? user.user : "",
                                user.host.get_host() ? user.host.get_host() : "");
              continue;
            }
            password_expired= true;
          }
        }

        if (table->s->fields > table_schema->account_locked_idx())
        {
          char *locked = get_field(&global_acl_memory,
                             table->field[table_schema->account_locked_idx()]);

          if (locked && (*locked == 'Y' || *locked == 'y'))
          {
            user.account_locked= true;
          }
        }

	/*
	   Initalize the values of timestamp and expire after day
	   to error and true respectively.
	*/
	user.password_last_changed.time_type= MYSQL_TIMESTAMP_ERROR;
	user.use_default_password_lifetime= true;
	user.password_lifetime= 0;

	if (table->s->fields > table_schema->password_last_changed_idx())
        {
	  if (!table->field[table_schema->password_last_changed_idx()]->is_null())
	  {
            char *password_last_changed= get_field(&global_acl_memory,
	          table->field[table_schema->password_last_changed_idx()]);

	    if (password_last_changed &&
	        memcmp(password_last_changed, INVALID_DATE, sizeof(INVALID_DATE)))
	    {
	      String str(password_last_changed, &my_charset_bin);
              str_to_time_with_warn(&str,&(user.password_last_changed));
	    }
	  }
	}

        if (table->s->fields > table_schema->password_lifetime_idx())
        {
          if (!table->
              field[table_schema->password_lifetime_idx()]->is_null())
	  {
	    char *ptr= get_field(&global_acl_memory,
		table->field[table_schema->password_lifetime_idx()]);
	    user.password_lifetime= ptr ? atoi(ptr) : 0;
	    user.use_default_password_lifetime= false;
	  }
	}

      } // end if (table->s->fields >= 31)
      else
      {
        user.ssl_type=SSL_TYPE_NONE;
        if (table->s->fields <= 13)
        {                                               // Without grant
          if (user.access & CREATE_ACL)
            user.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL;
        }
        /* Convert old privileges */
        user.access|= LOCK_TABLES_ACL | CREATE_TMP_ACL | SHOW_DB_ACL;
        if (user.access & FILE_ACL)
          user.access|= REPL_CLIENT_ACL | REPL_SLAVE_ACL;
        if (user.access & PROCESS_ACL)
          user.access|= SUPER_ACL | EXECUTE_ACL;
      }

      set_user_salt(&user);
      user.password_expired= password_expired;

      acl_users->push_back(user);
      if (user.host.check_allow_all_hosts())
        allow_all_hosts=1;                      // Anyone can connect
    }
  } // END while reading records from the mysql.user table

  end_read_record(&read_record_info);
  if (read_rec_errcode > 0)
    goto end;

  std::sort(acl_users->begin(), acl_users->end(), ACL_compare());
  acl_users->shrink_to_fit();

  if (super_users_with_empty_plugin)
  {
    sql_print_warning("Some of the user accounts with SUPER privileges were "
                      "disabled because of empty mysql.user.plugin value. "
                      "If you are upgrading from MySQL 5.6 to MySQL 5.7 it "
                      "means we were not able to substitute for empty plugin "
                      "column. Probably because of pre 4.1 password hash. "
                      "If your account is disabled you will need to:");
    sql_print_warning("1. Stop the server and restart it with "
                      "--skip-grant-tables.");
    sql_print_warning("2. Run mysql_upgrade.");
    sql_print_warning("3. Restart the server with the parameters you "
                      "normally use.");
    sql_print_warning("For complete instructions on how to upgrade MySQL "
                      "to a new version please see the 'Upgrading MySQL' "
                      "section from the MySQL manual");
  }

  /*
    Prepare reading from the mysql.db table
  */
  if (init_read_record(&read_record_info, thd, table=tables[1].table,
                       NULL, 1, 1, FALSE))
    goto end;
  table->use_all_columns();
  acl_dbs->clear();

  while (!(read_rec_errcode= read_record_info.read_record(&read_record_info)))
  {
    /* Reading record in mysql.db */
    ACL_DB db;
    db.host.update_hostname(get_field(&global_acl_memory, 
                            table->field[MYSQL_DB_FIELD_HOST]));
    db.db=get_field(&global_acl_memory, table->field[MYSQL_DB_FIELD_DB]);
    if (!db.db)
    {
      sql_print_warning("Found an entry in the 'db' table with empty database name; Skipped");
      continue;
    }
    db.user=get_field(&global_acl_memory, table->field[MYSQL_DB_FIELD_USER]);
    if (check_no_resolve && hostname_requires_resolving(db.host.get_host()))
    {
      sql_print_warning("'db' entry '%s %s@%s' "
                        "ignored in --skip-name-resolve mode.",
                        db.db,
                        db.user ? db.user : "",
                        db.host.get_host() ? db.host.get_host() : "");
    }
    db.access=get_access(table,3,0);
    db.access=fix_rights_for_db(db.access);
    if (lower_case_table_names)
    {
      /*
        convert db to lower case and give a warning if the db wasn't
        already in lower case
      */
      (void)my_stpcpy(tmp_name, db.db);
      my_casedn_str(files_charset_info, db.db);
      if (strcmp(db.db, tmp_name) != 0)
      {
        sql_print_warning("'db' entry '%s %s@%s' had database in mixed "
                          "case that has been forced to lowercase because "
                          "lower_case_table_names is set. It will not be "
                          "possible to remove this privilege using REVOKE.",
                          db.db,
                          db.user ? db.user : "",
                          db.host.get_host() ? db.host.get_host() : "");
      }
    }
    db.sort=get_sort(3,db.host.get_host(),db.db,db.user);
    if (table->s->fields <=  9)
    {                                           // Without grant
      if (db.access & CREATE_ACL)
        db.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL;
    }
    acl_dbs->push_back(db);
  } // END reading records from mysql.db tables

  end_read_record(&read_record_info);
  if (read_rec_errcode > 0)
    goto end;

  std::sort(acl_dbs->begin(), acl_dbs->end(), ACL_compare());
  acl_dbs->shrink_to_fit();

  /* Prepare to read records from the mysql.proxies_priv table */
  acl_proxy_users->clear();

  if (tables[2].table)
  {
    if (init_read_record(&read_record_info, thd, table= tables[2].table,
                         NULL, 1, 1, FALSE))
      goto end;
    table->use_all_columns();
    while (!(read_rec_errcode= read_record_info.read_record(&read_record_info)))
    {
      /* Reading record in mysql.proxies_priv */
      ACL_PROXY_USER proxy;
      proxy.init(table, &global_acl_memory);
      if (proxy.check_validity(check_no_resolve))
        continue;
      if (acl_proxy_users->push_back(proxy))
      {
        end_read_record(&read_record_info);
        goto end;
      }
    } // END reading records from the mysql.proxies_priv table

    end_read_record(&read_record_info);
    if (read_rec_errcode > 0)
      goto end;

    std::sort(acl_proxy_users->begin(), acl_proxy_users->end(), ACL_compare());
  }
  else
  {
    sql_print_error("Missing system table mysql.proxies_priv; "
                    "please run mysql_upgrade to create it");
  }
  acl_proxy_users->shrink_to_fit();
  validate_user_plugin_records();
  init_check_host();

  initialized=1;
  return_val= FALSE;

end:
  thd->variables.sql_mode= old_sql_mode;
  if (table_schema)
    delete table_schema;
  DBUG_RETURN(return_val);
}




void acl_free(bool end)
{
  free_root(&global_acl_memory,MYF(0));
  delete acl_users;
  acl_users= NULL;
  delete acl_dbs;
  acl_dbs= NULL;
  delete acl_wild_hosts;
  acl_wild_hosts= NULL;
  delete acl_proxy_users;
  acl_proxy_users= NULL;
  my_hash_free(&acl_check_hosts);
  if (!end)
    acl_cache->clear(1); /* purecov: inspected */
  else
  {
    plugin_unlock(0, native_password_plugin);
    delete acl_cache;
    acl_cache=0;

    if (rwlocks_initialized)
    {
      LOCK_grant.destroy();
      rwlocks_initialized= false;
    }
  }
}


/*
  Forget current user/db-level privileges and read new privileges
  from the privilege tables.

  SYNOPSIS
    acl_reload()
      thd  Current thread

  NOTE
    All tables of calling thread which were open and locked by LOCK TABLES
    statement will be unlocked and closed.
    This function is also used for initialization of structures responsible
    for user/db-level privilege checking.

  RETURN VALUE
    FALSE  Success
    TRUE   Failure
*/

my_bool acl_reload(THD *thd)
{
  TABLE_LIST tables[3];

  MEM_ROOT old_mem;
  bool old_initialized;
  my_bool return_val= TRUE;
  DBUG_ENTER("acl_reload");

  /*
    To avoid deadlocks we should obtain table locks before
    obtaining acl_cache->lock mutex.
  */
  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("user"), "user", TL_READ);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("db"), "db", TL_READ);
  tables[2].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("proxies_priv"), 
                           "proxies_priv", TL_READ);
  tables[0].next_local= tables[0].next_global= tables + 1;
  tables[1].next_local= tables[1].next_global= tables + 2;
  tables[0].open_type= tables[1].open_type= tables[2].open_type= OT_BASE_ONLY;
  tables[2].open_strategy= TABLE_LIST::OPEN_IF_EXISTS;

  if (open_and_lock_tables(thd, tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    /*
      Execution might have been interrupted; only print the error message
      if a user error condition has been raised.
    */
    if (thd->get_stmt_da()->is_error())
    {
      sql_print_error("Fatal error: Can't open and lock privilege tables: %s",
                      thd->get_stmt_da()->message_text());
    }
    close_acl_tables(thd);
    DBUG_RETURN(true);
  }

  if ((old_initialized=initialized))
    mysql_mutex_lock(&acl_cache->lock);

  Prealloced_array<ACL_USER, ACL_PREALLOC_SIZE> *old_acl_users= acl_users;
  Prealloced_array<ACL_DB, ACL_PREALLOC_SIZE> *old_acl_dbs= acl_dbs;
  Prealloced_array<ACL_PROXY_USER,
                   ACL_PREALLOC_SIZE> *old_acl_proxy_users = acl_proxy_users;

  acl_users= new Prealloced_array<ACL_USER,
                                  ACL_PREALLOC_SIZE>(key_memory_acl_mem);
  acl_dbs= new Prealloced_array<ACL_DB,
                                ACL_PREALLOC_SIZE>(key_memory_acl_mem);
  acl_proxy_users=
    new Prealloced_array<ACL_PROXY_USER,
                         ACL_PREALLOC_SIZE>(key_memory_acl_mem);  

  old_mem= global_acl_memory;
  delete acl_wild_hosts;
  acl_wild_hosts= NULL;
  my_hash_free(&acl_check_hosts);

  if ((return_val= acl_load(thd, tables)))
  {                                     // Error. Revert to old list
    DBUG_PRINT("error",("Reverting to old privileges"));
    acl_free();                         /* purecov: inspected */
    acl_users= old_acl_users;
    acl_dbs= old_acl_dbs;
    acl_proxy_users= old_acl_proxy_users;

    global_acl_memory= old_mem;
    init_check_host();
  }
  else
  {
    free_root(&old_mem,MYF(0));
    delete old_acl_users;
    delete old_acl_dbs;
    delete old_acl_proxy_users;
  }
  if (old_initialized)
    mysql_mutex_unlock(&acl_cache->lock);

  close_acl_tables(thd);

  DEBUG_SYNC(thd, "after_acl_reload");
  DBUG_RETURN(return_val);
}


void acl_insert_proxy_user(ACL_PROXY_USER *new_value)
{
  DBUG_ENTER("acl_insert_proxy_user");
  mysql_mutex_assert_owner(&acl_cache->lock);
  acl_proxy_users->push_back(*new_value);
  std::sort(acl_proxy_users->begin(), acl_proxy_users->end(), ACL_compare());
  DBUG_VOID_RETURN;
}


void free_grant_table(GRANT_TABLE *grant_table)
{
  my_hash_free(&grant_table->hash_columns);
}


/* Search after a matching grant. Prefer exact grants before not exact ones */

GRANT_NAME *name_hash_search(HASH *name_hash,
                             const char *host,const char* ip,
                             const char *db,
                             const char *user, const char *tname,
                             bool exact, bool name_tolower)
{
  char helping [NAME_LEN*2+USERNAME_LENGTH+3], *name_ptr;
  uint len;
  GRANT_NAME *grant_name,*found=0;
  HASH_SEARCH_STATE state;

  name_ptr= my_stpcpy(my_stpcpy(helping, user) + 1, db) + 1;
  len  = (uint) (my_stpcpy(name_ptr, tname) - helping) + 1;
  if (name_tolower)
    my_casedn_str(files_charset_info, name_ptr);
  for (grant_name= (GRANT_NAME*) my_hash_first(name_hash, (uchar*) helping,
                                               len, &state);
       grant_name ;
       grant_name= (GRANT_NAME*) my_hash_next(name_hash,(uchar*) helping,
                                              len, &state))
  {
    if (exact)
    {
      if (!grant_name->host.get_host() ||
          (host &&
           !my_strcasecmp(system_charset_info, host,
                          grant_name->host.get_host())) ||
          (ip && !strcmp(ip, grant_name->host.get_host())))
        return grant_name;
    }
    else
    {
      if (grant_name->host.compare_hostname(host, ip) &&
          (!found || found->sort < grant_name->sort))
        found=grant_name;                                       // Host ok
    }
  }
  return found;
}


/* Free grant array if possible */

void  grant_free(void)
{
  DBUG_ENTER("grant_free");
  my_hash_free(&column_priv_hash);
  my_hash_free(&proc_priv_hash);
  my_hash_free(&func_priv_hash);
  free_root(&memex,MYF(0));
  DBUG_VOID_RETURN;
}


/**
  @brief Initialize structures responsible for table/column-level privilege
   checking and load information for them from tables in the 'mysql' database.

  @param skip_grant_tables  true if the command line option
    --skip-grant-tables is specified, else false.

  @return Error status
    @retval false OK
    @retval true  Could not initialize grant subsystem.
*/

bool grant_init(bool skip_grant_tables)
{
  THD  *thd;
  my_bool return_val;
  DBUG_ENTER("grant_init");

  if (skip_grant_tables)
    DBUG_RETURN(false);

  if (!(thd= new THD))
    DBUG_RETURN(1);                             /* purecov: deadcode */
  thd->thread_stack= (char*) &thd;
  thd->store_globals();

  return_val=  grant_reload(thd);

  if (return_val && thd->get_stmt_da()->is_error())
    sql_print_error("Fatal: can't initialize grant subsystem - '%s'",
                    thd->get_stmt_da()->message_text());

  thd->release_resources();
  delete thd;

  DBUG_RETURN(return_val);
}


/**
  @brief Helper function to grant_reload

  Reads the procs_priv table into memory hash.

  @param table A pointer to the procs_priv table structure.

  @see grant_reload

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/

static my_bool grant_load_procs_priv(TABLE *p_table)
{
  MEM_ROOT *memex_ptr;
  my_bool return_val= 1;
  int error;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  MEM_ROOT **save_mem_root_ptr= my_thread_get_THR_MALLOC();
  DBUG_ENTER("grant_load_procs_priv");
  (void) my_hash_init(&proc_priv_hash, &my_charset_utf8_bin,
                      0,0,0, (my_hash_get_key) get_grant_table,
                      0, 0, key_memory_acl_memex);
  (void) my_hash_init(&func_priv_hash, &my_charset_utf8_bin,
                      0,0,0, (my_hash_get_key) get_grant_table,
                      0, 0, key_memory_acl_memex);
  error= p_table->file->ha_index_init(0, 1);
  if (error)
  {
    acl_print_ha_error(p_table, error);
    DBUG_RETURN(true);
  }
  p_table->use_all_columns();

  error= p_table->file->ha_index_first(p_table->record[0]);
  DBUG_EXECUTE_IF("se_error_grant_load_procs_read",
                  error= HA_ERR_LOCK_WAIT_TIMEOUT;);
  if (error)
  {
    if (error == HA_ERR_END_OF_FILE)
      return_val= 0; // Return Ok.
    else
      acl_print_ha_error(p_table, error);
  }
  else
  {
    memex_ptr= &memex;
    my_thread_set_THR_MALLOC(&memex_ptr);
    do
    {
      GRANT_NAME *mem_check;
      HASH *hash;
      if (!(mem_check=new (memex_ptr) GRANT_NAME(p_table, TRUE)))
      {
        /* This could only happen if we are out memory */
        goto end_unlock;
      }

      if (check_no_resolve)
      {
        if (hostname_requires_resolving(mem_check->host.get_host()))
        {
          sql_print_warning("'procs_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname, mem_check->user,
                            mem_check->host.get_host() ?
                            mem_check->host.get_host() : "");
        }
      }
      if (p_table->field[4]->val_int() == SP_TYPE_PROCEDURE)
      {
        hash= &proc_priv_hash;
      }
      else
      if (p_table->field[4]->val_int() == SP_TYPE_FUNCTION)
      {
        hash= &func_priv_hash;
      }
      else
      {
        sql_print_warning("'procs_priv' entry '%s' "
                          "ignored, bad routine type",
                          mem_check->tname);
        continue;
      }

      mem_check->privs= fix_rights_for_procedure(mem_check->privs);
      if (! mem_check->ok())
        delete mem_check;
      else if (my_hash_insert(hash, (uchar*) mem_check))
      {
        delete mem_check;
        goto end_unlock;
      }
      error= p_table->file->ha_index_next(p_table->record[0]);
      DBUG_EXECUTE_IF("se_error_grant_load_procs_read_next",
                      error= HA_ERR_LOCK_WAIT_TIMEOUT;);
      if (error)
      {
        if (error == HA_ERR_END_OF_FILE)
          return_val= 0;
        else
          acl_print_ha_error(p_table, error);
        goto end_unlock;
      }
    }
    while (true);
  }

end_unlock:
  p_table->file->ha_index_end();
  my_thread_set_THR_MALLOC(save_mem_root_ptr);
  DBUG_RETURN(return_val);
}


/**
  @brief Initialize structures responsible for table/column-level privilege
    checking and load information about grants from open privilege tables.

  @param thd Current thread
  @param tables List containing open "mysql.tables_priv" and
    "mysql.columns_priv" tables.

  @see grant_reload

  @return Error state
    @retval FALSE Success
    @retval TRUE Error
*/

static my_bool grant_load(THD *thd, TABLE_LIST *tables)
{
  MEM_ROOT *memex_ptr;
  my_bool return_val= 1;
  int error;
  TABLE *t_table= 0, *c_table= 0;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  MEM_ROOT **save_mem_root_ptr= my_thread_get_THR_MALLOC();
  sql_mode_t old_sql_mode= thd->variables.sql_mode;
  DBUG_ENTER("grant_load");

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  (void) my_hash_init(&column_priv_hash, &my_charset_utf8_bin,
                      0,0,0, (my_hash_get_key) get_grant_table,
                      (my_hash_free_key) free_grant_table,0,
                      key_memory_acl_memex);

  t_table = tables[0].table;
  c_table = tables[1].table;
  error= t_table->file->ha_index_init(0, 1);
  if (error)
  {
    acl_print_ha_error(t_table, error);
    goto end_index_init;
  }
  t_table->use_all_columns();
  c_table->use_all_columns();

  error= t_table->file->ha_index_first(t_table->record[0]);
  DBUG_EXECUTE_IF("se_error_grant_load_read",
                  error= HA_ERR_LOCK_WAIT_TIMEOUT;);
  if (error)
  {
    if (error == HA_ERR_END_OF_FILE)
      return_val= 0; // Return Ok.
    else
      acl_print_ha_error(t_table, error);
  }
  else
  {
    memex_ptr= &memex;
    my_thread_set_THR_MALLOC(&memex_ptr);
    do
    {
      GRANT_TABLE *mem_check;
      mem_check= new (memex_ptr) GRANT_TABLE(t_table);

      if (!mem_check)
      {
        /* This could only happen if we are out memory */
        goto end_unlock;
      }

      if (mem_check->init(c_table))
      {
        delete mem_check;
        goto end_unlock;
      }

      if (check_no_resolve)
      {
        if (hostname_requires_resolving(mem_check->host.get_host()))
        {
          sql_print_warning("'tables_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname,
                            mem_check->user ? mem_check->user : "",
                            mem_check->host.get_host() ?
                            mem_check->host.get_host() : "");
        }
      }

      if (! mem_check->ok())
        delete mem_check;
      else if (my_hash_insert(&column_priv_hash,(uchar*) mem_check))
      {
        delete mem_check;
        goto end_unlock;
      }
      error= t_table->file->ha_index_next(t_table->record[0]);
      DBUG_EXECUTE_IF("se_error_grant_load_read_next",
                      error= HA_ERR_LOCK_WAIT_TIMEOUT;);
      if (error)
      {
        if (error != HA_ERR_END_OF_FILE)
          acl_print_ha_error(t_table, error);
        else
          return_val= 0;
        goto end_unlock;
      }

    }
    while (true);
  }

end_unlock:
  t_table->file->ha_index_end();
  my_thread_set_THR_MALLOC(save_mem_root_ptr);
end_index_init:
  thd->variables.sql_mode= old_sql_mode;
  DBUG_RETURN(return_val);
}


/**
  @brief Helper function to grant_reload. Reloads procs_priv table is it
    exists.

  @param thd A pointer to the thread handler object.
  @param table A pointer to the table list.

  @see grant_reload

  @return Error state
    @retval FALSE Success
    @retval TRUE An error has occurred.
*/

static my_bool grant_reload_procs_priv(THD *thd, TABLE_LIST *table)
{
  HASH old_proc_priv_hash, old_func_priv_hash;
  my_bool return_val= FALSE;
  DBUG_ENTER("grant_reload_procs_priv");

  /* Save a copy of the current hash if we need to undo the grant load */
  old_proc_priv_hash= proc_priv_hash;
  old_func_priv_hash= func_priv_hash;

  if ((return_val= grant_load_procs_priv(table->table)))
  {
    /* Error; Reverting to old hash */
    DBUG_PRINT("error",("Reverting to old privileges"));
    my_hash_free(&proc_priv_hash);
    my_hash_free(&func_priv_hash);
    proc_priv_hash= old_proc_priv_hash;
    func_priv_hash= old_func_priv_hash;
  }
  else
  {
    my_hash_free(&old_proc_priv_hash);
    my_hash_free(&old_func_priv_hash);
  }

  DBUG_RETURN(return_val);
}


/**
  @brief Reload information about table and column level privileges if possible

  @param thd Current thread

  Locked tables are checked by acl_reload() and doesn't have to be checked
  in this call.
  This function is also used for initialization of structures responsible
  for table/column-level privilege checking.

  @return Error state
    @retval FALSE Success
    @retval TRUE  Error
*/

my_bool grant_reload(THD *thd)
{
  TABLE_LIST tables[3];
  HASH old_column_priv_hash;
  MEM_ROOT old_mem;
  my_bool return_val= 1;
  DBUG_ENTER("grant_reload");

  /* Don't do anything if running with --skip-grant-tables */
  if (!initialized)
    DBUG_RETURN(0);

  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("tables_priv"),
                           "tables_priv", TL_READ);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("columns_priv"),
                           "columns_priv", TL_READ);
  tables[2].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("procs_priv"),
                           "procs_priv", TL_READ);

  tables[0].next_local= tables[0].next_global= tables+1;
  tables[1].next_local= tables[1].next_global= tables+2;
  tables[0].open_type= tables[1].open_type= tables[2].open_type= OT_BASE_ONLY;

  /*
    Reload will work in the following manner:-

                             proc_priv_hash structure
                              /                     \
                    not initialized                 initialized
                   /               \                     |
    mysql.procs_priv table        Server Startup         |
        is missing                      \                |
             |                         open_and_lock_tables()
    Assume we are working on           /success             \failure
    pre 4.1 system tables.        Normal Scenario.          An error is thrown.
    A warning is printed          Reload column privilege.  Retain the old hash.
    and continue with             Reload function and
    reloading the column          procedure privileges,
    privileges.                   if available.
  */

  if (!(my_hash_inited(&proc_priv_hash)))
    tables[2].open_strategy= TABLE_LIST::OPEN_IF_EXISTS;

  /*
    To avoid deadlocks we should obtain table locks before
    obtaining LOCK_grant rwlock.
  */
  if (open_and_lock_tables(thd, tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    if (thd->get_stmt_da()->is_error())
    {
      sql_print_error("Fatal error: Can't open and lock privilege tables: %s",
                      thd->get_stmt_da()->message_text());
    }
    goto end;
  }

  if (tables[2].table == NULL)
  {
    sql_print_warning("Table 'mysql.procs_priv' does not exist. "
                      "Please run mysql_upgrade.");
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_NO_SUCH_TABLE,
                        ER(ER_NO_SUCH_TABLE), tables[2].db,
                        tables[2].table_name);
  }

  LOCK_grant.wrlock();

  /* Save a copy of the current hash if we need to undo the grant load */
  old_column_priv_hash= column_priv_hash;

  /*
    Create a new memory pool but save the current memory pool to make an undo
    opertion possible in case of failure.
  */
  old_mem= memex;
  init_sql_alloc(key_memory_acl_memex,
                 &memex, ACL_ALLOC_BLOCK_SIZE, 0);
  /*
    tables[2].table i.e. procs_priv can be null if we are working with
    pre 4.1 privilage tables
  */
  if ((return_val= (grant_load(thd, tables) ||
                    (tables[2].table != NULL &&
                     grant_reload_procs_priv(thd, &tables[2])))
     ))
  {                                             // Error. Revert to old hash
    DBUG_PRINT("error",("Reverting to old privileges"));
    my_hash_free(&column_priv_hash);
    free_root(&memex,MYF(0));
    column_priv_hash= old_column_priv_hash;     /* purecov: deadcode */
    memex= old_mem;                             /* purecov: deadcode */
  }
  else
  {                                             //Reload successful
    my_hash_free(&old_column_priv_hash);
    free_root(&old_mem,MYF(0));
    grant_version++;
  }
  LOCK_grant.wrunlock();

end:
  close_acl_tables(thd);
  DBUG_RETURN(return_val);
}


void acl_update_user(const char *user, const char *host,
                     enum SSL_type ssl_type,
                     const char *ssl_cipher,
                     const char *x509_issuer,
                     const char *x509_subject,
                     USER_RESOURCES  *mqh,
                     ulong privileges,
                     const LEX_CSTRING &plugin,
                     const LEX_CSTRING &auth,
		     MYSQL_TIME password_change_time,
                     LEX_ALTER password_life,
                     ulong what_is_set)
{
  DBUG_ENTER("acl_update_user");
  mysql_mutex_assert_owner(&acl_cache->lock);
  for (ACL_USER *acl_user= acl_users->begin();
       acl_user != acl_users->end(); ++acl_user)
  {
    if ((!acl_user->user && !user[0]) ||
        (acl_user->user && !strcmp(user,acl_user->user)))
    {
      if ((!acl_user->host.get_host() && !host[0]) ||
          (acl_user->host.get_host() &&
          !my_strcasecmp(system_charset_info, host, acl_user->host.get_host())))
      {
        if (plugin.length > 0)
        {
          acl_user->plugin.str= plugin.str;
          acl_user->plugin.length = plugin.length;
          optimize_plugin_compare_by_pointer(&acl_user->plugin);
          if (!auth_plugin_is_built_in(acl_user->plugin.str))
            acl_user->plugin.str= strmake_root(&global_acl_memory,
                                               plugin.str, plugin.length);
          /* Update auth string only when specified in ALTER/GRANT */
          if (auth.str)
          {
            if (auth.length == 0)
              acl_user->auth_string.str= const_cast<char*>("");
            else
              acl_user->auth_string.str= strmake_root(&global_acl_memory,
                              auth.str, auth.length);
            acl_user->auth_string.length= auth.length;
            set_user_salt(acl_user);
            if (password_change_time.time_type != MYSQL_TIMESTAMP_ERROR)
              acl_user->password_last_changed= password_change_time;
          }
        }
        acl_user->access=privileges;
        if (mqh->specified_limits & USER_RESOURCES::QUERIES_PER_HOUR)
          acl_user->user_resource.questions=mqh->questions;
        if (mqh->specified_limits & USER_RESOURCES::UPDATES_PER_HOUR)
          acl_user->user_resource.updates=mqh->updates;
        if (mqh->specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR)
          acl_user->user_resource.conn_per_hour= mqh->conn_per_hour;
        if (mqh->specified_limits & USER_RESOURCES::USER_CONNECTIONS)
          acl_user->user_resource.user_conn= mqh->user_conn;
        if (ssl_type != SSL_TYPE_NOT_SPECIFIED)
        {
          acl_user->ssl_type= ssl_type;
          acl_user->ssl_cipher= (ssl_cipher ? strdup_root(&global_acl_memory,
                                                    ssl_cipher) :        0);
          acl_user->x509_issuer= (x509_issuer ? strdup_root(&global_acl_memory,
                                                      x509_issuer) : 0);
          acl_user->x509_subject= (x509_subject ?
                                   strdup_root(&global_acl_memory, x509_subject) : 0);
        }
        /* update details related to password lifetime, password expiry */
        if (password_life.update_password_expired_column ||
            what_is_set & PLUGIN_ATTR)
          acl_user->password_expired= password_life.update_password_expired_column;
        if (!password_life.update_password_expired_column &&
            password_life.update_password_expired_fields)
        {
          if (!password_life.use_default_password_lifetime)
          {
            acl_user->password_lifetime= password_life.expire_after_days;
            acl_user->use_default_password_lifetime= false;
          }
          else
            acl_user->use_default_password_lifetime= true;
        }

        if (password_life.update_account_locked_column)
        {
          acl_user->account_locked = password_life.account_locked;
        }

        /* search complete: */
        break;
      }
    }
  }
  DBUG_VOID_RETURN;
}


void acl_insert_user(const char *user, const char *host,
                     enum SSL_type ssl_type,
                     const char *ssl_cipher,
                     const char *x509_issuer,
                     const char *x509_subject,
                     USER_RESOURCES *mqh,
                     ulong privileges,
                     const LEX_CSTRING &plugin,
                     const LEX_CSTRING &auth,
		     MYSQL_TIME password_change_time,
                     LEX_ALTER password_life)
{
  DBUG_ENTER("acl_insert_user");
  ACL_USER acl_user;

  mysql_mutex_assert_owner(&acl_cache->lock);
  /*
     All accounts can authenticate per default. This will change when
     we add a new field to the user table.

     Currently this flag is only set to false when authentication is attempted
     using an unknown user name.
  */
  acl_user.can_authenticate= true;

  acl_user.user= *user ? strdup_root(&global_acl_memory,user) : 0;
  acl_user.host.update_hostname(*host ? strdup_root(&global_acl_memory, host) : 0);
  if (plugin.str[0])
  {
    acl_user.plugin= plugin;
    optimize_plugin_compare_by_pointer(&acl_user.plugin);
    if (!auth_plugin_is_built_in(acl_user.plugin.str))
      acl_user.plugin.str= strmake_root(&global_acl_memory,
                                        plugin.str, plugin.length);
    acl_user.auth_string.str= auth.str ?
      strmake_root(&global_acl_memory, auth.str,
                   auth.length) : const_cast<char*>("");
    acl_user.auth_string.length= auth.length;

    optimize_plugin_compare_by_pointer(&acl_user.plugin);
  }
  else
  {
    acl_user.plugin= native_password_plugin_name;
    acl_user.auth_string.str= const_cast<char*>("");
    acl_user.auth_string.length= 0;
  }

  acl_user.access= privileges;
  acl_user.user_resource= *mqh;
  acl_user.sort= get_sort(2,acl_user.host.get_host(), acl_user.user);
  //acl_user.hostname_length=(uint) strlen(host);
  acl_user.ssl_type=
    (ssl_type != SSL_TYPE_NOT_SPECIFIED ? ssl_type : SSL_TYPE_NONE);
  acl_user.ssl_cipher=
    ssl_cipher ? strdup_root(&global_acl_memory, ssl_cipher) : 0;
  acl_user.x509_issuer=
    x509_issuer ? strdup_root(&global_acl_memory, x509_issuer) : 0;
  acl_user.x509_subject=
    x509_subject ? strdup_root(&global_acl_memory, x509_subject) : 0;
  /* update details related to password lifetime, password expiry */
  acl_user.password_expired= password_life.update_password_expired_column;
  acl_user.password_lifetime= password_life.expire_after_days;
  acl_user.use_default_password_lifetime= password_life.use_default_password_lifetime;
  acl_user.password_last_changed= password_change_time;
  acl_user.account_locked= password_life.account_locked;

  set_user_salt(&acl_user);

  acl_users->push_back(acl_user);
  if (acl_user.host.check_allow_all_hosts())
    allow_all_hosts=1;          // Anyone can connect /* purecov: tested */
  std::sort(acl_users->begin(), acl_users->end(), ACL_compare());

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();
  DBUG_VOID_RETURN;
}


void acl_update_proxy_user(ACL_PROXY_USER *new_value, bool is_revoke)
{
  mysql_mutex_assert_owner(&acl_cache->lock);

  DBUG_ENTER("acl_update_proxy_user");
  for (ACL_PROXY_USER *acl_user= acl_proxy_users->begin();
       acl_user != acl_proxy_users->end(); ++acl_user)
  {
    if (acl_user->pk_equals(new_value))
    {
      if (is_revoke)
      {
        DBUG_PRINT("info", ("delting ACL_PROXY_USER"));
        acl_proxy_users->erase(acl_user);
      }
      else
      {
        DBUG_PRINT("info", ("updating ACL_PROXY_USER"));
        acl_user->set_data(new_value);
      }
      break;
    }
  }
  DBUG_VOID_RETURN;
}


void acl_update_db(const char *user, const char *host, const char *db,
                   ulong privileges)
{
  mysql_mutex_assert_owner(&acl_cache->lock);

  for (ACL_DB *acl_db= acl_dbs->begin(); acl_db < acl_dbs->end(); )
  {
    if ((!acl_db->user && !user[0]) ||
        (acl_db->user &&
        !strcmp(user,acl_db->user)))
    {
      if ((!acl_db->host.get_host() && !host[0]) ||
          (acl_db->host.get_host() &&
          !strcmp(host, acl_db->host.get_host())))
      {
        if ((!acl_db->db && !db[0]) ||
            (acl_db->db && !strcmp(db,acl_db->db)))
        {
          if (privileges)
            acl_db->access=privileges;
          else
          {
            acl_db= acl_dbs->erase(acl_db);
            // Don't increment loop variable.
            continue;
          }
        }
      }
    }
    ++acl_db;
  }
}


/*
  Insert a user/db/host combination into the global acl_cache

  SYNOPSIS
    acl_insert_db()
    user                User name
    host                Host name
    db                  Database name
    privileges          Bitmap of privileges

  NOTES
    acl_cache->lock must be locked when calling this
*/

void acl_insert_db(const char *user, const char *host, const char *db,
                   ulong privileges)
{
  ACL_DB acl_db;
  mysql_mutex_assert_owner(&acl_cache->lock);
  acl_db.user= strdup_root(&global_acl_memory,user);
  acl_db.host.update_hostname(*host ? strdup_root(&global_acl_memory, host) : 0);
  acl_db.db= strdup_root(&global_acl_memory, db);
  acl_db.access= privileges;
  acl_db.sort= get_sort(3,acl_db.host.get_host(), acl_db.db, acl_db.user);
  acl_dbs->push_back(acl_db);
  std::sort(acl_dbs->begin(), acl_dbs->end(), ACL_compare());
}


void get_mqh(const char *user, const char *host, USER_CONN *uc)
{
  ACL_USER *acl_user;

  mysql_mutex_lock(&acl_cache->lock);

  if (initialized && (acl_user= find_acl_user(host,user, FALSE)))
    uc->user_resources= acl_user->user_resource;
  else
    memset(&uc->user_resources, 0, sizeof(uc->user_resources));

  mysql_mutex_unlock(&acl_cache->lock);
}



/**
  Update the security context when updating the user

  Helper function.
  Update only if the security context is pointing to the same user and
  the user is not a proxied user for a different proxy user.
  And return true if the update happens (i.e. we're operating on the
  user account of the current user).
  Normalize the names for a safe compare.

  @param sctx           The security context to update
  @param acl_user_ptr   User account being updated
  @param expired        new value of the expiration flag
  @return               did the update happen ?
 */
bool
update_sctx_cache(Security_context *sctx, ACL_USER *acl_user_ptr, bool expired)
{
  const char *acl_host= acl_user_ptr->host.get_host();
  const char *acl_user= acl_user_ptr->user;
  const char *sctx_user= sctx->priv_user().str;
  const char *sctx_host= sctx->priv_host().str;

  /* If the user is connected as a proxied user, verify against proxy user */
  if (sctx->proxy_user().str && *sctx->proxy_user().str != '\0')
  {
    sctx_user = sctx->user().str;
  }

  if (!acl_host)
    acl_host= "";
  if(!acl_user)
    acl_user= "";
  if (!sctx_host)
    sctx_host= "";
  if (!sctx_user)
    sctx_user= "";

  if (!strcmp(acl_user, sctx_user) && !strcmp(acl_host, sctx_host))
  {
    sctx->set_password_expired(expired);
    return true;
  }

  return false;
}



#endif /* NO_EMBEDDED_ACCESS_CHECKS */

