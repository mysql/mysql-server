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

#ifndef ODBC_CODEGEN_Code_query_filter_hpp
#define ODBC_CODEGEN_Code_query_filter_hpp

#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_table_list.hpp"
#include "Code_pred.hpp"

/**
 * @class Plan_query_filter
 * @brief Filter node in PlanTree
 */
class Plan_query_filter : public Plan_query {
public:
    Plan_query_filter(Plan_root* root);
    virtual ~Plan_query_filter();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setQuery(Plan_query* query);
    void setPred(Plan_pred* pred);
protected:
    friend class Plan_select;
    friend class Plan_update;
    friend class Plan_delete;
    Plan_query* m_query;
    Plan_pred* m_pred;
    Plan_table* m_topTable;	// top level table for interpreted progs
};

inline
Plan_query_filter::Plan_query_filter(Plan_root* root) :
    Plan_query(root),
    m_query(0),
    m_pred(0),
    m_topTable(0)
{
}

// children

inline void
Plan_query_filter::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Plan_query_filter::setPred(Plan_pred* pred)
{
    ctx_assert(pred != 0);
    m_pred = pred;
}

/**
 * @class Exec_query_filter
 * @brief Filter node in ExecTree
 */
class Exec_query_filter : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs);
	virtual ~Code();
    protected:
	friend class Exec_query_filter;
	// sets reference to SqlSpecs from subquery
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_filter* node, const SqlRow& sqlRow);
	virtual ~Data();
    protected:
	friend class Exec_query_filter;
	// sets reference to SqlRow from subquery
    };
    Exec_query_filter(Exec_root* root);
    virtual ~Exec_query_filter();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setQuery(Exec_query* query);
    void setPred(Exec_pred* pred);
protected:
    Exec_query* m_query;
    Exec_pred* m_pred;
};

inline
Exec_query_filter::Code::Code(const SqlSpecs& sqlSpecs) :
    Exec_query::Code(sqlSpecs)
{
}

inline
Exec_query_filter::Data::Data(Exec_query_filter* node, const SqlRow& sqlRow) :
    Exec_query::Data(node, sqlRow)
{
}

inline
Exec_query_filter::Exec_query_filter(Exec_root* root) :
    Exec_query(root),
    m_query(0),
    m_pred(0)
{
}

// children

inline const Exec_query_filter::Code&
Exec_query_filter::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_filter::Data&
Exec_query_filter::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_filter::setQuery(Exec_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Exec_query_filter::setPred(Exec_pred* pred)
{
    ctx_assert(pred != 0);
    m_pred = pred;
}

#endif
