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

#ifndef ODBC_CODEGEN_Code_delete_hpp
#define ODBC_CODEGEN_Code_delete_hpp

#include <common/common.hpp>
#include "Code_base.hpp"
#include "Code_dml.hpp"
#include "Code_table.hpp"
#include "Code_query.hpp"
#include "Code_pred.hpp"

/**
 * @class Plan_delete
 * @brief Delete in PlanTree
 */
class Plan_delete : public Plan_dml {
public:
    Plan_delete(Plan_root* root);
    virtual ~Plan_delete();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    void describe(Ctx& ctx);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setTable(Plan_table* table);
    void setPred(Plan_pred* pred);
protected:
    Plan_table* m_table;
    Plan_pred* m_pred;
};

inline
Plan_delete::Plan_delete(Plan_root* root) :
    Plan_dml(root),
    m_table(0),
    m_pred(0)
{
}

inline void
Plan_delete::setTable(Plan_table* table)
{
    ctx_assert(table != 0);
    m_table = table;
}

inline void
Plan_delete::setPred(Plan_pred* pred)
{
    ctx_assert(pred != 0);
    m_pred = pred;
}

#endif
