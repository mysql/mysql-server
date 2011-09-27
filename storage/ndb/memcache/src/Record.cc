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

Record::Record(int ncol) : ncolumns(ncol), rec_size(0), nkeys(0), nvalues(0),  
                           index(0), n_nullable(0),
                           start_of_nullmap(0), size_of_nullmap(0), 
                           handlers(new DataTypeHandler *[ncol]),
                           specs(new NdbDictionary::RecordSpecification[ncol])
{};

Record::~Record() {
  delete[] handlers;
  delete[] specs;
};


/*
 * add a column to a Record
 */
void Record::addColumn(short col_type, const NdbDictionary::Column *column) {
  assert(index < ncolumns);

  /* place the index correctly into the columns array */
  switch(col_type) {
    case COL_STORE_KEY:
      map[COL_STORE_KEY + nkeys++] = index;
      assert(nkeys < (COL_STORE_VALUE - COL_STORE_KEY));  // max key columns
      break;
    case COL_STORE_VALUE:
      map[COL_STORE_VALUE + nvalues++] = index;
      assert(nvalues < (COL_MAX_COLUMNS - COL_STORE_VALUE));  // max value cols
      break;
    case COL_STORE_CAS:
    case COL_STORE_MATH:
    case COL_STORE_EXPIRES:
    case COL_STORE_FLAGS:
      map[col_type] = index;
      break;
    default:
      assert("Bad column type" == 0);
  }

  /* Build the Record Specification */
  specs[index].column = column;
  specs[index].offset = rec_size;  /* use the current record size */
  if(column->getNullable()) {
    specs[index].nullbit_byte_offset = n_nullable / 8;
    specs[index].nullbit_bit_in_byte = n_nullable % 8;
    n_nullable++;
  }
  else {
    specs[index].nullbit_byte_offset = 0;
    specs[index].nullbit_bit_in_byte = 0;
  }

  /* Link in the correct DataTypeHandler */
  handlers[index] = getDataTypeHandlerForColumn(column);

  /* Increment the counter and record size */
  index += 1;
  rec_size += getColumnRecordSize(column);
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
                      const NdbDictionary::Index *index) {                       
  build_null_bitmap();
  ndb_record = dict->createRecord(index, specs, ncolumns, sizeof(specs[0]));

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


int Record::getIntValue(int id, char *data) const {
  int index = map[id];
  NumericHandler * h = handlers[index]->native_handler;
  const char * buffer = data + specs[index].offset;
  int i = 0;
  
  if(h) {
    if(h->read_int32(i, buffer) < 0) return 0;
  }
  else {
    logger->log(LOG_WARNING, 0, "getIntValue() failed for column %s - "
                "unsupported column type.", specs[index].column->getName());
  }
  return i;
}


bool Record::setIntValue(int id, int value, char *data) const {
  int index = map[id];
  NumericHandler * h = handlers[index]->native_handler;
  char * buffer = data + specs[index].offset;
  
  if(h) {
    return (h->write_int32(value,buffer) > 0);
  } 
  else {
    logger->log(LOG_WARNING, 0, "setIntValue() failed for column %s - "
                "unsupported column type.", specs[index].column->getName());
    return false;
  }
}


Uint64 Record::getUint64Value(int id, char *data) const {
  int index = map[id];
  const char * buffer = data + specs[index].offset;

  if(specs[index].column->getType() != NdbDictionary::Column::Bigunsigned) {
    logger->log(LOG_WARNING, 0, "Operation failed - column %s must be BIGINT UNSIGNED",
                specs[index].column->getName());
    return 0;
  }
  
  LOAD_FOR_ARCHITECTURE(Uint64, value, buffer);
  return value;
}


bool Record::setUint64Value(int id, Uint64 value, char *data) const {
  int index = map[id];
  const char * buffer = data + specs[index].offset;

  if(specs[index].column->getType() != NdbDictionary::Column::Bigunsigned) {
    logger->log(LOG_WARNING, 0, "Operation failed - column %s must be BIGINT UNSIGNED",
                specs[index].column->getName());
    return false;
  }
  
  STORE_FOR_ARCHITECTURE(Uint64, value, buffer);
  return true;
}  


int Record::encode(int id, const char *key, int nkey,
                   char *buffer) const {
  return handlers[map[id]]->writeToNdb(specs[map[id]].column, nkey, key, 
                                       buffer + specs[map[id]].offset);
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

