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


#define JAM_FILE_ID 228

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


#undef JAM_FILE_ID

#endif
