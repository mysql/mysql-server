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

#ifndef ODBC_CODEGEN_Code_table_list_hpp
#define ODBC_CODEGEN_Code_table_list_hpp

#include <common/common.hpp>
#include "Code_base.hpp"
#include "Code_table.hpp"

/**
 * @class Plan_table_list
 * @brief List of tables in select statement
 */
class Plan_table_list : public Plan_base {
public:
    Plan_table_list(Plan_root* root);
    virtual ~Plan_table_list();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    unsigned countTable() const;
    void addTable(Plan_table* table);
    Plan_table* getTable(unsigned i) const;
protected:
    friend class Plan_select;
    TableVector m_tableList;
};

inline
Plan_table_list::Plan_table_list(Plan_root* root) :
    Plan_base(root),
    m_tableList(1)
{
}

// children

inline unsigned
Plan_table_list::countTable() const
{
    return m_tableList.size() - 1;
}

inline void
Plan_table_list::addTable(Plan_table* table)
{
    ctx_assert(table != 0);
    m_tableList.push_back(table);
}

inline Plan_table*
Plan_table_list::getTable(unsigned i) const
{
    ctx_assert(1 <= i && i <= countTable() && m_tableList[i] != 0);
    return m_tableList[i];
}

#endif
