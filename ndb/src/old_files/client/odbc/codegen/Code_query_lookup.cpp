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

#include <common/StmtArea.hpp>
#include <dictionary/DictTable.hpp>
#include <dictionary/DictColumn.hpp>
#include "Code_query_lookup.hpp"
#include "Code_column.hpp"
#include "Code_expr.hpp"
#include "Code_root.hpp"

// Plan_query_lookup

Plan_query_lookup::~Plan_query_lookup()
{
}

Plan_base*
Plan_query_lookup::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_query_lookup::codegen(Ctx& ctx, Ctl& ctl)
{
    // set up
    ctx_assert(m_table != 0);
    const BaseString& tableName = m_table->getName();
    const DictTable& dictTable = m_table->dictTable();
    const unsigned keyCount = dictTable.keyCount();
    const ColumnVector& columns = m_table->exprColumns();
    ctx_assert(columns.size() > 0);
    const unsigned attrCount = columns.size() - 1;
    // create the code
    Exec_query_lookup::Code& code = *new Exec_query_lookup::Code(keyCount, attrCount);
    code.m_tableName = strcpy(new char[tableName.length() + 1], tableName.c_str());
    // key attributes
    code.m_keyId = new NdbAttrId[1 + keyCount];
    code.m_keyId[0] = (NdbAttrId)-1;
    for (unsigned k = 1; k <= keyCount; k++) {
	const DictColumn* keyColumn = dictTable.getKey(k);
	const SqlType& sqlType = keyColumn->sqlType();
	SqlSpec sqlSpec(sqlType, SqlSpec::Physical);
	code.m_keySpecs.setEntry(k, sqlSpec);
	code.m_keyId[k] = keyColumn->getAttrId();
    }
    // matching expressions
    const Plan_table::Index& index = m_table->m_indexList[0];
    ctx_assert(index.m_keyFound);
    const ExprVector& keyEq = index.m_keyEq;
    ctx_assert(keyEq.size() == 1 + keyCount);
    code.m_keyMatch = new Exec_expr* [1 + keyCount];
    code.m_keyMatch[0] = 0;
    for (unsigned k = 1; k <= keyCount; k++) {
	Plan_expr* expr = keyEq[k];
	Exec_expr* execExpr = static_cast<Exec_expr*>(expr->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execExpr != 0);
	code.m_keyMatch[k] = execExpr;
    }
    // queried attributes
    code.m_attrId = new NdbAttrId[1 + attrCount];
    code.m_attrId[0] = (NdbAttrId)-1;
    for (unsigned i = 1; i <= attrCount; i++) {
	Plan_column* column = columns[i];
	ctx_assert(column != 0);
	const DictColumn& dictColumn = column->dictColumn();
	const SqlType& sqlType = dictColumn.sqlType();
	SqlSpec sqlSpec(sqlType, SqlSpec::Physical);
	code.m_sqlSpecs.setEntry(i, sqlSpec);
	code.m_attrId[i] = dictColumn.getAttrId();
    }
    // create the exec
    Exec_query_lookup* exec = new Exec_query_lookup(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setCode(code);
    return exec;
}

void
Plan_query_lookup::print(Ctx& ctx)
{
    ctx.print(" [query_lookup");
    Plan_base* a[] = { m_table };
    printList(ctx, a, 1);
    ctx.print("]");
}

// Exec_query_lookup

Exec_query_lookup::Code::~Code()
{
    delete[] m_tableName;
    delete[] m_keyId;
    delete[] m_keyMatch;
    delete[] m_attrId;
}

Exec_query_lookup::Data::~Data()
{
    delete[] m_recAttr;
}

Exec_query_lookup::~Exec_query_lookup()
{
}

void
Exec_query_lookup::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // create data
    Data& data = *new Data(this, code.sqlSpecs());
    setData(data);
    // allocate matching expressions
    for (unsigned k = 1; k <= code.m_keyCount; k++) {
	Exec_expr* expr = code.m_keyMatch[k];
	ctx_assert(expr != 0);
	expr->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
    // needed for isNULL
    data.m_recAttr = new NdbRecAttr* [1 + code.m_attrCount];
    for (unsigned i = 0; i <= code.m_attrCount; i++) {
	data.m_recAttr[i] = 0;
    }
}

void
Exec_query_lookup::close(Ctx& ctx)
{
    Data& data = getData();
    if (data.m_con != 0) {
	Ndb* const ndb = ndbObject();
	ndb->closeTransaction(data.m_con);
	data.m_con = 0;
	data.m_op = 0;
	data.m_done = true;
	ctx_log2(("lookup closed at statement close"));
    }
}

void
Exec_query_lookup::print(Ctx& ctx)
{
    ctx.print(" [query_lookup");
    if (m_code != 0) {
	const Code& code = getCode();
	ctx.print(" keyId=");
	for (unsigned i = 1; i <= code.m_keyCount; i++) {
	    if (i > 1)
		ctx.print(",");
	    ctx.print("%u", (unsigned)code.m_keyId[i]);
	}
	ctx.print(" attrId=");
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    if (i > 1)
		ctx.print(",");
	    ctx.print("%u", (unsigned)code.m_attrId[i]);
	}
	ctx.print(" table=%s", code.m_tableName);
    }
    ctx.print("]");
}
