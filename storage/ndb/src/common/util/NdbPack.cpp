/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>
#include <NdbPack.hpp>
#include <NdbOut.hpp>
#include <NdbEnv.h>

// NdbPack::Error

int
NdbPack::Error::get_error_code() const
{
  return m_error_code;
}

int
NdbPack::Error::get_error_line() const
{
  return m_error_line;
}

void
NdbPack::Error::set_error(int code, int line) const
{
  m_error_code = code;
  m_error_line = line;
#ifdef VM_TRACE
  const char* p = NdbEnv_GetEnv("NDB_PACK_ABORT_ON_ERROR", (char*)0, 0);
  if (p != 0 && strchr("1Y", p[0]) != 0)
    require(false);
#endif
}

void
NdbPack::Error::set_error(const Error& e2) const
{
  set_error(e2.m_error_code, e2.m_error_line);
}

// NdbPack::Endian

void
NdbPack::Endian::convert(void* ptr, Uint32 len)
{
  Uint8* p = (Uint8*)ptr;
  for (Uint32 i = 0; i < len / 2; i++)
  {
    Uint32 j = len - i - 1;
    Uint8 tmp = p[i];
    p[i] = p[j];
    p[j] = tmp;
  }
}

// NdbPack::Type

struct Ndb_pack_type_info {
  bool m_supported;
  Uint16 m_fixSize;     // if non-zero must have this exact size
  Uint16 m_arrayType;   // 0,1,2 length bytes
  bool m_charType;      // type with character set
  bool m_convert;       // convert endian (reverse byte order)
};

static const Ndb_pack_type_info
g_ndb_pack_type_info[] = {
  { 0, 0, 0, 0, 0 }, // NDB_TYPE_UNDEFINED
  { 1, 1, 0, 0, 1 }, // NDB_TYPE_TINYINT
  { 1, 1, 0, 0, 1 }, // NDB_TYPE_TINYUNSIGNED
  { 1, 2, 0, 0, 1 }, // NDB_TYPE_SMALLINT
  { 1, 2, 0, 0, 1 }, // NDB_TYPE_SMALLUNSIGNED
  { 1, 3, 0, 0, 1 }, // NDB_TYPE_MEDIUMINT
  { 1, 3, 0, 0, 1 }, // NDB_TYPE_MEDIUMUNSIGNED
  { 1, 4, 0, 0, 1 }, // NDB_TYPE_INT
  { 1, 4, 0, 0, 1 }, // NDB_TYPE_UNSIGNED
  { 1, 8, 0, 0, 1 }, // NDB_TYPE_BIGINT
  { 1, 8, 0, 0, 1 }, // NDB_TYPE_BIGUNSIGNED
  { 1, 4, 0, 0, 1 }, // NDB_TYPE_FLOAT
  { 1, 8, 0, 0, 1 }, // NDB_TYPE_DOUBLE
  { 1, 0, 0, 0, 0 }, // NDB_TYPE_OLDDECIMAL
  { 1, 0, 0, 1, 0 }, // NDB_TYPE_CHAR
  { 1, 0, 1, 1, 0 }, // NDB_TYPE_VARCHAR
  { 1, 0, 0, 0, 0 }, // NDB_TYPE_BINARY
  { 1, 0, 1, 0, 0 }, // NDB_TYPE_VARBINARY
  { 1, 8, 0, 0, 0 }, // NDB_TYPE_DATETIME
  { 1, 3, 0, 0, 0 }, // NDB_TYPE_DATE
  { 0, 0, 0, 0, 0 }, // NDB_TYPE_BLOB
  { 0, 0, 0, 1, 0 }, // NDB_TYPE_TEXT
  { 0, 0, 0, 0, 0 }, // NDB_TYPE_BIT
  { 1, 0, 2, 1, 0 }, // NDB_TYPE_LONGVARCHAR
  { 1, 0, 2, 0, 0 }, // NDB_TYPE_LONGVARBINARY
  { 1, 3, 0, 0, 0 }, // NDB_TYPE_TIME
  { 1, 1, 0, 0, 0 }, // NDB_TYPE_YEAR
  { 1, 4, 0, 0, 0 }, // NDB_TYPE_TIMESTAMP
  { 1, 0, 0, 0, 0 }, // NDB_TYPE_OLDDECIMALUNSIGNED
  { 1, 0, 0, 0, 0 }, // NDB_TYPE_DECIMAL
  { 1, 0, 0, 0, 0 }  // NDB_TYPE_DECIMALUNSIGNED
};

static const int g_ndb_pack_type_info_cnt =
  sizeof(g_ndb_pack_type_info) / sizeof(g_ndb_pack_type_info[0]);

int
NdbPack::Type::complete()
{
  if (m_typeId == 0)
  {
    set_error(TypeNotSet, __LINE__);
    return -1;
  }
  if (m_typeId >= g_ndb_pack_type_info_cnt)
  {
    set_error(TypeNotSet, __LINE__);
    return -1;
  }
  const Ndb_pack_type_info& info = g_ndb_pack_type_info[m_typeId];
  if (!info.m_supported)
  {
    set_error(TypeNotSupported, __LINE__);
    return -1;
  }
  if (m_byteSize == 0)
  {
    set_error(TypeSizeZero, __LINE__);
    return -1;
  }
  if (info.m_fixSize != 0 && m_byteSize != info.m_fixSize)
  {
    set_error(TypeFixSizeInvalid, __LINE__);
    return -1;
  }
  if (!(m_nullable <= 1))
  {
    set_error(TypeNullableNotBool, __LINE__);
    return -1;
  }
  if (info.m_charType && m_csNumber == 0)
  {
    set_error(CharsetNotSpecified, __LINE__);
    return -1;
  }
  if (info.m_charType && all_charsets[m_csNumber] == 0)
  {
    CHARSET_INFO* cs = get_charset(m_csNumber, MYF(0));
    if (cs == 0)
    {
      set_error(CharsetNotFound, __LINE__);
      return -1;
    }
    all_charsets[m_csNumber] = cs; // yes caller must do this
  }
  if (!info.m_charType && m_csNumber != 0)
  {
    set_error(CharsetNotAllowed, __LINE__);
    return -1;
  }
  m_arrayType = info.m_arrayType;
  return 0;
}

// NdbPack::Spec

int
NdbPack::Spec::add(Type type)
{
  Uint32 cnt = m_cnt;
  Uint32 nullable_cnt = m_nullableCnt;
  Uint32 varsize_cnt = m_varsizeCnt;
  Uint32 max_byte_size = m_maxByteSize;
  if (type.complete() == -1)
  {
    set_error(type);
    return -1;
  }
  type.m_nullbitPos = 0xFFFF;
  if (type.m_nullable)
  {
    type.m_nullbitPos = nullable_cnt;
    nullable_cnt++;
  }
  if (type.m_arrayType != 0)
  {
    varsize_cnt++;
  }
  max_byte_size += type.m_byteSize;
  if (cnt >= m_bufMaxCnt)
  {
    set_error(SpecBufOverflow, __LINE__);
    return -1;
  }
  m_buf[cnt] = type;
  cnt++;
  m_cnt = cnt;
  m_nullableCnt = nullable_cnt;
  m_varsizeCnt = varsize_cnt;
  m_maxByteSize = max_byte_size;
  return 0;
}

int
NdbPack::Spec::add(Type type, Uint32 cnt)
{
  for (Uint32 i = 0; i < cnt; i++)
  {
    if (add(type) == -1)
      return -1;
  }
  return 0;
}

void
NdbPack::Spec::copy(const Spec& s2)
{
  assert(m_bufMaxCnt >= s2.m_cnt);
  reset();
  m_cnt = s2.m_cnt;
  m_nullableCnt = s2.m_nullableCnt;
  m_varsizeCnt = s2.m_varsizeCnt;
  m_maxByteSize = s2.m_maxByteSize;
  for (Uint32 i = 0; i < m_cnt; i++)
  {
    m_buf[i] = s2.m_buf[i];
  }
}

// NdbPack::Iter

int
NdbPack::Iter::desc(const Uint8* item)
{
  const Uint32 i = m_cnt; // item index
  assert(i < m_spec.m_cnt);
  const Type& type = m_spec.m_buf[i];
  const Uint32 lenBytes = type.m_arrayType;
  Uint32 bareLen = 0;
  switch (lenBytes) {
  case 0:
    bareLen = type.m_byteSize;
    break;
  case 1:
    bareLen = item[0];
    break;
  case 2:
    bareLen = item[0] + (item[1] << 8);
    break;
  default:
    assert(false);
    set_error(InternalError, __LINE__);
    return -1;
  }
  const Uint32 itemLen = lenBytes + bareLen;
  if (itemLen > type.m_byteSize)
  {
    set_error(DataValueOverflow, __LINE__);
    return -1;
  }
  m_itemPos += m_itemLen; // skip previous item
  m_cnt++;
  m_lenBytes = lenBytes;
  m_bareLen = bareLen;
  m_itemLen = itemLen;
  return 0;
}

int
NdbPack::Iter::desc_null()
{
  assert(m_cnt < m_spec.m_cnt);
  // caller checks if null allowed
  m_itemPos += m_itemLen; // skip previous item
  m_cnt++;
  m_nullCnt++;
  m_lenBytes = 0;
  m_bareLen = 0;
  m_itemLen = 0;
  return 0;
}

int
NdbPack::Iter::cmp(const Iter& r2, const Uint8* buf1, const Uint8* buf2) const
{
  const Iter& r1 = *this;
  assert(&r1.m_spec == &r2.m_spec);
  assert(r1.m_cnt == r2.m_cnt && r1.m_cnt > 0);
  const Uint32 i = r1.m_cnt - 1; // item index
  int res = 0;
  const Uint32 n1 = r1.m_itemLen;
  const Uint32 n2 = r2.m_itemLen;
  if (n1 != 0)
  {
    if (n2 != 0)
    {
      const Type& type = r1.m_spec.m_buf[i];
      const NdbSqlUtil::Type& sqlType = getSqlType(type.m_typeId);
      const Uint8* p1 = &buf1[r1.m_itemPos];
      const Uint8* p2 = &buf2[r2.m_itemPos];
      CHARSET_INFO* cs = all_charsets[type.m_csNumber];
      res = (*sqlType.m_cmp)(cs, p1, n1, p2, n2);
    }
    else
    {
      res = +1;
    }
  }
  else
  {
    if (n2 != 0)
      res = -1;
  }
  return res;
}

// NdbPack::DataC

int
NdbPack::DataC::desc(Iter& r) const
{
  const Uint32 i = r.m_cnt; // item index
  assert(i < m_cnt);
  const Type& type = m_spec.m_buf[i];
  if (type.m_nullable || m_allNullable)
  {
    Uint32 nullbitPos = 0;
    if (!m_allNullable)
      nullbitPos = type.m_nullbitPos;
    else
      nullbitPos = i;
    const Uint32 byte_pos = nullbitPos / 8;
    const Uint32 bit_pos = nullbitPos % 8;
    const Uint8 bit_mask = (1 << bit_pos);
    const Uint8& the_byte = m_buf[byte_pos];
    if ((the_byte & bit_mask) != 0)
    {
      if (r.desc_null() == -1)
      {
        set_error(r);
        return -1;
      }
      return 0;
    }
  }
  const Uint32 pos = r.m_itemPos + r.m_itemLen;
  const Uint8* item = &m_buf[pos];
  if (r.desc(item) == -1)
  {
    set_error(r);
    return -1;
  }
  return 0;
}

int
NdbPack::DataC::cmp(const DataC& d2, Uint32 cnt, Uint32& num_eq) const
{
  const DataC& d1 = *this;
  assert(cnt <= d1.m_cnt);
  assert(cnt <= d2.m_cnt);
  Iter r1(d1);
  Iter r2(d2);
  int res = 0;
  Uint32 i; // remember last
  for (i = 0; i < cnt; i++)
  {
    d1.desc(r1);
    d2.desc(r2);
    res = r1.cmp(r2, d1.m_buf, d2.m_buf);
    if (res != 0)
      break;
  }
  num_eq = i;
  return res;
}

// NdbPack::Data

int
NdbPack::Data::add(const void* data, Uint32* len_out)
{
  assert(data != 0);
  const Uint8* item = (const Uint8*)data;
  const Uint32 i = m_cnt; // item index
  if (i >= m_spec.m_cnt)
  {
    set_error(DataCntOverflow, __LINE__);
    return -1;
  }
  Iter& r = m_iter;
  assert(r.m_cnt == i);
  const Uint32 fullLen = m_varBytes + r.m_itemPos + r.m_itemLen;
  if (r.desc(item) == -1)
  {
    set_error(r);
    return -1;
  }
  if (fullLen + r.m_itemLen > m_bufMaxLen)
  {
    set_error(DataBufOverflow, __LINE__);
    return -1;
  }
  memcpy(&m_buf[fullLen], item, r.m_itemLen);
  *len_out = r.m_itemLen;
  m_cnt++;
  return 0;
}

int
NdbPack::Data::add(const void* data, Uint32 cnt, Uint32* len_out)
{
  const Uint8* data_ptr = (const Uint8*)data;
  Uint32 len_tot = 0;
  for (Uint32 i = 0; i < cnt; i++)
  {
    Uint32 len;
    if (add(data_ptr, &len) == -1)
      return -1;
    if (data != 0)
      data_ptr += len;
    len_tot += len;
  }
  *len_out = len_tot;
  return 0;
}

int
NdbPack::Data::add_null(Uint32* len_out)
{
  const Uint32 i = m_cnt; // item index
  if (i >= m_spec.m_cnt)
  {
    set_error(DataCntOverflow, __LINE__);
    return -1;
  }
  Iter& r = m_iter;
  assert(r.m_cnt == i);
  if (r.desc_null() == -1)
  {
    set_error(r);
    return -1;
  }
  Uint32 nullbitPos = 0;
  if (!m_allNullable)
  {
    const Type& type = m_spec.m_buf[i];
    if (!type.m_nullable)
    {
      set_error(DataNotNullable, __LINE__);
      return -1;
    }
    nullbitPos = type.m_nullbitPos;
  }
  else
  {
    nullbitPos = i;
  }
  const Uint32 byte_pos = nullbitPos / 8;
  const Uint32 bit_pos = nullbitPos % 8;
  const Uint8 bit_mask = (1 << bit_pos);
  Uint8& the_byte = m_buf[m_varBytes + byte_pos];
  assert((the_byte & bit_mask) == 0);
  the_byte |= bit_mask;
  *len_out = r.m_itemLen;
  m_cnt++;
  return 0;
}

int
NdbPack::Data::add_null(Uint32 cnt, Uint32* len_out)
{
  Uint32 len_tot = 0;
  for (Uint32 i = 0; i < cnt; i++)
  {
    Uint32 len;
    if (add_null(&len) == -1)
      return -1;
    len_tot += len;
  }
  *len_out = len_tot;
  return 0;
}

int
NdbPack::Data::add_poai(const Uint32* poai, Uint32* len_out)
{
  const AttributeHeader ah = *(const AttributeHeader*)&poai[0];
  if (!ah.isNULL())
  {
    if (add(&poai[1], len_out) == -1)
      return -1;
  }
  else
  {
    if (add_null(len_out) == -1)
      return -1;
  }
  if (ah.getByteSize() != *len_out)
  {
    set_error(InvalidAttrInfo, __LINE__);
    return -1;
  }
  return 0;
}

int
NdbPack::Data::add_poai(const Uint32* poai, Uint32 cnt, Uint32* len_out)
{
  Uint32 len_tot = 0;
  for (Uint32 i = 0; i < cnt; i++)
  {
    Uint32 len;
    if (add_poai(poai, &len) == -1)
      return -1;
    len_tot += len;
    poai += 1 + (len + 3) / 4;
  }
  *len_out = len_tot;
  return 0;
}

int
NdbPack::Data::finalize_impl()
{
  const Uint32 dataLen = m_iter.m_itemPos + m_iter.m_itemLen;
  switch (m_varBytes) {
  // case 0: inlined
  case 1:
    if (dataLen <= 0xFF)
    {
      m_buf[0] = dataLen;
      return 0;
    }
    break;
  case 2:
    if (dataLen <= 0xFFFF)
    {
      m_buf[0] = (dataLen & 0xFF);
      m_buf[1] = (dataLen >> 8);
      return 0;
    }
    break;
  default:
    break;
  }
  set_error(InternalError, __LINE__);
  return -1;
}

int
NdbPack::Data::desc_all(Uint32 cnt, Endian::Value from_endian)
{
  if (from_endian == NdbPack::Endian::Native)
    from_endian = NdbPack::Endian::get_endian();
  m_endian = from_endian;
  assert(m_cnt == 0); // reset() would destroy nullmask
  for (Uint32 i = 0; i < cnt; i++)
  {
    m_cnt++;
    if (desc(m_iter) == -1)
      return -1;
  }
  if (finalize() == -1)
    return -1;
  return 0;
}

int
NdbPack::Data::copy(const DataC& d2)
{
  reset();
  Iter r2(d2);
  const Uint32 cnt2 = d2.m_cnt;
  for (Uint32 i = 0; i < cnt2; i++)
  {
    if (d2.desc(r2) == -1)
      return -1;
    Uint32 len_out = ~(Uint32)0;
    if (r2.m_itemLen != 0)
    {
      if (add(&d2.m_buf[r2.m_itemPos], &len_out) == -1)
          return -1;
      assert(len_out == r2.m_itemLen);
    }
    else
    {
      if (add_null(&len_out) == -1)
        return -1;
      assert(len_out ==0);
    }
  }
  if (finalize() == -1)
    return -1;
  return 0;
}

int
NdbPack::Data::convert_impl(Endian::Value to_endian)
{
  const Spec& spec = m_spec;
  Iter r(*this);
  for (Uint32 i = 0; i < m_cnt; i++)
  {
    if (DataC::desc(r) == -1)
    {
      set_error(r);
      return -1;
    }
    const Type& type = spec.m_buf[i];
    const Uint32 typeId = type.m_typeId;
    const Ndb_pack_type_info& info = g_ndb_pack_type_info[typeId];
    if (info.m_convert)
    {
      Uint8* ptr = &m_buf[m_varBytes + r.m_itemPos];
      Uint32 len = r.m_itemLen;
      Endian::convert(ptr, len);
    }
  }
  return 0;
}

// NdbPack::BoundC

int
NdbPack::BoundC::finalize(int side)
{
  if (m_data.m_cnt == 0 && side != 0)
  {
    set_error(BoundEmptySide, __LINE__);
    return -1;
  }
  if (m_data.m_cnt != 0 && side != -1 && side != +1)
  {
    set_error(BoundNonemptySide, __LINE__);
    return -1;
  }
  m_side = side;
  return 0;
}

int
NdbPack::BoundC::cmp(const BoundC& b2, Uint32 cnt, Uint32& num_eq) const
{
  const BoundC& b1 = *this;
  const DataC& d1 = b1.m_data;
  const DataC& d2 = b2.m_data;
  int res = d1.cmp(d2, cnt, num_eq);
  if (res == 0)
  {
    if (cnt < d1.m_cnt && cnt < d2.m_cnt)
      ;
    else if (d1.m_cnt < d2.m_cnt)
      res = (+1) * b1.m_side;
    else if (d1.m_cnt > d2.m_cnt)
      res = (-1) * b2.m_side;
    else if (b1.m_side < b2.m_side)
      res = -1;
    else if (b1.m_side > b2.m_side)
      res = +1;
  }
  return res;
}

// NdbPack::Bound

// print

NdbPack::Print::Print(char* buf, Uint32 bufsz) :
  m_buf(buf), m_bufsz(bufsz), m_sz(0) {}

void
NdbPack::Print::print(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  if (m_bufsz > m_sz)
  {
    BaseString::vsnprintf(&m_buf[m_sz], m_bufsz - m_sz, fmt, ap);
    m_sz += (Uint32)strlen(&m_buf[m_sz]);
  }
  va_end(ap);
}

// print Type

NdbOut&
operator<<(NdbOut& out, const NdbPack::Type& a)
{
  a.print(out);
  return out;
}

void
NdbPack::Type::print(NdbOut& out) const
{
  char buf[200];
  out << print(buf, sizeof(buf));
}

const char*
NdbPack::Type::print(char* buf, Uint32 bufsz) const
{
  Print p(buf, bufsz);
  p.print("typeId:%u", m_typeId);
  p.print(" byteSize:%u", m_byteSize);
  p.print(" nullable:%u", m_nullable);
  p.print(" csNumber:%u", m_csNumber);
  return buf;
}

// print Spec

NdbOut&
operator<<(NdbOut& out, const NdbPack::Spec& a)
{
  a.print(out);
  return out;
}

void
NdbPack::Spec::print(NdbOut& out) const
{
  char buf[8000];
  out << print(buf, sizeof(buf));
}

const char*
NdbPack::Spec::print(char* buf, Uint32 bufsz) const
{
  Print p(buf, bufsz);
  p.print("cnt:%u", m_cnt);
  p.print(" nullableCnt:%u", m_nullableCnt);
  p.print(" varsizeCnt:%u", m_varsizeCnt);
  p.print(" nullmaskLen:%u", get_nullmask_len(false));
  p.print(" maxByteSize:%u", m_maxByteSize);
  for (Uint32 i = 0; i < m_cnt; i++)
  {
    const Type& type = m_buf[i];
    p.print(" [%u", i);
    p.print(" typeId:%u", type.m_typeId);
    p.print(" nullable:%u", type.m_nullable);
    p.print(" byteSize:%u", type.m_byteSize);
    p.print(" csNumber:%u", type.m_csNumber);
    p.print("]");
  }
  return buf;
}

// print DataC

bool g_ndb_pack_print_hex_always = true;

NdbOut&
operator<<(NdbOut& out, const NdbPack::DataC& a)
{
  a.print(out);
  return out;
}

void
NdbPack::DataC::print(NdbOut& out) const
{
  char buf[8000];
  out << print(buf, sizeof(buf));
}

const char*
NdbPack::DataC::print(char* buf, Uint32 bufsz, bool convert_flag) const
{
  Print p(buf, bufsz);
  const Spec& spec = m_spec;
  const Uint32 nullmask_len = spec.get_nullmask_len(m_allNullable);
  if (nullmask_len != 0)
  {
    p.print("nullmask:");
    for (Uint32 i = 0; i < nullmask_len; i++)
    {
      int x = m_buf[i];
      p.print("%02x", x);
    }
  }
  Iter r(*this);
  for (Uint32 i = 0; i < m_cnt; i++)
  {
    desc(r);
    const Uint8* value = &m_buf[r.m_itemPos];
    p.print(" [%u", i);
    p.print(" pos:%u", r.m_itemPos);
    p.print(" len:%u", r.m_itemLen);
    if (r.m_itemLen > 0)
    {
      p.print(" value:");
      // some specific types for debugging
      const Type& type = spec.m_buf[i];
      bool ok = true;
      switch (type.m_typeId) {
      case NDB_TYPE_TINYINT:
        {
          Int8 x;
          memcpy(&x, value, 1);
          if (convert_flag)
            Endian::convert(&x, 1);
          p.print("%d", (int)x);
        }
        break;
      case NDB_TYPE_TINYUNSIGNED:
        {
          Uint8 x;
          memcpy(&x, value, 1);
          if (convert_flag)
            Endian::convert(&x, 1);
          p.print("%u", (uint)x);
        }
        break;
      case NDB_TYPE_SMALLINT:
        {
          Int16 x;
          memcpy(&x, value, 2);
          if (convert_flag)
            Endian::convert(&x, 2);
          p.print("%d", (int)x);
        }
        break;
      case NDB_TYPE_SMALLUNSIGNED:
        {
          Uint16 x;
          memcpy(&x, value, 2);
          if (convert_flag)
            Endian::convert(&x, 2);
          p.print("%u", (uint)x);
        }
        break;
      case NDB_TYPE_INT:
        {
          Int32 x;
          memcpy(&x, value, 4);
          if (convert_flag)
            Endian::convert(&x, 4);
          p.print("%d", (int)x);
        }
        break;
      case NDB_TYPE_UNSIGNED:
        {
          Uint32 x;
          memcpy(&x, value, 4);
          if (convert_flag)
            Endian::convert(&x, 4);
          p.print("%u", (uint)x);
        }
        break;
      case NDB_TYPE_FLOAT:
        {
          float x;
          memcpy(&x, value, 4);
          if (convert_flag)
            Endian::convert(&x, 4);
          p.print("%g", (double)x);
        }
        break;
      case NDB_TYPE_DOUBLE:
        {
          double x;
          memcpy(&x, value, 8);
          if (convert_flag)
            Endian::convert(&x, 8);
          p.print("%g", x);
        }
        break;
      case NDB_TYPE_CHAR:
      case NDB_TYPE_VARCHAR:
      case NDB_TYPE_LONGVARCHAR:
        {
          const Uint32 off = type.m_arrayType;
          for (Uint32 j = 0; j < r.m_bareLen; j++)
          {
            Uint8 x = value[off + j];
            p.print("%c", (int)x);
          }
        }
        break;
      default:
        ok = false;
        break;
      }
      if (!ok || g_ndb_pack_print_hex_always)
      {
        p.print("<");
        for (Uint32 j = 0; j < r.m_itemLen; j++)
        {
          int x = value[j];
          p.print("%02x", x);
        }
        p.print(">");
      }
    }
    p.print("]");
  }
  return buf;
}

// print Data

NdbOut&
operator<<(NdbOut& out, const NdbPack::Data& a)
{
  a.print(out);
  return out;
}

void
NdbPack::Data::print(NdbOut& out) const
{
  char buf[8000];
  out << print(buf, sizeof(buf));
}

const char*
NdbPack::Data::print(char* buf, Uint32 bufsz) const
{
  Print p(buf, bufsz);
  char* ptr = buf;
  if (m_varBytes != 0)
  {
    p.print("varBytes:");
    for (Uint32 i = 0; i < m_varBytes; i++)
    {
      int r = m_buf[i];
      p.print("%02x", r);
    }
    p.print(" ");
  }
  p.print("dataLen:%u", m_iter.m_itemPos + m_iter.m_itemLen);
  p.print(" ");
  const bool convert_flag =
    m_endian != Endian::Native &&
    m_endian != Endian::get_endian();
  DataC::print(&buf[p.m_sz], bufsz - p.m_sz, convert_flag);
  return buf;
}

// print BoundC

NdbOut&
operator<<(NdbOut& out, const NdbPack::BoundC& a)
{
  a.print(out);
  return out;
}

void
NdbPack::BoundC::print(NdbOut& out) const
{
  char buf[8000];
  out << print(buf, sizeof(buf));
}

const char*
NdbPack::BoundC::print(char* buf, Uint32 bufsz) const
{
  Print p(buf, bufsz);
  p.print("side:%s ", m_side < 0 ? "-" : m_side > 0 ? "+" : "0");
  m_data.print(&buf[p.m_sz], bufsz - p.m_sz);
  return buf;
}

// print Bound

NdbOut&
operator<<(NdbOut& out, const NdbPack::Bound& a)
{
  a.print(out);
  return out;
}

void
NdbPack::Bound::print(NdbOut& out) const
{
  char buf[8000];
  out << print(buf, sizeof(buf));
}

const char*
NdbPack::Bound::print(char* buf, Uint32 bufsz) const
{
  BoundC::print(buf, bufsz);
  return buf;
}

// validate

int
NdbPack::Type::validate() const
{
  Type type2 = *this;
  if (type2.complete() == -1)
  {
    set_error(type2);
    return -1;
  }
  if (memcmp(this, &type2, sizeof(Type)) != 0)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  return 0;
}

int
NdbPack::Spec::validate() const
{
  Uint32 nullableCnt = 0;
  Uint32 varsizeCnt = 0;
  for (Uint32 i = 0; i < m_cnt; i++)
  {
    const Type& type = m_buf[i];
    if (type.validate() == -1)
    {
      set_error(type);
      return -1;
    }
    if (type.m_nullable)
      nullableCnt++;
    if (type.m_arrayType != 0)
      varsizeCnt++;
  }
  if (m_nullableCnt != nullableCnt)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  if (m_varsizeCnt != varsizeCnt)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  return 0;
}

int
NdbPack::Data::validate() const
{
  if (DataC::validate() == -1)
    return -1;
  const Iter& r = m_iter;
  if (r.m_cnt != m_cnt)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  Iter r2(*this);
  for (Uint32 i = 0; i < m_cnt; i++)
  {
    if (desc(r2) == -1)
      return -1;
  }
  if (r.m_itemPos != r2.m_itemPos)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  if (r.m_cnt != r2.m_cnt)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  if (r.m_nullCnt != r2.m_nullCnt)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  if (r.m_itemLen != r2.m_itemLen)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  return 0;
}

int
NdbPack::BoundC::validate() const
{
  if (m_data.validate() == -1)
  {
    set_error(m_data);
    return -1;
  }
  if (m_data.m_cnt == 0 && m_side != 0)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  if (m_data.m_cnt != 0 && m_side != -1 && m_side != +1)
  {
    set_error(ValidationError, __LINE__);
    return -1;
  }
  return 0;
}

int
NdbPack::Bound::validate() const
{
  if (BoundC::validate() == -1)
    return -1;
  if (m_data.validate() == -1)
  {
    set_error(m_data);
    return -1;
  }
  return 0;
}

#ifdef TEST_NDB_PACK
#include <util/NdbTap.hpp>

#define chk1(x) do { if (x) break; ndbout << "line " << __LINE__ << ": " << #x << endl; require(false); } while (0)

#define chk2(x, e) do { if (x) break; ndbout << "line " << __LINE__ << ": " << #x << endl; ndbout << "NdbPack code: " << (e).get_error_code() << " line: " << (e).get_error_line() << endl; require(false); } while (0)

#define ll0(x) do { if (verbose < 0) break; ndbout << "0- " << x << endl; } while (0)
#define ll1(x) do { if (verbose < 1) break; ndbout << "1- " << x << endl; } while (0)
#define ll2(x) do { if (verbose < 2) break; ndbout << "2- " << x << endl; } while (0)
#define ll3(x) do { if (verbose < 3) break; ndbout << "3- " << x << endl; } while (0)

#define xmin(a, b) ((a) < (b) ? (a) : (b))

#include <ndb_rand.h>

static uint // random 0..n-1
getrandom(uint n)
{
  if (n != 0) {
    uint k = ndb_rand();
    return k % n;
  }
  return 0;
}

static uint // random 0..n-1 biased exponentially to smaller
getrandom(uint n, uint bias)
{
  assert(bias != 0);
  uint k = getrandom(n);
  bias--;
  while (bias != 0) {
    k = getrandom(k + 1);
    bias--;
  }
  return k;
}

static bool
getrandompct(uint pct)
{
  return getrandom(100) < pct;
}

// change in TAPTEST
static int seed = -1; // random
static int loops = 0;
static int spec_cnt = -1; // random
static int fix_type = 0; // all types
static int no_nullable = 0;
static int data_cnt = -1; // Max
static int bound_cnt = -1; // Max
static int verbose = 0;

struct Tspec {
  enum { Max = 100 };
  enum { MaxBuf = Max * 4000 };
  NdbPack::Spec m_spec;
  NdbPack::Type m_type[Max];
  Tspec() {
    m_spec.set_buf(m_type, Max);
  }
  void create();
};

static NdbOut&
operator<<(NdbOut& out, const Tspec& tspec)
{
  out << tspec.m_spec;
  return out;
}

void
Tspec::create()
{
  m_spec.reset();
  int cnt = spec_cnt == -1 ? 1 + getrandom(Tspec::Max, 3) : spec_cnt;
  int i = 0;
  while (i < cnt) {
    int typeId = fix_type;
    if (typeId == 0)
      typeId = getrandom(g_ndb_pack_type_info_cnt);
    const Ndb_pack_type_info& info = g_ndb_pack_type_info[typeId];
    switch (typeId) {
    case NDB_TYPE_INT:
    case NDB_TYPE_UNSIGNED:
    case NDB_TYPE_CHAR:
    case NDB_TYPE_VARCHAR:
    case NDB_TYPE_LONGVARCHAR:
      break;
    default:
      continue;
    }
    require(info.m_supported);
    int byteSize = 0;
    if (info.m_fixSize != 0)
      byteSize = info.m_fixSize;
    else if (info.m_arrayType == 0)
      byteSize = 1 + getrandom(128, 1);  // char(1-128)
    else if (info.m_arrayType == 1)
      byteSize = 1 + getrandom(256, 2);  // varchar(0-255)
    else if (info.m_arrayType == 2)
      byteSize = 2 + getrandom(1024, 3); // longvarchar(0-1023)
    else
      require(false);
    bool nullable = no_nullable ? false : getrandompct(50);
    int csNumber = 0;
    if (info.m_charType) {
      csNumber = 8; // should include ascii
    }
    NdbPack::Type type(typeId, byteSize, nullable, csNumber);
    chk2(m_spec.add(type) == 0, m_spec);
    i++;
  }
  chk2(m_spec.validate() == 0, m_spec);
}

struct Tdata {
  const Tspec& m_tspec;
  NdbPack::Data m_data;
  const bool m_isBound;
  int m_cnt;
  Uint8* m_xbuf;        // unpacked
  int m_xsize;
  int m_xoff[Tspec::Max];
  int m_xlen[Tspec::Max];
  bool m_xnull[Tspec::Max];
  int m_xnulls;
  Uint32* m_poaiBuf;    // plain old attr info
  int m_poaiSize;
  Uint8* m_packBuf;     // packed
  int m_packLen;
  Tdata(Tspec& tspec, bool isBound, uint varBytes) :
    m_tspec(tspec),
    m_data(tspec.m_spec, isBound, varBytes),
    m_isBound(isBound)
  {
    m_cnt = tspec.m_spec.get_cnt();
    m_xbuf = 0;
    m_poaiBuf = 0;
    m_packBuf = 0;
  }
  ~Tdata() {
    delete [] m_xbuf;
    delete [] m_poaiBuf;
    delete [] m_packBuf;
  }
  void create();
  void add();
  void finalize();
  // compare using unpacked data
  int xcmp(const Tdata& tdata2, int* num_eq) const;
};

static NdbOut&
operator<<(NdbOut& out, const Tdata& tdata)
{
  out << tdata.m_data;
  return out;
}

void
Tdata::create()
{
  union {
    Uint8 xbuf[Tspec::MaxBuf];
    Uint64 xbuf_align;
  };
  memset(xbuf, 0x3f, sizeof(xbuf));
  m_xsize = 0;
  m_xnulls = 0;
  Uint32 poaiBuf[Tspec::MaxBuf / 4];
  memset(poaiBuf, 0x5f, sizeof(poaiBuf));
  m_poaiSize = 0;
  m_packLen = m_data.get_var_bytes();
  m_packLen += (m_tspec.m_spec.get_nullable_cnt(m_isBound) + 7) / 8;
  int i = 0, j;
  while (i < m_cnt) {
    const NdbPack::Type& type = m_tspec.m_spec.get_type(i);
    const int typeId = type.get_type_id();
    const Ndb_pack_type_info& info = g_ndb_pack_type_info[typeId];
    m_xnull[i] = type.get_nullable() && getrandompct(25);
    m_xnull[i] = false;
    if (type.get_nullable() || m_isBound)
      m_xnull[i] = getrandompct(20);
    int pad = 0; // null-char pad not counted in xlen
    if (!m_xnull[i]) {
      m_xoff[i] = m_xsize;
      Uint8* xptr = &xbuf[m_xsize];
      switch (typeId) {
      case NDB_TYPE_INT:
        {
          Int32 x = getrandom(10);
          if (getrandompct(50))
            x = (-1) * x;
          memcpy(xptr, &x, 4);
          m_xlen[i] = info.m_fixSize;
        }
        break;
      case NDB_TYPE_UNSIGNED:
        {
          Uint32 x = getrandom(10);
          memcpy(xptr, &x, 4);
          m_xlen[i] = info.m_fixSize;
        }
        break;
      case NDB_TYPE_CHAR:
        {
          require(type.get_byte_size() >= 1);
          int max_len = type.get_byte_size();
          int len = getrandom(max_len + 1, 1);
          for (j = 0; j < len; j++)
          {
            xptr[j] = 'a' + getrandom(3);
          }
          for (j = len; j < max_len; j++)
          {
            xptr[j] = 0x20;
          }
          m_xlen[i] = max_len;
          xptr[max_len] = 0;
          pad = 1;
        }
        break;
      case NDB_TYPE_VARCHAR:
        {
          require(type.get_byte_size() >= 1);
          int max_len = type.get_byte_size() - 1;
          int len = getrandom(max_len, 2);
          require(len < 256);
          xptr[0] = len;
          for (j = 0; j < len; j++)
          {
            xptr[1 + j] = 'a' + getrandom(3);
          }
          m_xlen[i] = 1 + len;
          xptr[1 + len] = 0;
          pad = 1;
        }
        break;
      case NDB_TYPE_LONGVARCHAR:
        {
          require(type.get_byte_size() >= 2);
          int max_len = type.get_byte_size() - 2;
          int len = getrandom(max_len, 3);
          require(len < 256 * 256);
          xptr[0] = (len & 0xFF);
          xptr[1] = (len >> 8);
          for (j = 0; j < len; j++)
          {
            xptr[2 + j] = 'a' + getrandom(3);
          }
          m_xlen[i] = 2 + len;
          xptr[2 + len] = 0;
          pad = 1;
        }
        break;
      default:
        require(false);
        break;
      }
      m_xsize += m_xlen[i] + pad;
      while (m_xsize % 8 != 0)
        m_xsize++;
      m_packLen += m_xlen[i];
    } else {
      m_xoff[i] = -1;
      m_xlen[i] = 0;
      m_xnulls++;
    }
    require(m_xnull[i] == (m_xoff[i] == -1));
    require(m_xnull[i] == (m_xlen[i] == 0));
    AttributeHeader* ah = (AttributeHeader*)&poaiBuf[m_poaiSize];
    ah->setAttributeId(i); // not used
    ah->setByteSize(m_xlen[i]);
    m_poaiSize++;
    if (!m_xnull[i]) {
      memcpy(&poaiBuf[m_poaiSize], &xbuf[m_xoff[i]], m_xlen[i]);
      m_poaiSize += (m_xlen[i] + 3) / 4;
    }
    i++;
  }
  require(m_xsize % 8 == 0);
  m_xbuf = (Uint8*) new Uint64 [m_xsize / 8];
  memcpy(m_xbuf, xbuf, m_xsize);
  m_poaiBuf = (Uint32*) new Uint32 [m_poaiSize];
  memcpy(m_poaiBuf, poaiBuf, m_poaiSize << 2);
}

void
Tdata::add()
{
  m_packBuf = new Uint8 [m_packLen];
  m_data.set_buf(m_packBuf, m_packLen);
  int i, j;
  j = 0;
  while (j <= 1) {
    if (j == 1)
      m_data.reset();
    i = 0;
    while (i < m_cnt) {
      Uint32 xlen = ~(Uint32)0;
      if (!m_xnull[i]) {
        int xoff = m_xoff[i];
        const Uint8* xptr = &m_xbuf[xoff];
        chk2(m_data.add(xptr, &xlen) == 0, m_data);
        chk1((int)xlen == m_xlen[i]);
      } else {
        chk2(m_data.add_null(&xlen) == 0, m_data);
        chk1(xlen == 0);
      }
      i++;
    }
    chk2(m_data.validate() == 0, m_data);
    chk1((int)m_data.get_null_cnt() == m_xnulls);
    j++;
  }
}

void
Tdata::finalize()
{
  chk2(m_data.finalize() == 0, m_data);
  ll3("create: " << m_data);
  chk1((int)m_data.get_full_len() == m_packLen);
  {
    const Uint8* p = (const Uint8*)m_data.get_full_buf();
    chk1(p[0] + (p[1] << 8) == m_packLen - 2);
  }
}

int
Tdata::xcmp(const Tdata& tdata2, int* num_eq) const
{
  const Tdata& tdata1 = *this;
  require(&tdata1.m_tspec == &tdata2.m_tspec);
  const Tspec& tspec = tdata1.m_tspec;
  int res = 0;
  int cnt = xmin(tdata1.m_cnt, tdata2.m_cnt);
  int i;
  for (i = 0; i < cnt; i++) {
    if (!tdata1.m_xnull[i]) {
      if (!tdata2.m_xnull[i]) {
        // the pointers are Uint64-aligned
        const Uint8* xptr1 = &tdata1.m_xbuf[tdata1.m_xoff[i]];
        const Uint8* xptr2 = &tdata2.m_xbuf[tdata2.m_xoff[i]];
        const int xlen1 = tdata1.m_xlen[i];
        const int xlen2 = tdata2.m_xlen[i];
        const NdbPack::Type& type = tspec.m_spec.get_type(i);
        const int typeId = type.get_type_id();
        const int csNumber = type.get_cs_number();
        CHARSET_INFO* cs = all_charsets[csNumber];
        switch (typeId) {
        case NDB_TYPE_INT:
          {
            require(cs == 0);
            Int32 x1 = *(const Int32*)xptr1;
            Int32 x2 = *(const Int32*)xptr2;
            if (x1 < x2)
              res = -1;
            else if (x1 > x2)
              res = +1;
            ll3("cmp res:" << res <<" x1:" << x1 << " x2:" << x2);
          }
          break;
        case NDB_TYPE_UNSIGNED:
          {
            require(cs == 0);
            Uint32 x1 = *(const Uint32*)xptr1;
            Uint32 x2 = *(const Uint32*)xptr2;
            if (x1 < x2)
              res = -1;
            else if (x1 > x2)
              res = +1;
            ll3("cmp res:" << res <<" x1:" << x1 << " x2:" << x2);
          }
          break;
        case NDB_TYPE_CHAR:
          {
            require(cs != 0 && cs->coll != 0);
            const uint n1 = xlen1;
            const uint n2 = xlen2;
            const uchar* t1 = &xptr1[0];
            const uchar* t2 = &xptr2[0];
            const char* s1 = (const char*)t1;
            const char* s2 = (const char*)t2;
            chk1(n1 == strlen(s1));
            chk1(n2 == strlen(s2));
            res = (*cs->coll->strnncollsp)(cs, t1, n1, t2, n2, false);
            ll3("cmp res:" << res <<" s1:" << s1 << " s2:" << s2);
          }
          break;
        case NDB_TYPE_VARCHAR:
          {
            require(cs != 0 && cs->coll != 0);
            const uint n1 = xptr1[0];
            const uint n2 = xptr2[0];
            const uchar* t1 = &xptr1[1];
            const uchar* t2 = &xptr2[1];
            const char* s1 = (const char*)t1;
            const char* s2 = (const char*)t2;
            chk1(n1 == strlen(s1));
            chk1(n2 == strlen(s2));
            res = (*cs->coll->strnncollsp)(cs, t1, n1, t2, n2, false);
            ll3("cmp res:" << res <<" s1:" << s1 << " s2:" << s2);
          }
          break;
        case NDB_TYPE_LONGVARCHAR:
          {
            require(cs != 0 && cs->coll != 0);
            const uint n1 = xptr1[0] | (xptr1[1] << 8);
            const uint n2 = xptr2[0] | (xptr2[1] << 8);
            const uchar* t1 = &xptr1[2];
            const uchar* t2 = &xptr2[2];
            const char* s1 = (const char*)t1;
            const char* s2 = (const char*)t2;
            chk1(n1 == strlen(s1));
            chk1(n2 == strlen(s2));
            res = (*cs->coll->strnncollsp)(cs, t1, n1, t2, n2, false);
            ll3("cmp res:" << res <<" s1:" << s1 << " s2:" << s2);
          }
          break;
        default:
          require(false);
          break;
        }
      } else
        res = +1;
    } else if (!tdata2.m_xnull[i])
      res = -1;
    if (res != 0)
      break;
  }
  *num_eq = i;
  ll3("xcmp res:" << res << " num_eq:" << *num_eq);
  return res;
}

struct Tbound {
  Tdata& m_tdata;
  NdbPack::Bound m_bound;
  Tbound(Tdata& tdata) :
    m_tdata(tdata),
    m_bound(tdata.m_data)
  {
    m_tdata.m_cnt = 1 + getrandom(m_tdata.m_cnt);
  }
  void create();
  void add();
  void finalize();
  int xcmp(const Tdata& tdata2, int* num_eq) const;
  int xcmp(const Tbound& tbound2, int* num_eq) const;
};

static NdbOut&
operator<<(NdbOut& out, const Tbound& tbound)
{
  out << tbound.m_bound;
  return out;
}

void
Tbound::create()
{
  m_tdata.create();
}

void
Tbound::add()
{
  m_tdata.add();
}

void
Tbound::finalize()
{
  int side = getrandompct(50) ? -1 : +1;
  chk2(m_bound.finalize(side) == 0, m_bound);
  chk2(m_bound.validate() == 0, m_bound);
  chk1((int)m_tdata.m_data.get_full_len() == m_tdata.m_packLen);
}

int
Tbound::xcmp(const Tdata& tdata2, int* num_eq) const
{
  const Tbound& tbound1 = *this;
  const Tdata& tdata1 = tbound1.m_tdata;
  require(tdata1.m_cnt <= tdata2.m_cnt);
  *num_eq = -1;
  int res = tdata1.xcmp(tdata2, num_eq);
  if (res == 0) {
    chk1(*num_eq == tdata1.m_cnt);
    res = m_bound.get_side();
  }
  return res;
}

int
Tbound::xcmp(const Tbound& tbound2, int* num_eq) const
{
  const Tbound& tbound1 = *this;
  const Tdata& tdata1 = tbound1.m_tdata;
  const Tdata& tdata2 = tbound2.m_tdata;
  *num_eq = -1;
  int res = tdata1.xcmp(tdata2, num_eq);
  chk1(0 <= *num_eq && *num_eq <= xmin(tdata1.m_cnt, tdata2.m_cnt));
  if (res == 0) {
    chk1(*num_eq == xmin(tdata1.m_cnt, tdata2.m_cnt));
    if (tdata1.m_cnt < tdata2.m_cnt)
      res = (+1) * tbound1.m_bound.get_side();
    else if (tdata1.m_cnt > tdata2.m_cnt)
      res = (-1) * tbound2.m_bound.get_side();
    else if (tbound1.m_bound.get_side() < tbound2.m_bound.get_side())
      res = -1;
    else if (tbound1.m_bound.get_side() > tbound2.m_bound.get_side())
      res = +1;
  }
  return res;
}

struct Tdatalist {
  enum { Max = 1000 };
  Tdata* m_tdata[Max];
  int m_cnt;
  Tdatalist(Tspec& tspec) {
    m_cnt = data_cnt == -1 ? Max : data_cnt;
    int i;
    for (i = 0; i < m_cnt; i++) {
      m_tdata[i] = new Tdata(tspec, false, 2);
    }
  }
  ~Tdatalist() {
    int i;
    for (i = 0; i < m_cnt; i++) {
      delete m_tdata[i];
    }
  }
  void create();
  void sort();
};

static NdbOut&
operator<<(NdbOut& out, const Tdatalist& tdatalist)
{
  int i;
  for (i = 0; i < tdatalist.m_cnt; i++) {
    out << "data " << i << ": " << *tdatalist.m_tdata[i];
    if (i + 1 < tdatalist.m_cnt)
      out << endl;
  }
  return out;
}

void
Tdatalist::create()
{
  int i;
  for (i = 0; i < m_cnt; i++) {
    Tdata& tdata = *m_tdata[i];
    tdata.create();
    tdata.add();
    tdata.finalize();
  }
}

static int
data_cmp(const void* a1, const void* a2)
{
  const Tdata& tdata1 = **(const Tdata**)a1;
  const Tdata& tdata2 = **(const Tdata**)a2;
  require(tdata1.m_cnt == tdata2.m_cnt);
  const Uint32 cnt = tdata1.m_cnt;
  Uint32 num_eq = ~(Uint32)0;
  int res = tdata1.m_data.cmp(tdata2.m_data, cnt, num_eq);
  require(num_eq <= (Uint32)tdata1.m_cnt);
  require(num_eq <= (Uint32)tdata2.m_cnt);
  return res;
}

void
Tdatalist::sort()
{
  ll1("data sort: in");
  ll3(endl << *this);
  qsort(m_tdata, m_cnt, sizeof(Tdata*), data_cmp);
  ll1("data sort: out");
  ll3(endl << *this);
  int i;
  for (i = 0; i + 1 < m_cnt; i++) {
    const Tdata& tdata1 = *m_tdata[i];
    const Tdata& tdata2 = *m_tdata[i + 1];
    require(tdata1.m_cnt == tdata2.m_cnt);
    const Uint32 cnt = tdata1.m_cnt;
    Uint32 num_eq1 = ~(Uint32)0;
    int res = tdata1.m_data.cmp(tdata2.m_data, cnt, num_eq1);
    chk1(res <= 0);
    // also via unpacked data
    int num_eq2 = -1;
    int res2 = tdata1.xcmp(tdata2, &num_eq2);
    if (res < 0)
      chk1(res2 < 0);
    else if (res == 0)
      chk1(res2 == 0);
    else
      chk1(res2 > 0);
    chk1(num_eq1 == (Uint32)num_eq2);
  }
}

struct Tboundlist {
  enum { Max = 1000 };
  Tbound* m_tbound[Max];
  int m_cnt;
  Tboundlist(Tspec& tspec) {
    m_cnt = bound_cnt == -1 ? Max : bound_cnt;
    int i;
    for (i = 0; i < m_cnt; i++) {
      Tdata* tdata = new Tdata(tspec, true, 0);
      m_tbound[i] = new Tbound(*tdata);
    }
  }
  ~Tboundlist() {
    int i;
    for (i = 0; i < m_cnt; i++) {
      Tdata* tdata = &m_tbound[i]->m_tdata;
      delete m_tbound[i];
      delete tdata;
    }
  }
  void create();
  void sort();
};

static NdbOut&
operator<<(NdbOut& out, const Tboundlist& tboundlist)
{
  int i;
  for (i = 0; i < tboundlist.m_cnt; i++) {
    out << "bound " << i << ": " << *tboundlist.m_tbound[i];
    if (i + 1 < tboundlist.m_cnt)
      out << endl;
  }
  return out;
}

void
Tboundlist::create()
{
  int i;
  for (i = 0; i < m_cnt; i++) {
    Tbound& tbound = *m_tbound[i];
    tbound.create();
    tbound.add();
    tbound.finalize();
  }
}

static int
bound_cmp(const void* a1, const void* a2)
{
  const Tbound& tbound1 = **(const Tbound**)a1;
  const Tbound& tbound2 = **(const Tbound**)a2;
  const Uint32 cnt = xmin(tbound1.m_tdata.m_cnt, tbound2.m_tdata.m_cnt);
  Uint32 num_eq = ~(Uint32)0;
  int res = tbound1.m_bound.cmp(tbound2.m_bound, cnt, num_eq);
  require(num_eq <= cnt);
  require(num_eq <= cnt);
  return res;
}

void
Tboundlist::sort()
{
  ll1("bound sort: in");
  ll3(endl << *this);
  qsort(m_tbound, m_cnt, sizeof(Tbound*), bound_cmp);
  ll1("bound sort: out");
  ll3(endl << *this);
  int i;
  for (i = 0; i + 1 < m_cnt; i++) {
    const Tbound& tbound1 = *m_tbound[i];
    const Tbound& tbound2 = *m_tbound[i + 1];
    const Uint32 cnt = xmin(tbound1.m_tdata.m_cnt, tbound2.m_tdata.m_cnt);
    Uint32 num_eq1 = ~(Uint32)0;
    int res = tbound1.m_bound.cmp(tbound2.m_bound, cnt, num_eq1);
    chk1(res <= 0);
    // also via unpacked data
    int num_eq2 = -1;
    int res2 = tbound1.xcmp(tbound2, &num_eq2);
    if (res < 0)
      chk1(res2 < 0);
    else if (res == 0)
      chk1(res2 == 0);
    else
      chk1(res2 > 0);
    chk1(num_eq1 == (Uint32)num_eq2);
  }
}

static void
testdesc(const Tdata& tdata)
{
  ll3("testdesc: " << tdata);
  const Tspec& tspec = tdata.m_tspec;
  const NdbPack::Data& data = tdata.m_data;
  const Uint8* buf_old = (const Uint8*)data.get_full_buf();
  const Uint32 varBytes = data.get_var_bytes();
  const Uint32 nullMaskLen = tspec.m_spec.get_nullmask_len(false);
  const Uint32 dataLen = data.get_data_len();
  const Uint32 fullLen = data.get_full_len();
  const Uint32 cnt = data.get_cnt();
  chk1(fullLen == varBytes + dataLen);
  NdbPack::Data data_new(tspec.m_spec, false, varBytes);
  Uint8 buf_new[Tspec::MaxBuf];
  data_new.set_buf(buf_new, sizeof(buf_new));
  memcpy(buf_new, buf_old, fullLen);
  chk2(data_new.desc_all(cnt, NdbPack::Endian::Native) == 0, data_new);
  chk1(memcmp(buf_new, data.get_full_buf(), data.get_full_len()) == 0);
  chk1(data_new.get_data_len() == data.get_data_len());
  chk1(data_new.get_cnt() == data.get_cnt());
  chk1(data_new.get_null_cnt() == data.get_null_cnt());
}

static void
testcopy(const Tdata& tdata)
{
  ll3("testcopy: " << tdata);
  const Tspec& tspec = tdata.m_tspec;
  const NdbPack::Data& data = tdata.m_data;
  uint n = getrandom(tdata.m_cnt + 1);
  do {
    ll3("testcopy: cnt:" << tdata.m_cnt << " n:" << n);
    NdbPack::DataC data_old(tspec.m_spec, false);
    data_old.set_buf(data.get_data_buf(), data.get_data_len(), n);
    chk1(data_old.get_cnt() == n);
    NdbPack::Data data_new(tspec.m_spec, false, 0);
    Uint8 buf_new[Tspec::MaxBuf];
    data_new.set_buf(buf_new, sizeof(buf_new));
    chk2(data_new.copy(data_old) == 0, data_new);
    chk1(data_new.get_cnt() == n);
    Uint32 num_eq1 = ~(Uint32)0;
    chk1(data_new.cmp(data_old, n, num_eq1) == 0);
    chk1(num_eq1 == n);
    Uint32 num_eq2 = ~(Uint32)0;
    chk1(data_old.cmp(data_new, n, num_eq2) == 0);
    chk1(num_eq2 == n);
    n = getrandom(n);
  } while (n != 0);
}

static void
testpoai(const Tdata& tdata)
{
  ll3("testpoai: " << tdata);
  const Tspec& tspec = tdata.m_tspec;
  const NdbPack::Data& data = tdata.m_data;
  NdbPack::Data data_new(tspec.m_spec, false, data.get_var_bytes());
  Uint8 buf_new[Tspec::MaxBuf];
  data_new.set_buf(buf_new, sizeof(buf_new));
  Uint32 poaiLen = ~(Uint32)0;
  chk2(data_new.add_poai(tdata.m_poaiBuf, tdata.m_cnt, &poaiLen) == 0, data);
  chk2(data_new.finalize() == 0, data_new);
  chk2(data_new.validate() == 0, data_new);
  chk1(tspec.m_spec.get_nullmask_len(false) + poaiLen == data.get_data_len());
  chk1(data_new.get_full_len() == data.get_full_len());
  chk1(memcmp(data_new.get_full_buf(), data.get_full_buf(), data.get_full_len()) == 0);
  chk1(data_new.get_null_cnt() == data.get_null_cnt());
}

static void
testconvert(const Tdata& tdata)
{
  ll3("testconvert: " << tdata);
  const Tspec& tspec = tdata.m_tspec;
  const NdbPack::Data& data = tdata.m_data;
  NdbPack::Data data_new(tspec.m_spec, false, 2);
  Uint8 buf_new[Tspec::MaxBuf];
  data_new.set_buf(buf_new, sizeof(buf_new));
  chk2(data_new.copy(data) == 0, data_new);
  require(tdata.m_cnt == (int)data.get_cnt());
  require(data.get_cnt() == data_new.get_cnt());
  const Uint32 cnt = tdata.m_cnt;
  Uint32 num_eq;
  int i;
  for (i = 0; i < 10; i++) {
    int k = getrandom(3); // assumes Endian::Value 0,1,2
    NdbPack::Endian::Value v = (NdbPack::Endian::Value)k;
    chk2(data_new.convert(v) == 0, data_new);
    if (v == NdbPack::Endian::Native ||
        v == NdbPack::Endian::get_endian()) {
      num_eq = ~(Uint32)0;
      chk1(data.cmp(data_new, cnt, num_eq) == 0);
      require(num_eq == cnt);
    }
  }
}

static void
testdata(const Tdatalist& tdatalist)
{
  int i;
  for (i = 0; i < tdatalist.m_cnt; i++) {
    const Tdata& tdata = *tdatalist.m_tdata[i];
    testdesc(tdata);
    testcopy(tdata);
    testpoai(tdata);
    testconvert(tdata);
  }
}

static void
testcmp(const Tbound& tbound, const Tdatalist& tdatalist, int* kb)
{
  ll3("testcmp: " << tbound);
  int oldres = 0;
  int n1 = 0;
  int n2 = 0;
  int i;
  for (i = 0; i < tdatalist.m_cnt; i++) {
    const Tdata& tdata = *tdatalist.m_tdata[i];
    require(tbound.m_tdata.m_cnt == (int)tbound.m_bound.get_data().get_cnt());
    const Uint32 cnt = tbound.m_tdata.m_cnt;
    Uint32 num_eq1 = ~(Uint32)0;
    // reverse result for key vs bound
    int res = (-1) * tbound.m_bound.cmp(tdata.m_data, cnt, num_eq1);
    chk1(res != 0);
    res = (res < 0 ? (n1++, -1) : (n2++, +1));
    if (i > 0) {
      // at some point flips from -1 to +1
      chk1(oldres <= res);
    }
    oldres = res;
    // also via unpacked data
    int num_eq2 = -1;
    int res2 = (-1) * tbound.xcmp(tdata, &num_eq2);
    if (res < 0)
      chk1(res2 < 0);
    else
      chk1(res2 > 0);
    chk1(num_eq1 == (Uint32)num_eq2);
  }
  require(n1 + n2 == tdatalist.m_cnt);
  ll2("keys before:" << n1 << " after:" << n2);
  *kb = n1;
}

static void
testcmp(const Tboundlist& tboundlist, const Tdatalist& tdatalist)
{
  int i;
  int oldkb = 0;
  for (i = 0; i < tboundlist.m_cnt; i++) {
    const Tbound& tbound = *tboundlist.m_tbound[i];
    int kb = 0;
    testcmp(tbound, tdatalist, &kb);
    if (i > 0) {
      chk1(oldkb <= kb);
    }
    oldkb = kb;
  }
}

static void
testrun()
{
  Tspec tspec;
  tspec.create();
  ll1("spec: " << tspec);
  Tdatalist tdatalist(tspec);
  tdatalist.create();
  tdatalist.sort();
  testdata(tdatalist);
  if (bound_cnt != 0) {
    Tboundlist tboundlist(tspec);
    tboundlist.create();
    tboundlist.sort();
    testcmp(tboundlist, tdatalist);
  }
}

extern void NdbOut_Init();

static int
testmain()
{
  my_init();
  NdbOut_Init();
  signal(SIGABRT, SIG_DFL);
  { const char* p = NdbEnv_GetEnv("TEST_NDB_PACK_VERBOSE", (char*)0, 0);
    if (p != 0)
      verbose = atoi(p);
  }
  if (seed == 0)
    ll0("random seed: loop number");
  else {
    if (seed < 0)
      seed = getpid();
    ll0("random seed: " << seed);
    ndb_srand(seed);
  }
  loops = 100;
  int i;
  for (i = 0; loops == 0 || i < loops; i++) {
    ll0("loop:" << i << "/" << loops);
    if (seed == 0)
      ndb_srand(i);
    testrun();
  }
  // do not print "ok" in TAPTEST
  ndbout << "passed" << endl;
  return 0;
}

TAPTEST(NdbPack)
{
  int ret = testmain();
  return (ret == 0);
}

#endif
