/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef CONNECTION_CONTROL_FAILED_ATTEMPTS_LIST_IMP_H
#define CONNECTION_CONTROL_FAILED_ATTEMPTS_LIST_IMP_H

#include <map>
#include <shared_mutex>
#include "connection_control.h"
#include "connection_control_pfs_table.h"

namespace connection_control {
class Failed_attempts_list_imp : public Connection_control_alloc {
 public:
  void failed_attempts_define(const char *userhost);
  bool failed_attempts_undefine(const char *userhost);

  /**
    Fetch a copy of the queue data to return to a PFS table
    @retval the data to put in the PFS table
  */
  Connection_control_pfs_table_data *copy_pfs_table_data();
  unsigned long long get_failed_attempts_list_count();
  unsigned long long get_failed_attempts_count(const char *userhost);
  void reset();

 private:
  //* A case insensitive comparator using the C library */
  struct ciLessLibC {
    bool operator()(const std::string &lhs, const std::string &rhs) const {
#if defined _WIN32
      return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
#else
      return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
#endif
    }
  };
  std::map<std::string, PSI_ulong, ciLessLibC> failed_attempts_map;
  std::shared_mutex mutex_;
};
}  // namespace connection_control
extern connection_control::Failed_attempts_list_imp g_failed_attempts_list;

#endif /* CONNECTION_CONTROL_FAILED_ATTEMPTS_LIST_IMP_H */
