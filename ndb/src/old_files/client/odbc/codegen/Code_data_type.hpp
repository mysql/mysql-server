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

#ifndef ODBC_CODEGEN_Code_data_type_hpp
#define ODBC_CODEGEN_Code_data_type_hpp

#include <common/common.hpp>
#include <common/DataType.hpp>
#include "Code_base.hpp"

/**
 * @class Plan_data_type
 * @brief Data type in DDL statement
 *
 * This is pure plan node.
 */
class Plan_data_type : public Plan_base {
public:
    Plan_data_type(Plan_root* root, const SqlType& sqlType);
    virtual ~Plan_data_type();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
private:
    friend class Plan_ddl_column;
    SqlType m_sqlType;
};

inline
Plan_data_type::Plan_data_type(Plan_root* root, const SqlType& sqlType) :
    Plan_base(root),
    m_sqlType(sqlType)
{
}

#endif
