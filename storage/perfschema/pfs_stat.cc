/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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

#include "storage/perfschema/pfs_stat.h"

/**
  @file storage/perfschema/pfs_stat.cc
  Statistics (implementation).
*/

/**
  @addtogroup performance_schema_buffers
  @{
*/

void PFS_memory_safe_stat::reset() {
  m_used = false;

  m_alloc_count = 0;
  m_free_count = 0;
  m_alloc_size = 0;
  m_free_size = 0;

  m_alloc_count_capacity = 0;
  m_free_count_capacity = 0;
  m_alloc_size_capacity = 0;
  m_free_size_capacity = 0;
}

void PFS_memory_safe_stat::rebase() {
  if (!m_used) {
    return;
  }

  size_t base;

  base = std::min<size_t>(m_alloc_count, m_free_count);
  m_alloc_count -= base;
  m_free_count -= base;

  base = std::min<size_t>(m_alloc_size, m_free_size);
  m_alloc_size -= base;
  m_free_size -= base;

  m_alloc_count_capacity = 0;
  m_free_count_capacity = 0;
  m_alloc_size_capacity = 0;
  m_free_size_capacity = 0;
}

PFS_memory_stat_alloc_delta *PFS_memory_safe_stat::count_alloc(
    size_t size, PFS_memory_stat_alloc_delta *delta) {
  m_used = true;

  m_alloc_count++;
  m_free_count_capacity++;
  m_alloc_size += size;
  m_free_size_capacity += size;

  if ((m_alloc_count_capacity >= 1) && (m_alloc_size_capacity >= size)) {
    m_alloc_count_capacity--;
    m_alloc_size_capacity -= size;
    return nullptr;
  }

  if (m_alloc_count_capacity >= 1) {
    m_alloc_count_capacity--;
    delta->m_alloc_count_delta = 0;
  } else {
    delta->m_alloc_count_delta = 1;
  }

  if (m_alloc_size_capacity >= size) {
    m_alloc_size_capacity -= size;
    delta->m_alloc_size_delta = 0;
  } else {
    delta->m_alloc_size_delta = size - m_alloc_size_capacity;
    m_alloc_size_capacity = 0;
  }

  return delta;
}

PFS_memory_stat_free_delta *PFS_memory_safe_stat::count_free(
    size_t size, PFS_memory_stat_free_delta *delta) {
  m_used = true;

  m_free_count++;
  m_alloc_count_capacity++;
  m_free_size += size;
  m_alloc_size_capacity += size;

  if ((m_free_count_capacity >= 1) && (m_free_size_capacity >= size)) {
    m_free_count_capacity--;
    m_free_size_capacity -= size;
    return nullptr;
  }

  if (m_free_count_capacity >= 1) {
    m_free_count_capacity--;
    delta->m_free_count_delta = 0;
  } else {
    delta->m_free_count_delta = 1;
  }

  if (m_free_size_capacity >= size) {
    m_free_size_capacity -= size;
    delta->m_free_size_delta = 0;
  } else {
    delta->m_free_size_delta = size - m_free_size_capacity;
    m_free_size_capacity = 0;
  }

  return delta;
}

void PFS_memory_shared_stat::reset() {
  m_used = false;

  m_alloc_count = 0;
  m_free_count = 0;
  m_alloc_size = 0;
  m_free_size = 0;

  m_alloc_count_capacity = 0;
  m_free_count_capacity = 0;
  m_alloc_size_capacity = 0;
  m_free_size_capacity = 0;
}

void PFS_memory_shared_stat::rebase() {
  if (!m_used) {
    return;
  }

  size_t base;

  base = std::min<size_t>(m_alloc_count, m_free_count);
  m_alloc_count -= base;
  m_free_count -= base;

  base = std::min<size_t>(m_alloc_size, m_free_size);
  m_alloc_size -= base;
  m_free_size -= base;

  m_alloc_count_capacity = 0;
  m_free_count_capacity = 0;
  m_alloc_size_capacity = 0;
  m_free_size_capacity = 0;
}

void PFS_memory_shared_stat::count_builtin_alloc(size_t size) {
  m_used = true;

  m_alloc_count++;
  m_free_count_capacity++;
  m_alloc_size += size;
  m_free_size_capacity += size;

  size_t old_value;

  /* Optimistic */
  old_value = m_alloc_count_capacity.fetch_sub(1);

  /* Adjustment */
  if (old_value <= 0) {
    m_alloc_count_capacity++;
  }

  /* Optimistic */
  old_value = m_alloc_size_capacity.fetch_sub(size);

  /* Adjustment */
  if (old_value < size) {
    m_alloc_size_capacity = 0;
  }
}

void PFS_memory_shared_stat::count_builtin_free(size_t size) {
  m_used = true;

  m_free_count++;
  m_alloc_count_capacity++;
  m_free_size += size;
  m_alloc_size_capacity += size;

  size_t old_value;

  /* Optimistic */
  old_value = m_free_count_capacity.fetch_sub(1);

  /* Adjustment */
  if (old_value <= 0) {
    m_free_count_capacity++;
  }

  /* Optimistic */
  old_value = m_free_size_capacity.fetch_sub(size);

  /* Adjustment */
  if (old_value < size) {
    m_free_size_capacity = 0;
  }
}

PFS_memory_stat_alloc_delta *PFS_memory_shared_stat::count_alloc(
    size_t size, PFS_memory_stat_alloc_delta *delta) {
  m_used = true;

  m_alloc_count++;
  m_free_count_capacity++;
  m_alloc_size += size;
  m_free_size_capacity += size;

  if ((m_alloc_count_capacity >= 1) && (m_alloc_size_capacity >= size)) {
    m_alloc_count_capacity--;
    m_alloc_size_capacity -= size;
    return nullptr;
  }

  if (m_alloc_count_capacity >= 1) {
    m_alloc_count_capacity--;
    delta->m_alloc_count_delta = 0;
  } else {
    delta->m_alloc_count_delta = 1;
  }

  if (m_alloc_size_capacity >= size) {
    m_alloc_size_capacity -= size;
    delta->m_alloc_size_delta = 0;
  } else {
    delta->m_alloc_size_delta = size - m_alloc_size_capacity;
    m_alloc_size_capacity = 0;
  }

  return delta;
}

PFS_memory_stat_free_delta *PFS_memory_shared_stat::count_free(
    size_t size, PFS_memory_stat_free_delta *delta) {
  m_used = true;

  m_free_count++;
  m_alloc_count_capacity++;
  m_free_size += size;
  m_alloc_size_capacity += size;

  if ((m_free_count_capacity >= 1) && (m_free_size_capacity >= size)) {
    m_free_count_capacity--;
    m_free_size_capacity -= size;
    return nullptr;
  }

  if (m_free_count_capacity >= 1) {
    m_free_count_capacity--;
    delta->m_free_count_delta = 0;
  } else {
    delta->m_free_count_delta = 1;
  }

  if (m_free_size_capacity >= size) {
    m_free_size_capacity -= size;
    delta->m_free_size_delta = 0;
  } else {
    delta->m_free_size_delta = size - m_free_size_capacity;
    m_free_size_capacity = 0;
  }

  return delta;
}

PFS_memory_stat_alloc_delta *PFS_memory_shared_stat::apply_alloc_delta(
    const PFS_memory_stat_alloc_delta *delta,
    PFS_memory_stat_alloc_delta *delta_buffer) {
  size_t val;
  size_t remaining_alloc_count = 0;
  size_t remaining_alloc_size = 0;
  bool has_remaining = false;

  m_used = true;

  val = delta->m_alloc_count_delta;
  if (val > 0) {
    if (val <= m_alloc_count_capacity) {
      m_alloc_count_capacity -= val;
      remaining_alloc_count = 0;
    } else {
      remaining_alloc_count = val - m_alloc_count_capacity;
      m_alloc_count_capacity = 0;
      has_remaining = true;
    }
  }

  val = delta->m_alloc_size_delta;
  if (val > 0) {
    if (val <= m_alloc_size_capacity) {
      m_alloc_size_capacity -= val;
      remaining_alloc_size = 0;
    } else {
      remaining_alloc_size = val - m_alloc_size_capacity;
      m_alloc_size_capacity = 0;
      has_remaining = true;
    }
  }

  if (!has_remaining) {
    return nullptr;
  }

  delta_buffer->m_alloc_count_delta = remaining_alloc_count;
  delta_buffer->m_alloc_size_delta = remaining_alloc_size;
  return delta_buffer;
}

PFS_memory_stat_free_delta *PFS_memory_shared_stat::apply_free_delta(
    const PFS_memory_stat_free_delta *delta,
    PFS_memory_stat_free_delta *delta_buffer) {
  size_t val;
  size_t remaining_free_count = 0;
  size_t remaining_free_size = 0;
  bool has_remaining = false;

  m_used = true;

  val = delta->m_free_count_delta;
  if (val > 0) {
    if (val <= m_free_count_capacity) {
      m_free_count_capacity -= val;
      remaining_free_count = 0;
    } else {
      remaining_free_count = val - m_free_count_capacity;
      m_free_count_capacity = 0;
      has_remaining = true;
    }
  }

  val = delta->m_free_size_delta;
  if (val > 0) {
    if (val <= m_free_size_capacity) {
      m_free_size_capacity -= val;
      remaining_free_size = 0;
    } else {
      remaining_free_size = val - m_free_size_capacity;
      m_free_size_capacity = 0;
      has_remaining = true;
    }
  }

  if (!has_remaining) {
    return nullptr;
  }

  delta_buffer->m_free_count_delta = remaining_free_count;
  delta_buffer->m_free_size_delta = remaining_free_size;
  return delta_buffer;
}

void PFS_memory_monitoring_stat::reset() {
  m_alloc_count = 0;
  m_free_count = 0;
  m_alloc_size = 0;
  m_free_size = 0;

  m_alloc_count_capacity = 0;
  m_free_count_capacity = 0;
  m_alloc_size_capacity = 0;
  m_free_size_capacity = 0;

  m_missing_free_count_capacity = 0;
  m_missing_free_size_capacity = 0;

  m_low_count_used = 0;
  m_high_count_used = 0;
  m_low_size_used = 0;
  m_high_size_used = 0;
}

void PFS_session_all_memory_stat::reset() {
  m_controlled.reset();
  m_total.reset();
}

void PFS_session_all_memory_stat::count_controlled_alloc(size_t size) {
  m_controlled.count_alloc(size);
  m_total.count_alloc(size);
}

void PFS_session_all_memory_stat::count_uncontrolled_alloc(size_t size) {
  m_total.count_alloc(size);
}

void PFS_session_all_memory_stat::count_controlled_free(size_t size) {
  m_controlled.count_free(size);
  m_total.count_free(size);
}

void PFS_session_all_memory_stat::count_uncontrolled_free(size_t size) {
  m_total.count_free(size);
}

void PFS_memory_monitoring_stat::normalize(bool global) {
  if (m_free_count_capacity > m_missing_free_count_capacity) {
    m_free_count_capacity -= m_missing_free_count_capacity;
  } else {
    m_free_count_capacity = 0;
  }

  if (m_free_size_capacity > m_missing_free_size_capacity) {
    m_free_size_capacity -= m_missing_free_size_capacity;
  } else {
    m_free_size_capacity = 0;
  }

  const ssize_t current_count = m_alloc_count - m_free_count;
  m_low_count_used = current_count - m_free_count_capacity;
  m_high_count_used = current_count + m_alloc_count_capacity;

  const ssize_t current_size = m_alloc_size - m_free_size;
  m_low_size_used = current_size - m_free_size_capacity;
  m_high_size_used = current_size + m_alloc_size_capacity;

  if (global) {
    if (m_low_count_used < 0) {
      m_low_count_used = 0;
    }
    if (m_low_size_used < 0) {
      m_low_size_used = 0;
    }
  }
}

void memory_partial_aggregate(PFS_memory_safe_stat *from,
                              PFS_memory_shared_stat *stat) {
  if (!from->m_used) {
    return;
  }

  size_t base;

  stat->m_used = true;

  base = std::min<size_t>(from->m_alloc_count, from->m_free_count);
  if (base != 0) {
    stat->m_alloc_count += base;
    stat->m_free_count += base;
    from->m_alloc_count -= base;
    from->m_free_count -= base;
  }

  base = std::min<size_t>(from->m_alloc_size, from->m_free_size);
  if (base != 0) {
    stat->m_alloc_size += base;
    stat->m_free_size += base;
    from->m_alloc_size -= base;
    from->m_free_size -= base;
  }

  size_t tmp;

  tmp = from->m_alloc_count_capacity;
  if (tmp != 0) {
    stat->m_alloc_count_capacity += tmp;
    from->m_alloc_count_capacity = 0;
  }

  tmp = from->m_free_count_capacity;
  if (tmp != 0) {
    stat->m_free_count_capacity += tmp;
    from->m_free_count_capacity = 0;
  }

  tmp = from->m_alloc_size_capacity;
  if (tmp != 0) {
    stat->m_alloc_size_capacity += tmp;
    from->m_alloc_size_capacity = 0;
  }

  tmp = from->m_free_size_capacity;
  if (tmp != 0) {
    stat->m_free_size_capacity += tmp;
    from->m_free_size_capacity = 0;
  }
}

void memory_partial_aggregate(PFS_memory_shared_stat *from,
                              PFS_memory_shared_stat *stat) {
  if (!from->m_used) {
    return;
  }

  size_t base;

  stat->m_used = true;

  base = std::min<size_t>(from->m_alloc_count, from->m_free_count);
  if (base != 0) {
    stat->m_alloc_count += base;
    stat->m_free_count += base;
    from->m_alloc_count -= base;
    from->m_free_count -= base;
  }

  base = std::min<size_t>(from->m_alloc_size, from->m_free_size);
  if (base != 0) {
    stat->m_alloc_size += base;
    stat->m_free_size += base;
    from->m_alloc_size -= base;
    from->m_free_size -= base;
  }

  size_t tmp;

  tmp = from->m_alloc_count_capacity;
  if (tmp != 0) {
    stat->m_alloc_count_capacity += tmp;
    from->m_alloc_count_capacity = 0;
  }

  tmp = from->m_free_count_capacity;
  if (tmp != 0) {
    stat->m_free_count_capacity += tmp;
    from->m_free_count_capacity = 0;
  }

  tmp = from->m_alloc_size_capacity;
  if (tmp != 0) {
    stat->m_alloc_size_capacity += tmp;
    from->m_alloc_size_capacity = 0;
  }

  tmp = from->m_free_size_capacity;
  if (tmp != 0) {
    stat->m_free_size_capacity += tmp;
    from->m_free_size_capacity = 0;
  }
}

void memory_partial_aggregate(PFS_memory_safe_stat *from,
                              PFS_memory_shared_stat *stat1,
                              PFS_memory_shared_stat *stat2) {
  if (!from->m_used) {
    return;
  }

  size_t base;

  stat1->m_used = true;
  stat2->m_used = true;

  base = std::min<size_t>(from->m_alloc_count, from->m_free_count);
  if (base != 0) {
    stat1->m_alloc_count += base;
    stat2->m_alloc_count += base;
    stat1->m_free_count += base;
    stat2->m_free_count += base;
    from->m_alloc_count -= base;
    from->m_free_count -= base;
  }

  base = std::min<size_t>(from->m_alloc_size, from->m_free_size);
  if (base != 0) {
    stat1->m_alloc_size += base;
    stat2->m_alloc_size += base;
    stat1->m_free_size += base;
    stat2->m_free_size += base;
    from->m_alloc_size -= base;
    from->m_free_size -= base;
  }

  size_t tmp;

  tmp = from->m_alloc_count_capacity;
  if (tmp != 0) {
    stat1->m_alloc_count_capacity += tmp;
    stat2->m_alloc_count_capacity += tmp;
    from->m_alloc_count_capacity = 0;
  }

  tmp = from->m_free_count_capacity;
  if (tmp != 0) {
    stat1->m_free_count_capacity += tmp;
    stat2->m_free_count_capacity += tmp;
    from->m_free_count_capacity = 0;
  }

  tmp = from->m_alloc_size_capacity;
  if (tmp != 0) {
    stat1->m_alloc_size_capacity += tmp;
    stat2->m_alloc_size_capacity += tmp;
    from->m_alloc_size_capacity = 0;
  }

  tmp = from->m_free_size_capacity;
  if (tmp != 0) {
    stat1->m_free_size_capacity += tmp;
    stat2->m_free_size_capacity += tmp;
    from->m_free_size_capacity = 0;
  }
}

void memory_partial_aggregate(PFS_memory_shared_stat *from,
                              PFS_memory_shared_stat *stat1,
                              PFS_memory_shared_stat *stat2) {
  if (!from->m_used) {
    return;
  }

  size_t base;

  stat1->m_used = true;
  stat2->m_used = true;

  base = std::min<size_t>(from->m_alloc_count, from->m_free_count);
  if (base != 0) {
    stat1->m_alloc_count += base;
    stat2->m_alloc_count += base;
    stat1->m_free_count += base;
    stat2->m_free_count += base;
    from->m_alloc_count -= base;
    from->m_free_count -= base;
  }

  base = std::min<size_t>(from->m_alloc_size, from->m_free_size);
  if (base != 0) {
    stat1->m_alloc_size += base;
    stat2->m_alloc_size += base;
    stat1->m_free_size += base;
    stat2->m_free_size += base;
    from->m_alloc_size -= base;
    from->m_free_size -= base;
  }

  size_t tmp;

  tmp = from->m_alloc_count_capacity;
  if (tmp != 0) {
    stat1->m_alloc_count_capacity += tmp;
    stat2->m_alloc_count_capacity += tmp;
    from->m_alloc_count_capacity = 0;
  }

  tmp = from->m_free_count_capacity;
  if (tmp != 0) {
    stat1->m_free_count_capacity += tmp;
    stat2->m_free_count_capacity += tmp;
    from->m_free_count_capacity = 0;
  }

  tmp = from->m_alloc_size_capacity;
  if (tmp != 0) {
    stat1->m_alloc_size_capacity += tmp;
    stat2->m_alloc_size_capacity += tmp;
    from->m_alloc_size_capacity = 0;
  }

  tmp = from->m_free_size_capacity;
  if (tmp != 0) {
    stat1->m_free_size_capacity += tmp;
    stat2->m_free_size_capacity += tmp;
    from->m_free_size_capacity = 0;
  }
}

void memory_full_aggregate_with_reassign(const PFS_memory_safe_stat *from,
                                         PFS_memory_shared_stat *stat,
                                         PFS_memory_shared_stat *global) {
  if (!from->m_used) {
    return;
  }

  stat->m_used = true;

  const size_t alloc_count = from->m_alloc_count;
  const size_t free_count = from->m_free_count;
  const size_t alloc_size = from->m_alloc_size;
  const size_t free_size = from->m_free_size;

  size_t net;
  size_t capacity;

  if (likely(alloc_count <= free_count)) {
    /* Nominal path */
    stat->m_alloc_count += alloc_count;
    stat->m_free_count += free_count;
    stat->m_alloc_count_capacity += from->m_alloc_count_capacity;
    stat->m_free_count_capacity += from->m_free_count_capacity;
  } else {
    /*
      Global net alloc.

      Problem:
      - the instrumented memory is not flagged global,
        as we are keeping per thread statistics,
      - the running thread is now disconnecting,
        as we are aggregating the thread (PFS_memory_safe_stat) stats,
      - the running thread did contribute memory globally,
        but did not un claim it (see pfs_memory_claim_vc)

      This is an issue because:
      - the memory contributed will be accounted for the
        parent account / user / host statistics
      - the corresponding memory cleanup can be done by another
        thread, aggregating statistics in ** another **
        parent account / user / host statistics bucket
      - resulting in unbalanced alloc / free watermarks, globally.

      Solution:
      - consider that a unclaim is missing for the part contributed
      - aggregate the balanced stats to the parent bucket
      - aggregate the stats for the net gain to the global bucket directly.

      This is so that net alloc (this code) and net free
      (pfs_memory_free_vc) statistics balance themselves using the unique
      and therefore same global bucket.
    */

    global->m_used = true;

    stat->m_alloc_count += free_count; /* base */
    stat->m_free_count += free_count;

    /* Net memory contributed affected to the global bucket directly. */
    net = alloc_count - free_count;
    global->m_alloc_count += net;

    stat->m_alloc_count_capacity += from->m_alloc_count_capacity;

    size_t free_count_capacity;
    free_count_capacity = from->m_free_count_capacity;
    capacity = std::min(free_count_capacity, net);
    free_count_capacity -= capacity;

    /*
      Corresponding low watermark split between the parent and global
      bucket.
    */
    stat->m_free_count_capacity += free_count_capacity;
    global->m_free_count_capacity += capacity;
  }

  if (likely(alloc_size <= free_size)) {
    /* Nominal path. */
    stat->m_alloc_size += alloc_size;
    stat->m_free_size += free_size;
    stat->m_alloc_size_capacity += from->m_alloc_size_capacity;
    stat->m_free_size_capacity += from->m_free_size_capacity;
  } else {
    /* Global net alloc. */

    global->m_used = true;

    stat->m_alloc_size += free_size; /* base */
    stat->m_free_size += free_size;

    net = alloc_size - free_size;
    global->m_alloc_size += net;

    stat->m_alloc_size_capacity += from->m_alloc_size_capacity;

    size_t free_size_capacity;
    free_size_capacity = from->m_free_size_capacity;
    capacity = std::min(free_size_capacity, net);
    free_size_capacity -= capacity;

    stat->m_free_size_capacity += free_size_capacity;
    global->m_free_size_capacity += capacity;
  }
}

void memory_full_aggregate_with_reassign(const PFS_memory_safe_stat *from,
                                         PFS_memory_shared_stat *stat1,
                                         PFS_memory_shared_stat *stat2,
                                         PFS_memory_shared_stat *global) {
  if (!from->m_used) {
    return;
  }

  stat1->m_used = true;
  stat2->m_used = true;

  const size_t alloc_count = from->m_alloc_count;
  const size_t free_count = from->m_free_count;
  const size_t alloc_size = from->m_alloc_size;
  const size_t free_size = from->m_free_size;

  size_t tmp;
  size_t net;
  size_t capacity;

  if (likely(alloc_count <= free_count)) {
    /* Nominal path */
    stat1->m_alloc_count += alloc_count;
    stat2->m_alloc_count += alloc_count;
    stat1->m_free_count += free_count;
    stat2->m_free_count += free_count;

    tmp = from->m_alloc_count_capacity;
    stat1->m_alloc_count_capacity += tmp;
    stat2->m_alloc_count_capacity += tmp;

    tmp = from->m_free_count_capacity;
    stat1->m_free_count_capacity += tmp;
    stat2->m_free_count_capacity += tmp;
  } else {
    /* Global net alloc. */

    global->m_used = true;

    stat1->m_alloc_count += free_count; /* base */
    stat2->m_alloc_count += free_count; /* base */
    stat1->m_free_count += free_count;
    stat2->m_free_count += free_count;

    net = alloc_count - free_count;
    global->m_alloc_count += net;

    tmp = from->m_alloc_count_capacity;
    stat1->m_alloc_count_capacity += tmp;
    stat2->m_alloc_count_capacity += tmp;

    size_t free_count_capacity;
    free_count_capacity = from->m_free_count_capacity;
    capacity = std::min(free_count_capacity, net);
    free_count_capacity -= capacity;

    stat1->m_free_count_capacity += free_count_capacity;
    stat2->m_free_count_capacity += free_count_capacity;
    global->m_free_count_capacity += capacity;
  }

  if (likely(alloc_size <= free_size)) {
    /* Nominal path. */
    stat1->m_alloc_size += alloc_size;
    stat2->m_alloc_size += alloc_size;
    stat1->m_free_size += free_size;
    stat2->m_free_size += free_size;

    tmp = from->m_alloc_size_capacity;
    stat1->m_alloc_size_capacity += tmp;
    stat2->m_alloc_size_capacity += tmp;

    tmp = from->m_free_size_capacity;
    stat1->m_free_size_capacity += tmp;
    stat2->m_free_size_capacity += tmp;
  } else {
    /* Global net alloc. */

    global->m_used = true;

    stat1->m_alloc_size += free_size; /* base */
    stat2->m_alloc_size += free_size; /* base */
    stat1->m_free_size += free_size;
    stat2->m_free_size += free_size;

    net = alloc_size - free_size;
    global->m_alloc_size += net;

    tmp = from->m_alloc_size_capacity;
    stat1->m_alloc_size_capacity += tmp;
    stat2->m_alloc_size_capacity += tmp;

    size_t free_size_capacity;
    free_size_capacity = from->m_free_size_capacity;
    capacity = std::min(free_size_capacity, net);
    free_size_capacity -= capacity;

    stat1->m_free_size_capacity += free_size_capacity;
    stat2->m_free_size_capacity += free_size_capacity;
    global->m_free_size_capacity += capacity;
  }
}

void memory_full_aggregate(const PFS_memory_safe_stat *from,
                           PFS_memory_shared_stat *stat) {
  if (!from->m_used) {
    return;
  }

  stat->m_used = true;

  stat->m_alloc_count += from->m_alloc_count;
  stat->m_free_count += from->m_free_count;
  stat->m_alloc_size += from->m_alloc_size;
  stat->m_free_size += from->m_free_size;

  stat->m_alloc_count_capacity += from->m_alloc_count_capacity;
  stat->m_free_count_capacity += from->m_free_count_capacity;
  stat->m_alloc_size_capacity += from->m_alloc_size_capacity;
  stat->m_free_size_capacity += from->m_free_size_capacity;
}

void memory_full_aggregate(const PFS_memory_shared_stat *from,
                           PFS_memory_shared_stat *stat) {
  if (!from->m_used) {
    return;
  }

  stat->m_used = true;

  stat->m_alloc_count += from->m_alloc_count;
  stat->m_free_count += from->m_free_count;
  stat->m_alloc_size += from->m_alloc_size;
  stat->m_free_size += from->m_free_size;

  stat->m_alloc_count_capacity += from->m_alloc_count_capacity;
  stat->m_free_count_capacity += from->m_free_count_capacity;
  stat->m_alloc_size_capacity += from->m_alloc_size_capacity;
  stat->m_free_size_capacity += from->m_free_size_capacity;
}

void memory_full_aggregate(const PFS_memory_shared_stat *from,
                           PFS_memory_shared_stat *stat1,
                           PFS_memory_shared_stat *stat2) {
  if (!from->m_used) {
    return;
  }

  stat1->m_used = true;
  stat2->m_used = true;

  size_t tmp;

  tmp = from->m_alloc_count;
  stat1->m_alloc_count += tmp;
  stat2->m_alloc_count += tmp;

  tmp = from->m_free_count;
  stat1->m_free_count += tmp;
  stat2->m_free_count += tmp;

  tmp = from->m_alloc_size;
  stat1->m_alloc_size += tmp;
  stat2->m_alloc_size += tmp;

  tmp = from->m_free_size;
  stat1->m_free_size += tmp;
  stat2->m_free_size += tmp;

  tmp = from->m_alloc_count_capacity;
  stat1->m_alloc_count_capacity += tmp;
  stat2->m_alloc_count_capacity += tmp;

  tmp = from->m_free_count_capacity;
  stat1->m_free_count_capacity += tmp;
  stat2->m_free_count_capacity += tmp;

  tmp = from->m_alloc_size_capacity;
  stat1->m_alloc_size_capacity += tmp;
  stat2->m_alloc_size_capacity += tmp;

  tmp = from->m_free_size_capacity;
  stat1->m_free_size_capacity += tmp;
  stat2->m_free_size_capacity += tmp;
}

void memory_monitoring_aggregate(const PFS_memory_safe_stat *from,
                                 PFS_memory_monitoring_stat *stat) {
  if (!from->m_used) {
    return;
  }

  const size_t alloc_count = from->m_alloc_count;
  const size_t free_count = from->m_free_count;
  const size_t alloc_size = from->m_alloc_size;
  const size_t free_size = from->m_free_size;

  stat->m_alloc_count += alloc_count;
  stat->m_free_count += free_count;
  stat->m_alloc_size += alloc_size;
  stat->m_free_size += free_size;

  stat->m_alloc_count_capacity += from->m_alloc_count_capacity;
  stat->m_free_count_capacity += from->m_free_count_capacity;
  stat->m_alloc_size_capacity += from->m_alloc_size_capacity;
  stat->m_free_size_capacity += from->m_free_size_capacity;

  if (alloc_count < free_count) {
    stat->m_missing_free_count_capacity += (free_count - alloc_count);
  }

  if (alloc_size < free_size) {
    stat->m_missing_free_size_capacity += (free_size - alloc_size);
  }
}

void memory_monitoring_aggregate(const PFS_memory_shared_stat *from,
                                 PFS_memory_monitoring_stat *stat) {
  if (!from->m_used) {
    return;
  }

  const size_t alloc_count = from->m_alloc_count;
  const size_t free_count = from->m_free_count;
  const size_t alloc_size = from->m_alloc_size;
  const size_t free_size = from->m_free_size;

  stat->m_alloc_count += from->m_alloc_count;
  stat->m_free_count += from->m_free_count;
  stat->m_alloc_size += from->m_alloc_size;
  stat->m_free_size += from->m_free_size;

  stat->m_alloc_count_capacity += from->m_alloc_count_capacity;
  stat->m_free_count_capacity += from->m_free_count_capacity;
  stat->m_alloc_size_capacity += from->m_alloc_size_capacity;
  stat->m_free_size_capacity += from->m_free_size_capacity;

  if (alloc_count < free_count) {
    stat->m_missing_free_count_capacity += (free_count - alloc_count);
  }

  if (alloc_size < free_size) {
    stat->m_missing_free_size_capacity += (free_size - alloc_size);
  }
}

/** @} */
