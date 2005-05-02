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

#ifndef ODBC_CODEGEN_Code_create_index_hpp
#define ODBC_CODEGEN_Code_create_index_hpp

#include <vector>
#include <NdbApi.hpp>
#include <common/common.hpp>
#include "Code_ddl.hpp"
#include "Code_table.hpp"
#include "Code_idx_column.hpp"

class DictTable;
class DictColumn;

/**
 * @class Plan_create_index
 * @brief Create table in PlanTree
 */
class Plan_create_index : public Plan_ddl {
public:
    Plan_create_index(Plan_root* root, const BaseString& name);
    virtual ~Plan_create_index();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void describe(Ctx & ctx);
    void print(Ctx& ctx);
    // attributes
    const BaseString& getName() const;
    // children
    void setType(NdbDictionary::Object::Type type);
    void setTable(Plan_table* table);
    unsigned countColumn() const;
    void addColumn(Plan_idx_column* column);
    Plan_idx_column* getColumn(unsigned i) const;
    void setFragmentType(NdbDictionary::Object::FragmentType fragmentType);
    void setLogging(bool logging);
protected:
    BaseString m_name;
    NdbDictionary::Object::Type m_type;
    Plan_table* m_table;
    IdxColumnVector m_columnList;
    NdbDictionary::Object::FragmentType m_fragmentType;
    bool m_logging;
};

inline
Plan_create_index::Plan_create_index(Plan_root* root, const BaseString& name) :
    Plan_ddl(root),
    m_name(name),
    m_type(NdbDictionary::Object::TypeUndefined),
    m_columnList(1),
    m_fragmentType(NdbDictionary::Object::FragUndefined),
    m_logging(true)
{
}

inline const BaseString&
Plan_create_index::getName() const
{
    return m_name;
}

// children

inline void
Plan_create_index::setType(NdbDictionary::Object::Type type)
{
    m_type = type;
}

inline void
Plan_create_index::setTable(Plan_table* table)
{
    ctx_assert(table != 0);
    m_table = table;
}

inline unsigned
Plan_create_index::countColumn() const
{
    return m_columnList.size() - 1;
}

inline void
Plan_create_index::addColumn(Plan_idx_column* column)
{
    ctx_assert(column != 0);
    m_columnList.push_back(column);
}

inline Plan_idx_column*
Plan_create_index::getColumn(unsigned i) const
{
    ctx_assert(1 <= i && i <= countColumn() && m_columnList[i] != 0);
    return m_columnList[i];
}

inline void
Plan_create_index::setFragmentType(NdbDictionary::Object::FragmentType fragmentType)
{
    m_fragmentType = fragmentType;
}

inline void
Plan_create_index::setLogging(bool logging)
{
    m_logging = logging;
}

/**
 * @class Exec_create_index
 * @brief Create table in ExecTree
 */
class Exec_create_index : public Exec_ddl {
public:
    class Code : public Exec_ddl::Code {
    public:
	Code(const BaseString& indexName, const BaseString& tableName, NdbDictionary::Object::Type type, unsigned attrCount, const char** attrList);
	virtual ~Code();
    protected:
	friend class Plan_create_index;
	friend class Exec_create_index;
	const BaseString m_indexName;
	const BaseString m_tableName;
	NdbDictionary::Object::Type m_type;
	const unsigned m_attrCount;
	const char** m_attrList;
	NdbDictionary::Object::FragmentType m_fragmentType;
	bool m_logging;
    };
    class Data : public Exec_ddl::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_create_index;
    };
    Exec_create_index(Exec_root* root);
    virtual ~Exec_create_index();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execute(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_create_index::Code::Code(const BaseString& indexName, const BaseString& tableName, NdbDictionary::Object::Type type, unsigned attrCount, const char** attrList) :
    m_indexName(indexName),
    m_tableName(tableName),
    m_type(type),
    m_attrCount(attrCount),
    m_attrList(attrList),
    m_fragmentType(NdbDictionary::Object::FragUndefined),
    m_logging(true)
{
}

inline
Exec_create_index::Data::Data()
{
}

inline
Exec_create_index::Exec_create_index(Exec_root* root) :
    Exec_ddl(root)
{
}

// children

inline const Exec_create_index::Code&
Exec_create_index::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_create_index::Data&
Exec_create_index::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
