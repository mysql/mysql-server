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

#ifndef ODBC_DICTIONARY_DictSchema_hpp
#define ODBC_DICTIONARY_DictSchema_hpp

#include <list>
#include <common/common.hpp>
#include "DictTable.hpp"

class Ctx;
class ConnArea;
class DictCatalog;
class DictTable;

/**
 * @class DictSchema
 * @brief Collection of tables
 */
class DictSchema {
public:
    DictSchema(const ConnArea& connArea, const BaseString& name);
    ~DictSchema();
    const BaseString& getName() const;
    void setParent(DictCatalog* parent);
    DictCatalog* getParent() const;
    void addTable(DictTable* table);
    DictTable* findTable(const BaseString& name);
    DictTable* loadTable(Ctx& ctx, const BaseString& name);
    void deleteTable(Ctx& ctx, const BaseString& name);
    void deleteTableByIndex(Ctx& ctx, const BaseString& indexName);
protected:
    friend class DictCatalog;
    friend class DictSys;
    const ConnArea& m_connArea;
    BaseString m_name;
    DictCatalog* m_parent;
    typedef std::list<DictTable*> Tables;
    Tables m_tables;
};

inline
DictSchema::DictSchema(const ConnArea& connArea, const BaseString& name) :
    m_connArea(connArea),
    m_name(name),
    m_parent(0)
{
    ctx_assert(strcmp(name.c_str(), "NDB") == 0);
}

inline const BaseString&
DictSchema::getName() const
{
    return m_name;
}

inline void
DictSchema::setParent(DictCatalog* parent)
{
    m_parent = parent;
}

inline DictCatalog*
DictSchema::getParent() const
{
    return m_parent;
}

inline void
DictSchema::addTable(DictTable* table)
{
    m_tables.push_back(table);
    table->setParent(this);
}

#endif
