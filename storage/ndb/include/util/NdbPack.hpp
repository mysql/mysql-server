/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_PACK_HPP
#define NDB_PACK_HPP

#include <ndb_global.h>
#include "portlib/ndb_compiler.h"
#include "my_sys.h"
#include <kernel/AttributeHeader.hpp>
#include <NdbSqlUtil.hpp>

class NdbOut;

/*
 * Pack an array of NDB data values.  The types are specified by an
 * array of data types.  There is no associated table or attribute ids.
 * All or an initial sequence of the specified values are present.
 *
 * Currently used for ordered index keys and bounds in kernel (DBTUX)
 * and in index statistics (mysqld).  The comparison methods use the
 * primitive type comparisons from NdbSqlUtil.
 *
 * Keys and bounds use same spec.  However a value in an index bound can
 * be NULL even if the key attribute is not nullable.  Therefore bounds
 * set the "allNullable" property and have a longer null mask.
 *
 * There are two distinct use occasions: 1) construction of data or
 * bound 2) operating on previously constructed data or bound.  There
 * are classes Data/DataC and Bound/BoundC for these uses.  The latter
 * often can return a result without interpreting the full value.
 *
 * Methods return -1 on error and 0 on success.  Comparison methods
 * assume well-formed data and return negative, zero, positive for less,
 * equal, greater.
 */

class NdbPack {
public:
  class Endian;
  class Type;
  class Spec;
  class Iter;
  class DataC;
  class Data;
  class BoundC;
  class Bound;
  class DataArray;
  class BoundArray;

  /*
   * Get SQL type.
   */
  static const NdbSqlUtil::Type& getSqlType(Uint32 typeId);

  /*
   * Error codes for core dumps.
   */
  class Error {
  public:
    enum {
      TypeNotSet = -101,          // type id was not set
      TypeOutOfRange = -102,      // type id is out of range
      TypeNotSupported = -103,    // blob (and for now bit) types
      TypeSizeZero = -104,        // max size was set to zero
      TypeFixSizeInvalid = -105,  // fixed size specified wrong
      TypeNullableNotBool = -106, // nullable must be 0 or 1
      CharsetNotSpecified = -107, // char type with no charset number
      CharsetNotFound = -108,     // cannot install in all_charsets[]
      CharsetNotAllowed = -109,   // non-char type with charset
      SpecBufOverflow = -201,     // more spec items than allocated
      DataCntOverflow = -301,     // more data items than in spec
      DataBufOverflow = -302,     // more data bytes than allocated
      DataValueOverflow = -303,   // var length exceeds max size
      DataNotNullable = -304,     // NULL value to not-nullable type
      InvalidAttrInfo = -305,     // invalid plain old attr info
      BoundEmptySide = -401,      // side not 0 for empty bound
      BoundNonemptySide = -402,   // side not -1,+1 for non-empty bound
      InternalError = -901,
      ValidationError = -902,
      NoError = 0
    };
    Error();
    int get_error_code() const;
    int get_error_line() const;

  private:
    friend class Endian;
    friend class Type;
    friend class Spec;
    friend class Iter;
    friend class DataC;
    friend class Data;
    friend class BoundC;
    friend class Bound;
    void set_error(int code, int line) const;
    void set_error(const Error& e2) const;
    mutable int m_error_code;
    mutable int m_error_line;
  };

  /*
   * Endian definitions.
   */
  class Endian {
  public:
    enum Value {
      Native = 0, // replaced by actual value
      Little = 1,
      Big = 2
    };
    static Value get_endian();
    static void convert(void* ptr, Uint32 len);
  };

  /*
   * Data type.
   */
  class Type : public Error {
  public:
    Type();
    Type(int typeId, Uint32 byteSize, bool nullable, Uint32 csNumber);
    /*
     * Define the type.  Size is fixed or max size.  Values of variable
     * length have length bytes.  The definition is verified when the
     * type is added to the specification.  This also installs missing
     * CHARSET_INFO* into all_charsets[].
     */
    void set(Uint32 typeId, Uint32 byteSize, bool nullable, Uint32 csNumber);
    // getters
    Uint32 get_type_id() const;
    Uint32 get_byte_size() const;
    bool get_nullable() const;
    Uint32 get_cs_number() const;
    Uint32 get_array_type() const;
    // print
    friend NdbOut& operator<<(NdbOut&, const Type&);
    void print(NdbOut& out) const;
    const char* print(char* buf, Uint32 bufsz) const;
    int validate() const;

  private:
    friend class Spec;
    friend class Iter;
    friend class DataC;
    friend class Data;
    friend class DataArray;
    // verify and complete when added to specification
    int complete();
    Uint16 m_typeId;
    Uint16 m_byteSize;    // fixed or max size in bytes
    Uint16 m_nullable;
    Uint16 m_csNumber;
    Uint16 m_arrayType;   // 0,1,2 length bytes
    Uint16 m_nullbitPos;  // computed as part of Spec
  };

  /*
   * Data specification i.e. array of types.  Usually constructed on the
   * heap, so keep fairly small.  Used for both keys and bounds.
   */
  class Spec : public Error {
  public:
    Spec();
    ~Spec() {}
    // set initial buffer (calls reset)
    void set_buf(Type* buf, Uint32 bufMaxCnt);
    // use if buffer is relocated
    void set_buf(Type* buf);
    // reset but keep buffer
    void reset();
    // add type to specification once or number of times
    int add(Type type);
    int add(Type type, Uint32 cnt);
    // copy from
    void copy(const Spec& s2);
    // getters (bounds set allNullable)
    const Type& get_type(Uint32 i) const;
    Uint32 get_cnt() const;
    Uint32 get_nullable_cnt(bool allNullable) const;
    Uint32 get_nullmask_len(bool allNullable) const;
    // max data length including null mask
    Uint32 get_max_data_len(bool allNullable) const;
    // minimum var bytes (if used by Data instance)
    Uint32 get_min_var_bytes(bool allNullable) const;
    // print
    friend NdbOut& operator<<(NdbOut&, const Spec&);
    void print(NdbOut& out) const;
    const char* print(char* buf, Uint32 bufsz) const;
    int validate() const;

  private:
    friend class Iter;
    friend class DataC;
    friend class Data;
    friend class BoundC;
    friend class DataArray;
    // undefined
    Spec(const Spec&);
    Spec& operator=(const Spec&);
    Type* m_buf;
    Uint16 m_bufMaxCnt;
    Uint16 m_cnt;
    Uint16 m_nullableCnt;
    Uint16 m_varsizeCnt;
    Uint32 m_maxByteSize; // excludes null mask
  };

  /*
   * Iterator over data items.  DataC uses external Iter instances in
   * comparison methods etc.  Data contains an Iter instance which
   * iterates on items added.
   */
  class Iter : public Error {
  public:
    // the data instance is only used to set metadata
    Iter(const DataC& data);
    ~Iter() {}
    void reset();

  private:
    friend class DataC;
    friend class Data;
    friend class BoundC;
    friend class DataArray;
    // undefined
    Iter(const Iter&);
    Iter& operator=(const Iter&);
    // describe next non-null or null item and advance iterator
    int desc(const Uint8* item);
    int desc_null();
    // compare current items (DataC buffers are passed)
    int cmp(const Iter& r2, const Uint8* buf1, const Uint8* buf2) const;

    const Spec& m_spec;
    const bool m_allNullable;
    // iterator
    Uint32 m_itemPos;     // position of current item in DataC buffer
    Uint32 m_cnt;         // number of items described so far
    Uint32 m_nullCnt;
    // current item
    Uint32 m_lenBytes;    // 0-2
    Uint32 m_bareLen;     // excludes length bytes
    Uint32 m_itemLen;     // full length, value zero means null
  };

  /*
   * Read-only superclass of Data.  Initialized from a previously
   * constructed Data buffer (any var bytes skipped).  Methods interpret
   * one data item at a time.  Values are native endian.
   */
  class DataC : public Error {
  public:
    DataC(const Spec& spec, bool allNullable);
    // set buffer to previously constructed one with given item count
    void set_buf(const void* buf, Uint32 bufMaxLen, Uint32 cnt);
    // interpret next data item
    int desc(Iter& r) const;
    // compare cnt attrs and also return number of initial equal attrs
    int cmp(const DataC& d2, Uint32 cnt, Uint32& num_eq) const;
    // getters
    const Spec& get_spec() const;
    const void* get_data_buf() const;
    Uint32 get_cnt() const;
    bool is_empty() const;
    bool is_full() const;
    // print
    friend NdbOut& operator<<(NdbOut&, const DataC&);
    void print(NdbOut& out) const;
    const char* print(char* buf, Uint32 bufsz, bool convert_flag = false) const;
    int validate() const { return 0; }

  private:
    friend class Iter;
    friend class Data;
    friend class BoundC;
    friend class DataArray;
    // undefined
    DataC(const Data&);
    DataC& operator=(const DataC&);
    const Spec& m_spec;
    const bool m_allNullable;
    const Uint8* m_buf;
    Uint32 m_bufMaxLen;
    // can be updated as part of Data instance
    Uint32 m_cnt;
  };

  /*
   * Instance of an array of data values.  The values are packed into
   * a byte buffer.  The buffer is also maintained as a single varbinary
   * value if non-zero var bytes (length bytes) is specified.
   *
   * Data instances can be received from another source (such as table
   * in database) and may not be native-endian.  Such instances must
   * first be completed with desc_all() and convert().
   */
  class Data : public DataC {
  public:
    Data(const Spec& spec, bool allNullable, Uint32 varBytes);
    // set buffer (calls reset)
    void set_buf(void* buf, Uint32 bufMaxLen);
    // reset but keep buffer (header is zeroed)
    void reset();
    // add non-null data items and return length in bytes
    int add(const void* data, Uint32* len_out);
    int add(const void* data, Uint32 cnt, Uint32* len_out);
    // add null data items and return length 0 bytes
    int add_null(Uint32* len_out);
    int add_null(Uint32 cnt, Uint32* len_out);
    // add from "plain old attr info"
    int add_poai(const Uint32* poai, Uint32* len_out);
    int add_poai(const Uint32* poai, Uint32 cnt, Uint32* len_out);
    // call this before first use
    int finalize();
    // copy from
    int copy(const DataC& d2);
    // convert endian
    int convert(Endian::Value to_endian);
    // create complete instance from buffer contents
    int desc_all(Uint32 cnt, Endian::Value from_endian);
    // getters
    Uint32 get_max_len() const;
    Uint32 get_max_len4() const;
    Uint32 get_var_bytes() const;
    void* get_full_buf();
    const void* get_full_buf() const;
    Uint32 get_full_len() const;
    Uint32 get_data_len() const;
    Uint32 get_null_cnt() const;
    Endian::Value get_endian() const;
    // print
    friend NdbOut& operator<<(NdbOut&, const Data&);
    void print(NdbOut& out) const;
    const char* print(char* buf, Uint32 bufsz) const;
    int validate() const;

  private:
    friend class Iter;
    friend class Bound;
    // undefined
    Data(const Data&);
    Data& operator=(const Data&);
    int finalize_impl();
    int convert_impl();
    const Uint32 m_varBytes;
    Uint8* m_buf;
    Uint32 m_bufMaxLen;
    Endian::Value m_endian;
    // iterator on items added
    Iter m_iter;
  };

  /*
   * Read-only superclass of BoundC, analogous to DataC.  Initialized
   * from a previously constructed Bound or DataC buffer.
   */
  class BoundC : public Error {
  public:
    BoundC(DataC& data);
    ~BoundC() {}
    // call this before first use
    int finalize(int side);
    // compare bound to key (may return 0 if bound is longer)
    int cmp(const DataC& d2, Uint32 cnt, Uint32& num_eq) const;
    // compare bounds (may return 0 if cnt is less than min length)
    int cmp(const BoundC& b2, Uint32 cnt, Uint32& num_eq) const;
    // getters
    DataC& get_data() const;
    int get_side() const;
    // print
    friend NdbOut& operator<<(NdbOut&, const BoundC&);
    void print(NdbOut& out) const;
    const char* print(char* buf, Uint32 bufsz) const;
    int validate() const;

  private:
    friend class Bound;
    friend class DataArray;
    // undefined
    BoundC(const BoundC&);
    BoundC& operator=(const BoundC&);
    DataC& m_data;
    int m_side;
  };

  /*
   * Ordered index range bound consists of a partial key and a "side".
   * The partial key is a Data instance where some initial number of
   * values are present.  It is defined separately by the caller and
   * passed to Bound ctor by reference.
   */
  class Bound : public BoundC {
  public:
    Bound(Data& data);
    ~Bound() {}
    void reset();
    // call this before first use
    int finalize(int side);
    // getters
    Data& get_data() const;
    // print
    friend NdbOut& operator<<(NdbOut&, const Bound&);
    void print(NdbOut& out) const;
    const char* print(char* buf, Uint32 bufsz) const;
    int validate() const;

  private:
    // undefined
    Bound(const Bound&);
    Bound& operator=(const Bound&);
    Data& m_data;
  };

  /**
   * These are classes that are optimised for quick
   * comparisons, in particular when the same
   * object is used over and over again. Typical
   * cases for this is scans where multiple rows
   * can be compared in a single time slot. Even more
   * so index builds that execute a very long time
   * using the same objects.
   *
   * The idea is that we build an array of objects with
   * length and a pointer to the data. This means that
   * we can build this array from Attribute information
   * retrieved from TUP, we can build it from a
   * Bound supplied for a scan and we can build it for
   * searches to update the index.
   */
  class DataEntry
  {
  public:
    DataEntry() {}
    ~DataEntry() {}
  private:
    friend class DataArray;
    const Uint8* m_data_ptr;
    Uint32 m_data_len;
  };

  class DataArray
  {
  public:
    DataArray() {}
    ~DataArray() {}
    void init_poai(const Uint32* buf, const Uint32 cnt);
    void init_bound(const BoundC&, const Uint32 cnt);
    int cmp(const Spec* spec,
            const DataArray* d2,
            const Uint32 cnt) const;
    Uint32 cnt() const;
    Uint32 get_null_cnt() const;
    Uint32 get_data_len() const;
  private:
    friend class BoundArray;
    Uint32 m_cnt;
    Uint32 m_null_cnt;
    DataEntry m_entries[MAX_ATTRIBUTES_IN_INDEX];
  };

  class BoundArray
  {
    public:
      BoundArray();
      BoundArray(const Spec*,
                 const DataArray*,
                 const int side);
      ~BoundArray() {}
      int cmp(const DataArray* d2, const Uint32 cnt, bool ok_to_ret_eq) const;
      Uint32 cnt() const;
    private:
      const Spec* m_spec;
      const DataArray* m_data_array;
      const int m_side;
  };

  /*
   * Helper for print() methods.
   */
  struct Print {
  private:
    friend class Endian;
    friend class Type;
    friend class Spec;
    friend class Iter;
    friend class DataC;
    friend class Data;
    friend class BoundC;
    friend class Bound;
    Print(char* buf, Uint32 bufsz);
    void print(const char* frm, ...)
      ATTRIBUTE_FORMAT(printf, 2, 3);
    char* m_buf;
    Uint32 m_bufsz;
    Uint32 m_sz;
  };
};

// NdbPack

inline const NdbSqlUtil::Type&
NdbPack::getSqlType(Uint32 typeId)
{
  return NdbSqlUtil::m_typeList[typeId];
}

// NdbPack::Error

inline
NdbPack::Error::Error()
{
  m_error_code = 0;
  m_error_line = 0;
}

// NdbPack::Endian

inline NdbPack::Endian::Value
NdbPack::Endian::get_endian()
{
#ifndef WORDS_BIGENDIAN
  return Little;
#else
  return Big;
#endif
}

// NdbPack::Type

inline
NdbPack::Type::Type()
{
  m_typeId = NDB_TYPE_UNDEFINED;
  m_byteSize = 0;
  m_nullable = true;
  m_csNumber = 0;
  m_arrayType = 0;
  m_nullbitPos = 0;
}

inline
NdbPack::Type::Type(int typeId, Uint32 byteSize, bool nullable, Uint32 csNumber)
{
  set(typeId, byteSize, nullable, csNumber);
}

inline void
NdbPack::Type::set(Uint32 typeId, Uint32 byteSize, bool nullable, Uint32 csNumber)
{
  m_typeId = typeId;
  m_byteSize = byteSize;
  m_nullable = nullable;
  m_csNumber = csNumber;
}

inline Uint32
NdbPack::Type::get_type_id() const
{
  return m_typeId;
}

inline Uint32
NdbPack::Type::get_byte_size() const
{
  return m_byteSize;
}

inline bool
NdbPack::Type::get_nullable() const
{
  return (bool)m_nullable;
}

inline Uint32
NdbPack::Type::get_cs_number() const
{
  return m_csNumber;
}

inline Uint32
NdbPack::Type::get_array_type() const
{
  return m_arrayType;
}

// NdbPack::Spec

inline
NdbPack::Spec::Spec()
{
  reset();
  m_buf = nullptr;
  m_bufMaxCnt = 0;
}

inline void
NdbPack::Spec::set_buf(Type* buf, Uint32 bufMaxCnt)
{
  reset();
  m_buf = buf;
  m_bufMaxCnt = bufMaxCnt;
}

inline void
NdbPack::Spec::set_buf(Type* buf)
{
  m_buf = buf;
}

inline void
NdbPack::Spec::reset()
{
  m_cnt = 0;
  m_nullableCnt = 0;
  m_varsizeCnt = 0;
  m_maxByteSize = 0;
}

inline const NdbPack::Type&
NdbPack::Spec::get_type(Uint32 i) const
{
  assert(i < m_cnt);
  return m_buf[i];
}

inline Uint32
NdbPack::Spec::get_cnt() const
{
  return m_cnt;
}

inline Uint32
NdbPack::Spec::get_nullable_cnt(bool allNullable) const
{
  if (!allNullable)
    return m_nullableCnt;
  else
    return m_cnt;
}

inline Uint32
NdbPack::Spec::get_nullmask_len(bool allNullable) const
{
  return (get_nullable_cnt(allNullable) + 7) / 8;
}

inline Uint32
NdbPack::Spec::get_max_data_len(bool allNullable) const
{
  return get_nullmask_len(allNullable) + m_maxByteSize;
}

inline Uint32
NdbPack::Spec::get_min_var_bytes(bool allNullable) const
{
  const Uint32 len = get_max_data_len(allNullable);
  return (len < 256 ? 1 : 2);
}

// NdbPack::Iter

inline
NdbPack::Iter::Iter(const DataC& data) :
  m_spec(data.m_spec),
  m_allNullable(data.m_allNullable)
{
  reset();
}

inline void
NdbPack::Iter::reset()
{
  m_itemPos = m_spec.get_nullmask_len(m_allNullable);
  m_cnt = 0;
  m_nullCnt = 0;
  m_lenBytes = 0;
  m_bareLen = 0;
  m_itemLen = 0;
}

// NdbPack::DataC

inline
NdbPack::DataC::DataC(const Spec& spec, bool allNullable) :
  m_spec(spec),
  m_allNullable(allNullable)
{
  m_buf = nullptr;
  m_bufMaxLen = 0;
  m_cnt = 0;
}

inline void
NdbPack::DataC::set_buf(const void* buf, Uint32 bufMaxLen, Uint32 cnt)
{
  m_buf = static_cast<const Uint8*>(buf);
  m_bufMaxLen = bufMaxLen;
  m_cnt = cnt;
}

inline const NdbPack::Spec&
NdbPack::DataC::get_spec() const
{
  return m_spec;
}

inline const void*
NdbPack::DataC::get_data_buf() const
{
  return &m_buf[0];
}

inline Uint32
NdbPack::DataC::get_cnt() const
{
  return m_cnt;
}

inline bool
NdbPack::DataC::is_empty() const
{
  return m_cnt == 0;
}

inline bool
NdbPack::DataC::is_full() const
{
  return m_cnt == m_spec.m_cnt;
}

// NdbPack::Data

inline
NdbPack::Data::Data(const Spec& spec, bool allNullable, Uint32 varBytes) :
  DataC(spec, allNullable),
  m_varBytes(varBytes),
  m_iter(*this)
{
  m_buf = nullptr;
  m_bufMaxLen = 0;
  m_endian = Endian::get_endian();
}

inline void
NdbPack::Data::set_buf(void* buf, Uint32 bufMaxLen)
{
  m_buf = static_cast<Uint8*>(buf);
  m_bufMaxLen = bufMaxLen;
  reset();
  assert(bufMaxLen >= m_varBytes);
  DataC::set_buf(&m_buf[m_varBytes], m_bufMaxLen - m_varBytes, 0);
}

inline void
NdbPack::Data::reset()
{
  m_cnt = 0;  // in DataC
  const Uint32 bytes = m_varBytes + m_spec.get_nullmask_len(m_allNullable);
  memset(m_buf, 0, bytes);
  m_endian = Endian::get_endian();
  m_iter.reset();
}

inline int
NdbPack::Data::finalize()
{
  if (likely(m_varBytes == 0 ||
             finalize_impl() == 0))
    return 0;
  return -1;
}

inline int
NdbPack::Data::convert(Endian::Value to_endian)
{
  if (to_endian == Endian::Native)
    to_endian = Endian::get_endian();
  if (m_endian == to_endian)
    return 0;
  if (convert_impl() == 0)
  {
    m_endian = to_endian;
    return 0;
  }
  return -1;
}

inline Uint32
NdbPack::Data::get_max_len() const
{
  return m_varBytes + m_spec.get_max_data_len(m_allNullable);
}

inline Uint32
NdbPack::Data::get_max_len4() const
{
  Uint32 len4 = get_max_len();
  len4 += 3;
  len4 /= 4;
  len4 *= 4;
  return len4;
}

inline Uint32
NdbPack::Data::get_var_bytes() const
{
  return m_varBytes;
}

inline const void* NdbPack::Data::get_full_buf() const { return m_buf; }

inline void* NdbPack::Data::get_full_buf() { return m_buf; }

inline Uint32
NdbPack::Data::get_full_len() const
{
  return m_varBytes + m_iter.m_itemPos + m_iter.m_itemLen;
}

inline Uint32
NdbPack::Data::get_data_len() const
{
  return m_iter.m_itemPos + m_iter.m_itemLen;
}

inline Uint32
NdbPack::Data::get_null_cnt() const
{
  return m_iter.m_nullCnt;
}

inline NdbPack::Endian::Value
NdbPack::Data::get_endian() const
{
  return m_endian;
}

// NdbPack::BoundC

inline
NdbPack::BoundC::BoundC(DataC& data) :
  m_data(data)
{
  m_side = 0;
}

inline int
NdbPack::BoundC::cmp(const DataC& d2, Uint32 cnt, Uint32& num_eq) const
{
  const BoundC& b1 = *this;
  const DataC& d1 = b1.m_data;
  int res = d1.cmp(d2, cnt, num_eq);
  if (res == 0 && d1.m_cnt <= d2.m_cnt)
    res = b1.m_side;
  return res;
}

inline NdbPack::DataC&
NdbPack::BoundC::get_data() const
{
  return m_data;
}

inline int
NdbPack::BoundC::get_side() const
{
  return m_side;
}

// NdbPack::Bound

inline
NdbPack::Bound::Bound(Data& data) :
  BoundC(data),
  m_data(data)
{
}

inline void
NdbPack::Bound::reset()
{
  m_data.reset();
  m_side = 0;
}

inline int
NdbPack::Bound::finalize(int side)
{
  if (unlikely(m_data.finalize() == -1))
  {
    set_error(m_data);
    return -1;
  }
  if (unlikely(BoundC::finalize(side) == -1))
    return -1;
  return 0;
}

inline NdbPack::Data&
NdbPack::Bound::get_data() const
{
  return m_data;
}

inline Uint32
NdbPack::DataArray::cnt() const
{
  return m_cnt;
}

inline Uint32
NdbPack::DataArray::get_null_cnt() const
{
  return m_null_cnt;
}

inline Uint32
NdbPack::DataArray::get_data_len() const
{
  Uint32 cnt = m_cnt;
  Uint32 len = 0;
  for (Uint32 i = 0; i < cnt; i++)
  {
    len += m_entries[i].m_data_len;
  }
  return len;
}

inline NdbPack::BoundArray::BoundArray() :
  m_spec(nullptr),
  m_data_array(nullptr),
  m_side(0)
{
}

inline NdbPack::BoundArray::BoundArray(
                   const Spec* spec,
                   const DataArray* data_array,
                   const int side) :
  m_spec(spec),
  m_data_array(data_array),
  m_side(side)
{
}

inline int
NdbPack::BoundArray::cmp(const DataArray* d2,
                         const Uint32 cnt,
                         const bool ok_to_ret_eq) const
{
  int res = m_data_array->cmp(m_spec, d2, cnt);
  if (res == 0 && !ok_to_ret_eq && m_data_array->m_cnt <= d2->cnt())
    res = m_side;
  return res;
}

inline Uint32
NdbPack::BoundArray::cnt() const
{
  return m_data_array->cnt();
}
#endif // NDB_PACK_HPP
