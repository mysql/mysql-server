/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PACKER_HPP
#define PACKER_HPP

#include <TransporterDefinitions.hpp>
#include "TransporterInternalDefinitions.hpp"

#ifdef WORDS_BIGENDIAN
  #define MY_OWN_BYTE_ORDER 1
#else
  #define MY_OWN_BYTE_ORDER 0
#endif

class Packer {
private: 
  Uint32 preComputedWord1;
  const Uint32 checksumUsed;   // Checksum shall be included in the message
  const Uint32 signalIdUsed;   // Senders signal id shall be included in the message

public:
  Packer(bool signalId, bool checksum);

  template <typename AnySectionPtr>
  Uint32 getMessageLength(const SignalHeader* header, 
			  const AnySectionPtr ptr[3]) const;

  template <typename AnySectionArg>
  void pack(Uint32 * insertPtr, 
	    Uint32 prio, 
	    const SignalHeader* header, 
	    const Uint32* data,
	    AnySectionArg section) const;

  /**
   * Below we define the variants of 'AnySectionArg' which may
   * be used to call the templated ::pack(). Required as the
   * SegmentedSection variant also need the extra 'Pool' parameter,
   * and the C++ 11 'variadic template' feature cant be used yet.
   */
  class LinearSectionArg
  {
  public:
    friend class Packer;
    LinearSectionArg(const LinearSectionPtr ptr[3]) : m_ptr(ptr) {};
    const LinearSectionPtr *m_ptr;
  };

  class GenericSectionArg
  {
  public:
    friend class Packer;
    GenericSectionArg(const GenericSectionPtr ptr[3]) : m_ptr(ptr) {};
    const GenericSectionPtr *m_ptr;
  };

  class SegmentedSectionArg
  {
  public:
    friend class Packer;
    SegmentedSectionArg(class SectionSegmentPool &pool,
                        const SegmentedSectionPtr ptr[3]) : m_pool(pool), m_ptr(ptr) {};

    class SectionSegmentPool &m_pool;
    const SegmentedSectionPtr *m_ptr;
  };
};

template <typename AnySectionPtr>
inline
Uint32
Packer::getMessageLength(const SignalHeader* header,
			 const AnySectionPtr ptr[3]) const
{
  Uint32 tLen32 = header->theLength;
  const Uint32 no_seg = header->m_noOfSections; 
  tLen32 += checksumUsed;
  tLen32 += signalIdUsed;
  tLen32 += no_seg;

  for(Uint32 i = 0; i<no_seg; i++){
    tLen32 += ptr[i].sz;
  }
  
  return (tLen32 * 4) + sizeof(Protocol6);
}


#endif
