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
#include <codegen/Code_query_range.hpp>
#include <codegen/Code_table.hpp>

#define startBuddyTransaction(x)	hupp(x)

void
Exec_query_range::execImpl(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    data.m_done = false;
    Ndb* const ndb = ndbObject();
    NdbConnection* const tcon = ndbConnection();
    if (data.m_con != 0) {
	data.m_con->stopScan();
	ndb->closeTransaction(data.m_con);
	data.m_con = 0;
	data.m_op = 0;
	ctx_log2(("range scan closed at re-execute"));
    }
    data.m_con = ndb->startBuddyTransaction(tcon);
    if (data.m_con == 0) {
	ctx.pushStatus(ndb, tcon, 0, "startBuddyTransaction");
	return;
    }
    data.m_op = data.m_con->getNdbOperation(code.m_indexName, code.m_tableName);
    if (data.m_op == 0) {
	ctx.pushStatus(ndb, data.m_con, 0, "getNdbOperation");
	return;
    }
    if (! code.m_exclusive) {
	if (data.m_op->openScanReadCommitted(data.m_parallel) == -1) {
	    ctx.pushStatus(ndb, data.m_con, data.m_op, "openScanReadCommitted");
	    return;
	}
    } else {
	if (data.m_op->openScanExclusive(data.m_parallel) == -1) {
	    ctx.pushStatus(ndb, data.m_con, data.m_op, "openScanExclusive");
	    return;
	}
    }
    // set bounds
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
	const unsigned len = f.allocSize();
	if (data.m_op->setBound(keyId, NdbOperation::BoundEQ, value, len) == -1) {
	    ctx.pushStatus(ndb, data.m_con, data.m_op, "setBound attrId=%u", (unsigned)keyId);
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
	    ctx.pushStatus(ndb, data.m_con, data.m_op, "getValue attrId=%u", (unsigned)attrId);
	    return;
	}
	data.m_recAttr[i] = recAttr;
    }
    if (code.m_attrCount == 0) {		// NDB requires one
	(void)data.m_op->getValue((NdbAttrId)0);
    }
    data.setCount(0);
    if (data.m_con->executeScan() == -1) {
	ctx.pushStatus(ndb, data.m_con, data.m_op, "executeScan");
	return;
    }
    ctx_log2(("range scan %s [%08x] started", ! code.m_exclusive ? "read" : "exclusive", (unsigned)this));
    ctl.m_scanOp = data.m_op;
}

bool
Exec_query_range::fetchImpl(Ctx &ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    Ndb* const ndb = ndbObject();
    // if never started
    if (data.m_done)
	return false;
    int ret = data.m_con->nextScanResult();
    if (ret != 0) {
	if (ret == -1) {
	    ctx.pushStatus(ndb, data.m_con, data.m_op, "nextScanResult");
	}
	data.m_con->stopScan();
	ndb->closeTransaction(data.m_con);
	data.m_con = 0;
	data.m_op = 0;
	ctx_log2(("range scan [%08x] closed at last row", (unsigned)this));
	return false;
    }
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
    stmtArea().incTuplesFetched();
    return true;
}
