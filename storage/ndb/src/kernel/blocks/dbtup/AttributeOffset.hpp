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

#ifndef ATTRIBUTE_OFFSET_HPP
#define ATTRIBUTE_OFFSET_HPP

#define JAM_FILE_ID 425

class AttributeOffset {
  friend class Dbtup;

 public:
  static void setOffset(Uint32 &desc, Uint32 offset);
  static void setCharsetPos(Uint32 &desc, Uint32 offset);
  static void setNullFlagPos(Uint32 &desc, Uint32 offset);

  static Uint32 getOffset(const Uint32 &);
  static bool getCharsetFlag(const Uint32 &);
  static Uint32 getCharsetPos(const Uint32 &);
  static Uint32 getNullFlagPos(const Uint32 &);
  static Uint32 getNullFlagOffset(const Uint32 &);
  static Uint32 getNullFlagByteOffset(const Uint32 &desc);
  static Uint32 getNullFlagBitOffset(const Uint32 &);

  static Uint32 getMaxOffset();

  Uint32 m_data;

  friend class NdbOut &operator<<(class NdbOut &, const AttributeOffset &);
};

/**
 * Allow for 4096 attributes, all nullable, and for 128 different
 * character sets.
 *
 * a = Attribute offset         - 11 bits  0-10 ( addr word in 8 kb )
 * c = Has charset flag           1  bits 11-11
 * s = Charset pointer position - 7  bits 12-18 ( in table descriptor )
 * f = Null flag offset in word - 5  bits 20-24 ( address 32 bits )
 * w = Null word offset         - 7  bits 25-31 ( f+w addr 4096 attrs )
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * aaaaaaaaaaacsssssss fffffwwwwwww
 */

#define AO_ATTRIBUTE_OFFSET_SHIFT 0
#define AO_ATTRIBUTE_OFFSET_MASK 0x7ff

#define AO_CHARSET_FLAG_SHIFT 11
#define AO_CHARSET_POS_SHIFT 12
#define AO_CHARSET_POS_MASK 127

#define AO_NULL_FLAG_POS_MASK 0xfff  // f+w
#define AO_NULL_FLAG_POS_SHIFT 20

#define AO_NULL_FLAG_WORD_MASK 31  // f
#define AO_NULL_FLAG_OFFSET_SHIFT 5
#define AO_NULL_FLAG_BYTE_OFFSET_SHIFT 3

inline void AttributeOffset::setOffset(Uint32 &desc, Uint32 offset) {
  ASSERT_MAX(offset, AO_ATTRIBUTE_OFFSET_MASK, "AttributeOffset::setOffset");
  desc &= ~(Uint32)(AO_ATTRIBUTE_OFFSET_MASK << AO_ATTRIBUTE_OFFSET_SHIFT);
  desc |= (offset << AO_ATTRIBUTE_OFFSET_SHIFT);
}

inline void AttributeOffset::setCharsetPos(Uint32 &desc, Uint32 offset) {
  ASSERT_MAX(offset, AO_CHARSET_POS_MASK, "AttributeOffset::setCharsetPos");
  desc |= (1 << AO_CHARSET_FLAG_SHIFT);
  desc |= (offset << AO_CHARSET_POS_SHIFT);
}

inline void AttributeOffset::setNullFlagPos(Uint32 &desc, Uint32 pos) {
  ASSERT_MAX(pos, AO_NULL_FLAG_POS_MASK, "AttributeOffset::setNullFlagPos");
  desc |= (pos << AO_NULL_FLAG_POS_SHIFT);
}

inline Uint32 AttributeOffset::getOffset(const Uint32 &desc) {
  return (desc >> AO_ATTRIBUTE_OFFSET_SHIFT) & AO_ATTRIBUTE_OFFSET_MASK;
}

inline bool AttributeOffset::getCharsetFlag(const Uint32 &desc) {
  return (desc >> AO_CHARSET_FLAG_SHIFT) & 1;
}

inline Uint32 AttributeOffset::getCharsetPos(const Uint32 &desc) {
  return (desc >> AO_CHARSET_POS_SHIFT) & AO_CHARSET_POS_MASK;
}

inline Uint32 AttributeOffset::getNullFlagPos(const Uint32 &desc) {
  return ((desc >> AO_NULL_FLAG_POS_SHIFT) & AO_NULL_FLAG_POS_MASK);
}

/* Offset of NULL bit in 32-bit words. */
inline Uint32 AttributeOffset::getNullFlagOffset(const Uint32 &desc) {
  return (getNullFlagPos(desc) >> AO_NULL_FLAG_OFFSET_SHIFT);
}

/* Offset of NULL bit in bytes. */
inline Uint32 AttributeOffset::getNullFlagByteOffset(const Uint32 &desc) {
  return (getNullFlagPos(desc) >> AO_NULL_FLAG_BYTE_OFFSET_SHIFT);
}

inline Uint32 AttributeOffset::getNullFlagBitOffset(const Uint32 &desc) {
  return (getNullFlagPos(desc) & AO_NULL_FLAG_WORD_MASK);
}

inline Uint32 AttributeOffset::getMaxOffset() {
  return AO_ATTRIBUTE_OFFSET_MASK;
}

class NdbOut &operator<<(class NdbOut &, const AttributeOffset &);

#undef JAM_FILE_ID

#endif
