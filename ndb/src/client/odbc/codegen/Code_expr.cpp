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

#include "Code_expr.hpp"
#include "Code_expr_row.hpp"

// Plan_expr

Plan_expr::~Plan_expr()
{
}

bool
Plan_expr::isEqual(const Plan_expr* expr) const
{
    return false;
}

bool
Plan_expr::isAnyEqual(const Plan_expr_row* row) const
{
    ctx_assert(row != 0);
    const unsigned size = row->getSize();
    for (unsigned i = 1; i <= size; i++) {
	if (isEqual(row->getExpr(i)))
	    return true;
    }
    return false;
}

bool
Plan_expr::isGroupBy(const Plan_expr_row* row) const
{
    return false;
}

// Exec_expr

Exec_expr::Code::~Code()
{
}

Exec_expr::Data::~Data()
{
}

Exec_expr::~Exec_expr()
{
}

SqlField&
Exec_expr::Data::groupField(const SqlType& sqlType, unsigned i, bool initFlag)
{
    if (m_groupField.size() == 0) {
	m_groupField.resize(1);
    }
    if (initFlag) {
	//unsigned i2 = m_groupField.size();
	//ctx_assert(i == i2);
	const SqlSpec sqlSpec(sqlType, SqlSpec::Physical);
	const SqlField sqlField(sqlSpec);
	m_groupField.push_back(sqlField);
    }
    ctx_assert(i != 0 && i < m_groupField.size());
    return m_groupField[i];
}
