/* Copyright 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef NDB_LS_IMPL_HPP
#define NDB_LS_IMPL_HPP

#include "LongSignal.hpp"

#ifdef NDBD_MULTITHREADED
#include "mt.hpp"
#define SPC_ARG SectionSegmentPool::Cache& cache,
#define SPC_SEIZE_ARG f_section_lock, cache,
#define SPC_CACHE_ARG cache,
static
SectionSegmentPool::LockFun
f_section_lock =
{
  mt_section_lock,
  mt_section_unlock
};
#else
#define SPC_ARG
#define SPC_SEIZE_ARG
#define SPC_CACHE_ARG
#endif

/* Calculate number of segments to release based on section size
 * Always release one segment, even if size is zero
 */
#define relSz(x) ((x == 0)? 1 : ((x + SectionSegment::DataLength - 1) / SectionSegment::DataLength))

bool import(SPC_ARG Ptr<SectionSegment> & first, const Uint32 * src, Uint32 len);

/* appendToSection : If firstSegmentIVal == RNIL, import */
bool appendToSection(SPC_ARG Uint32& firstSegmentIVal, const Uint32* src, Uint32 len);
/* dupSection : Create new section as copy of src section */
bool dupSection(SPC_ARG Uint32& copyFirstIVal, Uint32 srcFirstIVal);
/* writeToSection : Overwrite section from offset with data.  */
bool writeToSection(Uint32 firstSegmentIVal, Uint32 offset, const Uint32* src, Uint32 len);

void release(SPC_ARG SegmentedSectionPtr & ptr);
void releaseSection(SPC_ARG Uint32 firstSegmentIVal);

#endif
