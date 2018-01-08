/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_AUTHENTICATION_INCLUDED
#define SQL_AUTHENTICATION_INCLUDED

#include <sys/types.h>

#include "lex_string.h"
#include "m_ctype.h"
#include "my_thread_local.h"            // my_thread_id
#include "mysql/plugin.h"
#include "mysql/plugin_auth.h"          // MYSQL_SERVER_AUTH_INFO
#include "mysql/plugin_auth_common.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "sql/sql_plugin_ref.h"         // plugin_ref
#include "sql/thr_malloc.h"

#include <string>

class THD;

typedef struct charset_info_st CHARSET_INFO;
typedef struct st_mysql_show_var SHOW_VAR;
class ACL_USER;
class Protocol_classic;

typedef struct st_net NET;

/* Classes */

class Thd_charset_adapter
{
  THD *thd;
public:
  Thd_charset_adapter(THD *thd_arg) : thd (thd_arg) {} 
  bool init_client_charset(uint cs_number);

  const CHARSET_INFO *charset();
};


/**
  The internal version of what plugins know as MYSQL_PLUGIN_VIO,
  basically the context of the authentication session
*/
struct MPVIO_EXT : public MYSQL_PLUGIN_VIO
{
  MYSQL_SERVER_AUTH_INFO auth_info;
  const ACL_USER *acl_user;
  plugin_ref plugin;        ///< what plugin we're under
  LEX_STRING db;            ///< db name from the handshake packet
  /** when restarting a plugin this caches the last client reply */
  struct {
    const char *plugin, *pkt;     ///< pointers into NET::buff
    uint pkt_len;
  } cached_client_reply;
  /** this caches the first plugin packet for restart request on the client */
  struct {
    char *pkt;
    uint pkt_len;
  } cached_server_packet;
  int packets_read, packets_written; ///< counters for send/received packets
  /** when plugin returns a failure this tells us what really happened */
  enum { SUCCESS, FAILURE, RESTART } status;

  /* encapsulation members */
  char *scramble;
  MEM_ROOT *mem_root;
  struct  rand_struct *rand;
  my_thread_id  thread_id;
  uint      *server_status;
  Protocol_classic *protocol;
  ulong max_client_packet_length;
  char *ip;
  char *host;
  Thd_charset_adapter *charset_adapter;
  LEX_CSTRING acl_user_plugin;
  int vio_is_encrypted;
  bool can_authenticate();
};

#if defined(HAVE_OPENSSL)
class String;

bool init_rsa_keys(void);
void deinit_rsa_keys(void);
int show_rsa_public_key(THD *thd, SHOW_VAR *var, char *buff);

#ifndef HAVE_YASSL
typedef struct rsa_st RSA;
#endif
class Rsa_authentication_keys
{
private:
  RSA *m_public_key;
  RSA *m_private_key;
  int m_cipher_len;
  char *m_pem_public_key;
  char **m_private_key_path;
  char **m_public_key_path;

  void get_key_file_path(char *key, String *key_file_path);
  bool read_key_file(RSA **key_ptr, bool is_priv_key, char **key_text_buffer);

public:
  Rsa_authentication_keys(char **private_key_path,
                          char **public_key_path)
    : m_public_key(0),
      m_private_key(0),
      m_cipher_len(0),
      m_pem_public_key(0),
      m_private_key_path(private_key_path),
      m_public_key_path(public_key_path)
  {}
  ~Rsa_authentication_keys()
  {}

  void free_memory();
  void *allocate_pem_buffer(size_t buffer_len);
  RSA *get_private_key()
  {
    return m_private_key;
  }

  RSA *get_public_key()
  {
    return m_public_key;
  }

  int get_cipher_length();
  bool read_rsa_keys();
  const char *get_public_key_as_pem(void)
  {
    return m_pem_public_key;
  }
  
};

#endif /* HAVE_OPENSSL */

/* Data Structures */

extern LEX_CSTRING native_password_plugin_name;
extern LEX_CSTRING sha256_password_plugin_name;
extern LEX_CSTRING validate_password_plugin_name;
extern LEX_CSTRING default_auth_plugin_name;
extern LEX_CSTRING caching_sha2_password_plugin_name;

extern bool allow_all_hosts;

extern plugin_ref native_password_plugin;

#define AUTH_DEFAULT_RSA_PRIVATE_KEY "private_key.pem"
#define AUTH_DEFAULT_RSA_PUBLIC_KEY "public_key.pem"

#endif /* SQL_AUTHENTICATION_INCLUDED */
