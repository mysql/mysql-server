/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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

/***  This file includes public wrapper functions exported to C++ code 
      Users of these wrappers must supply their HandleScopes
***/

#include "Record.h"
#include "JsWrapper.h"

class BatchImpl;
class QueryOperation;

Local<Value> Record_Wrapper(const Record *);
Local<Value> Ndb_Wrapper(Ndb *);
Local<Value> NdbError_Wrapper(const NdbError &);
Local<Value> BatchImpl_Wrapper(BatchImpl *);
Local<Value> BatchImpl_Recycle(Local<Object>, BatchImpl *);
Local<Value> QueryOperation_Wrapper(QueryOperation *);

/* Not actual wrapper functions, but functions that provide an envelope */

Envelope * getNdbInterpretedCodeEnvelope(void);
Envelope * getConstNdbInterpretedCodeEnvelope(void);
Envelope * getNdbDictTableEnvelope(void);
Envelope * getNdbScanOperationEnvelope(void);
