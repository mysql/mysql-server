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

#ifndef ODBC_CODEGEN_Code_comp_op_hpp
#define ODBC_CODEGEN_Code_comp_op_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_pred.hpp"
#include "Code_expr.hpp"
#include "Code_expr_column.hpp"

/**
 * @class Comp_op
 * @brief Comparison operations
 */
struct Comp_op {
    enum Opcode {
	Eq = 1,		// binary
	Noteq,
	Lt,
	Lteq,
	Gt,
	Gteq,
	Like,
	Notlike,
	Isnull,		// unary
	Isnotnull
    };
    Comp_op(Opcode opcode);
    const char* name() const;
    unsigned arity() const;
    Opcode m_opcode;
};

inline
Comp_op::Comp_op(Opcode opcode) :
    m_opcode(opcode)
{
}

/**
 * @class Plan_comp_op
 * @brief Comparison operator node in PlanTree
 */
class Plan_comp_op : public Plan_pred {
public:
    Plan_comp_op(Plan_root* root, Comp_op op);
    virtual ~Plan_comp_op();
    void setExpr(unsigned i, Plan_expr* expr);
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    virtual bool isGroupBy(const Plan_expr_row* row) const;
protected:
    Comp_op m_op;
    Plan_expr* m_expr[1 + 2];
    Plan_expr_column* m_interpColumn[1 + 2];	// candidates
};

inline
Plan_comp_op::Plan_comp_op(Plan_root* root, Comp_op op) :
    Plan_pred(root, TypeComp),
    m_op(op)
{
    m_expr[0] = m_expr[1] = m_expr[2] = 0;
    m_interpColumn[0] = m_interpColumn[1] = m_interpColumn[2] = 0;
}

inline void
Plan_comp_op::setExpr(unsigned i, Plan_expr* expr)
{
    ctx_assert(1 <= i && i <= 2);
    m_expr[i] = expr;
}

/**
 * @class Exec_comp_op
 * @brief Comparison operator node in ExecTree
 */
class Exec_comp_op : public Exec_pred {
public:
    class Code : public Exec_pred::Code {
    public:
	Code(Comp_op op);
	virtual ~Code();
    protected:
	friend class Plan_comp_op;
	friend class Exec_comp_op;
	Comp_op m_op;
	unsigned m_interpColumn;	// 1 or 2 if interpreted column, 0 if both constant
	NdbAttrId m_interpAttrId;
    };
    class Data : public Exec_pred::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_comp_op;
    };
    Exec_comp_op(Exec_root* root);
    virtual ~Exec_comp_op();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execInterp(Ctx& ctx, Ctl& ctl);
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
Exec_comp_op::Code::Code(Comp_op op) :
    m_op(op),
    m_interpColumn(0),
    m_interpAttrId((NdbAttrId)-1)
{
}

inline
Exec_comp_op::Data::Data()
{
}

inline
Exec_comp_op::Exec_comp_op(Exec_root* root) :
    Exec_pred(root)
{
    m_expr[0] = m_expr[1] = m_expr[2] = 0;
}

// children

inline const Exec_comp_op::Code&
Exec_comp_op::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_comp_op::Data&
Exec_comp_op::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_comp_op::setExpr(unsigned i, Exec_expr* expr)
{
    ctx_assert(1 <= i && i <= 2 && m_expr[i] == 0);
    m_expr[i] = expr;
}

#endif
