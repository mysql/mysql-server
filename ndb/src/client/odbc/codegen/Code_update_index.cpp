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
#include "Code_dml_column.hpp"
#include "Code_expr.hpp"
#include "Code_update_index.hpp"
#include "Code_table.hpp"
#include "Code_root.hpp"

// Plan_update_index

Plan_update_index::~Plan_update_index()
{
}

Plan_base*
Plan_update_index::analyze(Ctx& ctx, Ctl& ctl)
{
    ctl.m_dmlRow = m_dmlRow;	// row type to convert to
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

void
Plan_update_index::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "UPDATE WHERE", SQL_DIAG_UPDATE_WHERE);
}

Exec_base*
Plan_update_index::codegen(Ctx& ctx, Ctl& ctl)
{   
    // generate code for the query
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // set up
    ctx_assert(m_table != 0 && m_index != 0);
    const BaseString& tableName = m_table->getName();
    ctx_assert(m_index->m_dictIndex != 0);
    const DictIndex& dictIndex = *m_index->m_dictIndex;
    const BaseString& indexName = dictIndex.getName();
    const unsigned keyCount = m_index->m_keyCount;
    const ColumnVector& columns = m_table->dmlColumns();
    ctx_assert(columns.size() > 0);
    const unsigned attrCount = columns.size() - 1;
    // create the code
    Exec_update_index::Code& code = *new Exec_update_index::Code(keyCount);
    code.m_tableName = strcpy(new char[tableName.length() + 1], tableName.c_str());
    code.m_indexName = strcpy(new char[indexName.length() + 1], indexName.c_str());
    // key attributes
    code.m_keyId = new NdbAttrId[1 + keyCount];
    code.m_keyId[0] = (NdbAttrId)-1;
    for (unsigned k = 1; k <= keyCount; k++) {
	const DictColumn* keyColumn = dictIndex.getColumn(k);
	const SqlType& sqlType = keyColumn->sqlType();
	SqlSpec sqlSpec(sqlType, SqlSpec::Physical);
	code.m_keySpecs.setEntry(k, sqlSpec);
	code.m_keyId[k] = k - 1;	// index column order
    }
    // matching expressions
    ctx_assert(m_index->m_keyFound);
    const ExprVector& keyEq = m_index->m_keyEq;
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
    // updated attributes
    code.m_attrCount = attrCount;
    code.m_attrId = new NdbAttrId[1 + attrCount];
    code.m_attrId[0] = (NdbAttrId)-1;
    for (unsigned i = 1; i <= attrCount; i++) {
	Plan_column* column = columns[i];
	ctx_assert(column != 0);
	const DictColumn& dictColumn = column->dictColumn();
	code.m_attrId[i] = dictColumn.getAttrId();
    }
    // create the exec
    Exec_update_index* exec = new Exec_update_index(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setCode(code);
    exec->setQuery(execQuery);
    return exec;
}    

void
Plan_update_index::print(Ctx& ctx)
{
    ctx.print(" [update_index");
    Plan_base* a[] = { m_table, m_query };
    printList(ctx, a, sizeof(a)/sizeof(a[0]));
    ctx.print("]");
}

// Exec_delete

Exec_update_index::Code::~Code()
{
    delete[] m_tableName;
    delete[] m_keyId;
    delete[] m_keyMatch;
    delete[] m_attrId;
}

Exec_update_index::Data::~Data()
{
}

Exec_update_index::~Exec_update_index()
{
}

void
Exec_update_index::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // allocate the subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
        return;
    // create data
    Data& data = *new Data;
    setData(data);
    // allocate matching expressions
    for (unsigned k = 1; k <= code.m_keyCount; k++) {
	Exec_expr* expr = code.m_keyMatch[k];
	ctx_assert(expr != 0);
	expr->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
}

void
Exec_update_index::close(Ctx& ctx)
{
    ctx_assert(m_query != 0);
    m_query->close(ctx);
}

void
Exec_update_index::print(Ctx& ctx)
{
    ctx.print(" [update_index");
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
    }
    Exec_base* a[] = { m_query };
    printList(ctx, a, 1);
    ctx.print("]");
}
