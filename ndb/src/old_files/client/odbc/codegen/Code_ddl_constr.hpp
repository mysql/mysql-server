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

#ifndef ODBC_CODEGEN_Code_ddl_constr_hpp
#define ODBC_CODEGEN_Code_ddl_constr_hpp

#include <common/common.hpp>
#include "Code_ddl_row.hpp"

/**
 * @class Plan_ddl_constr
 * @brief Constraint in DDL statement
 *
 * Only unnamed primary key constraint exists.
 */
class Plan_ddl_constr : public Plan_base {
public:
    Plan_ddl_constr(Plan_root* root);
    virtual ~Plan_ddl_constr();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setRow(Plan_ddl_row* ddlRow);
    Plan_ddl_row* getRow() const;
protected:
    Plan_ddl_row* m_ddlRow;
};

inline
Plan_ddl_constr::Plan_ddl_constr(Plan_root* root) :
    Plan_base(root)
{
}

// children

inline void
Plan_ddl_constr::setRow(Plan_ddl_row* ddlRow)
{
    ctx_assert(ddlRow != 0);
    m_ddlRow = ddlRow;
}

inline Plan_ddl_row*
Plan_ddl_constr::getRow() const
{
    ctx_assert(m_ddlRow != 0);
    return m_ddlRow;
}

#endif
