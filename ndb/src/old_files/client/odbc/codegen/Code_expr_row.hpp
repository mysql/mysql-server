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

#ifndef ODBC_CODEGEN_Code_expr_row_hpp
#define ODBC_CODEGEN_Code_expr_row_hpp

#include <vector>
#include <common/common.hpp>
#include <common/DataRow.hpp>
#include "Code_base.hpp"
#include "Code_expr.hpp"

class Plan_expr;

/**
 * @class Plan_expr_row
 * @brief Row of expressions in PlanTree
 *
 * Used for select, value, and order by rows.
 */
class Plan_expr_row : public Plan_base {
public:
    Plan_expr_row(Plan_root* root);
    virtual ~Plan_expr_row();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setAsterisk();
    bool getAsterisk() const;
    unsigned getSize() const;
    Plan_expr* getExpr(unsigned i) const;
    void setExpr(unsigned i, Plan_expr* expr);
    void addExpr(Plan_expr* expr);
    void addExpr(Plan_expr* expr, const BaseString& alias);
    void setAlias(unsigned i, const BaseString& alias);
    void addExpr(Plan_expr* expr, bool asc);
    bool anyAggr() const;
    bool allBound() const;
    bool isAllGroupBy(const Plan_expr_row* row) const;
protected:
    friend class Plan_query;
    friend class Plan_query_sort;
    bool m_asterisk;		// early plan node type
    ExprVector m_exprList;
    typedef std::vector<BaseString> AliasList;
    AliasList m_aliasList;
    typedef std::vector<bool> AscList;
    AscList m_ascList;
    bool m_anyAggr;		// at least one aggreate
    bool m_allBound;		// all bound
};

inline
Plan_expr_row::Plan_expr_row(Plan_root* root) :
    Plan_base(root),
    m_asterisk(false),
    m_exprList(1),
    m_aliasList(1),
    m_ascList(1),
    m_anyAggr(false),
    m_allBound(false)
{
}

// children

inline void
Plan_expr_row::setAsterisk()
{
    m_asterisk = true;
}

inline bool
Plan_expr_row::getAsterisk() const
{
    return m_asterisk;
}

inline unsigned
Plan_expr_row::getSize() const
{
    ctx_assert(m_exprList.size() >= 1);
    return m_exprList.size() - 1;
}

inline void
Plan_expr_row::addExpr(Plan_expr* expr)
{
    ctx_assert(expr != 0);
    addExpr(expr, expr->m_alias);
}

inline void
Plan_expr_row::addExpr(Plan_expr* expr, const BaseString& alias)
{
    ctx_assert(expr != 0);
    m_exprList.push_back(expr);
    m_aliasList.push_back(alias);
}

inline void
Plan_expr_row::addExpr(Plan_expr* expr, bool asc)
{
    ctx_assert(expr != 0);
    m_exprList.push_back(expr);
    m_ascList.push_back(asc);
}

inline void
Plan_expr_row::setExpr(unsigned i, Plan_expr* expr)
{
    ctx_assert(1 <= i && i < m_exprList.size() && expr != 0);
    m_exprList[i] = expr;
}

inline Plan_expr*
Plan_expr_row::getExpr(unsigned i) const
{
    ctx_assert(1 <= i && i < m_exprList.size() && m_exprList[i] != 0);
    return m_exprList[i];
}

inline void
Plan_expr_row::setAlias(unsigned i, const BaseString& alias)
{
    ctx_assert(1 <= i && i < m_aliasList.size());
    m_aliasList[i] = alias;
}

inline bool
Plan_expr_row::anyAggr() const
{
    return m_anyAggr;
}

inline bool
Plan_expr_row::allBound() const
{
    return m_allBound;
}

/**
 * @class Expr_expr_row
 * @brief Row of expressions in ExecTree
 */
class Exec_expr_row : public Exec_base {
public:
    class Code : public Exec_base::Code {
    public:
	typedef char Alias[40];
	Code(const SqlSpecs& sqlSpecs, const Alias* aliasList);
	virtual ~Code();
	const SqlSpecs& sqlSpecs() const;
	const char* getAlias(unsigned i) const;
    protected:
	friend class Exec_expr_row;
	const SqlSpecs m_sqlSpecs;
	const Alias* m_aliasList;
    };
    class Data : public Exec_base::Data {
    public:
	Data(const SqlRow& sqlRow);
	virtual ~Data();
	const SqlRow& sqlRow() const;
    protected:
	friend class Exec_expr_row;
	SqlRow m_sqlRow;
    };
    Exec_expr_row(Exec_root* root, unsigned size);
    virtual ~Exec_expr_row();
    void alloc(Ctx& ctx, Ctl& ctl);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    Exec_expr* getExpr(unsigned i) const;
    void setExpr(unsigned i, Exec_expr* expr);
protected:
    Exec_expr** m_expr;		// numbered from 1
    unsigned m_size;
};

inline
Exec_expr_row::Code::Code(const SqlSpecs& sqlSpecs, const Alias* aliasList) :
    m_sqlSpecs(sqlSpecs),
    m_aliasList(aliasList)
{
}

inline const SqlSpecs&
Exec_expr_row::Code::sqlSpecs() const
{
    return m_sqlSpecs;
}

inline const char*
Exec_expr_row::Code::getAlias(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_sqlSpecs.count() && m_aliasList != 0);
    return m_aliasList[i];
}

inline
Exec_expr_row::Data::Data(const SqlRow& sqlRow) :
    m_sqlRow(sqlRow)
{
}

inline const SqlRow&
Exec_expr_row::Data::sqlRow() const
{
    return m_sqlRow;
}

inline
Exec_expr_row::Exec_expr_row(Exec_root* root, unsigned size) :
    Exec_base(root),
    m_expr(new Exec_expr* [1 + size]),
    m_size(size)
{
    m_expr[0] = (Exec_expr*)-1;
    for (unsigned i = 1; i <= m_size; i++)
	m_expr[i] = 0;
}

// children

inline const Exec_expr_row::Code&
Exec_expr_row::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr_row::Data&
Exec_expr_row::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline Exec_expr*
Exec_expr_row::getExpr(unsigned i) const
{
    ctx_assert(1 <= i && i <= m_size && m_expr != 0 && m_expr[i] != 0);
    return m_expr[i];
}

inline void
Exec_expr_row::setExpr(unsigned i, Exec_expr* expr)
{
    ctx_assert(1 <= i && i <= m_size && m_expr != 0 && m_expr[i] == 0);
    m_expr[i] = expr;
}

#endif
