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

/***  This file includes public wrapper functions exported to C++ code 
***/

#include "Record.h"
#include "JsWrapper.h"

class DBOperationSet;

Handle<Value> Record_Wrapper(const Record *);
Handle<Value> Ndb_Wrapper(Ndb *);
Handle<Value> NdbError_Wrapper(const NdbError &);
Handle<Value> NdbScanOperation_Wrapper(NdbScanOperation *);
Handle<Value> DBOperationSet_Wrapper(DBOperationSet *);
Handle<Value> DBOperationSet_Recycle(Handle<Object>, DBOperationSet *);

/* Not actual wrapper functions, but functions that provide an envelope */

Envelope * getNdbInterpretedCodeEnvelope(void);
Envelope * getConstNdbInterpretedCodeEnvelope(void);
Envelope * getNdbDictTableEnvelope(void);
Envelope * getNdbScanOperationEnvelope(void);
