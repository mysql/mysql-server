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

#ifndef ODBC_CODEGEN_Code_expr_hpp
#define ODBC_CODEGEN_Code_expr_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_base.hpp"

class Ctx;
class Plan_expr_row;
class Exec_expr;

/**
 * @class Plan_expr
 * @brief Base class for expressions in PlanTree
 */
class Plan_expr : public Plan_base {
public:
    // type is convenient since RTTI cannot be used
    enum Type {
	TypeUndefined = 0,
	TypeColumn,
	TypeConst,
	TypeConv,
	TypeFunc,
	TypeOp,
	TypeParam,
	TypeValue
    };
    Plan_expr(Plan_root* root, Type type);
    virtual ~Plan_expr() = 0;
    Type type() const;
    const SqlType& sqlType() const;	// data type set by analyze
    const TableSet& tableSet() const;
    const BaseString& getAlias() const;
    bool isAggr() const;
    bool isBound() const;
    bool isAnyEqual(const Plan_expr_row* row) const;
    virtual bool isEqual(const Plan_expr* expr) const;
    virtual bool isGroupBy(const Plan_expr_row* row) const;
protected:
    friend class Plan_expr_row;
    friend class Plan_expr_op;
    friend class Plan_expr_func;
    friend class Plan_comp_op;
    const Type m_type;
    SqlType m_sqlType;			// subclass must set
    BaseString m_alias;			// for row expression alias
    TableSet m_tableSet;		// depends on these tables
    bool m_isAggr;			// contains an aggregate expression
    bool m_isBound;			// only constants and aggregates
    Exec_expr* m_exec;			// XXX wrong move
};

inline
Plan_expr::Plan_expr(Plan_root* root, Type type) :
    Plan_base(root),
    m_type(type),
    m_isAggr(false),
    m_isBound(false),
    m_exec(0)
{
}

inline Plan_expr::Type
Plan_expr::type() const
{
    return m_type;
}

inline const SqlType&
Plan_expr::sqlType() const
{
    ctx_assert(m_sqlType.type() != SqlType::Undef);
    return m_sqlType;
}

inline const Plan_base::TableSet&
Plan_expr::tableSet() const
{
    return m_tableSet;
}

inline const BaseString&
Plan_expr::getAlias() const
{
    return m_alias;
}

inline bool
Plan_expr::isAggr() const
{
    return m_isAggr;
}

inline bool
Plan_expr::isBound() const
{
    return m_isBound;
}

/**
 * @class Exec_expr
 * @brief Base class for expressions in ExecTree
 */
class Exec_expr : public Exec_base {
public:
    /**
     * Exec_expr::Code includes reference to SqlSpec which
     * specifies data type and access method.
     */
    class Code : public Exec_base::Code {
    public:
	Code(const SqlSpec& sqlSpec);
	virtual ~Code() = 0;
	const SqlSpec& sqlSpec() const;
    protected:
	friend class Exec_expr;
	const SqlSpec& m_sqlSpec;	// subclass must contain
    };
    /**
     * Exec_expr::Data includes reference to SqlField which
     * contains specification and data address.
     */
    class Data : public Exec_base::Data {
    public:
	Data(const SqlField& sqlField);
	virtual ~Data() = 0;
	const SqlField& sqlField() const;
	const SqlField& groupField(unsigned i) const;
    protected:
	friend class Exec_expr;
	const SqlField& m_sqlField;		// subclass must contain
	// group-by data
	typedef std::vector<SqlField> GroupField;
	GroupField m_groupField;
	SqlField& groupField(const SqlType& sqlType, unsigned i, bool initFlag);
    };
    Exec_expr(Exec_root* root);
    virtual ~Exec_expr() = 0;
    /**
     * Evaluate the expression.  Must be implemented by all
     * subclasses.  Check ctx.ok() for errors.
     */
    virtual void evaluate(Ctx& ctx, Ctl& ctl) = 0;
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_expr::Code::Code(const SqlSpec& sqlSpec) :
    m_sqlSpec(sqlSpec)
{
}

inline const SqlSpec&
Exec_expr::Code::sqlSpec() const {
    return m_sqlSpec;
}

inline
Exec_expr::Data::Data(const SqlField& sqlField) :
    m_sqlField(sqlField)
{
}

inline const SqlField&
Exec_expr::Data::sqlField() const
{
    return m_sqlField;
}

inline const SqlField&
Exec_expr::Data::groupField(unsigned i) const
{
    ctx_assert(i != 0 && i < m_groupField.size());
    return m_groupField[i];
}

inline
Exec_expr::Exec_expr(Exec_root* root) :
    Exec_base(root)
{
}

// children

inline const Exec_expr::Code&
Exec_expr::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr::Data&
Exec_expr::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}


#endif
