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
#include <common/ResultArea.hpp>
#include <codegen/Code_update_scan.hpp>
#include <codegen/Code_query.hpp>

void
Exec_update_scan::execImpl(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    Ndb* const ndb = ndbObject();
    NdbConnection* const tcon = ndbConnection();
    // execute subquery
    ctx_assert(m_query != 0);
    m_query->execute(ctx, ctl);
    if (! ctx.ok())
	return;
    ctx_assert(ctl.m_scanOp != 0);
    // update each row from query
    data.setCount(0);
    while (m_query->fetch(ctx, ctl)) {
	NdbOperation* toOp = ctl.m_scanOp->takeOverForUpdate(tcon);
	if (toOp == 0) {
	    ctx.pushStatus(ndb, tcon, ctl.m_scanOp, "takeOverScanOp");
	    return;
	}
	const SqlRow& sqlRow = m_query->getData().sqlRow();
	for (unsigned i = 1; i <= sqlRow.count(); i++) {
	    const SqlField& f = sqlRow.getEntry(i);
	    const void* addr = f.sqlNull() ? 0 : f.addr();
	    const char* value = static_cast<const char*>(addr);
	    if (toOp->setValue(code.m_attrId[i], value) == -1) {
		ctx.pushStatus(ndb, tcon, toOp, "setValue");
		return;
	    }
	}
	if (tcon->execute(NoCommit) == -1) {
	    ctx.pushStatus(ndb, tcon, toOp, "execute without commit");
	    return;
	}
	data.addCount();
    }
    stmtArea().setRowCount(ctx, data.getCount());
}
