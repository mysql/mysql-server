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

#include <NdbApi.hpp>
#include <common/common.hpp>
#include <common/Ctx.hpp>
#include <common/ConnArea.hpp>
#include "DictSchema.hpp"
#include "DictTable.hpp"
#include "DictColumn.hpp"
#include "DictColumn.hpp"

DictTable::~DictTable()
{
    for (Columns::iterator i = m_columns.begin(); i != m_columns.end(); i++) {
	delete *i;
	*i = 0;
    }
    for (Indexes::iterator i = m_indexes.begin(); i != m_indexes.end(); i++) {
	delete *i;
	*i = 0;
    }
}

DictColumn*
DictTable::findColumn(const BaseString& name) const
{
    for (unsigned i = 1; i <= getSize(); i++) {
	DictColumn* column = m_columns[i];
	ctx_assert(column != 0);
	if (strcmp(column->getName().c_str(), name.c_str()) == 0)
	    return column;
    }
    return 0;
}

DictColumn*
DictTable::loadColumn(Ctx& ctx, unsigned position)
{
    Ndb* ndb = m_connArea.ndbObject();
    NdbDictionary::Dictionary* ndbDictionary = ndb->getDictionary();
    if (ndbDictionary == 0) {
	ctx.pushStatus(ndb, "getDictionary");
	return 0;
    }
    const NdbDictionary::Table* ndbTable = ndbDictionary->getTable(m_name.c_str());
    ctx_assert(ndbTable != 0);
    ctx_assert(position != 0);
    NdbAttrId attrId = position - 1;
    const NdbDictionary::Column* ndbColumn = ndbTable->getColumn(attrId);
    ctx_assert(ndbColumn != 0);
    SqlType sqlType(ctx, ndbColumn);
    if (! ctx.ok())
	return 0;
    DictColumn* column = new DictColumn(m_connArea, ndbColumn->getName(), sqlType);
    setColumn(position, column);
    column->m_key = column->m_tupleId = false;
    if (ndbColumn->getPrimaryKey())
	column->m_key = true;
    if (ndbColumn->getTupleKey())
	column->m_key = column->m_tupleId = true;
    if (column->m_key)
	m_keys.push_back(column);
    // props
    const char* value;
    column->m_autoIncrement = false;
    if (ndbColumn->getAutoIncrement())
	column->m_autoIncrement = true;
    column->m_defaultValue = 0;
    if ((value = ndbColumn->getDefaultValue()) != 0 && strlen(value) != 0)
	column->m_defaultValue = strcpy(new char[strlen(value) + 1], value);
    ctx_log4(("column %u %s keyFlag=%d idFlag=%d", position, ndbColumn->getName(), column->m_key, column->m_tupleId));
    if (column->m_tupleId)
	m_tupleId = position;
    if (column->m_autoIncrement)
	m_autoIncrement = position;
    return column;
}
