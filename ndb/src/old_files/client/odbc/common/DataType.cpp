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

#include "DataType.hpp"

// SqlType

SqlType::SqlType() :
    m_type(Undef)
{
}

SqlType::SqlType(Type type, bool nullable)
{
    Ctx ctx;
    setType(ctx, type, nullable);
    ctx_assert(ctx.ok());
}

SqlType::SqlType(Type type, unsigned length, bool nullable)
{
    Ctx ctx;
    setType(ctx, type, length, nullable);
    ctx_assert(ctx.ok());
}

SqlType::SqlType(Type type, unsigned precision, unsigned scale, bool nullable)
{
    Ctx ctx;
    setType(ctx, type, precision, scale, nullable);
    ctx_assert(ctx.ok());
}

SqlType::SqlType(Ctx& ctx, Type type, bool nullable)
{
    setType(ctx, type, nullable);
}

SqlType::SqlType(Ctx& ctx, Type type, unsigned length, bool nullable)
{
    setType(ctx, type, length, nullable);
}

SqlType::SqlType(Ctx& ctx, Type type, unsigned precision, unsigned scale, bool nullable)
{
    setType(ctx, type, precision, scale, nullable);
}

SqlType::SqlType(Ctx& ctx, const NdbDictionary::Column* ndbColumn)
{
    setType(ctx, ndbColumn);
}

void
SqlType::setType(Ctx& ctx, Type type, bool nullable)
{
    switch (type) {
    case Smallint:
    case Integer:
    case Bigint:
    case Real:
    case Double:
    case Datetime:
	break;
    case Blob:
	setType(ctx, Varbinary, FAKE_BLOB_SIZE, nullable);	// XXX BLOB hack
	return;
    case Clob:
	setType(ctx, Varchar, FAKE_BLOB_SIZE, nullable);	// XXX BLOB hack
	return;
    case Null:
    case Unbound:
	break;
    default:
	ctx_assert(false);
	break;
    }
    m_type = type;
    m_precision = 0;
    m_scale = 0;
    m_length = 0;
    m_nullable = nullable;
    m_unSigned = false;
}

void
SqlType::setType(Ctx& ctx, Type type, unsigned length, bool nullable)
{
    switch (type) {
    case Char:
    case Varchar:
    case Binary:
    case Varbinary:
	break;
    default:
	ctx_assert(false);
	break;
    }
    m_type = type;
    m_precision = 0;
    m_scale = 0;
    m_length = length;
    m_nullable = nullable;
    m_unSigned = false;
}

void
SqlType::setType(Ctx& ctx, Type type, unsigned precision, unsigned scale, bool nullable)
{
    ctx_assert(false);		// not yet
}

void
SqlType::setType(Ctx& ctx, const NdbDictionary::Column* ndbColumn)
{
    NdbDictionary::Column::Type type = ndbColumn->getType();
    unsigned length = ndbColumn->getLength();
    unsigned precision = ndbColumn->getPrecision();
    unsigned scale = ndbColumn->getScale();
    bool nullable = ndbColumn->getNullable();
    switch (type) {
    case NdbDictionary::Column::Undefined:
	break;
    case NdbDictionary::Column::Int:
	if (length == 1)
	    setType(ctx, Integer, nullable);
	else
	    setType(ctx, Binary, length * sizeof(SqlInteger), nullable);
	return;
    case NdbDictionary::Column::Unsigned:
	if (length == 1) {
	    setType(ctx, Integer, nullable);
	    unSigned(true);
	} else
	    setType(ctx, Binary, length * sizeof(SqlUinteger), nullable);
	return;
    case NdbDictionary::Column::Bigint:
	if (length == 1)
	    setType(ctx, Bigint, nullable);
	else
	    setType(ctx, Binary, length * sizeof(SqlBigint), nullable);
	return;
    case NdbDictionary::Column::Bigunsigned:
	if (length == 1) {
	    setType(ctx, Bigint, nullable);
	    unSigned(true);
	} else
	    setType(ctx, Binary, length * sizeof(SqlBigint), nullable);
	return;
    case NdbDictionary::Column::Float:
	if (length == 1)
	    setType(ctx, Real, nullable);
	else
	    setType(ctx, Binary, length * sizeof(SqlReal), nullable);
	return;
    case NdbDictionary::Column::Double:
	if (length == 1)
	    setType(ctx, Double, nullable);
	else
	    setType(ctx, Binary, length * sizeof(SqlDouble), nullable);
	return;
    case NdbDictionary::Column::Decimal:
	setType(ctx, Decimal, precision, scale, nullable);
	return;
    case NdbDictionary::Column::Char:
	setType(ctx, Char, length, nullable);
	return;
    case NdbDictionary::Column::Varchar:
	setType(ctx, Varchar, length, nullable);
	return;
    case NdbDictionary::Column::Binary:
	setType(ctx, Binary, length, nullable);
	return;
    case NdbDictionary::Column::Varbinary:
	setType(ctx, Varbinary, length, nullable);
	return;
    case NdbDictionary::Column::Datetime:
	// XXX not yet
	break;
    case NdbDictionary::Column::Timespec:
	setType(ctx, Datetime, nullable);
	return;
    case NdbDictionary::Column::Blob:
	setType(ctx, Blob, nullable);
	return;
    case NdbDictionary::Column::Clob:
	setType(ctx, Clob, nullable);
	return;
    default:
	break;
    }
    ctx.pushStatus(Error::Gen, "unsupported NDB type %d", (signed)type);
}

bool
SqlType::equal(const SqlType& sqlType) const
{
    return
	m_type == sqlType.m_type &&
	m_precision == sqlType.m_precision &&
	m_scale == sqlType.m_scale &&
	m_length == sqlType.m_length;
}

unsigned
SqlType::size() const
{
    switch (m_type) {
    case Char:
    case Varchar:
    case Binary:
    case Varbinary:
	return m_length;
    case Smallint:
	return sizeof(SqlSmallint);
    case Integer:
	return sizeof(SqlInteger);
    case Bigint:
	return sizeof(SqlBigint);
    case Real:
	return sizeof(SqlReal);
    case Double:
	return sizeof(SqlDouble);
    case Datetime:
	return sizeof(SqlDatetime);
    case Null:
	return 0;
    default:
	break;
    }
    ctx_assert(false);
    return 0;
}

unsigned
SqlType::displaySize() const
{
    switch (m_type) {
    case Char:
    case Varchar:
	return m_length;
    case Binary:
    case Varbinary:
	return m_length;
    case Smallint:
	return m_unSigned ? 5 : 6;
    case Integer:
	return m_unSigned ? 10 : 11;
    case Bigint:
	return m_unSigned ? 20 : 21;
    case Real:
	return 10;
    case Double:
	return 20;
    case Datetime:
	return 30;
    case Null:
	return 0;
    default:
	break;
    }
    ctx_assert(false);
    return 0;
}

void
SqlType::getType(Ctx& ctx, NdbDictionary::Column* ndbColumn) const
{
    switch (m_type) {
    case Char:
	ndbColumn->setType(NdbDictionary::Column::Char);
	ndbColumn->setLength(m_length);
	break;
    case Varchar:
	ndbColumn->setType(NdbDictionary::Column::Varchar);
	ndbColumn->setLength(m_length);
	break;
    case Binary:
	ndbColumn->setType(NdbDictionary::Column::Binary);
	ndbColumn->setLength(m_length);
	break;
    case Varbinary:
	ndbColumn->setType(NdbDictionary::Column::Varbinary);
	ndbColumn->setLength(m_length);
	break;
    case Smallint:
	break;	// XXX
    case Integer:
	if (! m_unSigned)
	    ndbColumn->setType(NdbDictionary::Column::Int);
	else
	    ndbColumn->setType(NdbDictionary::Column::Unsigned);
	ndbColumn->setLength(1);
	break;
    case Bigint:
	if (! m_unSigned)
	    ndbColumn->setType(NdbDictionary::Column::Bigint);
	else
	    ndbColumn->setType(NdbDictionary::Column::Bigunsigned);
	ndbColumn->setLength(1);
	break;
    case Real:
	ndbColumn->setType(NdbDictionary::Column::Float);
	ndbColumn->setLength(1);
	break;
    case Double:
	ndbColumn->setType(NdbDictionary::Column::Double);
	ndbColumn->setLength(1);
	break;
    case Datetime:
	ndbColumn->setType(NdbDictionary::Column::Timespec);
	ndbColumn->setLength(1);
	break;
    default:
	ctx_assert(false);
	break;
    }
    ndbColumn->setNullable(m_nullable);
}

const char*
SqlType::typeName() const
{
    switch (m_type) {
    case Char:
	return "CHAR";
    case Varchar:
	return "VARCHAR";
    case Binary:
	return "BINARY";
    case Varbinary:
	return "VARBINARY";
    case Smallint:
	return "SMALLINT";
    case Integer:
	return "INTEGER";
    case Bigint:
	return "BIGINT";
    case Real:
	return "REAL";
    case Double:
	return "FLOAT";
    case Datetime:
	return "DATETIME";
    default:
	break;
    }
    return "UNKNOWN";
}

void
SqlType::print(char* buf, unsigned size) const
{
    switch (m_type) {
    case Char:
	snprintf(buf, size, "char(%d)", m_length);
	break;
    case Varchar:
	snprintf(buf, size, "varchar(%d)", m_length);
	break;
    case Binary:
	snprintf(buf, size, "binary(%d)", m_length);
	break;
    case Varbinary:
	snprintf(buf, size, "varbinary(%d)", m_length);
	break;
    case Smallint:
	snprintf(buf, size, "smallint%s", m_unSigned ? " unsigned" : "");
	break;
    case Integer:
	snprintf(buf, size, "integer%s", m_unSigned ? " unsigned" : "");
	break;
    case Bigint:
	snprintf(buf, size, "bigint%s", m_unSigned ? " unsigned" : "");
	break;
    case Real:
	snprintf(buf, size, "real");
	break;
    case Double:
	snprintf(buf, size, "double");
	break;
    case Datetime:
	snprintf(buf, size, "datetime");
	break;
    case Null:
	snprintf(buf, size, "null");
	break;
    case Unbound:
	snprintf(buf, size, "unbound");
	break;
    default:
	snprintf(buf, size, "sqltype(%d)", (int)m_type);
	break;
    }
}

// ExtType

ExtType::ExtType() :
    m_type(Undef)
{
}

ExtType::ExtType(Type type)
{
    Ctx ctx;
    setType(ctx, type);
    ctx_assert(ctx.ok());
}

ExtType::ExtType(Ctx& ctx, Type type)
{
    setType(ctx, type);
}

void
ExtType::setType(Ctx& ctx, Type type)
{
    switch (type) {
    case Char:
    case Short:
    case Sshort:
    case Ushort:
    case Long:
    case Slong:
    case Ulong:
    case Sbigint:
    case Ubigint:
    case Float:
    case Double:
    case Timestamp:
    case Binary:	// XXX BLOB hack
    case Unbound:
	break;
    default:
	ctx.pushStatus(Error::Gen, "unsupported external type %d", (int)type);
	return;
    }
    m_type = type;
}

unsigned
ExtType::size() const
{
    ctx_assert(false);
    return 0;
}

// LexType

LexType::LexType() :
    m_type(Undef)
{
}

LexType::LexType(Type type)
{
    Ctx ctx;
    setType(ctx, type);
    ctx_assert(ctx.ok());
}

LexType::LexType(Ctx& ctx, Type type)
{
    setType(ctx, type);
}

void
LexType::setType(Ctx& ctx, Type type)
{
    switch (type) {
    case Char:
    case Integer:
    case Float:
    case Null:
	break;
    default:
	ctx_assert(false);
	break;
    }
    m_type = type;
}

// convert types

SQLSMALLINT
SqlType::sqlcdefault(Ctx& ctx) const
{
    switch (m_type) {
    case Char:
	return SQL_C_CHAR;
    case Varchar:
	return SQL_C_CHAR;
    case Binary:
	return SQL_C_BINARY;
    case Varbinary:
	return SQL_C_BINARY;
    case Smallint:
	return m_unSigned ? SQL_C_USHORT : SQL_C_SSHORT;
    case Integer:
	return m_unSigned ? SQL_C_ULONG : SQL_C_SLONG;
    case Bigint:
	return SQL_C_CHAR;
	// or maybe this
	return m_unSigned ? SQL_C_UBIGINT : SQL_C_SBIGINT;
    case Real:
	return SQL_C_FLOAT;
    case Double:
	return SQL_C_DOUBLE;
    case Datetime:
	return SQL_C_TYPE_TIMESTAMP;
    default:
	break;
    }
    return SQL_C_DEFAULT;	// no default
}

void
LexType::convert(Ctx& ctx, SqlType& out, unsigned length) const
{
    switch (m_type) {
    case Char:
	out.setType(ctx, SqlType::Char, length, true);
	return;
    case Integer:
	out.setType(ctx, SqlType::Bigint, false);
	return;
    case Float:
	out.setType(ctx, SqlType::Double, false);
	return;
    case Null:
	out.setType(ctx, SqlType::Null, true);
	return;
    default:
	break;
    }
    ctx.pushStatus(Error::Gen, "unsupported lexical to SQL type conversion");
}
