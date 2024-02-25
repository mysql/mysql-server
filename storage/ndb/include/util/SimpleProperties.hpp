/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SIMPLE_PROPERTIES_HPP
#define SIMPLE_PROPERTIES_HPP

#include <ndb_global.h>
#include <NdbOut.hpp>
#include <EventLogger.hpp>

/**
 * @class SimpleProperties
 * @brief Key-value-pair container.  Actually a list of named elements.
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
    size_t Offset;
    ValueType Type;
    Uint32 maxLength;
    size_t Length_Offset; // Offset used for looking up length of
                          // data if Type = BinaryValue, or the flag value
                          // ExternalData.
    enum { ExternalData = 0xFFFFFF };
  };

  /**
   * UnpackStatus - Value returned from unpack
   */
  enum UnpackStatus {
    Eof = 0,            // Success, end of SimpleProperties object reached
    Break = 1,          // Success 
    TypeMismatch = 2,
    __unused__ = 3,
    ValueTooLong = 4,
    UnknownKey = 5,
    OutOfMemory = 6     // Only used when packing
  };

  /**
   * Unpack
   */
  class Reader;

  /* Callback function for reading indirect values.
     The callback is expected to read the current value of the iterator.
  */
  typedef void IndirectReader(class Reader & it, void * dst);

  static UnpackStatus unpack(class Reader &,
			     void * struct_dst,
                             const SP2StructMapping[], Uint32 mapSz,
                             IndirectReader *indirectReader = nullptr,
                             void * readerExtra = nullptr);
  
  class Writer;

  /* Callback function for writing indirect values.
     The callback is expected to retrieve the value using key and src,
     add() it to the iterator, and return UnpackStatus::Eof on success.
  */
  typedef bool IndirectWriter(class Writer & it, Uint16 key, const void * src);

  static UnpackStatus pack(class Writer &,
			   const void * struct_src,
			   const SP2StructMapping[], Uint32 mapSz,
                           IndirectWriter *indirectWriter = nullptr,
                           const void * writerExtra = nullptr);
  
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
     * Get value length including any padding that may be returned
     * from getString()
     */
    size_t getPaddedLength() const;

    /**
     * Get value type
     *  Note only valid is valid() == true
     */
    ValueType getValueType() const;

    /**
     * Read value iteratively into buffer.
       Returns number of bytes read, 0 on EOF, or -1 on error.
     */
     int getBuffered(char * buf, Uint32 buf_size);

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
    void printAll(EventLogger* logger);

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
    bool add(Uint16 key, const char * value)  {
      return add(StringValue, key, value, strlen(value)+1);
    }
    bool add(Uint16 key, const void* value, int len) {
      return add(BinaryValue, key, value, len);
    }

    /* Two part API: add a key, then iteratively set value from buffer.
       append() returns
         the number of bytes written;
         0 after writing the complete length as specified by value_length;
         -1 on storage error.
    */
    bool addKey(Uint16 key, ValueType type, Uint32 value_length);
    int append(const char * buf, Uint32 buf_size);

  protected:
    bool add(ValueType type, Uint16 key, const void * value, int len);
    virtual ~Writer() {}
    virtual bool reset() = 0;
    virtual bool putWord(Uint32 val) = 0;
    virtual bool putWords(const Uint32 * src, Uint32 len) = 0;
  private:
    bool add(const char* value, int len);

  private:
    Uint32 m_value_length;
    Uint32 m_bytes_written;
  };
};

/**
 * Reader for linear memory
 */
class SimplePropertiesLinearReader : public SimpleProperties::Reader {
public:
  SimplePropertiesLinearReader(const Uint32 * src, Uint32 len);
  ~SimplePropertiesLinearReader() override {}
  
  void reset() override;
  bool step(Uint32 len) override;
  bool getWord(Uint32 * dst) override;
  bool peekWord(Uint32 * dst) const override ;
  bool peekWords(Uint32 * dst, Uint32 len) const override;
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
  ~LinearWriter() override {}

  bool reset() override;
  bool putWord(Uint32 val) override;
  bool putWords(const Uint32 * src, Uint32 len) override;
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
  ~UtilBufferWriter() override {}
  
  bool reset() override;
  bool putWord(Uint32 val) override;
  bool putWords(const Uint32 * src, Uint32 len) override;
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
  ~SimplePropertiesSectionReader() override {}
  
  void reset() override;
  bool step(Uint32 len) override;
  bool getWord(Uint32 * dst) override;
  bool peekWord(Uint32 * dst) const override ;
  bool peekWords(Uint32 * dst, Uint32 len) const override;
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
  ~SimplePropertiesSectionWriter() override;

  bool reset() override;
  bool putWord(Uint32 val) override;
  bool putWords(const Uint32 * src, Uint32 len) override;
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
