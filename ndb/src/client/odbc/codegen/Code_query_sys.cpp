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
#include "Code_query_sys.hpp"
#include "Code_column.hpp"
#include "Code_root.hpp"

// Plan_query_sys

Plan_query_sys::~Plan_query_sys()
{
}

Plan_base*
Plan_query_sys::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_query_sys::codegen(Ctx& ctx, Ctl& ctl)
{
    // set up
    ctx_assert(m_table != 0);
    const DictTable& dictTable = m_table->dictTable();
    const ColumnVector& columns = m_table->exprColumns();
    ctx_assert(columns.size() > 0);
    const unsigned attrCount = columns.size() - 1;
    // create the code
    Exec_query_sys::Code& code = *new Exec_query_sys::Code(attrCount);
    code.m_sysId = dictTable.sysId();
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
    Exec_query_sys* exec = new Exec_query_sys(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setCode(code);
    return exec;
}

void
Plan_query_sys::print(Ctx& ctx)
{
    ctx.print(" [query_sys");
    Plan_base* a[] = { m_table };
    printList(ctx, a, 1);
    ctx.print("]");
}

// Exec_query_sys

Exec_query_sys::Code::~Code()
{
    delete[] m_attrId;
}

Exec_query_sys::Data::~Data()
{
}

Exec_query_sys::~Exec_query_sys()
{
}

void
Exec_query_sys::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // create data
    Data& data = *new Data(this, code.sqlSpecs());
    setData(data);
}

void
Exec_query_sys::close(Ctx& ctx)
{
    Data& data = getData();
    data.m_rowPos = 0;
    data.m_tablePos = 0;
    data.m_attrPos = 0;
    data.m_keyPos = 0;
}

void
Exec_query_sys::print(Ctx& ctx)
{
    ctx.print(" [query_sys");
    if (m_code != 0) {
	const Code& code = getCode();
	ctx.print(" attrId=");
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    if (i > 1)
		ctx.print(",");
	    ctx.print("%u", (unsigned)code.m_attrId[i]);
	}
	ctx.print(" sysId=%u", (unsigned)code.m_sysId);
    }
    ctx.print("]");
}
