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

#ifndef ODBC_CODEGEN_Code_expr_func_hpp
#define ODBC_CODEGEN_Code_expr_func_hpp

#include <common/common.hpp>
#include <common/DataField.hpp>
#include "Code_expr.hpp"
#include "Code_expr_row.hpp"

/**
 * @class Expr_func
 * @brief Specifies a function
 */
struct Expr_func {
    enum Code {
	Undef = 0,
	Substr,
	Left,
	Right,
	Count,
	Max,
	Min,
	Sum,
	Avg,
	Rownum,
	Sysdate
    };
    Expr_func(Code code, const char* name, bool aggr);
    Code m_code;
    const char* m_name;
    bool m_aggr;
    static const Expr_func& find(const char* name);
};

inline
Expr_func::Expr_func(Code code, const char* name, bool aggr) :
    m_code(code),
    m_name(name),
    m_aggr(aggr)
{
}

/**
 * @class Plan_expr_func
 * @brief Function node in an expression in PlanTree
 */
class Plan_expr_func : public Plan_expr {
public:
    Plan_expr_func(Plan_root* root, const Expr_func& func);
    virtual ~Plan_expr_func();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    bool isEqual(const Plan_expr* expr) const;
    bool isGroupBy(const Plan_expr_row* row) const;
    // children
    void setArgs(Plan_expr_row* args);
protected:
    const Expr_func& m_func;
    Plan_expr_row* m_args;
    unsigned m_narg;
    SqlType* m_conv;	// temp work area
};

inline
Plan_expr_func::Plan_expr_func(Plan_root* root, const Expr_func& func) :
    Plan_expr(root, TypeFunc),
    m_func(func),
    m_args(0),
    m_narg(0),
    m_conv(0)
{
}

inline void
Plan_expr_func::setArgs(Plan_expr_row* args)
{
    if (args == 0) {
	m_args = 0;
	m_narg = 0;
    } else {
	m_args = args;
	m_narg = m_args->getSize();
	delete[] m_conv;
	m_conv = new SqlType[1 + m_narg];
    }
}

/**
 * @class Exec_expr_func
 * @brief Function node in an expression in ExecTree
 */
class Exec_expr_func : public Exec_expr {
public:
    class Code : public Exec_expr::Code {
    public:
	Code(const Expr_func& func, const SqlSpec& spec);
	virtual ~Code();
    protected:
	friend class Plan_expr_func;
	friend class Exec_expr_func;
	const Expr_func& m_func;
	const SqlSpec m_sqlSpec;
	unsigned m_narg;
	Exec_expr** m_args;		// XXX pointers for now
    };
    class Data : public Exec_expr::Data {
    public:
	Data(const SqlField& sqlField);
	virtual ~Data();
    protected:
	friend class Exec_expr_func;
	SqlField m_sqlField;
	struct Acc {			// accumulators etc
	    SqlBigint m_count;		// current row count
	    union {
		SqlBigint m_bigint;
		SqlDouble m_double;
		SqlDatetime m_sysdate;
	    };
	};
	// group-by extra accumulators (default in entry 0)
	typedef std::vector<Acc> GroupAcc;
	GroupAcc m_groupAcc;
    };
    Exec_expr_func(Exec_root* root);
    virtual ~Exec_expr_func();
    void alloc(Ctx& ctx, Ctl& ctl);
    void evaluate(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
protected:
    void init(Ctx &ctx, Ctl& ctl);	// initialize values
};

inline
Exec_expr_func::Code::Code(const Expr_func& func, const SqlSpec& sqlSpec) :
    Exec_expr::Code(m_sqlSpec),
    m_func(func),
    m_sqlSpec(sqlSpec),
    m_args(0)
{
}

inline
Exec_expr_func::Data::Data(const SqlField& sqlField) :
    Exec_expr::Data(m_sqlField),
    m_sqlField(sqlField),
    m_groupAcc(1)
{
}

inline
Exec_expr_func::Exec_expr_func(Exec_root* root) :
    Exec_expr(root)
{
}

// children

inline const Exec_expr_func::Code&
Exec_expr_func::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_expr_func::Data&
Exec_expr_func::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
