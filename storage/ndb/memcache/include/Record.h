/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 
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
#ifndef NDBMEMCACHE_RECORD_H
#define NDBMEMCACHE_RECORD_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <assert.h>
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
  COL_STORE_EXT_ID, 
  COL_STORE_EXT_SIZE,
  COL_STORE_KEY,
  COL_STORE_VALUE = COL_STORE_KEY   + MAX_KEY_COLUMNS,
  COL_MAX_COLUMNS = COL_STORE_VALUE + MAX_VAL_COLUMNS
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
  /* Set masks */
  void maskActive(int id, Uint8 *mask) const;
  void maskInactive(int id, Uint8 *mask) const;

  /* Readers */
  bool isNull(int id, char *data) const;
  void * getPointer(int id, char *data) const;
  int getIntValue(int id, char *data) const;
  Uint64 getUint64Value(int id, char *data) const;
  size_t getStringifiedLength(char *data) const;
  bool decodeNoCopy(int id, char **dest_ptr, size_t *len_ptr, 
                    const char * const src) const;
  size_t decodeCopy(int id, char *dest, char *src) const;
  bool appendCRLF(int id, size_t offset, char *data) const;
  
  /* Writers */
  void clearNullBits(char *data) const;
  void setNullBits(char *data) const;
  void setNull(int id, char *data, Uint8 *mask) const;
  void setNotNull(int id, char *data, Uint8 *mask) const;
  bool setIntValue(int id, int value, char *buffer, Uint8 *mask) const;
  bool setUint64Value(int id, Uint64 value, char *buffer, Uint8 *mask) const;
  int encode(int id, const char *key, int nkey, char *buffer, Uint8 *mask) const;
 
  /* Public instance variables */
  const int ncolumns;
  size_t rec_size;
  NdbRecord *ndb_record; 
  int nkeys;
  int nvalues;
  size_t value_length;         /* total length of text value columns */
  
  private:
  /* Private instance variables */
  int index;
  short map[COL_MAX_COLUMNS];   /* map col. identifier to col index in record */
  short tmap[COL_MAX_COLUMNS];  /* map col. identifier to col no. in table */
  int n_nullable;
  size_t start_of_nullmap;
  size_t size_of_nullmap;
  DataTypeHandler ** const handlers;
  NdbDictionary::RecordSpecification * const specs;
  NdbDictionary::Dictionary * m_dict;
  
  /* Private methods */
  const Record & operator=(const Record &) const;
  void build_null_bitmap();
  void pad_offset_for_alignment();
  void nullmapSetNull(int idx, char *data) const;
  void nullmapSetNotNull(int idx, char *data) const;
};


/* Inline functions */

inline void Record::clearNullBits(char *data) const {
  memset(data + start_of_nullmap, 0, size_of_nullmap);
}

inline void Record::setNullBits(char *data) const {
  memset(data + start_of_nullmap, 0xFF, size_of_nullmap);
}

inline bool Record::isNull(int id, char *data) const {
  if(specs[map[id]].column->getNullable())
    return (*(data + specs[map[id]].nullbit_byte_offset) & 
             (1 << specs[map[id]].nullbit_bit_in_byte));
  return false;
}

inline void * Record::getPointer(int idx, char *data) const {
  return data + specs[map[idx]].offset;
}

inline void Record::maskActive(int id, Uint8 *mask) const {
  const short & col_num = tmap[id];
  if(col_num >= 0)
    mask[col_num >> 3] |= (1 << (col_num & 7));
}

inline void Record::maskInactive(int id, Uint8 *mask) const {
  const short & col_num = tmap[id];
  if(col_num >= 0)
    mask[col_num >> 3] &= (0xff ^ (1 << (col_num & 7)));
}


#endif
