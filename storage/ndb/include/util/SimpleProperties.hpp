/*
   Copyright (C) 2003-2008 MySQL AB, 2008 Sun Microsystems, Inc.
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

#ifndef SIMPLE_PROPERTIES_HPP
#define SIMPLE_PROPERTIES_HPP

#include <ndb_global.h>
#include <NdbOut.hpp>

/**
 * @class SimpleProperties
 * @brief Key-value-pair container.  Actully a list of named elements.
 *
 * SimpleProperties:
 * - The keys are Uint16
 * - The values are either Uint32 or null terminated c-strings
 * 
 * @note  Keys may be repeated.
 * 
 * Examples of things that can be stored in a SimpleProperties object:
 * - Lists like: ((1, "foo"), (2, "bar"), (3, 32), (2, "baz"))
 */
class SimpleProperties {
public:
  /**
   * Value types
   */
   enum ValueType {
    Uint32Value  = 0,
    StringValue  = 1,
    BinaryValue  = 2,
    InvalidValue = 3
   };

  /**
   * Struct for defining mapping to be used with unpack
   */
  struct SP2StructMapping {
    Uint16 Key;
    Uint32 Offset;
    ValueType Type;
    Uint32 minValue;
    Uint32 maxValue;
    Uint32 Length_Offset; // Offset used for looking up length of 
                          // data if Type = BinaryValue
  };

  /**
   * UnpackStatus - Value returned from unpack
   */
  enum UnpackStatus {
    Eof = 0,            // Success, end of SimpleProperties object reached
    Break = 1,          // Success 
    TypeMismatch = 2,
    ValueTooLow = 3,
    ValueTooHigh = 4,
    UnknownKey = 5,
    OutOfMemory = 6     // Only used when packing
  };

  /**
   * Unpack
   */
  class Reader;
  static UnpackStatus unpack(class Reader & it, 
			     void * dst, 
			     const SP2StructMapping[], Uint32 mapSz,
			     bool ignoreMinMax,
			     bool ignoreUnknownKeys);
  
  class Writer;
  static UnpackStatus pack(class Writer &,
			   const void * src,
			   const SP2StructMapping[], Uint32 mapSz, 
			   bool ignoreMinMax);
  
  /**
   * Reader class
   */
  class Reader {
  public:
    virtual ~Reader() {}

    /**
     * Move to first element
     *   Return true if element exist
     */
    bool first();
    
    /**
     * Move to next element
     *   Return true if element exist
     */
    bool next();
    
    /**
     * Is this valid
     */
    bool valid() const;

    /**
     * Get key
     *  Note only valid is valid() == true
     */
    Uint16 getKey() const;

    /**
     * Get value length in bytes - (including terminating 0 for strings)
     *  Note only valid is valid() == true
     */
    Uint16 getValueLen() const;

    /**
     * Get value type
     *  Note only valid is valid() == true
     */
    ValueType getValueType() const;
    
    /**
     * Get value
     *  Note only valid is valid() == true
     */
    Uint32 getUint32() const;
    char * getString(char * dst) const;
    
    /**
     * Print the complete simple properties (for debugging)
     */
    void printAll(NdbOut& ndbout);

  private:
    bool readValue();
    
    Uint16 m_key;
    Uint16 m_itemLen;
    union {
      Uint32 m_ui32_value;
      Uint32 m_strLen; // Including 0-byte in words
    };
    ValueType m_type;
  protected:
    Reader();
    virtual void reset() = 0;
    
    virtual bool step(Uint32 len) = 0;
    virtual bool getWord(Uint32 * dst) = 0;
    virtual bool peekWord(Uint32 * dst) const = 0;
    virtual bool peekWords(Uint32 * dst, Uint32 len) const = 0;
  };

  /**
   * Writer class
   */
  class Writer {
  public:
    Writer() {}

    bool first();
    bool add(Uint16 key, Uint32 value);
    bool add(Uint16 key, const char * value);
    bool add(Uint16 key, const void* value, int len);
  protected:
    virtual ~Writer() {}
    virtual bool reset() = 0;
    virtual bool putWord(Uint32 val) = 0;
    virtual bool putWords(const Uint32 * src, Uint32 len) = 0;
  private:
    bool add(const char* value, int len);
  };
};

/**
 * Reader for linear memory
 */
class SimplePropertiesLinearReader : public SimpleProperties::Reader {
public:
  SimplePropertiesLinearReader(const Uint32 * src, Uint32 len);
  virtual ~SimplePropertiesLinearReader() {}
  
  virtual void reset();
  virtual bool step(Uint32 len);
  virtual bool getWord(Uint32 * dst);
  virtual bool peekWord(Uint32 * dst) const ;
  virtual bool peekWords(Uint32 * dst, Uint32 len) const;
private:
  Uint32 m_len;
  Uint32 m_pos;
  const Uint32 * m_src;
};

/**
 * Writer for linear memory
 */
class LinearWriter : public SimpleProperties::Writer {
public:
  LinearWriter(Uint32 * src, Uint32 len);
  virtual ~LinearWriter() {}

  virtual bool reset();
  virtual bool putWord(Uint32 val);
  virtual bool putWords(const Uint32 * src, Uint32 len);
  Uint32 getWordsUsed() const;
private:
  Uint32 m_len;
  Uint32 m_pos;
  Uint32 * m_src;
};

/**
 * Writer for UtilBuffer
 */
class UtilBufferWriter : public SimpleProperties::Writer {
public:
  UtilBufferWriter(class UtilBuffer & buf);
  virtual ~UtilBufferWriter() {}
  
  virtual bool reset();
  virtual bool putWord(Uint32 val);
  virtual bool putWords(const Uint32 * src, Uint32 len);
  Uint32 getWordsUsed() const;
private:
  class UtilBuffer & m_buf;
};

/**
 * Reader for long signal section memory
 *
 *
 * Implemented in kernel/vm/SimplePropertiesSection.cpp
 */
class SimplePropertiesSectionReader : public SimpleProperties::Reader {
public:
  SimplePropertiesSectionReader(struct SegmentedSectionPtr &,
				class SectionSegmentPool &);
  virtual ~SimplePropertiesSectionReader() {}
  
  virtual void reset();
  virtual bool step(Uint32 len);
  virtual bool getWord(Uint32 * dst);
  virtual bool peekWord(Uint32 * dst) const ;
  virtual bool peekWords(Uint32 * dst, Uint32 len) const;
  Uint32 getSize() const;
  bool getWords(Uint32 * dst, Uint32 len);

private:
  Uint32 m_pos;
  Uint32 m_len;
  class SectionSegmentPool & m_pool;
  struct SectionSegment * m_head;
  struct SectionSegment * m_currentSegment;
};

inline
Uint32 SimplePropertiesSectionReader::getSize() const
{
  return m_len;
}

/**
 * Writer for long signal section memory
 *
 *
 * Implemented in kernel/vm/SimplePropertiesSection.cpp
 */
class SimplePropertiesSectionWriter : public SimpleProperties::Writer {
public:
  SimplePropertiesSectionWriter(class SimulatedBlock & block);
  virtual ~SimplePropertiesSectionWriter();

  virtual bool reset();
  virtual bool putWord(Uint32 val);
  virtual bool putWords(const Uint32 * src, Uint32 len);
  Uint32 getWordsUsed() const;

  /**
   * This "unlinks" the writer from the memory
   */
  void getPtr(struct SegmentedSectionPtr & dst);
  
private:
  void release();

  Int32 m_pos;
  Uint32 m_sz;

  class SectionSegmentPool & m_pool;
  class SimulatedBlock & m_block;
  struct SectionSegment * m_head;
  Uint32 m_prevPtrI; // Prev to m_currentSegment
  struct SectionSegment * m_currentSegment;
};

#endif
