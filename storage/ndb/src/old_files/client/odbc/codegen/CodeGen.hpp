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

#ifndef ODBC_CODEGEN_CodeGen_hpp
#define ODBC_CODEGEN_CodeGen_hpp

#include <common/common.hpp>

class StmtArea;
class SqlField;
class ExtField;

/**
 * @class CodeGen
 * @brief Compiles SQL text into ExecTree::Code
 */
class CodeGen {
public:
    CodeGen(StmtArea& stmtArea);
    ~CodeGen();
    // parse and analyze SQL statement
    void prepare(Ctx& ctx);
    // these are passed to Executor
    void execute(Ctx& ctx);
    void fetch(Ctx& ctx);
    // close statement (mainly scan)
    void close(Ctx& ctx);
    // free data structures
    void free(Ctx& ctx);
    // odbc support
    void sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind);
    void sqlParamData(Ctx& ctx, SQLPOINTER* value);
    void sqlPutData(Ctx& ctx, SQLPOINTER data, SQLINTEGER strlen_or_Ind);
private:
    void parse(Ctx& ctx);
    void analyze(Ctx& ctx);
    void describe(Ctx& ctx);
    void codegen(Ctx& ctx);
    void alloc(Ctx& ctx);
    void freePlan(Ctx& ctx);
    void freeExec(Ctx& ctx);
    StmtArea& m_stmtArea;
};

inline
CodeGen::CodeGen(StmtArea& stmtArea) :
    m_stmtArea(stmtArea)
{
}

inline
CodeGen::~CodeGen()
{
}

#endif
