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

#ifndef ODBC_CODEGEN_Code_query_distinct_hpp
#define ODBC_CODEGEN_Code_query_distinct_hpp

#include <functional>
#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_expr_row.hpp"
#include "Code_pred.hpp"

/**
 * @class Plan_query_distinct
 * @brief Group-by node in PlanTree
 */
class Plan_query_distinct : public Plan_query {
public:
    Plan_query_distinct(Plan_root* root);
    virtual ~Plan_query_distinct();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setQuery(Plan_query* query);
    Plan_expr_row* getRow();
protected:
    Plan_query* m_query;
};

inline
Plan_query_distinct::Plan_query_distinct(Plan_root* root) :
    Plan_query(root),
    m_query(0)
{
}

// children

inline void
Plan_query_distinct::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

/**
 * Distinct preserves order of input rows so we use 2 data structures:
 * map<row> = index and vector<index> = row (index >= 0).
 */

class Exec_query_distinct;

struct DistinctLess : std::binary_function<const SqlRow*, const SqlRow*, bool> {
    bool operator()(const SqlRow* s1, const SqlRow* s2) const;
};

typedef std::map<const SqlRow*, unsigned, DistinctLess> DistinctList;

typedef std::vector<const SqlRow*> DistinctVector;

/**
 * @class Exec_query_distinct
 * @brief Group-by node in ExecTree
 */
class Exec_query_distinct : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs);
	virtual ~Code();
    protected:
	friend class Exec_query_distinct;
	// sets reference to Sqlspecs from subquery
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_distinct* node, const SqlSpecs& sqlSpecs);
	virtual ~Data();
    protected:
	friend class Exec_query_distinct;
	SqlRow m_sqlRow;	// current row
	bool m_grouped;		// fetch and group-by done
	unsigned m_count;
	DistinctList m_groupList;
	DistinctVector m_groupVector;
	unsigned m_index;
    };
    Exec_query_distinct(Exec_root* root);
    virtual ~Exec_query_distinct();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setQuery(Exec_query* query);
    const Exec_query* getRawQuery() const;
protected:
    friend class Exec_query;
    Exec_query* m_query;
};

inline
Exec_query_distinct::Code::Code(const SqlSpecs& sqlSpecs) :
    Exec_query::Code(sqlSpecs)
{
}

inline
Exec_query_distinct::Data::Data(Exec_query_distinct* node, const SqlSpecs& sqlSpecs) :
    Exec_query::Data(node, m_sqlRow),
    m_sqlRow(sqlSpecs),
    m_grouped(false),
    m_count(0),
    m_index(0)
{
}

inline
Exec_query_distinct::Exec_query_distinct(Exec_root* root) :
    Exec_query(root),
    m_query(0)
{
}

// children

inline const Exec_query_distinct::Code&
Exec_query_distinct::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_distinct::Data&
Exec_query_distinct::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_distinct::setQuery(Exec_query* query)
{
    ctx_assert(m_query == 0 && query != 0);
    m_query = query;
}

#endif
