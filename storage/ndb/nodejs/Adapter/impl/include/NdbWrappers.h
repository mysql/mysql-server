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

#ifndef NDBWRAPPERS_H
#define NDBWRAPPERS_H
#pragma once


/***  This file includes public wrapper functions exported to C++ code 
***/

#include "Record.h"
#include "Operation.h"

using namespace v8;

Handle<Value> Record_Wrapper(Record *);
Handle<Value> Ndb_Wrapper(Ndb *);
Handle<Value> NdbError_Wrapper(const NdbError &);
Handle<Value> NdbOperation_Wrapper(const NdbOperation *);
Handle<Value> NdbScanOperation_Wrapper(NdbScanOperation *);


/* Not actual wrapper functions, but functions that provide an envelope */

Envelope * getNdbTransactionEnvelope(void);
Envelope * getNdbInterpretedCodeEnvelope(void);
Envelope * getConstNdbInterpretedCodeEnvelope(void);
Envelope * getNdbDictTableEnvelope(void);
Envelope * getNdbScanOperationEnvelope(void);
#endif // NDBWRAPPERS_H

