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
#include "Code_delete.hpp"
#include "Code_delete_lookup.hpp"
#include "Code_delete_index.hpp"
#include "Code_delete_scan.hpp"
#include "Code_query_filter.hpp"
#include "Code_query_lookup.hpp"
#include "Code_query_index.hpp"
#include "Code_query_scan.hpp"
#include "Code_query_range.hpp"
#include "Code_query_repeat.hpp"
#include "Code_table.hpp"
#include "Code_root.hpp"

Plan_delete::~Plan_delete()
{
}

Plan_base*
Plan_delete::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_delete);
    // analyze the table
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    // set name resolution scope
    ctl.m_tableList.resize(1 + 1);	// indexed from 1
    ctl.m_tableList[1] = m_table;
    Plan_dml* stmt = 0;
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
	    const bool direct = ! ctl.m_extra && exactKey;
	    ctx_log3(("delete direct=%d: extra=%d exact=%d", direct, ctl.m_extra, exactKey));
	    if (indexBest->m_rank == 0) {
		// primary key
		Plan_delete_lookup* deleteLookup = new Plan_delete_lookup(m_root);
		m_root->saveNode(deleteLookup);
		deleteLookup->setTable(m_table);
		if (direct) {
		    // key match with no extra conditions
		    Plan_query_repeat* queryRepeat = new Plan_query_repeat(m_root, 1);
		    m_root->saveNode(queryRepeat);
		    deleteLookup->setQuery(queryRepeat);
		} else {
		    // key match with extra conditions
		    Plan_query_lookup* queryLookup = new Plan_query_lookup(m_root);
		    m_root->saveNode(queryLookup);
		    Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
		    m_root->saveNode(queryFilter);
		    queryLookup->setTable(m_table);
		    queryFilter->setQuery(queryLookup);
		    queryFilter->setPred(m_pred);
		    queryFilter->m_topTable = m_table;
		    deleteLookup->setQuery(queryFilter);
		}
		stmt = deleteLookup;
	    } else if (indexBest->m_rank == 1) {
		// hash index
		Plan_delete_index* deleteIndex = new Plan_delete_index(m_root);
		m_root->saveNode(deleteIndex);
		deleteIndex->setTable(m_table, indexBest);
		if (direct) {
		    // key match with no extra conditions
		    Plan_query_repeat* queryRepeat = new Plan_query_repeat(m_root, 1);
		    m_root->saveNode(queryRepeat);
		    deleteIndex->setQuery(queryRepeat);
		} else {
		    // key match with extra conditions
		    Plan_query_index* queryIndex = new Plan_query_index(m_root);
		    m_root->saveNode(queryIndex);
		    Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
		    m_root->saveNode(queryFilter);
		    queryIndex->setTable(m_table, indexBest);
		    queryFilter->setQuery(queryIndex);
		    queryFilter->setPred(m_pred);
		    queryFilter->m_topTable = m_table;
		    deleteIndex->setQuery(queryFilter);
		}
		stmt = deleteIndex;
	    } else if (indexBest->m_rank == 2) {
		// ordered index
		Plan_delete_scan* deleteScan = new Plan_delete_scan(m_root);
		m_root->saveNode(deleteScan);
		Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
		m_root->saveNode(queryFilter);
		Plan_query_range* queryRange = new Plan_query_range(m_root);
		m_root->saveNode(queryRange);
		queryRange->setTable(m_table, indexBest);
		queryRange->setExclusive();
		queryFilter->setQuery(queryRange);
		queryFilter->setPred(m_pred);
		queryFilter->m_topTable = m_table;
		const TableSet& ts2 = m_pred->noInterp();
		ctx_assert(ts2.size() <= 1);
		if (ts2.size() == 0) {
		    queryRange->setInterp(m_pred);
		}
		deleteScan->setQuery(queryFilter);
		stmt = deleteScan;
	    } else {
		ctx_assert(false);
	    }
	} else {
	    // scan delete with filter
	    Plan_delete_scan* deleteScan = new Plan_delete_scan(m_root);
	    m_root->saveNode(deleteScan);
	    Plan_query_filter* queryFilter = new Plan_query_filter(m_root);
	    m_root->saveNode(queryFilter);
	    Plan_query_scan* queryScan = new Plan_query_scan(m_root);
	    m_root->saveNode(queryScan);
	    queryScan->setTable(m_table);
	    queryScan->setExclusive();
	    queryFilter->setQuery(queryScan);
	    queryFilter->setPred(m_pred);
	    queryFilter->m_topTable = m_table;
	    // interpeter
	    const TableSet& ts2 = m_pred->noInterp();
	    ctx_assert(ts2.size() <= 1);
	    if (ts2.size() == 0) {
		queryScan->setInterp(m_pred);
	    }
	    deleteScan->setQuery(queryFilter);
	    stmt = deleteScan;
	}
    } else {
	// scan delete without filter
	Plan_delete_scan* deleteScan = new Plan_delete_scan(m_root);
	m_root->saveNode(deleteScan);
	Plan_query_scan* queryScan = new Plan_query_scan(m_root);
	m_root->saveNode(queryScan);
	queryScan->setTable(m_table);
	queryScan->setExclusive();
	deleteScan->setQuery(queryScan);
	stmt = deleteScan;
    }
    // set base for column position offsets
    m_table->m_resOff = 1;
    return stmt;
}

void
Plan_delete::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "DELETE WHERE", SQL_DIAG_DELETE_WHERE);
}

Exec_base*
Plan_delete::codegen(Ctx& ctx, Ctl& ctl)
{   
    ctx_assert(false);
    return 0;
}    

void
Plan_delete::print(Ctx& ctx)
{
    ctx.print(" [delete");
    Plan_base* a[] = { m_table, m_pred };
    printList(ctx, a, 1);
    ctx.print("]");
}
