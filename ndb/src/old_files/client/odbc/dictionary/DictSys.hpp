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

#ifndef ODBC_DICTIONARY_DictSys_hpp
#define ODBC_DICTIONARY_DictSys_hpp

#include <common/common.hpp>
#include <common/DataType.hpp>

class Ctx;
class DictSchema;
class DictTable;
class SqlType;

/**
 * @class DictSys
 * @brief Built-in tables (replaced later by real systables)
 */
class DictSys {
public:
    enum Id {
	Undef = 0,
	OdbcTypeinfo = 1,
	OdbcTables = 2,
	OdbcColumns = 3,
	OdbcPrimarykeys = 4,
	Dual = 5
    };
    struct Column {
	Column(unsigned position, const char* name, bool key, const SqlType& sqlType);
	const unsigned m_position;
	const char* const m_name;
	const bool m_key;
	const SqlType m_sqlType;
    };
    struct Table {
	Table(Id id, const char* name, const Column* columnList, unsigned columnCount);
	const Id m_id;
	const char* m_name;
	const Column* const m_columnList;
	const unsigned m_columnCount;
    };
    static DictTable* loadTable(Ctx& ctx, DictSchema* schema, const BaseString& name);
};

inline
DictSys::Column::Column(unsigned position, const char* name, bool key, const SqlType& sqlType) :
    m_position(position),
    m_name(name),
    m_key(key),
    m_sqlType(sqlType)
{
}

inline
DictSys::Table::Table(DictSys::Id id, const char* name, const Column* columnList, unsigned columnCount) :
    m_id(id),
    m_name(name),
    m_columnList(columnList),
    m_columnCount(columnCount)
{
}

#endif
