/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <EventLogger.hpp>
#include <Ndb.hpp>
#include <NdbError.hpp>
#include "NdbDictionaryImpl.hpp"
#include "NdbEventOperationImpl.hpp"

NdbEventOperation::NdbEventOperation(Ndb *ndb,
                                     const NdbDictionary::Event *event)
    : m_impl(*new NdbEventOperationImpl(*this, ndb, std::move(event))) {}

NdbEventOperation::NdbEventOperation(NdbEventOperationImpl &impl)
    : m_impl(impl) {}

NdbEventOperation::~NdbEventOperation() {
  NdbEventOperationImpl *tmp = &m_impl;
  if (this != tmp) delete tmp;
}

NdbEventOperation::State NdbEventOperation::getState() {
  return m_impl.getState();
}

void NdbEventOperation::mergeEvents(bool flag) { m_impl.m_mergeEvents = flag; }

NdbRecAttr *NdbEventOperation::getValue(const char *colName, char *aValue) {
  return m_impl.getValue(colName, aValue, 0);
}

NdbRecAttr *NdbEventOperation::getPreValue(const char *colName, char *aValue) {
  return m_impl.getValue(colName, aValue, 1);
}

NdbBlob *NdbEventOperation::getBlobHandle(const char *colName) {
  return m_impl.getBlobHandle(colName, 0);
}

NdbBlob *NdbEventOperation::getPreBlobHandle(const char *colName) {
  return m_impl.getBlobHandle(colName, 1);
}

int NdbEventOperation::execute() { return m_impl.execute(); }

int NdbEventOperation::isOverrun() const {
  return 0;  // ToDo
}

bool NdbEventOperation::isConsistent() const { return true; }

void NdbEventOperation::clearError() {}

int NdbEventOperation::hasError() const {
  // Never has error
  return 0;
}

bool NdbEventOperation::tableNameChanged() const {
  return m_impl.tableNameChanged();
}

bool NdbEventOperation::tableFrmChanged() const {
  return m_impl.tableFrmChanged();
}

bool NdbEventOperation::tableFragmentationChanged() const {
  return m_impl.tableFragmentationChanged();
}

bool NdbEventOperation::tableRangeListChanged() const {
  return m_impl.tableRangeListChanged();
}

Uint64 NdbEventOperation::getEpoch() const { return m_impl.getGCI(); }

Uint64 NdbEventOperation::getGCI() const { return getEpoch(); }

Uint32 NdbEventOperation::getAnyValue() const { return m_impl.getAnyValue(); }

Uint64 NdbEventOperation::getLatestGCI() const { return m_impl.getLatestGCI(); }

Uint64 NdbEventOperation::getTransId() const { return m_impl.getTransId(); }

NdbDictionary::Event::TableEvent NdbEventOperation::getEventType2() const {
  return m_impl.getEventType2();
}

bool NdbEventOperation::isEmptyEpoch() { return m_impl.isEmptyEpoch(); }

bool NdbEventOperation::isErrorEpoch(
    NdbDictionary::Event::TableEvent *error_type) {
  return m_impl.isErrorEpoch(error_type);
}

NdbDictionary::Event::TableEvent NdbEventOperation::getEventType() const {
  NdbDictionary::Event::TableEvent type = getEventType2();
  /**
   * Since this is called after nextEvent() returns a valid operation,
   * and nextEvent() does not return a valid operation
   * for exceptional event data
   *  (nextEvent does not return a valid operation for TE_INCONSIS,
   *   it crashes at TE_OUT_OF_MEMORY and TE_EMPTY),
   * getEventType should not see the new event types, unless getEventType
   * is called after nextEvent2().
   * Following assert will ensure that.
   */

  if (type >= NdbDictionary::Event::TE_EMPTY) {
    g_eventLogger->error(
        "Ndb::getEventType: Found exceptional event type 0x%x. Use methods "
        "either from the old event API or from the new API. Do not mix.",
        type);
  }

  // event types >= TE_EMPTY are the new exceptional ones
  assert(type < NdbDictionary::Event::TE_EMPTY);
  return type;
}

void NdbEventOperation::print() { m_impl.print(); }

/*
 * Internal for the mysql server
 */
const NdbDictionary::Table *NdbEventOperation::getTable() const {
  return m_impl.m_eventImpl->m_tableImpl->m_facade;
}
const NdbDictionary::Event *NdbEventOperation::getEvent() const {
  return m_impl.m_eventImpl->m_facade;
}
const NdbRecAttr *NdbEventOperation::getFirstPkAttr() const {
  return m_impl.theFirstPkAttrs[0];
}
const NdbRecAttr *NdbEventOperation::getFirstPkPreAttr() const {
  return m_impl.theFirstPkAttrs[1];
}
const NdbRecAttr *NdbEventOperation::getFirstDataAttr() const {
  return m_impl.theFirstDataAttrs[0];
}
const NdbRecAttr *NdbEventOperation::getFirstDataPreAttr() const {
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
void NdbEventOperation::setCustomData(void *data) {
  m_impl.m_custom_data = data;
}
void *NdbEventOperation::getCustomData() const { return m_impl.m_custom_data; }

void NdbEventOperation::setFilterAnyvalueMySQLNoLogging() {
  m_impl.m_requestInfo |= SubStartReq::FILTER_ANYVALUE_MYSQL_NO_LOGGING;
}

void NdbEventOperation::setFilterAnyvalueMySQLNoReplicaUpdates() {
  m_impl.m_requestInfo |=
      SubStartReq::SubStartReq::FILTER_ANYVALUE_MYSQL_NO_REPLICA_UPDATES;
}

void NdbEventOperation::setAnyValueFilter(AnyValueFilterFn fn) {
  m_impl.m_any_value_filter = fn;
}

int NdbEventOperation::getReqNodeId() const {
  return SubTableData::getReqNodeId(m_impl.m_data_item->sdata->requestInfo);
}

int NdbEventOperation::getNdbdNodeId() const {
  return SubTableData::getNdbdNodeId(m_impl.m_data_item->sdata->requestInfo);
}

/*
 * Private members
 */

const struct NdbError &NdbEventOperation::getNdbError() const {
  return m_impl.getNdbError();
}

void NdbEventOperation::setAllowEmptyUpdate(bool allow) {
  m_impl.m_allow_empty_update = allow;
}

bool NdbEventOperation::getAllowEmptyUpdate() {
  return m_impl.m_allow_empty_update;
}
