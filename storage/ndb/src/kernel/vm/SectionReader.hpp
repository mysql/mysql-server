/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef SECTION_READER_HPP
#define SECTION_READER_HPP

#include <ndb_types.h>

#define JAM_FILE_ID 219

class SectionReader {
 public:
  SectionReader(struct SegmentedSectionPtr &, class SectionSegmentPool &);
  SectionReader(Uint32 firstSectionIVal, class SectionSegmentPool &);

  /* reset : Set SectionReader to start of section */
  void reset();
  /* step : Step over given number of words */
  bool step(Uint32 len);
  /* getWord : Copy one word to dst + move forward */
  bool getWord(Uint32 *dst);
  /* peekWord : Copy one word to dst */
  bool peekWord(Uint32 *dst) const;
  /* peekWords : Copy len words to dst */
  bool peekWords(Uint32 *dst, Uint32 len) const;
  /* getSize : Get total size of section */
  Uint32 getSize() const;
  /* getWords : Copy len words to dst + move forward */
  bool getWords(Uint32 *dst, Uint32 len);

  /* getWordsPtr : Get const ptr to next contiguous
   *               block of words
   * In success case will return at least 1 word
   */
  bool getWordsPtr(const Uint32 *&readPtr, Uint32 &actualLen);
  /* getWordsPtr : Get const ptr to at most maxLen words
   * In success case will return at least 1 word
   */
  bool getWordsPtr(Uint32 maxLen, const Uint32 *&readPtr, Uint32 &actualLen);

  /* PosInfo
   * Structure for efficiently saving/restoring a SectionReader
   * to a position
   * Must be treated as opaque and never 'mippled' with!
   */
  struct PosInfo {
    Uint32 currPos;
    Uint32 currIVal;
  };

  PosInfo getPos();
  bool setPos(PosInfo posinfo);

  /**
   * Update word at current position to <em>value</em>
   */
  bool updateWord(Uint32 value) const;

 private:
  Uint32 m_pos;
  Uint32 m_len;
  class SectionSegmentPool &m_pool;
  Uint32 m_headI;
  struct SectionSegment *m_head;
  Uint32 m_currI;
  struct SectionSegment *m_currentSegment;

  bool segmentContainsPos(PosInfo posInfo);
};

inline Uint32 SectionReader::getSize() const { return m_len; }

#undef JAM_FILE_ID

#endif
