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

#ifndef ODBC_CODEGEN_Code_table_hpp
#define ODBC_CODEGEN_Code_table_hpp

#include <vector>
#include <common/common.hpp>
#include "Code_base.hpp"

class DictTable;
class DictColumn;
class DictIndex;
class Plan_query_filter;
class Plan_query_lookup;
class Plan_query_range;
class Plan_column;
class Plan_expr_column;
class Plan_select;
class Plan_delete;
class Plan_delete_lookup;
class Plan_update;
class Plan_update_lookup;

/**
 * @class Plan_table
 * @brief Table node in PlanTree
 *
 * This is a pure Plan node.  Final executable nodes have table
 * information built-in.
 */
class Plan_table : public Plan_base {
public:
    Plan_table(Plan_root* root, const BaseString& name);
    virtual ~Plan_table();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // attributes
    const BaseString& getName() const;
    const BaseString& getCname() const;
    const char* getPrintName() const;
    void setCname(const BaseString& cname);
    const DictTable& dictTable() const;
    unsigned indexCount() const;
    // resolve
    const ColumnVector& exprColumns() const;
    const ColumnVector& dmlColumns() const;
protected:
    friend class Plan_column;
    friend class Plan_query_filter;
    friend class Plan_query_lookup;
    friend class Plan_query_index;
    friend class Plan_query_range;
    friend class Plan_expr_column;
    friend class Plan_select;
    friend class Plan_delete;
    friend class Plan_delete_lookup;
    friend class Plan_delete_index;
    friend class Plan_update;
    friend class Plan_update_lookup;
    friend class Plan_update_index;
    BaseString m_name;
    BaseString m_cname;
    BaseString m_printName;
    DictTable* m_dictTable;
    /*
     * Resolve column.  Returns 1 on found, 0 on not found, and -1 on error.
     * Modifies both table and column data.
     */
    int resolveColumn(Ctx& ctx, Plan_column* column, bool stripSchemaName = false);
    ColumnVector m_exprColumns;
    ColumnVector m_dmlColumns;
    /*
     * Offset for resolved columns in join.  This is sum over m_exprColumns
     * lengths for all preceding tables.
     */
    unsigned m_resOff;
    /*
     * Each column in primary key and unique hash index has list of
     * expressions it is set equal to in the where-clause (at top level).
     */
    bool resolveEq(Ctx& ctx, Plan_expr_column* column, Plan_expr* expr);
    /*
     * Index struct for primary key and indexes.
     */
    struct Index {
	Index() :
	    m_pos(0),
	    m_keyFound(false),
	    m_dictIndex(0),
	    m_rank(~0),
	    m_keyCount(0),
	    m_keyCountUsed(0) {
	}
	unsigned m_pos;
	ExprListVector m_keyEqList;
	bool m_keyFound;
	ExprVector m_keyEq;
	TableSet m_keySet;
	const DictIndex* m_dictIndex;		// for index only
	unsigned m_rank;			// 0-pk 1-hash index 2-ordered index
	unsigned m_keyCount;			// number of columns
	unsigned m_keyCountUsed;		// may be less for ordered index
	unsigned m_keyCountUnused;		// m_keyCount - m_keyCountUsed
    };
    typedef std::vector<Index> IndexList;	// primary key is entry 0
    IndexList m_indexList;
    /*
     * Find set of additional tables (maybe empty) required to resolve the key
     * columns.
     */
    void resolveSet(Ctx& ctx, Index& index, const TableSet& tsDone);
    void resolveSet(Ctx& ctx, Index& index, const TableSet& tsDone, ExprVector& keyEq, unsigned n);
    /*
     * Check for exactly one key or index match.
     */
    bool exactKey(Ctx& ctx, const Index* indexKey) const;
};

inline
Plan_table::Plan_table(Plan_root* root, const BaseString& name) :
    Plan_base(root),
    m_name(name),
    m_printName(name),
    m_dictTable(0),
    m_exprColumns(1),		// 1-based
    m_dmlColumns(1),		// 1-based
    m_resOff(0),
    m_indexList(1)
{
}

inline const BaseString&
Plan_table::getName() const
{
    return m_name;
}

inline const BaseString&
Plan_table::getCname() const
{
    return m_cname;
}

inline const char*
Plan_table::getPrintName() const
{
    return m_printName.c_str();
}

inline void
Plan_table::setCname(const BaseString& cname)
{
    m_cname.assign(cname);
    m_printName.assign(m_name);
    if (! m_cname.empty()) {
	m_printName.append(" ");
	m_printName.append(m_cname);
    }
}

inline const DictTable&
Plan_table::dictTable() const
{
    ctx_assert(m_dictTable != 0);
    return *m_dictTable;
}

inline unsigned
Plan_table::indexCount() const
{
    ctx_assert(m_indexList.size() > 0);
    return m_indexList.size() - 1;
}

inline const Plan_table::ColumnVector&
Plan_table::exprColumns() const
{
    return m_exprColumns;
}

inline const Plan_table::ColumnVector&
Plan_table::dmlColumns() const
{
    return m_dmlColumns;
}

#endif
