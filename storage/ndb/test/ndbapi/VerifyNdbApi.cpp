/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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
#include <NdbOut.hpp>
#include <NdbMutex.h>

#include "VerifyNdbApi.hpp"


NdbMutex* g_pNdbMutexVerify = 0;


void VerifyBegin(void)
{
  if(!g_pNdbMutexVerify)
  {
    g_pNdbMutexVerify = NdbMutex_Create();
  }
  NdbMutex_Lock(g_pNdbMutexVerify);
}

void VerifyEnd(void)
{
  NdbMutex_Unlock(g_pNdbMutexVerify);
}



void CVerifyNdbSchemaOp::VerifyIntError(const int i, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbSchemaOp::" << szMethod << " returned " << dec << i;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbSchemaCon::VerifyIntError(const int i, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbSchemaCon::" << szMethod << " returned " << dec << i;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbSchemaCon::VerifyPtrError(void* p, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbSchemaCon::" << szMethod << " returned " << hex << (Uint32)p;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbRecAttr::VerifyValueError(const int iNull, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbRecAttr::" << szMethod << " : isNULL() returned " << dec << iNull;
  ndbout << endl;
  VerifyEnd();
}


void CVerifyNdbOperation::VerifyIntError(const int i, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbOperation::" << szMethod << " returned " << dec << i;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbOperation::VerifyPtrError(void* p, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbOperation::" << szMethod << " returned " << hex << (Uint32)p;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbIndexOperation::VerifyIntError(const int i, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbIndexOperation::" << szMethod << " returned " << dec << i;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbIndexOperation::VerifyPtrError(void* p, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbIndexOperation::" << szMethod << " returned " << hex << (Uint32)p;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbConnection::VerifyIntError(const int i, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbConnection::" << szMethod << " returned " << dec << i;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdbConnection::VerifyPtrError(void* p, const char* szMethod)
{
  VerifyBegin();
  ndbout << "NdbConnection::" << szMethod << " returned " << hex << (Uint32)p;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdb::VerifyPtrError(void* p, const char* szMethod)
{
  VerifyBegin();
  ndbout << "Ndb::" << szMethod << " returned " << hex << (Uint32)p;
  ndbout << " : " << dec << getNdbError().code << " : " << getNdbError().message << endl;
  VerifyEnd();
}


void CVerifyNdb::VerifyVoidError(const int iCode, const char* szMethod)
{
  VerifyBegin();
  ndbout << "Ndb::" << szMethod << " : getNdbError().code returned " << dec << iCode;
  ndbout << " : " << getNdbError().message << endl;
  VerifyEnd();
}


