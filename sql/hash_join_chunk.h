#ifndef SQL_HASH_JOIN_CHUNK_H_
#define SQL_HASH_JOIN_CHUNK_H_

/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stddef.h>

#include "my_alloc.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "sql/hash_join_buffer.h"

class String;

// A HashJoinChunk is a file located on disk that can be used to store rows.
// It is used in on-disk hash join when a table is to be partitioned out to
// several smaller files, a.k.a. HashJoinChunks.
//
// When writing a column to a HashJoinChunk, we use StoreFromTableBuffers for
// converting the necessary columns into a format suitable for storage on disk.
// Conveniently, StoreFromTableBuffers creates a contiguous range of bytes and a
// corresponding length that easily and efficiently can be written out to the
// file. When reading rows back from a file, LoadIntoTableBuffers() is used to
// put the row back into the table record buffers.
//
// The basic usage goes like this:
//
//   HashJoinChunk chunk;
//   chunk.Init(tables); // Initialize a chunk to hold data from the given
//                       // tables.
//   String buffer; // A buffer that is used when copying data between tables
//                  // and the chunk file, and vica versa.
//   while (iterator->Read() == 0) {
//     chunk.WriteRowToChunk(&buffer); // Write the row that lies in the record
//                                     // buffers of "tables" to this chunk,
//                                     // using the provided buffer.
//   };
//
//   chunk.Rewind(); // Prepare to read the first row in this chunk.
//   chunk.LoadRowFromChunk(&buffer); // Put the row from the chunk to the
//                                    // record buffers of "tables", using the
//                                    // provided buffer.
class HashJoinChunk {
 public:
  HashJoinChunk() = default;  // Constructible.

  HashJoinChunk(HashJoinChunk &&other);  // Movable.

  HashJoinChunk(const HashJoinChunk &obj) = delete;

  ~HashJoinChunk();

  /// Initialize this HashJoinChunk.
  ///
  /// @returns true if the initialization failed.
  bool Init(const hash_join_buffer::TableCollection &tables);

  /// @returns the number of rows in this HashJoinChunk
  ha_rows num_rows() const { return m_num_rows; }

  /// Write a row to the HashJoinChunk.
  ///
  /// Read the row that lies in the record buffer (record[0]) of the given
  /// tables and write it out to the underlying file. If the QEP_TAB signals
  /// that the row ID should be kept, it is also written out. Note that
  /// TABLE::read_set is used to signal which columns that should be written to
  /// the chunk.
  ///
  /// @param buffer a buffer that is used when copying data from the tables to
  ///   the chunk file. Note that any existing data in "buffer" is overwritten.
  ///
  /// @retval true on error.
  bool WriteRowToChunk(String *buffer);

  /// Read a row from the HashJoinChunk and put it in the record buffer.
  ///
  /// The function will read a row from file on disk and put it in the record
  /// buffers (table->record[0]) in the provided tables. The file on disk should
  /// already be pointing to the start of a row.
  ///
  /// @param buffer a buffer that is used when copying data from the chunk file
  ///   to the tables. Note that any existing data in "buffer" is overwritten.
  ///
  /// @retval true on error.
  bool LoadRowFromChunk(String *buffer);

  /// Flush the file buffer, and prepare the file for reading.
  ///
  /// @retval true on error
  bool Rewind();

 private:
  // A collection of which tables the chunk file holds data from. Used to
  // determine where to read data from, and where to put the data back.
  hash_join_buffer::TableCollection m_tables;

  // The number of rows in this chunk file.
  ha_rows m_num_rows{0};

  // The underlying file that is used when reading data to and from disk.
  IO_CACHE m_file;
};

#endif  // SQL_HASH_JOIN_CHUNK_H_
