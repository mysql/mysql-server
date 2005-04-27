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
#include <common/StmtArea.hpp>
#include <dictionary/DictSchema.hpp>
#include <dictionary/DictColumn.hpp>
#include "Code_column.hpp"
#include "Code_table_list.hpp"
#include "Code_table.hpp"

// Plan_column

Plan_column::~Plan_column()
{
}

void
Plan_column::analyzeColumn(Ctx& ctx, Plan_base::Ctl& ctl)
{
    if (m_resTable != 0)	// done on previous pass
	return;
    if (! (ctl.m_tableList.size() > 1)) {
	ctx.pushStatus(Sqlstate::_42000, Error::Gen, "column %s not allowed here", getPrintName());
	return;
    }
    unsigned resCount = 0;
    for (unsigned i = 1; i < ctl.m_tableList.size(); i++) {
	Plan_table* table = ctl.m_tableList[i];
	ctx_assert(table != 0);
	int ret = table->resolveColumn(ctx, this);
	if (ret < 0)
	    return;
	if (ret)
	    resCount++;
    }
    if (resCount == 0) {
	// XXX try to strip "schema name" from table name
	for (unsigned i = 1; i < ctl.m_tableList.size(); i++) {
	    Plan_table* table = ctl.m_tableList[i];
	    ctx_assert(table != 0);
	    int ret = table->resolveColumn(ctx, this, true);
	    if (ret < 0)
		return;
	    if (ret)
		resCount++;
	}
    }
    if (resCount == 0) {
	ctx.pushStatus(Sqlstate::_42S22, Error::Gen, "column %s not found", getPrintName());
	return;
    }
    if (resCount > 1) {
	ctx.pushStatus(Error::Gen, "column %s is ambiguous", getPrintName());
	return;
    }
    // copy SQL type
    m_sqlType = dictColumn().sqlType();
}
