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
#include <codegen/Code_query_lookup.hpp>
#include <codegen/Code_table.hpp>

void
Exec_query_lookup::execImpl(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    data.m_done = false;
    Ndb* const ndb = ndbObject();
    NdbConnection* const tcon = ndbConnection();
    if (data.m_con != 0) {
	ndb->closeTransaction(data.m_con);
	data.m_con = 0;
	data.m_op = 0;
	ctx_log2(("lookup closed at re-execute"));
    }
    const bool unco = connArea().uncommitted();
    if (! unco) {
	// use new transaction to not run out of operations
	data.m_con = ndb->startTransaction();
	if (data.m_con == 0) {
	    ctx.pushStatus(ndb, "startTransaction");
	    return;
	}
    } else {
	ctx_log3(("lookup using main transaction"));
    }
    data.m_op = (unco ? tcon : data.m_con)->getNdbOperation(code.m_tableName);
    if (data.m_op == 0) {
	ctx.pushStatus(ndb, (unco ? tcon : data.m_con), 0, "getNdbOperation");
	return;
    }
    if (data.m_op->readTuple() == -1) {
	ctx.pushStatus(ndb, (unco ? tcon : data.m_con), data.m_op, "readTuple");
	return;
    }
    // key attributes
    for (unsigned k = 1; k <= code.m_keyCount; k++) {
	Exec_expr* exprMatch = code.m_keyMatch[k];
	ctx_assert(exprMatch != 0);
	exprMatch->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	const SqlField& keyMatch = exprMatch->getData().sqlField();
	SqlField f(code.m_keySpecs.getEntry(k));
	if (! keyMatch.cast(ctx, f)) {
	    data.m_done = true;		// match is not possible
	    return;
	}
	const NdbAttrId keyId = code.m_keyId[k];
	const void* addr = f.addr();
	const char* value = static_cast<const char*>(addr);
	if (data.m_op->equal(keyId, value) == -1) {
	    ctx.pushStatus(ndb, (unco ? tcon : data.m_con), data.m_op, "equal attrId=%u", (unsigned)keyId);
	    return;
	}
    }
    // queried attributes
    const SqlRow& sqlRow = data.sqlRow();
    ctx_assert(sqlRow.count() == code.m_attrCount);
    for (unsigned i = 1; i <= code.m_attrCount; i++) {
	const NdbAttrId attrId = code.m_attrId[i];
	SqlField& f = sqlRow.getEntry(i);
	char* addr = static_cast<char*>(f.addr());
	NdbRecAttr* recAttr = data.m_op->getValue(attrId, addr);
	if (recAttr == 0) {
	    ctx.pushStatus(ndb, (unco ? tcon : data.m_con), data.m_op, "getValue attrId=%u", (unsigned)attrId);
	    return;
	}
	data.m_recAttr[i] = recAttr;
    }
    if (code.m_attrCount == 0) {		// NDB requires one
	(void)data.m_op->getValue((NdbAttrId)0);
    }
    data.setCount(0);
    if ((unco ? tcon : data.m_con)->execute(unco ? NoCommit : Commit) == -1) {
	// XXX when did 626 move to connection level
	if ((unco ? tcon : data.m_con)->getNdbError().code != 626 && data.m_op->getNdbError().code != 626) {
	    ctx.pushStatus(ndb, (unco ? tcon : data.m_con), data.m_op, "execute xxx");
	    return;
	}
	data.m_done = true;
    } else {
	stmtArea().incTuplesFetched();
	data.m_done = false;
    }
    if (! unco) {
	ndb->closeTransaction(data.m_con);
	data.m_con = 0;
	data.m_op = 0;
	ctx_log3(("lookup closed at execute"));
    }
}

bool
Exec_query_lookup::fetchImpl(Ctx &ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    // returns at most one row
    if (data.m_done)
	return false;
    // set null bits
    const SqlRow& sqlRow = data.sqlRow();
    ctx_assert(sqlRow.count() == code.m_attrCount);
    for (unsigned i = 1; i <= code.m_attrCount; i++) {
	NdbRecAttr* recAttr = data.m_recAttr[i];
	int isNULL = recAttr->isNULL();
	SqlField& f = sqlRow.getEntry(i);
	ctx_assert(isNULL == 0 || isNULL == 1);
	f.sqlNull(isNULL == 1);
    }
    data.m_done = true;
    return true;
}
