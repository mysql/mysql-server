/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SCHEMA_TRANS_GUARD_H
#define NDB_SCHEMA_TRANS_GUARD_H

#include "sql/ndb_thd_ndb.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"

// RAII style class for managing a NDB schema transaction
class Ndb_schema_trans_guard {
  const Thd_ndb *const m_thd_ndb;
  NdbDictionary::Dictionary *const m_dict;
  // Remember if transaction has been successfully comitted, otherwise
  // it will be aborted when class goes out of scope.
  // NOTE! It's not an error to end a schema trans more than once
  // so there is not much point in keeping more state than this.
  bool m_comitted{false};

 public:
  Ndb_schema_trans_guard(const Thd_ndb *thd_ndb,
                         NdbDictionary::Dictionary *dict)
      : m_thd_ndb(thd_ndb), m_dict(dict) {}

  ~Ndb_schema_trans_guard() {
    if (!m_comitted) {
      abort_trans();
    }
  }

  bool begin_trans();
  bool commit_trans();
  bool abort_trans();
};

#endif
