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

#ifndef ODBC_CODEGEN_Code_update_index_hpp
#define ODBC_CODEGEN_Code_update_index_hpp

#include <common/common.hpp>
#include "Code_base.hpp"
#include "Code_dml.hpp"
#include "Code_table.hpp"
#include "Code_query.hpp"

/**
 * @class Plan_update_index
 * @brief Update in PlanTree
 */
class Plan_update_index : public Plan_dml {
public:
    Plan_update_index(Plan_root* root);
    virtual ~Plan_update_index();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    void describe(Ctx& ctx);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setTable(Plan_table* table, Plan_table::Index* index);
    void setDmlRow(Plan_dml_row* dmlRow);
    void setQuery(Plan_query* query);
protected:
    Plan_table* m_table;
    Plan_table::Index* m_index;
    Plan_dml_row* m_dmlRow;
    Plan_query* m_query;
};

inline
Plan_update_index::Plan_update_index(Plan_root* root) :
    Plan_dml(root),
    m_table(0),
    m_dmlRow(0),
    m_query(0)
{
}

inline void
Plan_update_index::setTable(Plan_table* table, Plan_table::Index* index)
{
    ctx_assert(table != 0 && index != 0 && index == &table->m_indexList[index->m_pos] && index->m_pos != 0);
    m_table = table;
    m_index = index;
}

inline void
Plan_update_index::setDmlRow(Plan_dml_row* dmlRow)
{
    ctx_assert(dmlRow != 0);
    m_dmlRow = dmlRow;
}

inline void
Plan_update_index::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

/**
 * @class Exec_update_index
 * @brief Insert in ExecTree
 */
class Exec_update_index : public Exec_dml {
public:
    class Code : public Exec_dml::Code {
    public:
	Code(unsigned keyCount);
	virtual ~Code();
    protected:
	friend class Plan_update_index;
	friend class Exec_update_index;
	const char* m_tableName;
	const char* m_indexName;
	unsigned m_keyCount;
	SqlSpecs m_keySpecs;		// key types
	NdbAttrId* m_keyId;
	Exec_expr** m_keyMatch;		// XXX pointers for now
	unsigned m_attrCount;
	NdbAttrId* m_attrId;
    };
    class Data : public Exec_dml::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_update_index;
    };
    Exec_update_index(Exec_root* root);
    virtual ~Exec_update_index();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
    void setQuery(Exec_query* query);
protected:
    Exec_query* m_query;
};

inline
Exec_update_index::Code::Code(unsigned keyCount) :
    m_tableName(0),
    m_indexName(0),
    m_keyCount(keyCount),
    m_keySpecs(keyCount),
    m_keyId(0),
    m_keyMatch(0),
    m_attrCount(0),
    m_attrId(0)
{
}

inline
Exec_update_index::Data::Data()
{
}

inline
Exec_update_index::Exec_update_index(Exec_root* root) :
    Exec_dml(root),
    m_query(0)
{
}

// children

inline const Exec_update_index::Code&
Exec_update_index::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_update_index::Data&
Exec_update_index::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_update_index::setQuery(Exec_query* query)
{
    ctx_assert(query != 0 && m_query == 0);
    m_query = query;
}

#endif
