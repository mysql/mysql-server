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

#ifndef ODBC_DICTIONARY_DictColumn_hpp
#define ODBC_DICTIONARY_DictColumn_hpp

#include <common/common.hpp>
#include <common/DataType.hpp>

class Ctx;
class SqlType;
class ConnArea;
class DictTable;

/**
 * @class DictColumn
 * @brief Table column
 */
class DictColumn {
public:
    DictColumn(const ConnArea& connArea, const BaseString& name, const SqlType& sqlType);
    ~DictColumn();
    const BaseString& getName() const;
    const SqlType& sqlType() const;
    void setPosition(unsigned position);
    unsigned getPosition() const;
    void setParent(DictTable* parent);
    DictTable* getParent() const;
    NdbAttrId getAttrId() const;
    bool isKey() const;
    bool isTupleId() const;
    bool isAutoIncrement() const;
    const char* getDefaultValue() const;
protected:
    friend class DictSys;
    friend class DictTable;
    const ConnArea& m_connArea;
    const BaseString m_name;
    SqlType m_sqlType;
    unsigned m_position;
    DictTable* m_parent;
    bool m_key;			// part of key
    bool m_tupleId;		// the tuple id
    bool m_autoIncrement;
    const char* m_defaultValue;
};

inline
DictColumn::DictColumn(const ConnArea& connArea, const BaseString& name, const SqlType& sqlType) :
    m_connArea(connArea),
    m_name(name),
    m_sqlType(sqlType),
    m_position(0),
    m_parent(0),
    m_key(false),
    m_tupleId(false),
    m_autoIncrement(false),
    m_defaultValue(0)
{
}

inline const SqlType&
DictColumn::sqlType() const
{
    return m_sqlType;
}

inline void
DictColumn::setPosition(unsigned position)
{
    ctx_assert(position != 0);
    m_position = position;
}

inline unsigned
DictColumn::getPosition() const
{
    return m_position;
}

inline void
DictColumn::setParent(DictTable* parent)
{
    m_parent = parent;
}

inline DictTable*
DictColumn::getParent() const
{
    return m_parent;
}

inline const BaseString&
DictColumn::getName() const
{
    return m_name;
}

inline NdbAttrId
DictColumn::getAttrId() const
{
    ctx_assert(m_position != 0);
    return static_cast<NdbAttrId>(m_position - 1);
}

inline bool
DictColumn::isKey() const
{
    return m_key;
}

inline bool
DictColumn::isTupleId() const
{
    return m_tupleId;
}

inline bool
DictColumn::isAutoIncrement() const
{
    return m_autoIncrement;
}

inline const char*
DictColumn::getDefaultValue() const
{
    return m_defaultValue;
}

#endif
