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


#include <NdbError.hpp>
#include <NdbStdio.h> 
#include <stdarg.h>

#include <assert.h>

#include <NdbOut.hpp>

const char *ndberror_status_message(const NdbError::Status & status);
const char *ndberror_classification_message(const NdbError::Classification & classification);
int ndb_error_string(int err_no, char *str, size_t size);
void ndberror_update(const NdbError & _err);

/**
 * operators
 */
NdbOut &
operator<<(NdbOut & out, const NdbError & error){
  if(error.message != 0)
    out << error.code << ": " << error.message;
  else
    out << error.code << ": ";
  return out;
}

NdbOut &
operator<<(NdbOut & out, const NdbError::Status & status){
  return out << ndberror_status_message(status);
}

NdbOut &
operator<<(NdbOut & out, const NdbError::Classification & classification){
  return out << ndberror_classification_message(classification);
}

/******************************************************
 *
 */
#include "NdbImpl.hpp"
#include "NdbDictionaryImpl.hpp"
#include <NdbSchemaCon.hpp>
#include <NdbOperation.hpp>
#include <NdbConnection.hpp>


const 
NdbError & 
Ndb::getNdbError(int code){
  theError.code = code;
  ndberror_update(theError);
  return theError;
}

const 
NdbError & 
Ndb::getNdbError() const {
  ndberror_update(theError);
  return theError;
}

const 
NdbError & 
NdbDictionaryImpl::getNdbError() const {
  ndberror_update(m_error);
  return m_error;
}

const 
NdbError & 
NdbConnection::getNdbError() const {
  ndberror_update(theError);
  return theError;
}

const 
NdbError & 
NdbOperation::getNdbError() const {
  ndberror_update(theError);
  return theError;
}

const 
NdbError & 
NdbSchemaCon::getNdbError() const {
  ndberror_update(theError);
  return theError;
}



