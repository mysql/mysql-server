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

#ifndef ODBC_CODEGEN_Code_stmt_hpp
#define ODBC_CODEGEN_Code_stmt_hpp

#include <common/common.hpp>
#include <common/DataType.hpp>
#include "Code_base.hpp"

class Ctx;

/**
 * @class Plan_stmt
 * @brief Base class for statements in PlanTree
 *
 * A statement is a complete or partial SQL statement which can
 * be optimized into executable statements Exec_stmt.
 */
class Plan_stmt : public Plan_base {
public:
    Plan_stmt(Plan_root* root);
    virtual ~Plan_stmt() = 0;
    virtual void describe(Ctx& ctx);
};

inline
Plan_stmt::Plan_stmt(Plan_root* root) :
    Plan_base(root)
{
}

/**
 * @class Exec_stmt
 * @brief Base class for statements in ExecTree
 */
class Exec_stmt : public Exec_base {
public:
    class Code : public Exec_base::Code {
    public:
	virtual ~Code() = 0;
    };
    class Data : public Exec_base::Data {
    public:
	virtual ~Data() = 0;
    };
    Exec_stmt(Exec_root* root);
    virtual ~Exec_stmt() = 0;
    virtual void bind(Ctx& ctx);
    virtual void execute(Ctx& ctx, Ctl& ctl) = 0;
protected:
    friend class Exec_root;
    bool m_topLevel;
};

inline
Exec_stmt::Exec_stmt(Exec_root* root) :
    Exec_base(root),
    m_topLevel(false)
{
}

#endif
