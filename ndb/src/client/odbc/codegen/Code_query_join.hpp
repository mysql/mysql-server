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

#ifndef ODBC_CODEGEN_Code_query_join_hpp
#define ODBC_CODEGEN_Code_query_join_hpp

#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_table_list.hpp"
#include "Code_pred.hpp"

/**
 * @class Plan_query_join
 * @brief Filter node in PlanTree
 */
class Plan_query_join : public Plan_query {
public:
    Plan_query_join(Plan_root* root);
    virtual ~Plan_query_join();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setInner(Plan_query* query);
    void setOuter(Plan_query* query);
protected:
    Plan_query* m_inner;
    Plan_query* m_outer;
};

inline
Plan_query_join::Plan_query_join(Plan_root* root) :
    Plan_query(root),
    m_inner(0),
    m_outer(0)
{
}

// children

inline void
Plan_query_join::setInner(Plan_query* query)
{
    ctx_assert(query != 0);
    m_inner = query;
}

inline void
Plan_query_join::setOuter(Plan_query* query)
{
    ctx_assert(query != 0);
    m_outer = query;
}

/**
 * @class Exec_query_join
 * @brief Filter node in ExecTree
 */
class Exec_query_join : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs);
	virtual ~Code();
    protected:
	friend class Exec_query_join;
	SqlSpecs m_sqlSpecs;
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_join* node, const SqlRow& sqlRow);
	virtual ~Data();
    protected:
	friend class Exec_query_join;
	SqlRow m_sqlRow;
    };
    Exec_query_join(Exec_root* root);
    virtual ~Exec_query_join();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setInner(Exec_query* query);
    void setOuter(Exec_query* query);
protected:
    Exec_query* m_inner;
    Exec_query* m_outer;
};

inline
Exec_query_join::Code::Code(const SqlSpecs& sqlSpecs) :
    Exec_query::Code(m_sqlSpecs),
    m_sqlSpecs(sqlSpecs)
{
}

inline
Exec_query_join::Data::Data(Exec_query_join* node, const SqlRow& sqlRow) :
    Exec_query::Data(node, m_sqlRow),
    m_sqlRow(sqlRow)
{
}

inline
Exec_query_join::Exec_query_join(Exec_root* root) :
    Exec_query(root),
    m_inner(0),
    m_outer(0)
{
}

// children

inline const Exec_query_join::Code&
Exec_query_join::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_join::Data&
Exec_query_join::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_join::setInner(Exec_query* query)
{
    ctx_assert(query != 0);
    m_inner = query;
}

inline void
Exec_query_join::setOuter(Exec_query* query)
{
    ctx_assert(query != 0);
    m_outer = query;
}

#endif
