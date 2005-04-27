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

#ifndef ODBC_DICTIONARY_DictTable_hpp
#define ODBC_DICTIONARY_DictTable_hpp

#include <vector>
#include <list>
#include <common/common.hpp>
#include "DictColumn.hpp"
#include "DictIndex.hpp"
#include "DictSys.hpp"

class Ctx;
class ConnArea;
class DictSchema;
class DictColumn;
class DictIndex;

/**
 * @class DictTable
 * @brief Database table
 */
class DictTable {
    friend class DictSchema;
public:
    DictTable(const ConnArea& connArea, const BaseString& name, unsigned size);
    ~DictTable();
    unsigned getSize() const;
    void setParent(DictSchema* parent);
    DictSchema* getParent() const;
    void setColumn(unsigned i, DictColumn* column);
    DictColumn* getColumn(unsigned i) const;
    const BaseString& getName() const;
    DictColumn* findColumn(const BaseString& name) const;
    DictColumn* loadColumn(Ctx& ctx, unsigned position);
    unsigned keyCount() const;
    DictColumn* getKey(unsigned i) const;
    unsigned tupleId() const;
    unsigned autoIncrement() const;
    void sysId(DictSys::Id id);
    DictSys::Id sysId() const;
    // indexes
    void addIndex(DictIndex* index);
    unsigned indexCount() const;
    const DictIndex* getIndex(unsigned i) const;	// indexed from 1
protected:
    friend class DictSys;
    const ConnArea& m_connArea;
    const BaseString m_name;
    unsigned m_size;
    DictSchema* m_parent;
    typedef std::vector<DictColumn*> Columns;
    Columns m_columns;
    Columns m_keys;
    unsigned m_tupleId;		// tuple id column
    unsigned m_autoIncrement;	// autoincrement key
    DictSys::Id m_sysId;	// built-in system table id (if non-zero)
    typedef std::vector<DictIndex*> Indexes;
    Indexes m_indexes;
};

inline
DictTable::DictTable(const ConnArea& connArea, const BaseString& name, unsigned size) :
    m_connArea(connArea),
    m_name(name),
    m_size(size),
    m_parent(0),
    m_columns(1 + size),
    m_keys(1),		// indexed from 1
    m_tupleId(0),
    m_autoIncrement(0),
    m_sysId(DictSys::Undef),
    m_indexes(1)
{
}

inline unsigned
DictTable::getSize() const
{
    ctx_assert(m_columns.size() == 1 + m_size);
    return m_size;
}

inline void
DictTable::setParent(DictSchema* parent)
{
    m_parent = parent;
}

inline DictSchema*
DictTable::getParent() const
{
    return m_parent;
}

inline void
DictTable::setColumn(unsigned i, DictColumn* column)
{
    ctx_assert(1 <= i && i <= m_size);
    m_columns[i] = column;
    column->setPosition(i);
    column->setParent(this);
}

inline DictColumn*
DictTable::getColumn(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_size);
    ctx_assert(m_columns[i] != 0);
    return m_columns[i];
}

inline const BaseString&
DictTable::getName() const
{
    return m_name;
}

inline unsigned
DictTable::keyCount() const
{
    ctx_assert(m_keys.size() >= 1);
    return m_keys.size() - 1;
}

inline DictColumn*
DictTable::getKey(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_keys.size() && m_keys[i] != 0);
    return m_keys[i];
}

inline unsigned
DictTable::tupleId() const
{
    return m_tupleId;
}

inline unsigned
DictTable::autoIncrement() const
{
    return m_autoIncrement;
}

inline void
DictTable::sysId(DictSys::Id id)
{
    m_sysId = id;
}

inline DictSys::Id
DictTable::sysId() const
{
    return m_sysId;
}

inline void
DictTable::addIndex(DictIndex* index)
{
    m_indexes.push_back(index);
    index->setTable(this);
}

inline unsigned
DictTable::indexCount() const
{
    ctx_assert(m_indexes.size() >= 1);
    return m_indexes.size() - 1;
}

inline const DictIndex*
DictTable::getIndex(unsigned i) const
{
    ctx_assert(1 <= i && i < m_indexes.size() && m_indexes[i] != 0);
    return m_indexes[i];
}

#endif
