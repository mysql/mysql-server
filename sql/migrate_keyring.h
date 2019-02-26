/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MIGRATE_KEYRING_H_INCLUDED
#define MIGRATE_KEYRING_H_INCLUDED

#include "mysql/plugin_keyring.h"
#include "mysql.h"
#include <string>
#include <vector>

class THD;

using std::string;

#define MAX_KEY_LEN 16384

enum enum_plugin_type
{
  SOURCE_PLUGIN= 0, DESTINATION_PLUGIN
};

class Key_info
{
public:
  Key_info()
    : m_key_id_len(0),
      m_user_id_len(0)
  {}
  Key_info(char     *key_id,
           char     *user_id)
  {
    m_key_id_len= strlen(key_id);
    memcpy(m_key_id, key_id, m_key_id_len);
    m_key_id[m_key_id_len]= '\0';
    m_user_id_len= strlen(user_id);
    memcpy(m_user_id, user_id, m_user_id_len);
    m_user_id[m_user_id_len]= '\0';
  }
  Key_info(const Key_info &ki)
  {
    this->m_key_id_len= ki.m_key_id_len;
    memcpy(this->m_key_id, ki.m_key_id, this->m_key_id_len);
    this->m_key_id[this->m_key_id_len]= '\0';
    this->m_user_id_len= ki.m_user_id_len;
    memcpy(this->m_user_id, ki.m_user_id, this->m_user_id_len);
    this->m_user_id[this->m_user_id_len]= '\0';
  }
public:
  char     m_key_id[MAX_KEY_LEN];
  int      m_key_id_len;
  char     m_user_id[USERNAME_LENGTH];
  int      m_user_id_len;
};

class Migrate_keyring
{
public:
  /**
    Standard constructor.
  */
  Migrate_keyring();
  /**
    Initialize all needed parameters to proceed with migration process.
  */
  bool init(int  argc,
            char **argv,
            char *source_plugin,
            char *destination_plugin,
            char *user, char *host, char *password,
            char *socket, ulong port);
  /**
    Migrate keys from source keyring to destination keyring.
  */
  bool execute();
  /**
    Standard destructor
  */
  ~Migrate_keyring();

private:
  /**
    Load source or destination plugin.
  */
  bool load_plugin(enum_plugin_type plugin_type);
  /**
    Fetch keys from source plugin and store in destination plugin.
  */
  bool fetch_and_store_keys();
  /**
    Disable @@keyring_operations variable.
  */
  bool disable_keyring_operations();
  /**
    Enable @@keyring_operations variable.
  */
  bool enable_keyring_operations();

private:
  int m_argc;
  char **m_argv;
  string m_source_plugin_option;
  string m_destination_plugin_option;
  string m_source_plugin_name;
  string m_destination_plugin_name;
  string m_internal_option;
  st_mysql_keyring *m_source_plugin_handle;
  st_mysql_keyring *m_destination_plugin_handle;
  std::vector<Key_info> m_source_keys;
  MYSQL *mysql;
};

#endif /* MIGRATE_KEYRING_H_INCLUDED */
