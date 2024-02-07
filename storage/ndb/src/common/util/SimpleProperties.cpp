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

#include <ndb_global.h>
#include <NdbOut.hpp>
#include <SimpleProperties.hpp>
#include <UtilBuffer.hpp>
#include "portlib/ndb_socket.h"  // ntohl()
#include "util/require.h"

bool SimpleProperties::Writer::first() { return reset(); }

bool SimpleProperties::Writer::addKey(Uint16 key, ValueType type, Uint32 data) {
  Uint32 head = type;
  head <<= 16;
  head += key;
  if (!putWord(htonl(head))) return false;

  m_value_length = data;
  m_bytes_written = 0;

  return putWord(htonl(data));
}

bool SimpleProperties::Writer::add(Uint16 key, Uint32 value) {
  Uint32 head = Uint32Value;
  head <<= 16;
  head += key;
  return (putWord(htonl(head)) && putWord(htonl(value)));
}

bool SimpleProperties::Writer::add(const char *value, int len) {
  const Uint32 valLen = (len + 3) / 4;

  if ((len % 4) == 0) return putWords((const Uint32 *)value, valLen);

  const Uint32 putLen = valLen - 1;
  if (!putWords((const Uint32 *)value, putLen)) return false;

  // Special handling of last bytes
  union {
    Uint32 lastWord;
    char lastBytes[4];
  } tmp;
  tmp.lastWord = 0;
  memcpy(tmp.lastBytes, value + putLen * 4, len - putLen * 4);
  return putWord(tmp.lastWord);
}

bool SimpleProperties::Writer::add(ValueType type, Uint16 key,
                                   const void *value, int len) {
  if (!addKey(key, type, len)) return false;

  return add((const char *)value, len);
}

int SimpleProperties::Writer::append(const char *buf, Uint32 buf_size) {
  if (m_bytes_written < m_value_length) {
    Uint32 bytesToAdd = m_value_length - m_bytes_written;
    if (bytesToAdd > buf_size) bytesToAdd = buf_size;

    if (add(buf, bytesToAdd)) {
      m_bytes_written += bytesToAdd;
      return bytesToAdd;
    } else {
      return -1;
    }
  }
  return 0;
}

SimpleProperties::Reader::Reader() { m_itemLen = 0; }

bool SimpleProperties::Reader::first() {
  reset();
  m_itemLen = 0;
  return readValue();
}

bool SimpleProperties::Reader::next() { return readValue(); }

bool SimpleProperties::Reader::valid() const { return m_type != InvalidValue; }

Uint16 SimpleProperties::Reader::getKey() const { return m_key; }

Uint16 SimpleProperties::Reader::getValueLen() const {
  switch (m_type) {
    case Uint32Value:
      return 4;
    case StringValue:
    case BinaryValue:
      return m_strLen;
    case InvalidValue:
      return 0;
  }
  return 0;
}

size_t SimpleProperties::Reader::getPaddedLength() const {
  return m_itemLen * 4;
}

SimpleProperties::ValueType SimpleProperties::Reader::getValueType() const {
  return m_type;
}

Uint32 SimpleProperties::Reader::getUint32() const { return m_ui32_value; }

char *SimpleProperties::Reader::getString(char *dst) const {
  if (peekWords((Uint32 *)dst, m_itemLen)) return dst;
  return nullptr;
}

int SimpleProperties::Reader::getBuffered(char *buf, Uint32 buf_size) {
  require(buf_size % 4 == 0);

  Uint32 readWords = m_itemLen;
  if (readWords) {
    Uint32 bufSizeInWords = buf_size / 4;
    if (readWords > bufSizeInWords) readWords = bufSizeInWords;

    if (!peekWords((Uint32 *)buf, readWords)) return -1;

    step(readWords);

    /* decrement m_itemLen and m_strLen towards zero */
    m_itemLen -= readWords;
    if (m_itemLen) {
      m_strLen -= buf_size;
      return buf_size;
    }
    return m_strLen;
  }
  return 0;
}

bool SimpleProperties::Reader::readValue() {
  if (!step(m_itemLen)) {
    m_type = InvalidValue;
    return false;
  }

  Uint32 tmp;
  if (!getWord(&tmp)) {
    m_type = InvalidValue;
    return false;
  }

  tmp = ntohl(tmp);
  m_key = tmp & 0xFFFF;
  m_type = (SimpleProperties::ValueType)(tmp >> 16);
  switch (m_type) {
    case Uint32Value:
      m_itemLen = 1;
      if (!peekWord(&m_ui32_value)) return false;
      m_ui32_value = ntohl(m_ui32_value);
      return true;
    case StringValue:
    case BinaryValue:
      if (!getWord(&tmp)) return false;
      m_strLen = ntohl(tmp);
      m_itemLen = (m_strLen + 3) / 4;
      return true;
    default:
      m_itemLen = 0;
      m_type = InvalidValue;
      return false;
  }
}

int findKeyInMapping(Uint16 key, const SimpleProperties::SP2StructMapping map[],
                     Uint32 mapSz) {
  /* Linear search */
  for (Uint32 i = 0; i < mapSz; i++)
    if (key == map[i].Key) return i;
  return -1;
}

SimpleProperties::UnpackStatus SimpleProperties::unpack(
    Reader &it, void *dst, const SP2StructMapping _map[], Uint32 mapSz,
    IndirectReader *indirectReader, void *extra) {
  static const bool ignoreUnknownKeys = true;

  do {
    if (!it.valid()) break;

    Uint16 key = it.getKey();
    int i = findKeyInMapping(key, _map, mapSz);

    if (i >= 0) {
      if (_map[i].Type == InvalidValue) return Break;
      if (_map[i].Type != it.getValueType()) return TypeMismatch;

      if (_map[i].Length_Offset == SP2StructMapping::ExternalData) {
        if (indirectReader) indirectReader(it, extra);
      } else {
        char *_dst = (char *)dst;
        _dst += _map[i].Offset;

        switch (it.getValueType()) {
          case Uint32Value:
            *((Uint32 *)_dst) = it.getUint32();
            break;
          case BinaryValue:
          case StringValue:
            if (_map[i].maxLength && it.getValueLen() > _map[i].maxLength)
              return ValueTooLong;
            it.getString(_dst);
            break;
          default:
            abort();
        }
      }
    } else if (!ignoreUnknownKeys)
      return UnknownKey;
  } while (it.next());

  return Eof;
}

SimpleProperties::UnpackStatus SimpleProperties::pack(
    Writer &it, const void *__src, const SP2StructMapping _map[], Uint32 mapSz,
    IndirectWriter *indirectWriter, const void *extra) {
  static const bool ignoreMinMax = true;

  const char *_src = (const char *)__src;

  for (Uint32 i = 0; i < mapSz; i++) {
    bool ok = false;
    Uint16 key = _map[i].Key;
    const char *src = _src + _map[i].Offset;
    if (_map[i].Length_Offset == SP2StructMapping::ExternalData) {
      ok = indirectWriter(it, key, extra);
    } else {
      switch (_map[i].Type) {
        case SimpleProperties::InvalidValue:
          ok = true;
          break;
        case SimpleProperties::Uint32Value: {
          Uint32 val = *((const Uint32 *)src);
          ok = it.add(key, val);
        } break;
        case SimpleProperties::BinaryValue: {
          const char *src_len = _src + _map[i].Length_Offset;
          Uint32 len = *((const Uint32 *)src_len);
          if ((!ignoreMinMax) && _map[i].maxLength && len > _map[i].maxLength)
            return ValueTooLong;
          ok = it.add(key, src, len);
          break;
        }
        case SimpleProperties::StringValue:
          if ((!ignoreMinMax) && _map[i].maxLength &&
              strlen(src) > _map[i].maxLength)
            return ValueTooLong;
          ok = it.add(key, src);
          break;
      }
    }
    if (!ok) return OutOfMemory;
  }

  return Eof;
}

void SimpleProperties::Reader::printAll(NdbOut &ndbout) {
  char tmp[MAX_LOG_MESSAGE_SIZE];
  for (first(); valid(); next()) {
    switch (getValueType()) {
      case SimpleProperties::Uint32Value:
        ndbout << "Key: " << getKey() << " value(" << getValueLen()
               << ") : " << getUint32() << endl;
        break;
      case SimpleProperties::BinaryValue:
      case SimpleProperties::StringValue:
        if (getValueLen() < MAX_LOG_MESSAGE_SIZE) {
          getString(tmp);
          ndbout << "Key: " << getKey() << " value(" << getValueLen() << ") : "
                 << "\"" << tmp << "\"" << endl;
        } else {
          ndbout << "Key: " << getKey() << " value(" << getValueLen() << ") : "
                 << "\""
                 << "<TOO LONG>"
                 << "\"" << endl;
        }
        break;
      default:
        ndbout << "Unknown type for key: " << getKey()
               << " type: " << (Uint32)getValueType() << endl;
    }
  }
}

void SimpleProperties::Reader::printAll(EventLogger *logger) {
  char tmp[MAX_LOG_MESSAGE_SIZE];
  for (first(); valid(); next()) {
    switch (getValueType()) {
      case SimpleProperties::Uint32Value:
        logger->info("Key: %u value(%u) : %u", getKey(), getValueLen(),
                     getUint32());
        break;
      case SimpleProperties::BinaryValue:
      case SimpleProperties::StringValue:
        if (getValueLen() < MAX_LOG_MESSAGE_SIZE) {
          getString(tmp);
          logger->info("Key: %u value(%u) : \"%s\"", getKey(), getValueLen(),
                       tmp);
        } else {
          logger->info("Key: %u value(%u) : \"<TOO LONG>\"", getKey(),
                       getValueLen());
        }
        break;
      default:
        logger->info("Unknown type for key: %u type: %u", getKey(),
                     (Uint32)getValueType());
    }
  }
}
SimplePropertiesLinearReader::SimplePropertiesLinearReader(const Uint32 *src,
                                                           Uint32 len) {
  m_src = src;
  m_len = len;
  m_pos = 0;
  first();
}

void SimplePropertiesLinearReader::reset() { m_pos = 0; }

bool SimplePropertiesLinearReader::step(Uint32 len) {
  m_pos += len;
  return m_pos < m_len;
}

bool SimplePropertiesLinearReader::getWord(Uint32 *dst) {
  if (m_pos < m_len) {
    *dst = m_src[m_pos++];
    return true;
  }
  return false;
}

bool SimplePropertiesLinearReader::peekWord(Uint32 *dst) const {
  if (m_pos < m_len) {
    *dst = m_src[m_pos];
    return true;
  }
  return false;
}

bool SimplePropertiesLinearReader::peekWords(Uint32 *dst, Uint32 len) const {
  if (m_pos + len <= m_len) {
    memcpy(dst, &m_src[m_pos], 4 * len);
    return true;
  }
  return false;
}

LinearWriter::LinearWriter(Uint32 *src, Uint32 len) {
  m_src = src;
  m_len = len;
  reset();
}

bool LinearWriter::reset() {
  m_pos = 0;
  return m_len > 0;
}

bool LinearWriter::putWord(Uint32 val) {
  if (m_pos < m_len) {
    m_src[m_pos++] = val;
    return true;
  }
  return false;
}

bool LinearWriter::putWords(const Uint32 *src, Uint32 len) {
  if (m_pos + len <= m_len) {
    memcpy(&m_src[m_pos], src, 4 * len);
    m_pos += len;
    return true;
  }
  return false;
}

Uint32 LinearWriter::getWordsUsed() const { return m_pos; }

UtilBufferWriter::UtilBufferWriter(UtilBuffer &b) : m_buf(b) { reset(); }

bool UtilBufferWriter::reset() {
  m_buf.clear();
  return true;
}

bool UtilBufferWriter::putWord(Uint32 val) {
  return (m_buf.append(&val, 4) == 0);
}

bool UtilBufferWriter::putWords(const Uint32 *src, Uint32 len) {
  return (m_buf.append(src, 4 * len) == 0);
}

Uint32 UtilBufferWriter::getWordsUsed() const { return m_buf.length() / 4; }

#if 0
LinearPagesReader::LinearPagesReader(const Uint32 * base, 
				     Uint32 pageSize, 
				     Uint32 headerSize,
				     Uint32 noOfPages, 
				     Uint32 len){
  m_base = base;
  m_pageSz = pageSize;
  m_noOfPages = noOfPages;
  m_pageHeaderSz = headerSize;
  m_len = len;
  reset();
}

void 
LinearPagesReader::reset() { m_pos = 0;}

bool 
LinearPagesReader::step(Uint32 len){
  m_pos += len;
  return m_pos < m_len;
}

bool 
LinearPagesReader::getWord(Uint32 * dst) { 
  if(m_pos<m_len){
    * dst = m_base[getPos(m_pos++)];
    return true;
  } 
  return false;
}

bool 
LinearPagesReader::peekWord(Uint32 * dst) const {
  if(m_pos<m_len){
    * dst = m_base[getPos(m_pos)];
    return true;
  } 
  return false;
}

bool 
LinearPagesReader::peekWords(Uint32 * dst, Uint32 len) const {
  if(m_pos + len <= m_len){
    for(Uint32 i = 0; i<len; i++)
      * (dst + i) = m_base[getPos(m_pos + i)];
    return true;
  }
  return false;
}

Uint32 
LinearPagesReader::getPos(Uint32 pos) const {
  const Uint32 sz = (m_pageSz - m_pageHeaderSz);
  Uint32 no = pos / sz;
  Uint32 in = pos % sz;
  return no * m_pageSz + m_pageHeaderSz + in;
}

LinearPagesWriter::LinearPagesWriter(Uint32 * base, 
				     Uint32 pageSize, 
				     Uint32 noOfPages, 
				     Uint32 headerSize){
  m_base = base;
  m_pageSz = pageSize;
  m_noOfPages = noOfPages;
  m_pageHeaderSz = headerSize;
  m_len = noOfPages * (pageSize - headerSize);
  reset();
}

bool 
LinearPagesWriter::putWord(Uint32 val){
  if(m_pos < m_len){
    m_base[getPos(m_pos++)] = val;
    return true;
  }
  return false;
}

bool 
LinearPagesWriter::putWords(const Uint32 * src, Uint32 len){
  if(m_pos + len <= m_len){
    for(Uint32 i = 0; i<len; i++)
      m_base[getPos(m_pos++)] = src[i];
    return true;
  }
  return false;
}

#if 0
Uint32 
LinearPagesWriter::getWordsUsed() const { 
  return getPos(m_pos);
}
#endif

Uint32 
LinearPagesWriter::getPagesUsed() const { 
  return m_pos / (m_pageSz - m_pageHeaderSz);
}

Uint32 
LinearPagesWriter::getPos(Uint32 pos) const {
  const Uint32 sz = (m_pageSz - m_pageHeaderSz);
  Uint32 no = pos / sz;
  Uint32 in = pos % sz;
  return no * m_pageSz + m_pageHeaderSz + in;
}
#endif
