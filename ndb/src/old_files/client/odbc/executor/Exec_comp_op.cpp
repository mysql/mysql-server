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

#include <NdbScanFilter.hpp>
#include <NdbSqlUtil.hpp>
#include <codegen/Code_comp_op.hpp>

void
Exec_comp_op::execInterp(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    const unsigned arity = code.m_op.arity();
    const Comp_op::Opcode opcode = code.m_op.m_opcode;
    Data& data = getData();
    ctx_assert(ctl.m_scanFilter != 0);
    NdbScanFilter& scanFilter = *ctl.m_scanFilter;
    if (code.m_interpColumn == 0) {
	// args are constant on this level so evaluate entire predicate
	evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	if (data.m_value == Pred_value_true)
	    scanFilter.istrue();
	else
	    scanFilter.isfalse();
	return;
    }
    const NdbAttrId interpAttrId = code.m_interpAttrId;
    if (arity == 1) {
	ctx_assert(m_expr[1] != 0);
	const SqlType& t1 = m_expr[1]->getCode().sqlSpec().sqlType();
	const SqlField& f1 = m_expr[1]->getData().sqlField();
	switch (code.m_op.m_opcode) {
	case Comp_op::Isnull:
	    scanFilter.isnull(interpAttrId);
	    break;
	case Comp_op::Isnotnull:
	    scanFilter.isnotnull(interpAttrId);
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else if (arity == 2) {
	ctx_assert(m_expr[1] != 0 && m_expr[2] != 0);
	// one is column and the other is constant at this level
	ctx_assert(code.m_interpColumn == 1 || code.m_interpColumn == 2);
	const unsigned i = code.m_interpColumn;
	const unsigned j = 3 - i;
	// evaluate the constant
	m_expr[j]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	const SqlType& t1 = m_expr[i]->getCode().sqlSpec().sqlType();
	const SqlField& f1 = m_expr[i]->getData().sqlField();
	const SqlType& t2 = m_expr[j]->getCode().sqlSpec().sqlType();
	const SqlField& f2 = m_expr[j]->getData().sqlField();
	// handle null constant
	if (f2.sqlNull()) {
	    scanFilter.isfalse();
	    return;
	}
	// handle null in interpreter
	scanFilter.begin(NdbScanFilter::AND);
	scanFilter.isnotnull(interpAttrId);
	if (t1.type() == SqlType::Char || t1.type() == SqlType::Varchar) {
	    const char* v2 = 0;
	    unsigned n2 = 0;
	    bool nopad = false;
	    if (t1.type() == SqlType::Char && t2.type() == SqlType::Char) {
		v2 = reinterpret_cast<const char*>(f2.sqlChar());
		n2 = t2.length();
		nopad = false;
	    } else if (t1.type() == SqlType::Char && t2.type() == SqlType::Varchar) {
		v2 = reinterpret_cast<const char*>(f2.sqlVarchar(&n2));
		nopad = true;
	    } else if (t1.type() == SqlType::Varchar && t2.type() == SqlType::Char) {
		v2 = reinterpret_cast<const char*>(f2.sqlChar());
		n2 = t2.length();
		nopad = true;
	    } else if (t1.type() == SqlType::Varchar && t2.type() == SqlType::Varchar) {
		v2 = reinterpret_cast<const char*>(f2.sqlVarchar(&n2));
		nopad = true;
	    } else {
		ctx_assert(false);
	    }
	    switch (opcode) {
	    case Comp_op::Eq:
		scanFilter.eq(interpAttrId, v2, n2, nopad);
		break;
	    case Comp_op::Noteq:
		scanFilter.ne(interpAttrId, v2, n2, nopad);
		break;
	    case Comp_op::Lt:
		if (i == 1) {
		    scanFilter.lt(interpAttrId, v2, n2, nopad);
		} else {
		    scanFilter.gt(interpAttrId, v2, n2, nopad);
		}
		break;
	    case Comp_op::Lteq:
		if (i == 1) {
		    scanFilter.le(interpAttrId, v2, n2, nopad);
		} else {
		    scanFilter.ge(interpAttrId, v2, n2, nopad);
		}
		break;
	    case Comp_op::Gt:
		if (i == 1) {
		    scanFilter.gt(interpAttrId, v2, n2, nopad);
		} else {
		    scanFilter.lt(interpAttrId, v2, n2, nopad);
		}
		break;
	    case Comp_op::Gteq:
		if (i == 1) {
		    scanFilter.ge(interpAttrId, v2, n2, nopad);
		} else {
		    scanFilter.le(interpAttrId, v2, n2, nopad);
		}
		break;
	    case Comp_op::Like:
		scanFilter.like(interpAttrId, v2, n2, nopad);
		break;
	    case Comp_op::Notlike:
		scanFilter.notlike(interpAttrId, v2, n2, nopad);
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	} else if (t1.type() == SqlType::Smallint || t1.type() == SqlType::Integer || t1.type() == SqlType::Bigint) {
	    ctx_assert(t1.unSigned());
	    bool s2 = ! t2.unSigned();
	    SqlBigint v2;
	    SqlUbigint uv2;
	    if (s2) {
		v2 =
		    t2.type() == SqlType::Smallint ? f2.sqlSmallint() :
		    t2.type() == SqlType::Integer ? f2.sqlInteger() : f2.sqlBigint();
		uv2 = v2;
	    } else {
		uv2 =
		    t2.type() == SqlType::Smallint ? (SqlUsmallint)f2.sqlSmallint() :
		    t2.type() == SqlType::Integer ? (SqlUinteger)f2.sqlInteger() : (SqlUbigint)f2.sqlBigint();
		v2 = uv2;
	    }
	    switch (code.m_op.m_opcode) {
	    case Comp_op::Eq:
		if (s2 && v2 < 0)
		    scanFilter.isfalse();
		else
		    scanFilter.eq(interpAttrId, uv2);
		break;
	    case Comp_op::Noteq:
		if (s2 && v2 < 0)
		    scanFilter.istrue();
		else
		    scanFilter.ne(interpAttrId, uv2);
		break;
	    case Comp_op::Lt:
		if (i == 1) {
		    if (s2 && v2 < 0)
			scanFilter.isfalse();
		    else
			scanFilter.lt(interpAttrId, uv2);
		} else {
		    if (s2 && v2 < 0)
			scanFilter.istrue();
		    else
			scanFilter.gt(interpAttrId, uv2);
		}
		break;
	    case Comp_op::Lteq:
		if (i == 1) {
		    if (s2 && v2 < 0)
			scanFilter.isfalse();
		    else
			scanFilter.le(interpAttrId, uv2);
		} else {
		    if (s2 && v2 < 0)
			scanFilter.istrue();
		    else
			scanFilter.ge(interpAttrId, uv2);
		}
		break;
	    case Comp_op::Gt:
		if (i == 1) {
		    if (s2 && v2 < 0)
			scanFilter.istrue();
		    else
			scanFilter.gt(interpAttrId, uv2);
		} else {
		    if (s2 && v2 < 0)
			scanFilter.isfalse();
		    else
			scanFilter.lt(interpAttrId, uv2);
		}
		break;
	    case Comp_op::Gteq:
		if (i == 1) {
		    if (s2 && v2 < 0)
			scanFilter.istrue();
		    else
			scanFilter.ge(interpAttrId, uv2);
		} else {
		    if (s2 && v2 < 0)
			scanFilter.isfalse();
		    else
			scanFilter.le(interpAttrId, uv2);
		}
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	} else {
	    ctx_assert(false);
	}
	// end null guard
	scanFilter.end();
    } else {
	ctx_assert(false);
    }
}

static bool
do_sqlchar_comp(Comp_op::Opcode opcode, const SqlChar* s1, unsigned n1, const SqlChar* s2, unsigned n2, bool padded)
{
    int ret = NdbSqlUtil::char_compare(reinterpret_cast<const char*>(s1), n1, reinterpret_cast<const char *>(s2), n2, padded);
    switch (opcode) {
    case Comp_op::Eq:
	return ret == 0;
    case Comp_op::Noteq:
	return ret != 0;
    case Comp_op::Lt:
	return ret < 0;
    case Comp_op::Lteq:
	return ret <= 0;
    case Comp_op::Gt:
	return ret > 0;
    case Comp_op::Gteq:
	return ret >= 0;
    default:
	break;
    }
    ctx_assert(false);
    return false;
}

static bool
do_sqlchar_like(const SqlChar* s1, unsigned n1, const SqlChar* s2, unsigned n2, bool padded)
{
    bool ret = NdbSqlUtil::char_like(reinterpret_cast<const char*>(s1), n1, reinterpret_cast<const char *>(s2), n2, padded);
    return ret;
}

static bool
do_datetime_comp(Comp_op::Opcode opcode, SqlDatetime v1, SqlDatetime v2)
{
    int k = v1.less(v2) ? -1 : v2.less(v1) ? 1 : 0;
    switch (opcode) {
    case Comp_op::Eq:
	return k == 0;
    case Comp_op::Noteq:
	return k != 0;
    case Comp_op::Lt:
	return k < 0;
    case Comp_op::Lteq:
	return k <= 0;
    case Comp_op::Gt:
	return k > 0;
    case Comp_op::Gteq:
	return k >= 0;
    default:
	break;
    }
    ctx_assert(false);
    return false;
}

void
Exec_comp_op::evaluate(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    const unsigned arity = code.m_op.arity();
    const Comp_op::Opcode opcode = code.m_op.m_opcode;
    Data& data = getData();
    Pred_value v = Pred_value_unknown;
    if (arity == 1) {
	// evaluate sub-expression
	ctx_assert(m_expr[1] != 0);
	m_expr[1]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	if (ctl.m_postEval)
	    return;
	// get type and value
	const SqlType& t1 = m_expr[1]->getCode().sqlSpec().sqlType();
	const SqlField& f1 = ctl.m_groupIndex == 0 ? m_expr[1]->getData().sqlField() : m_expr[1]->getData().groupField(ctl.m_groupIndex);
	switch (code.m_op.m_opcode) {
	case Comp_op::Isnull:
	    v = f1.sqlNull() ? Pred_value_true : Pred_value_false;
	    break;
	case Comp_op::Isnotnull:
	    v = f1.sqlNull() ? Pred_value_false : Pred_value_true;
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else if (arity == 2) {
	// evaluate sub-expressions
	ctx_assert(m_expr[1] != 0 && m_expr[2] != 0);
	m_expr[1]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	m_expr[2]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	if (ctl.m_postEval)
	    return;
	// get types and values
	const SqlType& t1 = m_expr[1]->getCode().sqlSpec().sqlType();
	const SqlType& t2 = m_expr[2]->getCode().sqlSpec().sqlType();
	const SqlField& f1 = ctl.m_groupIndex == 0 ? m_expr[1]->getData().sqlField() : m_expr[1]->getData().groupField(ctl.m_groupIndex);
	const SqlField& f2 = ctl.m_groupIndex == 0 ? m_expr[2]->getData().sqlField() : m_expr[2]->getData().groupField(ctl.m_groupIndex);
	// handle null
	if (f1.sqlNull() || f2.sqlNull()) {
	    v = Pred_value_unknown;
	} else if (t1.type() == SqlType::Char) {
	    const SqlChar* v1 = f1.sqlChar();
	    unsigned n1 = t1.length();
	    if (t2.type() == SqlType::Char) {
		unsigned n2 = t2.length();
		const SqlChar* v2 = f2.sqlChar();
		bool b;
		switch (opcode) {
		case Comp_op::Like:
		    b = do_sqlchar_like(v1, n1, v2, n2, true);
		    break;
		case Comp_op::Notlike:
		    b = ! do_sqlchar_like(v1, n1, v2, n2, true);
		    break;
		default:
		    b = do_sqlchar_comp(opcode, v1, n1, v2, n2, true);
		    break;
		}
		v = b ? Pred_value_true : Pred_value_false;
	    } else if (t2.type() == SqlType::Varchar) {
		unsigned n2 = 0;
		const SqlChar* v2 = f2.sqlVarchar(&n2);
		bool b;
		switch (opcode) {
		case Comp_op::Like:
		    b = do_sqlchar_like(v1, n1, v2, n2, true);
		    break;
		case Comp_op::Notlike:
		    b = ! do_sqlchar_like(v1, n1, v2, n2, true);
		    break;
		default:
		    b = do_sqlchar_comp(opcode, v1, n1, v2, n2, false);
		    break;
		}
		v = b ? Pred_value_true : Pred_value_false;
	    } else {
		ctx_assert(false);
	    }
	} else if (t1.type() == SqlType::Varchar) {
	    unsigned n1 = 0;
	    const SqlChar* v1 = f1.sqlVarchar(&n1);
	    if (t2.type() == SqlType::Char) {
		unsigned n2 = t2.length();
		const SqlChar* v2 = f2.sqlChar();
		bool b;
		switch (opcode) {
		case Comp_op::Like:
		    b = do_sqlchar_like(v1, n1, v2, n2, false);
		    break;
		case Comp_op::Notlike:
		    b = ! do_sqlchar_like(v1, n1, v2, n2, false);
		    break;
		default:
		    b = do_sqlchar_comp(opcode, v1, n1, v2, n2, false);
		    break;
		}
		v = b ? Pred_value_true : Pred_value_false;
	    } else if (t2.type() == SqlType::Varchar) {
		unsigned n2 = 0;
		const SqlChar* v2 = f2.sqlVarchar(&n2);
		bool b;
		switch (opcode) {
		case Comp_op::Like:
		    b = do_sqlchar_like(v1, n1, v2, n2, false);
		    break;
		case Comp_op::Notlike:
		    b = ! do_sqlchar_like(v1, n1, v2, n2, false);
		    break;
		default:
		    b = do_sqlchar_comp(opcode, v1, n1, v2, n2, false);
		    break;
		}
		v = b ? Pred_value_true : Pred_value_false;
	    } else {
		ctx_assert(false);
	    }
	} else if (t1.type() == SqlType::Smallint || t1.type() == SqlType::Integer || t1.type() == SqlType::Bigint) {
	    // convert to bigint
	    bool s1 = ! t1.unSigned();
	    bool s2 = ! t2.unSigned();
	    SqlBigint v1, v2;
	    SqlUbigint uv1, uv2;
	    if (s1) {
		v1 =
		    t1.type() == SqlType::Smallint ? f1.sqlSmallint() :
		    t1.type() == SqlType::Integer ? f1.sqlInteger() : f1.sqlBigint();
		uv1 = v1;
	    } else {
		uv1 =
		    t1.type() == SqlType::Smallint ? (SqlUsmallint)f1.sqlSmallint() :
		    t1.type() == SqlType::Integer ? (SqlUinteger)f1.sqlInteger() : (SqlUbigint)f1.sqlBigint();
		v1 = uv1;
	    }
	    if (s2) {
		v2 =
		    t2.type() == SqlType::Smallint ? f2.sqlSmallint() :
		    t2.type() == SqlType::Integer ? f2.sqlInteger() : f2.sqlBigint();
		uv2 = v2;
	    } else {
		uv2 =
		    t2.type() == SqlType::Smallint ? (SqlUsmallint)f2.sqlSmallint() :
		    t2.type() == SqlType::Integer ? (SqlUinteger)f2.sqlInteger() : (SqlUbigint)f2.sqlBigint();
		v2 = uv2;
	    }
	    bool b;
	    switch (opcode) {
	    case Comp_op::Eq:
		b = s1 && s2 ? (v1 == v2) : s1 ? (v1 < 0 ? false : uv1 == uv2) : s2 ? (v2 < 0 ? false : uv1 == uv2) : (uv1 == uv2);
		break;
	    case Comp_op::Noteq:
		b = s1 && s2 ? (v1 == v2) : s1 ? (v1 < 0 ? true : uv1 != uv2) : s2 ? (v2 < 0 ? true : uv1 != uv2) : (uv1 != uv2);
		break;
	    case Comp_op::Lt:
		b = s1 && s2 ? (v1 < v2) : s1 ? (v1 < 0 ? true : uv1 < uv2) : s2 ? (v2 < 0 ? false : uv1 < uv2) : (uv1 < uv2);
		break;
	    case Comp_op::Lteq:
		b = s1 && s2 ? (v1 <= v2) : s1 ? (v1 < 0 ? true : uv1 <= uv2) : s2 ? (v2 < 0 ? false : uv1 <= uv2) : (uv1 <= uv2);
		break;
	    case Comp_op::Gt:
		b = s1 && s2 ? (v1 > v2) : s1 ? (v1 < 0 ? false : uv1 > uv2) : s2 ? (v2 < 0 ? true : uv1 > uv2) : (uv1 > uv2);
		break;
	    case Comp_op::Gteq:
		b = s1 && s2 ? (v1 >= v2) : s1 ? (v1 < 0 ? false : uv1 >= uv2) : s2 ? (v2 < 0 ? true : uv1 >= uv2) : (uv1 >= uv2);
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    v = b ? Pred_value_true : Pred_value_false;
	} else if (t1.type() == SqlType::Double) {
	    SqlDouble v1 = f1.sqlDouble();
	    SqlDouble v2 = f2.sqlDouble();
	    bool b;
	    switch (opcode) {
	    case Comp_op::Eq:
		b = (v1 == v2);
		break;
	    case Comp_op::Noteq:
		b = (v1 != v2);
		break;
	    case Comp_op::Lt:
		b = (v1 < v2);
		break;
	    case Comp_op::Lteq:
		b = (v1 <= v2);
		break;
	    case Comp_op::Gt:
		b = (v1 > v2);
		break;
	    case Comp_op::Gteq:
		b = (v1 >= v2);
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    v = b ? Pred_value_true : Pred_value_false;
	} else if (t1.type() == SqlType::Datetime) {
	    SqlDatetime v1 = f1.sqlDatetime();
	    SqlDatetime v2 = f2.sqlDatetime();
	    bool b;
	    b = do_datetime_comp(opcode, v1, v2);
	    v = b ? Pred_value_true : Pred_value_false;
	} else {
	    ctx_assert(false);
	}
    } else {
	ctx_assert(false);
    }
    // set result
    if (ctl.m_groupIndex == 0)
	data.m_value = v;
    else
	data.groupValue(ctl.m_groupIndex, ctl.m_groupInit) = v;
}
