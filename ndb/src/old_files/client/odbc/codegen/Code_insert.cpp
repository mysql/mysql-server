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
#include "Code_insert.hpp"
#include "Code_query_repeat.hpp"
#include "Code_query_project.hpp"
#include "Code_table.hpp"
#include "Code_root.hpp"

// Plan_insert

Plan_insert::~Plan_insert()
{
}

Plan_base*
Plan_insert::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_insert);
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    // handle MySql syntax
    if (m_mysqlRow != 0) {
	setDmlRow(m_mysqlRow->m_dmlRow);
	setExprRow(m_mysqlRow->m_exprRow);
	m_mysqlRow = 0;
    }
    if (m_dmlRow == 0) {
	// construct column list
	setDmlRow(new Plan_dml_row(m_root));
	m_root->saveNode(m_dmlRow);
	const DictTable& dictTable = m_table->dictTable();
	unsigned n = dictTable.getSize();
	for (unsigned i = 1; i <= n; i++) {
	    DictColumn* dictColumn = dictTable.getColumn(i);
	    Plan_dml_column* column = new Plan_dml_column(m_root, dictColumn->getName());
	    m_root->saveNode(column);
	    m_dmlRow->addColumn(column);
	}
    }
    // set name resolution scope
    ctl.m_tableList.resize(1 + 1);	// indexed from 1
    ctl.m_tableList[1] = m_table;
    // analyze the dml columns
    m_dmlRow->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctl.m_dmlRow = m_dmlRow;	// row type to convert to
    if (m_query != 0) {
	m_query->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    } else if (m_select == 0) {
	// analyze the expression row
	m_exprRow->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
	// transform the row into query
	Plan_query_repeat* queryRepeat = new Plan_query_repeat(m_root, 1);
	m_root->saveNode(queryRepeat);
	Plan_query_project* queryProject = new Plan_query_project(m_root);
	m_root->saveNode(queryProject);
	queryProject->setQuery(queryRepeat);
	queryProject->setRow(m_exprRow);
	setQuery(queryProject);
    } else {
	// analyze the select into query
	Plan_query* query = static_cast<Plan_query*>(m_select->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	setQuery(query);
    }
    return this;
}

void
Plan_insert::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "INSERT", SQL_DIAG_INSERT);
}

Exec_base*
Plan_insert::codegen(Ctx& ctx, Ctl& ctl)
{
    // create code for the query
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // set up
    ctx_assert(m_table != 0);
    const BaseString& tableName = m_table->getName();
    const DictTable& dictTable = m_table->dictTable();
    const ColumnVector& columns = m_table->dmlColumns();
    ctx_assert(columns.size() > 0);
    const unsigned attrCount = columns.size() - 1;
    // create the code
    Exec_insert::Code& code = *new Exec_insert::Code();
    code.m_insertOp = m_insertOp;
    code.m_tableName = strcpy(new char[tableName.length() + 1], tableName.c_str());
    code.m_attrCount = attrCount;
    code.m_attrId = new NdbAttrId[1 + attrCount];
    code.m_isKey = new bool[1 + attrCount];
    code.m_attrId[0] = (NdbAttrId)-1;
    code.m_tupleId = dictTable.tupleId();		// maybe 0
    code.m_autoIncrement = dictTable.autoIncrement();	// maybe 0
    unsigned k;
    if ((k = code.m_tupleId) != 0 || (k = code.m_autoIncrement) != 0) {
	const DictColumn& dictColumn = *dictTable.getColumn(k);
	code.m_idType = dictColumn.sqlType();
    }
    for (unsigned i = 1; i <= attrCount; i++) {
	Plan_column* column = columns[i];
	ctx_assert(column != 0);
	const DictColumn& dictColumn = column->dictColumn();
	code.m_attrId[i] = dictColumn.getAttrId();
	code.m_isKey[i] = dictColumn.isKey();
    }
    // default values XXX a mess
    code.m_defaultCount = 0;
    for (unsigned j = 1; j <= dictTable.getSize(); j++) {
	const DictColumn& dictColumn = *dictTable.getColumn(j);
	if (dictColumn.getDefaultValue() != 0 && ! code.findAttrId(dictColumn.getAttrId()))
	    code.m_defaultCount++;
    }
    if (code.m_defaultCount != 0) {
	code.m_defaultId = new NdbAttrId[1 + code.m_defaultCount];
	for (unsigned i = 0, j = 1; j <= dictTable.getSize(); j++) {
	    const DictColumn& dictColumn = *dictTable.getColumn(j);
	    if (dictColumn.getDefaultValue() != 0 && ! code.findAttrId(dictColumn.getAttrId()))
		code.m_defaultId[++i] = dictColumn.getAttrId();
	}
	SqlSpecs sqlSpecs(code.m_defaultCount);
	for (unsigned i = 0, j = 1; j <= dictTable.getSize(); j++) {
	    const DictColumn& dictColumn = *dictTable.getColumn(j);
	    if (dictColumn.getDefaultValue() != 0 && ! code.findAttrId(dictColumn.getAttrId())) {
		SqlSpec sqlSpec(dictColumn.sqlType(), SqlSpec::Physical);
		sqlSpecs.setEntry(++i, sqlSpec);
	    }
	}
	code.m_defaultValue = new SqlRow(sqlSpecs);
	for (unsigned i = 0, j = 1; j <= dictTable.getSize(); j++) {
	    const DictColumn& dictColumn = *dictTable.getColumn(j);
	    if (dictColumn.getDefaultValue() != 0 && ! code.findAttrId(dictColumn.getAttrId())) {
		const char* defaultValue = dictColumn.getDefaultValue();
		ExtType extType(ExtType::Char);
		ExtSpec extSpec(extType);
		SQLINTEGER ind = SQL_NTS;
		ExtField extField(extSpec, (SQLPOINTER)defaultValue, 0, &ind);
		SqlField& f = code.m_defaultValue->getEntry(++i);
		f.copyin(ctx, extField);
		if (! ctx.ok())
		    return 0;
	    }
	}
    }
    // create the exec
    Exec_insert* exec = new Exec_insert(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setCode(code);
    exec->setQuery(execQuery);
    return exec;
}

void
Plan_insert::print(Ctx& ctx)
{
    ctx.print(" [%s", m_insertOp == Insert_op_insert ? "insert" : "write");
    Plan_base* a[] = { m_table, m_dmlRow, m_exprRow, m_query };
    printList(ctx, a, 4);
    ctx.print("]");
}

// Exec_insert

Exec_insert::Code::~Code()
{
    delete[] m_tableName;
    delete[] m_attrId;
    delete[] m_isKey;
    delete[] m_defaultId;
    delete m_defaultValue;
}

bool
Exec_insert::Code::findAttrId(NdbAttrId attrId) const
{
    for (unsigned i = 1; i <= m_attrCount; i++) {
	if (m_attrId[i] == attrId)
	    return true;
    }
    return false;
}

Exec_insert::Data::~Data()
{
}

Exec_insert::~Exec_insert()
{
}

void
Exec_insert::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate the query
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    // create data
    Data& data = *new Data();
    setData(data);
}

void
Exec_insert::close(Ctx& ctx)
{
}

void
Exec_insert::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx_assert(m_query != 0);
    ctx.print(" [insert");
    ctx.print(" attrId=");
    for (unsigned i = 1; i <= code.m_attrCount; i++) {
	if (i > 1)
	    ctx.print(",");
	ctx.print("%u", (unsigned)code.m_attrId[i]);
    }
    ctx.print(" table=%s", code.m_tableName);
    m_query->print(ctx);
    ctx.print("]");
}
