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

#ifndef ODBC_CODEGEN_Code_delete_lookup_hpp
#define ODBC_CODEGEN_Code_delete_lookup_hpp

#include <common/common.hpp>
#include "Code_dml.hpp"
#include "Code_query.hpp"
#include "Code_table.hpp"

/**
 * @class Plan_delete_lookup
 * @brief Delete by primary key
 */
class Plan_delete_lookup : public Plan_dml {
public:
    Plan_delete_lookup(Plan_root* root);
    virtual ~Plan_delete_lookup();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    void describe(Ctx& ctx);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setQuery(Plan_query* query);
    void setTable(Plan_table* table);
protected:
    Plan_query* m_query;
    Plan_table* m_table;
};

inline
Plan_delete_lookup::Plan_delete_lookup(Plan_root* root) :
    Plan_dml(root),
    m_query(0),
    m_table(0)
{
}

inline void
Plan_delete_lookup::setQuery(Plan_query* query)
{
    ctx_assert(query != 0);
    m_query = query;
}

inline void
Plan_delete_lookup::setTable(Plan_table* table)
{
    ctx_assert(table != 0);
    m_table = table;
}

/**
 * @class Exec_delete_lookup
 * @brief Delete by primary key
 */
class Exec_delete_lookup : public Exec_dml {
public:
    class Code : public Exec_dml::Code {
    public:
	Code(unsigned keyCount);
	virtual ~Code();
    protected:
	friend class Plan_delete_lookup;
	friend class Exec_delete_lookup;
	char* m_tableName;
	unsigned m_keyCount;
	SqlSpecs m_keySpecs;		// key types
	NdbAttrId* m_keyId;
	Exec_expr** m_keyMatch;		// XXX pointers for now
    };
    class Data : public Exec_dml::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_delete_lookup;
    };
    Exec_delete_lookup(Exec_root* root);
    virtual ~Exec_delete_lookup();
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
Exec_delete_lookup::Code::Code(unsigned keyCount) :
    m_tableName(0),
    m_keyCount(keyCount),
    m_keySpecs(keyCount),
    m_keyId(0),
    m_keyMatch(0)
{
}

inline
Exec_delete_lookup::Data::Data()
{
}

inline
Exec_delete_lookup::Exec_delete_lookup(Exec_root* root) :
    Exec_dml(root),
    m_query(0)
{
}

// children

inline const Exec_delete_lookup::Code&
Exec_delete_lookup::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_delete_lookup::Data&
Exec_delete_lookup::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_delete_lookup::setQuery(Exec_query* query)
{
    ctx_assert(query != 0 && m_query == 0);
    m_query = query;
}

#endif
