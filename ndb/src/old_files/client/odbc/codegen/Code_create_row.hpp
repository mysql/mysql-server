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

#ifndef ODBC_CODEGEN_Code_create_row_hpp
#define ODBC_CODEGEN_Code_create_row_hpp

#include <vector>
#include <common/common.hpp>
#include "Code_base.hpp"
#include "Code_ddl_column.hpp"
#include "Code_ddl_constr.hpp"

/**
 * @class Plan_create_row
 * @brief Row of columns and constraints in create statement
 */
class Plan_create_row : public Plan_base {
public:
    Plan_create_row(Plan_root* root);
    virtual ~Plan_create_row();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    unsigned countColumn() const;
    void addColumn(Plan_ddl_column* column);
    Plan_ddl_column* getColumn(unsigned i) const;
    unsigned countConstr() const;
    void addConstr(Plan_ddl_constr* constr);
    Plan_ddl_constr* getConstr(unsigned i) const;
protected:
    DdlColumnVector m_columnList;
    DdlConstrVector m_constrList;
};

inline
Plan_create_row::Plan_create_row(Plan_root* root) :
    Plan_base(root),
    m_columnList(1),
    m_constrList(1)
{
}

// children

inline unsigned
Plan_create_row::countColumn() const
{
    return m_columnList.size() - 1;
}

inline void
Plan_create_row::addColumn(Plan_ddl_column* column)
{
    ctx_assert(column != 0);
    m_columnList.push_back(column);
}

inline Plan_ddl_column*
Plan_create_row::getColumn(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_columnList.size() && m_columnList[i] != 0);
    return m_columnList[i];
}

inline unsigned
Plan_create_row::countConstr() const
{
    return m_constrList.size() - 1;
}

inline void
Plan_create_row::addConstr(Plan_ddl_constr* constr)
{
    ctx_assert(constr != 0);
    m_constrList.push_back(constr);
}

inline Plan_ddl_constr*
Plan_create_row::getConstr(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_constrList.size() && m_constrList[i] != 0);
    return m_constrList[i];
}

#endif
