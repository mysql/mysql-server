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

#ifndef ODBC_COMMON_DataField_hpp
#define ODBC_COMMON_DataField_hpp

#include <NdbApi.hpp>
#include <common/common.hpp>
#include "DataType.hpp"

/**
 * @class SqlSpec
 * @brief Specification of data in SQL format
 */
class SqlSpec {
public:
    enum Store {
	Undef = 0,
	Reference = 1,	// reference to read-only SqlField of same type
	Physical = 2	// stored within or in allocated storage
    };
    SqlSpec();
    SqlSpec(const SqlType& sqlType, Store store);
    SqlSpec(const SqlSpec& sqlSpec);
    SqlSpec(const SqlSpec& sqlSpec, Store store);
    const SqlType& sqlType() const;
    const Store store() const;
    unsigned size() const;
private:
    //SqlSpec& operator=(const SqlSpec& sqlSpec);		// disallowed
    SqlType m_sqlType;
    Store m_store;
};

/**
 * @class ExtSpec
 * @brief Specification of data in external format
 */
class ExtSpec {
public:
    ExtSpec();
    ExtSpec(const ExtType& extType);
    ExtSpec(const ExtSpec& extSpec);
    const ExtType& extType() const;
    unsigned size() const;
    void setValue(const ExtType& extType);
private:
    ExtType m_extType;
};

/**
 * @class LexSpec
 * @brief Specification of lexical data
 *
 * Used only for converting lexical data to SQL data.
 */
class LexSpec {
public:
    LexSpec();
    LexSpec(const LexType& lexType);
    /**
     * Lexical data is represented as string.  Following
     * converts it to SQL data.
     */
    void convert(Ctx& ctx, const BaseString& value, class SqlField& out);
private:
    LexType m_lexType;
};

// SqlSpec

inline
SqlSpec::SqlSpec() :
    m_store(Undef)
{
}

inline
SqlSpec::SqlSpec(const SqlType& sqlType, Store store) :
    m_sqlType(sqlType),
    m_store(store)
{
}

inline
SqlSpec::SqlSpec(const SqlSpec& sqlSpec) :
    m_sqlType(sqlSpec.m_sqlType),
    m_store(sqlSpec.m_store)
{
}

inline
SqlSpec::SqlSpec(const SqlSpec& sqlSpec, Store store) :
    m_sqlType(sqlSpec.m_sqlType),
    m_store(store)
{
}

inline const SqlType&
SqlSpec::sqlType() const
{
    return m_sqlType;
}

inline const SqlSpec::Store
SqlSpec::store() const
{
    return m_store;
}

inline unsigned
SqlSpec::size() const
{
    return sqlType().size();
}

// ExtSpec

inline
ExtSpec::ExtSpec()
{
}

inline
ExtSpec::ExtSpec(const ExtType& extType) :
    m_extType(extType)
{
}

inline
ExtSpec::ExtSpec(const ExtSpec& extSpec) :
    m_extType(extSpec.m_extType)
{
}

inline const ExtType&
ExtSpec::extType() const
{
    return m_extType;
}

inline unsigned
ExtSpec::size() const
{
    return m_extType.size();
}

inline void
ExtSpec::setValue(const ExtType& extType)
{
    m_extType = extType;
}

// LexSpec

inline
LexSpec::LexSpec(const LexType& lexType) :
    m_lexType(lexType)
{
}

/**
 * @class SqlField
 * @brief Sql data field accessor
 */
class SqlField {
public:
    SqlField();
    SqlField(const SqlSpec& sqlSpec);
    SqlField(const SqlSpec& sqlSpec, const SqlField* sqlField);
    SqlField(const SqlField& sqlField);
    ~SqlField();
    const SqlSpec& sqlSpec() const;
    const void* addr() const;		// address of data
    void* addr();
    unsigned allocSize() const;
    const SqlChar* sqlChar() const;			// get
    const SqlChar* sqlVarchar(unsigned* length) const;
    const SqlChar* sqlBinary() const;
    const SqlChar* sqlVarbinary(unsigned* length) const;
    SqlSmallint sqlSmallint() const;
    SqlInteger sqlInteger() const;
    SqlBigint sqlBigint() const;
    SqlReal sqlReal() const;
    SqlDouble sqlDouble() const;
    SqlDatetime sqlDatetime() const;
    void sqlChar(const char* value, int length);	// set
    void sqlChar(const SqlChar* value, int length);
    void sqlVarchar(const char* value, int length);
    void sqlVarchar(const SqlChar* value, int length);
    void sqlBinary(const char* value, int length);
    void sqlBinary(const SqlChar* value, int length);
    void sqlVarbinary(const char* value, int length);
    void sqlVarbinary(const SqlChar* value, int length);
    void sqlSmallint(SqlSmallint value);
    void sqlInteger(SqlInteger value);
    void sqlBigint(SqlBigint value);
    void sqlReal(SqlReal value);
    void sqlDouble(SqlDouble value);
    void sqlDatetime(SqlDatetime value);
    bool sqlNull() const;		// get and set null
    void sqlNull(bool value);
    unsigned trim() const;		// right trimmed length
    void copy(Ctx& ctx, SqlField& out) const;
    bool cast(Ctx& ctx, SqlField& out) const;
    bool less(const SqlField& sqlField) const;
    // application input and output
    void copyin(Ctx& ctx, class ExtField& extField);
    void copyout(Ctx& ctx, class ExtField& extField) const;
    // print for debugging
    void print(char* buf, unsigned size) const;
    // public for forte6
    //enum { CharSmall = 20 };
#define SqlField_CharSmall 20	// redhat-6.2 (egcs-2.91.66)
private:
    friend class LexSpec;
    friend class SqlRow;
    void alloc();			// allocate Physical
    void alloc(const SqlField& sqlField);
    void free();			// free Physical
    SqlSpec m_sqlSpec;
    union Data {
	Data();	
	Data(const SqlField* sqlField);
	const SqlField* m_sqlField;
	// Physical
	SqlChar* m_sqlChar;		// all char types
	SqlChar m_sqlCharSmall[SqlField_CharSmall];
	SqlSmallint m_sqlSmallint;
	SqlInteger m_sqlInteger;
	SqlBigint m_sqlBigint;
	SqlReal m_sqlReal;
	SqlDouble m_sqlDouble;
	SqlDatetime m_sqlDatetime;
    } u_data;
    union Null {
	Null();
	bool m_nullFlag;
    } u_null;
};

/**
 * @class ExtField
 * @brief External data field accessor
 */
class ExtField {
public:
    ExtField();
    ExtField(const ExtSpec& extSpec, int fieldId = 0);
    ExtField(const ExtSpec& extSpec, SQLPOINTER dataPtr, SQLINTEGER dataLen, SQLINTEGER* indPtr, int fieldId = 0);
    ~ExtField();
    const ExtSpec& extSpec() const;
    void setValue(SQLPOINTER dataPtr, SQLINTEGER dataLen);
    void setValue(const ExtSpec& extSpec, SQLPOINTER dataPtr, SQLINTEGER dataLen, SQLINTEGER* indPtr);
    int fieldId() const;
    void setPos(int pos);
    int getPos() const;
private:
    friend class SqlField;
    friend class Exec_root;
    ExtSpec m_extSpec;
    SQLPOINTER m_dataPtr;		// data buffer
    SQLINTEGER m_dataLen;		// data buffer length
    SQLINTEGER* m_indPtr;		// null indicator or length
    int m_fieldId;			// field id > 0 for error messages
    int m_pos;				// output position for SQLGetData (if != -1)
};

inline int
ExtField::fieldId() const
{
    return m_fieldId;
}

inline void
ExtField::setPos(int pos)
{
    m_pos = pos;
}

inline int
ExtField::getPos() const
{
    return m_pos;
}

// SqlField

inline
SqlField::SqlField()
{
}

inline
SqlField::SqlField(const SqlSpec& sqlSpec) :
    m_sqlSpec(sqlSpec)
{
    if (m_sqlSpec.store() == SqlSpec::Physical)
	alloc();
}

inline
SqlField::SqlField(const SqlSpec& sqlSpec, const SqlField* sqlField) :
    m_sqlSpec(sqlSpec),
    u_data(sqlField)
{
    ctx_assert(m_sqlSpec.store() == SqlSpec::Reference);
}

inline
SqlField::SqlField(const SqlField& sqlField) :
    m_sqlSpec(sqlField.m_sqlSpec),
    u_data(sqlField.u_data),
    u_null(sqlField.u_null)
{
    if (m_sqlSpec.store() == SqlSpec::Physical)
	alloc(sqlField);
}

inline
SqlField::Data::Data()
{
}

inline
SqlField::Data::Data(const SqlField* sqlField)
{
    m_sqlField = sqlField;
}

inline
SqlField::Null::Null()
{
}

inline
SqlField::~SqlField()
{
    if (m_sqlSpec.store() == SqlSpec::Physical)
	free();
}

inline const SqlSpec&
SqlField::sqlSpec() const
{
    return m_sqlSpec;
}

inline void
SqlField::sqlChar(const char* value, int length)
{
    sqlChar(reinterpret_cast<const SqlChar*>(value), length);
}

inline void
SqlField::sqlVarchar(const char* value, int length)
{
    sqlVarchar(reinterpret_cast<const SqlChar*>(value), length);
}

inline void
SqlField::sqlBinary(const char* value, int length)
{
    sqlBinary(reinterpret_cast<const SqlChar*>(value), length);
}

inline void
SqlField::sqlVarbinary(const char* value, int length)
{
    sqlVarbinary(reinterpret_cast<const SqlChar*>(value), length);
}

// ExtField

inline
ExtField::ExtField() :
    m_dataPtr(0),
    m_dataLen(0),
    m_indPtr(0),
    m_pos(-1)
{
}

inline
ExtField::ExtField(const ExtSpec& extSpec, int fieldId) :
    m_extSpec(extSpec),
    m_dataPtr(0),
    m_dataLen(0),
    m_indPtr(0),
    m_fieldId(fieldId),
    m_pos(-1)
{
}

inline
ExtField::ExtField(const ExtSpec& extSpec, SQLPOINTER dataPtr, SQLINTEGER dataLen, SQLINTEGER* indPtr, int fieldId) :
    m_extSpec(extSpec),
    m_dataPtr(dataPtr),
    m_dataLen(dataLen),
    m_indPtr(indPtr),
    m_fieldId(fieldId),
    m_pos(-1)
{
}

inline
ExtField::~ExtField()
{
}

inline const ExtSpec&
ExtField::extSpec() const
{
    return m_extSpec;
}

inline void
ExtField::setValue(SQLPOINTER dataPtr, SQLINTEGER dataLen)
{
    m_dataPtr = dataPtr;
    m_dataLen = dataLen;
}

inline void
ExtField::setValue(const ExtSpec& extSpec, SQLPOINTER dataPtr, SQLINTEGER dataLen, SQLINTEGER* indPtr)
{
    m_extSpec.setValue(extSpec.extType());
    m_dataPtr = dataPtr;
    m_dataLen = dataLen;
    m_indPtr = indPtr;
}

#endif
