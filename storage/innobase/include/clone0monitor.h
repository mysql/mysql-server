/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/clone0monitor.h
 Performance Schema stage instrumentation to monitor clone progress.

 ****************************************************************************/

#ifndef CLONE_MONITOR_H
#define CLONE_MONITOR_H

/* mysql_stage_inc_work_completed */
#include "mysql/psi/mysql_stage.h"
#include "ut0dbg.h"

#ifdef HAVE_PSI_STAGE_INTERFACE

/** Class used to report CLONE progress via Performance Schema. */
class Clone_Monitor {
 public:
  /** Constructor */
  Clone_Monitor()
      : m_estimate(),
        m_work_done(),
        m_progress(),
        m_work_bytes_left(),
        m_cur_phase(NOT_STARTED) {
    m_pfs_data_chunk_size = ut_2_exp(PFS_DATA_CHUNK_SIZE_POW2);
  }

  /** Destructor. */
  ~Clone_Monitor() {
    if (m_progress == nullptr) {
      return;
    }

    mysql_end_stage();
  }

  /** Initialize all monitoring data.
  @param[in]	key	key to register stage event with performance
  schema. */
  void init_state(int key) {
    change_phase();
    m_progress = nullptr;
    m_estimate = 0;
    m_work_done = 0;
    m_work_bytes_left = 0;
    m_cur_phase = ESTIMATE_WORK;

    if (key != s_invalid_key) {
      m_progress = mysql_set_stage(key);
    }
  }

  /** Update the work estimated for the clone operation.
  @param[in]	size	size in bytes that needs to transferred
  across. */
  void add_estimate(ib_uint64_t size) {
    ut_ad(m_cur_phase == ESTIMATE_WORK);
    ut_ad(m_progress != nullptr);

    m_estimate += convert_bytes_to_work(size);

    mysql_stage_set_work_estimated(m_progress, m_estimate);
  }

  /** Update the progress of the clone operation.
  param[in]	size	size in bytes that is being transferred
  across. */
  void update_work(uint size) {
    ut_ad(m_cur_phase == COMPLETE_WORK);
    ut_ad(m_progress != nullptr);

    m_work_done += convert_bytes_to_work(size);

    mysql_stage_set_work_completed(m_progress, m_work_done);
  }

  /** Change from one phase to the other. */
  void change_phase() {
    switch (m_cur_phase) {
      case NOT_STARTED:
        return;

      case ESTIMATE_WORK:
        if (m_work_bytes_left != 0) {
          m_work_bytes_left = 0;
          m_estimate++;
          mysql_stage_set_work_estimated(m_progress, m_estimate);
        }

        m_cur_phase = COMPLETE_WORK;
        break;

      case COMPLETE_WORK:
        if (m_work_bytes_left != 0) {
          m_work_bytes_left = 0;
          if (m_work_done != m_estimate) {
            m_work_done++;
          }
          mysql_stage_set_work_completed(m_progress, m_work_done);
        }

        m_cur_phase = NOT_STARTED;
        break;
    }
  }

  /* Invalid PFS key */
  static const int s_invalid_key = -1;

 private:
  /** Translate bytes to  work unit.
  @param[in]	size	size in bytes that needs to be converted to the
  corresponding work unit.
  @return the number of PFS chunks that the size constitutes. */
  uint convert_bytes_to_work(ib_uint64_t size) {
    uint aligned_size;
    int chunks;

    size += m_work_bytes_left;
    aligned_size =
        static_cast<uint>(ut_uint64_align_down(size, m_pfs_data_chunk_size));
    chunks = aligned_size >> PFS_DATA_CHUNK_SIZE_POW2;

    m_work_bytes_left = static_cast<uint>(size) - aligned_size;

    return chunks;
  }

  /* Number of PFS chunks which needs to be transferred across.  */
  uint m_estimate;

  /* Number of PFS chunks already transferred. */
  uint m_work_done;

  /* Performance schema accounting object. */
  PSI_stage_progress *m_progress;

  /* Size in bytes which couldn't fit the chunk size during the
  estimation or the transfer. */
  uint m_work_bytes_left;

  /* Current phase. */
  enum { NOT_STARTED = 0, ESTIMATE_WORK, COMPLETE_WORK } m_cur_phase;

  /* PFS Chunk size in power of 2 in unit of bytes. */
  static const int PFS_DATA_CHUNK_SIZE_POW2 = 20;

  /* PFS chunk size. */
  uint m_pfs_data_chunk_size;
};

#endif /* HAVE_PSI_STAGE_INTERFACE */
#endif /* CLONE_MONITOR_H */
