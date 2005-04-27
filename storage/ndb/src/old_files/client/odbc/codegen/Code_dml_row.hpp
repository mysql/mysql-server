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

#ifndef ODBC_CODEGEN_Code_dml_row_hpp
#define ODBC_CODEGEN_Code_dml_row_hpp

#include <vector>
#include <common/common.hpp>
#include <common/DataRow.hpp>
#include "Code_base.hpp"
#include "Code_dml_column.hpp"

class Plan_dml_column;

/**
 * @class Plan_dml_row
 * @brief Row of lvalue columns in insert or update
 */
class Plan_dml_row : public Plan_base {
public:
    Plan_dml_row(Plan_root* root);
    virtual ~Plan_dml_row();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    unsigned getSize() const;
    void addColumn(Plan_dml_column* column);
    Plan_dml_column* getColumn(unsigned i) const;
protected:
    DmlColumnVector m_columnList;
};

inline
Plan_dml_row::Plan_dml_row(Plan_root* root) :
    Plan_base(root),
    m_columnList(1)
{
}

// children

inline unsigned
Plan_dml_row::getSize() const
{
    return m_columnList.size() - 1;
}

inline void
Plan_dml_row::addColumn(Plan_dml_column* column)
{
    ctx_assert(column != 0);
    m_columnList.push_back(column);
}

inline Plan_dml_column*
Plan_dml_row::getColumn(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_columnList.size() && m_columnList[i] != 0);
    return m_columnList[i];
}

#endif
