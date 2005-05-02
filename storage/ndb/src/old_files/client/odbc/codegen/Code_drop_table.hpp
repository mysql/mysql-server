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

#ifndef ODBC_CODEGEN_Code_drop_table_hpp
#define ODBC_CODEGEN_Code_drop_table_hpp

#include <vector>
#include <NdbApi.hpp>
#include <common/common.hpp>
#include "Code_ddl.hpp"

class DictTable;
class DictColumn;

/**
 * @class Plan_drop_table
 * @brief Drop table in PlanTree
 */
class Plan_drop_table : public Plan_ddl {
public:
    Plan_drop_table(Plan_root* root, const BaseString& name);
    virtual ~Plan_drop_table();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void describe(Ctx & ctx);
    void print(Ctx& ctx);
    // attributes
    const BaseString& getName() const;
protected:
    BaseString m_name;
};

inline
Plan_drop_table::Plan_drop_table(Plan_root* root, const BaseString& name) :
    Plan_ddl(root),
    m_name(name)
{
}

inline const BaseString&
Plan_drop_table::getName() const
{
    return m_name;
}

/**
 * @class Exec_drop_table
 * @brief Drop table in ExecTree
 */
class Exec_drop_table : public Exec_ddl {
public:
    class Code : public Exec_ddl::Code {
    public:
	Code(const BaseString& tableName);
	virtual ~Code();
    protected:
	friend class Exec_drop_table;
	const BaseString m_tableName;
    };
    class Data : public Exec_ddl::Data {
    public:
	Data();
	virtual ~Data();
    protected:
	friend class Exec_drop_table;
    };
    Exec_drop_table(Exec_root* root);
    virtual ~Exec_drop_table();
    void alloc(Ctx& ctx, Ctl& ctl);
    void execute(Ctx& ctx, Ctl& ctl);
    void close(Ctx& ctx);
    void print(Ctx& ctx);
    // children
    const Code& getCode() const;
    Data& getData() const;
};

inline
Exec_drop_table::Code::Code(const BaseString& tableName) :
    m_tableName(tableName)
{
}

inline
Exec_drop_table::Data::Data()
{
}

inline
Exec_drop_table::Exec_drop_table(Exec_root* root) :
    Exec_ddl(root)
{
}

// children

inline const Exec_drop_table::Code&
Exec_drop_table::getCode() const
{
    const Code* code = static_cast<const Code*>(m_code);
    return *code;
}

inline Exec_drop_table::Data&
Exec_drop_table::getData() const
{
    Data* data = static_cast<Data*>(m_data);
    return *data;
}

#endif
