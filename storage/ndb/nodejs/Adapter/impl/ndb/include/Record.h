/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

#ifndef NODEJS_ADAPTER_INCLUDE_RECORD_H
#define NODEJS_ADAPTER_INCLUDE_RECORD_H

// NOTE WHAT IS FRAGILE ABOUT THIS FILE:
// NdbApi uses Uint32 as a uint32_t, but v8 uses Uint32 as a class.
// Record.h must generally be included *before* node.h

#include "NdbApi.hpp"

class Record {
private:
  NdbDictionary::Dictionary *dict;
  size_t ncolumns, 
         n_nullable, 
         index,
         rec_size,
         start_of_nullmap, 
         size_of_nullmap;
  NdbRecord * ndb_record;
  NdbDictionary::RecordSpecification * const specs;
  union {
    unsigned char array[4];
    Uint32 value;
  } pkColumnMask, allColumnMask;
  bool isPrimaryKey;

  void build_null_bitmap();
  void pad_offset_for_alignment();

public:
  Record(NdbDictionary::Dictionary *, int, bool isPK=false);
  ~Record();
  void addColumn(const NdbDictionary::Column *);
  bool completeTableRecord(const NdbDictionary::Table *);
  bool completeIndexRecord(const NdbDictionary::Index *); 
  
  const NdbRecord * getNdbRecord() const;
  Uint32 getNoOfColumns() const;
  Uint32 getPkColumnMask() const;
  Uint32 getAllColumnMask() const;
  size_t getColumnOffset(int idx) const;
  const NdbDictionary::Column * getColumn(int idx) const;
  size_t getBufferSize() const;

  void setNull(int idx, char *data) const;
  void setNotNull(int idx, char *data) const;
  Uint32 isNull(int idx, char * data) const;
  bool isPK() const;
};


inline const NdbRecord * Record::getNdbRecord() const {
  return ndb_record;
}

inline Uint32 Record::getNoOfColumns() const {
  return ncolumns;
}

inline size_t Record::getColumnOffset(int idx) const {
  return specs[idx].offset;
}

inline const NdbDictionary::Column * Record::getColumn(int idx) const {
  return specs[idx].column;
}

inline size_t Record::getBufferSize() const {
  return rec_size;
}

inline void Record::setNull(int idx, char * data) const {
  if(specs[idx].column->getNullable()) {
    *(data + specs[idx].nullbit_byte_offset) |= 
      (1 << specs[idx].nullbit_bit_in_byte);
  }
}

inline void Record::setNotNull(int idx, char * data) const {
  if(specs[idx].column->getNullable()) {
    *(data +specs[idx].nullbit_byte_offset) &=
      (0xFF ^ (1 << specs[idx].nullbit_bit_in_byte));
  }
}

inline Uint32 Record::isNull(int idx, char * data) const {
  if(specs[idx].column->getNullable()) {
    return (*(data + specs[idx].nullbit_byte_offset) &
             (1 << specs[idx].nullbit_bit_in_byte));
  }
  else return 0;
}

inline bool Record::isPK() const {
  return isPrimaryKey;
}

inline Uint32 Record::getPkColumnMask() const {
  return pkColumnMask.value;
}

inline Uint32 Record::getAllColumnMask() const {
  return allColumnMask.value;
}


#endif
