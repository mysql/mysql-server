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

#ifndef ODBC_CODEGEN_Code_ddl_column_hpp
#define ODBC_CODEGEN_Code_ddl_column_hpp

#include <common/common.hpp>
#include "Code_column.hpp"
#include "Code_data_type.hpp"
#include "Code_expr.hpp"

class DictColumn;
class Plan_table;

/**
 * @class Plan_ddl_column
 * @brief Column in DDL statement
 */
class Plan_ddl_column : public Plan_base, public Plan_column {
public:
    Plan_ddl_column(Plan_root* root, const BaseString& name);
    virtual ~Plan_ddl_column();
    Plan_base* analyze(Ctx& ctx, Ctl& ctl);
    Exec_base* codegen(Ctx& ctx, Ctl& ctl);
    void print(Ctx& ctx);
    // attributes
    void setNotNull();
    void setUnSigned();
    void setPrimaryKey();
    bool getPrimaryKey() const;
    void setTupleId();
    bool getTupleId() const;
    void setAutoIncrement();
    bool getAutoIncrement() const;
    // children
    void setType(Plan_data_type* type);
    void setDefaultValue(Plan_expr* defaultValue);
    Plan_expr* getDefaultValue() const;
protected:
    friend class Plan_create_row;
    Plan_data_type* m_type;
    Plan_expr* m_defaultValue;
    bool m_nullable;
    bool m_unSigned;
    bool m_primaryKey;
    bool m_tupleId;
    bool m_autoIncrement;
};

inline
Plan_ddl_column::Plan_ddl_column(Plan_root* root, const BaseString& name) :
    Plan_base(root),
    Plan_column(Type_ddl, name),
    m_type(0),
    m_defaultValue(0),
    m_nullable(true),
    m_unSigned(false),
    m_primaryKey(false),
    m_tupleId(false),
    m_autoIncrement(false)
{
}

inline void
Plan_ddl_column::setNotNull()
{
    m_nullable = false;
}

inline void
Plan_ddl_column::setUnSigned()
{
    m_unSigned = true;
}

inline void
Plan_ddl_column::setPrimaryKey()
{
    m_nullable = false;
    m_primaryKey = true;
}

inline bool
Plan_ddl_column::getPrimaryKey() const
{
    return m_primaryKey;
}

inline void
Plan_ddl_column::setTupleId()
{
    m_nullable = false;
    m_tupleId = true;
}

inline bool
Plan_ddl_column::getTupleId() const
{
    return m_tupleId;
}

inline void
Plan_ddl_column::setAutoIncrement()
{
    m_nullable = false;
    m_autoIncrement = true;
}

inline bool
Plan_ddl_column::getAutoIncrement() const
{
    return m_autoIncrement;
}

// children

inline void
Plan_ddl_column::setType(Plan_data_type* type)
{
    ctx_assert(type != 0);
    m_type = type;
}

inline void
Plan_ddl_column::setDefaultValue(Plan_expr* defaultValue)
{
    ctx_assert(defaultValue != 0);
    m_defaultValue = defaultValue;
}

inline Plan_expr*
Plan_ddl_column::getDefaultValue() const
{
    return m_defaultValue;
}

#endif
