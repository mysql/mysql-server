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

#include <algorithm>
#include <common/StmtArea.hpp>
#include <dictionary/DictTable.hpp>
#include "Code_select.hpp"
#include "Code_query_lookup.hpp"
#include "Code_query_index.hpp"
#include "Code_query_scan.hpp"
#include "Code_query_range.hpp"
#include "Code_query_sys.hpp"
#include "Code_query_project.hpp"
#include "Code_query_filter.hpp"
#include "Code_query_join.hpp"
#include "Code_query_count.hpp"
#include "Code_query_sort.hpp"
#include "Code_query_group.hpp"
#include "Code_query_distinct.hpp"
#include "Code_expr_column.hpp"
#include "Code_expr_const.hpp"
#include "Code_pred_op.hpp"
#include "Code_root.hpp"

Plan_select::~Plan_select()
{
}

Plan_base*
Plan_select::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_select);
    // analyze tables
    ctx_assert(m_tableList != 0);
    for (unsigned i = 1; i <= m_tableList->countTable(); i++) {
	Plan_table* table = m_tableList->getTable(i);
	table->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    ctx_assert(m_exprRow != 0);
    if (m_exprRow->getAsterisk()) {
	// expand unqualified asterisk to table-qualified columns
	setRow(new Plan_expr_row(m_root));
	m_root->saveNode(m_exprRow);
	for (unsigned i = 1; i <= m_tableList->countTable(); i++) {
	    const Plan_table* table = m_tableList->getTable(i);
	    const DictTable& dictTable = table->dictTable();
	    for (unsigned i = 1; i <= dictTable.getSize(); i++) {
		DictColumn* dictColumn = dictTable.getColumn(i);
		Plan_expr_column* column = new Plan_expr_column(m_root, dictColumn->getName());
		m_root->saveNode(column);
		column->setCname(table->getCname());
		m_exprRow->addExpr(column);
	    }
	}
    }
    // set name resolution scope
    ctl.m_tableList = m_tableList->m_tableList;
    ctx_assert(ctl.m_tableList.size() >= 1 + 1);
    ctl.m_aggrin = false;
    // analyze select row
    ctl.m_aggrok = true;
    ctx_assert(m_exprRow != 0);
    m_exprRow = static_cast<Plan_expr_row*>(m_exprRow->analyze(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(m_exprRow != 0);
    // analyze group by row
    ctl.m_aggrok = false;
    if (m_groupRow != 0) {
	m_groupRow = static_cast<Plan_expr_row*>(m_groupRow->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(m_groupRow != 0);
    }
    // analyze having predicate
    ctl.m_aggrok = true;
    if (m_havingPred != 0) {
	m_havingPred = static_cast<Plan_pred*>(m_havingPred->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(m_havingPred != 0);
    }
    // ana|yze order by row
    ctl.m_aggrok = true;
    if (m_sortRow != 0) {
	m_sortRow = static_cast<Plan_expr_row*>(m_sortRow->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(m_sortRow != 0);
    }
    // analyze the predicate
    ctl.m_aggrok = false;
    ctl.m_topand = true;
    ctl.m_extra = false;
    if (m_pred != 0) {
	m_pred = static_cast<Plan_pred*>(m_pred->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(m_pred != 0);
    }
    // check if group by required
    if (m_exprRow->anyAggr() && ! m_exprRow->allBound() && m_groupRow == 0) {
	ctx.pushStatus(Error::Gen, "missing GROUP BY clause");
	return 0;
    }
    // in special cases add "group by 1"
    if (m_groupRow == 0) {
	bool addgb = false;
	if (m_havingPred != 0) {
	    // allowed by oracle but nearly useless
	    addgb = true;
	} else if (m_exprRow->anyAggr() && m_sortRow != 0) {
	    // allowed by oracle but useless
	    ctx_assert(m_exprRow->allBound());
	    addgb = true;
	}
	if (addgb) {
	    ctx_log2(("adding 'group by 1'"));
	    m_groupRow = new Plan_expr_row(m_root);
	    m_root->saveNode(m_groupRow);
	    LexType type(LexType::Integer);
	    Plan_expr* expr = new Plan_expr_const(m_root, type, "1");
	    m_root->saveNode(expr);
	    m_groupRow->addExpr(expr);
	    m_groupRow = static_cast<Plan_expr_row*>(m_groupRow->analyze(ctx, ctl));
	    ctx_assert(ctx.ok());
	    ctx_assert(m_groupRow != 0);
	}
    }
    // check group by allowed
    if (m_groupRow != 0) {
	if (! m_exprRow->isAllGroupBy(m_groupRow)) {
	    ctx.pushStatus(Error::Gen, "invalid GROUP BY expression in SELECT list");
	    return 0;
	}
	if (m_havingPred != 0) {
	    if (! m_havingPred->isGroupBy(m_groupRow)) {
		ctx.pushStatus(Error::Gen, "invalid GROUP BY expression in HAVING clause");
		return 0;
	    }
	}
	if (m_sortRow != 0) {
	    if (! m_sortRow->isAllGroupBy(m_groupRow)) {
		ctx.pushStatus(Error::Gen, "invalid GROUP BY expression in ORDER BY clause");
		return 0;
	    }
	}
    }
    // log top level predicate
    {
	unsigned n = 0;
	for (PredList::iterator i = ctl.m_topcomp.begin(); i != ctl.m_topcomp.end(); i++)
	    ctx_log2(("top level pred %u: count tables = %u, not interp = %u",
                     ++n, 
                     (unsigned)(*i)->tableSet().size(), 
                     (unsigned)(*i)->noInterp().size()));
    }
    // compose the raw query from lookups and scans
    Plan_query* queryRaw = 0;
    TableVector tableVector(1);
    TableSet tsDone;
    while (tableVector.size() < ctl.m_tableList.size()) {
	Plan_table* tableBest = 0;
	Plan_table::Index* indexBest = 0;
	for (unsigned n = 1; n < ctl.m_tableList.size(); n++) {
	    Plan_table* table = ctl.m_tableList[n];
	    if (tsDone.find(table) != tsDone.end())
		continue;
	    // get system table out of the way
	    if (table->dictTable().sysId()) {
		tableBest = table;
		break;
	    }
	    // find best match for primary key or index
	    for (unsigned i = 0; i <= table->indexCount(); i++) {
		Plan_table::Index& index = table->m_indexList[i];
		table->resolveSet(ctx, index, tsDone);
		if (! ctx.ok())
		    return 0;
		if (! index.m_keyFound)
		    continue;
		// prefer smaller dependency set, smaller rank, less unused keys
		int k;
		(k = (indexBest == 0)) ||
		    (k = (indexBest->m_keySet.size() - index.m_keySet.size())) ||
		    (k = (indexBest->m_rank - index.m_rank)) ||
		    (k = (indexBest->m_keyCountUnused - index.m_keyCountUnused));
		if (k > 0) {
		    tableBest = table;
		    indexBest = &index;
		}
	    }
	}
	Plan_query* queryNext = 0;
	Plan_table* tableNext = 0;
	Plan_query_scan* queryScan = 0;		// for pushing interpreted program
	Plan_query_range* queryRange = 0;	// ditto
	if (tableBest == 0) {
	    // scan first unprocessed table
	    for (unsigned n = 1; n < ctl.m_tableList.size(); n++) {
		Plan_table* table = ctl.m_tableList[n];
		if (tsDone.find(table) != tsDone.end())
		    continue;
		tableNext = table;
		break;
	    }
	    ctx_assert(tableNext != 0);
	    queryScan = new Plan_query_scan(m_root);
	    m_root->saveNode(queryScan);
	    queryScan->setTable(tableNext);
	    queryNext = queryScan;
	    ctx_log2(("optim: scan %s", tableNext->getPrintName()));
	} else if (tableBest->dictTable().sysId()) {
	    // "scan" system table
	    tableNext = tableBest;
	    Plan_query_sys* querySys = new Plan_query_sys(m_root);
	    m_root->saveNode(querySys);
	    querySys->setTable(tableNext);
	    queryNext = querySys;
	    ctx_log2(("optim: scan %s", tableNext->getPrintName()));
	} else if (indexBest->m_keySet.size() > 0) {
	    // scan first table this one depends on
	    const TableSet& keySet = indexBest->m_keySet;
	    for (unsigned n = 1; n < ctl.m_tableList.size(); n++) {
		Plan_table* table = ctl.m_tableList[n];
		if (keySet.find(table) == keySet.end())
		    continue;
		ctx_assert(tsDone.find(table) == tsDone.end());
		tableNext = table;
		break;
	    }
	    ctx_assert(tableNext != 0);
	    queryScan = new Plan_query_scan(m_root);
	    m_root->saveNode(queryScan);
	    queryScan->setTable(tableNext);
	    queryNext = queryScan;
	    ctx_log2(("optim: scan %s for %s", tableNext->getPrintName(), tableBest->getPrintName()));
	} else if (indexBest->m_rank == 0) {
	    // primary key depends only on processed tables
	    tableNext = tableBest;
	    Plan_query_lookup* queryLookup = new Plan_query_lookup(m_root);
	    m_root->saveNode(queryLookup);
	    queryLookup->setTable(tableNext);
	    queryNext = queryLookup;
	    ctx_log2(("optim: lookup %s", tableNext->getPrintName()));
	} else if (indexBest->m_rank == 1) {
	    // hash index key depends only on processed tables
	    tableNext = tableBest;
	    Plan_query_index* queryIndex = new Plan_query_index(m_root);
	    m_root->saveNode(queryIndex);
	    queryIndex->setTable(tableNext, indexBest);
	    queryNext = queryIndex;
	    ctx_log2(("optim: lookup %s via index %s", tableNext->getPrintName(), indexBest->m_dictIndex->getName().c_str()));
	} else if (indexBest->m_rank == 2) {
	    // ordered index key depends only on processed tables
	    tableNext = tableBest;
	    queryRange = new Plan_query_range(m_root);
	    m_root->saveNode(queryRange);
	    queryRange->setTable(tableNext, indexBest);
	    queryNext = queryRange;
	    ctx_log2(("optim: range scan %s via index %s", tableNext->getPrintName(), indexBest->m_dictIndex->getName().c_str()));
	} else {
	    ctx_assert(false);
	}
	if (queryRaw == 0) {
	    queryRaw = queryNext;
	} else {
	    Plan_query_join* queryJoin = new Plan_query_join(m_root);
	    m_root->saveNode(queryJoin);
	    queryJoin->setInner(queryRaw);
	    queryJoin->setOuter(queryNext);
	    queryRaw = queryJoin;
	}
	tableVector.push_back(tableNext);
	tsDone.insert(tableNext);
	// push down part of top level predicate to table scan or range scan
	Plan_pred* predPush = 0;
	Plan_pred* predInterp = 0;
	PredList::iterator i = ctl.m_topcomp.begin();
	while (i != ctl.m_topcomp.end()) {
	    const TableSet& ts = (*i)->tableSet();
	    if (! std::includes(tsDone.begin(), tsDone.end(), ts.begin(), ts.end())) {
		i++;
		continue;
	    }
	    predPush = predPush == 0 ? *i : predPush->opAnd(*i);
	    if (queryScan != 0) {
		const TableSet& ts2 = (*i)->noInterp();
		if (ts2.find(tableNext) == ts2.end())
		    predInterp = predInterp == 0 ? *i : predInterp->opAnd(*i);
	    }
	    if (queryRange != 0) {
		const TableSet& ts2 = (*i)->noInterp();
		if (ts2.find(tableNext) == ts2.end())
		    predInterp = predInterp == 0 ? *i : predInterp->opAnd(*i);
	    }
	    // remove it from top level predicate
	    PredList::iterator j = i;
	    i++;
	    ctl.m_topcomp.erase(j);
	}
	if (predPush != 0) {
	    Plan_query_filter* queryPush = new Plan_query_filter(m_root);
	    m_root->saveNode(queryPush);
	    queryPush->setQuery(queryRaw);
	    queryPush->setPred(predPush);
	    queryPush->m_topTable = tableNext;
	    queryRaw = queryPush;
	}
	if (predInterp != 0) {
	    if (queryScan != 0)
		queryScan->setInterp(predInterp);
	    else if (queryRange != 0)
		queryRange->setInterp(predInterp);
	    else
		ctx_assert(false);
	}
    }
    ctx_assert(ctl.m_topcomp.empty());
    // set base for column position offsets
    for (unsigned n = 1; n < tableVector.size(); n++) {
	Plan_table* table = tableVector[n];
	if (n == 1) {
	    table->m_resOff = 1;
	} else {
	    Plan_table* tablePrev = tableVector[n - 1];
	    table->m_resOff = tablePrev->m_resOff + tablePrev->m_exprColumns.size() - 1;
	}
    }
    // next level up is one of project, count, group by
    Plan_query* queryTop;
    if (m_groupRow == 0) {
	if (! m_exprRow->anyAggr()) {
	    Plan_query_project* queryProject = new Plan_query_project(m_root);
	    m_root->saveNode(queryProject);
	    queryProject->setQuery(queryRaw);
	    queryProject->setRow(m_exprRow);
	    queryProject->setLimit(m_limitOff, m_limitCnt);
	    queryTop = queryProject;
	} else {
	    ctx_assert(m_exprRow->allBound());
	    Plan_query_count* queryCount = new Plan_query_count(m_root);
	    m_root->saveNode(queryCount);
	    queryCount->setQuery(queryRaw);
	    queryCount->setRow(m_exprRow);
	    queryTop = queryCount;
	}
    } else {
	Plan_query_group* queryGroup = new Plan_query_group(m_root);
	m_root->saveNode(queryGroup);
	queryGroup->setQuery(queryRaw);
	queryGroup->setDataRow(m_exprRow);
	queryGroup->setGroupRow(m_groupRow);
	if (m_havingPred != 0)
	    queryGroup->setHavingPred(m_havingPred);
	queryTop = queryGroup;
    }
    // optional sort becomes new top level
    if (m_sortRow != 0) {
	Plan_query_sort* querySort = new Plan_query_sort(m_root);
	m_root->saveNode(querySort);
	querySort->setQuery(queryTop);
	querySort->setRow(m_sortRow);
	queryTop = querySort;
    }
    // optional distinct becomes new top level
    if (m_distinct) {
	Plan_query_distinct* queryDistinct = new Plan_query_distinct(m_root);
	m_root->saveNode(queryDistinct);
	queryDistinct->setQuery(queryTop);
	queryTop = queryDistinct;
    }
    // return top node
    return queryTop;
}

Exec_base*
Plan_select::codegen(Ctx& ctx, Ctl& ctl)
{   
    ctx_assert(false);
    return 0;
}    

void
Plan_select::print(Ctx& ctx)
{
    ctx.print(" [select");
    Plan_base* a[] = { m_tableList, m_exprRow, m_pred, m_groupRow, m_havingPred };
    printList(ctx, a, 5);
    ctx.print("]");
}
