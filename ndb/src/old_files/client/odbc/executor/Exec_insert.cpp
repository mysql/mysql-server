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
#include <codegen/Code_insert.hpp>
#include <codegen/Code_query.hpp>

#ifdef NDB_WIN32
#define FMT_I64		"%I64d"
#else
#define FMT_I64		"%lld"
#endif

void
Exec_insert::execImpl(Ctx& ctx, Ctl& ctl)
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
    // insert each row from query
    data.setCount(0);
    while (m_query->fetch(ctx, ctl)) {
	NdbOperation* op = tcon->getNdbOperation(code.m_tableName);
	if (op == 0) {
	    ctx.pushStatus(ndb, tcon, 0, "getNdbOperation");
	    return;
	}
	if (code.m_insertOp == Insert_op_insert) {
	    if (op->insertTuple() == -1) {
		ctx.pushStatus(ndb, tcon, op, "insertTuple");
		return;
	    }
	} else if (code.m_insertOp == Insert_op_write) {
	    if (op->writeTuple() == -1) {
		ctx.pushStatus(ndb, tcon, op, "writeTuple");
		return;
	    }
	} else {
	    ctx_assert(false);
	}
	const SqlRow& sqlRow = m_query->getData().sqlRow();
	if (code.m_tupleId != 0) {
	    Uint64 tid = op->setTupleId();
	    if (tid == 0) {
		ctx.pushStatus(ndb, tcon, op, "setTupleId attrId=%u", (unsigned)code.m_tupleId);
		return;
	    }
	    ctx_log3(("generated TupleId " FMT_I64, tid));
	} else if (code.m_autoIncrement != 0) {
	    // XXX use cache size 1 for birdies and fishies
	    Uint64 tupleId = ndb->getAutoIncrementValue(code.m_tableName, 1);
	    if (tupleId == 0) {
		ctx.pushStatus(ndb, "getTupleIdFromNdb");
		return;
	    }
	    NdbAttrId attrId = code.m_autoIncrement - 1;
	    SqlSmallint sqlSmallint = 0;
	    SqlInteger sqlInteger = 0;
	    SqlBigint sqlBigint = 0;
	    const char* value = 0;
	    if (code.m_idType.type() == SqlType::Smallint) {
		sqlSmallint = tupleId;
		value = (const char*)&sqlSmallint;
	    } else if (code.m_idType.type() == SqlType::Integer) {
		sqlInteger = tupleId;
		value = (const char*)&sqlInteger;
	    } else if (code.m_idType.type() == SqlType::Bigint) {
		sqlBigint = tupleId;
		value = (const char*)&sqlBigint;
	    } else {
		ctx_assert(false);
	    }
	    if (op->equal(attrId, value) == -1) {
		ctx.pushStatus(ndb, tcon, op, "equal attrId=%u", (unsigned)attrId);
		return;
	    }
	} else {
	    // normal key attributes
	    for (unsigned i = 1; i <= sqlRow.count(); i++) {
		if (! code.m_isKey[i])
		    continue;
		NdbAttrId attrId = code.m_attrId[i];
		const SqlField& f = sqlRow.getEntry(i);
		const void* addr = f.sqlNull() ? 0 : f.addr();
		const char* value = static_cast<const char*>(addr);
		if (op->equal(attrId, value) == -1) {
		    ctx.pushStatus(ndb, tcon, op, "equal attrId=%u", (unsigned)attrId);
		    return;
		}
	    }
	}
	// non-key attributes
	for (unsigned i = 1; i <= sqlRow.count(); i++) {
	    if (code.m_isKey[i])
		continue;
	    NdbAttrId attrId = code.m_attrId[i];
	    const SqlField& f = sqlRow.getEntry(i);
	    const void* addr = f.sqlNull() ? 0 : f.addr();
	    const char* value = static_cast<const char*>(addr);
	    if (op->setValue(attrId, value) == -1) {
		ctx.pushStatus(ndb, tcon, op, "setValue attrId=%u", (unsigned)attrId);
		return;
	    }
	}
	// default non-key values
	for (unsigned i = 1; i <= code.m_defaultCount; i++) {
	    NdbAttrId attrId = code.m_defaultId[i];
	    const SqlField& f = code.m_defaultValue->getEntry(i);
	    const void* addr = f.sqlNull() ? 0 : f.addr();
	    const char* value = static_cast<const char*>(addr);
	    if (op->setValue(attrId, value) == -1) {
		ctx.pushStatus(ndb, tcon, op, "setValue attrId=%u", (unsigned)attrId);
		return;
	    }
	}
	if (tcon->execute(NoCommit) == -1) {
	    ctx.pushStatus(ndb, tcon, op, "execute without commit");
	    return;
	}
	data.addCount();
    }
    stmtArea().setRowCount(ctx, data.getCount());
}
