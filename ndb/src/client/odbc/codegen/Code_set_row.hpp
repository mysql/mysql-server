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

#ifndef ODBC_CODEGEN_Code_set_row_hpp
#define ODBC_CODEGEN_Code_set_row_hpp

#include <vector>
#include <common/common.hpp>
#include <common/DataRow.hpp>
#include "Code_base.hpp"
#include "Code_dml_row.hpp"
#include "Code_expr_row.hpp"
#include "Code_root.hpp"

/**
 * @class Plan_set_row
 * @brief Row of column assigments in update
 *
 * Used only in parse.  The column and expression rows are moved
 * to the update node immediately after parse.
 */
class Plan_set_row : public Plan_base {
public:
    Plan_set_row(Plan_root* root);
    virtual ~Plan_set_row();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void addColumn(Plan_dml_column* column);
    void addExpr(Plan_expr* expr);
protected:
    friend class Plan_update;
    friend class Plan_insert;	// for MySql
    Plan_dml_row* m_dmlRow;
    Plan_expr_row* m_exprRow;
};

inline
Plan_set_row::Plan_set_row(Plan_root* root) :
    Plan_base(root)
{
    m_dmlRow = new Plan_dml_row(root);
    root->saveNode(m_dmlRow);
    m_exprRow = new Plan_expr_row(root);
    root->saveNode(m_exprRow);
}

// children

inline void
Plan_set_row::addColumn(Plan_dml_column* column)
{
    m_dmlRow->addColumn(column);
}

inline void
Plan_set_row::addExpr(Plan_expr* expr)
{
    m_exprRow->addExpr(expr);
}

#endif
