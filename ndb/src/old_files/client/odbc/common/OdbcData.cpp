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

#include "OdbcData.hpp"

OdbcData::OdbcData() :
    m_type(Undef)
{
}

OdbcData::OdbcData(Type type) :
    m_type(type)
{
    switch (m_type) {
    case Smallint:
	m_smallint = 0;
	break;
    case Usmallint:
	m_usmallint = 0;
	break;
    case Integer:
	m_integer = 0;
	break;
    case Uinteger:
	m_uinteger = 0;
	break;
    case Pointer:
	m_pointer = 0;
	break;
    case SmallintPtr:
	m_smallintPtr = 0;
	break;
    case UsmallintPtr:
	m_usmallintPtr = 0;
	break;
    case IntegerPtr:
	m_integerPtr = 0;
	break;
    case UintegerPtr:
	m_uintegerPtr = 0;
	break;
    case PointerPtr:
	m_pointerPtr = 0;
	break;
    case Sqlchar:
	m_sqlchar = 0;
	break;
    case Sqlstate:
	m_sqlstate = 0;
	break;
    default:
	ctx_assert(false);
	break;
    };
}

OdbcData::OdbcData(const OdbcData& odbcData) :
    m_type(odbcData.m_type)
{
    switch (m_type) {
    case Smallint:
	m_smallint = odbcData.m_smallint;
	break;
    case Usmallint:
	m_usmallint = odbcData.m_usmallint;
	break;
    case Integer:
	m_integer = odbcData.m_integer;
	break;
    case Uinteger:
	m_uinteger = odbcData.m_uinteger;
	break;
    case Pointer:
	m_pointer = odbcData.m_pointer;
	break;
    case SmallintPtr:
	m_smallintPtr = odbcData.m_smallintPtr;
	break;
    case UsmallintPtr:
	m_usmallintPtr = odbcData.m_usmallintPtr;
	break;
    case IntegerPtr:
	m_integerPtr = odbcData.m_integerPtr;
	break;
    case UintegerPtr:
	m_uintegerPtr = odbcData.m_uintegerPtr;
	break;
    case PointerPtr:
	m_pointerPtr = odbcData.m_pointerPtr;
	break;
    case Sqlchar: {
	unsigned n = strlen(odbcData.m_sqlchar);
	m_sqlchar = new char[n + 1];
	memcpy(m_sqlchar, odbcData.m_sqlchar, n + 1);
	break;
	}
    case Sqlstate:
	m_sqlstate = odbcData.m_sqlstate;
	break;
    default:
	ctx_assert(false);
	break;
    };
}

OdbcData::~OdbcData()
{
    switch (m_type) {
    case Sqlchar:
	delete[] m_sqlchar;
	break;
    default:
	break;
    }
}

void
OdbcData::setValue()
{
    m_type = Undef;
}

void
OdbcData::setValue(Type type)
{
    if (m_type == Sqlchar) {
	delete[] m_sqlchar;
	m_sqlchar = 0;
    }
    switch (m_type) {
    case Smallint:
	m_smallint = 0;
	break;
    case Usmallint:
	m_usmallint = 0;
	break;
    case Integer:
	m_integer = 0;
	break;
    case Uinteger:
	m_uinteger = 0;
	break;
    case Pointer:
	m_pointer = 0;
	break;
    case SmallintPtr:
	m_smallintPtr = 0;
	break;
    case UsmallintPtr:
	m_usmallintPtr = 0;
	break;
    case IntegerPtr:
	m_integerPtr = 0;
	break;
    case UintegerPtr:
	m_uintegerPtr = 0;
	break;
    case PointerPtr:
	m_pointerPtr = 0;
	break;
    case Sqlchar:
	m_sqlchar = 0;
	break;
    case Sqlstate:
	m_sqlstate = 0;
	break;
    default:
	ctx_assert(false);
	break;
    };
}

void
OdbcData::setValue(const OdbcData odbcData)
{
    if (m_type == Sqlchar) {
	delete[] m_sqlchar;
	m_sqlchar = 0;
    }
    m_type = odbcData.m_type;
    switch (m_type) {
    case Smallint:
	m_smallint = odbcData.m_smallint;
	break;
    case Usmallint:
	m_usmallint = odbcData.m_usmallint;
	break;
    case Integer:
	m_integer = odbcData.m_integer;
	break;
    case Uinteger:
	m_uinteger = odbcData.m_uinteger;
	break;
    case Pointer:
	m_pointer = odbcData.m_pointer;
	break;
    case SmallintPtr:
	m_smallintPtr = odbcData.m_smallintPtr;
	break;
    case UsmallintPtr:
	m_usmallintPtr = odbcData.m_usmallintPtr;
	break;
    case IntegerPtr:
	m_integerPtr = odbcData.m_integerPtr;
	break;
    case UintegerPtr:
	m_uintegerPtr = odbcData.m_uintegerPtr;
	break;
    case PointerPtr:
	m_pointerPtr = odbcData.m_pointerPtr;
	break;
    case Sqlchar: {
	unsigned n = strlen(odbcData.m_sqlchar);
	m_sqlchar = new char[n + 1];
	memcpy(m_sqlchar, odbcData.m_sqlchar, n + 1);
	break;
	}
    case Sqlstate:
	m_sqlstate = odbcData.m_sqlstate;
	break;
    default:
	ctx_assert(false);
	break;
    };
}

// copy in from user buffer

void
OdbcData::copyin(Ctx& ctx, Type type, SQLPOINTER buf, SQLINTEGER length)
{
    if (m_type == Sqlchar) {
	delete[] m_sqlchar;
	m_sqlchar = 0;
    }
    m_type = type;
    switch (m_type) {
    case Smallint: {
	SQLSMALLINT val = 0;
	switch (length) {
	case 0:
	case SQL_IS_SMALLINT:
	    val = (SQLSMALLINT)(SQLINTEGER)buf;
	    break;
	case SQL_IS_USMALLINT:
	    val = (SQLUSMALLINT)(SQLUINTEGER)buf;
	    break;
	case SQL_IS_INTEGER:
	    val = (SQLINTEGER)buf;
	    break;
	case SQL_IS_UINTEGER:
	    val = (SQLUINTEGER)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "smallint input - invalid length %d", (int)length);
	    return;
	}
	m_smallint = val;
	break;
	}
    case Usmallint: {
	SQLUSMALLINT val = 0;
	switch (length) {
	case SQL_IS_SMALLINT:
	    val = (SQLSMALLINT)(SQLINTEGER)buf;
	    break;
	case 0:
	case SQL_IS_USMALLINT:
	    val = (SQLUSMALLINT)(SQLUINTEGER)buf;
	    break;
	case SQL_IS_INTEGER:
	    val = (SQLINTEGER)buf;
	    break;
	case SQL_IS_UINTEGER:
	    val = (SQLUINTEGER)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "unsigned smallint input - invalid length %d", (int)length);
	    return;
	}
	m_usmallint = val;
	break;
	}
    case Integer: {
	SQLINTEGER val = 0;
	switch (length) {
	case SQL_IS_SMALLINT:
	    val = (SQLSMALLINT)(SQLINTEGER)buf;
	    break;
	case SQL_IS_USMALLINT:
	    val = (SQLUSMALLINT)(SQLUINTEGER)buf;
	    break;
	case 0:
	case SQL_IS_INTEGER:
	    val = (SQLINTEGER)buf;
	    break;
	case SQL_IS_UINTEGER:
	    val = (SQLUINTEGER)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "integer input - invalid length %d", (int)length);
	    return;
	}
	m_integer = val;
	break;
	}
    case Uinteger: {
	SQLUINTEGER val = 0;
	switch (length) {
	case SQL_IS_SMALLINT:
	    val = (SQLSMALLINT)(SQLINTEGER)buf;
	    break;
	case SQL_IS_USMALLINT:
	    val = (SQLUSMALLINT)(SQLUINTEGER)buf;
	    break;
	case SQL_IS_INTEGER:
	    val = (SQLINTEGER)buf;
	    break;
	case 0:
	case SQL_IS_UINTEGER:
	    val = (SQLUINTEGER)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "unsigned integer input - invalid length %d", (int)length);
	    return;
	}
	m_uinteger = val;
	break;
	}
    case Pointer: {
	SQLPOINTER val = 0;
	switch (length) {
	case 0:
	case SQL_IS_POINTER:
	    val = (SQLPOINTER)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "pointer input - invalid length %d", (int)length);
	    return;
	}
	m_pointer = val;
	break;
	}
    case SmallintPtr: {
	SQLSMALLINT* val = 0;
	switch (length) {
	case 0:
	case SQL_IS_POINTER:
	    val = (SQLSMALLINT*)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "smallint pointer input - invalid length %d", (int)length);
	    return;
	}
	m_smallintPtr = val;
	break;
	}
    case UsmallintPtr: {
	SQLUSMALLINT* val = 0;
	switch (length) {
	case 0:
	case SQL_IS_POINTER:
	    val = (SQLUSMALLINT*)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "unsigned smallint pointer input - invalid length %d", (int)length);
	    return;
	}
	m_usmallintPtr = val;
	break;
	}
    case IntegerPtr: {
	SQLINTEGER* val = 0;
	switch (length) {
	case 0:
	case SQL_IS_POINTER:
	    val = (SQLINTEGER*)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "integer pointer input - invalid length %d", (int)length);
	    return;
	}
	m_integerPtr = val;
	break;
	}
    case UintegerPtr: {
	SQLUINTEGER* val = 0;
	switch (length) {
	case 0:
	case SQL_IS_POINTER:
	    val = (SQLUINTEGER*)buf;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "unsigned integer pointer input - invalid length %d", (int)length);
	    return;
	}
	m_uintegerPtr = val;
	break;
	}
    case Sqlchar: {
	const char* val = (char*)buf;
	if (val == 0) {
	    ctx.pushStatus(Sqlstate::_HY009, Error::Gen, "null string input");
	    return;
	}
	if (length < 0 && length != SQL_NTS) {
	    ctx.pushStatus(Error::Gen, "string input - invalid length %d", (int)length);
	    return;
	}
	if (length == SQL_NTS) {
	    m_sqlchar = strcpy(new char[strlen(val) + 1], val);
	} else {
	    m_sqlchar = (char*)memcpy(new char[length + 1], val, length);
	    m_sqlchar[length] = 0;
	}
	break;
	}
    default:
	ctx_assert(false);
	break;
    }
}

// copy out to user buffer

void
OdbcData::copyout(Ctx& ctx, SQLPOINTER buf, SQLINTEGER length, SQLINTEGER* total, SQLSMALLINT* total2)
{
    if (buf == 0) {
	ctx.setCode(SQL_ERROR);
	return;
    }
    switch (m_type) {
    case Smallint: {
	SQLSMALLINT* ptr = static_cast<SQLSMALLINT*>(buf);
	*ptr = m_smallint;
	break;
	}
    case Usmallint: {
	SQLUSMALLINT* ptr = static_cast<SQLUSMALLINT*>(buf);
	*ptr = m_usmallint;
	break;
	}
    case Integer: {
	SQLINTEGER* ptr = static_cast<SQLINTEGER*>(buf);
	*ptr = m_integer;
	break;
	}
    case Uinteger: {
	SQLUINTEGER* ptr = static_cast<SQLUINTEGER*>(buf);
	*ptr = m_uinteger;
	break;
	}
    case Pointer: {
	SQLPOINTER* ptr = static_cast<SQLPOINTER*>(buf);
	*ptr = m_pointer;
	break;
	}
    case Sqlchar: {
	char* ptr = static_cast<char*>(buf);
	if (length < 0 && length != SQL_NTS) {
	    ctx.setCode(SQL_ERROR);
	    return;
	}
	if (length == SQL_NTS) {
	    strcpy(ptr, m_sqlchar);
	} else {
	    strncpy(ptr, m_sqlchar, length);
	}
	if (total != 0)
	    *total = strlen(m_sqlchar);
	if (total2 != 0)
	    *total2 = strlen(m_sqlchar);
	break;
	}
    case Sqlstate: {
	char* ptr = static_cast<char*>(buf);
	const char* state = m_sqlstate->state();
	if (length < 0 && length != SQL_NTS) {
	    ctx.setCode(SQL_ERROR);
	    return;
	}
	if (length == SQL_NTS) {
	    strcpy(ptr, state);
	} else {
	    strncpy(ptr, state, length);
	}
	if (total != 0)
	    *total = strlen(state);
	if (total2 != 0)
	    *total2 = strlen(state);
	break;
	}
    default:
	ctx_assert(false);
	break;
    }
}

void
OdbcData::print(char* buf, unsigned size) const
{
    switch (m_type) {
    case Undef:
	snprintf(buf, size, "undef");
	break;
    case Smallint:
	snprintf(buf, size, "%d", (int)m_smallint);
	break;
    case Usmallint:
	snprintf(buf, size, "%u", (unsigned)m_usmallint);
	break;
    case Integer:
	snprintf(buf, size, "%ld", (long)m_integer);
	break;
    case Uinteger:
	snprintf(buf, size, "%lu", (unsigned long)m_uinteger);
	break;
    case Pointer:
	snprintf(buf, size, "0x%lx", (unsigned long)m_pointer);
	break;
    case SmallintPtr:
	snprintf(buf, size, "0x%lx", (unsigned long)m_smallintPtr);
	break;
    case UsmallintPtr:
	snprintf(buf, size, "0x%lx", (unsigned long)m_usmallintPtr);
	break;
    case IntegerPtr:
	snprintf(buf, size, "0x%lx", (unsigned long)m_integerPtr);
	break;
    case UintegerPtr:
	snprintf(buf, size, "0x%lx", (unsigned long)m_uintegerPtr);
	break;
    case PointerPtr:
	snprintf(buf, size, "0x%lx", (unsigned long)m_pointerPtr);
	break;
    case Sqlchar:
	snprintf(buf, size, "%s", m_sqlchar);
	break;
    case Sqlstate:
	snprintf(buf, size, "%s", m_sqlstate->state());
	break;
    default:
	snprintf(buf, size, "data(%d)", (int)m_type);
	break;
    };
}
