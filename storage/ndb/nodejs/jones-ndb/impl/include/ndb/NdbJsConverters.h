/*
 Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <NdbApi.hpp>

#include "JsConverter.h"

/* Template Specializations that are specific to NDBAPI */


/*****************************************************************
 JsValueConverter 
 Value conversion from JavaScript to C
******************************************************************/

template <>
class JsValueConverter <NdbTransaction::ExecType> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  NdbTransaction::ExecType toC() { 
    return static_cast<NdbTransaction::ExecType>(jsval->Int32Value());
  }
};

template <>
class JsValueConverter <NdbTransaction::CommitStatusType> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  NdbTransaction::CommitStatusType toC() { 
    return static_cast<NdbTransaction::CommitStatusType>(jsval->Int32Value());
  }
};

template <>
class JsValueConverter <NdbOperation::AbortOption> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  NdbOperation::AbortOption toC() { 
    return static_cast<NdbOperation::AbortOption>(jsval->Int32Value());
  }
};

template <>
class JsValueConverter <NdbScanFilter::Group> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  NdbScanFilter::Group toC() { 
    return static_cast<NdbScanFilter::Group>(jsval->Int32Value());
  }
};

/*****************************************************************
 toJs functions
 Value Conversion from C to JavaScript
******************************************************************/

// int
template <>
inline Local<Value> toJS<NdbTransaction::CommitStatusType>
                        (Isolate * isolate,
                         NdbTransaction::CommitStatusType cval) {
  return v8::Number::New(isolate, static_cast<int>(cval));
};

