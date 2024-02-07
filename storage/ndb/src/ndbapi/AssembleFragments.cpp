/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstring>
#include <new>
#include "util/require.h"

#include "AssembleFragments.hpp"
#include "NdbApiSignal.hpp"
#include "ndb_global.h"
#include "ndb_types.h"

AssembleBatchedFragments::AssembleBatchedFragments()
    : m_sender_ref(0), m_section_memory(nullptr) {}

AssembleBatchedFragments::~AssembleBatchedFragments() {
  delete[] m_section_memory;
}

bool AssembleBatchedFragments::setup(Uint32 size) {
  require(m_section_memory == nullptr);
  m_section_memory = new (std::nothrow) Uint32[size];
  m_size = size;
  return m_section_memory != nullptr;
}

/** extract fills in the assembled signal and its sections into signal and
 * ptr and returns the number of sections.
 */
Uint32 AssembleBatchedFragments::extract(NdbApiSignal *signal,
                                         LinearSectionPtr ptr[3]) const {
  NdbApiSignal sig{m_sigheader};
  sig.setDataPtr(sig.getDataPtrSend());
  std::memcpy(sig.getDataPtrSend(), m_theData, sig.theLength * 4);

  *signal = sig;
  signal->m_noOfSections = m_section_count;

  Uint32 *p = m_section_memory;
  Uint32 sec_idx;
  Uint32 sec_cnt = 0;
  for (sec_idx = 0; sec_idx < 3; sec_idx++) {
    ptr[sec_idx].p = p + m_section_offset[sec_idx];
    ptr[sec_idx].sz = m_section_size[sec_idx];
    if (ptr[sec_idx].sz > 0) {
      sec_cnt = sec_idx + 1;
    }
  }
  signal->m_noOfSections = sec_cnt;
  return signal->m_noOfSections;
}

void AssembleBatchedFragments::cleanup() {
  require(m_section_memory != nullptr);
  delete[] m_section_memory;
  m_section_memory = nullptr;
  m_size = 0;
  m_sender_ref = 0;
}

void AssembleBatchedFragments::extract_signal_only(NdbApiSignal *signal) {
  require(m_section_memory == nullptr);

  NdbApiSignal sig{m_sigheader};
  sig.setDataPtr(sig.getDataPtrSend());
  std::memcpy(sig.getDataPtrSend(), m_theData, sig.theLength * 4);

  *signal = sig;
  signal->m_noOfSections = m_section_count;
}

AssembleBatchedFragments::Result AssembleBatchedFragments::do_assemble(
    const NdbApiSignal *signal, const LinearSectionPtr ptr[3]) {
  if (signal->isFirstFragment()) {
    m_sigheader = *signal;  // slice intended
    std::memcpy(m_theData, signal->getDataPtr(), signal->theLength * 4);
    m_sigheader.theLength = signal->theLength - signal->m_noOfSections - 1;
    m_sigheader.m_noOfSections = 0;
    m_sender_ref = signal->theSendersBlockRef;
    m_fragment_id = signal->getFragmentId();
    m_offset = 0;
    m_section_count = 0;
    for (int i = 0; i < 3; i++) {
      m_section_offset[i] = 0;
      m_section_size[i] = 0;
    }
  }

  const Uint32 sec_cnt = signal->m_noOfSections;
  for (Uint32 sec_idx = 0; sec_idx < sec_cnt; sec_idx++) {
    const Uint32 sec_num = signal->getFragmentSectionNumber(sec_idx);

    require(sec_num < 3);
    if (m_size - m_offset < ptr[sec_idx].sz) {
      // Drop collected section data
      cleanup();
      return Result::ERR_DATA_DROPPED;  // No space left
    }
    if (m_section_size[sec_num] == 0) {
      require(m_section_offset[sec_num] == 0);
      m_section_offset[sec_num] = m_offset;
    }
    std::memcpy(m_section_memory + m_offset, ptr[sec_idx].p,
                ptr[sec_idx].sz * 4);
    m_offset += ptr[sec_idx].sz;
    m_section_size[sec_num] += ptr[sec_idx].sz;
  }
  if (!signal->isLastFragment()) {
    return Result::NEED_MORE;
  }
  if (m_offset != m_size) {
    // Drop collected section data
    cleanup();
    return Result::ERR_MESSAGE_INCOMPLETE;
  }
  return Result::MESSAGE_COMPLETE;
}
