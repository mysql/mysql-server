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

#ifndef ODBC_COMMON_OdbcData_hpp
#define ODBC_COMMON_OdbcData_hpp

#include <ndb_types.h>
#include <common/common.hpp>

/**
 * @class OdbcData
 * @brief Odbc data types and storage
 *
 * Stores diagnostics, attributes, and descriptors.  Also used
 * for converting to and from driver function arguments.
 */
class OdbcData {
public:
    enum Type {
	Undef = 0,
	Smallint,
	Usmallint,
	Integer,
	Uinteger,
	Pointer,
	SmallintPtr,
	UsmallintPtr,
	IntegerPtr,
	UintegerPtr,
	PointerPtr,
	Sqlchar,
	Sqlstate
    };
    OdbcData();
    OdbcData(Type type);
    OdbcData(const OdbcData& odbcData);
    OdbcData(SQLSMALLINT smallint);
    OdbcData(SQLUSMALLINT usmallint);
    OdbcData(SQLINTEGER integer);
    OdbcData(SQLUINTEGER uinteger);
    OdbcData(SQLPOINTER pointer);
    OdbcData(SQLSMALLINT* smallintPtr);
    OdbcData(SQLUSMALLINT* usmallintPtr);
    OdbcData(SQLINTEGER* integerPtr);
    OdbcData(SQLUINTEGER* uintegerPtr);
    OdbcData(SQLPOINTER* pointerPtr);
    OdbcData(const char* sqlchar);
    OdbcData(const ::Sqlstate& sqlstate);
    ~OdbcData();
    Type type() const;
    void setValue();
    void setValue(Type type);
    void setValue(const OdbcData odbcData);
    // get value
    SQLSMALLINT smallint() const;
    SQLUSMALLINT usmallint() const;
    SQLINTEGER integer() const;
    SQLUINTEGER uinteger() const;
    SQLPOINTER pointer() const;
    SQLSMALLINT* smallintPtr() const;
    SQLUSMALLINT* usmallintPtr() const;
    SQLINTEGER* integerPtr() const;
    SQLUINTEGER* uintegerPtr() const;
    SQLPOINTER* pointerPtr() const;
    const char* sqlchar() const;
    const ::Sqlstate& sqlstate() const;
    // copy in from user buffer
    void copyin(Ctx& ctx, Type type, SQLPOINTER buf, SQLINTEGER length);
    // copy out to user buffer
    void copyout(Ctx& ctx, SQLPOINTER buf, SQLINTEGER length, SQLINTEGER* total, SQLSMALLINT* total2 = 0);
    // logging
    void print(char* buf, unsigned size) const;
private:
    OdbcData& operator=(const OdbcData& odbcData);	// disallowed
    Type m_type;
    union {
	SQLSMALLINT m_smallint;
	SQLUSMALLINT m_usmallint;
	SQLINTEGER m_integer;
	SQLUINTEGER m_uinteger;
	SQLPOINTER m_pointer;
	SQLSMALLINT* m_smallintPtr;
	SQLUSMALLINT* m_usmallintPtr;
	SQLINTEGER* m_integerPtr;
	SQLUINTEGER* m_uintegerPtr;
	SQLPOINTER* m_pointerPtr;
	char* m_sqlchar;
	const ::Sqlstate* m_sqlstate;
    };
};

inline OdbcData::Type
OdbcData::type() const
{
    return m_type;
}

inline
OdbcData::OdbcData(SQLSMALLINT smallint) :
    m_type(Smallint),
    m_smallint(smallint)
{
}

inline
OdbcData::OdbcData(SQLUSMALLINT usmallint) :
    m_type(Usmallint),
    m_usmallint(usmallint)
{
}

inline
OdbcData::OdbcData(SQLINTEGER integer) :
    m_type(Integer),
    m_integer(integer)
{
}

inline
OdbcData::OdbcData(SQLUINTEGER uinteger) :
    m_type(Uinteger),
    m_uinteger(uinteger)
{
}

inline
OdbcData::OdbcData(SQLPOINTER pointer) :
    m_type(Pointer),
    m_pointer(pointer)
{
}

inline
OdbcData::OdbcData(SQLSMALLINT* smallintPtr) :
    m_type(SmallintPtr),
    m_smallintPtr(smallintPtr)
{
}

inline
OdbcData::OdbcData(SQLUSMALLINT* usmallintPtr) :
    m_type(UsmallintPtr),
    m_usmallintPtr(usmallintPtr)
{
}

inline
OdbcData::OdbcData(SQLINTEGER* integerPtr) :
    m_type(IntegerPtr),
    m_integerPtr(integerPtr)
{
}

inline
OdbcData::OdbcData(SQLUINTEGER* uintegerPtr) :
    m_type(UintegerPtr),
    m_uintegerPtr(uintegerPtr)
{
}

inline
OdbcData::OdbcData(SQLPOINTER* pointerPtr) :
    m_type(PointerPtr),
    m_pointerPtr(pointerPtr)
{
}

inline
OdbcData::OdbcData(const char* sqlchar) :
    m_type(Sqlchar)
{
    unsigned n = strlen(sqlchar);
    m_sqlchar = new char[n + 1];
    strcpy(m_sqlchar, sqlchar);
}

inline
OdbcData::OdbcData(const ::Sqlstate& sqlstate) :
    m_type(Sqlstate),
    m_sqlstate(&sqlstate)
{
}

// get value

inline SQLSMALLINT
OdbcData::smallint() const
{
    ctx_assert(m_type == Smallint);
    return m_smallint;
}

inline SQLUSMALLINT
OdbcData::usmallint() const
{
    ctx_assert(m_type == Usmallint);
    return m_usmallint;
}

inline SQLINTEGER
OdbcData::integer() const
{
    ctx_assert(m_type == Integer);
    return m_integer;
}

inline SQLUINTEGER
OdbcData::uinteger() const
{
    ctx_assert(m_type == Uinteger);
    return m_uinteger;
}

inline SQLPOINTER
OdbcData::pointer() const
{
    ctx_assert(m_type == Pointer);
    return m_pointer;
}

inline SQLSMALLINT*
OdbcData::smallintPtr() const
{
    ctx_assert(m_type == SmallintPtr);
    return m_smallintPtr;
}

inline SQLUSMALLINT*
OdbcData::usmallintPtr() const
{
    ctx_assert(m_type == UsmallintPtr);
    return m_usmallintPtr;
}

inline SQLINTEGER*
OdbcData::integerPtr() const
{
    ctx_assert(m_type == IntegerPtr);
    return m_integerPtr;
}

inline SQLUINTEGER*
OdbcData::uintegerPtr() const
{
    ctx_assert(m_type == UintegerPtr);
    return m_uintegerPtr;
}

inline SQLPOINTER*
OdbcData::pointerPtr() const
{
    ctx_assert(m_type == PointerPtr);
    return m_pointerPtr;
}

inline const char*
OdbcData::sqlchar() const
{
    ctx_assert(m_type == Sqlchar);
    return m_sqlchar;
}

inline const ::Sqlstate&
OdbcData::sqlstate() const
{
    ctx_assert(m_type == Sqlstate);
    return *m_sqlstate;
}

#endif
