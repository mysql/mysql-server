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
#include <codegen/Code_delete_scan.hpp>
#include <codegen/Code_query.hpp>

void
Exec_delete_scan::execImpl(Ctx& ctx, Ctl& ctl)
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
    // delete each row from the scan
    data.setCount(0);
    while (m_query->fetch(ctx, ctl)) {
	NdbOperation* toOp = ctl.m_scanOp->takeOverForDelete(tcon);
	if (toOp == 0) {
	    ctx.pushStatus(ndb, tcon, ctl.m_scanOp, "takeOverScanOp");
	    return;
	}
	if (tcon->execute(NoCommit) == -1) {
	    if (toOp->getNdbError().code != 626) {
		ctx.pushStatus(ndb, tcon, toOp, "execute without commit");
		return;
	    }
	} else {
	    data.addCount();
	}
    }
    stmtArea().setRowCount(ctx, data.getCount());
}
