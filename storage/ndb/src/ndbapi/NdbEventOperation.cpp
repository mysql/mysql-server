/*
   Copyright (C) 2003-2007 MySQL AB, 2009, 2010 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <Ndb.hpp>
#include <NdbError.hpp>
#include <portlib/NdbMem.h>
#include "NdbEventOperationImpl.hpp"
#include "NdbDictionaryImpl.hpp"

NdbEventOperation::NdbEventOperation(Ndb *theNdb,const char* eventName) 
  : m_impl(* new NdbEventOperationImpl(*this,theNdb,eventName))
{
}

NdbEventOperation::~NdbEventOperation()
{
  NdbEventOperationImpl * tmp = &m_impl;
  if (this != tmp)
    delete tmp;
}

NdbEventOperation::State NdbEventOperation::getState()
{
  return m_impl.getState();
}

void NdbEventOperation::mergeEvents(bool flag)
{
  m_impl.m_mergeEvents = flag;
}

NdbRecAttr *
NdbEventOperation::getValue(const char *colName, char *aValue)
{
  return m_impl.getValue(colName, aValue, 0);
}

NdbRecAttr *
NdbEventOperation::getPreValue(const char *colName, char *aValue)
{
  return m_impl.getValue(colName, aValue, 1);
}

NdbBlob *
NdbEventOperation::getBlobHandle(const char *colName)
{
  return m_impl.getBlobHandle(colName, 0);
}

NdbBlob *
NdbEventOperation::getPreBlobHandle(const char *colName)
{
  return m_impl.getBlobHandle(colName, 1);
}

int
NdbEventOperation::execute()
{
  return m_impl.execute();
}

int
NdbEventOperation::isOverrun() const
{
  return 0; // ToDo
}

bool
NdbEventOperation::isConsistent() const
{
  return true;
}

void
NdbEventOperation::clearError()
{
  m_impl.m_has_error= 0;
}

int
NdbEventOperation::hasError() const
{
  return m_impl.m_has_error;
}

bool NdbEventOperation::tableNameChanged() const
{
  return m_impl.tableNameChanged();
}

bool NdbEventOperation::tableFrmChanged() const
{
  return m_impl.tableFrmChanged();
}

bool NdbEventOperation::tableFragmentationChanged() const
{
  return m_impl.tableFragmentationChanged();
}

bool NdbEventOperation::tableRangeListChanged() const
{
  return m_impl.tableRangeListChanged();
}

Uint64
NdbEventOperation::getGCI() const
{
  return m_impl.getGCI();
}

Uint32
NdbEventOperation::getAnyValue() const
{
  return m_impl.getAnyValue();
}

Uint64
NdbEventOperation::getLatestGCI() const
{
  return m_impl.getLatestGCI();
}

Uint64
NdbEventOperation::getTransId() const
{
  return m_impl.getTransId();
}

NdbDictionary::Event::TableEvent
NdbEventOperation::getEventType() const
{
  return m_impl.getEventType();
}

void
NdbEventOperation::print()
{
  m_impl.print();
}

/*
 * Internal for the mysql server
 */
const NdbDictionary::Table *NdbEventOperation::getTable() const
{
  return m_impl.m_eventImpl->m_tableImpl->m_facade;
}
const NdbDictionary::Event *NdbEventOperation::getEvent() const
{
  return m_impl.m_eventImpl->m_facade;
}
const NdbRecAttr* NdbEventOperation::getFirstPkAttr() const
{
  return m_impl.theFirstPkAttrs[0];
}
const NdbRecAttr* NdbEventOperation::getFirstPkPreAttr() const
{
  return m_impl.theFirstPkAttrs[1];
}
const NdbRecAttr* NdbEventOperation::getFirstDataAttr() const
{
  return m_impl.theFirstDataAttrs[0];
}
const NdbRecAttr* NdbEventOperation::getFirstDataPreAttr() const
{
  return m_impl.theFirstDataAttrs[1];
}
/*
bool NdbEventOperation::validateTable(NdbDictionary::Table &table) const
{
  DBUG_ENTER("NdbEventOperation::validateTable");
  bool res = true;
  if (table.getObjectVersion() != m_impl.m_eventImpl->m_tableVersion)
  {
    DBUG_PRINT("info",("invalid version"));
    res= false;
  }
  DBUG_RETURN(res);
}
*/
void NdbEventOperation::setCustomData(void * data)
{
  m_impl.m_custom_data= data;
}
void * NdbEventOperation::getCustomData() const
{
  return m_impl.m_custom_data;
}

int NdbEventOperation::getReqNodeId() const
{
  return SubTableData::getReqNodeId(m_impl.m_data_item->sdata->requestInfo);
}

int NdbEventOperation::getNdbdNodeId() const
{
  return SubTableData::getNdbdNodeId(m_impl.m_data_item->sdata->requestInfo);
}

/*
 * Private members
 */

NdbEventOperation::NdbEventOperation(NdbEventOperationImpl& impl) 
  : m_impl(impl) {}

const struct NdbError & 
NdbEventOperation::getNdbError() const {
  return m_impl.getNdbError();
}
