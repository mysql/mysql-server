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

#ifndef ODBC_CODEGEN_Code_expr_const_hpp
#define ODBC_CODEGEN_Code_expr_const_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_expr.hpp"

/**
 * @class Plan_expr_const
 * @brief Constant expression value in PlanTree
 */
class Plan_expr_const : public Plan_expr {
public:
    Plan_expr_const(Plan_root* root, LexType lexType, const char* value);
    virtual ~Plan_expr_const();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    bool isEqual(const Plan_expr* expr) const;
    bool isGroupBy(const Plan_expr_row* row) const;
protected:
    // lexical type and token set by the parser
    LexType m_lexType;
    BaseString m_string;
};

inline
Plan_expr_const::Plan_expr_const(Plan_root* root, LexType lexType, const char* value) :
    Plan_expr(root, TypeConst),
    m_lexType(lexType),
    m_string(value)
{
}

/**
 * @class Exec_expr_const
 * @brief Constant expression value in ExecTree
 */
class Exec_expr_const : public Exec_expr {
public:
    class Code : public Exec_expr::Code {
    public:
	Code(const SqlField& sqlField);
	virtual ~Code();
    protected:
	friend class Exec_expr_const;
	const SqlField m_sqlField;
    };
    class Data : public Exec_expr::Data {
    public:
	Data(SqlField& sqlField);
	virtual ~Data();
    protected:
	friend class Exec_expr_const;
	SqlField m_sqlField;
    };
    Exec_expr_const(Exec_root* root);
    virtual ~Exec_expr_const();
    void alloc(Ctx& ctx, Ctl& ctl);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_expr_const::Code::Code(const SqlField& sqlField) :
    Exec_expr::Code(m_sqlField.sqlSpec()),
    m_sqlField(sqlField)
{
}

inline
Exec_expr_const::Data::Data(SqlField& sqlField) :
    Exec_expr::Data(m_sqlField),
    m_sqlField(sqlField)
{
}

inline
Exec_expr_const::Exec_expr_const(Exec_root* root) :
    Exec_expr(root)
{
}

// children

inline const Exec_expr_const::Code&
Exec_expr_const::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr_const::Data&
Exec_expr_const::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
