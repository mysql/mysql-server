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

#ifndef ODBC_CODEGEN_Code_query_count_hpp
#define ODBC_CODEGEN_Code_query_count_hpp

#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_expr_row.hpp"
#include "Code_root.hpp"

class Ctx;

/**
 * @class Plan_query_count
 * @brief Select count and other aggregates (no group by)
 */
class Plan_query_count : public Plan_query {
public:
    Plan_query_count(Plan_root* root);
    virtual ~Plan_query_count();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setQuery(Plan_query* query);
    void setRow(Plan_expr_row* exprRow);
protected:
    Plan_expr_row* getRow();
    Plan_query* m_query;
    Plan_expr_row* m_exprRow;
};

inline
Plan_query_count::Plan_query_count(Plan_root* root) :
    Plan_query(root),
    m_query(0),
    m_exprRow(0)
{
}

// children

inline void
Plan_query_count::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Plan_query_count::setRow(Plan_expr_row* exprRow)
{
    ctx_assert(exprRow != 0);
    m_exprRow = exprRow;
}

/**
 * @class Exec_query_count
 * @brief Select count and other aggregates (no group by)
 */
class Exec_query_count : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs);
	virtual ~Code();
    protected:
	friend class Exec_query_count;
	// sets reference to Sqlspecs from the row
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_count* node, const SqlRow& sqlRow);
	virtual ~Data();
    protected:
	friend class Exec_query_count;
	// sets reference to SqlRow from the row
	bool m_done;		// returns one row
    };
    Exec_query_count(Exec_root* root);
    virtual ~Exec_query_count();
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
    Exec_query* m_query;
    Exec_expr_row* m_exprRow;
};

inline
Exec_query_count::Code::Code(const SqlSpecs& sqlSpecs) :
    Exec_query::Code(sqlSpecs)
{
}

inline
Exec_query_count::Data::Data(Exec_query_count* node, const SqlRow& sqlRow) :
    Exec_query::Data(node, sqlRow)
{
}

inline
Exec_query_count::Exec_query_count(Exec_root* root) :
    Exec_query(root),
    m_query(0),
    m_exprRow(0)
{
}

// children

inline const Exec_query_count::Code&
Exec_query_count::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_count::Data&
Exec_query_count::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_count::setQuery(Exec_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Exec_query_count::setRow(Exec_expr_row* exprRow)
{
    ctx_assert(m_exprRow == 0 && exprRow != 0);
    m_exprRow = exprRow;
}

#endif
