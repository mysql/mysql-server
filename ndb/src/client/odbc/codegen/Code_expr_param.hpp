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

#ifndef ODBC_CODEGEN_Code_expr_param_hpp
#define ODBC_CODEGEN_Code_expr_param_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_expr.hpp"

/**
 * @class Plan_expr_param
 * @brief Constant expression value in PlanTree
 */
class Plan_expr_param : public Plan_expr {
public:
    Plan_expr_param(Plan_root* root, unsigned paramNumber);
    virtual ~Plan_expr_param();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    void describe(Ctx& ctx);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    bool isEqual(const Plan_expr* expr) const;
    bool isGroupBy(const Plan_expr_row* row) const;
protected:
    const unsigned m_paramNumber;
};

inline
Plan_expr_param::Plan_expr_param(Plan_root* root, unsigned paramNumber) :
    Plan_expr(root, TypeParam),
    m_paramNumber(paramNumber)
{
}

/**
 * @class Exec_expr_param
 * @brief Constant expression value in ExecTree
 */
class Exec_expr_param : public Exec_expr {
public:
    class Code : public Exec_expr::Code {
    public:
	Code(const SqlSpec& sqlSpec, unsigned paramNumber);
	virtual ~Code();
    protected:
	friend class Plan_expr_param;
	friend class Exec_expr_param;
	const SqlSpec m_sqlSpec;
	const unsigned m_paramNumber;
    };
    class Data : public Exec_expr::Data {
    public:
	Data(SqlField& sqlField);
	virtual ~Data();
	ExtField* extField() const;
    protected:
	friend class Exec_expr_param;
	friend class Exec_root;
	SqlField m_sqlField;
	ExtField* m_extField;		// input binding
	bool m_atExec;			// data at exec
	int m_extPos;			// position for SQLPutData (-1 before first call)
    };
    Exec_expr_param(Exec_root* root);
    virtual ~Exec_expr_param();
    void alloc(Ctx& ctx, Ctl& ctl);
    void bind(Ctx& ctx);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_expr_param::Code::Code(const SqlSpec& sqlSpec, unsigned paramNumber) :
    Exec_expr::Code(m_sqlSpec),
    m_sqlSpec(sqlSpec),
    m_paramNumber(paramNumber)
{
}

inline
Exec_expr_param::Data::Data(SqlField& sqlField) :
    Exec_expr::Data(m_sqlField),
    m_sqlField(sqlField),
    m_extField(0),
    m_atExec(false),
    m_extPos(-1)
{
}

inline ExtField*
Exec_expr_param::Data::extField() const
{
    return m_extField;
}

inline
Exec_expr_param::Exec_expr_param(Exec_root* root) :
    Exec_expr(root)
{
}

// children

inline const Exec_expr_param::Code&
Exec_expr_param::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr_param::Data&
Exec_expr_param::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
