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

#ifndef ODBC_CODEGEN_CodeTree_hpp
#define ODBC_CODEGEN_CodeTree_hpp

#include <common/common.hpp>

/*
 * Intermediary statement evalution plan.  Starts as parse tree.  Final
 * format maps directly to ExecTree.
 */
class PlanTree {
public:
    virtual ~PlanTree() = 0;
};

/*
 * Executable code and runtime data.  Currently looks like PlanTree.
 * Later may change code format to linear "byte code" and move execution
 * to a SQL block in NDB kernel.
 */
class ExecTree {
public:
    class Code {
    public:
	virtual ~Code() = 0;
    };
    class Data {
    public:
	virtual ~Data() = 0;
    };
    virtual ~ExecTree() = 0;
};

#endif
