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

#ifndef ODBC_CODEGEN_Code_query_repeat_hpp
#define ODBC_CODEGEN_Code_query_repeat_hpp

#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_expr_row.hpp"

/**
 * @class Plan_query_repeat
 * @brief Constant query node in PlanTree
 */
class Plan_query_repeat : public Plan_query {
public:
    Plan_query_repeat(Plan_root* root);
    Plan_query_repeat(Plan_root* root, CountType maxcount);
    virtual ~Plan_query_repeat();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
private:
    bool m_forever;
    CountType m_maxcount;
};

inline
Plan_query_repeat::Plan_query_repeat(Plan_root* root) :
    Plan_query(root),
    m_forever(true),
    m_maxcount(0)
{
}

inline
Plan_query_repeat::Plan_query_repeat(Plan_root* root, CountType maxcount) :
    Plan_query(root),
    m_forever(false),
    m_maxcount(maxcount)
{
}

/**
 * @class Exec_query_repeat
 * @brief Constant query node in ExecTree
 */
class Exec_query_repeat : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(const SqlSpecs& sqlSpecs, bool forever, CountType maxcount);
	virtual ~Code();
    protected:
	friend class Exec_query_repeat;
	SqlSpecs m_sqlSpecs;
	bool m_forever;
	CountType m_maxcount;
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_repeat* node, const SqlSpecs& sqlSpecs);
	virtual ~Data();
    protected:
	friend class Exec_query_repeat;
	SqlRow m_sqlRow;
	CountType m_count;
    };
    Exec_query_repeat(Exec_root* root);
    virtual ~Exec_query_repeat();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_query_repeat::Code::Code(const SqlSpecs& sqlSpecs, bool forever, CountType maxcount) :
    Exec_query::Code(m_sqlSpecs),
    m_sqlSpecs(sqlSpecs),
    m_forever(forever),
    m_maxcount(maxcount)
{
}

inline
Exec_query_repeat::Data::Data(Exec_query_repeat* node, const SqlSpecs& sqlSpecs) :
    Exec_query::Data(node, m_sqlRow),
    m_sqlRow(sqlSpecs),
    m_count(0)
{
}

inline
Exec_query_repeat::Exec_query_repeat(Exec_root* root) :
    Exec_query(root)
{
}

// children

inline const Exec_query_repeat::Code&
Exec_query_repeat::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_repeat::Data&
Exec_query_repeat::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
