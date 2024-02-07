/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef Ndb_sql_metadata_table_H
#define Ndb_sql_metadata_table_H

#include <cstdint>
#include <string>

#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_record_layout.h"
#include "storage/ndb/plugin/ndb_util_table.h"

// RAII style class for creating and updating the table
class Ndb_sql_metadata_table : public Ndb_util_table {
  Ndb_sql_metadata_table() = delete;
  Ndb_sql_metadata_table(const Ndb_sql_metadata_table &) = delete;

  bool define_table_ndb(NdbDictionary::Table &table,
                        unsigned mysql_version) const override;

  bool create_indexes(const NdbDictionary::Table &table) const override;

 public:
  Ndb_sql_metadata_table(class Thd_ndb *);
  virtual ~Ndb_sql_metadata_table();

  bool check_schema() const override;
  bool need_upgrade() const override;
  std::string define_table_dd() const override;
  const NdbDictionary::Index *get_index() const;
  bool drop_events_in_NDB() const override { return true; }
};

/* Class provides an API for using the table, NdbRecord-style.
   It has a default constructor, so it can be statically allocated,
   but it cannot be used until after setup() is called.
*/
class Ndb_sql_metadata_api {
 public:
  Ndb_sql_metadata_api()
      : m_record_layout(5),  // five columns in table
        m_restart_in_progress(false) {}
  ~Ndb_sql_metadata_api() = default;
  Ndb_sql_metadata_api(const Ndb_sql_metadata_api &) = delete;
  Ndb_sql_metadata_api &operator=(const Ndb_sql_metadata_api &) = delete;

  /* Record Types */
  static constexpr short TYPE_LOCK = 4;
  static constexpr short TYPE_USER = 11;
  static constexpr short TYPE_GRANT = 12;

  void setup(NdbDictionary::Dictionary *, const NdbDictionary::Table *);
  void clear(NdbDictionary::Dictionary *);
  bool isInitialized() const { return m_ordered_index_rec; }

  void setRestarting() { m_restart_in_progress = true; }
  bool isRestarting() { return m_restart_in_progress; }

  NdbRecord *rowNdbRecord() const { return m_row_rec; }
  NdbRecord *noteNdbRecord() const { return m_note_rec; }
  NdbRecord *keyNdbRecord() const { return m_hash_key_rec; }
  NdbRecord *orderedNdbRecord() const { return m_ordered_index_rec; }

  size_t getRowSize() const { return m_full_record_size; }
  size_t getNoteSize() const { return m_note_record_size; }
  size_t getKeySize() const { return m_key_record_size; }

  void initRowBuffer(char *buf) { layout().initRowBuffer(buf); }
  void setType(char *buf, short a) { layout().setValue(0, a, buf); }
  void setName(char *buf, std::string a) { layout().setValue(1, a, buf); }
  void packName(char *buf, std::string a) { layout().packValue(1, a, buf); }
  void setSeq(char *buf, short a) { layout().setValue(2, a, buf); }
  void setNote(char *buf, uint32_t *a) { layout().setValue(3, a, buf); }
  void setSql(char *buf, std::string a) { layout().setValue(4, a, buf); }

  void getType(const char *buf, unsigned short *a) {
    layout().getValue(buf, 0, a);
  }
  void getName(const char *buf, size_t *a, const char **b) {
    layout().getValue(buf, 1, a, b);
  }
  void getSeq(const char *buf, unsigned short *a) {
    layout().getValue(buf, 2, a);
  }
  /* Getter for nullable column returns bool; true = NOT NULL */
  bool getNote(const char *buf, uint32_t *a) {
    return layout().getValue(buf, 3, a);
  }
  void getSql(const char *buf, size_t *a, const char **b) {
    layout().getValue(buf, 4, a, b);
  }

  /* Global locking around snapshot updates
     After a mysql server has written a batch of rows to ndb_sql_metadata,
     it may use the schema change distribution protocol to force other mysql
     servers to read these rows. The global snapshot lock serializes these
     "snapshot refreshes" so that only one of them happens at a time.

     initializeSnapshotLock() assures that the token lock tuple exists.

     acquireSnapshotLock() attempts to acquire an exclusive read lock on the
     lock tuple, without waiting.

     releaseSnapshotLock() releases the read lock.
  */
  const NdbError &initializeSnapshotLock(Ndb *);
  const NdbError &acquireSnapshotLock(Ndb *, NdbTransaction *&);
  void releaseSnapshotLock(NdbTransaction *tx) { tx->close(); }

 private:
  void writeSnapshotLockRow(NdbTransaction *);
  Ndb_record_layout &layout() { return m_record_layout; }
  Ndb_record_layout m_record_layout;

  NdbRecord *m_row_rec{nullptr};
  NdbRecord *m_note_rec{nullptr};
  NdbRecord *m_hash_key_rec{nullptr};
  NdbRecord *m_ordered_index_rec{nullptr};

  size_t m_full_record_size{0};
  size_t m_note_record_size{0};
  size_t m_key_record_size{0};

  bool m_restart_in_progress;
};

#endif
