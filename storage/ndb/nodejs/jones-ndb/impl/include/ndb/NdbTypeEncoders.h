/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#include <node.h>
#include <NdbApi.hpp>

using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

typedef Local<Value> EncoderReader(const NdbDictionary::Column *, char *,
                                   uint32_t);

typedef Local<Value> EncoderWriter(const NdbDictionary::Column *, Local<Value>,
                                   char *, uint32_t);

typedef struct {
  EncoderReader *read;
  EncoderWriter *write;
  unsigned int flags;
} NdbTypeEncoder;

const NdbTypeEncoder *getEncoderForColumn(const NdbDictionary::Column *);

Local<Object> getBufferForText(const NdbDictionary::Column *, Local<String>);
Local<String> getTextFromBuffer(const NdbDictionary::Column *, Local<Object>);
