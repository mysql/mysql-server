/*
   Copyright (C) 2004-2006 MySQL AB
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


#include <ndb_global.h>

#include <NdbOut.hpp>

#include <NdbError.hpp>

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
  return out << ndberror_status_message((ndberror_status)status);
}

NdbOut &
operator<<(NdbOut & out, const NdbError::Classification & classification){
  return out << ndberror_classification_message((ndberror_classification)classification);
}

