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

#ifndef ODBC_DICTIONARY_DictCatalog_hpp
#define ODBC_DICTIONARY_DictCatalog_hpp

#include <list>
#include <common/common.hpp>
#include "DictSchema.hpp"

class Ctx;
class ConnArea;
class DictSchema;

/**
 * @class DictCatalog
 * @brief Collection of schemas
 */
class DictCatalog {
public:
    DictCatalog(const ConnArea& connArea);
    ~DictCatalog();
    const ConnArea& connArea() const;
    DictSchema* findSchema(Ctx& ctx, const BaseString& name);
    void addSchema(DictSchema* schema);
protected:
    const ConnArea& m_connArea;
    typedef std::list<DictSchema*> Schemas;
    Schemas m_schemas;
};

inline
DictCatalog::DictCatalog(const ConnArea& connArea) :
    m_connArea(connArea)
{
}

inline const ConnArea&
DictCatalog::connArea() const
{
    return m_connArea;
}

inline void
DictCatalog::addSchema(DictSchema* schema)
{
    m_schemas.push_back(schema);
    schema->setParent(this);
}

#endif
