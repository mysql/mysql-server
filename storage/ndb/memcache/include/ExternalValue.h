/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#ifndef NDBMEMCACHE_EXTERNALVALUE_H
#define NDBMEMCACHE_EXTERNALVALUE_H

#include "TableSpec.h"
#include "ExpireTime.h"
#include "ndb_pipeline.h"
#include "ndb_worker.h"


#define EXTERN_VAL_MAX_PARTS 256

class ExternalValue {

  /* friend functions */
  friend ndb_async_callback callback_ext_parts_read;
  friend ndb_async_callback callback_ext_write;
  friend worker_step        append_after_read;
  friend worker_step        delete_after_header_read;

  /* Spec encapsulates an id, length, and number of parts */
  class Spec { 
  public:
    const size_t part_size;
    Uint64 id;
    size_t length;
    int nparts;
     
    Spec(int sz) : part_size(sz), id(0), length(0), nparts(0) {};
    bool readFromHeader(Operation &);
    void setLength(int length);
  };

  /* Static class methods */
public:
  static TableSpec * createContainerRecord(const char *);
  static op_status_t do_write(workitem *);
  static op_status_t do_delete(workitem *);
  static int do_delete(memory_pool *, NdbTransaction *, QueryPlan *, Operation &);
  static op_status_t do_read_header(workitem *, ndb_async_callback *, worker_step *);
  static void append_after_read(NdbTransaction *, workitem *);
  static bool setupKey(workitem *, Operation &);

  /* Public instance methods */
  ExternalValue(workitem *, NdbTransaction *t = 0);
  ~ExternalValue();
  void worker_read_external(Operation &, NdbTransaction *);
 
private:
  /* Private member variables */
  Spec old_hdr;   /** The "old" value is the one read or deleted */
  Spec new_hdr;   /** The "new" value is the one to be updated or inserted */
  ExpireTime expire_time;
  NdbTransaction *tx;
  workitem * const wqitem;
  QueryPlan * const ext_plan;  
  memory_pool * pool;
  char * value;
  size_t value_size_in_header;
  bool do_server_cas;
  Uint64 stored_cas;
  
  /* Private methods */
  op_status_t do_update();
  op_status_t do_insert();

  void update_after_header_read();
  void insert_after_header_read();
  void affix_short(int old_length, char *old_value);
  void finalize_write();

  void append();
  void prepend();
  bool insert();
  bool update();
  bool readParts();
  bool readFinalPart();
  bool deleteParts();
  bool insertParts(char * val, size_t len, int nparts, int offset);
  bool updatePart(int id, int part, char * val, size_t len);

  bool startTransaction(Operation &);
  void readStoredCas(Operation &);

  void warnMissingParts() const;
  void build_hash_item() const;
  void setMiscColumns(Operation &) const;
  void setValueColumns(Operation &) const;
  bool shouldExternalize(size_t len) const;
  int readLongValueIntoBuffer(char *) const;
};

/* Inline Methods */

inline bool ExternalValue::shouldExternalize(size_t len) const {
  return (len > value_size_in_header);
};


#endif
