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

#include "StmtInfo.hpp"

const char*
StmtInfo::getDesc() const
{
    switch (m_name) {
    case Stmt_name_select:
	return "select";
    case Stmt_name_insert:
	return "insert";
    case Stmt_name_update:
	return "update";
    case Stmt_name_delete:
	return "delete";
    case Stmt_name_create_table:
	return "create table";
    case Stmt_name_create_index:
	return "create index";
    case Stmt_name_drop_table:
	return "drop table";
    case Stmt_name_drop_index:
	return "drop index";
    default:
	ctx_assert(false);
	break;
    }
    return "";
}

StmtType
StmtInfo::getType() const
{
    StmtType type = Stmt_type_undef;
    switch (m_name) {
    case Stmt_name_select:		// query
	type = Stmt_type_query;
	break;
    case Stmt_name_insert:		// DML
    case Stmt_name_update:
    case Stmt_name_delete:
	type = Stmt_type_DML;
	break;
    case Stmt_name_create_table:	// DDL
    case Stmt_name_create_index:
    case Stmt_name_drop_table:
    case Stmt_name_drop_index:
	type = Stmt_type_DDL;
	break;
    default:
	ctx_assert(false);
	break;
    }
    return type;
}

void
StmtInfo::free(Ctx& ctx)
{
    m_name = Stmt_name_undef;
    m_function = "";
    m_functionCode = SQL_DIAG_UNKNOWN_STATEMENT;
}
