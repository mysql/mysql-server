/*
   Copyright (C) 2003-2007 MySQL AB, 2009 Sun Microsystems, Inc.
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ATTRIBUTE_DESCRIPTOR_HPP
#define ATTRIBUTE_DESCRIPTOR_HPP

class AttributeDescriptor {
  friend class Dbdict;
  friend class Dbtc;
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbtux;
  friend class Dblqh;
  friend class SimulatedBlock;

public:
  static void setType(Uint32 &, Uint32 type);
  static void setSize(Uint32 &, Uint32 size);
  static void setArrayType(Uint32 &, Uint32 arrayType);
  static void setArraySize(Uint32 &, Uint32 arraySize);
  static void setNullable(Uint32 &, Uint32 nullable);
  static void setDKey(Uint32 &, Uint32 dkey);
  static void setPrimaryKey(Uint32 &, Uint32 dkey);
  static void setDynamic(Uint32 &, Uint32 dynamicInd);
  static void setDiskBased(Uint32 &, Uint32 val);
  
  static Uint32 getType(const Uint32 &);
  static Uint32 getSize(const Uint32 &);
  static Uint32 getSizeInBytes(const Uint32 &);
  static Uint32 getSizeInWords(const Uint32 &);
  static Uint32 getArrayType(const Uint32 &);
  static Uint32 getArraySize(const Uint32 &);
  static Uint32 getNullable(const Uint32 &);
  static Uint32 getDKey(const Uint32 &);
  static Uint32 getPrimaryKey(const Uint32 &);
  static Uint32 getDynamic(const Uint32 &);
  static Uint32 getDiskBased(const Uint32 &);

  static void clearArrayType(Uint32 &);

  Uint32 m_data;
};

/**
 *
 * a = Array type            - 2  Bits -> Max 3  (Bit 0-1)
 * t = Attribute type        - 6  Bits -> Max 63  (Bit 2-7)
 * s = Attribute size        - 3  Bits -> Max 7  (Bit 8-10)
 *                                0 is for bit types, stored in bitmap
 *                                1-2 unused
 *                                3 for byte-sized (char...)
 *                                4 for 16-bit sized
 *                                etc.
 * d = Disk based            - 1  Bit 11
 * n = Nullable              - 1  Bit 12
 * k = Distribution Key Ind  - 1  Bit 13
 * p = Primary key attribute - 1  Bit 14
 * y = Dynamic attribute     - 1  Bit 15
 * z = Array size            - 16 Bits -> Max 65535 (Bit 16-31)
 *                                Element size is determined by attribute size
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * aattttttsssdnkpyzzzzzzzzzzzzzzzz
 * aattsss n d k pyzzzzzzzzzzzzzzzz  [ old format ]
 *               
 */

#define AD_ARRAY_TYPE_SHIFT (0)
#define AD_ARRAY_TYPE_MASK  (3)

#define AD_TYPE_SHIFT       (2)
#define AD_TYPE_MASK        (63)

#define AD_SIZE_SHIFT       (8)
#define AD_SIZE_MASK        (7)

#define AD_SIZE_IN_BYTES_SHIFT (3)
#define AD_SIZE_IN_WORDS_OFFSET (31)
#define AD_SIZE_IN_WORDS_SHIFT  (5)

#define AD_DISK_SHIFT        (11)
#define AD_NULLABLE_SHIFT    (12)
#define AD_DISTR_KEY_SHIFT   (13)
#define AD_PRIMARY_KEY       (14)
#define AD_DYNAMIC           (15)

#define AD_ARRAY_SIZE_SHIFT  (16)
#define AD_ARRAY_SIZE_MASK   (65535)

inline
void
AttributeDescriptor::setType(Uint32 & desc, Uint32 type){
  assert(type <= AD_TYPE_MASK);
  desc |= (type << AD_TYPE_SHIFT);
}

inline
void
AttributeDescriptor::setSize(Uint32 & desc, Uint32 size){
  assert(size <= AD_SIZE_MASK);
  desc |= (size << AD_SIZE_SHIFT);
}

inline
void
AttributeDescriptor::setArrayType(Uint32 & desc, Uint32 arrayType){
  assert(arrayType <= AD_ARRAY_TYPE_MASK);
  desc |= (arrayType << AD_ARRAY_TYPE_SHIFT);
}

inline
void
AttributeDescriptor::clearArrayType(Uint32 & desc)
{
  desc &= ~Uint32(AD_ARRAY_TYPE_MASK << AD_ARRAY_TYPE_SHIFT);
}

inline
void
AttributeDescriptor::setArraySize(Uint32 & desc, Uint32 arraySize){
  assert(arraySize <= AD_ARRAY_SIZE_MASK);
  desc |= (arraySize << AD_ARRAY_SIZE_SHIFT);
}

inline
void
AttributeDescriptor::setNullable(Uint32 & desc, Uint32 nullable){
  assert(nullable <= 1);
  desc |= (nullable << AD_NULLABLE_SHIFT);
}

inline
void
AttributeDescriptor::setDKey(Uint32 & desc, Uint32 dkey){
  assert(dkey <= 1);
  desc |= (dkey << AD_DISTR_KEY_SHIFT);
}

inline
void
AttributeDescriptor::setPrimaryKey(Uint32 & desc, Uint32 dkey){
  assert(dkey <= 1);
  desc |= (dkey << AD_PRIMARY_KEY);
}

inline
void
AttributeDescriptor::setDynamic(Uint32 & desc, Uint32 dynamic){
  assert(dynamic <= 1);
  desc |= (dynamic << AD_DYNAMIC);
}

inline
void
AttributeDescriptor::setDiskBased(Uint32 & desc, Uint32 val)
{
  assert(val <= 1);
  desc |= (val << AD_DISK_SHIFT);
}

/**
 * Getters
 */
inline
Uint32
AttributeDescriptor::getType(const Uint32 & desc){
  return (desc >> AD_TYPE_SHIFT) & AD_TYPE_MASK;
}

inline
Uint32
AttributeDescriptor::getSize(const Uint32 & desc){
  return (desc >> AD_SIZE_SHIFT) & AD_SIZE_MASK;
}

inline
Uint32
AttributeDescriptor::getSizeInBytes(const Uint32 & desc){
  return (getArraySize(desc) << getSize(desc))
                             >> AD_SIZE_IN_BYTES_SHIFT;
}

inline
Uint32
AttributeDescriptor::getSizeInWords(const Uint32 & desc){
  return ((getArraySize(desc) << getSize(desc)) 
          + AD_SIZE_IN_WORDS_OFFSET) 
                              >> AD_SIZE_IN_WORDS_SHIFT;
}

inline
Uint32
AttributeDescriptor::getArrayType(const Uint32 & desc){
  return (desc >> AD_ARRAY_TYPE_SHIFT) & AD_ARRAY_TYPE_MASK;
}

inline
Uint32
AttributeDescriptor::getArraySize(const Uint32 & desc){
  return (desc >> AD_ARRAY_SIZE_SHIFT) & AD_ARRAY_SIZE_MASK;
}

inline
Uint32
AttributeDescriptor::getNullable(const Uint32 & desc){
  return (desc >> AD_NULLABLE_SHIFT) & 1;
}

inline
Uint32
AttributeDescriptor::getDKey(const Uint32 & desc){
  return (desc >> AD_DISTR_KEY_SHIFT) & 1;
}

inline
Uint32
AttributeDescriptor::getPrimaryKey(const Uint32 & desc){
  return (desc >> AD_PRIMARY_KEY) & 1;
}

inline
Uint32
AttributeDescriptor::getDynamic(const Uint32 & desc){
  return (desc >> AD_DYNAMIC) & 1;
}

inline
Uint32
AttributeDescriptor::getDiskBased(const Uint32 & desc)
{
  return (desc >> AD_DISK_SHIFT) & 1;
}

class NdbOut&
operator<<(class NdbOut&, const AttributeDescriptor&);

#endif
