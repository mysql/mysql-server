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

#include "Code_create_row.hpp"
#include "Code_root.hpp"

Plan_create_row::~Plan_create_row()
{
}

Plan_base*
Plan_create_row::analyze(Ctx& ctx, Ctl& ctl)
{
    // check for duplicate column name
    for (unsigned i = 1, n = countColumn(); i < n; i++) {
	const BaseString& a = getColumn(i)->getName();
	for (unsigned i2 = i + 1; i2 <= n; i2++) {
	    const BaseString& a2 = getColumn(i2)->getName();
	    if (strcmp(a.c_str(), a2.c_str()) == 0) {
		ctx.pushStatus(Error::Gen, "duplicate column %s", a.c_str());
		return 0;
	    }
	}
    }
    // move single-column primary key constraint to constraint list
    for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	Plan_ddl_column* column = getColumn(i);
	if (column->m_primaryKey) {
	    Plan_ddl_row* ddlRow = new Plan_ddl_row(m_root);
	    m_root->saveNode(ddlRow);
	    ddlRow->addColumn(column);
	    Plan_ddl_constr* constr = new Plan_ddl_constr(m_root);
	    m_root->saveNode(constr);
	    constr->setRow(ddlRow);
	    addConstr(constr);
	    column->m_primaryKey = false;	// will be set again
	}
    }
    // check primary key constraints
    if (countConstr() < 1) {
	ctx.pushStatus(Error::Gen, "table must have a primary key");
	return 0;
    }
    if (countConstr() > 1) {
	ctx.pushStatus(Error::Gen, "table can have only one primary key");
	return 0;
    }
    Plan_ddl_row* ddlRow = getConstr(1)->getRow();
    for (unsigned i = 1, n = ddlRow->countColumn(); i <= n; i++) {
	Plan_ddl_column* column = ddlRow->getColumn(i);
	const BaseString& a = column->getName();
	bool found = false;
	for (unsigned i2 = 1, n2 = countColumn(); i2 <= n2; i2++) {
	    Plan_ddl_column* column2 = getColumn(i2);
	    const BaseString& a2 = column2->getName();
	    if (strcmp(a.c_str(), a2.c_str()) != 0)
		continue;
	    if (column2->getPrimaryKey()) {
		ctx.pushStatus(Error::Gen, "duplicate primary key constraint on %s", a.c_str());
		return 0;
	    }
	    column2->setPrimaryKey();
	    found = true;
	    break;
	}
	if (! found) {
	    ctx.pushStatus(Error::Gen, "undefined primary key column %s", a.c_str());
	    return 0;
	}
    }
    // analyze column types
    for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	getColumn(i)->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    // check TupleId
    unsigned tupleId = 0;
    for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	Plan_ddl_column* column = getColumn(i);
	if (! column->getTupleId())
	    continue;
	if (i != 1) {
	    ctx.pushStatus(Error::Gen, "tuple id column %u is not first column", i);
	    return 0;
	}
	if (tupleId != 0) {	// cannot happen now since attr name is fixed
	    ctx.pushStatus(Error::Gen, "duplicate tuple id column %u", i);
	    return 0;
	}
	tupleId = i;
    }
    if (tupleId != 0) {
	for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	    Plan_ddl_column* column = getColumn(i);
	    if (i == tupleId)
		continue;
	    if (! column->getPrimaryKey())
		continue;
	    ctx.pushStatus(Error::Gen, "cannot have both tuple id and other primary key column %u", i);
	    return 0;
	}
    }
    // check auto-increment
    unsigned autoIncrement = 0;
    for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	Plan_ddl_column* column = getColumn(i);
	if (! column->getAutoIncrement())
	    continue;
	if (autoIncrement != 0) {
	    ctx.pushStatus(Error::Gen, "duplicate auto-increment column %u", i);
	    return 0;
	}
	autoIncrement = i;
    }
    if (autoIncrement != 0) {
	for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	    Plan_ddl_column* column = getColumn(i);
	    if (i == autoIncrement)
		continue;
	    if (! column->getPrimaryKey())
		continue;
	    ctx.pushStatus(Error::Gen, "cannot have both auto-increment column and other primary key column %u", i);
	    return 0;
	}
    }
    return this;
}

Exec_base*
Plan_create_row::codegen(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(false);
    return 0;
}

void
Plan_create_row::print(Ctx& ctx)
{
    ctx.print(" [create_row");
    for (unsigned i = 1; i <= countColumn(); i++) {
	Plan_base* a = m_columnList[i];
	printList(ctx, &a, 1);
    }
    for (unsigned i = 1; i <= countConstr(); i++) {
	Plan_base* a = m_constrList[i];
	printList(ctx, &a, 1);
    }
}
