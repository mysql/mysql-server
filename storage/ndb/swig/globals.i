/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(SWIGLUA)
%include "typemaps.i"
#endif

%include "exception.i"

 //%include "cstring.i"

%include "ndberror.h"
%include "ndb_constants.h"
%include "ndb_init.h"

 // Suppress SWIG warning
#pragma SWIG nowarn=SWIGWARN_PARSE_NESTED_CLASS

%define %ndbexception(EXCEPTION)
#if defined(SWIGJAVA)
%javaexception(EXCEPTION)
#else
%exception
#endif
%enddef

%define %ndbnoexception
#if defined(SWIGJAVA)
%nojavaexception;
#else
%noexception; // clear exception handler
#endif
%enddef

%{
  enum NdbException {
    BaseRuntimeError,
    NdbApiException,
    BlobUndefinedException,
    NdbApiPermanentException,
    NdbApiRuntimeException,
    NdbApiTemporaryException,
    NdbApiTimeStampOutOfBoundsException,
    NdbApiUserErrorPermanentException,
    NdbClusterConnectionPermanentException,
    NdbClusterConnectionTemporaryException,
    NoSuchColumnException,
    NoSuchIndexException,
    NoSuchTableException,
    NdbMgmException,
    InvalidSchemaObjectVersionException,
  };
  %}

typedef unsigned long long Uint64;
typedef unsigned int Uint32;
typedef signed long long Int64;
typedef signed int Int32;
