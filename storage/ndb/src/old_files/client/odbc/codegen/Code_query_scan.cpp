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
#include <dictionary/DictTable.hpp>
#include <dictionary/DictColumn.hpp>
#include "Code_query_scan.hpp"
#include "Code_column.hpp"
#include "Code_root.hpp"

// Plan_query_scan

Plan_query_scan::~Plan_query_scan()
{
}

Plan_base*
Plan_query_scan::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    if (m_interp != 0) {
	m_interp = static_cast<Plan_pred*>(m_interp->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(m_interp != 0);
    }
    return this;
}

Exec_base*
Plan_query_scan::codegen(Ctx& ctx, Ctl& ctl)
{
    // set up
    ctx_assert(m_table != 0);
    const BaseString& tableName = m_table->getName();
    const DictTable& dictTable = m_table->dictTable();
    const ColumnVector& columns = m_table->exprColumns();
    ctx_assert(columns.size() > 0);
    const unsigned attrCount = columns.size() - 1;
    // create the code
    Exec_query_scan::Code& code = *new Exec_query_scan::Code(attrCount);
    code.m_tableName = strcpy(new char[tableName.length() + 1], tableName.c_str());
    code.m_exclusive = m_exclusive;
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
    Exec_query_scan* exec = new Exec_query_scan(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setCode(code);
    // interpreter
    Exec_pred* execInterp = 0;
    ctl.m_execQuery = exec;
    ctl.m_topTable = m_table;
    if (m_interp != 0) {
	execInterp = static_cast<Exec_pred*>(m_interp->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execInterp != 0);
    }
    ctl.m_topTable = 0;
    if (m_interp != 0)
	exec->setInterp(execInterp);
    return exec;
}

void
Plan_query_scan::print(Ctx& ctx)
{
    ctx.print(" [query_scan");
    Plan_base* a[] = { m_table, m_interp };
    printList(ctx, a, 2);
    ctx.print("]");
}

// Exec_query_scan

Exec_query_scan::Code::~Code()
{
    delete[] m_tableName;
    delete[] m_attrId;
}

Exec_query_scan::Data::~Data()
{
    delete[] m_recAttr;
}

Exec_query_scan::~Exec_query_scan()
{
}

void
Exec_query_scan::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // create data
    Data& data = *new Data(this, code.sqlSpecs());
    // needed for isNULL
    data.m_recAttr = new NdbRecAttr* [1 + code.m_attrCount];
    for (unsigned i = 0; i <= code.m_attrCount; i++) {
	data.m_recAttr[i] = 0;
    }
    data.m_parallel = code.m_exclusive ? 1 : 240;	// best supported
    setData(data);
    // interpreter
    ctl.m_query = this;
    if (m_interp != 0) {
	//m_interp->alloc(ctx, ctl); XXX
	if (! ctx.ok())
	    return;
    }
}

void
Exec_query_scan::close(Ctx& ctx)
{
    Data& data = getData();
    if (data.m_con != 0) {
	Ndb* const ndb = ndbObject();
	int ret = data.m_con->stopScan();
	if (ret == -1) {
	    ctx.pushStatus(ndb, data.m_con, data.m_op, "stopScan");
	}
	ndb->closeTransaction(data.m_con);
	data.m_con = 0;
	data.m_op = 0;
	ctx_log2(("scan closed at statement close"));
    }
    if (m_interp != 0)
	m_interp->close(ctx);
}

void
Exec_query_scan::print(Ctx& ctx)
{
    ctx.print(" [query_scan");
    if (m_code != 0) {
	const Code& code = getCode();
	ctx.print(" attrId=");
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    if (i > 1)
		ctx.print(",");
	    ctx.print("%u", (unsigned)code.m_attrId[i]);
	}
	ctx.print(" table=%s", code.m_tableName);
    }
    if (m_interp != 0)
	m_interp->print(ctx);
    ctx.print("]");
}
