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
#include <common/ConnArea.hpp>
#include "DictCatalog.hpp"
#include "DictSchema.hpp"
#include "DictTable.hpp"
#include "DictTable.hpp"
#include "DictColumn.hpp"
#include "DictIndex.hpp"
#include "DictSys.hpp"

DictSchema::~DictSchema()
{
    for (Tables::iterator i = m_tables.begin(); i != m_tables.end(); i++) {
	delete *i;
	*i = 0;
    }
}

DictTable*
DictSchema::findTable(const BaseString& name)
{
    for (Tables::iterator i = m_tables.begin(); i != m_tables.end(); i++) {
	DictTable* table = *i;
	ctx_assert(table != 0);
	if (strcmp(table->getName().c_str(), name.c_str()) == 0)
	    return table;
    }
    return 0;
}

void
DictSchema::deleteTable(Ctx& ctx, const BaseString& name)
{
    Tables::iterator i = m_tables.begin();
    while (i != m_tables.end()) {
	DictTable* table = *i;
	ctx_assert(table != 0);
	if (strcmp(table->getName().c_str(), name.c_str()) == 0) {
	    ctx_log2(("purge table %s from dictionary", name.c_str()));
	    delete table;
	    Tables::iterator j = i;
	    i++;
	    m_tables.erase(j);
	    break;
	}
	i++;
    }
}

void
DictSchema::deleteTableByIndex(Ctx& ctx, const BaseString& indexName)
{
    DictTable* foundTable = 0;
    for (Tables::iterator i = m_tables.begin(); i != m_tables.end(); i++) {
	DictTable* table = *i;
	ctx_assert(table != 0);
	for (unsigned k = 1; k <= table->indexCount(); k++) {
	    const DictIndex* index = table->getIndex(k);
	    if (strcmp(index->getName().c_str(), indexName.c_str()) == 0) {
		foundTable = table;
		break;
	    }
	}
	if (foundTable != 0)
	    break;
    }
    if (foundTable != 0)
	deleteTable(ctx, foundTable->getName());
}

DictTable*
DictSchema::loadTable(Ctx& ctx, const BaseString& name)
{
    ctx_log4(("%s: load from NDB", name.c_str()));
    Ndb* ndb = m_connArea.ndbObject();
    NdbDictionary::Dictionary* ndbDictionary = ndb->getDictionary();
    if (ndbDictionary == 0) {
	ctx.pushStatus(ndb, "getDictionary");
        return 0;
    }
    const NdbDictionary::Table* ndbTable = ndbDictionary->getTable(name.c_str());
    if (ndbTable == 0) {
	const NdbError& ndbError = ndbDictionary->getNdbError();
	if (ndbError.code == 709) {
	    // try built-in system table
	    DictTable* table = DictSys::loadTable(ctx, this, name);
	    if (table != 0) {
		return table;
	    }
	    ctx_log3(("%s: not found in NDB", name.c_str()));
	    return 0;
	}
	ctx.pushStatus(ndbDictionary->getNdbError(), "getTable");
	return 0;
    }
    int nattr = ndbTable->getNoOfColumns();
    DictTable* table = new DictTable(m_connArea, name, nattr);
    for (unsigned position = 1; position <= (unsigned)nattr; position++) {
	DictColumn* column = table->loadColumn(ctx, position);
	if (column == 0)
	    return 0;
	ctx_log4(("add column %u %s", column->getPosition(), column->getName().c_str()));
    }
    // load indexes
    NdbDictionary::Dictionary::List list;
    if (ndbDictionary->listIndexes(list, name.c_str()) == -1) {
	ctx.pushStatus(ndbDictionary->getNdbError(), "listIndexes");
	return 0;
    }
    for (unsigned i = 0; i < list.count; i++) {
	const NdbDictionary::Dictionary::List::Element& elt = list.elements[i];
	if (elt.state != NdbDictionary::Object::StateOnline) {
	    ctx_log1(("%s: skip broken index %s", name.c_str(), elt.name));
	    continue;
	}
	if (elt.type != NdbDictionary::Object::UniqueHashIndex && elt.type != NdbDictionary::Object::OrderedIndex) {
	    ctx_log1(("%s: skip unknown index type %s", name.c_str(), elt.name));
	    continue;
	}
	const NdbDictionary::Index* ndbIndex = ndbDictionary->getIndex(elt.name, name.c_str());
	if (ndbIndex == 0) {
	    ctx.pushStatus(ndbDictionary->getNdbError(), "table %s getIndex %s", name.c_str(), elt.name);
	    return 0;
	}
	DictIndex* index = new DictIndex(m_connArea, elt.name, elt.type, ndbIndex->getNoOfIndexColumns());
	for (unsigned j = 0; j < index->getSize(); j++) {
	    const char* cname = ndbIndex->getIndexColumn(j);
	    ctx_assert(cname != 0);
	    DictColumn* icolumn = table->findColumn(cname);
	    ctx_assert(icolumn != 0);
	    index->setColumn(1 + j, icolumn);
	}
	table->addIndex(index);
	ctx_log3(("%s: index %s: load from NDB done", name.c_str(), elt.name));
    }
    addTable(table);
    ctx_log3(("%s: load from NDB done", name.c_str()));
    return table;
}
