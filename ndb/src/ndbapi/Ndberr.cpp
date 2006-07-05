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
#include "NdbImpl.hpp"
#include "NdbDictionaryImpl.hpp"
#include <NdbOperation.hpp>
#include <NdbTransaction.hpp>
#include <NdbBlob.hpp>

static void
update(const NdbError & _err){
  NdbError & error = (NdbError &) _err;
  ndberror_struct ndberror = (ndberror_struct)error;
  ndberror_update(&ndberror);
  error = NdbError(ndberror);
}

const 
NdbError & 
Ndb::getNdbError(int code){
  theError.code = code;
  update(theError);
  return theError;
}

const 
NdbError & 
Ndb::getNdbError() const {
  update(theError);
  return theError;
}

const 
NdbError & 
NdbDictionaryImpl::getNdbError() const {
  update(m_error);
  return m_error;
}

const 
NdbError & 
NdbTransaction::getNdbError() const {
  update(theError);
  return theError;
}

const 
NdbError & 
NdbOperation::getNdbError() const {
  update(theError);
  return theError;
}

const
NdbError &
NdbBlob::getNdbError() const {
  update(theError);
  return theError;
}
