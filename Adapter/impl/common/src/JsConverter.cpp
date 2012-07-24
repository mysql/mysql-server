/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include "JsConverter.h"


/*****************************************************************
 JsValueConverter specializations
 Value Conversion from JavaScript to C
******************************************************************/

// Instantiations:

template class JsValueConverter <int>;
template class JsValueConverter <uint32_t>;
template class JsValueConverter <double>;
template class JsValueConverter <int64_t>;
template class JsValueConverter <bool>;
template class JsValueConverter <const char *>;


/*****************************************************************
 toJs specializations
 Value Conversion from C to JavaScript
******************************************************************/


// int
template <>
Local<Value> toJS<int>(int cval){ 
  return Number::New(cval);
};

// uint32_t
template <>
Local<Value> toJS<uint32_t>(uint32_t cval) {
  return Uint32::New(cval);
};

// double
template <>
Local<Value> toJS<double>(double cval) {
  return Number::New(cval);
};

// const char *
template <> 
Local<Value> toJS<const char *>(const char * cval) {
  return v8::String::New(cval);
}

