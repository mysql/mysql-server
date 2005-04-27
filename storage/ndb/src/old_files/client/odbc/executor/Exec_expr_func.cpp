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

#include <time.h>
#include <NdbTick.h>
#include <codegen/Code_expr_func.hpp>

void
Exec_expr_func::init(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    const SqlType& t = code.sqlSpec().sqlType();
    SqlField& f = ctl.m_groupIndex == 0 ? data.m_sqlField : data.groupField(code.sqlSpec().sqlType(), ctl.m_groupIndex, ctl.m_groupInit);
    if (ctl.m_groupIndex >= data.m_groupAcc.size())
	data.m_groupAcc.resize(1 + ctl.m_groupIndex);
    Data::Acc& acc = data.m_groupAcc[ctl.m_groupIndex];
    acc.m_count = 0;
    Expr_func::Code fc = code.m_func.m_code;
    if (fc == Expr_func::Substr || fc == Expr_func::Left || fc == Expr_func::Right) {
    } else if (fc == Expr_func::Count) {
	f.sqlBigint(0);
    } else if (fc == Expr_func::Min || fc == Expr_func::Max) {
	f.sqlNull(true);
    } else if (fc == Expr_func::Sum) {
	f.sqlNull(true);
	switch (t.type()) {
	case SqlType::Bigint:
	    acc.m_bigint = 0;
	    break;
	case SqlType::Double:
	    acc.m_double = 0;
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else if (fc == Expr_func::Avg) {
	f.sqlNull(true);
	switch (t.type()) {
	case SqlType::Double:
	    acc.m_double = 0.0;
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else if (fc == Expr_func::Rownum) {
	// uses only m_count
    } else if (fc == Expr_func::Sysdate) {
	// set time once
	NDB_TICKS secs = 0;
	Uint32 micros = 0;
	NdbTick_CurrentMicrosecond(&secs, &micros);
	time_t clock = secs;
	struct tm* t = gmtime(&clock);
	SqlDatetime& d = acc.m_sysdate;
	d.cc((1900 + t->tm_year) / 100);
	d.yy((1900 + t->tm_year) % 100);
	d.mm(1 + t->tm_mon);
	d.dd(t->tm_mday);
	d.HH(t->tm_hour);
	d.MM(t->tm_min);
	d.SS(t->tm_sec);
	d.ff(1000 * micros);
    } else {
	ctx_assert(false);
    }
}

void
Exec_expr_func::evaluate(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    const SqlType& t = code.sqlSpec().sqlType();
    if (ctl.m_groupInit)
	init(ctx, ctl);
    SqlField& f = ctl.m_groupIndex == 0 ? data.m_sqlField : data.groupField(code.sqlSpec().sqlType(), ctl.m_groupIndex, false);
    Data::Acc& acc = data.m_groupAcc[ctl.m_groupIndex];
    Expr_func::Code fc = code.m_func.m_code;
    const unsigned narg = code.m_narg;
    Exec_expr** args = code.m_args;
    ctx_assert(args != 0);
    // evaluate arguments
    for (unsigned i = 1; i <= narg; i++) {
	ctx_assert(args[i] != 0);
	unsigned save = ctl.m_groupIndex;
	if (code.m_func.m_aggr)
	    ctl.m_groupIndex = 0;
	args[i]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	ctl.m_groupIndex = save;
    }
    if (fc == Expr_func::Substr || fc == Expr_func::Left || fc == Expr_func::Right) {
	ctx_assert((narg == (unsigned)2) || (narg == (unsigned)(fc == Expr_func::Substr ? 3 : 2)));
	const SqlType& t1 = args[1]->getCode().sqlSpec().sqlType();
	const SqlField& f1 = args[1]->getData().sqlField();
	int pos, len;
	for (unsigned i = 2; i <= narg; i++) {
	    int& num = (fc == Expr_func::Substr ? (i == 2 ? pos : len) : len);
	    const SqlType& t2 = args[i]->getCode().sqlSpec().sqlType();
	    const SqlField& f2 = args[i]->getData().sqlField();
	    switch (t2.type()) {
	    case SqlType::Smallint:
		num = static_cast<int>(f2.sqlSmallint());
		break;
	    case SqlType::Integer:
		num = static_cast<int>(f2.sqlInteger());
		break;
	    case SqlType::Bigint:
		num = static_cast<int>(f2.sqlBigint());
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	}
	int length = 0;
	const SqlChar* data = 0;
	switch (t1.type()) {
	case SqlType::Char:
	    length = t1.length();
	    data = f1.sqlChar();
	    break;
	case SqlType::Varchar:
	    unsigned ulength;
	    data = f1.sqlVarchar(&ulength);
	    length = ulength;
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
	if (fc == Expr_func::Left)
	    pos = 1;
	else if (fc == Expr_func::Right)
	    pos = len > length ? 1 : length - len + 1;
	else if (pos < 0)
	    pos += length + 1;
	if (pos <= 0 || pos > length || len <= 0) {
	    f.sqlNull(true);	// oracle-ish
	    return;
	}
	if (len > length - pos + 1)
	    len = length - pos + 1;
	switch (t1.type()) {
	case SqlType::Char:
	    f.sqlChar(data + (pos - 1), len);
	    break;
	case SqlType::Varchar:
	    f.sqlVarchar(data + (pos - 1), len);
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else if (fc == Expr_func::Count) {
	ctx_assert(narg == 0 || narg == 1);
	if (ctl.m_postEval)
	    return;
	if (narg == 1) {
	    const SqlField& f1 = args[1]->getData().sqlField();
	    if (f1.sqlNull())
		return;
	}
	f.sqlBigint(++acc.m_count);
    } else if (fc == Expr_func::Min) {
	ctx_assert(narg == 1);
	if (ctl.m_postEval)
	    return;
	const SqlField& f1 = args[1]->getData().sqlField();
	if (f1.sqlNull())
	    return;
	if (f.sqlNull() || f1.less(f))
	    f1.copy(ctx, f);
    } else if (fc == Expr_func::Max) {
	ctx_assert(narg == 1);
	if (ctl.m_postEval)
	    return;
	const SqlField& f1 = args[1]->getData().sqlField();
	if (f1.sqlNull())
	    return;
	if (f.sqlNull() || f.less(f1))
	    f1.copy(ctx, f);
    } else if (fc == Expr_func::Sum) {
	ctx_assert(narg == 1);
	if (ctl.m_postEval)
	    return;
	const SqlType& t1 = args[1]->getCode().sqlSpec().sqlType();
	const SqlField& f1 = args[1]->getData().sqlField();
	if (f1.sqlNull())
	    return;
	switch (t.type()) {
	case SqlType::Bigint:
	    switch (t1.type()) {
	    case SqlType::Integer:
		acc.m_bigint += f1.sqlInteger();
		break;
	    case SqlType::Bigint:
		acc.m_bigint += f1.sqlBigint();
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    f.sqlBigint(acc.m_bigint);
	    break;
	case SqlType::Double:
	    switch (t1.type()) {
	    case SqlType::Real:
		acc.m_double += f1.sqlReal();
		break;
	    case SqlType::Double:
		acc.m_double += f1.sqlDouble();
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    f.sqlDouble(acc.m_double);
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
    } else if (fc == Expr_func::Avg) {
	ctx_assert(narg == 1);
	if (ctl.m_postEval)
	    return;
	const SqlType& t1 = args[1]->getCode().sqlSpec().sqlType();
	const SqlField& f1 = args[1]->getData().sqlField();
	if (f1.sqlNull())
	    return;
	switch (t1.type()) {
	case SqlType::Smallint:
	    acc.m_bigint += f1.sqlSmallint();
	    break;
	case SqlType::Integer:
	    acc.m_bigint += f1.sqlInteger();
	    break;
	case SqlType::Bigint:
	    acc.m_bigint += f1.sqlBigint();
	    break;
	case SqlType::Real:
	    acc.m_double += f1.sqlReal();
	    break;
	case SqlType::Double:
	    acc.m_double += f1.sqlDouble();
	    break;
	default:
	    ctx_assert(false);
	    break;
	}
	f.sqlDouble(acc.m_double / (SqlDouble)++acc.m_count);
    } else if (fc == Expr_func::Rownum) {
	ctx_assert(narg == 0);
	if (! ctl.m_postEval)
	    f.sqlBigint(1 + acc.m_count);
	else
	    acc.m_count++;
    } else if (fc == Expr_func::Sysdate) {
	ctx_assert(narg == 0);
	if (ctl.m_postEval)
	    return;
	f.sqlDatetime(acc.m_sysdate);
    } else {
	ctx_assert(false);
    }
}
