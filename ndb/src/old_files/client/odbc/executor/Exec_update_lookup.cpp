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
#include <codegen/Code_expr.hpp>
#include <codegen/Code_update_lookup.hpp>
#include <codegen/Code_query.hpp>

void
Exec_update_lookup::execImpl(Ctx& ctx, Ctl& ctl)
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
    // update each row from the query
    while (m_query->fetch(ctx, ctl)) {
	NdbOperation* op = tcon->getNdbOperation(code.m_tableName);
	if (op == 0) {
	    ctx.pushStatus(ndb, tcon, 0, "getNdbOperation");
	    return;
	}
	if (op->updateTuple() == -1) {
	    ctx.pushStatus(ndb, tcon, op, "updateTuple");
	    return;
	}
	// key attributes
	bool done = false;
	for (unsigned k = 1; k <= code.m_keyCount; k++) {
	    Exec_expr* exprMatch = code.m_keyMatch[k];
	    ctx_assert(exprMatch != 0);
	    exprMatch->evaluate(ctx, ctl);
	    if (! ctx.ok())
		return;
	    const SqlField& keyMatch = exprMatch->getData().sqlField();
	    SqlField f(code.m_keySpecs.getEntry(k));
	    if (! keyMatch.cast(ctx, f)) {
		done = true;		// match is not possible
		break;
	    }
	    const NdbAttrId keyId = code.m_keyId[k];
	    const void* addr = f.addr();
	    const char* value = static_cast<const char*>(addr);
	    if (op->equal(keyId, value) == -1) {
		ctx.pushStatus(ndb, tcon, op, "equal attrId=%u", (unsigned)keyId);
		return;
	    }
	}
	if (done)
	    continue;
	// updated attributes
	const SqlRow& sqlRow = m_query->getData().sqlRow();
	ctx_assert(sqlRow.count() == code.m_attrCount);
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    const NdbAttrId attrId = code.m_attrId[i];
	    const SqlField& f = sqlRow.getEntry(i);
	    const void* addr = f.sqlNull() ? 0 : f.addr();
	    const char* value = static_cast<const char*>(addr);
	    if (op->setValue(attrId, value) == -1) {
		ctx.pushStatus(ndb, tcon, op, "setValue attrId=%u", (unsigned)attrId);
		return;
	    }
	}
	data.setCount(0);
	if (tcon->execute(NoCommit) == -1) {
	    // XXX when did 626 move to connection level
	    if (tcon->getNdbError().code != 626 && op->getNdbError().code != 626) {
		ctx.pushStatus(ndb, tcon, op, "execute without commit");
		return;
	    }
	} else {
	    data.addCount();
	}
    }
    stmtArea().setRowCount(ctx, data.getCount());
}
