/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ATTRIBUTE_OFFSET_HPP
#define ATTRIBUTE_OFFSET_HPP

class AttributeOffset {
  friend class Dbtup;
  
private:
  static void   setOffset(Uint32 & desc, Uint32 offset);
  static void   setCharsetPos(Uint32 & desc, Uint32 offset);
  static void   setNullFlagPos(Uint32 & desc, Uint32 offset);

  static Uint32 getOffset(const Uint32 &);
  static bool   getCharsetFlag(const Uint32 &);
  static Uint32 getCharsetPos(const Uint32 &);
  static Uint32 getNullFlagPos(const Uint32 &);
  static Uint32 getNullFlagOffset(const Uint32 &);
  static Uint32 getNullFlagBitOffset(const Uint32 &);
  static bool   isNULL(const Uint32 &, const Uint32 &);
};

/**
 * Allow for 4096 attributes, all nullable, and for 128 different
 * character sets.
 *
 * a = Attribute offset         - 11 bits  0-10 ( addr word in 8 kb )
 * c = Has charset flag           1  bits 11-11
 * s = Charset pointer position - 7  bits 12-18 ( in table descriptor )
 * f = Null flag offset in word - 5  bits 20-24 ( address 32 bits )
 * w = Null word offset         - 7  bits 25-32 ( f+w addr 4096 attrs )
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * aaaaaaaaaaacsssssss fffffwwwwwww
 */

#define AO_ATTRIBUTE_OFFSET_SHIFT       0
#define AO_ATTRIBUTE_OFFSET_MASK        0x7ff

#define AO_CHARSET_FLAG_SHIFT           11
#define AO_CHARSET_POS_SHIFT            12
#define AO_CHARSET_POS_MASK             127

#define AO_NULL_FLAG_POS_MASK           0xfff   // f+w
#define AO_NULL_FLAG_POS_SHIFT          20

#define AO_NULL_FLAG_WORD_MASK          31      // f
#define AO_NULL_FLAG_OFFSET_SHIFT       5

inline
void
AttributeOffset::setOffset(Uint32 & desc, Uint32 offset){
  ASSERT_MAX(offset, AO_ATTRIBUTE_OFFSET_MASK, "AttributeOffset::setOffset");
  desc |= (offset << AO_ATTRIBUTE_OFFSET_SHIFT);
}

inline
void
AttributeOffset::setCharsetPos(Uint32 & desc, Uint32 offset) {
  ASSERT_MAX(offset, AO_CHARSET_POS_MASK, "AttributeOffset::setCharsetPos");
  desc |= (1 << AO_CHARSET_FLAG_SHIFT);
  desc |= (offset << AO_CHARSET_POS_SHIFT);
}

inline
void
AttributeOffset::setNullFlagPos(Uint32 & desc, Uint32 pos){
  ASSERT_MAX(pos, AO_NULL_FLAG_POS_MASK, "AttributeOffset::setNullFlagPos");
  desc |= (pos << AO_NULL_FLAG_POS_SHIFT);
}

inline
Uint32
AttributeOffset::getOffset(const Uint32 & desc)
{
  return (desc >> AO_ATTRIBUTE_OFFSET_SHIFT) & AO_ATTRIBUTE_OFFSET_MASK;
}

inline
bool
AttributeOffset::getCharsetFlag(const Uint32 & desc)
{
  return (desc >> AO_CHARSET_FLAG_SHIFT) & 1;
}

inline
Uint32
AttributeOffset::getCharsetPos(const Uint32 & desc)
{
  return (desc >> AO_CHARSET_POS_SHIFT) & AO_CHARSET_POS_MASK;
}

inline 
Uint32
AttributeOffset::getNullFlagPos(const Uint32 & desc)
{
  return ((desc >> AO_NULL_FLAG_POS_SHIFT) & AO_NULL_FLAG_POS_MASK);
}

inline
Uint32
AttributeOffset::getNullFlagOffset(const Uint32 & desc)
{
  return (getNullFlagPos(desc) >> AO_NULL_FLAG_OFFSET_SHIFT);
}

inline
Uint32
AttributeOffset::getNullFlagBitOffset(const Uint32 & desc)
{
  return (getNullFlagPos(desc) & AO_NULL_FLAG_WORD_MASK);
}

inline
bool
AttributeOffset::isNULL(const Uint32 & pageWord, const Uint32 & desc)
{
  return (((pageWord >> getNullFlagBitOffset(desc)) & 1) == 1);
}

#endif
