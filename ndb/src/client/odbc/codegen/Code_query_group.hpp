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

#ifndef ODBC_CODEGEN_Code_query_group_hpp
#define ODBC_CODEGEN_Code_query_group_hpp

#include <functional>
#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_expr_row.hpp"
#include "Code_pred.hpp"

/**
 * @class Plan_query_group
 * @brief Group-by node in PlanTree
 */
class Plan_query_group : public Plan_query {
public:
    Plan_query_group(Plan_root* root);
    virtual ~Plan_query_group();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setQuery(Plan_query* query);
    void setDataRow(Plan_expr_row* dataRow);
    void setGroupRow(Plan_expr_row* groupRow);
    void setHavingPred(Plan_pred* havingPred);
    Plan_expr_row* getRow();
protected:
    Plan_query* m_query;
    Plan_expr_row* m_dataRow;
    Plan_expr_row* m_groupRow;
    Plan_pred* m_havingPred;
};

inline
Plan_query_group::Plan_query_group(Plan_root* root) :
    Plan_query(root),
    m_query(0),
    m_dataRow(0),
    m_groupRow(0),
    m_havingPred(0)
{
}

// children

inline void
Plan_query_group::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Plan_query_group::setDataRow(Plan_expr_row* dataRow)
{
    ctx_assert(dataRow != 0);
    m_dataRow = dataRow;
}

inline void
Plan_query_group::setGroupRow(Plan_expr_row* groupRow)
{
    ctx_assert(groupRow != 0);
    m_groupRow = groupRow;
}

inline void
Plan_query_group::setHavingPred(Plan_pred* havingPred)
{
    ctx_assert(havingPred != 0);
    m_havingPred = havingPred;
}

/**
 * Group-by uses a std::map.  Key is values grouped by.  Data is unique index
 * (starting at 1) into arrays in expression data.
 */

class Exec_query_group;

struct GroupLess : std::binary_function<const SqlRow*, const SqlRow*, bool> {
    bool operator()(const SqlRow* s1, const SqlRow* s2) const;
};

typedef std::map<const SqlRow*, unsigned, GroupLess> GroupList;

/**
 * @class Exec_query_group
 * @brief Group-by node in ExecTree
 */
class Exec_query_group : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs);
	virtual ~Code();
    protected:
	friend class Exec_query_group;
	// sets reference to Sqlspecs from subquery
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_group* node, const SqlSpecs& sqlSpecs);
	virtual ~Data();
    protected:
	friend class Exec_query_group;
	SqlRow m_sqlRow;	// current row
	bool m_grouped;		// fetch and group-by done
	unsigned m_count;
	GroupList m_groupList;
	GroupList::iterator m_iterator;
    };
    Exec_query_group(Exec_root* root);
    virtual ~Exec_query_group();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setQuery(Exec_query* query);
    void setDataRow(Exec_expr_row* dataRow);
    void setGroupRow(Exec_expr_row* groupRow);
    void setHavingPred(Exec_pred* havingPred);
    const Exec_query* getRawQuery() const;
protected:
    friend class Exec_query;
    Exec_query* m_query;
    Exec_expr_row* m_dataRow;
    Exec_expr_row* m_groupRow;
    Exec_pred* m_havingPred;
};

inline
Exec_query_group::Code::Code(const SqlSpecs& sqlSpecs) :
    Exec_query::Code(sqlSpecs)
{
}

inline
Exec_query_group::Data::Data(Exec_query_group* node, const SqlSpecs& sqlSpecs) :
    Exec_query::Data(node, m_sqlRow),
    m_sqlRow(sqlSpecs),
    m_grouped(false),
    m_count(0)
{
}

inline
Exec_query_group::Exec_query_group(Exec_root* root) :
    Exec_query(root),
    m_query(0),
    m_dataRow(0),
    m_groupRow(0),
    m_havingPred(0)
{
}

// children

inline const Exec_query_group::Code&
Exec_query_group::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_group::Data&
Exec_query_group::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_group::setQuery(Exec_query* query)
{
    ctx_assert(m_query == 0 && query != 0);
    m_query = query;
}

inline void
Exec_query_group::setDataRow(Exec_expr_row* dataRow)
{
    ctx_assert(m_dataRow == 0 && dataRow != 0);
    m_dataRow = dataRow;
}

inline void
Exec_query_group::setGroupRow(Exec_expr_row* groupRow)
{
    ctx_assert(m_groupRow == 0 && groupRow != 0);
    m_groupRow = groupRow;
}

inline void
Exec_query_group::setHavingPred(Exec_pred* havingPred)
{
    ctx_assert(m_havingPred == 0 && havingPred != 0);
    m_havingPred = havingPred;
}

#endif
