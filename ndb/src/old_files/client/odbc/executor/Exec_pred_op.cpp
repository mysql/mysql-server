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

#include <codegen/Code_pred_op.hpp>
#include <NdbScanFilter.hpp>

struct TableUnary {
    Pred_value value1;
    Pred_value result;
};

struct TableBinary {
    Pred_value value1;
    Pred_value value2;
    Pred_value result;
};

static TableUnary
tableNot[] = {
    {	Pred_value_unknown,	Pred_value_unknown	},
    {	Pred_value_false,	Pred_value_true		},
    {	Pred_value_true,	Pred_value_false	},
};

static TableBinary
tableAnd[] = {
    {	Pred_value_unknown,	Pred_value_unknown,	Pred_value_unknown	},
    {	Pred_value_unknown,	Pred_value_false,	Pred_value_false	},
    {	Pred_value_unknown,	Pred_value_true,	Pred_value_unknown	},
    {	Pred_value_false,	Pred_value_unknown,	Pred_value_false	},
    {	Pred_value_false,	Pred_value_false,	Pred_value_false	},
    {	Pred_value_false,	Pred_value_true,	Pred_value_false	},
    {	Pred_value_true,	Pred_value_unknown,	Pred_value_unknown	},
    {	Pred_value_true,	Pred_value_false,	Pred_value_false	},
    {	Pred_value_true,	Pred_value_true,	Pred_value_true		}
};

static TableBinary
tableOr[] = {
    {	Pred_value_unknown,	Pred_value_unknown,	Pred_value_unknown	},
    {	Pred_value_unknown,	Pred_value_false,	Pred_value_unknown	},
    {	Pred_value_unknown,	Pred_value_true,	Pred_value_true		},
    {	Pred_value_false,	Pred_value_unknown,	Pred_value_unknown	},
    {	Pred_value_false,	Pred_value_false,	Pred_value_false	},
    {	Pred_value_false,	Pred_value_true,	Pred_value_true		},
    {	Pred_value_true,	Pred_value_unknown,	Pred_value_true		},
    {	Pred_value_true,	Pred_value_false,	Pred_value_true		},
    {	Pred_value_true,	Pred_value_true,	Pred_value_true		}
};

void
Exec_pred_op::execInterp(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    ctx_assert(ctl.m_scanFilter != 0);
    NdbScanFilter& scanFilter = *ctl.m_scanFilter;
    if (code.m_op.arity() == 1) {
	ctx_assert(m_pred[1] != 0);
	switch (code.m_op.m_opcode) {
	case Pred_op::Not:
	    scanFilter.begin(NdbScanFilter::NAND);
	    m_pred[1]-> execInterp(ctx, ctl);
	    if (! ctx.ok())
		return;
	    scanFilter.end();
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else if (code.m_op.arity() == 2) {
	ctx_assert(m_pred[1] != 0 && m_pred[2] != 0);
	switch (code.m_op.m_opcode) {
	case Pred_op::And:
	    scanFilter.begin(NdbScanFilter::AND);
	    m_pred[1]-> execInterp(ctx, ctl);
	    if (! ctx.ok())
		return;
	    m_pred[2]-> execInterp(ctx, ctl);
	    if (! ctx.ok())
		return;
	    scanFilter.end();
	    break;
	case Pred_op::Or:
	    scanFilter.begin(NdbScanFilter::OR);
	    m_pred[1]-> execInterp(ctx, ctl);
	    if (! ctx.ok())
		return;
	    m_pred[2]-> execInterp(ctx, ctl);
	    if (! ctx.ok())
		return;
	    scanFilter.end();
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else {
	ctx_assert(false);
    }
}

void
Exec_pred_op::evaluate(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    Pred_value v = Pred_value_unknown;
    if (code.m_op.arity() == 1) {
	// evaluate sub-expression
	ctx_assert(m_pred[1] != 0);
	m_pred[1]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	if (ctl.m_postEval)
	    return;
	Pred_value v1 = ctl.m_groupIndex == 0 ? m_pred[1]->getData().getValue() : m_pred[1]->getData().groupValue(ctl.m_groupIndex);
	// look up result
	TableUnary* table = 0;
	unsigned size = 0;
	switch (code.m_op.m_opcode) {
	case Pred_op::Not:
	    table = tableNot;
	    size = sizeof(tableNot) / sizeof(tableNot[0]);
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
	unsigned i;
	for (i = 0; i < size; i++) {
	    if (table[i].value1 == v1) {
		v = table[i].result;
		break;
	    }
	}
	ctx_assert(i < size);
    } else if (code.m_op.arity() == 2) {
	// evaluate sub-expressions
	ctx_assert(m_pred[1] != 0 && m_pred[2] != 0);
	m_pred[1]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	m_pred[2]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	if (ctl.m_postEval)
	    return;
	Pred_value v1 = ctl.m_groupIndex == 0 ? m_pred[1]->getData().getValue() : m_pred[1]->getData().groupValue(ctl.m_groupIndex);
	Pred_value v2 = ctl.m_groupIndex == 0 ? m_pred[2]->getData().getValue() : m_pred[2]->getData().groupValue(ctl.m_groupIndex);
	// look up result
	TableBinary* table = 0;
	unsigned size = 0;
	switch (code.m_op.m_opcode) {
	case Pred_op::And:
	    table = tableAnd;
	    size = sizeof(tableAnd) / sizeof(tableAnd[0]);
	    break;
	case Pred_op::Or:
	    table = tableOr;
	    size = sizeof(tableOr) / sizeof(tableOr[0]);
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
	unsigned i;
	for (i = 0; i < size; i++) {
	    if (table[i].value1 == v1 && table[i].value2 == v2) {
		v = table[i].result;
		break;
	    }
	}
	ctx_assert(i < size);
    } else {
	ctx_assert(false);
    }
    // set result
    if (ctl.m_groupIndex == 0)
	data.m_value = v;
    else
	data.groupValue(ctl.m_groupIndex, ctl.m_groupInit) = v;
}
