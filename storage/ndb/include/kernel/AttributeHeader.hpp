/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ATTRIBUTE_HEADER
#define ATTRIBUTE_HEADER

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
   * Pseudo columns
   */
  STATIC_CONST( PSEUDO       = 0x8000 );
  STATIC_CONST( FRAGMENT     = 0xFFFE ); // Read fragment no
  STATIC_CONST( ROW_COUNT    = 0xFFFD ); // Read row count (committed)
  STATIC_CONST( COMMIT_COUNT = 0xFFFC ); // Read commit count
  STATIC_CONST( RANGE_NO     = 0xFFFB ); // Read range no (when batched ranges)
  
  STATIC_CONST( ROW_SIZE     = 0xFFFA );
  STATIC_CONST( FRAGMENT_FIXED_MEMORY= 0xFFF9 );

  STATIC_CONST( RECORDS_IN_RANGE = 0xFFF8 );
  STATIC_CONST( DISK_REF     = 0xFFF7 );
  STATIC_CONST( ROWID        = 0xFFF6 );
  STATIC_CONST( ROW_GCI      = 0xFFF5 );
  STATIC_CONST( FRAGMENT_VARSIZED_MEMORY = 0xFFF4 );
  // 0xFFF3  to be used for read packed when merged
  STATIC_CONST( ANY_VALUE    = 0xFFF2 );
  STATIC_CONST( COPY_ROWID   = 0xFFF1 );
  
  // NOTE: in 5.1 ctors and init take size in bytes

  /** Initialize AttributeHeader at location aHeaderPtr */
  static void init(Uint32* aHeaderPtr, Uint32 anAttributeId, Uint32 aByteSize);

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
  Uint32  getByteSize() const;
  void    setByteSize(Uint32);
  Uint32  getDataSize() const;   // In 32-bit words, rounded up
  void    setDataSize(Uint32);   // Set size to multiple of word size
  bool    isNULL() const;
  void    setNULL();

  /** Print **/
  //void    print(NdbOut&);
  void    print(FILE*);

  static Uint32 getByteSize(Uint32);
  static Uint32 getDataSize(Uint32);
  
public:
  AttributeHeader(Uint32 = 0);
  AttributeHeader(Uint32 anAttributeId, Uint32 aByteSize);
  ~AttributeHeader();
  
  Uint32 m_value;
};

/**
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * ssssssssssssssssiiiiiiiiiiiiiiii
 *
 * i = Attribute Id
 * s = Size of current "chunk" in bytes - 16 bits.
 *     To allow round up to word, max value is 0xFFFC (not checked).
 * e - [ obsolete future ]
 *     Element data/Blob, read element of array
 *     If == 0 next data word contains attribute value.
 *     If == 1 next data word contains:
 *       For Array of Fixed size Elements
 *         Start Index (16 bit), Stop Index(16 bit)
 *       For Blob
 *         Start offset (32 bit) (length is defined in previous word)
 * 
 * An attribute value equal to "null" is represented by setting s == 0.
 */

inline
void AttributeHeader::init(Uint32* aHeaderPtr, Uint32 anAttributeId, 
                           Uint32 aByteSize)
{
  AttributeHeader ah(anAttributeId, aByteSize);
  *aHeaderPtr = ah.m_value;
}

inline
AttributeHeader::AttributeHeader(Uint32 aHeader)
{
  m_value = aHeader;
}

inline
AttributeHeader::AttributeHeader(Uint32 anAttributeId, Uint32 aByteSize)
{
  m_value = 0;
  this->setAttributeId(anAttributeId);
  this->setByteSize(aByteSize);
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
Uint32 AttributeHeader::getByteSize() const
{
  return (m_value & 0xFFFF);
}

inline
void AttributeHeader::setByteSize(Uint32 aByteSize)
{
  m_value &= (~0xFFFF);
  m_value |= aByteSize;
}

inline
Uint32 AttributeHeader::getDataSize() const
{
  return (((m_value & 0xFFFF) + 3) >> 2);
}

inline
void AttributeHeader::setDataSize(Uint32 aDataSize)
{
  m_value &= (~0xFFFF);
  m_value |= (aDataSize << 2);
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
AttributeHeader::getByteSize(Uint32 m_value){
  return (m_value & 0xFFFF);  
}

inline
Uint32
AttributeHeader::getDataSize(Uint32 m_value){
  return (((m_value & 0xFFFF) + 3) >> 2);
}

#endif







