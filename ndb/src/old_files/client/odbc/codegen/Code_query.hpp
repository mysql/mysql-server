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

#ifndef ODBC_CODEGEN_Code_query_hpp
#define ODBC_CODEGEN_Code_query_hpp

#include <common/common.hpp>
#include <common/DataRow.hpp>
#include <common/ResultArea.hpp>
#include "Code_stmt.hpp"

class Plan_expr_row;
class Plan_table;
class Exec_expr_row;

/**
 * @class Plan_query
 * @brief Base class for queries in PlanTree
 */
class Plan_query : public Plan_stmt {
public:
    Plan_query(Plan_root* root);
    virtual ~Plan_query() = 0;
    void describe(Ctx& ctx);
    virtual Plan_expr_row* getRow();
};

inline
Plan_query::Plan_query(Plan_root* root) :
    Plan_stmt(root)
{
}

/**
 * @class Exec_query
 * @brief Base class for executable queries.
 *
 * Executable queriable statement.
 */
class Exec_query : public Exec_stmt {
public:
    class Code : public Exec_stmt::Code {
    public:
	Code(const SqlSpecs& sqlSpecs);
	virtual ~Code() = 0;
	const SqlSpecs& sqlSpecs() const;
    protected:
	friend class Exec_query;
	const SqlSpecs& m_sqlSpecs;	// subclass must contain
    };
    class Data : public Exec_stmt::Data, public ResultSet {
    public:
	Data(Exec_query* node, const SqlRow& sqlRow);
	virtual ~Data() = 0;
	const SqlRow& sqlRow() const;
	ExtRow* extRow() const;
	bool fetchImpl(Ctx& ctx, Ctl& ctl);
    protected:
	friend class Exec_query;
	Exec_query* const m_node;
	ExtRow* m_extRow;		// output bindings
	int* m_extPos;			// positions for SQLGetData
    };
    Exec_query(Exec_root* root);
    virtual ~Exec_query() = 0;
    void bind(Ctx& ctx);
    void execute(Ctx& ctx, Ctl& ctl);
    bool fetch(Ctx& ctx, Ctl& ctl);
    // children
    const Code& getCode() const;
    Data& getData() const;
    virtual const Exec_query* getRawQuery() const;
    // odbc support
    void sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind);
protected:
    friend class Data;
    virtual void execImpl(Ctx& ctx, Ctl& ctl) = 0;
    virtual bool fetchImpl(Ctx& ctx, Ctl& ctl) = 0;
};

inline
Exec_query::Code::Code(const SqlSpecs& sqlSpecs) :
    m_sqlSpecs(sqlSpecs)
{
}

inline const SqlSpecs&
Exec_query::Code::sqlSpecs() const
{
    return m_sqlSpecs;
}

inline
Exec_query::Data::Data(Exec_query* node, const SqlRow& sqlRow) :
    ResultSet(sqlRow),
    m_node(node),
    m_extRow(0),
    m_extPos(0)
{
}

inline const SqlRow&
Exec_query::Data::sqlRow() const
{
    return static_cast<const SqlRow&>(m_dataRow);
}

inline ExtRow*
Exec_query::Data::extRow() const
{
    return m_extRow;
}

inline bool
Exec_query::Data::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    return m_node->fetchImpl(ctx, ctl);
}

inline
Exec_query::Exec_query(Exec_root* root) :
    Exec_stmt(root)
{
}

// children

inline const Exec_query::Code&
Exec_query::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query::Data&
Exec_query::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
