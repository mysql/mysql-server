/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef Ndb_mgmclient_hpp
#define Ndb_mgmclient_hpp

class CommandInterpreter;
class Ndb_mgmclient {
 public:
  Ndb_mgmclient(const char *host, const char *default_prompt, int verbose,
                int connect_retry_delay, const char *tls_search_path,
                int tls_start_type);
  ~Ndb_mgmclient();
  bool execute(const char *line, int try_reconnect = -1,
               bool interactive = true, int *error = NULL);
  const char *get_current_prompt() const;
  int set_default_backup_password(const char backup_password[]) const;
  int set_always_encrypt_backup(bool on) const;
  int test_tls();

 private:
  CommandInterpreter *m_cmd;
};

#endif  // Ndb_mgmclient_hpp
