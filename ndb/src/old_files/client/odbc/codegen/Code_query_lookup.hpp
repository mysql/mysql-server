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

#ifndef ODBC_CODEGEN_Code_query_lookup_hpp
#define ODBC_CODEGEN_Code_query_lookup_hpp

#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_table.hpp"

class Ctx;
class StmtArea;
class NdbConnection;
class NdbOperation;
class NdbRecAttr;

/**
 * @class Plan_query_lookup
 * @brief Full select (no where clause)
 */
class Plan_query_lookup : public Plan_query {
public:
    Plan_query_lookup(Plan_root* root);
    virtual ~Plan_query_lookup();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // children
    void setTable(Plan_table* table);
protected:
    Plan_table* m_table;
};

inline
Plan_query_lookup::Plan_query_lookup(Plan_root* root) :
    Plan_query(root),
    m_table(0)
{
}

// children

inline void
Plan_query_lookup::setTable(Plan_table* table)
{
    ctx_assert(table != 0);
    m_table = table;
}

/**
 * @class Exec_query_lookup
 * @brief Full select (no where clause)
 */
class Exec_query_lookup : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(unsigned keyCount, unsigned attrCount);
	virtual ~Code();
    protected:
	friend class Plan_query_lookup;
	friend class Exec_query_lookup;
	char* m_tableName;
	unsigned m_keyCount;
	SqlSpecs m_keySpecs;		// key types
	NdbAttrId* m_keyId;
	Exec_expr** m_keyMatch;		// XXX pointers for now
	unsigned m_attrCount;
	SqlSpecs m_sqlSpecs;
	NdbAttrId* m_attrId;
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_lookup* node, const SqlSpecs& sqlSpecs);
	virtual ~Data();
    protected:
	friend class Exec_query_lookup;
	SqlRow m_sqlRow;
	NdbConnection* m_con;
	NdbOperation* m_op;
	NdbRecAttr** m_recAttr;
	bool m_done;			// returns one row
    };
    Exec_query_lookup(Exec_root* root);
    virtual ~Exec_query_lookup();
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
Exec_query_lookup::Code::Code(unsigned keyCount, unsigned attrCount) :
    Exec_query::Code(m_sqlSpecs),
    m_tableName(0),
    m_keyCount(keyCount),
    m_keySpecs(keyCount),
    m_keyId(0),
    m_attrCount(attrCount),
    m_sqlSpecs(attrCount),
    m_attrId(0)
{
}

inline
Exec_query_lookup::Data::Data(Exec_query_lookup* node, const SqlSpecs& sqlSpecs) :
    Exec_query::Data(node, m_sqlRow),
    m_sqlRow(sqlSpecs),
    m_con(0),
    m_op(0),
    m_recAttr(0),
    m_done(false)
{
}

inline
Exec_query_lookup::Exec_query_lookup(Exec_root* root) :
    Exec_query(root)
{
}

// children

inline const Exec_query_lookup::Code&
Exec_query_lookup::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_lookup::Data&
Exec_query_lookup::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
