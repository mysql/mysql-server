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

#ifndef ODBC_COMMON_DataRow_hpp
#define ODBC_COMMON_DataRow_hpp

#include <new>
#include <common/common.hpp>
#include "DataField.hpp"

class Ctx;

/**
 * @class SqlSpecs
 * @brief Specification of row of SQL data
 */
class SqlSpecs {
public:
    SqlSpecs(unsigned count);
    SqlSpecs(const SqlSpecs& sqlSpecs);
    ~SqlSpecs();
    unsigned count() const;
    void setEntry(unsigned i, const SqlSpec& sqlSpec);
    const SqlSpec& getEntry(unsigned i) const;
private:
    SqlSpecs& operator=(const SqlSpecs& sqlSpecs);	// disallowed
    const unsigned m_count;
    SqlSpec* m_sqlSpec;
};

inline unsigned
SqlSpecs::count() const
{
    return m_count;
}

inline void
SqlSpecs::setEntry(unsigned i, const SqlSpec& sqlSpec)
{
    ctx_assert(m_sqlSpec != 0 && 1 <= i && i <= m_count);
    void* place = static_cast<void*>(&m_sqlSpec[i]);
    new (place) SqlSpec(sqlSpec);
}

inline const SqlSpec&
SqlSpecs::getEntry(unsigned i) const
{
    ctx_assert(m_sqlSpec != 0 && 1 <= i && i <= m_count);
    return m_sqlSpec[i];
}

/**
 * @class ExtSpecs
 * @brief Specification of row of external data
 */
class ExtSpecs {
public:
    ExtSpecs(unsigned count);
    ExtSpecs(const ExtSpecs& extSpecs);
    ~ExtSpecs();
    unsigned count() const;
    void setEntry(unsigned i, const ExtSpec& extSpec);
    const ExtSpec& getEntry(unsigned i) const;
private:
    ExtSpecs& operator=(const ExtSpecs& extSpecs);	// disallowed
    const unsigned m_count;
    ExtSpec* m_extSpec;
};

inline unsigned
ExtSpecs::count() const
{
    return m_count;
}

inline void
ExtSpecs::setEntry(unsigned i, const ExtSpec& extSpec)
{
    ctx_assert(m_extSpec != 0 && 1 <= i && i <= m_count);
    void* place = static_cast<void*>(&m_extSpec[i]);
    new (place) ExtSpec(extSpec);
}

inline const ExtSpec&
ExtSpecs::getEntry(unsigned i) const
{
    ctx_assert(m_extSpec != 0 && 1 <= i && i <= m_count);
    return m_extSpec[i];
}

/**
 * @class SqlRow
 * @brief Sql data row
 */
class SqlRow {
public:
    SqlRow(const SqlSpecs& sqlSpecs);
    SqlRow(const SqlRow& sqlRow);
    ~SqlRow();
    unsigned count() const;
    void setEntry(unsigned i, const SqlField& sqlField);
    SqlField& getEntry(unsigned i) const;
    SqlRow* copy() const;
    void copyout(Ctx& ctx, class ExtRow& extRow) const;
private:
    SqlRow& operator=(const SqlRow& sqlRow);	// disallowed
    SqlSpecs m_sqlSpecs;
    SqlField* m_sqlField;
};

inline unsigned
SqlRow::count() const
{
    return m_sqlSpecs.count();
}

inline void
SqlRow::setEntry(unsigned i, const SqlField& sqlField)
{
    ctx_assert(1 <= i && i <= count() && m_sqlField != 0);
    m_sqlField[i].~SqlField();
    void* place = static_cast<void*>(&m_sqlField[i]);
    new (place) SqlField(sqlField);
}

inline SqlField&
SqlRow::getEntry(unsigned i) const
{
    ctx_assert(1 <= i && i <= count() && m_sqlField != 0);
    return m_sqlField[i];
}

/**
 * @class ExtRow
 * @brief External data row
 */
class ExtRow {
public:
    ExtRow(const ExtSpecs& extSpecs);
    ExtRow(const ExtRow& extRow);
    ~ExtRow();
    unsigned count() const;
    void setEntry(unsigned i, const ExtField& extField);
    ExtField& getEntry(unsigned i) const;
private:
    ExtRow& operator=(const ExtRow& extRow);	// disallowed
    ExtSpecs m_extSpecs;
    ExtField* m_extField;
};

inline unsigned
ExtRow::count() const
{
    return m_extSpecs.count();
}

inline void
ExtRow::setEntry(unsigned i, const ExtField& extField)
{
    ctx_assert(1 <= i && i <= count() && m_extField != 0);
    void* place = static_cast<void*>(&m_extField[i]);
    new (place) ExtField(extField);
}

inline ExtField&
ExtRow::getEntry(unsigned i) const
{
    ctx_assert(1 <= i && i <= count() && m_extField != 0);
    return m_extField[i];
}

#endif
