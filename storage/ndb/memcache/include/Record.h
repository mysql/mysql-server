/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
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
#ifndef NDBMEMCACHE_RECORD_H
#define NDBMEMCACHE_RECORD_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <sys/types.h>
#include <string.h>

#include <NdbApi.hpp>

#include "DataTypeHandler.h"
#include "debug.h"
#include "TableSpec.h"

#define MAX_KEY_COLUMNS 4
#define MAX_VAL_COLUMNS 16

enum { 
  COL_STORE_CAS = 0, 
  COL_STORE_MATH, 
  COL_STORE_EXPIRES, 
  COL_STORE_FLAGS, 
  COL_STORE_KEY = 4,
  COL_STORE_VALUE = 8,   /* COL_STORE_KEY + MAX_KEY_COLUMNS */
  COL_MAX_COLUMNS = 24    /* COL_STORE_VLAUE + MAX_VAL_COLUMNS */
};

/** @class Record
 *  @brief Enable a TableSpec to be used through the NDB API.
 */


class Record {
  public:
  /* Public methods */
  Record(int ncol);
  ~Record();

  void addColumn(short col_type, const NdbDictionary::Column *column);
  bool complete(NdbDictionary::Dictionary *, 
                const NdbDictionary::Table *);
  bool complete(NdbDictionary::Dictionary *, 
                const NdbDictionary::Index *);
  void debug_dump();

  /* const public methods that operate on an (external) data buffer */
  void clearNullBits(char *data) const;
  bool isNull(int idx, char *data) const;
  void setNull(int idx, char *data) const;
  bool setIntValue(int idx, int value, char *buffer) const;
  void * getPointer(int idx, char *data) const;
  int getIntValue(int idx, char *data) const;
  bool setUint64Value(int idx, Uint64 value, char *buffer) const;
  Uint64 getUint64Value(int idx, char *data) const;
  int encode(int idx, const char *key, int nkey, char *buffer) const;
  size_t getStringifiedLength(char *data) const;
  bool decodeNoCopy(int idx, char **dest_ptr, size_t *len_ptr, 
                    const char * const src) const;
  size_t decodeCopy(int idx, char *dest, char *src) const;
  bool appendCRLF(int idx, size_t offset, char *data) const;
  
  /* Public instance variables */
  const int ncolumns;
  size_t rec_size;
  const NdbRecord *ndb_record; 
  int nkeys;
  int nvalues;
  
  private:
  /* Private instance variables */
  int index;
  short map[COL_MAX_COLUMNS];  /* map col. identifier to col. index in record */
  int n_nullable;
  size_t start_of_nullmap;
  size_t size_of_nullmap;
  DataTypeHandler ** const handlers;
  NdbDictionary::RecordSpecification * const specs;
  
  /* Private methods */
  const Record & operator=(const Record &) const;
  void build_null_bitmap();
  void pad_offset_for_alignment();
};


/* Inline functions */

inline void Record::clearNullBits(char *data) const {
  memset(data + start_of_nullmap, 0, size_of_nullmap);
}

inline void Record::setNull(int idx, char *data) const {  
  *(data + specs[map[idx]].nullbit_byte_offset) |= 
     (1 << specs[map[idx]].nullbit_bit_in_byte);
}

inline bool Record::isNull(int idx, char *data) const {
  if(specs[map[idx]].column->getNullable())
    return (*(data + specs[map[idx]].nullbit_byte_offset) & 
             (1 << specs[map[idx]].nullbit_bit_in_byte));
  return false;
}

inline void * Record::getPointer(int idx, char *data) const {
  return data + specs[map[idx]].offset;
}


#endif
