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
#include <common/CodeTree.hpp>
#include <executor/Executor.hpp>
#include "CodeGen.hpp"
#include "Code_root.hpp"

#include <FlexLexer.h>
#include "SimpleParser.hpp"

void
CodeGen::prepare(Ctx& ctx)
{
    parse(ctx);
    if (! ctx.ok())
	return;
    analyze(ctx);
    if (! ctx.ok())
	return;
    describe(ctx);
}

void
CodeGen::execute(Ctx& ctx)
{
    DescArea& ipd = m_stmtArea.descArea(Desc_usage_IPD);
    if (m_stmtArea.m_unbound) {
	analyze(ctx);
	if (! ctx.ok())
	    return;
	describe(ctx);
	if (! ctx.ok())
	    return;
	if (m_stmtArea.m_unbound) {
	    ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "%u input parameters have unbound SQL type", m_stmtArea.m_unbound);
	    return;
	}
	ipd.setBound(true);
    }
    if (! ipd.isBound()) {
	ctx_log2(("IPD changed between executes - reanalyze"));
	// jdbc can change parameter length at each execute
	analyze(ctx);
	if (! ctx.ok())
	    return;
	describe(ctx);
	if (! ctx.ok())
	    return;
	freeExec(ctx);
	codegen(ctx);
	if (! ctx.ok())
	    return;
	alloc(ctx);
	if (! ctx.ok())
	    return;
	ipd.setBound(true);
    }
    if (m_stmtArea.m_execTree == 0) {
	codegen(ctx);
	if (! ctx.ok())
	    return;
	alloc(ctx);
	if (! ctx.ok())
	    return;
    }
    Executor executor(m_stmtArea);
    executor.execute(ctx);
}

void
CodeGen::fetch(Ctx& ctx)
{
    // XXX parameter types are not checked any more
    ctx_assert(! m_stmtArea.m_unbound);
    Executor executor(m_stmtArea);
    executor.fetch(ctx);
}

void
CodeGen::parse(Ctx& ctx)
{
    Plan_root* planRoot = new Plan_root(m_stmtArea);
    SimpleParser simpleParser(ctx, m_stmtArea, planRoot);
    simpleParser.yyparse();
    if (! ctx.ok())
	return;
    planRoot->m_paramList.resize(1 + simpleParser.paramNumber());
    ctx_log2(("CodeGen: parse done - plan tree follows"));
    if (ctx.logLevel() >= 2)
	planRoot->print(ctx);
    m_stmtArea.m_planTree = planRoot;
}

void
CodeGen::analyze(Ctx& ctx)
{
    Plan_root* planRoot = static_cast<Plan_root*>(m_stmtArea.m_planTree);
    ctx_assert(planRoot != 0);
    Plan_base::Ctl ctl(0);
    planRoot->analyze(ctx, ctl);	// returns itself
    if (! ctx.ok())
	return;
    ctx_log2(("CodeGen: analyze done - plan tree follows"));
    if (ctx.logLevel() >= 2)
	planRoot->print(ctx);
}

void
CodeGen::describe(Ctx& ctx)
{
    Plan_root* planRoot = static_cast<Plan_root*>(m_stmtArea.m_planTree);
    ctx_assert(planRoot != 0);
    planRoot->describe(ctx);
    ctx_log2(("CodeGen: describe done"));
}

void
CodeGen::codegen(Ctx& ctx)
{
    Plan_root* planRoot = static_cast<Plan_root*>(m_stmtArea.m_planTree);
    ctx_assert(planRoot != 0);
    Plan_base::Ctl ctl(0);
    Exec_root* execRoot = static_cast<Exec_root*>(planRoot->codegen(ctx, ctl));
    if (! ctx.ok())
	return;
    ctx_assert(execRoot != 0);
    ctx_log2(("CodeGen: codegen done - code tree follows"));
    if (ctx.logLevel() >= 2)
	execRoot->print(ctx);
    m_stmtArea.m_execTree = execRoot;
}

void
CodeGen::alloc(Ctx& ctx)
{
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    ctx_assert(execRoot != 0);
    Exec_base::Ctl ctl(0);
    execRoot->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    ctx_log2(("CodeGen: alloc done"));
}

void
CodeGen::close(Ctx& ctx)
{
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    if (execRoot != 0) {
	execRoot->close(ctx);
	ctx_log2(("CodeGen: close done"));
    }
}

void
CodeGen::free(Ctx& ctx)
{
    freePlan(ctx);
    freeExec(ctx);
}

void
CodeGen::freePlan(Ctx & ctx)
{
    if (m_stmtArea.m_planTree != 0) {
	Plan_root* planRoot = static_cast<Plan_root*>(m_stmtArea.m_planTree);
	ctx_assert(planRoot != 0);
	unsigned count = 1 + planRoot->m_nodeList.size();
	planRoot->freeNodeList();
	delete planRoot;
	m_stmtArea.m_planTree = 0;
	ctx_log3(("CodeGen: freed %u plan tree nodes", count));
    }
}

void
CodeGen::freeExec(Ctx & ctx)
{
    if (m_stmtArea.m_execTree != 0) {
	Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
	ctx_assert(execRoot != 0);
	unsigned count = 1 + execRoot->m_nodeList.size();
	execRoot->freeNodeList();
	delete execRoot;
	m_stmtArea.m_execTree = 0;
	ctx_log3(("CodeGen: freed %u exec tree nodes", count));
    }
}

// odbc support

void
CodeGen::sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind)
{
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    ctx_assert(execRoot != 0);
    execRoot->sqlGetData(ctx, columnNumber, targetType, targetValue, bufferLength, strlen_or_Ind);
}

void
CodeGen::sqlParamData(Ctx& ctx, SQLPOINTER* value)
{
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    ctx_assert(execRoot != 0);
    execRoot->sqlParamData(ctx, value);
}

void
CodeGen::sqlPutData(Ctx& ctx, SQLPOINTER data, SQLINTEGER strlen_or_Ind)
{
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    ctx_assert(execRoot != 0);
    execRoot->sqlPutData(ctx, data, strlen_or_Ind);
}
