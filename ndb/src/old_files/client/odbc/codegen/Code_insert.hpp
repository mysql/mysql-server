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

#ifndef ODBC_CODEGEN_Code_insert_hpp
#define ODBC_CODEGEN_Code_insert_hpp

#include <common/common.hpp>
#include "Code_base.hpp"
#include "Code_dml.hpp"
#include "Code_dml_row.hpp"
#include "Code_expr_row.hpp"
#include "Code_select.hpp"
#include "Code_query.hpp"
#include "Code_table.hpp"
#include "Code_set_row.hpp"

enum Insert_op {
    Insert_op_undef = 0,
    Insert_op_insert = 1,
    Insert_op_write = 2
};

/**
 * @class Plan_insert
 * @brief Insert in PlanTree
 *
 * Insert.  Becomes directly executable.
 */
class Plan_insert : public Plan_dml {
public:
    Plan_insert(Plan_root* root, Insert_op insertOp);
    virtual ~Plan_insert();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    void describe(Ctx& ctx);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setTable(Plan_table* table);
    Plan_dml_row* getDmlRow() const;
    void setDmlRow(Plan_dml_row* dmlRow);
    void setExprRow(Plan_expr_row* exprRow);
    void setSelect(Plan_select* select);
    void setQuery(Plan_query* query);
    void setMysqlRow(Plan_set_row* mysqlRow);
protected:
    Insert_op m_insertOp;
    Plan_table* m_table;
    Plan_dml_row* m_dmlRow;
    Plan_expr_row* m_exprRow;
    Plan_select* m_select;
    Plan_query* m_query;
    Plan_set_row* m_mysqlRow;
};

inline
Plan_insert::Plan_insert(Plan_root* root, Insert_op insertOp) :
    Plan_dml(root),
    m_insertOp(insertOp),
    m_table(0),
    m_dmlRow(0),
    m_exprRow(0),
    m_select(0),
    m_query(0),
    m_mysqlRow(0)
{
}

// children

inline void
Plan_insert::setTable(Plan_table* table)
{
    ctx_assert(table != 0);
    m_table = table;
}

inline void
Plan_insert::setDmlRow(Plan_dml_row* dmlRow)
{
    ctx_assert(dmlRow != 0);
    m_dmlRow = dmlRow;
}

inline Plan_dml_row*
Plan_insert::getDmlRow() const
{
    ctx_assert(m_dmlRow != 0);
    return m_dmlRow;
}

inline void
Plan_insert::setExprRow(Plan_expr_row* exprRow)
{
    ctx_assert(exprRow != 0);
    m_exprRow = exprRow;
}

inline void
Plan_insert::setSelect(Plan_select* select)
{
    ctx_assert(select != 0);
    m_select = select;
}

inline void
Plan_insert::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Plan_insert::setMysqlRow(Plan_set_row* mysqlRow)
{
    ctx_assert(mysqlRow != 0);
    m_mysqlRow = mysqlRow;
}

/**
 * @class Exec_insert
 * @brief Executable insert
 */
class Exec_insert : public Exec_dml {
public:
    class Code : public Exec_dml::Code {
    public:
	Code();
	virtual ~Code();
    protected:
	friend class Plan_insert;
	friend class Exec_insert;
	Insert_op m_insertOp;
	char* m_tableName;
	unsigned m_attrCount;
	NdbAttrId* m_attrId;
	bool* m_isKey;
	unsigned m_tupleId;		// position of tuple id
	unsigned m_autoIncrement;	// position of ai key
	SqlType m_idType;		// type of tuple id or ai key
	unsigned m_defaultCount;
	NdbAttrId* m_defaultId;
	SqlRow* m_defaultValue;
	bool findAttrId(NdbAttrId attrId) const;
    };
    class Data : public Exec_dml::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_insert;
    };
    Exec_insert(Exec_root* root);
    virtual ~Exec_insert();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setQuery(Exec_query* query);
private:
    Exec_query* m_query;
};

inline
Exec_insert::Code::Code() :
    m_insertOp(Insert_op_undef),
    m_tableName(0),
    m_attrCount(0),
    m_attrId(0),
    m_isKey(0),
    m_tupleId(0),
    m_autoIncrement(0),
    m_defaultCount(0),
    m_defaultId(0),
    m_defaultValue(0)
{
}

inline
Exec_insert::Data::Data()
{
}

inline
Exec_insert::Exec_insert(Exec_root* root) :
    Exec_dml(root),
    m_query(0)
{
}

// children

inline const Exec_insert::Code&
Exec_insert::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_insert::Data&
Exec_insert::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_insert::setQuery(Exec_query* query)
{
    ctx_assert(m_query == 0 && query != 0);
    m_query = query;
}

#endif
