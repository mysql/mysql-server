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

#include "Code_pred.hpp"
#include "Code_pred_op.hpp"
#include "Code_root.hpp"

// Pred_op

const char*
Pred_op::name() const
{
    switch (m_opcode) {
    case And:
	return "and";
    case Or:
	return "or";
    case Not:
	return "not";
    }
    ctx_assert(false);
    return "";
}

unsigned
Pred_op::arity() const
{
    switch (m_opcode) {
    case And:
    case Or:
	return 2;
    case Not:
	return 1;
    }
    ctx_assert(false);
    return 0;
}

// Plan_pred_op

Plan_pred_op::~Plan_pred_op()
{
}

Plan_base*
Plan_pred_op::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    unsigned arity = m_op.arity();
    // check if we remain in top-level AND-clause
    const bool topand = ctl.m_topand;
    if (m_op.m_opcode != Pred_op::And)
	ctl.m_topand = false;
    // analyze sub-predicates
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_pred[i] != 0);
	m_pred[i]->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    // save top level predicate on list
    if (topand && ! ctl.m_topand) {
	ctl.m_topcomp.push_back(this);
    }
    ctl.m_topand = topand;
    // table dependencies are union from operands
    m_tableSet.clear();
    for (unsigned i = 1; i <= arity; i++) {
	const TableSet& ts = m_pred[i]->tableSet();
	m_tableSet.insert(ts.begin(), ts.end());
    }
    // set of tables for which interpreter cannot be used
    m_noInterp.clear();
    for (unsigned i = 1; i <= arity; i++) {
	const TableSet& ts = m_pred[i]->noInterp();
	m_noInterp.insert(ts.begin(), ts.end());
    }
    return this;
}

Exec_base*
Plan_pred_op::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    unsigned arity = m_op.arity();
    Exec_pred_op* exec = new Exec_pred_op(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // create code for operands
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_pred[i] != 0);
	Exec_pred* execPred = static_cast<Exec_pred*>(m_pred[i]->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execPred != 0);
	exec->setPred(i, execPred);
    }
    // create the code
    Exec_pred_op::Code& code = *new Exec_pred_op::Code(m_op);
    exec->setCode(code);
    m_exec = exec;
    return exec;
}

void
Plan_pred_op::print(Ctx& ctx)
{
    ctx.print(" [%s", m_op.name());
    Plan_base* a[] = { m_pred[1], m_pred[2] };
    printList(ctx, a, m_op.arity());
    ctx.print("]");
}

bool
Plan_pred_op::isGroupBy(const Plan_expr_row* row) const
{
    const unsigned arity = m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_pred[i] != 0);
	if (! m_pred[i]->isGroupBy(row))
	    return false;
    }
    return true;
}

// Code_pred_op

Exec_pred_op::Code::~Code()
{
}

Exec_pred_op::Data::~Data()
{
}

Exec_pred_op::~Exec_pred_op()
{
}

void
Exec_pred_op::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // allocate sub-predicates
    unsigned arity = code.m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_pred[i] != 0);
	m_pred[i]->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
    Data& data = *new Data;
    setData(data);
}

void
Exec_pred_op::close(Ctx& ctx)
{
    const Code& code = getCode();
    unsigned arity = code.m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_pred[i] != 0);
	m_pred[i]->close(ctx);
    }
}

void
Exec_pred_op::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [%s", code.m_op.name());
    Exec_base* a[] = { m_pred[1], m_pred[2] };
    printList(ctx, a, code.m_op.arity());
    ctx.print("]");
}
