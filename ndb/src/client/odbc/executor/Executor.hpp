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

#ifndef ODBC_EXECUTOR_Executor_hpp
#define ODBC_EXECUTOR_Executor_hpp

#include <common/common.hpp>
class StmtArea;

/**
 * @class Executor
 * @brief Executes an ExecTree
 */
class Executor {
public:
    Executor(StmtArea& stmtArea);
    ~Executor();
    // execute prepared statement
    void execute(Ctx& ctx);
    // fetch next row from query
    void fetch(Ctx& ctx);
private:
    // rebind if necessary
    void rebind(Ctx& ctx);
    StmtArea m_stmtArea;
};

inline
Executor::Executor(StmtArea& stmtArea) :
    m_stmtArea(stmtArea)
{
}

inline
Executor::~Executor()
{
}

#endif
