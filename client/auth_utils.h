/*
   Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#ifndef AUTH_UTILS_INCLUDED
#define AUTH_UTILS_INCLUDED
#include <string>
#include <iostream>
#include <map>

#define ERR_FILE 1 // File related error
#define ERR_ENCRYPTION 2  // Encryption related error
#define ERR_SYNTAX 3 // Syntax and parsing related error
#define ERR_OTHER 4 // Unspecified error
#define ERR_NO_SUCH_CATEGORY 5 // The specified category isn't present
#define ALL_OK 0 // Reporting success and good fortune

/**
 Trivial parser for the login.cnf file which assumes that first entry
 is a [client] header followed by some attribute/value -pairs

  @param sin Input stream
  @param[out] options Output map
  @return success rate
  @retval ALL_OK Reporting success and good fortune
  @retval ERR_SYNTAX Failed to parse the stream
*/
int parse_cnf_file(std::istream &sin,
                    std::map<std::string, std::string > *options,
                    const std::string &header);
/**
  Decrypts a file and produces a stringstream.

  @param fin Input stream
  @param[out] sout Output stream
  @return success rate
  @retval ALL_OK Reporting success and good fortune
  @retval ERR_ENCRYPTION Failed to decrypt the input stream
*/
int decrypt_login_cnf_file(std::istream &fin, std::ostream &sout);

void generate_password(std::string *password, int size);
void trim(std::string *s);
const std::string get_allowed_pwd_chars();

/**
  An experimental uniform representation of access privileges in MySQL
*/
class Access_privilege
{
public:
  Access_privilege() : m_priv(0) {}
  Access_privilege(uint64_t privileges) : m_priv(privileges) {}
  Access_privilege(const Access_privilege &priv) : m_priv(priv.m_priv) {}
  bool has_select_ac()  { return (m_priv & (1L)) > 0; }
  bool has_insert_ac() { return (m_priv & (1L << 1)) > 0; }
  bool has_update_ac() { return (m_priv & (1L << 2)) > 0; }
  bool has_delete_ac() { return (m_priv & (1L << 3)) > 0; }
  bool has_create_ac() { return (m_priv & (1L << 4)) > 0; }
  bool has_drop_ac() { return (m_priv & (1L << 5)) > 0; }
  bool has_relead_ac() { return (m_priv & (1L << 6)) > 0; }
  bool has_shutdown_ac() { return (m_priv & (1L << 7)) > 0; }
  bool has_process_ac() { return (m_priv & (1L << 8)) > 0; }
  bool has_file_ac() { return (m_priv & (1L << 9)) > 0; }
  bool has_grant_ac() { return (m_priv & (1L << 10)) > 0; }
  bool has_references_ac() { return (m_priv & (1L << 11)) > 0; }
  bool has_index_ac() { return (m_priv & (1L << 12)) > 0; }
  bool has_alter_ac() { return (m_priv & (1L << 13)) > 0; }
  bool has_show_db_ac() { return (m_priv & (1L << 14)) > 0; }
  bool has_super_ac() { return (m_priv & (1L << 15)) > 0; }
  bool has_create_tmp_ac() { return (m_priv & (1L << 16)) > 0; }
  bool has_lock_tables_ac() { return (m_priv & (1L << 17)) > 0; }
  bool has_execute_ac() { return (m_priv & (1L << 18)) > 0; }
  bool has_repl_slave_ac() { return (m_priv & (1L << 19)) > 0; }
  bool has_repl_client_ac() { return (m_priv & (1L << 20)) > 0; }
  bool has_create_view_ac() { return (m_priv & (1L << 21)) > 0; }
  bool has_show_view_ac() { return (m_priv & (1L << 22)) > 0; }
  bool has_create_proc_ac() { return (m_priv & (1L << 23)) > 0; }
  bool has_alter_proc_ac() { return (m_priv & (1L << 24)) > 0; }
  bool has_create_user_ac() { return (m_priv & (1L << 25)) > 0; }
  bool has_event_ac() { return (m_priv & (1L << 26)) > 0; }
  bool has_trigger_ac() { return (m_priv & (1L << 27)) > 0; }
  bool has_create_tablespace_ac() { return (m_priv & (1L << 28)) > 0; }
  inline static uint64_t select_ac()  { return (1L); }
  inline uint64_t insert_ac() { return (1L << 1); }
  inline uint64_t update_ac() { return (1L << 2); }
  inline uint64_t delete_ac() { return (1L << 3); }
  inline static uint64_t create_ac() { return (1L << 4); }
  inline static uint64_t drop_ac() { return (1L << 5); }
  inline static uint64_t relead_ac() { return (1L << 6); }
  inline static uint64_t shutdown_ac() { return (1L << 7); }
  inline static uint64_t process_ac() { return (1L << 8); }
  inline static uint64_t file_ac() { return (1L << 9); }
  inline static uint64_t grant_ac() { return (1L << 10); }
  inline static uint64_t references_ac() { return (1L << 11); }
  inline static uint64_t index_ac() { return (1L << 12); }
  inline static uint64_t alter_ac() { return (1L << 13); }
  inline static uint64_t show_db_ac() { return (1L << 14); }
  inline static uint64_t super_ac() { return (1L << 15); }
  inline static uint64_t create_tmp_ac() { return (1L << 16); }
  inline static uint64_t lock_tables_ac() { return (1L << 17); }
  inline static uint64_t execute_ac() { return (1L << 18); }
  inline static uint64_t repl_slave_ac() { return (1L << 19); }
  inline static uint64_t repl_client_ac() { return (1L << 20); }
  inline static uint64_t create_view_ac() { return (1L << 21); }
  inline static uint64_t show_view_ac() { return (1L << 22); }
  inline static uint64_t create_proc_ac() { return (1L << 23); }
  inline static uint64_t alter_proc_ac() { return (1L << 24); }
  inline static uint64_t create_user_ac() { return (1L << 25); }
  inline static uint64_t event_ac() { return (1L << 26); }
  inline static uint64_t trigger_ac() { return (1L << 27); }
  inline static uint64_t create_tablespace_ac() { return (1L << 28); }
  inline static uint64_t acl_all() { return 0xfffffff; }
  uint64_t to_int() const { return m_priv; };
private:
  uint64_t m_priv;
};
#endif
