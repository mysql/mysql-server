/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "filesort_utils.h"
#include "opt_costmodel.h"
#include "sql_const.h"
#include "sql_sort.h"
#include "table.h"

#include <algorithm>
#include <functional>
#include <vector>

PSI_memory_key key_memory_Filesort_buffer_sort_keys;

namespace {
/**
  A local helper function. See comments for get_merge_buffers_cost().
 */
double get_merge_cost(ha_rows num_elements, ha_rows num_buffers, uint elem_size,
                      const Cost_model_table *cost_model)
{
  const double io_ops= static_cast<double>(num_elements * elem_size) / IO_SIZE;
  const double io_cost= cost_model->io_block_read_cost(io_ops);
  const double cpu_cost=
    cost_model->key_compare_cost(num_elements * log((double) num_buffers) /
                                 M_LN2);
  return 2 * io_cost + cpu_cost;
}
}

/**
  This is a simplified, and faster version of @see get_merge_many_buffs_cost().
  We calculate the cost of merging buffers, by simulating the actions
  of @see merge_many_buff. For explanations of formulas below,
  see comments for get_merge_buffers_cost().
  TODO: Use this function for Unique::get_use_cost().
*/
double get_merge_many_buffs_cost_fast(ha_rows num_rows,
                                      ha_rows num_keys_per_buffer,
                                      uint elem_size,
                                      const Cost_model_table *cost_model)
{
  ha_rows num_buffers= num_rows / num_keys_per_buffer;
  ha_rows last_n_elems= num_rows % num_keys_per_buffer;
  double total_cost;

  // Calculate CPU cost of sorting buffers.
  total_cost=
    num_buffers * cost_model->key_compare_cost(num_keys_per_buffer *
                                               log(1.0 + num_keys_per_buffer)) +
    cost_model->key_compare_cost(last_n_elems * log(1.0 + last_n_elems));

  // Simulate behavior of merge_many_buff().
  while (num_buffers >= MERGEBUFF2)
  {
    // Calculate # of calls to merge_buffers().
    const ha_rows loop_limit= num_buffers - MERGEBUFF*3/2;
    const ha_rows num_merge_calls= 1 + loop_limit/MERGEBUFF;
    const ha_rows num_remaining_buffs=
      num_buffers - num_merge_calls * MERGEBUFF;

    // Cost of merge sort 'num_merge_calls'.
    total_cost+=
      num_merge_calls *
      get_merge_cost(num_keys_per_buffer * MERGEBUFF, MERGEBUFF, elem_size,
                     cost_model);

    // # of records in remaining buffers.
    last_n_elems+= num_remaining_buffs * num_keys_per_buffer;

    // Cost of merge sort of remaining buffers.
    total_cost+=
      get_merge_cost(last_n_elems, 1 + num_remaining_buffs, elem_size,
                     cost_model);

    num_buffers= num_merge_calls;
    num_keys_per_buffer*= MERGEBUFF;
  }

  // Simulate final merge_buff call.
  last_n_elems+= num_keys_per_buffer * num_buffers;
  total_cost+= get_merge_cost(last_n_elems, 1 + num_buffers, elem_size,
                              cost_model);
  return total_cost;
}


uchar *Filesort_buffer::alloc_sort_buffer(uint num_records, uint record_length)
{
  DBUG_EXECUTE_IF("alloc_sort_buffer_fail",
                  DBUG_SET("+d,simulate_out_of_memory"););

  /*
    For subqueries we try to re-use the buffer, in order to save
    expensive malloc/free calls. Both of the sizing parameters may change:
    - num_records due to e.g. different statistics from the engine.
    - record_length due to different buffer usage:
      a heap table may be flushed to myisam, which allows us to sort by
      <key, addon fields> rather than <key, rowid>
    If we already have a buffer, but with wrong size, we simply delete it.
   */
  if (m_rawmem != NULL)
  {
    if (num_records != m_num_records ||
        record_length != m_record_length)
      free_sort_buffer();
  }

  m_size_in_bytes= ALIGN_SIZE(num_records * (record_length + sizeof(uchar*)));
  if (m_rawmem == NULL)
    m_rawmem= (uchar*) my_malloc(key_memory_Filesort_buffer_sort_keys,
                                 m_size_in_bytes, MYF(0));
  if (m_rawmem == NULL)
  {
    m_size_in_bytes= 0;
    return NULL;
  }
  m_record_pointers= reinterpret_cast<uchar**>(m_rawmem)
    + ((m_size_in_bytes / sizeof(uchar*)) - 1);
  m_num_records= num_records;
  m_record_length= record_length;
  m_idx= 0;
  return m_rawmem;
}

namespace {

/*
  An inline function which does memcmp().
  This one turns out to be pretty fast on all platforms, except sparc.
  See the accompanying unit tests, which measure various implementations.
 */
inline bool my_mem_compare(const uchar *s1, const uchar *s2, size_t len)
{
  DBUG_ASSERT(len > 0);
  DBUG_ASSERT(s1 != NULL);
  DBUG_ASSERT(s2 != NULL);
  do {
    if (*s1++ != *s2++)
      return *--s1 < *--s2;
  } while (--len != 0);
  return false;
}

#define COMPARE(N) if (s1[N] != s2[N]) return s1[N] < s2[N]

inline bool my_mem_compare_longkey(const uchar *s1, const uchar *s2, size_t len)
{
  COMPARE(0);
  COMPARE(1);
  COMPARE(2);
  COMPARE(3);
  return memcmp(s1 + 4, s2 + 4, len - 4) < 0;
}


class Mem_compare :
  public std::binary_function<const uchar*, const uchar*, bool>
{
public:
  Mem_compare(size_t n) : m_size(n) {}
  bool operator()(const uchar *s1, const uchar *s2) const
  {
#ifdef __sun
    // The native memcmp is faster on SUN.
    return memcmp(s1, s2, m_size) < 0;
#else
    return my_mem_compare(s1, s2, m_size);
#endif
  }
private:
  size_t m_size;
};

class Mem_compare_longkey :
  public std::binary_function<const uchar*, const uchar*, bool>
{
public:
  Mem_compare_longkey(size_t n) : m_size(n) {}
  bool operator()(const uchar *s1, const uchar *s2) const
  {
#ifdef __sun
    // The native memcmp is faster on SUN.
    return memcmp(s1, s2, m_size) < 0;
#else
    return my_mem_compare_longkey(s1, s2, m_size);
#endif
  }
private:
  size_t m_size;
};

template <typename type>
size_t try_reserve(std::pair<type*, ptrdiff_t> *buf, ptrdiff_t size)
{
  *buf= std::get_temporary_buffer<type>(size);
  if (buf->second != size)
  {
    std::return_temporary_buffer(buf->first);
    return 0;
  }
  return buf->second;
}

} // namespace

void Filesort_buffer::sort_buffer(const Sort_param *param, uint count)
{
  m_sort_keys= get_sort_keys();

  if (count <= 1)
    return;
  if (param->sort_length == 0)
    return;

  // For priority queue we have already reversed the pointers.
  if (!param->using_pq)
  {
    reverse_record_pointers();
  }
  std::pair<uchar**, ptrdiff_t> buffer;
  if (radixsort_is_appliccable(count, param->sort_length) &&
      try_reserve(&buffer, count))
  {
    radixsort_for_str_ptr(m_sort_keys, count, param->sort_length, buffer.first);
    std::return_temporary_buffer(buffer.first);
    return;
  }
  /*
    std::stable_sort has some extra overhead in allocating the temp buffer,
    which takes some time. The cutover point where it starts to get faster
    than quicksort seems to be somewhere around 10 to 40 records.
    So we're a bit conservative, and stay with quicksort up to 100 records.
  */
  if (count <= 100)
  {
    if (param->sort_length < 10)
    {
      std::sort(m_sort_keys, m_sort_keys + count,
                Mem_compare(param->sort_length));
      return;
    }
    std::sort(m_sort_keys, m_sort_keys + count,
              Mem_compare_longkey(param->sort_length));
    return;
  }
  // Heuristics here: avoid function overhead call for short keys.
  if (param->sort_length < 10)
  {
    std::stable_sort(m_sort_keys, m_sort_keys + count,
                     Mem_compare(param->sort_length));
    return;
  }
  std::stable_sort(m_sort_keys, m_sort_keys + count,
                   Mem_compare_longkey(param->sort_length));
}
