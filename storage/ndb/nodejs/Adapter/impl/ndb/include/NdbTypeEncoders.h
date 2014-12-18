/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

#include <NdbApi.hpp>
#include <node.h>

using namespace v8;

typedef Handle<Value> EncoderReader(const NdbDictionary::Column *, 
                                    char *, size_t);

typedef Handle<Value> EncoderWriter(const NdbDictionary::Column *, 
                                    Handle<Value>, char *, size_t);

typedef struct {
  EncoderReader * read;
  EncoderWriter * write;
  unsigned int flags;
} NdbTypeEncoder;

const NdbTypeEncoder * getEncoderForColumn(const NdbDictionary::Column *);

Handle<Object> getBufferForText(const NdbDictionary::Column *, Handle<String>);
Handle<String> getTextFromBuffer(const NdbDictionary::Column *, Handle<Object>);
