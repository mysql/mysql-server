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

#ifndef ODBC_CODEGEN_Code_update_hpp
#define ODBC_CODEGEN_Code_update_hpp

#include <common/common.hpp>
#include "Code_base.hpp"
#include "Code_dml.hpp"
#include "Code_set_row.hpp"
#include "Code_table.hpp"
#include "Code_pred.hpp"
#include "Code_query.hpp"

/**
 * @class Plan_update
 * @brief Update in PlanTree
 */
class Plan_update : public Plan_dml {
public:
    Plan_update(Plan_root* root);
    virtual ~Plan_update();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void describe(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    void setTable(Plan_table* table);
    void setRow(Plan_set_row* setRow);
    void setDmlRow(Plan_dml_row* dmlRow);
    void setExprRow(Plan_expr_row* exprRow);
    void setPred(Plan_pred* pred);
protected:
    Plan_table* m_table;
    Plan_set_row* m_setRow;
    Plan_dml_row* m_dmlRow;
    Plan_expr_row* m_exprRow;
    Plan_pred* m_pred;
};

inline
Plan_update::Plan_update(Plan_root* root) :
    Plan_dml(root),
    m_table(0),
    m_setRow(0),
    m_dmlRow(0),
    m_exprRow(0),
    m_pred(0)
{
}

// children

inline void
Plan_update::setTable(Plan_table* table)
{
    ctx_assert(table != 0);
    m_table = table;
}

inline void
Plan_update::setRow(Plan_set_row* setRow)
{
    ctx_assert(setRow != 0);
    m_setRow = setRow;
}

inline void
Plan_update::setDmlRow(Plan_dml_row* dmlRow)
{
    ctx_assert(dmlRow != 0);
    m_dmlRow = dmlRow;
}

inline void
Plan_update::setExprRow(Plan_expr_row* exprRow)
{
    ctx_assert(exprRow != 0);
    m_exprRow = exprRow;
}

inline void
Plan_update::setPred(Plan_pred* pred)
{
    ctx_assert(pred != 0);
    m_pred = pred;
}

#endif
