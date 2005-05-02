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

#ifndef ODBC_COMMON_DataType_hpp
#define ODBC_COMMON_DataType_hpp

#include <map>
#include <ndb_types.h>
#include <AttrType.hpp>
#include <NdbDictionary.hpp>
#include <common/common.hpp>

/**
 * Sql data exists in several formats:
 *
 * - as NDB data at the bottom
 * - as SQL data during intermediary processing
 * - as external data in user input and output buffers
 * - as lexical constants in SQL statement text
 *
 * Each data format has specific types (e.g. number) and each
 * type has specific attributes (e.g. precision).
 */
enum DataFormat {
    Undef_format = 0,
    Ndb_format = 1,	// not used in NDB version >= v2.10
    Sql_format = 2,
    Ext_format = 3,
    Lex_format = 4
};

#define UndefDataType	990
#define NullDataType	991
#define UnboundDataType	992

class SqlType;
class ExtType;
class LexType;

/**
 * @class SqlType
 * @brief Sql data type
 */
class SqlType {
public:
    enum Type {
	Undef = UndefDataType,
	Char = SQL_CHAR,
	Varchar = SQL_VARCHAR,
	Longvarchar = SQL_LONGVARCHAR,
	Binary = SQL_BINARY,
	Varbinary = SQL_VARBINARY,
	Longvarbinary = SQL_LONGVARBINARY,
	Decimal = SQL_DECIMAL,
	Tinyint = SQL_TINYINT,
	Smallint = SQL_SMALLINT,
	Integer = SQL_INTEGER,
	Bigint = SQL_BIGINT,
	Real = SQL_REAL,
	Double = SQL_DOUBLE,
	Date = SQL_DATE,
	Datetime = SQL_TYPE_TIMESTAMP,
	Blob = SQL_BLOB,
	Clob = SQL_CLOB,
	Null = NullDataType,		// not an ODBC SQL type
	Unbound = UnboundDataType	// special for placeholders
    };
    SqlType();
    SqlType(Type type, bool nullable = true);
    SqlType(Type type, unsigned length, bool nullable = true);
    SqlType(Type type, unsigned precision, unsigned scale, bool nullable = true);
    SqlType(Ctx& ctx, Type type, bool nullable = true);
    SqlType(Ctx& ctx, Type type, unsigned length, bool nullable = true);
    SqlType(Ctx& ctx, Type type, unsigned precision, unsigned scale, bool nullable = true);
    SqlType(Ctx& ctx, const NdbDictionary::Column* ndbColumn);
    Type type() const;
    void setType(Ctx& ctx, Type type, bool nullable = true);
    void setType(Ctx& ctx, Type type, unsigned length, bool nullable = true);
    void setType(Ctx& ctx, Type type, unsigned precision, unsigned scale, bool nullable = true);
    void setType(Ctx& ctx, const NdbDictionary::Column* ndbColumn);
    bool equal(const SqlType& sqlType) const;
    unsigned size() const;
    unsigned displaySize() const;
    const char* typeName() const;
    unsigned length() const;
    bool nullable() const;
    void nullable(bool value);
    bool unSigned() const;
    void unSigned(bool value);
    // forwards compatible
    void getType(Ctx& ctx, NdbDictionary::Column* ndbColumn) const;
    // type conversion
    SQLSMALLINT sqlcdefault(Ctx& ctx) const;
    // print for debugging
    void print(char* buf, unsigned size) const;
private:
    friend class LexType;
    Type m_type;
    unsigned m_precision;
    unsigned m_scale;
    unsigned m_length;
    bool m_nullable;
    bool m_unSigned;	// qualifier instead of separate types
};

inline SqlType::Type
SqlType::type() const
{
    return m_type;
}

inline unsigned
SqlType::length() const
{
    return m_length;
}

inline bool
SqlType::nullable() const
{
    return m_nullable;
}

inline void
SqlType::nullable(bool value)
{
    m_nullable = value;
}

inline bool
SqlType::unSigned() const
{
    return m_unSigned;
}

inline void
SqlType::unSigned(bool value)
{
    ctx_assert(m_type == Smallint || m_type == Integer || m_type == Bigint);
    m_unSigned = value;
}

/**
 * Actual SQL datatypes.
 */
typedef unsigned char SqlChar;		// Char and Varchar via pointer
typedef Int16 SqlSmallint;
typedef Int32 SqlInteger;
typedef Int64 SqlBigint;
typedef Uint16 SqlUsmallint;
typedef Uint32 SqlUinteger;
typedef Uint64 SqlUbigint;
typedef float SqlReal;
typedef double SqlDouble;

// datetime cc yy mm dd HH MM SS 00 ff ff ff ff stored as String(12)
struct SqlDatetime {
    int cc() const { return *(signed char*)&m_data[0]; }
    void cc(int x) { *(signed char*)&m_data[0] = x; }
    unsigned yy() const { return *(unsigned char*)&m_data[1]; }
    void yy(unsigned x) { *(unsigned char*)&m_data[1] = x; }
    unsigned mm() const { return *(unsigned char*)&m_data[2]; }
    void mm(unsigned x) { *(unsigned char*)&m_data[2] = x; }
    unsigned dd() const { return *(unsigned char*)&m_data[3]; }
    void dd(unsigned x) { *(unsigned char*)&m_data[3] = x; }
    unsigned HH() const { return *(unsigned char*)&m_data[4]; }
    void HH(unsigned x) { *(unsigned char*)&m_data[4] = x; }
    unsigned MM() const { return *(unsigned char*)&m_data[5]; }
    void MM(unsigned x) { *(unsigned char*)&m_data[5] = x; }
    unsigned SS() const { return *(unsigned char*)&m_data[6]; }
    void SS(unsigned x) { *(unsigned char*)&m_data[6] = x; }
    unsigned ff() const {
	const unsigned char* p = (unsigned char*)&m_data[8];
	unsigned x = 0;
	x += *p++ << 24;
	x += *p++ << 16;
	x += *p++ << 8;
	x += *p++;
	return x;
    }
    void ff(unsigned x) {
	unsigned char* p = (unsigned char*)&m_data[8];
	*p++ = (x >> 24) & 0xff;
	*p++ = (x >> 16) & 0xff;
	*p++ = (x >> 8) & 0xff;
	*p++ = x & 0xff;
    }
    bool valid() { return true; }	// XXX later
    bool less(const SqlDatetime t) const {
	if (cc() != t.cc())
	    return cc() < t.cc();
	if (yy() != t.yy())
	    return yy() < t.yy();
	if (mm() != t.mm())
	    return mm() < t.mm();
	if (dd() != t.dd())
	    return dd() < t.dd();
	if (HH() != t.HH())
	    return HH() < t.HH();
	if (MM() != t.MM())
	    return MM() < t.MM();
	if (SS() != t.SS())
	    return SS() < t.SS();
	if (ff() != t.ff())
	    return ff() < t.ff();
	return false;
    }
private:
    char m_data[12];	// use array to avoid gaps
};

/**
 * @class ExtType
 * @brief External data type
 */
class ExtType {
public:
    enum Type {
	Undef = UndefDataType,
	Char = SQL_C_CHAR,
	Short = SQL_C_SHORT,
	Sshort = SQL_C_SSHORT,
	Ushort = SQL_C_USHORT,
	Long = SQL_C_LONG,		// for sun.jdbc.odbc
	Slong = SQL_C_SLONG,
	Ulong = SQL_C_ULONG,
	Sbigint = SQL_C_SBIGINT,
	Ubigint = SQL_C_UBIGINT,
	Float = SQL_C_FLOAT,
	Double = SQL_C_DOUBLE,
	Timestamp = SQL_C_TYPE_TIMESTAMP,
	Binary = SQL_C_BINARY,		// XXX BLOB hack
	Unbound = UnboundDataType
    };
    ExtType();
    ExtType(Type type);
    ExtType(Ctx& ctx, Type type);
    Type type() const;
    void setType(Ctx& ctx, Type type);
    unsigned size() const;
private:
    Type m_type;
};

inline ExtType::Type
ExtType::type() const
{
    return m_type;
}

/**
 * @class LexType
 * @class Lexical data type
 */
class LexType {
public:
    enum Type {
	Undef = UndefDataType,
	Char = 1,
	Integer = 2,
	Float = 3,
	Null = 4
    };
    LexType();
    LexType(Type type);
    LexType(Ctx& ctx, Type type);
    Type type() const;
    void setType(Ctx& ctx, Type type);
    void convert(Ctx& ctx, SqlType& out, unsigned length = 0) const;
private:
    Type m_type;
};

inline LexType::Type
LexType::type() const
{
    return m_type;
}

#endif
