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
#include "Code_update_scan.hpp"
#include "Code_table.hpp"
#include "Code_root.hpp"

// Plan_update_scan

Plan_update_scan::~Plan_update_scan()
{
}

Plan_base*
Plan_update_scan::analyze(Ctx& ctx, Ctl& ctl)
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
Plan_update_scan::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "UPDATE WHERE", SQL_DIAG_UPDATE_WHERE);
}

Exec_base*
Plan_update_scan::codegen(Ctx& ctx, Ctl& ctl)
{   
    // generate code for the query
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // set up
    ctx_assert(m_table != 0);
    const ColumnVector& columns = m_table->dmlColumns();
    ctx_assert(columns.size() > 0);
    const unsigned attrCount = columns.size() - 1;
    // create the code
    Exec_update_scan::Code& code = *new Exec_update_scan::Code();
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
    Exec_update_scan* exec = new Exec_update_scan(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setCode(code);
    exec->setQuery(execQuery);
    return exec;
}    

void
Plan_update_scan::print(Ctx& ctx)
{
    ctx.print(" [update_scan");
    Plan_base* a[] = { m_table, m_query };
    printList(ctx, a, sizeof(a)/sizeof(a[0]));
    ctx.print("]");
}

// Exec_delete

Exec_update_scan::Code::~Code()
{
    delete[] m_attrId;
}

Exec_update_scan::Data::~Data()
{
}

Exec_update_scan::~Exec_update_scan()
{
}

void
Exec_update_scan::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate the subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
        return;
    // create data
    Data& data = *new Data;
    setData(data);
}

void
Exec_update_scan::close(Ctx& ctx)
{
    ctx_assert(m_query != 0);
    m_query->close(ctx);
}

void
Exec_update_scan::print(Ctx& ctx)
{
    ctx.print(" [update_scan");
    if (m_code != 0) {
	const Code& code = getCode();
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
