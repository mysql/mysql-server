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

#ifndef ODBC_CODEGEN_Code_query_project_hpp
#define ODBC_CODEGEN_Code_query_project_hpp

#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_expr_row.hpp"

/**
 * @class Plan_query_project
 * @brief Project node in PlanTree
 */
class Plan_query_project : public Plan_query {
public:
    Plan_query_project(Plan_root* root);
    virtual ~Plan_query_project();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setQuery(Plan_query* query);
    void setRow(Plan_expr_row* exprRow);
    void setLimit(int off, int cnt);
protected:
    Plan_expr_row* getRow();
    Plan_query* m_query;
    Plan_expr_row* m_exprRow;
    int m_limitOff;
    int m_limitCnt;
};

inline
Plan_query_project::Plan_query_project(Plan_root* root) :
    Plan_query(root),
    m_query(0),
    m_exprRow(0),
    m_limitOff(0),
    m_limitCnt(-1)
{
}

// children

inline void
Plan_query_project::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Plan_query_project::setRow(Plan_expr_row* exprRow)
{
    ctx_assert(exprRow != 0);
    m_exprRow = exprRow;
}

inline void
Plan_query_project::setLimit(int off, int cnt)
{
    m_limitOff = off;
    m_limitCnt = cnt;
}

/**
 * @class Exec_query_project
 * @brief Project node in ExecTree
 */
class Exec_query_project : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs);
	virtual ~Code();
    protected:
	friend class Plan_query_project;
	friend class Exec_query_project;
	// sets reference to Sqlspecs from the row
	int m_limitOff;
	int m_limitCnt;
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_project* node, const SqlRow& sqlRow);
	virtual ~Data();
    protected:
	friend class Exec_query_project;
	// sets reference to SqlRow from the row
	unsigned m_cnt;
    };
    Exec_query_project(Exec_root* root);
    virtual ~Exec_query_project();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setQuery(Exec_query* query);
    void setRow(Exec_expr_row* exprRow);
    const Exec_query* getRawQuery() const;
protected:
    friend class Exec_query;
    Exec_query* m_query;
    Exec_expr_row* m_exprRow;
};

inline
Exec_query_project::Code::Code(const SqlSpecs& sqlSpecs) :
    Exec_query::Code(sqlSpecs),
    m_limitOff(0),
    m_limitCnt(-1)
{
}

inline
Exec_query_project::Data::Data(Exec_query_project* node, const SqlRow& sqlRow) :
    Exec_query::Data(node, sqlRow),
    m_cnt(0)
{
}

inline
Exec_query_project::Exec_query_project(Exec_root* root) :
    Exec_query(root),
    m_query(0),
    m_exprRow(0)
{
}

// children

inline const Exec_query_project::Code&
Exec_query_project::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_project::Data&
Exec_query_project::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_project::setQuery(Exec_query* query)
{
    ctx_assert(m_query == 0 && query != 0);
    m_query = query;
}

inline void
Exec_query_project::setRow(Exec_expr_row* exprRow)
{
    ctx_assert(m_exprRow == 0 && exprRow != 0);
    m_exprRow = exprRow;
}

#endif
