/*
   Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_schema_trans_guard.h"

bool Ndb_schema_trans_guard::begin_trans() {
  DBUG_TRACE;

  if (m_dict->beginSchemaTrans() != 0 ||
      DBUG_EVALUATE_IF("ndb_schema_trans_start_fail", true, false)) {
    DBUG_PRINT("info", ("Failed to start NDB schema transaction"));
    m_thd_ndb->push_ndb_error_warning(m_dict->getNdbError());
    m_thd_ndb->push_warning("Failed to start NDB schema transaction");
    return false;  // Failed
  }
  DBUG_PRINT("info", ("Started NDB schema transaction"));
  return true;
}

bool Ndb_schema_trans_guard::commit_trans() {
  DBUG_TRACE;

  if (DBUG_EVALUATE_IF("ndb_schema_trans_commit_fail", true, false) ||
      m_dict->endSchemaTrans() != 0) {
    DBUG_PRINT("info", ("Failed to commit NDB schema transaction"));
    m_thd_ndb->push_ndb_error_warning(m_dict->getNdbError());
    m_thd_ndb->push_warning("Failed to commit NDB schema transaction");
    return false;  // Failed
  }
  DBUG_PRINT("info", ("Comitted NDB schema transaction"));
  m_comitted = true;
  return true;
}

bool Ndb_schema_trans_guard::abort_trans() {
  DBUG_TRACE;

  const bool abort_flags = NdbDictionary::Dictionary::SchemaTransAbort;
  if (m_dict->endSchemaTrans(abort_flags) != 0) {
    DBUG_PRINT("info", ("Failed to abort NDB schema transaction"));
    m_thd_ndb->push_ndb_error_warning(m_dict->getNdbError());
    m_thd_ndb->push_warning("Failed to abort NDB schema transaction");
    return false;  // Failed
  }
  DBUG_PRINT("info", ("Aborted NDB schema transaction"));
  return true;
}
