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

#ifndef ODBC_DICTIONARY_DictIndex_hpp
#define ODBC_DICTIONARY_DictIndex_hpp

#include <vector>
#include <common/common.hpp>
#include "DictColumn.hpp"

class Ctx;
class ConnArea;
class DictTable;
class DictColumn;
class DictIndex;

/**
 * @class DictIndex
 * @brief Database table
 */
class DictIndex {
    friend class DictSchema;
public:
    DictIndex(const ConnArea& connArea, const BaseString& name, NdbDictionary::Object::Type type, unsigned size);
    ~DictIndex();
    NdbDictionary::Object::Type getType() const;
    unsigned getSize() const;
    void setTable(DictTable* table);
    DictTable* getTable() const;
    void setColumn(unsigned i, DictColumn* column);
    DictColumn* getColumn(unsigned i) const;
    const BaseString& getName() const;
    DictColumn* findColumn(const BaseString& name) const;
protected:
    const ConnArea& m_connArea;
    const BaseString m_name;
    const NdbDictionary::Object::Type m_type;
    const unsigned m_size;
    DictSchema* m_parent;
    DictTable* m_table;
    typedef std::vector<DictColumn*> Columns;	// pointers to table columns
    Columns m_columns;
};

inline
DictIndex::DictIndex(const ConnArea& connArea, const BaseString& name, NdbDictionary::Object::Type type, unsigned size) :
    m_connArea(connArea),
    m_name(name),
    m_type(type),
    m_size(size),
    m_parent(0),
    m_columns(1 + size)
{
}

inline NdbDictionary::Object::Type
DictIndex::getType() const
{
    return m_type;
}

inline unsigned
DictIndex::getSize() const
{
    ctx_assert(m_columns.size() == 1 + m_size);
    return m_size;
}

inline void
DictIndex::setTable(DictTable* table)
{
    m_table = table;
}

inline DictTable*
DictIndex::getTable() const
{
    return m_table;
}

inline DictColumn*
DictIndex::getColumn(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_size);
    ctx_assert(m_columns[i] != 0);
    return m_columns[i];
}

inline const BaseString&
DictIndex::getName() const
{
    return m_name;
}

#endif
