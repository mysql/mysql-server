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
  static void   setNullFlagPos(Uint32 & desc, Uint32 offset);

  static Uint32 getOffset(const Uint32 &);
  static Uint32 getNullFlagPos(const Uint32 &);
  static Uint32 getNullFlagOffset(const Uint32 &);
  static Uint32 getNullFlagBitOffset(const Uint32 &);
  static bool   isNULL(const Uint32 &, const Uint32 &);
};

#define AO_ATTRIBUTE_OFFSET_MASK  (0xffff)
#define AO_NULL_FLAG_POS_MASK     (0x7ff)
#define AO_NULL_FLAG_POS_SHIFT    (21)
#define AO_NULL_FLAG_WORD_MASK    (31)
#define AO_NULL_FLAG_OFFSET_SHIFT (5)

inline
void
AttributeOffset::setOffset(Uint32 & desc, Uint32 offset){
  ASSERT_MAX(offset, AO_ATTRIBUTE_OFFSET_MASK, "AttributeOffset::setOffset");
  desc |= offset;
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
  return desc & AO_ATTRIBUTE_OFFSET_MASK;
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
