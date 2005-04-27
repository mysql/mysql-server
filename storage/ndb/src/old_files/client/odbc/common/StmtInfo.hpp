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

#ifndef ODBC_COMMON_StmtInfo_hpp
#define ODBC_COMMON_StmtInfo_hpp

#include <common/common.hpp>

// general type (determined by SQL command)
enum StmtType {
    Stmt_type_undef = 0,
    Stmt_type_query,			// select
    Stmt_type_DML,			// insert, update, delete
    Stmt_type_DDL,			// create, drop, alter table
    Stmt_type_info			// virtual query
};

// specific SQL command (first 1-2 words)
enum StmtName {
    Stmt_name_undef = 0,
    Stmt_name_select,
    Stmt_name_insert,
    Stmt_name_update,
    Stmt_name_delete,
    Stmt_name_create_table,
    Stmt_name_create_index,
    Stmt_name_drop_table,
    Stmt_name_drop_index
};

/**
 * @class StmtInfo
 * @brief Statement Info
 * 
 * Statement info.  This is a separate class which could
 * be used in cases not tied to statement execution.
 */
class StmtInfo {
public:
    StmtInfo();
    void setName(StmtName name);
    StmtName getName() const;
    const char* getDesc() const;
    StmtType getType() const;
    void free(Ctx& ctx);
private:
    friend class StmtArea;
    StmtName m_name;
    const char* m_function;		// not allocated
    SQLINTEGER m_functionCode;
};

inline
StmtInfo::StmtInfo() :
    m_name(Stmt_name_undef),
    m_function(""),
    m_functionCode(SQL_DIAG_UNKNOWN_STATEMENT)
{
}

inline void
StmtInfo::setName(StmtName name)
{
    m_name = name;
}

inline StmtName
StmtInfo::getName() const
{
    return m_name;
}

#endif
