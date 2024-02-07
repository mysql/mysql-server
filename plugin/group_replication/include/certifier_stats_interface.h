/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef CERTIFIER_STATS_INTERFACE_INCLUDE
#define CERTIFIER_STATS_INTERFACE_INCLUDE

#include <mysql/group_replication_priv.h>

class Certifier_stats {
 public:
  virtual ~Certifier_stats() = default;
  virtual ulonglong get_positive_certified() = 0;
  virtual ulonglong get_negative_certified() = 0;
  virtual ulonglong get_certification_info_size() = 0;
  virtual int get_group_stable_transactions_set_string(char **buffer,
                                                       size_t *length) = 0;
  virtual void get_last_conflict_free_transaction(std::string *value) = 0;
};

#endif /* CERTIFIER_STATS_INTERFACE_INCLUDE */
