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

#ifndef ODBC_CODEGEN_Code_column_hpp
#define ODBC_CODEGEN_Code_column_hpp

#include <common/common.hpp>
#include <common/DataType.hpp>
#include "Code_base.hpp"

class DictColumn;
class Plan_table;

/**
 * @class Plan_column
 * @brief Abstract base class for columns
 */
class Plan_column {
public:
    enum Type {
	Type_expr = 1,
	Type_dml = 2,
	Type_ddl = 3,	// new columns in create table
	Type_idx = 4	// old columns in create index
    };
    Plan_column(Type type, const BaseString& name);
    virtual ~Plan_column() = 0;
    void analyzeColumn(Ctx& ctx, Plan_base::Ctl& ctl);
    // attributes
    const BaseString& getName() const;
    const BaseString& getCname() const;
    const char* getPrintName() const;
    void setCname(const BaseString& cname);
    const DictColumn& dictColumn() const;
    const SqlType& sqlType() const;
protected:
    friend class Plan_table;
    friend class Plan_comp_op;
    Type m_type;
    BaseString m_name;
    BaseString m_cname;
    BaseString m_printName;
    DictColumn* m_dictColumn;
    /**
     * Resolve to table and operational position (for example
     * column number in scan query).
     */
    Plan_table* m_resTable;
    unsigned m_resPos;
    SqlType m_sqlType;
};

inline
Plan_column::Plan_column(Type type, const BaseString& name) :
    m_type(type),
    m_name(name),
    m_printName(name),
    m_dictColumn(0),
    m_resTable(0),
    m_resPos(0)
{
}

inline const BaseString&
Plan_column::getName() const
{
    return m_name;
}

inline const BaseString&
Plan_column::getCname() const
{
    return m_cname;
}

inline const char*
Plan_column::getPrintName() const
{
    return m_printName.c_str();
}

inline void
Plan_column::setCname(const BaseString& cname)
{
    m_cname.assign(cname);
    if (m_cname.empty())
	m_printName.assign(m_name);
    else {
	m_printName.assign(m_cname);
	m_printName.append(".");
	m_printName.append(m_name);
    }
}

inline const DictColumn&
Plan_column::dictColumn() const
{
    ctx_assert(m_dictColumn != 0);
    return *m_dictColumn;
}

inline const SqlType&
Plan_column::sqlType() const
{
    ctx_assert(m_sqlType.type() != SqlType::Undef);
    return m_sqlType;
}

#endif
