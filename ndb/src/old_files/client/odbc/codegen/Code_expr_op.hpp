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

#ifndef ODBC_CODEGEN_Code_expr_op_hpp
#define ODBC_CODEGEN_Code_expr_op_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_expr.hpp"

/**
 * @class Expr_op
 * @brief Arithmetic and string operators
 */
struct Expr_op {
    enum Opcode {
	Add = 1,	// binary
	Subtract,
	Multiply,
	Divide,
	Plus,		// unary
	Minus
    };
    Expr_op(Opcode opcode);
    const char* name() const;
    unsigned arity() const;
    Opcode m_opcode;
};

inline
Expr_op::Expr_op(Opcode opcode) :
    m_opcode(opcode)
{
}

/**
 * @class Plan_expr_op
 * @brief Operator node in an expression in PlanTree
 */
class Plan_expr_op : public Plan_expr {
public:
    Plan_expr_op(Plan_root* root, Expr_op op);
    virtual ~Plan_expr_op();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    bool isEqual(const Plan_expr* expr) const;
    bool isGroupBy(const Plan_expr_row* row) const;
    // children
    void setExpr(unsigned i, Plan_expr* expr);
protected:
    Expr_op m_op;
    Plan_expr* m_expr[1 + 2];
};

inline
Plan_expr_op::Plan_expr_op(Plan_root* root, Expr_op op) :
    Plan_expr(root, TypeOp),
    m_op(op)
{
    m_expr[0] = m_expr[1] = m_expr[2] = 0;
}

inline void
Plan_expr_op::setExpr(unsigned i, Plan_expr* expr)
{
    ctx_assert(1 <= i && i <= 2 && expr != 0);
    m_expr[i] = expr;
}

/**
 * @class Exec_expr_op
 * @brief Operator node in an expression in ExecTree
 */
class Exec_expr_op : public Exec_expr {
public:
    class Code : public Exec_expr::Code {
    public:
	Code(Expr_op op, const SqlSpec& spec);
	virtual ~Code();
    protected:
	friend class Exec_expr_op;
	Expr_op m_op;
	const SqlSpec m_sqlSpec;
    };
    class Data : public Exec_expr::Data {
    public:
	Data(const SqlField& sqlField);
	virtual ~Data();
    protected:
	friend class Exec_expr_op;
	SqlField m_sqlField;
    };
    Exec_expr_op(Exec_root* root);
    virtual ~Exec_expr_op();
    void alloc(Ctx& ctx, Ctl& ctl);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setExpr(unsigned i, Exec_expr* expr);
protected:
    Exec_expr* m_expr[1 + 2];
};

inline
Exec_expr_op::Code::Code(Expr_op op, const SqlSpec& sqlSpec) :
    Exec_expr::Code(m_sqlSpec),
    m_op(op),
    m_sqlSpec(sqlSpec)
{
}

inline
Exec_expr_op::Data::Data(const SqlField& sqlField) :
    Exec_expr::Data(m_sqlField),
    m_sqlField(sqlField)
{
}

inline
Exec_expr_op::Exec_expr_op(Exec_root* root) :
    Exec_expr(root)
{
    m_expr[0] = m_expr[1] = m_expr[2] = 0;
}

// children

inline const Exec_expr_op::Code&
Exec_expr_op::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr_op::Data&
Exec_expr_op::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_expr_op::setExpr(unsigned i, Exec_expr* expr)
{
    ctx_assert(1 <= i && i <= 2 && m_expr[i] == 0);
    m_expr[i] = expr;
}

#endif
