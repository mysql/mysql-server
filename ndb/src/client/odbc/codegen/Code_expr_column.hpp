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

#ifndef ODBC_CODEGEN_Code_expr_column_hpp
#define ODBC_CODEGEN_Code_expr_column_hpp

#include <common/common.hpp>
#include "Code_column.hpp"
#include "Code_expr.hpp"

class DictColumn;
class Plan_table;

/**
 * @class Plan_expr_column
 * @brief Column in query expression
 */
class Plan_expr_column : public Plan_expr, public Plan_column {
public:
    Plan_expr_column(Plan_root* root, const BaseString& name);
    virtual ~Plan_expr_column();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    bool resolveEq(Ctx& ctx, Plan_expr* expr);
    void print(Ctx& ctx);
    bool isEqual(const Plan_expr* expr) const;
    bool isGroupBy(const Plan_expr_row* row) const;
};

inline
Plan_expr_column::Plan_expr_column(Plan_root* root, const BaseString& name) :
    Plan_expr(root, TypeColumn),
    Plan_column(Type_expr, name)
{
}

/**
 * @class Exec_expr_column
 * @brief Column in query expression
 */
class Exec_expr_column : public Exec_expr {
public:
    class Code : public Exec_expr::Code {
    public:
	Code(const SqlSpec& sqlSpec, unsigned resOff);
	virtual ~Code();
    protected:
	friend class Exec_expr_column;
	SqlSpec m_sqlSpec;
	unsigned m_resOff;
    };
    class Data : public Exec_expr::Data {
    public:
	Data(SqlField& sqlField);
	virtual ~Data();
    protected:
	friend class Exec_expr_column;
	// set reference to SqlField in query
    };
    Exec_expr_column(Exec_root* root);
    virtual ~Exec_expr_column();
    void alloc(Ctx& ctx, Ctl& ctl);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_expr_column::Code::Code(const SqlSpec& sqlSpec, unsigned resOff) :
    Exec_expr::Code(m_sqlSpec),
    m_sqlSpec(sqlSpec),
    m_resOff(resOff)
{
}

inline
Exec_expr_column::Data::Data(SqlField& sqlField) :
    Exec_expr::Data(sqlField)
{
}

inline
Exec_expr_column::Exec_expr_column(Exec_root* root) :
    Exec_expr(root)
{
}

// children

inline const Exec_expr_column::Code&
Exec_expr_column::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr_column::Data&
Exec_expr_column::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
