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

#ifndef ODBC_CODEGEN_Code_pred_op_hpp
#define ODBC_CODEGEN_Code_pred_op_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_pred.hpp"

/**
 * @class Pred_op
 * @brief Boolean operators
 */
struct Pred_op {
    enum Opcode {
	And = 1,	// binary
	Or,
	Not		// unary
    };
    Pred_op(Opcode opcode);
    const char* name() const;
    unsigned arity() const;
    Opcode m_opcode;
};

inline
Pred_op::Pred_op(Opcode opcode) :
    m_opcode(opcode)
{
}

/**
 * @class Plan_pred_op
 * @brief Operator node in a predicate in PlanTree
 */
class Plan_pred_op : public Plan_pred {
public:
    Plan_pred_op(Plan_root* root, Pred_op op);
    virtual ~Plan_pred_op();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    bool isGroupBy(const Plan_expr_row* row) const;
    // children
    void setPred(unsigned i, Plan_pred* pred);
protected:
    friend class Plan_pred;
    Pred_op m_op;
    Plan_pred* m_pred[1 + 2];
};

inline
Plan_pred_op::Plan_pred_op(Plan_root* root, Pred_op op) :
    Plan_pred(root, TypeOp),
    m_op(op)
{
    m_pred[0] = m_pred[1] = m_pred[2] = 0;
}

inline void
Plan_pred_op::setPred(unsigned i, Plan_pred* pred)
{
    ctx_assert(1 <= i && i <= m_op.arity() && pred != 0);
    m_pred[i] = pred;
}

/**
 * @class Exec_pred_op
 * @brief Operator node in a predicate in ExecTree
 */
class Exec_pred_op : public Exec_pred {
public:
    class Code : public Exec_pred::Code {
    public:
	Code(Pred_op op);
	virtual ~Code();
    protected:
	friend class Exec_pred_op;
	Pred_op m_op;
    };
    class Data : public Exec_pred::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_pred_op;
    };
    Exec_pred_op(Exec_root* root);
    virtual ~Exec_pred_op();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execInterp(Ctx& ctx, Ctl& ctl);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setPred(unsigned i, Exec_pred* pred);
protected:
    Exec_pred* m_pred[1 + 2];
};

inline
Exec_pred_op::Code::Code(Pred_op op) :
    m_op(op)
{
}

inline
Exec_pred_op::Data::Data()
{
}

inline
Exec_pred_op::Exec_pred_op(Exec_root* root) :
    Exec_pred(root)
{
    m_pred[0] = m_pred[1] = m_pred[2] = 0;
}

// children

inline const Exec_pred_op::Code&
Exec_pred_op::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_pred_op::Data&
Exec_pred_op::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_pred_op::setPred(unsigned i, Exec_pred* pred)
{
    ctx_assert(1 <= i && i <= 2 && m_pred[i] == 0);
    m_pred[i] = pred;
}

#endif
