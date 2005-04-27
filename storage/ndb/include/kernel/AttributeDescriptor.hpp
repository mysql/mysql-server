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

#ifndef ATTRIBUTE_DESCRIPTOR_HPP
#define ATTRIBUTE_DESCRIPTOR_HPP

class AttributeDescriptor {
  friend class Dbdict;
  friend class Dbtc;
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbtux;
  
private:
  static void setType(Uint32 &, Uint32 type);
  static void setSize(Uint32 &, Uint32 size);
  static void setArray(Uint32 &, Uint32 arraySize);
  static void setNullable(Uint32 &, Uint32 nullable);
  static void setDKey(Uint32 &, Uint32 dkey);
  static void setPrimaryKey(Uint32 &, Uint32 dkey);
  static void setDynamic(Uint32 &, Uint32 dynamicInd);
  
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
};

/**
 *
 * a = Array type            - 2  Bits -> Max 3  (Bit 0-1)
 * t = Attribute type        - 5  Bits -> Max 31  (Bit 2-6)
 * s = Attribute size        - 3  Bits -> Max 7  (Bit 8-10)
 * d = Disk based            - 1  Bit 11
 * n = Nullable              - 1  Bit 12
 * k = Distribution Key Ind  - 1  Bit 13
 * p = Primary key attribute - 1  Bit 14
 * y = Dynamic attribute     - 1  Bit 15
 * z = Array size            - 16 Bits -> Max 65535 (Bit 16-31)
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * aattttt sssdnkpyzzzzzzzzzzzzzzzz
 * aattsss n d k pyzzzzzzzzzzzzzzzz  [ old format ]
 *               
 */

#define AD_ARRAY_TYPE_SHIFT (0)
#define AD_ARRAY_TYPE_MASK  (3)

#define AD_TYPE_SHIFT       (2)
#define AD_TYPE_MASK        (31)

#define AD_SIZE_SHIFT       (8)
#define AD_SIZE_MASK        (7)

#define AD_SIZE_IN_BYTES_SHIFT (3)
#define AD_SIZE_IN_WORDS_OFFSET (31)
#define AD_SIZE_IN_WORDS_SHIFT  (5)

#define AD_NULLABLE_SHIFT    (12)
#define AD_DISTR_KEY_SHIFT   (13)
#define AD_PRIMARY_KEY       (14)
#define AD_DYNAMIC           (15)

#define AD_ARRAY_SIZE_SHIFT  (16)
#define AD_ARRAY_SIZE_MASK   (65535)

inline
void
AttributeDescriptor::setType(Uint32 & desc, Uint32 type){
  ASSERT_MAX(type, AD_TYPE_MASK, "AttributeDescriptor::setType");
  desc |= (type << AD_TYPE_SHIFT);
}

inline
void
AttributeDescriptor::setSize(Uint32 & desc, Uint32 size){
  ASSERT_MAX(size, AD_SIZE_MASK, "AttributeDescriptor::setSize");
  desc |= (size << AD_SIZE_SHIFT);
}

inline
void
AttributeDescriptor::setArray(Uint32 & desc, Uint32 size){
  ASSERT_MAX(size, AD_ARRAY_SIZE_MASK, "AttributeDescriptor::setArray");
  desc |= (size << AD_ARRAY_SIZE_SHIFT);
  if(size <= 1){
    desc |= (size << AD_ARRAY_TYPE_SHIFT);
  } else {
    desc |= (2 << AD_ARRAY_TYPE_SHIFT);
  }
}

inline
void
AttributeDescriptor::setNullable(Uint32 & desc, Uint32 nullable){
  ASSERT_BOOL(nullable, "AttributeDescriptor::setNullable");
  desc |= (nullable << AD_NULLABLE_SHIFT);
}

inline
void
AttributeDescriptor::setDKey(Uint32 & desc, Uint32 dkey){
  ASSERT_BOOL(dkey, "AttributeDescriptor::setDKey");
  desc |= (dkey << AD_DISTR_KEY_SHIFT);
}

inline
void
AttributeDescriptor::setPrimaryKey(Uint32 & desc, Uint32 dkey){
  ASSERT_BOOL(dkey, "AttributeDescriptor::setPrimaryKey");
  desc |= (dkey << AD_PRIMARY_KEY);
}

inline
void
AttributeDescriptor::setDynamic(Uint32 & desc, Uint32 dynamic){
  ASSERT_BOOL(dynamic, "AttributeDescriptor::setDynamic");
  desc |= (dynamic << AD_DYNAMIC);
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

#endif
