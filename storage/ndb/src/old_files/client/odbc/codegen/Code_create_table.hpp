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

#ifndef ODBC_CODEGEN_Code_create_table_hpp
#define ODBC_CODEGEN_Code_create_table_hpp

#include <vector>
#include <common/common.hpp>
#include "Code_ddl.hpp"
#include "Code_ddl_row.hpp"
#include "Code_create_row.hpp"

class DictTable;
class DictColumn;

/**
 * @class Plan_create_table
 * @brief Create table in PlanTree
 */
class Plan_create_table : public Plan_ddl {
public:
    Plan_create_table(Plan_root* root, const BaseString& name);
    virtual ~Plan_create_table();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void describe(Ctx & ctx);
    void print(Ctx& ctx);
    // attributes
    const BaseString& getName() const;
    // children
    void setCreateRow(Plan_create_row* createRow);
    void setFragmentType(NdbDictionary::Object::FragmentType fragmentType);
    void setLogging(bool logging);
protected:
    BaseString m_name;
    Plan_create_row* m_createRow;
    NdbDictionary::Object::FragmentType m_fragmentType;
    bool m_logging;
};

inline
Plan_create_table::Plan_create_table(Plan_root* root, const BaseString& name) :
    Plan_ddl(root),
    m_name(name),
    m_createRow(0),
    m_fragmentType(NdbDictionary::Object::FragUndefined),
    m_logging(true)
{
}

inline const BaseString&
Plan_create_table::getName() const
{
    return m_name;
}

// children

inline void
Plan_create_table::setCreateRow(Plan_create_row* createRow)
{
    ctx_assert(createRow != 0);
    m_createRow = createRow;
}

inline void
Plan_create_table::setFragmentType(NdbDictionary::Object::FragmentType fragmentType)
{
    m_fragmentType = fragmentType;
}

inline void
Plan_create_table::setLogging(bool logging)
{
    m_logging = logging;
}

/**
 * @class Exec_create_table
 * @brief Create table in ExecTree
 */
class Exec_create_table : public Exec_ddl {
public:
    class Code : public Exec_ddl::Code {
    public:
	struct Attr {
	    Attr() : m_defaultValue(0) {}
	    BaseString m_attrName;
	    SqlType m_sqlType;
	    bool m_tupleKey;
	    bool m_tupleId;
	    bool m_autoIncrement;
	    Exec_expr* m_defaultValue;
	};
	Code(const BaseString& tableName, unsigned attrCount, const Attr* attrList, unsigned tupleId, unsigned autoIncrement);
	virtual ~Code();
    protected:
	friend class Plan_create_table;
	friend class Exec_create_table;
	const BaseString m_tableName;
	const unsigned m_attrCount;
	const Attr* const m_attrList;
	unsigned m_tupleId;
	unsigned m_autoIncrement;
	NdbDictionary::Object::FragmentType m_fragmentType;
	bool m_logging;
    };
    class Data : public Exec_ddl::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_create_table;
    };
    Exec_create_table(Exec_root* root);
    virtual ~Exec_create_table();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execute(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_create_table::Code::Code(const BaseString& tableName, unsigned attrCount, const Attr* attrList, unsigned tupleId, unsigned autoIncrement) :
    m_tableName(tableName),
    m_attrCount(attrCount),
    m_attrList(attrList),
    m_tupleId(tupleId),
    m_autoIncrement(autoIncrement),
    m_fragmentType(NdbDictionary::Object::FragUndefined),
    m_logging(true)
{
}

inline
Exec_create_table::Data::Data()
{
}

inline
Exec_create_table::Exec_create_table(Exec_root* root) :
    Exec_ddl(root)
{
}

// children

inline const Exec_create_table::Code&
Exec_create_table::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_create_table::Data&
Exec_create_table::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
