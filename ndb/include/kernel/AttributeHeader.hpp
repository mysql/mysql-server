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

#ifndef ATTRIBUTE_HEADER
#define ATTRIBUTE_HEADER

#include <new>
/**
 * @class AttributeHeader
 * @brief Header passed in front of every attribute value in AttrInfo signal
 */
class AttributeHeader {
  friend class Dbtup;
  friend class Backup;
  friend class NdbOperation;
  friend class DbUtil;
  friend class Suma;

public:
  /**
   * Psuedo columns
   */
  STATIC_CONST( PSUEDO       = 0x8000 );
  STATIC_CONST( FRAGMENT     = 0xFFFE ); // Read fragment no
  STATIC_CONST( ROW_COUNT    = 0xFFFD ); // Read row count (committed)
  STATIC_CONST( COMMIT_COUNT = 0xFFFC ); // Read commit count
  STATIC_CONST( RANGE_NO     = 0xFFFB ); // Read range no (when batched ranges)
  
  /** Initialize AttributeHeader at location aHeaderPtr */
  static AttributeHeader& init(void* aHeaderPtr, Uint32 anAttributeId, 
			       Uint32 aDataSize);

  /** Returns size of AttributeHeader (usually one or two words) */
  Uint32 getHeaderSize() const; // In 32-bit words

  /** Store AttributeHeader in location given as argument */
  void insertHeader(Uint32*);

  /** Get next attribute header (if there is one) */
  AttributeHeader* getNext() const;             

  /** Get location of attribute value */
  Uint32* getDataPtr() const;

  /** Getters and Setters */
  Uint32  getAttributeId() const;
  void    setAttributeId(Uint32);
  Uint32  getDataSize() const;   // In 32-bit words
  void    setDataSize(Uint32);
  bool    isNULL() const;
  void    setNULL();

  /** Print **/
  //void    print(NdbOut&);
  void    print(FILE*);

  static Uint32 getDataSize(Uint32);
  
public:
  AttributeHeader(Uint32 = 0);
  AttributeHeader(Uint32 anAttributeId, Uint32 aDataSize);
  ~AttributeHeader();
  
  Uint32 m_value;
};

/**
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * ssssssssssssss eiiiiiiiiiiiiiiii
 *
 * i = Attribute Id
 * s = Size of current "chunk" - 14 Bits -> 16384 (words) = 65k
 *     Including optional extra word(s).
 * e - Element data/Blob, read element of array
 *     If == 0 next data word contains attribute value.
 *     If == 1 next data word contains:
 *       For Array of Fixed size Elements
 *         Start Index (16 bit), Stop Index(16 bit)
 *       For Blob
 *         Start offset (32 bit) (length is defined in previous word)
 * 
 * An attribute value equal to "null" is represented by setting s == 0.
 * 
 * Bit 14 is not yet used.
 */

inline
AttributeHeader& AttributeHeader::init(void* aHeaderPtr, Uint32 anAttributeId, 
				       Uint32 aDataSize)
{
  return * new (aHeaderPtr) AttributeHeader(anAttributeId, aDataSize);
}

inline
AttributeHeader::AttributeHeader(Uint32 aHeader)
{
  m_value = aHeader;
}

inline
AttributeHeader::AttributeHeader(Uint32 anAttributeId, Uint32 aDataSize)
{
  m_value = 0;
  this->setAttributeId(anAttributeId);
  this->setDataSize(aDataSize);
}

inline
AttributeHeader::~AttributeHeader()
{}

inline
Uint32 AttributeHeader::getHeaderSize() const
{
  // Should check 'e' bit here
  return 1;
}

inline
Uint32 AttributeHeader::getAttributeId() const
{
  return (m_value & 0xFFFF0000) >> 16;
}

inline
void AttributeHeader::setAttributeId(Uint32 anAttributeId)
{
  m_value &= 0x0000FFFF; // Clear attribute id
  m_value |= (anAttributeId << 16);
}

inline
Uint32 AttributeHeader::getDataSize() const
{
  return (m_value & 0x3FFF);
}

inline
void AttributeHeader::setDataSize(Uint32 aDataSize)
{
  m_value &= (~0x3FFF);
  m_value |= aDataSize;
}

inline
bool AttributeHeader::isNULL() const
{
  return (getDataSize() == 0);
}

inline
void AttributeHeader::setNULL()
{
  setDataSize(0);
}

inline
Uint32* AttributeHeader::getDataPtr() const
{
  return (Uint32*)&m_value + getHeaderSize();
}

inline
void AttributeHeader::insertHeader(Uint32* target)
{
  *target = m_value;
}

inline
AttributeHeader* 
AttributeHeader::getNext() const {
  return (AttributeHeader*)(getDataPtr() + getDataSize());
}

inline
void
//AttributeHeader::print(NdbOut& output) {
AttributeHeader::print(FILE* output) {
  fprintf(output, "AttributeId: H\'%.8x (D\'%d), DataSize: H\'%.8x (D\'%d), "
	  "isNULL: %d\n", 
	  getAttributeId(), getAttributeId(), 
	  getDataSize(), getDataSize(), 
	  isNULL());
}

inline
Uint32
AttributeHeader::getDataSize(Uint32 m_value){
  return (m_value & 0x3FFF);  
}

#endif







