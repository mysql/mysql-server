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

#ifndef ODBC_CODEGEN_Code_dml_hpp
#define ODBC_CODEGEN_Code_dml_hpp

#include <common/common.hpp>
#include <common/ResultArea.hpp>
#include "Code_stmt.hpp"

/**
 * @class Plan_dml
 * @brief Base class for DML statements in PlanTree
 */
class Plan_dml : public Plan_stmt {
public:
    Plan_dml(Plan_root* root);
    virtual ~Plan_dml() = 0;
};

inline
Plan_dml::Plan_dml(Plan_root* root) :
    Plan_stmt(root)
{
}

/**
 * @class Exec_dml
 * @brief Base class for DML statements in ExecTree
 */
class Exec_dml : public Exec_stmt {
public:
    class Code : public Exec_stmt::Code {
    public:
	virtual ~Code() = 0;
    };
    class Data : public Exec_stmt::Data, public ResultArea {
    public:
	virtual ~Data() = 0;
    };
    Exec_dml(Exec_root* root);
    virtual ~Exec_dml() = 0;
    void execute(Ctx& ctx, Ctl& ctl);
protected:
    virtual void execImpl(Ctx& ctx, Ctl& ctl) = 0;
};

inline
Exec_dml::Exec_dml(Exec_root* root) :
    Exec_stmt(root)
{
}

#endif
