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

#ifndef ODBC_CODEGEN_Code_expr_conv_hpp
#define ODBC_CODEGEN_Code_expr_conv_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_expr.hpp"

/**
 * @class Plan_expr_conv
 * @brief Data type conversion in PlanTree
 *
 * Inserted to convert value to another compatible type.
 */
class Plan_expr_conv : public Plan_expr {
public:
    Plan_expr_conv(Plan_root* root, const SqlType& sqlType);
    virtual ~Plan_expr_conv();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    bool isEqual(const Plan_expr* expr) const;
    bool isGroupBy(const Plan_expr_row* row) const;
    // children
    void setExpr(Plan_expr* expr);
protected:
    Plan_expr* m_expr;
};

inline
Plan_expr_conv::Plan_expr_conv(Plan_root* root, const SqlType& sqlType) :
    Plan_expr(root, TypeConv),
    m_expr(0)
{
    ctx_assert(sqlType.type() != SqlType::Undef);
    m_sqlType = sqlType;
}

inline void
Plan_expr_conv::setExpr(Plan_expr* expr)
{
    ctx_assert(expr != 0);
    m_expr = expr;
}

/**
 * @class Exec_expr_conv
 * @brief Data type conversion in ExecTree
 */
class Exec_expr_conv : public Exec_expr {
public:
    class Code : public Exec_expr::Code {
    public:
	Code(const SqlSpec& spec);
	virtual ~Code();
    protected:
	friend class Exec_expr_conv;
	const SqlSpec m_sqlSpec;
    };
    class Data : public Exec_expr::Data {
    public:
	Data(const SqlField& sqlField);
	virtual ~Data();
    protected:
	friend class Exec_expr_conv;
	SqlField m_sqlField;
    };
    Exec_expr_conv(Exec_root* root);
    virtual ~Exec_expr_conv();
    void alloc(Ctx& ctx, Ctl& ctl);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setExpr(Exec_expr* expr);
protected:
    Exec_expr* m_expr;
};

inline
Exec_expr_conv::Code::Code(const SqlSpec& sqlSpec) :
    Exec_expr::Code(m_sqlSpec),
    m_sqlSpec(sqlSpec)
{
}

inline
Exec_expr_conv::Data::Data(const SqlField& sqlField) :
    Exec_expr::Data(m_sqlField),
    m_sqlField(sqlField)
{
}

inline
Exec_expr_conv::Exec_expr_conv(Exec_root* root) :
    Exec_expr(root),
    m_expr(0)
{
}

// children

inline const Exec_expr_conv::Code&
Exec_expr_conv::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr_conv::Data&
Exec_expr_conv::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_expr_conv::setExpr(Exec_expr* expr)
{
    ctx_assert(m_expr == 0 && expr != 0);
    m_expr = expr;
}

#endif
