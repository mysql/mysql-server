/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include <NdbMutex.h>

#include "TraceNdbApi.hpp"


int g_nParamTrace;
NdbMutex* g_pNdbMutexTrace = 0;


void TraceBegin(void)
{
  if(!g_pNdbMutexTrace)
  {
    g_pNdbMutexTrace = NdbMutex_Create();
  }
  NdbMutex_Lock(g_pNdbMutexTrace);
  g_nParamTrace = 0;
}

void TraceEnd(void)
{
  ndbout << endl;
  g_nParamTrace = 0;
  NdbMutex_Unlock(g_pNdbMutexTrace);
}

void TraceMethod(const char* szMethod)
{
  ndbout << "->" << szMethod << "(";
  g_nParamTrace = 0;
}

void TraceParamComma(void)
{
  if(g_nParamTrace)
  {
    ndbout << ", ";
  }
  ++g_nParamTrace;
}

void TraceNdb(Ndb* pNdb)
{
  TraceParamComma();
  ndbout << "((Ndb*)" << hex << (Uint32)pNdb << ")";
}

void TraceNdbSchemaCon(NdbSchemaCon* pNdbSchemaCon)
{
  TraceParamComma();
  ndbout << "((NdbSchemaCon*)" << hex << (Uint32)pNdbSchemaCon << ")";
}

void TraceNdbSchemaOp(NdbSchemaOp* pNdbSchemaOp)
{
  TraceParamComma();
  ndbout << "((NdbSchemaOp*)" << hex << (Uint32)pNdbSchemaOp << ")";
}

void TraceNdbConnection(const NdbConnection* pNdbConnection)
{
  TraceParamComma();
  ndbout << "((NdbConnection*)" << hex << (Uint32)pNdbConnection << ")";
}

void TraceNdbOperation(NdbOperation* pNdbOperation)
{
  TraceParamComma();
  ndbout << "((NdbOperation*)" << hex << (Uint32)pNdbOperation << ")";
}

void TraceNdbIndexOperation(NdbIndexOperation* pNdbIndexOperation)
{
  TraceParamComma();
  ndbout << "((NdbIndexOperation*)" << hex << (Uint32)pNdbIndexOperation << ")";
}

void TraceNdbRecAttr(NdbRecAttr* pNdbRecAttr)
{
  TraceParamComma();
  ndbout << "((NdbRecAttr*)" << hex << (Uint32)pNdbRecAttr << ")";
}

void TraceTable(Table* pTable)
{
  TraceParamComma();
  ndbout << "((Table*)" << hex << (Uint32)pTable << ")";
}

void TraceString(const char* szParam)
{
  TraceParamComma();
  ndbout << "\"" << szParam << "\"";
}

void TraceInt(const int i)
{
  TraceParamComma();
  ndbout << "(int)" << dec << i;
}

void TraceUint32(const Uint32 n)
{
  TraceParamComma();
  ndbout << "(Uint32)" << dec << n;
}

void TraceKeyType(const KeyType aKeyType)
{
  TraceParamComma();
  switch(aKeyType)
  {
  case Undefined: ndbout << "Undefined"; break;
  case NoKey: ndbout << "NoKey"; break;
  case TupleKey: ndbout << "TupleKey"; break;
  case TupleId: ndbout << "TupleId"; break;
  default:  ndbout << "(KeyType)" << aKeyType; break;
  }
}

void TraceExecType(const ExecType aExecType)
{
  switch(aExecType)
  {
  case NoExecTypeDef: ndbout << "NoExecTypeDef"; break;
  case Prepare: ndbout << "Prepare"; break;
  case NoCommit: ndbout << "NoCommit"; break;
  case Commit: ndbout << "Commit"; break;
  case Rollback: ndbout << "Rollback"; break;
  default: ndbout << "(ExecType)" << aExecType; break;
  }
}


void TraceNdbError(const NdbError& err)
{
  TraceParamComma();
  ndbout << "(NdbError)" << err;
}



void TraceVoid(void)
{
  ndbout << "void";
}

void TraceReturn(void)
{
  ndbout << "); // return ";
  g_nParamTrace = 0;
}


// TraceNdbSchemaOp

int CTraceNdbSchemaOp::createTable(const char* aTableName)
{
  int i = NdbSchemaOp::createTable(aTableName);
  TraceBegin();
  TraceNdbSchemaOp(this);
  TraceMethod("createTable");
  TraceString(aTableName);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

int CTraceNdbSchemaOp::createAttribute(const char* aAttrName, KeyType aTupleyKey)
{
  int i = NdbSchemaOp::createAttribute(aAttrName, aTupleyKey);
  TraceBegin();
  TraceNdbSchemaOp(this);
  TraceMethod("createAttribute");
  TraceString(aAttrName);
  TraceKeyType(aTupleyKey);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}



// TraceNdbSchemaCon

CTraceNdbSchemaOp* CTraceNdbSchemaCon::getNdbSchemaOp()
{ 
  NdbSchemaOp* pNdbSchemaOp = NdbSchemaCon::getNdbSchemaOp();
  TraceBegin();
  TraceNdbSchemaCon(this);
  TraceMethod("getNdbSchemaOp");
  TraceReturn();
  TraceNdbSchemaOp(pNdbSchemaOp);
  TraceEnd();
  return (CTraceNdbSchemaOp*)pNdbSchemaOp; 
}

int CTraceNdbSchemaCon::execute()
{
  int i = NdbSchemaCon::execute();
  TraceBegin();
  TraceNdbSchemaCon(this);
  TraceMethod("execute");
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}



// TraceNdbRecAttr

Uint32 CTraceNdbRecAttr::u_32_value()
{
  Uint32 n = NdbRecAttr::u_32_value();
  TraceBegin();
  TraceNdbRecAttr(this);
  TraceMethod("u_32_value");
  TraceReturn();
  TraceUint32(n);
  TraceEnd();
  return n;
}



// TraceNdbOperation

int CTraceNdbOperation::insertTuple()
{
  int i = NdbOperation::insertTuple();
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("insertTuple");
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

int CTraceNdbOperation::updateTuple()
{
  int i = NdbOperation::updateTuple();
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("updateTuple");
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

int CTraceNdbOperation::interpretedUpdateTuple()
{
  int i = NdbOperation::interpretedUpdateTuple();
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("interpretedUpdateTuple");
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

int CTraceNdbOperation::readTuple()
{
  int i = NdbOperation::readTuple();
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("readTuple");
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}


int CTraceNdbOperation::readTupleExclusive()
{
  int i = NdbOperation::readTupleExclusive();
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("readTupleExclusive");
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}


int CTraceNdbOperation::deleteTuple()
{
  int i = NdbOperation::deleteTuple();
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("deleteTuple");
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

int CTraceNdbOperation::equal(const char* anAttrName, Uint32 aValue)
{
  int i = NdbOperation::equal(anAttrName, aValue);
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("equal");
  TraceString(anAttrName);
  TraceUint32(aValue);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

int CTraceNdbOperation::setValue(const char* anAttrName, Uint32 aValue)
{
  int i = NdbOperation::setValue(anAttrName, aValue);
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("setValue");
  TraceString(anAttrName);
  TraceUint32(aValue);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

int CTraceNdbOperation::incValue(const char* anAttrName, Uint32 aValue)
{
  int i = NdbOperation::incValue(anAttrName, aValue);
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("incValue");
  TraceString(anAttrName);
  TraceUint32(aValue);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

CTraceNdbRecAttr* CTraceNdbOperation::getValue(const char* anAttrName)
{
  NdbRecAttr* pNdbRecAttr = NdbOperation::getValue(anAttrName);
  TraceBegin();
  TraceNdbOperation(this);
  TraceMethod("getValue");
  TraceString(anAttrName);
  TraceReturn();
  TraceNdbRecAttr(pNdbRecAttr);
  TraceEnd();
  return (CTraceNdbRecAttr*)pNdbRecAttr;
}


// TraceNdbIndexOperation

int CTraceNdbIndexOperation::equal(const char* anAttrName, Uint32 aValue)
{
  int i = NdbIndexOperation::equal(anAttrName, aValue);
  TraceBegin();
  TraceNdbIndexOperation(this);
  TraceMethod("equal");
  TraceString(anAttrName);
  TraceUint32(aValue);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}


int CTraceNdbIndexOperation::incValue(const char* anAttrName, Uint32 aValue)
{
  int i = NdbIndexOperation::incValue(anAttrName, aValue);
  TraceBegin();
  TraceNdbIndexOperation(this);
  TraceMethod("incValue");
  TraceString(anAttrName);
  TraceUint32(aValue);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}


CTraceNdbRecAttr* CTraceNdbIndexOperation::getValue(const char* anAttrName)
{
  NdbRecAttr* pNdbRecAttr = NdbIndexOperation::getValue(anAttrName);
  TraceBegin();
  TraceNdbIndexOperation(this);
  TraceMethod("getValue");
  TraceString(anAttrName);
  TraceReturn();
  TraceNdbRecAttr(pNdbRecAttr);
  TraceEnd();
  return (CTraceNdbRecAttr*)pNdbRecAttr;
}


// TraceNdbConnection

CTraceNdbOperation* CTraceNdbConnection::getNdbOperation(const char* aTableName)
{
  NdbOperation* pNdbOperation = NdbConnection::getNdbOperation(aTableName);
  TraceBegin();
  TraceNdbConnection(this);
  TraceMethod("getNdbOperation");
  TraceString(aTableName);
  TraceReturn();
  TraceNdbOperation(pNdbOperation);
  TraceEnd();
  return (CTraceNdbOperation*)pNdbOperation; 
}

CTraceNdbIndexOperation* CTraceNdbConnection::getNdbIndexOperation(const char* anIndexName, const char* aTableName)
{
  NdbIndexOperation* pNdbIndexOperation = NdbConnection::getNdbIndexOperation(anIndexName, aTableName);
  TraceBegin();
  TraceNdbConnection(this);
  TraceMethod("getNdbIndexOperation");
  TraceString(anIndexName);
  TraceString(aTableName);
  TraceReturn();
  TraceNdbIndexOperation(pNdbIndexOperation);
  TraceEnd();
  return (CTraceNdbIndexOperation*)pNdbIndexOperation; 
}

int CTraceNdbConnection::execute(ExecType aTypeOfExec)
{
  int i = NdbConnection::execute(aTypeOfExec);
  TraceBegin();
  TraceNdbConnection(this);
  TraceMethod("execute");
  TraceExecType(aTypeOfExec);
  TraceReturn();
  TraceInt(i);
  TraceEnd();
  return i;
}

const NdbError & CTraceNdbConnection::getNdbError(void) const
{
  const NdbError& err = NdbConnection::getNdbError();
  TraceBegin();
  TraceNdbConnection(this);
  TraceMethod("getNdbError");
  TraceReturn();
  TraceNdbError(err);
  TraceEnd();
  return err;
}



// TraceNdb

CTraceNdb::CTraceNdb(const char* aDataBase)
: Ndb(aDataBase) 
{
  TraceBegin();
  TraceNdb(this);
  TraceMethod("Ndb");
  TraceString(aDataBase);
  TraceReturn();
  TraceVoid();
  TraceEnd();
}

CTraceNdbSchemaCon* CTraceNdb::startSchemaTransaction() 
{ 
  NdbSchemaCon* pNdbSchemaCon = Ndb::startSchemaTransaction();
  TraceBegin();
  TraceNdb(this);
  TraceMethod("startSchemaTransaction");
  TraceReturn();
  TraceNdbSchemaCon(pNdbSchemaCon);
  TraceEnd();
  return (CTraceNdbSchemaCon*)pNdbSchemaCon;
}

void CTraceNdb::closeSchemaTransaction(CTraceNdbSchemaCon* aSchemaCon)
{
  Ndb::closeSchemaTransaction(aSchemaCon);
  TraceBegin();
  TraceNdb(this);
  TraceMethod("closeSchemaTransaction");
  TraceReturn();
  TraceVoid();
  TraceEnd();
}

CTraceNdbConnection* CTraceNdb::startTransaction()
{ 
  NdbConnection* pNdbConnection = Ndb::startTransaction();
  TraceBegin();
  TraceNdb(this);
  TraceMethod("startTransaction");
  TraceReturn();
  TraceNdbConnection(pNdbConnection);
  TraceEnd();
  return (CTraceNdbConnection*)pNdbConnection;
}

void CTraceNdb::closeTransaction(CTraceNdbConnection* aConnection)
{
  Ndb::closeTransaction(aConnection);
  TraceBegin();
  TraceNdb(this);
  TraceMethod("closeTransaction");
  TraceNdbConnection(aConnection);
  TraceReturn();
  TraceVoid();
  TraceEnd();
}


