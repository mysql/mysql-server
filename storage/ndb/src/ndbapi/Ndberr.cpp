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

#include "API.hpp"

static void update(NdbError &error) {
  ndberror_struct ndberror = (ndberror_struct)error;
  ndberror_update(&ndberror);
  error = NdbError(ndberror);
}

const NdbError &Ndb::getNdbError(int code) {
  theError.code = code;
  update(theError);
  return theError;
}

const NdbError &Ndb::getNdbError() const {
  update(theError);
  return theError;
}

const NdbError &NdbDictionaryImpl::getNdbError() const {
  update(m_error);
  return m_error;
}

const NdbError &NdbTransaction::getNdbError() const {
  update(theError);
  return theError;
}

const NdbError &NdbOperation::getNdbError() const {
  update(theError);
  return theError;
}

const NdbError &NdbBlob::getNdbError() const {
  update(theError);
  return theError;
}

const NdbError &NdbEventOperationImpl::getNdbError() const {
  update(m_error);
  return m_error;
}

const NdbError &NdbDictInterface::getNdbError() const {
  update(m_error);
  return m_error;
}

const NdbError &NdbQueryBuilderImpl::getNdbError() const {
  update(m_error);
  return m_error;
}

const NdbError &NdbQueryImpl::getNdbError() const {
  update(m_error);
  return m_error;
}

const NdbIndexStat::Error &NdbIndexStatImpl::getNdbError() const {
  update(m_error);
  return m_error;
}
