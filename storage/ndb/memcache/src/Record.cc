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
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <NdbApi.hpp>
#include <memcached/extension_loggers.h>

#include "DataTypeHandler.h"
#include "Record.h"
#include "debug.h"

extern EXTENSION_LOGGER_DESCRIPTOR *logger;

Record::Record(int ncol) : ncolumns(ncol), rec_size(0), ndb_record(0), 
                           nkeys(0), nvalues(0),  
                           value_length(0),
                           index(0),
                           n_nullable(0),
                           start_of_nullmap(0),
                           size_of_nullmap(0),
                           handlers(new DataTypeHandler *[ncol]),
                           specs(new NdbDictionary::RecordSpecification[ncol])
{
  for(int i = 0 ; i < COL_MAX_COLUMNS; i++)
    map[i] = tmap[i] = -1;
};

Record::~Record() {
  if(ndb_record) 
    m_dict->releaseRecord(ndb_record);
  delete[] handlers;
  delete[] specs;
};


/*
 * add a column to a Record
 */
void Record::addColumn(short col_type, const NdbDictionary::Column *column) {
  assert(col_type <= COL_STORE_VALUE);
  assert(index < ncolumns);
  int col_identifier = col_type;

  if(col_type == COL_STORE_KEY)
    col_identifier += nkeys++;
  else if(col_type == COL_STORE_VALUE) 
    col_identifier += nvalues++;
    
  assert(nkeys <= MAX_KEY_COLUMNS);
  assert(nvalues <= MAX_VAL_COLUMNS);
  
  /* The "record map" (map) is an array that maps a specifier like 
     "COL_STORE_VALUE + 1" (the second value column) or 
     "COL_STORE_CAS" (the cas column) to that column's index in the record.  */
  map[col_identifier] = index;

  /* Link to the Dictionary Column */
  specs[index].column = column;
    
  /* The "table map" (tmap) maps the specifier directly to the column's number
     column number in the underlying table. */
  tmap[col_identifier] = column->getColumnNo();

 /* Link to the correct DataTypeHandler */
  handlers[index] = getDataTypeHandlerForColumn(column);

  /* Keep track of total possible text size. */
  if(col_type == COL_STORE_VALUE && handlers[index]->contains_string) {
    value_length += column->getLength();
  }
    
  /* If the data type requires alignment, insert some padding.
     This call will alter rec_size if needed */
  pad_offset_for_alignment();

  /* The current record size is the offset of this column */
  specs[index].offset = rec_size;  

  /* Set nullbits in the record specification */
  if(column->getNullable()) {
    specs[index].nullbit_byte_offset = n_nullable / 8;
    specs[index].nullbit_bit_in_byte = n_nullable % 8;
    n_nullable++;
  }
  else {
    specs[index].nullbit_byte_offset = 0;
    specs[index].nullbit_bit_in_byte = 0;
  }

  /* Increment the counter and record size */
  index += 1;

  rec_size += column->getSizeInBytes();
};


void Record::build_null_bitmap() {
  /* The map needs 1 bit for each nullable column */
  size_of_nullmap = n_nullable / 8;         // whole bytes
  if(n_nullable % 8) size_of_nullmap += 1;  // partially-used bytes
  
  /* The null bitmap goes at the end of the record.
     Adjust ("relink") the null offsets in every RecordSpecification. 
     Do this even if there are no nullable columns.
  */
  start_of_nullmap = rec_size;
  for(int n = 0 ; n < ncolumns ; n++)
    specs[n].nullbit_byte_offset += start_of_nullmap;
  
  /* Then adjust the total record size */
  rec_size += size_of_nullmap;
}


/* 
 * Finish creating record after all columns have been added.
 */
bool Record::complete(NdbDictionary::Dictionary *dict, 
                      const NdbDictionary::Table *table) {
  build_null_bitmap();
  m_dict = dict;
  ndb_record = dict->createRecord(table, specs, ncolumns, sizeof(specs[0]));

  if(!ndb_record) {
    logger->log(LOG_WARNING, 0, "createRecord() failure: %s\n",
               dict->getNdbError().message);  
    return false;
  }
  assert(NdbDictionary::getRecordRowLength(ndb_record) == rec_size);
  return true;
}


bool Record::complete(NdbDictionary::Dictionary *dict, 
                      const NdbDictionary::Index *ndb_index) {                       
  build_null_bitmap();
  m_dict = dict;
  ndb_record = dict->createRecord(ndb_index, specs, ncolumns, sizeof(specs[0]));

  if(!ndb_record) {
    logger->log(LOG_WARNING, 0, "createRecord() failure: %s\n",
                dict->getNdbError().message);  
    return false;
  }
  assert(NdbDictionary::getRecordRowLength(ndb_record) == rec_size);
  return true;
}


/* TODO:
   Either handle NULL values appropriately in every method, or make sure that 
   the methods are protected, only ever accessed through Operation, and that 
   Operation handles all NULLS.
*/   

/* Append \r\n to a value so that it can be sent directly to a memcache client.
   DO NOT adjust the length.
*/
bool Record::appendCRLF(int id, size_t len, char *buffer) const {
  int idx = map[id];
  int length_bytes = handlers[idx]->contains_string;

if(length_bytes) {
    size_t offset = len + length_bytes - 1;  /* see DataTypeHandler.h */
    buffer[offset]   = '\r';
    buffer[offset+1] = '\n';
    return true;
  }
  return false;
}

bool Record::decodeNoCopy(int id,        /* Column identifier */
                          char **dest,   /* OUT: holds ptr to decoded string */
                          size_t *len,   /* OUT: holds string length */
                          const char * const src) /* source string */ const {

  int idx = map[id];
  if(! handlers[idx]->contains_string) return false;
  
  const char * src_buffer = src + specs[idx].offset;
  *len = handlers[idx]->readFromNdb(specs[idx].column, *dest, src_buffer);
  return true;
}


size_t Record::decodeCopy(int id, char *dest, char *src) const {
  size_t out_len;
  char * out_str;
  char * src_buffer = src + specs[map[id]].offset;

  if(decodeNoCopy(id, &out_str, &out_len, src)) {  // string types
    memcpy(dest, out_str, out_len);  // Copy from src to dest 
  }
  else {  /* Non-string types */
    out_len = handlers[map[id]]->readFromNdb(specs[map[id]].column, dest, src_buffer);
  }
  *(dest + out_len) = 0;  // terminating null; may be overwritten by a tab
  return out_len;
}


inline void Record::nullmapSetNull(int idx, char *data) const {
  *(data + specs[idx].nullbit_byte_offset) |= 
    (1 << specs[idx].nullbit_bit_in_byte);
}


inline void Record::nullmapSetNotNull(int idx, char *data) const {
  *(data +specs[idx].nullbit_byte_offset) &=
    (0xFF ^ (1 << specs[idx].nullbit_bit_in_byte));
}


/* Here's a pattern in the setter methods: 
     setIntValue(), setUint64Value(), encode(), setNull() and setNotNull(). 
   First map the column identifier to its index in the record. 
   If this is -1, then we're operating on some column (CAS or MATH or whatever)
   that doesn't exist in this record, and we just return harmlessly. 
   Otherwise:
     maskActive() is inlined, and sets the column bit in the mask.
     nullmapSet[Not]Null() is inlined, and operates on the nullmap.
*/

void Record::setNull(int id, char *data, Uint8 *mask) const {
  int idx = map[id];
  if(idx == -1) 
    return;
  maskActive(id, mask);
  if(specs[idx].column->getNullable())
    nullmapSetNull(idx, data);
}


void Record::setNotNull(int id, char *data, Uint8 *mask) const {
  int idx = map[id];
  if(idx == -1) 
    return;
  maskActive(id, mask);
  if(specs[idx].column->getNullable())
    nullmapSetNull(idx, data);
}


int Record::getIntValue(int id, char *data) const {
  int idx = map[id];
  NumericHandler * h = handlers[idx]->native_handler;
  const char * buffer = data + specs[idx].offset;
  int i = 0;
  
  if(h) {
    if(h->read_int32(i, buffer) < 0) return 0;
  }
  else {
    logger->log(LOG_WARNING, 0, "getIntValue() failed for column %s - "
                "unsupported column type.", specs[idx].column->getName());
  }
  return i;
}


bool Record::setIntValue(int id, int value, char *data, Uint8 *mask) const {
  int idx = map[id];
  if(idx == -1) 
    return true;
  maskActive(id, mask);
  if(specs[idx].column->getNullable())
    nullmapSetNotNull(idx, data);

  NumericHandler * h = handlers[idx]->native_handler;
  char * buffer = data + specs[idx].offset;
  
  if(h) {
    return (h->write_int32(value,buffer) > 0);
  } 
  else {
    logger->log(LOG_WARNING, 0, "setIntValue() failed for column %s - "
                "unsupported column type.", specs[idx].column->getName());
    return false;
  }
}


Uint64 Record::getUint64Value(int id, char *data) const {
  int idx = map[id];
  const char * buffer = data + specs[idx].offset;

  if(specs[idx].column->getType() != NdbDictionary::Column::Bigunsigned) {
    logger->log(LOG_WARNING, 0, "Operation failed - column %s must be BIGINT UNSIGNED",
                specs[idx].column->getName());
    return 0;
  }
  
  LOAD_FOR_ARCHITECTURE(Uint64, value, buffer);
  return value;
}


bool Record::setUint64Value(int id, Uint64 value, char *data, Uint8 *mask) const {
  int idx = map[id];
  if(idx == -1) 
    return true;
  maskActive(id, mask);
  if(specs[idx].column->getNullable())
    nullmapSetNotNull(idx, data);
  char * buffer = data + specs[idx].offset;

  if(specs[idx].column->getType() != NdbDictionary::Column::Bigunsigned) {
    logger->log(LOG_WARNING, 0, "Operation failed - column %s must be BIGINT UNSIGNED",
                specs[idx].column->getName());
    return false;
  }
  
  STORE_FOR_ARCHITECTURE(Uint64, value, buffer);
  return true;
}  


int Record::encode(int id, const char *key, int nkey,
                   char *buffer, Uint8 *mask) const {
  int idx = map[id];
  if(idx == -1) 
    return 0;
  maskActive(id, mask);
  if(specs[idx].column->getNullable())
    nullmapSetNotNull(idx, buffer);
  return handlers[idx]->writeToNdb(specs[idx].column, nkey, key, 
                                   buffer + specs[idx].offset);
}


size_t Record::getStringifiedLength(char *data) const {
  size_t total = 0;
  for(int t = 0; t < ncolumns ; t++) {
    if(t) total++;  // one for the tab
    total += handlers[t]->getStringifiedLength(specs[t].column, 
                                               data + specs[t].offset);
  }
  return total;
}


void Record::pad_offset_for_alignment() {
  int alignment = 1;
  int bad_offset = 0;
  
  if(index == map[COL_STORE_CAS]) {  // CAS column requires 8-byte alignment
    alignment = 8;
  }
  else if(! handlers[index]->contains_string) {
     alignment = specs[index].column->getSizeInBytes();
  }

  switch(alignment) {
    case 2: case 4: case 8:   /* insert padding */
      bad_offset = rec_size % alignment;
      if(bad_offset) 
        rec_size += (alignment - bad_offset);
      break;
    default:
      break;
  }
}


void Record::debug_dump() {
  DEBUG_PRINT("---------- Record ------------------");
  DEBUG_PRINT("Record size: %d", rec_size);
  DEBUG_PRINT("Nullmap start:   %d  Nullmap size:  %d", start_of_nullmap, 
              size_of_nullmap);
  for(int i = 0 ; i < ncolumns ; i++) {
    DEBUG_PRINT(" Col %d column  : %s %d/%d", i, specs[i].column->getName()
                , specs[i].column->getSize(), specs[i].column->getSizeInBytes());
    DEBUG_PRINT(" Col %d offset  : %d", i, specs[i].offset);
    DEBUG_PRINT(" Col %d null bit: %d.%d", i,
                specs[i].nullbit_byte_offset, specs[i].nullbit_bit_in_byte);
  }
  DEBUG_PRINT("-------------------------------------");
}
