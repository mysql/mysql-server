/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
  Uint32 preComputedWord1;
  Uint32 checksumUsed;     // Checksum shall be included in the message
  Uint32 signalIdUsed;     // Senders signal id shall be included in the message
public:
  Packer(bool signalId, bool checksum);
  
  Uint32 getMessageLength(const SignalHeader* header, 
			  const LinearSectionPtr ptr[3]) const ;


  Uint32 getMessageLength(const SignalHeader* header, 
			  const SegmentedSectionPtr ptr[3]) const ;


  Uint32 getMessageLength(const SignalHeader* header, 
			  const GenericSectionPtr ptr[3]) const ;
  
  void pack(Uint32 * insertPtr, 
	    Uint32 prio, 
	    const SignalHeader* header, 
	    const Uint32* data,
	    const LinearSectionPtr ptr[3]) const ;

  void pack(Uint32 * insertPtr, 
	    Uint32 prio, 
	    const SignalHeader* header, 
	    const Uint32* data,
	    class SectionSegmentPool & thePool,
	    const SegmentedSectionPtr ptr[3]) const ;

  void pack(Uint32 * insertPtr, 
	    Uint32 prio, 
	    const SignalHeader* header, 
	    const Uint32* data,
	    const GenericSectionPtr ptr[3]) const ;
};

inline
Uint32
Packer::getMessageLength(const SignalHeader* header,
			 const LinearSectionPtr ptr[3]) const {
  Uint32 tLen32 = header->theLength;
  Uint32 no_seg = header->m_noOfSections; 
  tLen32 += checksumUsed;
  tLen32 += signalIdUsed;
  tLen32 += no_seg;

  for(Uint32 i = 0; i<no_seg; i++){
    tLen32 += ptr[i].sz;
  }
  
  return (tLen32 * 4) + sizeof(Protocol6);
}

inline
Uint32
Packer::getMessageLength(const SignalHeader* header,
			 const SegmentedSectionPtr ptr[3]) const {
  Uint32 tLen32 = header->theLength;
  Uint32 no_seg = header->m_noOfSections; 
  tLen32 += checksumUsed;
  tLen32 += signalIdUsed;
  tLen32 += no_seg;
  
  for(Uint32 i = 0; i<no_seg; i++){
    tLen32 += ptr[i].sz;
  }
  
  return (tLen32 * 4) + sizeof(Protocol6);
}

inline
Uint32
Packer::getMessageLength(const SignalHeader* header,
			 const GenericSectionPtr ptr[3]) const {
  Uint32 tLen32 = header->theLength;
  Uint32 no_seg = header->m_noOfSections; 
  tLen32 += checksumUsed;
  tLen32 += signalIdUsed;
  tLen32 += no_seg;

  for(Uint32 i = 0; i<no_seg; i++){
    tLen32 += ptr[i].sz;
  }
  
  return (tLen32 * 4) + sizeof(Protocol6);
}

#endif
