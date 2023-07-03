/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef NDBAPI_ASSEMBLE_FRAGMENTS_HPP
#define NDBAPI_ASSEMBLE_FRAGMENTS_HPP

#include <cstring>

#include "ndb_global.h"
#include "ndb_types.h"
#include "NdbApiSignal.hpp"

/** AssembleBatchedFragments
 *  ========================
 *
 * When one need to send a long signal that is too big for a Protocol6 message
 * one can send it fragmented.
 *
 * The section data is split up and several smaller long signals are sent
 * and receiver needs to assemble the fragments back to a big long signal
 * before proceessing.
 *
 * One notable property is that sending signal fragmented may change signal
 * order since receiver will receive and process signals sent after first
 * fragment and before last fragment is sent before it has received the last
 * fragment and can process the big long signal.
 *
 * To prevent signal reordering one can choose to send all fragments
 * back-to-back without interleaving with other signals.
 * AssembleBatchedFragments is a helper class for ndbapi to receive signals
 * sent as batched fragments.
 *
 */
class AssembleBatchedFragments
{
public:
  enum Result : int {
    /* Message complete, no fragments use signal as is */
    MESSAGE_OK,
    /* Need to allocate memory for section buffers.
     * call setup() followed by assemble() again.
     */
    NEED_SETUP,
    /* Fragment processed, need more fragments */
    NEED_MORE,
    /* Fragmented signals now completely assemble.
     * Call extract() and cleanup().
     */
    MESSAGE_COMPLETE,
    /* For all error codes below.
     * Current signal/fragment does not fit the batched fragments in progress.
     * Call extract_signal_only() to get broken signal.
     * Then call assemble() again with current fragment.
     */
    ERR_BATCH_IN_PROGRESS,
    ERR_DATA_DROPPED,
    ERR_MESSAGE_INCOMPLETE,
    ERR_OUT_OF_SYNC,
    ERR_BAD_SIGNAL,
  };

  AssembleBatchedFragments();
  ~AssembleBatchedFragments();

  bool is_in_progress() const;
  Result assemble(const NdbApiSignal* signal, const LinearSectionPtr ptr[3]);
  bool setup(Uint32 size);
  Uint32 extract(NdbApiSignal* signal, LinearSectionPtr ptr[3]) const;
  void cleanup();
  void extract_signal_only(NdbApiSignal* signal);

private:
  Result do_assemble(const NdbApiSignal* signal,
                     const LinearSectionPtr ptr[3]);

  // Signal data for first fragment
  SignalHeader m_sigheader;
  Uint32 m_theData[25];

  // Key for fragments of same message
  Uint32 m_sender_ref;
  Uint32 m_fragment_id;

  // Section buffer
  Uint32* m_section_memory;
  Uint32 m_size;
  Uint32 m_offset;

  // Assembled sections
  Uint32 m_section_count;
  Uint32 m_section_offset[3];
  Uint32 m_section_size[3];
};

inline bool AssembleBatchedFragments::is_in_progress() const
{
  return m_section_memory != nullptr;
}

inline AssembleBatchedFragments::Result
AssembleBatchedFragments::assemble(const NdbApiSignal* signal,
                                   const LinearSectionPtr ptr[3])
{
  const bool in_progress = (m_section_memory != nullptr);

  if (!signal->isFragmented())
  {
    if (unlikely(in_progress))
    {
      // Drop section data
      cleanup();
      return Result::ERR_BATCH_IN_PROGRESS;
    }
    return Result::MESSAGE_OK;
  }

  if (in_progress)
  {
    if (signal->isFirstFragment() && m_sender_ref == 0)
    {}
    else if (signal->isFirstFragment() ||
             m_sender_ref != signal->theSendersBlockRef ||
             m_fragment_id != signal->getFragmentId())
    {
      // Drop section data
      cleanup();
      return Result::ERR_BATCH_IN_PROGRESS;
    }
  }
  else if (signal->isFirstFragment())
  {
    return Result::NEED_SETUP;
  }
  else
  {
    return Result::ERR_OUT_OF_SYNC;
  }

  return do_assemble(signal, ptr);
}

#endif
