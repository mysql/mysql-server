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

#ifndef ODBC_CODEGEN_Code_idx_column_hpp
#define ODBC_CODEGEN_Code_idx_column_hpp

#include <common/common.hpp>
#include "Code_column.hpp"
#include "Code_data_type.hpp"
#include "Code_expr.hpp"

class DictColumn;
class Plan_table;

/**
 * @class Plan_idx_column
 * @brief Column in create index statement
 */
class Plan_idx_column : public Plan_base, public Plan_column {
public:
    Plan_idx_column(Plan_root* root, const BaseString& name);
    virtual ~Plan_idx_column();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
protected:
    friend class Plan_create_row;
};

inline
Plan_idx_column::Plan_idx_column(Plan_root* root, const BaseString& name) :
    Plan_base(root),
    Plan_column(Type_idx, name)
{
}

#endif
