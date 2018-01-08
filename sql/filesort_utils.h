/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FILESORT_UTILS_INCLUDED
#define FILESORT_UTILS_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <utility>

#include "my_base.h"                   // ha_rows
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/service_mysql_alloc.h" // my_free
#include "sql/sql_array.h"             // Bounds_checked_array

class Cost_model_table;
class Sort_param;


/*
  Calculate cost of merge sort

    @param num_rows            Total number of rows.
    @param num_keys_per_buffer Number of keys per buffer.
    @param elem_size           Size of each element.
    @param cost_model          Cost model object that provides cost data.

    Calculates cost of merge sort by simulating call to merge_many_buff().

  @returns
    Computed cost of merge sort in disk seeks.

  @note
    Declared here in order to be able to unit test it,
    since library dependencies have not been sorted out yet.

    See also comments get_merge_many_buffs_cost().
*/

double get_merge_many_buffs_cost_fast(ha_rows num_rows,
                                      ha_rows num_keys_per_buffer,
                                      uint elem_size,
                                      const Cost_model_table *cost_model);


/**
  A wrapper class around the buffer used by filesort().
  The sort buffer is a contiguous chunk of memory,
  containing both records to be sorted, and pointers to said records:

  <start of buffer       | still unused   |                      end of buffer>
  |rec 0|record 1  |rec 2|  ............  |ptr to rec2|ptr to rec1|ptr to rec0|

  Records will be inserted "left-to-right". Records are not necessarily
  fixed-size, they can be packed and stored without any "gaps".

  Record pointers will be inserted "right-to-left", as a side-effect
  of inserting the actual records.

  We wrap the buffer in order to be able to do lazy initialization of the
  pointers: the buffer is often much larger than what we actually need.

  With this allocation scheme, and lazy initialization of the pointers,
  we are able to pack variable-sized records in the buffer,
  and thus possibly have space for more records than we initially estimated.

  The buffer must be kept available for multiple executions of the
  same sort operation, so we have explicit allocate and free functions,
  rather than doing alloc/free in CTOR/DTOR.
 */
class Filesort_buffer
{
public:
  Filesort_buffer() :
    m_next_rec_ptr(NULL), m_rawmem(NULL), m_record_pointers(NULL),
    m_sort_keys(NULL),
    m_num_records(0), m_record_length(0),
    m_size_in_bytes(0), m_idx(0)
  {}

  /** Sort me... */
  void sort_buffer(Sort_param *param, uint count);

  /**
    Reverses the record pointer array, to avoid recording new results for
    non-deterministic mtr tests.
  */
  void reverse_record_pointers()
  {
    if (m_idx < 2) // There is nothing to swap.
      return;
    uchar **keys= get_sort_keys();
    const longlong count= m_idx - 1;
    for (longlong ix= 0; ix <= count/2; ++ix)
      std::swap(keys[ix], keys[count - ix]);
  }

  /**
    Initializes all the record pointers.
  */
  void init_record_pointers()
  {
    init_next_record_pointer();
    while (m_idx < m_num_records)
      (void) get_next_record_pointer();
    reverse_record_pointers();
  }

  /**
    Prepares the buffer for the next batch of records to process.
   */
  void init_next_record_pointer()
  {
    m_idx= 0;
    m_next_rec_ptr= m_rawmem;
    m_sort_keys= NULL;
  }

  /**
    @returns the number of bytes currently in use for data.
   */
  size_t space_used_for_data() const
  {
    return m_next_rec_ptr ? m_next_rec_ptr - m_rawmem : 0;
  }

  /**
    @returns the number of bytes left in the buffer.
  */
  size_t spaceleft() const
  {
    DBUG_ASSERT(m_next_rec_ptr >= m_rawmem);
    const size_t spaceused=
      (m_next_rec_ptr - m_rawmem) +
      (static_cast<size_t>(m_idx) * sizeof(uchar*));
    return m_size_in_bytes - spaceused;
  }

  /**
    Is the buffer full?
  */
  bool isfull() const
  {
    if (m_idx < m_num_records)
      return false;
    return spaceleft() < (m_record_length + sizeof(uchar*));
  }

  /**
    Where should the next record be stored?
   */
  uchar *get_next_record_pointer()
  {
    uchar *retval= m_next_rec_ptr;
    // Save the return value in the record pointer array.
    m_record_pointers[-m_idx]= m_next_rec_ptr;
    // Prepare for the subsequent request.
    m_idx++;
    m_next_rec_ptr+= m_record_length;
    return retval;
  }

  /**
    Adjusts for actual record length. get_next_record_pointer() above was
    pessimistic, and assumed that the record could not be packed.
   */
  void adjust_next_record_pointer(uint val)
  {
    DBUG_ASSERT(m_record_length >= val);
    m_next_rec_ptr-= (m_record_length - val);
  }

  /**
    @returns total size of buffer: pointer array + record buffers.
  */
  size_t sort_buffer_size() const
  {
    return m_size_in_bytes;
  }

  /**
    Allocates the buffer, but does *not* initialize pointers.
    Total size = (num_records * record_length) + (num_records * sizeof(pointer))
                  space for records               space for pointer to records
    Caller is responsible for raising an error if allocation fails.

    @param num_records   Number of records.
    @param record_length (maximum) size of each record.
    @returns Pointer to allocated area, or NULL in case of out-of-memory.
  */
  uchar *alloc_sort_buffer(uint num_records, uint record_length);

  /// Frees the buffer.
  void free_sort_buffer()
  {
    my_free(m_rawmem);
    *this= Filesort_buffer();
  }

  /**
    Used to access the "right-to-left" array of record pointers as an ordinary
    "left-to-right" array, so that we can pass it directly on to std::sort().
  */
  uchar **get_sort_keys()
  {
    if (m_idx == 0)
      return NULL;
    return &m_record_pointers[1 - m_idx];
  }

  /**
    Gets sorted record number ix. @see get_sort_keys()
    Only valid after buffer has been sorted!
  */
  uchar *get_sorted_record(uint ix)
  {
    return m_sort_keys[ix];
  }

  /**
    @returns The entire buffer, as a character array.
    This is for reusing the memory for merge buffers.
   */
  Bounds_checked_array<uchar> get_raw_buf()
  {
    return Bounds_checked_array<uchar>(m_rawmem, m_size_in_bytes);
  }

  /**
    We need an assignment operator, see filesort().
    The default one is OK, we want memberwise assignment, i.e. shallow copy.
  */
  Filesort_buffer &operator=(const Filesort_buffer &rhs)= default;

private:
  uchar  *m_next_rec_ptr;    /// The next record will be inserted here.
  uchar  *m_rawmem;          /// The raw memory buffer.
  uchar **m_record_pointers; /// The "right-to-left" array of record pointers.
  uchar **m_sort_keys;       /// Caches the value of get_sort_keys()
  uint    m_num_records;     /// Saved value from alloc_sort_buffer()
  uint    m_record_length;   /// Saved value from alloc_sort_buffer()
  size_t  m_size_in_bytes;   /// Size of raw buffer, in bytes.

  /**
    This is the index in the "right-to-left" array of the next record to
    be inserted into the buffer. It is signed, because we use it in signed
    expressions like:
        m_record_pointers[-m_idx];
    It is longlong rather than int, to ensure that it covers UINT_MAX32
    without any casting/warning.
  */
  longlong m_idx;
};

#endif  // FILESORT_UTILS_INCLUDED
