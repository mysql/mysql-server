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

#ifndef ODBC_CODEGEN_Code_ddl_hpp
#define ODBC_CODEGEN_Code_ddl_hpp

#include <common/common.hpp>
#include "Code_stmt.hpp"

/**
 * @class Plan_ddl
 * @brief Base class for DDL statements in PlanTree
 */
class Plan_ddl : public Plan_stmt {
public:
    Plan_ddl(Plan_root* root);
    virtual ~Plan_ddl() = 0;
};

inline
Plan_ddl::Plan_ddl(Plan_root* root) :
    Plan_stmt(root)
{
}

/**
 * @class Exec_ddl
 * @brief Base class for DDL statements in ExecTree
 */
class Exec_ddl : public Exec_stmt {
public:
    class Code : public Exec_stmt::Code {
    public:
	virtual ~Code() = 0;
    };
    class Data : public Exec_stmt::Data {
    public:
	virtual ~Data() = 0;
    };
    Exec_ddl(Exec_root* root);
    virtual ~Exec_ddl() = 0;
};

inline
Exec_ddl::Exec_ddl(Exec_root* root) :
    Exec_stmt(root)
{
}

#endif
