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

#ifndef ODBC_CODEGEN_Code_query_range_hpp
#define ODBC_CODEGEN_Code_query_range_hpp

#include <common/common.hpp>
#include "Code_query.hpp"
#include "Code_table.hpp"
#include "Code_pred.hpp"

class Ctx;
class StmtArea;
class NdbConnection;
class NdbOperation;
class NdbRecAttr;

/*
 * Range scan via ordered index.  We implement only the case of equality
 * on an initial sequence of index keys.
 */

class Plan_query_range : public Plan_query {
public:
    Plan_query_range(Plan_root* root);
    virtual ~Plan_query_range();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    void setTable(Plan_table* table, Plan_table::Index* index);
    void setInterp(Plan_pred* interp);
    void setExclusive();
protected:
    Plan_table* m_table;
    Plan_table::Index* m_index;
    Plan_pred* m_interp;
    bool m_exclusive;
};

inline
Plan_query_range::Plan_query_range(Plan_root* root) :
    Plan_query(root),
    m_table(0),
    m_index(0),
    m_interp(0),
    m_exclusive(false)
{
}

inline void
Plan_query_range::setTable(Plan_table* table, Plan_table::Index* index)
{
    ctx_assert(table != 0 && index != 0 && index == &table->m_indexList[index->m_pos] && index->m_pos != 0);
    m_table = table;
    m_index = index;
}

inline void
Plan_query_range::setInterp(Plan_pred* interp)
{
    ctx_assert(interp != 0);
    m_interp = interp;
}

inline void
Plan_query_range::setExclusive()
{
    m_exclusive = true;
}

class Exec_query_range : public Exec_query {
public:
    class Code : public Exec_query::Code {
    public:
	Code(unsigned keyCount, unsigned attrCount);
	virtual ~Code();
    protected:
	friend class Plan_query_range;
	friend class Exec_query_range;
	const char* m_tableName;
	const char* m_indexName;
	unsigned m_keyCount;
	SqlSpecs m_keySpecs;		// key types
	NdbAttrId* m_keyId;
	Exec_expr** m_keyMatch;		// XXX pointers for now
	unsigned m_attrCount;
	SqlSpecs m_sqlSpecs;
	NdbAttrId* m_attrId;
	bool m_exclusive;
    };
    class Data : public Exec_query::Data {
    public:
	Data(Exec_query_range* node, const SqlSpecs& sqlSpecs);
	virtual ~Data();
    protected:
	friend class Exec_query_range;
	SqlRow m_sqlRow;
	NdbConnection* m_con;
	NdbOperation* m_op;
	NdbRecAttr** m_recAttr;
	unsigned m_parallel;
	bool m_done;			// if no match possible due to range
    };
    Exec_query_range(Exec_root* root);
    virtual ~Exec_query_range();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execImpl(Ctx& ctx, Ctl& ctl);
    bool fetchImpl(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    const Code& getCode() const;
    Data& getData() const;
    void setInterp(Exec_pred* interp);
protected:
    Exec_pred* m_interp;
};

inline
Exec_query_range::Code::Code(unsigned keyCount, unsigned attrCount) :
    Exec_query::Code(m_sqlSpecs),
    m_tableName(0),
    m_indexName(0),
    m_keyCount(keyCount),
    m_keySpecs(keyCount),
    m_keyId(0),
    m_attrCount(attrCount),
    m_sqlSpecs(attrCount),
    m_attrId(0),
    m_exclusive(false)
{
}

inline
Exec_query_range::Data::Data(Exec_query_range* node, const SqlSpecs& sqlSpecs) :
    Exec_query::Data(node, m_sqlRow),
    m_sqlRow(sqlSpecs),
    m_con(0),
    m_op(0),
    m_recAttr(0),
    m_parallel(1),
    m_done(false)
{
}

inline
Exec_query_range::Exec_query_range(Exec_root* root) :
    Exec_query(root),
    m_interp(0)
{
}

inline const Exec_query_range::Code&
Exec_query_range::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_query_range::Data&
Exec_query_range::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

inline void
Exec_query_range::setInterp(Exec_pred* interp)
{
    ctx_assert(interp != 0);
    m_interp = interp;
}

#endif
