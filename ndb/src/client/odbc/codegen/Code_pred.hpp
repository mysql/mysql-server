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

#ifndef ODBC_CODEGEN_Code_pred_hpp
#define ODBC_CODEGEN_Code_pred_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_base.hpp"

enum Pred_value {
    Pred_value_unknown = -1,
    Pred_value_false = 0,
    Pred_value_true = 1
};

class Ctx;
class Plan_expr_row;
class Exec_pred;

/**
 * @class Plan_pred
 * @brief Base class for predicates in PlanTree
 *
 * Predicate represents a boolean value.
 */
class Plan_pred : public Plan_base {
public:
    // type is convenient since RTTI cannot be used
    enum Type {
	TypeUndefined = 0,
	TypeComp = 1,
	TypeOp = 2
    };
    Plan_pred(Plan_root* root, Type type);
    virtual ~Plan_pred() = 0;
    Type type() const;
    const TableSet& tableSet() const;
    const TableSet& noInterp() const;
    virtual bool isGroupBy(const Plan_expr_row* row) const;
    // helpers
    Plan_pred* opAnd(Plan_pred* pred2);
protected:
    const Type m_type;
    TableSet m_tableSet;	// depends on these tables
    TableSet m_noInterp;	// cannot use interpreted TUP program on these tables
    Exec_pred* m_exec;		// probably stupid
};

inline
Plan_pred::Plan_pred(Plan_root* root, Type type) :
    Plan_base(root),
    m_type(type),
    m_exec(0)
{
}

inline Plan_pred::Type
Plan_pred::type() const
{
    return m_type;
}

inline const Plan_pred::TableSet&
Plan_pred::tableSet() const
{
    return m_tableSet;
}

inline const Plan_pred::TableSet&
Plan_pred::noInterp() const
{
    return m_noInterp;
}

/**
 * @class Exec_pred
 * @brief Base class for predicates in ExecTree
 */
class Exec_pred : public Exec_base {
public:
    class Code : public Exec_base::Code {
    public:
	Code();
	virtual ~Code() = 0;
    protected:
	friend class Exec_pred;
    };
    class Data : public Exec_base::Data {
    public:
	Data();
	virtual ~Data() = 0;
	Pred_value getValue() const;
	Pred_value groupValue(unsigned i) const;
    protected:
	friend class Exec_pred;
	Pred_value m_value;	// the value
	// group-by data
	typedef std::vector<Pred_value> GroupValue;
	GroupValue m_groupValue;
	Pred_value& groupValue(unsigned i, bool initFlag);
    };
    Exec_pred(Exec_root* root);
    virtual ~Exec_pred() = 0;
    virtual void execInterp(Ctx& ctx, Ctl& ctl) = 0;
    virtual void evaluate(Ctx& ctx, Ctl& ctl) = 0;
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_pred::Code::Code()
{
}

inline
Exec_pred::Data::Data() :
    m_value(Pred_value_unknown)
{
}

inline Pred_value
Exec_pred::Data::getValue() const
{
    return m_value;
}

inline Pred_value
Exec_pred::Data::groupValue(unsigned i) const
{
    ctx_assert(i != 0 && i < m_groupValue.size());
    return m_groupValue[i];
}

inline
Exec_pred::Exec_pred(Exec_root* root) :
    Exec_base(root)
{
}

// children

inline const Exec_pred::Code&
Exec_pred::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_pred::Data&
Exec_pred::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}


#endif
