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

#ifndef ODBC_CODEGEN_Code_query_sort_hpp
#define ODBC_CODEGEN_Code_query_sort_hpp

#include <functional>
#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_expr_row.hpp"

/**
 * @class Plan_query_sort
 * @brief Project node in PlanTree
 */
class Plan_query_sort : public Plan_query {
public:
    Plan_query_sort(Plan_root* root);
    virtual ~Plan_query_sort();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setQuery(Plan_query* query);
    void setRow(Plan_expr_row* sortRow);
protected:
    Plan_expr_row* getRow();
    Plan_query* m_query;
    Plan_expr_row* m_sortRow;
};

inline
Plan_query_sort::Plan_query_sort(Plan_root* root) :
    Plan_query(root),
    m_query(0),
    m_sortRow(0)
{
}

// children

inline void
Plan_query_sort::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Plan_query_sort::setRow(Plan_expr_row* sortRow)
{
    ctx_assert(sortRow != 0);
    m_sortRow = sortRow;
}

/**
 * Item to sort includes data row and sort row.
 */
struct SortItem {
    SortItem(const SqlRow* dataRow, const SqlRow* sortRow);
    const SqlRow* m_dataRow;	// copy of fetched row from subquery
    const SqlRow* m_sortRow;	// copy of values to sort on
};

typedef std::vector<SortItem> SortList;

class Exec_query_sort;

struct SortLess : std::binary_function<SortItem, SortItem, bool> {
    SortLess(const Exec_query_sort* node);
    const Exec_query_sort* m_node;
    bool operator()(SortItem s1, SortItem s2) const;
};

inline
SortItem::SortItem(const SqlRow* dataRow, const SqlRow* sortRow) :
    m_dataRow(dataRow),
    m_sortRow(sortRow)
{
}

inline
SortLess::SortLess(const Exec_query_sort* node) :
    m_node(node)
{
}

/**
 * @class Exec_query_sort
 * @brief Project node in ExecTree
 */
class Exec_query_sort : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs, bool* asc);
	virtual ~Code();
	bool getAsc(unsigned i) const;
    protected:
	friend class Exec_query_sort;
	const bool* const m_asc;
	// sets reference to Sqlspecs from subquery
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_sort* node, const SqlSpecs& sqlSpecs);
	virtual ~Data();
    protected:
	friend class Exec_query_sort;
	SqlRow m_sqlRow;	// current row
	bool m_sorted;		// fetch and sort done
	SortList m_sortList;
	unsigned m_count;	// number of rows
	unsigned m_index;	// current fetch index
    };
    Exec_query_sort(Exec_root* root);
    virtual ~Exec_query_sort();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setQuery(Exec_query* query);
    void setRow(Exec_expr_row* sortRow);
protected:
    friend class Exec_query;
    Exec_query* m_query;
    Exec_expr_row* m_sortRow;
};

inline
Exec_query_sort::Code::Code(const SqlSpecs& sqlSpecs, bool* asc) :
    Exec_query::Code(sqlSpecs),
    m_asc(asc)
{
}

inline bool
Exec_query_sort::Code::getAsc(unsigned i) const
{
    return m_asc[i];
}

inline
Exec_query_sort::Data::Data(Exec_query_sort* node, const SqlSpecs& sqlSpecs) :
    Exec_query::Data(node, m_sqlRow),
    m_sqlRow(sqlSpecs),
    m_sorted(false),
    m_count(0),
    m_index(0)
{
}

inline
Exec_query_sort::Exec_query_sort(Exec_root* root) :
    Exec_query(root),
    m_query(0),
    m_sortRow(0)
{
}

// children

inline const Exec_query_sort::Code&
Exec_query_sort::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_sort::Data&
Exec_query_sort::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_sort::setQuery(Exec_query* query)
{
    ctx_assert(m_query == 0 && query != 0);
    m_query = query;
}

inline void
Exec_query_sort::setRow(Exec_expr_row* sortRow)
{
    ctx_assert(m_sortRow == 0 && sortRow != 0);
    m_sortRow = sortRow;
}

#endif
