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

#include <assert.h>

#include "adapter_global.h"
#include "Record.h"

Record::Record(NdbDictionary::Dictionary *d, int ncol, bool isPK) : 
  dict(d),
  ncolumns(ncol),
  n_nullable(0),
  index(0),
  rec_size(0),
  start_of_nullmap(0),
  size_of_nullmap(0),
  ndb_record(0), 
  specs(new NdbDictionary::RecordSpecification[ncol]),
  pkColumnMask(),
  allColumnMask(),
  isPrimaryKey(isPK)                                                       {};

Record::~Record() {
  // dict->releaseRecord(ndb_record);  // causes crashes due to dict==0. ??
  delete[] specs;
}

/*
 * add a column to a Record
 */
void Record::addColumn(const NdbDictionary::Column *column) {
  assert(index < ncolumns);

  /* Link to the Dictionary Column */
  specs[index].column = column;
      
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

  /* Maintain smask of all columns and of PK columns */
  allColumnMask.array[index >> 3] |= (1 << (index & 7));
  if(column->getPrimaryKey()) {
    pkColumnMask.array[index >> 3] |= (1 << (index & 7));
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
  for(unsigned int n = 0 ; n < ncolumns ; n++)
    specs[n].nullbit_byte_offset += start_of_nullmap;
  
  /* Then adjust the total record size */
  rec_size += size_of_nullmap;
}


/* 
 * Finish Table or PrimaryKey record after all columns have been added.
 */
bool Record::completeTableRecord(const NdbDictionary::Table *table) {
  build_null_bitmap();
  ndb_record = dict->createRecord(table, specs, ncolumns, sizeof(specs[0]));

  assert(index == ncolumns);
  assert(ndb_record);
  assert(NdbDictionary::getRecordRowLength(ndb_record) == rec_size);

  return true;
}

/* Finish Secondary Index record after all columns have been added.
*/
bool Record::completeIndexRecord(const NdbDictionary::Index *ndb_index) {
  build_null_bitmap();
  ndb_record = dict->createRecord(ndb_index, specs, ncolumns, sizeof(specs[0]));

  assert(ndb_record);
  assert(index == ncolumns);
  assert(NdbDictionary::getRecordRowLength(ndb_record) == rec_size);

  return true;
}


/* This implementation of padding will align all 2-, 4-, or 8- byte columns,
   even if they happen to be character columns that do not strictly require
   alignment.  This is considered a plausibly good time/space trade-off, and 
   in the worst case wastes 3 bytes for a CHAR[5] column.
*/
void Record::pad_offset_for_alignment() {
  int alignment = specs[index].column->getSizeInBytes();
  int bad_offset = 0;
  
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
