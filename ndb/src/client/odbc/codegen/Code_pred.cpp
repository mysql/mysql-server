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

// Plan_pred

Plan_pred::~Plan_pred()
{
}

bool
Plan_pred::isGroupBy(const Plan_expr_row* row) const
{
    return false;
}

Plan_pred*
Plan_pred::opAnd(Plan_pred* pred2)
{
    Plan_pred_op* predAnd = new Plan_pred_op(m_root, Pred_op::And);
    m_root->saveNode(predAnd);
    predAnd->setPred(1, this);
    predAnd->setPred(2, pred2);
    return predAnd;
}

// Exec_pred

Exec_pred::Code::~Code()
{
}

Exec_pred::Data::~Data()
{
}

Exec_pred::~Exec_pred()
{
}

Pred_value&
Exec_pred::Data::groupValue(unsigned i, bool initFlag)
{
    if (m_groupValue.size() == 0) {
	m_groupValue.resize(1);
    }
    if (initFlag) {
	//unsigned i2 = m_groupValue.size();
	//ctx_assert(i == i2);
	m_groupValue.push_back(Pred_value_unknown);
    }
    ctx_assert(i != 0 && i < m_groupValue.size());
    return m_groupValue[i];
}
