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

#ifndef ODBC_CODEGEN_Code_select_hpp
#define ODBC_CODEGEN_Code_select_hpp

#include <common/common.hpp>
#include "Code_stmt.hpp"
#include "Code_expr_row.hpp"
#include "Code_table_list.hpp"
#include "Code_pred.hpp"

/**
 * @class Plan_select
 * @brief General select in PlanTree
 *
 * General select.  An initial PlanTree node.
 */
class Plan_select : public Plan_stmt {
public:
    Plan_select(Plan_root* root);
    virtual ~Plan_select();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setList(Plan_table_list* tableList);
    void setRow(Plan_expr_row* exprRow);
    void setPred(Plan_pred* pred);
    void setSort(Plan_expr_row* sortRow);
    void setDistinct(bool distinct);
    void setGroup(Plan_expr_row* groupRow);
    void setHaving(Plan_pred* havingPred);
    void setLimit(int off, int cnt);
protected:
    Plan_table_list* m_tableList;
    Plan_expr_row* m_exprRow;
    Plan_pred* m_pred;
    Plan_expr_row* m_sortRow;
    bool m_distinct;
    Plan_expr_row* m_groupRow;
    Plan_pred* m_havingPred;
    int m_limitOff;
    int m_limitCnt;
};

inline
Plan_select::Plan_select(Plan_root* root) :
    Plan_stmt(root),
    m_tableList(0),
    m_exprRow(0),
    m_pred(0),
    m_sortRow(0),
    m_distinct(false),
    m_groupRow(0),
    m_havingPred(0),
    m_limitOff(0),
    m_limitCnt(-1)
{
}

// children

inline void
Plan_select::setList(Plan_table_list* tableList)
{
    ctx_assert(tableList != 0);
    m_tableList = tableList;
}

inline void
Plan_select::setRow(Plan_expr_row* exprRow)
{
    ctx_assert(exprRow != 0);
    m_exprRow = exprRow;
}

inline void
Plan_select::setPred(Plan_pred* pred)
{
    ctx_assert(pred != 0);
    m_pred = pred;
}

inline void
Plan_select::setSort(Plan_expr_row* sortRow)
{
    ctx_assert(sortRow != 0);
    m_sortRow = sortRow;
}

inline void
Plan_select::setDistinct(bool distinct)
{
    m_distinct = distinct;
}

inline void
Plan_select::setGroup(Plan_expr_row* groupRow)
{
    ctx_assert(groupRow != 0);
    m_groupRow = groupRow;
}

inline void
Plan_select::setHaving(Plan_pred* havingPred)
{
    ctx_assert(havingPred != 0);
    m_havingPred = havingPred;
}

inline void
Plan_select::setLimit(int off, int cnt)
{
    m_limitOff = off;
    m_limitCnt = cnt;
}

#endif
