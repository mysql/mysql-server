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

#ifndef ODBC_CODEGEN_Code_root_hpp
#define ODBC_CODEGEN_Code_root_hpp

#include <list>
#include <common/common.hpp>
#include "Code_base.hpp"
#include "Code_stmt.hpp"

class SqlField;
class ExtField;

/**
 * @class Plan_root
 * @brief Root node above top level statement node
 */
class Plan_root : public Plan_base {
public:
    Plan_root(StmtArea& stmtArea);
    virtual ~Plan_root();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    void describe(Ctx& ctx);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setStmt(Plan_stmt* stmt);
    // save and free nodes
    void saveNode(Plan_base* node);
    void freeNodeList();
private:
    friend class CodeGen;
    friend class Plan_base;
    friend class Plan_expr_param;
    StmtArea& m_stmtArea;
    Plan_stmt* m_stmt;
    ParamVector m_paramList;
    typedef std::list<Plan_base*> NodeList;
    NodeList m_nodeList;
};

inline
Plan_root::Plan_root(StmtArea& stmtArea) :
    Plan_base(this),
    m_stmtArea(stmtArea),
    m_stmt(0)
{
}

inline void
Plan_root::setStmt(Plan_stmt* stmt)
{
    ctx_assert(stmt != 0);
    m_stmt = stmt;
}

/**
 * @class Exec_root
 * @brief Root node above top level statement node
 */
class Exec_root : public Exec_base {
public:
    class Code : public Exec_base::Code {
    public:
	Code();
	virtual ~Code();
    };
    class Data : public Exec_base::Data {
    public:
	Data();
	virtual ~Data();
    };
    Exec_root(StmtArea& stmtArea);
    virtual ~Exec_root();
    StmtArea& stmtArea() const;
    void alloc(Ctx& ctx, Ctl& ctl);
    void bind(Ctx& ctx);
    void execute(Ctx& ctx, Ctl& ctl);
    void fetch(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setStmt(Exec_stmt* stmt);
    // save and free nodes
    void saveNode(Exec_base* node);
    void freeNodeList();
    // odbc support
    void sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind);
    void sqlParamData(Ctx& ctx, SQLPOINTER* value);
    void sqlPutData(Ctx& ctx, SQLPOINTER data, SQLINTEGER strlen_or_Ind);
private:
    friend class Plan_root;
    friend class Exec_base;
    friend class CodeGen;
    StmtArea& m_stmtArea;
    Exec_stmt* m_stmt;
    ParamVector m_paramList;
    unsigned m_paramData;		// position of SQLParamData
    typedef std::list<Exec_base*> NodeList;
    NodeList m_nodeList;
};

inline
Exec_root::Code::Code()
{
}

inline
Exec_root::Data::Data()
{
}

inline
Exec_root::Exec_root(StmtArea& stmtArea) :
    Exec_base(this),
    m_stmtArea(stmtArea),
    m_stmt(0),
    m_paramData(0)
{
}

// children

inline const Exec_root::Code&
Exec_root::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_root::Data&
Exec_root::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_root::setStmt(Exec_stmt* stmt)
{
    ctx_assert(stmt != 0);
    m_stmt = stmt;
    m_stmt->m_topLevel = true;
}

#endif
