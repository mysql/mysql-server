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

#include "DataField.hpp"

#ifndef INT_MAX
#define INT_MAX		(2147483647)
#endif

#ifndef INT_MIN
#define INT_MIN		(-INT_MAX - 1)
#endif

#ifndef UINT_MAX
#define UINT_MAX	4294967295U
#endif

#ifndef FLT_MAX
#define FLT_MAX		(3.402823466E+38F)
#endif
#ifndef FLT_MIN
#define FLT_MIN		(1.175494351E-38F)
#endif

#ifdef NDB_WIN32
#define FMT_I64		"%I64d"
#define FMT_U64		"%I64u"
#else
#define FMT_I64		"%lld"
#define FMT_U64		"%llu"
#endif

#ifdef NDB_WIN32
#define strtoll(str, endptr, base)	strtoint64(str, endptr, base)
#define strtoull(str, endptr, base)	strtouint64(str, endptr, base)

static Int64
strtoint64(const char *str, char **endptr, int base)
{
    Int64 x = 0;
    while (*str == ' ')
	str++;
    const char* p = str;
    while ('0' <= *p && *p <= '9')
	x = 10 * x + *p++ - '0';
    if (p == str) {
	*endptr = 0;
	return 0;
    }
    *endptr = (char*)p;
    return x;
}

static Uint64
strtouint64(const char *str, char **endptr, int base)
{
    Uint64 x = 0;
    while (*str == ' ')
	str++;
    const char* p = str;
    while ('0' <= *p && *p <= '9')
	x = 10 * x + *p++ - '0';
    if (p == str) {
	*endptr = 0;
	return 0;
    }
    *endptr = (char*)p;
    return x;
}
#endif

// LexSpec

void
LexSpec::convert(Ctx& ctx, const BaseString& value, SqlField& out)
{
    const SqlSpec& sqlSpec = out.sqlSpec();
    const SqlType& sqlType = sqlSpec.sqlType();
    out.alloc();
    if (sqlType.type() == SqlType::Char) {
	const SqlChar* s = (const SqlChar*)value.c_str();
	out.sqlChar(s, SQL_NTS);
	return;
    }
    if (sqlType.type() == SqlType::Bigint) {
	char* endptr = 0;
	SqlBigint n = static_cast<SqlBigint>(strtoll(value.c_str(), &endptr, 10));
	if (endptr == 0 || *endptr != 0) {
	    ctx.pushStatus(Error::Gen, "cannot convert '%s' to integer", value.c_str());
	    return;
	}
	out.sqlBigint(n);
	return;
    }
    if (sqlType.type() == SqlType::Double) {
	char* endptr = 0;
	SqlDouble x = static_cast<SqlDouble>(strtod(value.c_str(), &endptr));
	if (endptr == 0 || *endptr != 0) {
	    ctx.pushStatus(Error::Gen, "cannot convert '%s' to number", value.c_str());
	    return;
	}
	out.sqlDouble(x);
	return;
    }
    if (sqlType.type() == SqlType::Null) {
	out.u_null.m_nullFlag = true;
	return;
    }
    ctx_assert(false);
}

// SqlField

void
SqlField::alloc()
{
    ctx_assert(sqlSpec().store() == SqlSpec::Physical);
    const SqlType& sqlType = sqlSpec().sqlType();
    if (sqlType.type() == SqlType::Char || sqlType.type() == SqlType::Varchar) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varchar)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    u_data.m_sqlChar = new SqlChar[n];
	}
    }
    if (sqlType.type() == SqlType::Binary || sqlType.type() == SqlType::Varbinary) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varbinary)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    u_data.m_sqlChar = new SqlChar[n];
	}
    }
}

void
SqlField::alloc(const SqlField& sqlField)
{
    alloc();
    const SqlType& sqlType = sqlSpec().sqlType();
    if (sqlType.type() == SqlType::Char || sqlType.type() == SqlType::Varchar) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varchar)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    memcpy(u_data.m_sqlChar, sqlField.u_data.m_sqlChar, n);
	}
    }
    if (sqlType.type() == SqlType::Binary || sqlType.type() == SqlType::Varbinary) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varbinary)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    memcpy(u_data.m_sqlChar, sqlField.u_data.m_sqlChar, n);
	}
    }
}

const void*
SqlField::addr() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->addr();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    if (sqlType.type() == SqlType::Char || sqlType.type() == SqlType::Varchar) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varchar)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    return static_cast<const void*>(u_data.m_sqlChar);
	}
	return static_cast<const void*>(u_data.m_sqlCharSmall);
    }
    if (sqlType.type() == SqlType::Binary || sqlType.type() == SqlType::Varbinary) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varbinary)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    return static_cast<const void*>(u_data.m_sqlChar);
	}
	return static_cast<const void*>(u_data.m_sqlCharSmall);
    }
    if (sqlType.type() == SqlType::Smallint) {
	return static_cast<const void*>(&u_data.m_sqlSmallint);
    }
    if (sqlType.type() == SqlType::Integer) {
	return static_cast<const void*>(&u_data.m_sqlInteger);
    }
    if (sqlType.type() == SqlType::Bigint) {
	return static_cast<const void*>(&u_data.m_sqlBigint);
    }
    if (sqlType.type() == SqlType::Real) {
	return static_cast<const void*>(&u_data.m_sqlReal);
    }
    if (sqlType.type() == SqlType::Double) {
	return static_cast<const void*>(&u_data.m_sqlDouble);
    }
    if (sqlType.type() == SqlType::Datetime) {
	return static_cast<const void*>(&u_data.m_sqlDatetime);
    }
    ctx_assert(false);	// SqlType::Null has no address
    return 0;
}

void*
SqlField::addr()
{
    const SqlType& sqlType = sqlSpec().sqlType();
    if (sqlType.type() == SqlType::Char || sqlType.type() == SqlType::Varchar) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varchar)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    return static_cast<void*>(u_data.m_sqlChar);
	}
	return static_cast<void*>(u_data.m_sqlCharSmall);
    }
    if (sqlType.type() == SqlType::Binary || sqlType.type() == SqlType::Varbinary) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varbinary)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    return static_cast<void*>(u_data.m_sqlChar);
	}
	return static_cast<void*>(u_data.m_sqlCharSmall);
    }
    if (sqlType.type() == SqlType::Smallint) {
	return static_cast<void*>(&u_data.m_sqlSmallint);
    }
    if (sqlType.type() == SqlType::Integer) {
	return static_cast<void*>(&u_data.m_sqlInteger);
    }
    if (sqlType.type() == SqlType::Bigint) {
	return static_cast<void*>(&u_data.m_sqlBigint);
    }
    if (sqlType.type() == SqlType::Real) {
	return static_cast<void*>(&u_data.m_sqlReal);
    }
    if (sqlType.type() == SqlType::Double) {
	return static_cast<void*>(&u_data.m_sqlDouble);
    }
    if (sqlType.type() == SqlType::Datetime) {
	return static_cast<void*>(&u_data.m_sqlDatetime);
    }
    ctx_assert(false);	// SqlType::Null has no address
    return 0;
}

unsigned
SqlField::allocSize() const
{
    const SqlType& sqlType = sqlSpec().sqlType();
    unsigned n = sqlType.size();
    if (sqlType.type() == SqlType::Varchar || sqlType.type() == SqlType::Varbinary) {
	n += 2;
    }
    return n;
}

void
SqlField::free()
{
    ctx_assert(sqlSpec().store() == SqlSpec::Physical);
    const SqlType& sqlType = sqlSpec().sqlType();
    if (sqlType.type() == SqlType::Char || sqlType.type() == SqlType::Varchar) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varchar)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    delete[] u_data.m_sqlChar;
	    u_data.m_sqlChar = 0;	// safety since dtor used explicitly
	}
    }
    if (sqlType.type() == SqlType::Binary || sqlType.type() == SqlType::Varbinary) {
	unsigned n = sqlType.length();
	if (sqlType.type() == SqlType::Varbinary)
	    n += 2;
	if (n > SqlField_CharSmall) {
	    delete[] u_data.m_sqlChar;
	    u_data.m_sqlChar = 0;	// safety since dtor used explicitly
	}
    }
}

// get

const SqlChar*
SqlField::sqlChar() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlChar();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Char);
    if (sqlType.length() > SqlField_CharSmall)
	return u_data.m_sqlChar;
    return u_data.m_sqlCharSmall;
}

const SqlChar*
SqlField::sqlVarchar(unsigned* length) const
{
#if NDB_VERSION_MAJOR >= 3
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlVarchar(length);
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varchar);
    const SqlChar* sqlChar;
    unsigned n = sqlType.length();
    if (2 + n > SqlField_CharSmall)
	sqlChar = u_data.m_sqlChar;
    else
	sqlChar = u_data.m_sqlCharSmall;
    if (length != 0)
	*length = (sqlChar[0] << 8) | sqlChar[1];	// big-endian
    return sqlChar + 2;
#else
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlVarchar(length);
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varchar);
    const SqlChar* sqlChar;
    unsigned n = sqlType.length();
    if (n + 2 > SqlField_CharSmall)
	sqlChar = u_data.m_sqlChar;
    else
	sqlChar = u_data.m_sqlCharSmall;
    if (length != 0)
	*length = (sqlChar[n + 0] << 8) | sqlChar[n + 1];	// big-endian
    return sqlChar;
#endif
}

const SqlChar*
SqlField::sqlBinary() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlChar();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Binary);
    if (sqlType.length() > SqlField_CharSmall)
	return u_data.m_sqlChar;
    return u_data.m_sqlCharSmall;
}

const SqlChar*
SqlField::sqlVarbinary(unsigned* length) const
{
#if NDB_VERSION_MAJOR >= 3
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlVarchar(length);
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varbinary);
    const SqlChar* sqlChar;
    unsigned n = sqlType.length();
    if (2 + n > SqlField_CharSmall)
	sqlChar = u_data.m_sqlChar;
    else
	sqlChar = u_data.m_sqlCharSmall;
    if (length != 0)
	*length = (sqlChar[0] << 8) | sqlChar[1];	// big-endian
    return sqlChar + 2;
#else
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlVarchar(length);
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varbinary);
    const SqlChar* sqlChar;
    unsigned n = sqlType.length();
    if (n + 2 > SqlField_CharSmall)
	sqlChar = u_data.m_sqlChar;
    else
	sqlChar = u_data.m_sqlCharSmall;
    if (length != 0)
	*length = (sqlChar[n + 0] << 8) | sqlChar[n + 1];	// big-endian
    return sqlChar;
#endif
}

SqlSmallint
SqlField::sqlSmallint() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlSmallint();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Smallint);
    return u_data.m_sqlSmallint;
}

SqlInteger
SqlField::sqlInteger() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlInteger();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Integer);
    return u_data.m_sqlInteger;
}

SqlBigint
SqlField::sqlBigint() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlBigint();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Bigint);
    return u_data.m_sqlBigint;
}

SqlReal
SqlField::sqlReal() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlReal();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Real);
    return u_data.m_sqlReal;
}

SqlDouble
SqlField::sqlDouble() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlDouble();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Double);
    return u_data.m_sqlDouble;
}

SqlDatetime
SqlField::sqlDatetime() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlDatetime();
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Datetime);
    return u_data.m_sqlDatetime;
}

// set

void
SqlField::sqlChar(const SqlChar* value, int length)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Char);
    unsigned n = sqlType.length();
    SqlChar* p = n > SqlField_CharSmall ? u_data.m_sqlChar : u_data.m_sqlCharSmall;
    const SqlChar* q = value;
    unsigned m = length == SQL_NTS ? strlen((const char*)q) : length;
    ctx_assert(m <= n);
    for (unsigned i = 0; i < m; i++)
	*p++ = *q++;
    for (unsigned i = m; i < n; i++)
	*p++ = 0x20;		// space
    u_null.m_nullFlag = false;
}

void
SqlField::sqlVarchar(const SqlChar* value, int length)
{
#if NDB_VERSION_MAJOR >= 3
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varchar);
    unsigned n = sqlType.length();
    SqlChar* p = 2 + n > SqlField_CharSmall ? u_data.m_sqlChar : u_data.m_sqlCharSmall;
    const SqlChar* q = value;
    unsigned m = length == SQL_NTS ? strlen((const char*)q) : length;
    ctx_assert(m <= n);
    *p++ = (m >> 8) & 0xff;	// big-endian
    *p++ = (m & 0xff);
    for (unsigned i = 0; i < m; i++)
	*p++ = *q++;
    for (unsigned i = m; i < n; i++)
	*p++ = 0x0;		// null
    u_null.m_nullFlag = false;
#else
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varchar);
    unsigned n = sqlType.length();
    SqlChar* p = n + 2 > SqlField_CharSmall ? u_data.m_sqlChar : u_data.m_sqlCharSmall;
    const SqlChar* q = value;
    unsigned m = length == SQL_NTS ? strlen((const char*)q) : length;
    ctx_assert(m <= n);
    for (unsigned i = 0; i < m; i++)
	*p++ = *q++;
    for (unsigned i = m; i < n; i++)
	*p++ = 0x0;		// null
    *p++ = (m >> 8) & 0xff;	// big-endian
    *p++ = (m & 0xff);
    u_null.m_nullFlag = false;
#endif
}

void
SqlField::sqlBinary(const SqlChar* value, int length)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Binary);
    unsigned n = sqlType.length();
    SqlChar* p = n > SqlField_CharSmall ? u_data.m_sqlChar : u_data.m_sqlCharSmall;
    const SqlChar* q = value;
    unsigned m = length;
    ctx_assert(m <= n);
    for (unsigned i = 0; i < m; i++)
	*p++ = *q++;
    for (unsigned i = m; i < n; i++)
	*p++ = 0x0;		// null
    u_null.m_nullFlag = false;
}

void
SqlField::sqlVarbinary(const SqlChar* value, int length)
{
#if NDB_VERSION_MAJOR >= 3
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varbinary);
    unsigned n = sqlType.length();
    SqlChar* p = 2 + n > SqlField_CharSmall ? u_data.m_sqlChar : u_data.m_sqlCharSmall;
    const SqlChar* q = value;
    unsigned m = length;
    ctx_assert(m <= n);
    *p++ = (m >> 8) & 0xff;	// big-endian
    *p++ = (m & 0xff);
    for (unsigned i = 0; i < m; i++)
	*p++ = *q++;
    for (unsigned i = m; i < n; i++)
	*p++ = 0x0;		// null
    u_null.m_nullFlag = false;
#else
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Varbinary);
    unsigned n = sqlType.length();
    SqlChar* p = n + 2 > SqlField_CharSmall ? u_data.m_sqlChar : u_data.m_sqlCharSmall;
    const SqlChar* q = value;
    unsigned m = length;
    ctx_assert(m <= n);
    for (unsigned i = 0; i < m; i++)
	*p++ = *q++;
    for (unsigned i = m; i < n; i++)
	*p++ = 0x0;		// null
    *p++ = (m >> 8) & 0xff;	// big-endian
    *p++ = (m & 0xff);
    u_null.m_nullFlag = false;
#endif
}

void
SqlField::sqlSmallint(SqlSmallint value)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Smallint);
    u_data.m_sqlSmallint = value;
    u_null.m_nullFlag = false;
}

void
SqlField::sqlInteger(SqlInteger value)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Integer);
    u_data.m_sqlInteger = value;
    u_null.m_nullFlag = false;
}

void
SqlField::sqlBigint(SqlBigint value)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Bigint);
    u_data.m_sqlBigint = value;
    u_null.m_nullFlag = false;
}

void
SqlField::sqlReal(SqlReal value)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Real);
    u_data.m_sqlReal = value;
    u_null.m_nullFlag = false;
}

void
SqlField::sqlDouble(SqlDouble value)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Double);
    u_data.m_sqlDouble = value;
    u_null.m_nullFlag = false;
}

void
SqlField::sqlDatetime(SqlDatetime value)
{
    const SqlType& sqlType = sqlSpec().sqlType();
    ctx_assert(sqlType.type() == SqlType::Datetime);
    u_data.m_sqlDatetime = value;
    u_null.m_nullFlag = false;
}

// get and and set null

bool
SqlField::sqlNull() const
{
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	return u_data.m_sqlField->sqlNull();
    }
    return u_null.m_nullFlag;
}

void
SqlField::sqlNull(bool value)
{
    u_null.m_nullFlag = value;
}

unsigned
SqlField::trim() const
{
    const SqlType& sqlType = sqlSpec().sqlType();
    unsigned n = 0;
    const SqlChar* s = 0;
    if (sqlType.type() == SqlType::Char) {
	n = sqlType.length();
	s = sqlChar();
    } else if (sqlType.type() == SqlType::Varchar) {
	s = sqlVarchar(&n);
    } else {
	ctx_assert(false);
	return 0;
    }
    while (n > 0 && s[n - 1] == 0x20)
	n--;
    return n;
}

void
SqlField::copy(Ctx& ctx, SqlField& out) const
{
    const SqlField& f1 = *this;
    SqlField& f2 = out;
    const SqlType& t1 = f1.sqlSpec().sqlType();
    const SqlType& t2 = f2.sqlSpec().sqlType();
    ctx_assert(t1.type() == t2.type());
    if (f1.sqlNull()) {
	f2.sqlNull(true);
	return;
    }
    if (t1.type() == SqlType::Char) {
	f2.sqlChar(f1.sqlChar(), t1.length());
	return;
    }
    if (t1.type() == SqlType::Varchar) {
	unsigned length;
	const SqlChar* s1 = f1.sqlVarchar(&length);
	f2.sqlVarchar(s1, length);
	return;
    }
    if (t1.type() == SqlType::Binary) {
	f2.sqlBinary(f1.sqlBinary(), t1.length());
	return;
    }
    if (t1.type() == SqlType::Varbinary) {
	unsigned length;
	const SqlChar* s1 = f1.sqlVarbinary(&length);
	f2.sqlVarbinary(s1, length);
	return;
    }
    if (t1.type() == SqlType::Smallint) {
	f2.sqlSmallint(f1.sqlSmallint());
	return;
    }
    if (t1.type() == SqlType::Integer) {
	f2.sqlInteger(f1.sqlInteger());
	return;
    }
    if (t1.type() == SqlType::Bigint) {
	f2.sqlBigint(f1.sqlBigint());
	return;
    }
    if (t1.type() == SqlType::Real) {
	f2.sqlReal(f1.sqlReal());
	return;
    }
    if (t1.type() == SqlType::Double) {
	f2.sqlDouble(f1.sqlDouble());
	return;
    }
    if (t1.type() == SqlType::Datetime) {
	f2.sqlDatetime(f1.sqlDatetime());
	return;
    }
    ctx_assert(false);
}

bool
SqlField::cast(Ctx& ctx, SqlField& out) const
{
    const SqlField& f1 = *this;
    SqlField& f2 = out;
    if (f1.sqlNull()) {
	f2.sqlNull(true);
	return true;
    }
    const SqlType& t1 = f1.sqlSpec().sqlType();
    const SqlType& t2 = f2.sqlSpec().sqlType();
    if (t1.type() == SqlType::Char) {
	if (t2.type() == SqlType::Char) {
	    unsigned n1 = f1.trim();
	    if (n1 > t2.length())
		return false;
	    f2.sqlChar(f1.sqlChar(), n1);
	    return true;
	}
	if (t2.type() == SqlType::Varchar) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlVarchar(f1.sqlChar(), n1);
	    return true;
	}
	if (t2.type() == SqlType::Binary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlBinary(f1.sqlChar(), n1);
	    return true;
	}
	if (t2.type() == SqlType::Varbinary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlVarbinary(f1.sqlChar(), n1);
	    return true;
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Varchar) {
	if (t2.type() == SqlType::Char) {
	    unsigned n1 = f1.trim();
	    if (n1 > t2.length())
		return false;
	    f2.sqlChar(f1.sqlVarchar(0), n1);
	    return true;
	}
	if (t2.type() == SqlType::Varchar) {
	    unsigned n1 = f1.trim();
	    if (n1 > t2.length())
		return false;
	    f2.sqlVarchar(f1.sqlVarchar(0), n1);
	    return true;
	}
	if (t2.type() == SqlType::Binary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlBinary(f1.sqlVarchar(0), n1);
	    return true;
	}
	if (t2.type() == SqlType::Varbinary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlVarbinary(f1.sqlVarchar(0), n1);
	    return true;
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Binary) {
	if (t2.type() == SqlType::Binary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlBinary(f1.sqlBinary(), n1);
	    return true;
	}
	if (t2.type() == SqlType::Varbinary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlVarbinary(f1.sqlBinary(), n1);
	    return true;
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Varbinary) {
	if (t2.type() == SqlType::Binary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlBinary(f1.sqlVarbinary(0), n1);
	    return true;
	}
	if (t2.type() == SqlType::Varbinary) {
	    unsigned n1 = t1.length();
	    if (n1 > t2.length())
		return false;
	    f2.sqlVarbinary(f1.sqlVarbinary(0), n1);
	    return true;
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Smallint) {
	if (! t2.unSigned()) {
	    SqlSmallint x1 = f1.sqlSmallint();
	    if (t2.type() == SqlType::Smallint) {
		f2.sqlSmallint(x1);
		return true;
	    }
	    if (t2.type() == SqlType::Integer) {
		SqlInteger x2 = static_cast<SqlInteger>(x1);
		f2.sqlInteger(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Bigint) {
		SqlBigint x2 = static_cast<SqlBigint>(x1);
		f2.sqlBigint(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Real) {
		SqlReal x2 = static_cast<SqlReal>(x1);
		f2.sqlReal(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Double) {
		SqlDouble x2 = static_cast<SqlDouble>(x1);
		f2.sqlDouble(x2);
		return true;
	    }
	} else {
	    SqlUsmallint x1 = f1.sqlSmallint();
	    if (t2.type() == SqlType::Smallint) {
		f2.sqlSmallint(x1);
		return true;
	    }
	    if (t2.type() == SqlType::Integer) {
		SqlUinteger x2 = static_cast<SqlUinteger>(x1);
		f2.sqlInteger(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Bigint) {
		SqlUbigint x2 = static_cast<SqlUbigint>(x1);
		f2.sqlBigint(x2);
		return true;
	    }
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Integer) {
	if (! t2.unSigned()) {
	    SqlInteger x1 = f1.sqlInteger();
	    if (t2.type() == SqlType::Smallint) {
		SqlSmallint x2 = static_cast<SqlSmallint>(x1);
		if (x1 != static_cast<SqlInteger>(x2))
		    return false;
		f2.sqlSmallint(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Integer) {
		f2.sqlInteger(x1);
		return true;
	    }
	    if (t2.type() == SqlType::Bigint) {
		SqlBigint x2 = static_cast<SqlBigint>(x1);
		f2.sqlBigint(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Real) {
		SqlReal x2 = static_cast<SqlReal>(x1);
		f2.sqlReal(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Double) {
		SqlDouble x2 = static_cast<SqlDouble>(x1);
		f2.sqlDouble(x2);
		return true;
	    }
	} else {
	    SqlUinteger x1 = f1.sqlInteger();
	    if (t2.type() == SqlType::Smallint) {
		SqlUsmallint x2 = static_cast<SqlUsmallint>(x1);
		if (x1 != static_cast<SqlUinteger>(x2))
		    return false;
		f2.sqlSmallint(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Integer) {
		f2.sqlInteger(x1);
		return true;
	    }
	    if (t2.type() == SqlType::Bigint) {
		SqlUbigint x2 = static_cast<SqlUbigint>(x1);
		f2.sqlBigint(x2);
		return true;
	    }
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Bigint) {
	if (! t2.unSigned()) {
	    SqlBigint x1 = f1.sqlBigint();
	    if (t2.type() == SqlType::Smallint) {
		SqlSmallint x2 = static_cast<SqlSmallint>(x1);
		if (x1 != static_cast<SqlBigint>(x2))
		    return false;
		f2.sqlSmallint(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Integer) {
		SqlInteger x2 = static_cast<SqlInteger>(x1);
		if (x1 != static_cast<SqlBigint>(x2))
		    return false;
		f2.sqlInteger(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Bigint) {
		f2.sqlBigint(x1);
		return true;
	    }
	    if (t2.type() == SqlType::Real) {
		SqlReal x2 = static_cast<SqlReal>(x1);
		f2.sqlReal(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Double) {
		SqlDouble x2 = static_cast<SqlDouble>(x1);
		f2.sqlDouble(x2);
		return true;
	    }
	} else {
	    SqlUbigint x1 = f1.sqlBigint();
	    if (t2.type() == SqlType::Smallint) {
		SqlUsmallint x2 = static_cast<SqlUsmallint>(x1);
		if (x1 != static_cast<SqlUbigint>(x2))
		    return false;
		f2.sqlSmallint(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Integer) {
		SqlUinteger x2 = static_cast<SqlUinteger>(x1);
		if (x1 != static_cast<SqlUbigint>(x2))
		    return false;
		f2.sqlInteger(x2);
		return true;
	    }
	    if (t2.type() == SqlType::Bigint) {
		f2.sqlBigint(x1);
		return true;
	    }
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Real) {
	SqlReal x1 = f1.sqlReal();
	int off = 0;
	if (x1 > 0.0 && x1 - floor(x1) >= 0.5)
	    off = 1;
	if (x1 < 0.0 && x1 - floor(x1) <= 0.5)
	    off = -1;
	bool b = (x1 - floor(x1) < 0.5);
	if (t2.type() == SqlType::Smallint) {
	    SqlSmallint x2 = static_cast<SqlSmallint>(x1) + off;
	    if (fabs(x1 - static_cast<SqlReal>(x2)) >= 1.0)
		return false;
	    f2.sqlSmallint(x2);
	    return true;
	}
	if (t2.type() == SqlType::Integer) {
	    SqlInteger x2 = static_cast<SqlInteger>(x1) + off;
	    if (fabs(x1 - static_cast<SqlReal>(x2)) >= 1.0)
		return false;
	    f2.sqlInteger(x2);
	    return true;
	}
	if (t2.type() == SqlType::Bigint) {
	    SqlBigint x2 = static_cast<SqlBigint>(x1) + off;
	    if (fabs(x1 - static_cast<SqlReal>(x2)) >= 1.0)
		return false;
	    f2.sqlBigint(x2);
	    return true;
	}
	if (t2.type() == SqlType::Real) {
	    f2.sqlReal(x1);
	    return true;
	}
	if (t2.type() == SqlType::Double) {
	    SqlDouble x2 = static_cast<SqlDouble>(x1);
	    f2.sqlDouble(x2);
	    return true;
	}
	ctx_assert(false);
	return false;
    }
    if (t1.type() == SqlType::Double) {
	SqlDouble x1 = f1.sqlDouble();
	int off = 0;
	if (x1 > 0.0 && x1 - floor(x1) >= 0.5)
	    off = 1;
	if (x1 < 0.0 && x1 - floor(x1) <= 0.5)
	    off = -1;
	bool b = (x1 - floor(x1) < 0.5);
	if (t2.type() == SqlType::Smallint) {
	    SqlSmallint x2 = static_cast<SqlSmallint>(x1) + off;
	    if (fabs(x1 - static_cast<SqlDouble>(x2)) >= 1.0)
		return false;
	    f2.sqlSmallint(x2);
	    return true;
	}
	if (t2.type() == SqlType::Integer) {
	    SqlInteger x2 = static_cast<SqlInteger>(x1) + off;
	    if (fabs(x1 - static_cast<SqlDouble>(x2)) >= 1.0)
		return false;
	    f2.sqlInteger(x2);
	    return true;
	}
	if (t2.type() == SqlType::Bigint) {
	    SqlBigint x2 = static_cast<SqlBigint>(x1) + off;
	    if (fabs(x1 - static_cast<SqlDouble>(x2)) >= 1.0)
		return false;
	    f2.sqlBigint(x2);
	    return true;
	}
	if (t2.type() == SqlType::Real) {
	    SqlReal x2 = static_cast<SqlReal>(x1);
	    if (fabs(x1 - static_cast<SqlDouble>(x2)) >= 1.0)	// XXX
		return false;
	    f2.sqlReal(x1);
	    return true;
	}
	if (t2.type() == SqlType::Double) {
	    f2.sqlDouble(x1);
	    return true;
	}
	ctx_assert(false);
	return false;
    }
    ctx_assert(false);
    return false;
}

bool
SqlField::less(const SqlField& sqlField) const
{
    const SqlField& f1 = *this;
    const SqlField& f2 = sqlField;
    const SqlType& t1 = f1.sqlSpec().sqlType();
    const SqlType& t2 = f2.sqlSpec().sqlType();
    ctx_assert(t1.type() == t2.type());
    if (t1.type() == SqlType::Char) {
	const SqlChar* s1 = f1.sqlChar();
	const SqlChar* s2 = f2.sqlChar();
	unsigned n1 = t1.length();
	unsigned n2 = t2.length();
	SqlChar c1 = 0;
	SqlChar c2 = 0;
	unsigned i = 0;
	while (i < n1 || i < n2) {
	    c1 = i < n1 ? s1[i] : 0x20;
	    c2 = i < n2 ? s2[i] : 0x20;
	    if (c1 != c2)
		break;
	    i++;
	}
	return (c1 < c2);
    }
    if (t1.type() == SqlType::Varchar) {
	unsigned n1, n2;
	const SqlChar* s1 = f1.sqlVarchar(&n1);
	const SqlChar* s2 = f2.sqlVarchar(&n2);
	SqlChar c1 = 0;
	SqlChar c2 = 0;
	unsigned i = 0;
	while (i < n1 || i < n2) {
	    c1 = i < n1 ? s1[i] : 0x0;
	    c2 = i < n2 ? s2[i] : 0x0;
	    if (c1 != c2)
		break;
	    i++;
	}
	return (c1 < c2);
    }
    if (t1.type() == SqlType::Smallint) {
	ctx_assert(t1.unSigned() == t2.unSigned());
	if (! t1.unSigned()) {
	    SqlSmallint x1 = f1.sqlSmallint();
	    SqlSmallint x2 = f2.sqlSmallint();
	    return (x1 < x2);
	} else {
	    SqlUsmallint x1 = f1.sqlSmallint();
	    SqlUsmallint x2 = f2.sqlSmallint();
	    return (x1 < x2);
	}
    }
    if (t1.type() == SqlType::Integer) {
	ctx_assert(t1.unSigned() == t2.unSigned());
	if (! t1.unSigned()) {
	    SqlInteger x1 = f1.sqlInteger();
	    SqlInteger x2 = f2.sqlInteger();
	    return (x1 < x2);
	} else {
	    SqlUinteger x1 = f1.sqlInteger();
	    SqlUinteger x2 = f2.sqlInteger();
	    return (x1 < x2);
	}
    }
    if (t1.type() == SqlType::Bigint) {
	ctx_assert(t1.unSigned() == t2.unSigned());
	if (! t1.unSigned()) {
	    SqlBigint x1 = f1.sqlBigint();
	    SqlBigint x2 = f2.sqlBigint();
	    return (x1 < x2);
	} else {
	    SqlUbigint x1 = f1.sqlBigint();
	    SqlUbigint x2 = f2.sqlBigint();
	    return (x1 < x2);
	}
    }
    if (t1.type() == SqlType::Real) {
	SqlReal x1 = f1.sqlReal();
	SqlReal x2 = f2.sqlReal();
	return (x1 < x2);
    }
    if (t1.type() == SqlType::Double) {
	SqlDouble x1 = f1.sqlDouble();
	SqlDouble x2 = f2.sqlDouble();
	return (x1 < x2);
    }
    if (t1.type() == SqlType::Datetime) {
	SqlDatetime x1 = f1.sqlDatetime();
	SqlDatetime x2 = f2.sqlDatetime();
	return x1.less(x2);
    }
    ctx_assert(false);
}

// copy from external

static bool
copyin_char_char(Ctx& ctx, char* value, unsigned n, const char* ptr, const SQLINTEGER* ind, int* off, SqlChar* addr, int fieldId)
{
    if (off != 0 && *off >= 0) {
	if ((unsigned)*off > n) {
	    ctx.pushStatus(Sqlstate::_22001, Error::Gen, "input parameter %d truncated (%u > %u)", fieldId, (unsigned)*off, n);
	    return false;
	}
	value += *off;
	n -= *off;
    }
    unsigned m;
    if (ind == 0 || *ind == SQL_NTS)
	m = strlen(ptr);
    else
	m = *ind;
    if (m > n) {
	ctx.pushStatus(Sqlstate::_22001, Error::Gen, "input parameter %d truncated (%u > %u)", fieldId, m, n);
	return false;
    }
    for (unsigned i = 0; i < m; i++)
	value[i] = ptr[i];
    if (off != 0 && *off >= 0)
	*off += m;
    for (unsigned i = m; i < n; i++)
	value[i] = addr == 0 ? 0x20 : 0x0;
    if (addr != 0) {
	if (off != 0 && *off >= 0)
	    m = *off;
	addr[0] = (m >> 8) & 0xff;
	addr[1] = (m & 0xff);
    }
    return true;
}

static bool
copyin_binary_binary(Ctx& ctx, char* value, unsigned n, const char* ptr, const SQLINTEGER* ind, int* off, SqlChar* addr, int fieldId)
{
    if (off != 0 && *off >= 0) {
	if ((unsigned)*off > n) {
	    ctx.pushStatus(Sqlstate::_22001, Error::Gen, "input parameter %d truncated (%u > %u)", fieldId, (unsigned)*off, n);
	    return false;
	}
	value += *off;
	n -= *off;
    }
    if (ind == 0) {
	ctx.pushStatus(Sqlstate::_22001, Error::Gen, "input parameter %d missing length", fieldId);
	return false;
    }
    if (*ind < 0) {
	ctx.pushStatus(Sqlstate::_22001, Error::Gen, "input parameter %d invalid length %d", fieldId, (int)*ind);
	return false;
    }
    unsigned m;
    m = *ind;
    if (m > n) {
	ctx.pushStatus(Sqlstate::_22001, Error::Gen, "input parameter %d truncated (%u > %u)", fieldId, m, n);
	return false;
    }
    for (unsigned i = 0; i < m; i++)
	value[i] = ptr[i];
    if (off != 0 && *off >= 0)
	*off += m;
    for (unsigned i = m; i < n; i++)
	value[i] = addr == 0 ? 0x0 : 0x0;	// just null
    if (addr != 0) {
	if (off != 0 && *off >= 0)
	    m = *off;
	addr[0] = (m >> 8) & 0xff;
	addr[1] = (m & 0xff);
    }
    return true;
}

static bool
copyin_signed_char(Ctx& ctx, SqlBigint* value, const char* ptr, int fieldId)
{
    errno = 0;
    char* endptr = 0;
    SqlBigint x = strtoll(ptr, &endptr, 10);
    if (endptr == 0 || *endptr != 0) {
        errno = 0;
        endptr = 0;
        double y = strtod(ptr, &endptr);
        if (endptr == 0 || *endptr != 0) {
            ctx.pushStatus(Sqlstate::_22005, Error::Gen, "input parameter %d value %s not numeric", fieldId, ptr);
            return false;
        } else if (errno != 0) {
            ctx.pushStatus(Sqlstate::_22003, Error::Gen, "input parameter %d value %s overflow", fieldId, ptr);
            return false;
        }
        // XXX should handle 123.000
        ctx.pushStatus(Sqlstate::_01004, Error::Gen, "input parameter %d value %s truncated", fieldId, ptr);
        x = static_cast<SqlBigint>(y);
    } else if (errno != 0) {
        ctx.pushStatus(Sqlstate::_22003, Error::Gen, "input parameter %d value %s overflow", fieldId, ptr);
        return false;
    }
    *value = x;
    return true;
}

static bool
copyin_double_char(Ctx& ctx, SqlDouble* value, const char* ptr, int fieldId)
{
    errno = 0;
    char* endptr = 0;
    double x = strtod(ptr, &endptr);
    if (endptr == 0 || *endptr != 0) {
        ctx.pushStatus(Sqlstate::_22005, Error::Gen, "input parameter %d value %s not numeric", fieldId, ptr);
        return false;
    } else if (errno != 0) {
        ctx.pushStatus(Sqlstate::_22003, Error::Gen, "input parameter %d value %s overflow", fieldId, ptr);
        return false;
    }
    *value = x;
    return true;
}

void
SqlField::copyin(Ctx& ctx, ExtField& extField)
{
    ctx_assert(extField.extSpec().extType().type() != ExtType::Unbound);
    ctx_assert(sqlSpec().store() == SqlSpec::Physical);
    SQLINTEGER* indPtr = extField.m_indPtr;
    const int fieldId = extField.fieldId();
    if (indPtr != 0 && *indPtr == SQL_NULL_DATA) {
	sqlNull(true);
	return;
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    const ExtType& extType = extField.extSpec().extType();
    if (extField.m_pos > 0) {
	if (sqlType.type() == SqlType::Char && extType.type() == ExtType::Char)
	    ;
	else if (sqlType.type() == SqlType::Varchar && extType.type() == ExtType::Char)
	    ;
	else {
	    char buf[40];
	    sqlType.print(buf, sizeof(buf));
	    ctx.pushStatus(Sqlstate::_HY019, Error::Gen, "cannot send %s data in pieces", buf);
	    return;
	}
    }
    if (sqlType.type() == SqlType::Char || sqlType.type() == SqlType::Varchar) {
	unsigned length = 0;
	char* value = 0;
	SqlChar* laddr = 0;	// Varchar length address
	if (sqlType.type() == SqlType::Char) {
	    length = sqlType.length();
	    if (length > SqlField_CharSmall)
		value = reinterpret_cast<char *>(u_data.m_sqlChar);
	    else
		value = reinterpret_cast<char *>(u_data.m_sqlCharSmall);
	    laddr = 0;
	} else {
#if NDB_VERSION_MAJOR >= 3
	    length = sqlType.length();
	    if (2 + length > SqlField_CharSmall)
		value = reinterpret_cast<char *>(u_data.m_sqlChar + 2);
	    else
		value = reinterpret_cast<char *>(u_data.m_sqlCharSmall + 2);
	    laddr = (SqlChar*)value - 2;
#else
	    length = sqlType.length();
	    if (length + 2 > SqlField_CharSmall)
		value = reinterpret_cast<char *>(u_data.m_sqlChar);
	    else
		value = reinterpret_cast<char *>(u_data.m_sqlCharSmall);
	    laddr = (SqlChar*)value + length;
#endif
	}
	if (extType.type() == ExtType::Char) {
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    int* off = 0;
	    if (extField.m_pos >= 0)
		off = &extField.m_pos;
	    if (! copyin_char_char(ctx, value, length, dataPtr, indPtr, off, laddr, fieldId))
		return;
	    sqlNull(false);
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    const short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, "%hd", *dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    const unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, "%hu", *dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    const long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, "%ld", *dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    const unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, "%lu", *dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    const SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, FMT_I64, *dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    const SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, FMT_U64, *dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    const float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, "%.7f", (double)*dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    const double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, "%.14f", *dataPtr);
	    if (! copyin_char_char(ctx, value, length, buf, indPtr, 0, laddr, fieldId))
		return;
	    sqlNull(false);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Binary || sqlType.type() == SqlType::Varbinary) {
	unsigned length = 0;
	char* value = 0;
	SqlChar* laddr = 0;	// Varbinary length address
	if (sqlType.type() == SqlType::Binary) {
	    length = sqlType.length();
	    if (length > SqlField_CharSmall)
		value = reinterpret_cast<char *>(u_data.m_sqlChar);
	    else
		value = reinterpret_cast<char *>(u_data.m_sqlCharSmall);
	    laddr = 0;
	} else {
#if NDB_VERSION_MAJOR >= 3
	    length = sqlType.length();
	    if (2 + length > SqlField_CharSmall)
		value = reinterpret_cast<char *>(u_data.m_sqlChar + 2);
	    else
		value = reinterpret_cast<char *>(u_data.m_sqlCharSmall + 2);
	    laddr = (SqlChar*)value - 2;
#else
	    length = sqlType.length();
	    if (length + 2 > SqlField_CharSmall)
		value = reinterpret_cast<char *>(u_data.m_sqlChar);
	    else
		value = reinterpret_cast<char *>(u_data.m_sqlCharSmall);
	    laddr = (SqlChar*)value + length;
#endif
	}
	if (extType.type() == ExtType::Binary) {
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    int* off = 0;
	    if (extField.m_pos >= 0)
		off = &extField.m_pos;
	    if (! copyin_binary_binary(ctx, value, length, dataPtr, indPtr, off, laddr, fieldId))
		return;
	    sqlNull(false);
	    return;
	}
    }
    if (sqlType.type() == SqlType::Smallint) {
	SqlSmallint value;
	if (extType.type() == ExtType::Char) {
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    SqlBigint x;
	    if (! copyin_signed_char(ctx, &x, dataPtr, fieldId))
		return;
	    value = x;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    value = (SqlSmallint)*dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    value = (SqlSmallint)*dataPtr;
	    sqlSmallint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Integer) {
	SqlInteger value;
	if (extType.type() == ExtType::Char) {
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    SqlBigint x;
	    if (! copyin_signed_char(ctx, &x, dataPtr, fieldId))
		return;
	    value = x;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    value = (SqlInteger)*dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    value = (SqlInteger)*dataPtr;
	    sqlInteger(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Bigint) {
	SqlBigint value;
	if (extType.type() == ExtType::Char) {
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    SqlBigint x;
	    if (! copyin_signed_char(ctx, &x, dataPtr, fieldId))
		return;
	    value = x;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    value = (SqlBigint)*dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    value = (SqlBigint)*dataPtr;
	    sqlBigint(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Real) {
	SqlReal value;
	if (extType.type() == ExtType::Char) {
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    SqlDouble x;
	    if (! copyin_double_char(ctx, &x, dataPtr, fieldId))
		return;
	    value = x;
	    sqlReal(x);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlReal(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Double) {
	SqlDouble value;
	if (extType.type() == ExtType::Char) {
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    SqlDouble x;
	    if (! copyin_double_char(ctx, &x, dataPtr, fieldId))
		return;
	    value = x;
	    sqlDouble(x);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    value = *dataPtr;
	    sqlDouble(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Datetime) {
	SqlDatetime value;
	if (extType.type() == ExtType::Char) {
	    // XXX replace sscanf by manual scan or regex
	    const char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    int cc = 0;
	    unsigned yy = 0, mm = 0, dd = 0, HH = 0, MM = 0, SS = 0, ff = 0;
	    bool setdate = false;
	    char dummy[10];
	    if (sscanf(dataPtr, "%2d%2u-%2u-%2u %2u:%2u:%2u.%4u%1s", &cc, &yy, &mm, &dd, &HH, &MM, &SS, &ff, dummy) == 8) {
		;
	    } else if (sscanf(dataPtr, "%2d%2u-%2u-%2u %2u:%2u:%2u%1s", &cc, &yy, &mm, &dd, &HH, &MM, &SS, dummy) == 7) {
		;
	    } else if (sscanf(dataPtr, "%2d%2u-%2u-%2u%1s", &cc, &yy, &mm, &dd, dummy) == 4) {
		;
	    } else if (sscanf(dataPtr, "%2u:%2u:%2u.%4u%1s", &HH, &MM, &SS, &ff, dummy) == 4) {
		setdate = true;
	    } else if (sscanf(dataPtr, "%2u:%2u:%2u%1s", &HH, &MM, &SS, dummy) == 3) {
		setdate = true;
	    } else {
		ctx.pushStatus(Sqlstate::_22008, Error::Gen, "invalid timestamp format '%s'", dataPtr);
		return;
	    }
	    if (setdate) {
		time_t clock = time(0);
		struct tm* t = localtime(&clock);
		cc = (1900 + t->tm_year) / 100;
		yy = (1900 + t->tm_year) % 100;
		mm = 1 + t->tm_mon;
		dd = t->tm_mday;
	    }
	    value.cc(cc);
	    value.yy(yy);
	    value.mm(mm);
	    value.dd(dd);
	    value.HH(HH);
	    value.MM(MM);
	    value.SS(SS);
	    value.ff(ff);
	    // XXX write date routines later
	    if (! value.valid()) {
		ctx.pushStatus(Sqlstate::_22008, Error::Gen, "invalid timestamp values '%s'", dataPtr);
		return;
	    }
	    sqlDatetime(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Timestamp) {
	    SQL_TIMESTAMP_STRUCT* dataPtr = static_cast<SQL_TIMESTAMP_STRUCT*>(extField.m_dataPtr);
	    // XXX assume same datatype
	    value.cc(dataPtr->year / 100);
	    value.yy(dataPtr->year / 100);
	    value.mm(dataPtr->month);
	    value.dd(dataPtr->day);
	    value.HH(dataPtr->hour);
	    value.MM(dataPtr->minute);
	    value.SS(dataPtr->second);
	    value.ff(dataPtr->fraction);
	    if (! value.valid()) {
		ctx.pushStatus(Sqlstate::_22008, Error::Gen, "invalid timestamp struct");
		return;
	    }
	    sqlDatetime(value);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    ctx_assert(false);	// SqlType::Null not applicable
}

// copy to external

static bool
copyout_char_char(Ctx& ctx, const char* value, unsigned n, char* ptr, unsigned len, SQLINTEGER* ind, int* off)
{
    unsigned n2 = n;
    if (off != 0 && *off >= 0) {
	ctx_assert((unsigned)*off <= n2);
	value += *off;
	n2 -= *off;
	if (len < n2 + 1) {
	    ctx.pushStatus(Sqlstate::_01004, Error::Gen, "more data at offset %d, current fetch %u, available %u", *off, len, n2);
	    n2 = len - 1;
	}
    } else {
	if (len < n + 1) {		// room for null byte
	    ctx.pushStatus(Sqlstate::_22003, Error::Gen, "char value '%.*s' overflow (%u < %u)", (int)n, value, (unsigned)len, (unsigned)(len + 1));
	    return false;
	}
    }
    memcpy(ptr, value, n2);
    ptr[n2] = 0;
    if (off != 0 && *off >= 0) {
	if (ind != 0)
	    *ind = n - *off;
	*off += n2;
    } else {
	if (ind != 0)
	    *ind = n;
    }
    return true;
}

static bool
copyout_binary_binary(Ctx& ctx, const char* value, unsigned n, char* ptr, unsigned len, SQLINTEGER* ind, int* off)
{
    unsigned n2 = n;
    if (off != 0 && *off >= 0) {
	ctx_assert((unsigned)*off <= n2);
	value += *off;
	n2 -= *off;
	if (len < n2 + 1) {
	    ctx.pushStatus(Sqlstate::_01004, Error::Gen, "more data at offset %d, current fetch %u, available %u", *off, len, n2);
	    n2 = len - 1;
	}
    } else {
	if (len < n) {		// no room for null byte
	    ctx.pushStatus(Sqlstate::_22003, Error::Gen, "binary value '%.*s' overflow (%u < %u)", (int)n, value, (unsigned)len, (unsigned)n);
	    return false;
	}
    }
    memcpy(ptr, value, n2);
    ptr[n2] = 0;
    if (off != 0 && *off >= 0) {
	if (ind != 0)
	    *ind = n - *off;
	*off += n2;
    } else {
	if (ind != 0)
	    *ind = n;
    }
    return true;
}

static bool
copyout_char_signed(Ctx& ctx, const char* value, unsigned n, long* ptr)
{
    while (n > 0 && value[0] == 0x20) {
	value++;
	n--;
    }
    char buf[200];
    if (n >= 200)
	n = 200 - 1;
    memcpy(buf, value, n);
    buf[n] = 0;
    errno = 0;
    char* endptr = 0;
    long x = strtol(buf, &endptr, 10);
    if (endptr == 0 || *endptr != 0) {
	errno = 0;
	endptr = 0;
	double y = strtod(buf, &endptr);
	if (endptr == 0 || *endptr != 0) {
	    ctx.pushStatus(Sqlstate::_22005, Error::Gen, "value %s not numeric", buf);
	    return false;
	} else if (errno != 0) {
	    ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	    return false;
	}
	// XXX should handle 123.000
	ctx.pushStatus(Sqlstate::_01004, Error::Gen, "value %s truncated", buf);
	x = static_cast<long>(y);
    } else if (errno != 0) {
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	return false;
    }
    *ptr = x;
    return true;
}

static bool
copyout_char_bigsigned(Ctx& ctx, const char* value, unsigned n, SQLBIGINT* ptr)
{
    while (n > 0 && value[0] == 0x20) {
	value++;
	n--;
    }
    char buf[200];
    if (n >= 200)
	n = 200 - 1;
    memcpy(buf, value, n);
    buf[n] = 0;
    errno = 0;
    char* endptr = 0;
    SQLBIGINT x = strtoll(buf, &endptr, 10);
    if (endptr == 0 || *endptr != 0) {
	errno = 0;
	endptr = 0;
	double y = strtod(buf, &endptr);
	if (endptr == 0 || *endptr != 0) {
	    ctx.pushStatus(Sqlstate::_22005, Error::Gen, "value %s not numeric", buf);
	    return false;
	} else if (errno != 0) {
	    ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	    return false;
	}
	// XXX should handle 123.000
	ctx.pushStatus(Sqlstate::_01004, Error::Gen, "value %s truncated", buf);
	x = static_cast<long>(y);
    } else if (errno != 0) {
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	return false;
    }
    *ptr = x;
    return true;
}

static bool
copyout_char_unsigned(Ctx& ctx, const char* value, unsigned n, unsigned long* ptr)
{
    while (n > 0 && value[0] == 0x20) {
	value++;
	n--;
    }
    char buf[200];
    if (n >= 200)
	n = 200 - 1;
    memcpy(buf, value, n);
    buf[n] = 0;
    errno = 0;
    char* endptr = 0;
    unsigned long x = strtoul(buf, &endptr, 10);
    if (endptr == 0 || *endptr != 0) {
	errno = 0;
	endptr = 0;
	double y = strtod(buf, &endptr);
	if (endptr == 0 || *endptr != 0) {
	    ctx.pushStatus(Sqlstate::_22005, Error::Gen, "value %s not numeric", buf);
	    return false;
	} else if (errno != 0) {
	    ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	    return false;
	}
	// XXX should handle 123.000
	ctx.pushStatus(Sqlstate::_01004, Error::Gen, "value %s truncated", buf);
	x = static_cast<unsigned long>(y);
    } else if (errno != 0) {
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	return false;
    }
    *ptr = x;
    return true;
}

static bool
copyout_char_bigunsigned(Ctx& ctx, const char* value, unsigned n, SQLUBIGINT* ptr)
{
    while (n > 0 && value[0] == 0x20) {
	value++;
	n--;
    }
    char buf[200];
    if (n >= 200)
	n = 200 - 1;
    memcpy(buf, value, n);
    buf[n] = 0;
    errno = 0;
    char* endptr = 0;
    SQLUBIGINT x = strtoull(buf, &endptr, 10);
    if (endptr == 0 || *endptr != 0) {
	errno = 0;
	endptr = 0;
	double y = strtod(buf, &endptr);
	if (endptr == 0 || *endptr != 0) {
	    ctx.pushStatus(Sqlstate::_22005, Error::Gen, "value %s not numeric", buf);
	    return false;
	} else if (errno != 0) {
	    ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	    return false;
	}
	// XXX should handle 123.000
	ctx.pushStatus(Sqlstate::_01004, Error::Gen, "value %s truncated", buf);
	x = static_cast<unsigned long>(y);
    } else if (errno != 0) {
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	return false;
    }
    *ptr = x;
    return true;
}

static bool
copyout_char_double(Ctx& ctx, const char* value, unsigned n, double* ptr)
{
    while (n > 0 && value[0] == 0x20) {
	value++;
	n--;
    }
    char buf[200];
    if (n >= 200)
	n = 200 - 1;
    memcpy(buf, value, n);
    buf[n] = 0;
    errno = 0;
    char* endptr = 0;
    double x = strtod(value, &endptr);
    if (endptr == 0 || *endptr != 0) {
	ctx.pushStatus(Sqlstate::_22005, Error::Gen, "value %s not numeric", value);
	return false;
    } else if (errno != 0) {
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", value);
	return false;
    }
    *ptr = x;
    return true;
}

static bool
copyout_signed_char(Ctx& ctx, Int64 value, char* ptr, int len, SQLINTEGER* ind)
{
    char buf[100];
    sprintf(buf, FMT_I64, value);
    unsigned n = strlen(buf);
    if (len <= 0) {
	ctx.pushStatus(Sqlstate::_HY090, Error::Gen, "invalid output buffer length %d", len);
	return false;
    }
    if ((unsigned)len < n + 1) {		// room for null byte
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	return false;
    }
    strcpy(ptr, buf);
    if (ind != 0)
	*ind = n;
    return true;
}

static bool
copyout_unsigned_char(Ctx& ctx, Uint64 uvalue, char* ptr, int len, SQLINTEGER* ind)
{
    char buf[100];
    sprintf(buf, FMT_U64, uvalue);
    unsigned n = strlen(buf);
    if (len <= 0) {
	ctx.pushStatus(Sqlstate::_HY090, Error::Gen, "invalid output buffer length %d", len);
	return false;
    }
    if ((unsigned)len < n + 1) {		// room for null byte
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	return false;
    }
    strcpy(ptr, buf);
    if (ind != 0)
	*ind = n;
    return true;
}

static bool
copyout_double_char(Ctx& ctx, double value, unsigned prec, char* ptr, int len, SQLINTEGER* ind)
{
    char buf[100];
    sprintf(buf, "%.*f", (int)prec, value);
    char* p = buf + strlen(buf);
    while (p > buf + prec)
	*--p = 0;
    while (p > buf && *(p - 1) == '0')
	*--p = 0;
    if (p > buf && *(p - 1) == '.') {
	*p++ = '0';
	*p = 0;
    }
    unsigned n = strlen(buf);
    if (len <= 0) {
	ctx.pushStatus(Sqlstate::_HY090, Error::Gen, "invalid output buffer length %d", len);
	return false;
    }
    if ((unsigned)len  < n + 1) {		// room for null byte
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", buf);
	return false;
    }
    strcpy(ptr, buf);
    if (ind != 0)
	*ind = n;
    return true;
}

void
SqlField::copyout(Ctx& ctx, ExtField& extField) const
{
    if (extField.extSpec().extType().type() == ExtType::Unbound) {
	return;		// output buffer may be unbound
    }
    if (sqlSpec().store() == SqlSpec::Reference) {
	ctx_assert(u_data.m_sqlField != 0);
	u_data.m_sqlField->copyout(ctx, extField);
	return;
    }
    SQLINTEGER* indPtr = extField.m_indPtr;
    if (u_null.m_nullFlag) {
	if (extField.m_pos > 0) {	// second time from SQLGetData
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	if (indPtr == 0) {
	    ctx.pushStatus(Sqlstate::_22002, Error::Gen, "indicator variable required");
	    return;
	}
	*indPtr = SQL_NULL_DATA;
	if (extField.m_pos >= 0)
	    extField.m_pos = 1;
	return;
    }
    const SqlType& sqlType = sqlSpec().sqlType();
    const ExtType& extType = extField.extSpec().extType();
    if (sqlType.type() == SqlType::Char || sqlType.type() == SqlType::Varchar) {
	unsigned n = 0;
	const char* value = 0;
	if (sqlType.type() == SqlType::Char) {
	    n = sqlType.length();
	    value = reinterpret_cast<const char*>(sqlChar());
	} else {
	    value = reinterpret_cast<const char*>(sqlVarchar(&n));
	}
	if (extType.type() == ExtType::Char) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    if (extField.m_dataLen <= 0) {
		ctx.pushStatus(Sqlstate::_HY090, Error::Gen, "invalid output buffer length %d", (int)extField.m_dataLen);
		return;
	    }
	    int* off = 0;
	    if (extField.m_pos >= 0) {
		off = &extField.m_pos;
		if ((unsigned)*off >= n) {
		    ctx.setCode(SQL_NO_DATA);
		    return;
		}
	    }
	    if (! copyout_char_char(ctx, value, n, dataPtr, extField.m_dataLen, indPtr, off))
		return;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    long x;
	    if (! copyout_char_signed(ctx, value, n, &x))
		return;
	    if (x < SHRT_MIN || x > SHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", value);
		return;
	    }
	    *dataPtr = static_cast<short>(x);
	    if (indPtr != 0)
		*indPtr = sizeof(short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    unsigned long x;
	    if (! copyout_char_unsigned(ctx, value, n, &x))
		return;
	    if (x > USHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", value);
		return;
	    }
	    *dataPtr = static_cast<unsigned short>(x);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    if (! copyout_char_signed(ctx, value, n, dataPtr))
		return;
	    if (indPtr != 0)
		*indPtr = sizeof(long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    if (! copyout_char_unsigned(ctx, value, n, dataPtr))
		return;
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    if (! copyout_char_bigsigned(ctx, value, n, dataPtr))
		return;
	    if (indPtr != 0)
		*indPtr = sizeof(SQLBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    if (! copyout_char_bigunsigned(ctx, value, n, dataPtr))
		return;
	    if (indPtr != 0)
		*indPtr = sizeof(SQLUBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    double x;
	    if (! copyout_char_double(ctx, value, n, &x))
		return;
	    if (fabs(x) < FLT_MIN || fabs(x) > FLT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %s overflow", value);
		return;
	    }
	    *dataPtr = static_cast<float>(x);
	    if (indPtr != 0)
		*indPtr = sizeof(float);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    if (extField.m_pos > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    double x;
	    if (! copyout_char_double(ctx, value, n, &x))
		return;
	    *dataPtr = static_cast<double>(x);
	    if (indPtr != 0)
		*indPtr = sizeof(double);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Binary || sqlType.type() == SqlType::Varbinary) {
	unsigned n = 0;
	const char* value = 0;
	if (sqlType.type() == SqlType::Binary) {
	    n = sqlType.length();
	    value = reinterpret_cast<const char*>(sqlBinary());
	} else {
	    value = reinterpret_cast<const char*>(sqlVarbinary(&n));
	}
	if (extType.type() == ExtType::Binary) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    if (extField.m_dataLen <= 0) {
		ctx.pushStatus(Sqlstate::_HY090, Error::Gen, "invalid output buffer length %d", (int)extField.m_dataLen);
		return;
	    }
	    int* off = 0;
	    if (extField.m_pos >= 0) {
		off = &extField.m_pos;
		if ((unsigned)*off >= n) {
		    ctx.setCode(SQL_NO_DATA);
		    return;
		}
	    }
	    if (! copyout_binary_binary(ctx, value, n, dataPtr, extField.m_dataLen, indPtr, off))
		return;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Smallint) {
	if (extField.m_pos > 0) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	const SqlSmallint value = sqlSmallint();
	const SqlUsmallint uvalue = value;
	if (extType.type() == ExtType::Char) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    if (! sqlType.unSigned()) {
		if (! copyout_signed_char(ctx, value, dataPtr, extField.m_dataLen, indPtr))
		    return;
	    } else {
		if (! copyout_unsigned_char(ctx, uvalue, dataPtr, extField.m_dataLen, indPtr))
		    return;
	    }
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    *dataPtr = static_cast<short>(value);	// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    *dataPtr = static_cast<unsigned short>(uvalue);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<long>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<unsigned long>(uvalue);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLBIGINT>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(SQLBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLUBIGINT>(uvalue);
	    if (indPtr != 0)
		*indPtr = sizeof(SQLUBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    *dataPtr = static_cast<float>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(float);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    *dataPtr = static_cast<double>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(double);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Integer) {
	if (extField.m_pos > 0) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	const SqlInteger value = sqlInteger();
	const SqlUinteger uvalue = value;
	if (extType.type() == ExtType::Char) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    if (! sqlType.unSigned()) {
		if (! copyout_signed_char(ctx, value, dataPtr, extField.m_dataLen, indPtr))
		    return;
	    } else {
		if (! copyout_unsigned_char(ctx, uvalue, dataPtr, extField.m_dataLen, indPtr))
		    return;
	    }
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    if (value < SHRT_MIN || value > SHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %d overflow", (int)value);
		return;
	    }
	    *dataPtr = static_cast<short>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    if (uvalue > USHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %u overflow", uvalue);
		return;
	    }
	    *dataPtr = static_cast<unsigned short>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<long>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<unsigned long>(uvalue);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLBIGINT>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(SQLBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLUBIGINT>(uvalue);
	    if (indPtr != 0)
		*indPtr = sizeof(SQLUBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    *dataPtr = static_cast<float>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(float);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    *dataPtr = static_cast<double>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(double);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Bigint) {
	if (extField.m_pos > 0) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	const SqlBigint value = sqlBigint();
	const SqlUbigint uvalue = value;
	if (extType.type() == ExtType::Char) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    if (! sqlType.unSigned()) {
		if (! copyout_signed_char(ctx, value, dataPtr, extField.m_dataLen, indPtr))
		    return;
	    } else {
		if (! copyout_unsigned_char(ctx, uvalue, dataPtr, extField.m_dataLen, indPtr))
		    return;
	    }
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    if (value < SHRT_MIN || value > SHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value " FMT_I64 " overflow", (Int64)value);
		return;
	    }
	    *dataPtr = static_cast<short>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    if (uvalue > USHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value " FMT_U64 " overflow", (Uint64)uvalue);
		return;
	    }
	    *dataPtr = static_cast<short>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    if (value < INT_MIN || value > INT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value " FMT_I64 " overflow", (Int64)value);
		return;
	    }
	    *dataPtr = static_cast<long>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    if (uvalue > UINT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value " FMT_U64 " overflow", (Uint64)uvalue);
		return;
	    }
	    *dataPtr = static_cast<unsigned long>(uvalue);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLBIGINT>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(SQLBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLUBIGINT>(uvalue);
	    if (indPtr != 0)
		*indPtr = sizeof(SQLUBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    *dataPtr = static_cast<float>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(float);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    *dataPtr = static_cast<double>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(double);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Real) {
	if (extField.m_pos > 0) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	const SqlReal value = sqlReal();
	if (extType.type() == ExtType::Char) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    if (! copyout_double_char(ctx, value, 7, dataPtr, extField.m_dataLen, indPtr))
		return;
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    *dataPtr = static_cast<short>(value);		// XXX todo
	    if (indPtr != 0)
		*indPtr = sizeof(short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    if (value < 0 || value > USHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %g overflow", (double)value);
		return;
	    }
	    *dataPtr = static_cast<short>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<long>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<unsigned long>(value);	// XXX todo
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLBIGINT>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(SQLBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLUBIGINT>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(SQLUBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    *dataPtr = static_cast<float>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(float);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    *dataPtr = static_cast<double>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(double);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Double) {
	if (extField.m_pos > 0) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	SqlDouble value = sqlDouble();
	if (extType.type() == ExtType::Char) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    if (! copyout_double_char(ctx, value, 14, dataPtr, extField.m_dataLen, indPtr))
		return;
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Short || extType.type() == ExtType::Sshort) {
	    short* dataPtr = static_cast<short*>(extField.m_dataPtr);
	    *dataPtr = static_cast<short>(value);		// XXX todo
	    if (indPtr != 0)
		*indPtr = sizeof(short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ushort) {
	    unsigned short* dataPtr = static_cast<unsigned short*>(extField.m_dataPtr);
	    if (value < 0 || value > USHRT_MAX) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "value %g overflow", (double)value);
		return;
	    }
	    *dataPtr = static_cast<short>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned short);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Long || extType.type() == ExtType::Slong) {
	    long* dataPtr = static_cast<long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<long>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ulong) {
	    unsigned long* dataPtr = static_cast<unsigned long*>(extField.m_dataPtr);
	    *dataPtr = static_cast<unsigned long>(value);	// XXX todo
	    if (indPtr != 0)
		*indPtr = sizeof(unsigned long);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Sbigint) {
	    SQLBIGINT* dataPtr = static_cast<SQLBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLBIGINT>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(SQLBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Ubigint) {
	    SQLUBIGINT* dataPtr = static_cast<SQLUBIGINT*>(extField.m_dataPtr);
	    *dataPtr = static_cast<SQLUBIGINT>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(SQLUBIGINT);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Float) {
	    float* dataPtr = static_cast<float*>(extField.m_dataPtr);
	    *dataPtr = static_cast<float>(value);		// big enough
	    if (indPtr != 0)
		*indPtr = sizeof(float);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Double) {
	    double* dataPtr = static_cast<double*>(extField.m_dataPtr);
	    *dataPtr = static_cast<double>(value);
	    if (indPtr != 0)
		*indPtr = sizeof(double);
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
    }
    if (sqlType.type() == SqlType::Datetime) {
	if (extField.m_pos > 0) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	SqlDatetime value = sqlDatetime();
	if (extType.type() == ExtType::Char) {
	    char* dataPtr = static_cast<char*>(extField.m_dataPtr);
	    char buf[100];
	    sprintf(buf, "%02d%02u-%02u-%02u\040%02u:%02u:%02u.%09u", value.cc(), value.yy(), value.mm(), value.dd(), value.HH(), value.MM(), value.SS(), value.ff());
	    int n = strlen(buf);
	    if (extField.m_dataLen < 20) {
		ctx.pushStatus(Sqlstate::_22003, Error::Gen, "buffer too small for timestamp %s", buf);
		return;
	    }
	    if (extField.m_dataLen < n) {
		ctx.pushStatus(Sqlstate::_01004, Error::Gen, "truncating fractional part of timestamp %s", buf);
		n = extField.m_dataLen;
	    }
	    if (! copyout_char_char(ctx, buf, n, dataPtr, extField.m_dataLen, indPtr, 0))
		return;
	    if (extField.m_pos >= 0)
		extField.m_pos = 1;
	    return;
	}
	if (extType.type() == ExtType::Timestamp) {
	    SQL_TIMESTAMP_STRUCT* dataPtr = static_cast<SQL_TIMESTAMP_STRUCT*>(extField.m_dataPtr);
	    // XXX assume same datatype
	    dataPtr->year = value.cc() * 100 + value.yy();
	    dataPtr->month = value.mm();
	    dataPtr->day = value.dd();
	    dataPtr->hour = value.HH();
	    dataPtr->minute = value.MM();
	    dataPtr->second = value.SS();
	    dataPtr->fraction = value.ff();
	    return;
	}
    }
    ctx_assert(false);	// SqlType::Null not applicable
}

void
SqlField::print(char* buf, unsigned size) const
{
    Ctx ctx;
    unsigned n = sqlSpec().sqlType().displaySize();
    SQLINTEGER ind = 0;
    ExtType extType(ExtType::Char);
    ExtSpec extSpec(extType);
    ExtField extField(extSpec, (SQLPOINTER)buf, size, &ind);
    buf[0] = 0;
    copyout(ctx, extField);
    if (ind == SQL_NULL_DATA)
	snprintf(buf, size, "NULL");
}
