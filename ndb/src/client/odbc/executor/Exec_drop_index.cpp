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
#include <dictionary/DictSchema.hpp>
#include <codegen/Code_drop_index.hpp>

void
Exec_drop_index::execute(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    Ndb* const ndb = ndbObject();
    NdbDictionary::Dictionary* ndbDictionary = ndb->getDictionary();
    if (ndbDictionary == 0) {
	ctx.pushStatus(ndb, "getDictionary");
	return;
    }
    dictSchema().deleteTableByIndex(ctx, code.m_indexName);
    if (ndbDictionary->dropIndex(code.m_indexName.c_str(), code.m_tableName.c_str()) == -1) {
	ctx.pushStatus(ndbDictionary->getNdbError(), "dropIndex %s on %s", code.m_indexName.c_str(), code.m_tableName.c_str());
	return;
    }
    ctx_log1(("index %s on %s dropped", code.m_indexName.c_str(), code.m_tableName.c_str()));
}
