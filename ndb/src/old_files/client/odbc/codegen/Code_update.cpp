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
#include "Code_update.hpp"
#include "Code_update_lookup.hpp"
#include "Code_update_index.hpp"
#include "Code_update_scan.hpp"
#include "Code_table.hpp"
#include "Code_query_project.hpp"
#include "Code_query_filter.hpp"
#include "Code_query_scan.hpp"
#include "Code_query_lookup.hpp"
#include "Code_query_index.hpp"
#include "Code_query_range.hpp"
#include "Code_query_repeat.hpp"
#include "Code_root.hpp"

// Plan_update

Plan_update::~Plan_update()
{
}

Plan_base*
Plan_update::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_update);
    // analyze the table
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    // get column and expression rows
    ctx_assert(m_setRow != 0);
    setDmlRow(m_setRow->m_dmlRow);
    setExprRow(m_setRow->m_exprRow);
    m_setRow = 0;
    // implied by parse
    ctx_assert(m_dmlRow->getSize() == m_exprRow->getSize());
    // set name resolution scope
    ctl.m_tableList.resize(1 + 1);	// indexed from 1
    ctl.m_tableList[1] = m_table;
    // analyze the rows
    m_dmlRow->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctl.m_dmlRow = m_dmlRow;	// row type to convert to
    ctl.m_const = true;		// set to constants
    m_exprRow->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    bool setConst = ctl.m_const;
    ctl.m_dmlRow = 0;
    Plan_dml* stmt = 0;
    // top level query is a project
    Plan_query_project* queryProject = new Plan_query_project(m_root);
    m_root->saveNode(queryProject);
    queryProject->setRow(m_exprRow);
    if (m_pred != 0) {
	// analyze the predicate
	ctl.m_topand = true;
	ctl.m_extra = false;
	m_pred = static_cast<Plan_pred*>(m_pred->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(m_pred != 0);
	// check for key match
	Plan_table::Index* indexBest = 0;
	for (unsigned i = 0; i <= m_table->indexCount(); i++) {
	    Plan_table::Index& index = m_table->m_indexList[i];
	    TableSet tsDone;
	    m_table->resolveSet(ctx, index, tsDone);
	    if (! ctx.ok())
		return 0;
	    if (! index.m_keyFound)
		continue;
	    // prefer smaller rank, less unused keys
	    int k;
	    (k = (indexBest == 0)) ||
		(k = (indexBest->m_rank - index.m_rank)) ||
		(k = (indexBest->m_keyCountUnused - index.m_keyCountUnused));
	    if (k > 0)
		indexBest = &index;
	}
	if (indexBest != 0) {
	    const bool exactKey = indexBest->m_rank <= 1 ? m_table->exactKey(ctx, indexBest) : false;
	    const bool direct = setConst && ! ctl.m_extra && exactKey;
	    ctx_log3(("update direct=%d: const=%d extra=%d exact=%d", direct, setConst, ctl.m_extra, exactKey));
	    if (indexBest->m_rank == 0) {
		// primary key
		Plan_update_lookup* updateLookup = new Plan_update_lookup(m_root);
		m_root->saveNode(updateLookup);
		updateLookup->setTable(m_table);
		updateLookup->setDmlRow(m_dmlRow);
		if (direct) {
		    // constant values and exact key match
		    Plan_query_repeat* queryRepeat = new Plan_query_repeat(m_root, 1);
		    m_root->saveNode(queryRepeat);
		    queryProject->setQuery(queryRepeat);
		} else {
		    // more conditions or non-constant values
		    Plan_query_lookup* queryLookup = new Plan_query_lookup(m_root);
		    m_root->saveNode(queryLookup);
		    Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
		    m_root->saveNode(queryFilter);
		    queryLookup->setTable(m_table);
		    queryFilter->setQuery(queryLookup);
		    queryFilter->setPred(m_pred);
		    queryFilter->m_topTable = m_table;
		    queryProject->setQuery(queryFilter);
		}
		updateLookup->setQuery(queryProject);
		stmt = updateLookup;
	    } else if (indexBest->m_rank == 1) {
		// hash index
		Plan_update_index* updateIndex = new Plan_update_index(m_root);
		m_root->saveNode(updateIndex);
		updateIndex->setTable(m_table, indexBest);
		updateIndex->setDmlRow(m_dmlRow);
		if (direct) {
		    // constant values and exact key match
		    Plan_query_repeat* queryRepeat = new Plan_query_repeat(m_root, 1);
		    m_root->saveNode(queryRepeat);
		    queryProject->setQuery(queryRepeat);
		} else {
		    // more conditions or non-constant values
		    Plan_query_index* queryIndex = new Plan_query_index(m_root);
		    m_root->saveNode(queryIndex);
		    Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
		    m_root->saveNode(queryFilter);
		    queryIndex->setTable(m_table, indexBest);
		    queryFilter->setQuery(queryIndex);
		    queryFilter->setPred(m_pred);
		    queryFilter->m_topTable = m_table;
		    queryProject->setQuery(queryFilter);
		}
		updateIndex->setQuery(queryProject);
		stmt = updateIndex;
	    } else if (indexBest->m_rank == 2) {
		// ordered index
		Plan_update_scan* updateScan = new Plan_update_scan(m_root);
		m_root->saveNode(updateScan);
		updateScan->setTable(m_table);
		updateScan->setDmlRow(m_dmlRow);
		Plan_query_range* queryRange = new Plan_query_range(m_root);
		m_root->saveNode(queryRange);
		queryRange->setTable(m_table, indexBest);
		queryRange->setExclusive();
		Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
		m_root->saveNode(queryFilter);
		queryFilter->setQuery(queryRange);
		queryFilter->setPred(m_pred);
		queryFilter->m_topTable = m_table;
		// interpeter
		const TableSet& ts2 = m_pred->noInterp();
		ctx_assert(ts2.size() <= 1);
		if (ts2.size() == 0) {
		    queryRange->setInterp(m_pred);
		}
		queryProject->setQuery(queryFilter);
		updateScan->setQuery(queryProject);
		stmt = updateScan;
	    } else {
		ctx_assert(false);
	    }
	} else {
	    // scan update with filter
	    Plan_update_scan* updateScan = new Plan_update_scan(m_root);
	    m_root->saveNode(updateScan);
	    updateScan->setTable(m_table);
	    updateScan->setDmlRow(m_dmlRow);
	    Plan_query_scan* queryScan = new Plan_query_scan(m_root);
	    m_root->saveNode(queryScan);
	    queryScan->setTable(m_table);
	    queryScan->setExclusive();
	    Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
	    m_root->saveNode(queryFilter);
	    queryFilter->setQuery(queryScan);
	    queryFilter->setPred(m_pred);
	    queryFilter->m_topTable = m_table;
	    // interpeter
	    const TableSet& ts2 = m_pred->noInterp();
	    ctx_assert(ts2.size() <= 1);
	    if (ts2.size() == 0) {
		queryScan->setInterp(m_pred);
	    }
	    queryProject->setQuery(queryFilter);
	    updateScan->setQuery(queryProject);
	    stmt = updateScan;
	}
    } else {
	// scan update without filter
	Plan_update_scan* updateScan = new Plan_update_scan(m_root);
	m_root->saveNode(updateScan);
	updateScan->setTable(m_table);
	updateScan->setDmlRow(m_dmlRow);
	Plan_query_scan* queryScan = new Plan_query_scan(m_root);
	m_root->saveNode(queryScan);
	queryScan->setTable(m_table);
	queryScan->setExclusive();
	queryProject->setQuery(queryScan);
	updateScan->setQuery(queryProject);
	stmt = updateScan;
    }
    // set base for column position offsets
    m_table->m_resOff = 1;
    return stmt;
}

void
Plan_update::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "UPDATE WHERE", SQL_DIAG_UPDATE_WHERE);
}

Exec_base*
Plan_update::codegen(Ctx& ctx, Ctl& ctl)
{   
    ctx_assert(false);
    return 0;
}    

void
Plan_update::print(Ctx& ctx)
{
    ctx.print(" [update");
    Plan_base* a[] = { m_table, m_setRow, m_dmlRow, m_exprRow };
    printList(ctx, a, 4);
    ctx.print("]");
}
