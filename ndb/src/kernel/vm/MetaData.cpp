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

#include "MetaData.hpp"
#include "SimulatedBlock.hpp"
#include <blocks/dbdict/Dbdict.hpp>
#include <blocks/dbdih/Dbdih.hpp>

// MetaData::Common

MetaData::Common::Common(Dbdict& dbdict, Dbdih& dbdih) :
  m_dbdict(dbdict),
  m_dbdih(dbdih)
{
  m_lock[false] = m_lock[true] = 0;
}

// MetaData::Table

// MetaData

MetaData::MetaData(Common& common) :
  m_common(common)
{
  m_lock[false] = m_lock[true] = 0;
}

MetaData::MetaData(SimulatedBlock* block) :
  m_common(*block->getMetaDataCommon())
{
  m_lock[false] = m_lock[true] = 0;
}

MetaData::~MetaData()
{
  for (int i = false; i <= true; i++) {
    assert(m_common.m_lock[i] >= m_lock[i]);
    m_common.m_lock[i] -= m_lock[i];
    m_lock[i] = 0;
  }
}

int
MetaData::lock(bool exclusive)
{
  if (m_common.m_lock[true] > m_lock[true]) {
    // locked exclusively by another instance
    return MetaData::Locked;
  }
  m_lock[exclusive]++;
  m_common.m_lock[exclusive]++;
  return 0;
}

int
MetaData::unlock(bool exclusive)
{
  if (m_lock[exclusive] == 0) {
    return MetaData::NotLocked;
  }
  m_lock[exclusive]--;
  m_common.m_lock[exclusive]--;
  return 0;
}

int
MetaData::getTable(MetaData::Table& table, Uint32 tableId, Uint32 tableVersion)
{
  if (m_lock[false] + m_lock[true] == 0) {
    return MetaData::NotLocked;
  }
  return m_common.m_dbdict.getMetaTable(table, tableId, tableVersion);
}

int
MetaData::getTable(MetaData::Table& table, const char* tableName)
{
  if (m_lock[false] + m_lock[true] == 0) {
    return MetaData::NotLocked;
  }
  return m_common.m_dbdict.getMetaTable(table, tableName);
}

int
MetaData::getAttribute(MetaData::Attribute& attribute, const MetaData::Table& table, Uint32 attributeId)
{
  if (m_lock[false] + m_lock[true] == 0) {
    return MetaData::NotLocked;
  }
  return m_common.m_dbdict.getMetaAttribute(attribute, table, attributeId);
}

int
MetaData::getAttribute(MetaData::Attribute& attribute, const MetaData::Table& table, const char* attributeName)
{
  if (m_lock[false] + m_lock[true] == 0) {
    return MetaData::NotLocked;
  }
  return m_common.m_dbdict.getMetaAttribute(attribute, table, attributeName);
}
