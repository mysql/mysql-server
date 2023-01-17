/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_BLOBS_BUFFER_H
#define NDB_BLOBS_BUFFER_H

#include <memory>

#include "my_inttypes.h"

/**
   @brief Buffer holding the data for blob column(s) received from NDB.

   The buffer is normally allocated when the size of all received blob
   columns are known. After that the data for each blob is read (copied out)
   from the NdbApi and finally Field_blob pointers are set to point into the
   buffer.
 */
class Ndb_blobs_buffer {
  std::unique_ptr<uchar[]> m_buf;
  size_t m_size{0};  // Size of allocated buffer
 public:
  /**
     @brief Allocate space in the buffer, discard any prior buffer space

     @param size  Number of bytes to allocate

     @return false failed to allocate
   */
  bool allocate(size_t size) {
    try {
      m_buf = std::make_unique<uchar[]>(size);
      m_size = size;
    } catch (...) {
      m_size = 0;
      return false;
    }
    return true;
  }

  /**
     @brief Release memory allocated for the buffer
  */
  void release() {
    m_buf.reset();
    m_size = 0;
  }

  /**
     @brief Get pointer to specified offset in buffer. The offset must be
     located inside the buffer previously allocated.

     @return pointer to data or nullptr if offset is invalid
   */
  uchar *get_ptr(size_t offset) const {
    if (m_size == 0 || offset >= m_size) {
      return nullptr;
    }
    return m_buf.get() + offset;
  }

  /**
     @brief Number of bytes managed by the buffer
   */
  size_t size() const { return m_size; }
};

#endif
