/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NdbApi.hpp>
#include <NdbSchemaCon.hpp>
#include <NdbMutex.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <NdbTick.h>
#include <NdbMain.h>
#include <NdbTest.hpp>
#include <random.h>

//#define TRACE
#define DEBUG
//#define RELEASE
#define NODE_REC // epaulsa: introduces pointer checks to help 'acid' keep core
// during node recovery

#ifdef TRACE

#define VerifyMethodInt(c, m) (ReportMethodInt(c->m, c, #c, #m, __FILE__, __LINE__))
#define VerifyMethodPtr(v, c, m) (v=ReportMethodPtr(c->m, c, #v, #c, #m, __FILE__, __LINE__))
#define VerifyMethodVoid(c, m) (c->m, ReportMethodVoid(c, #c, #m, __FILE__, __LINE__))

int ReportMethodInt(int iRes, NdbConnection* pNdbConnection, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  ndbout << szFile << "(" << iLine << ") : ";
  ndbout << szClass << "->" << szMethod << " return " << iRes << " : ";
  ndbout << pNdbConnection->getNdbError();
  NdbOperation* pNdbOperation = pNdbConnection->getNdbErrorOperation();
  if(pNdbOperation) {
    ndbout << " : " << pNdbOperation->getNdbError();
  }
  ndbout << " : " << pNdbConnection->getNdbErrorLine();
  ndbout << endl;
  return iRes;
}

template <class C>
int ReportMethodInt(int iRes, C* pC, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  ndbout << szFile << "(" << iLine << ") : ";
  ndbout << szClass << "->" << szMethod << " return " << iRes << " : ";
  ndbout << pC->getNdbError();
  ndbout << endl;
  return iRes;
}

template <class R, class C>
R* ReportMethodPtr(R* pR, C* pC, const char* szVariable, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  ndbout << szFile << "(" << iLine << ") : ";
  ndbout << szVariable << " = " << szClass << "->" << szMethod << " return " << (long)(void*)pR << " : ";
  ndbout << pC->getNdbError();
  ndbout << endl;
  return pR;
}

template <class C>
void ReportMethodVoid(C* pC, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  ndbout << szFile << "(" << iLine << ") : ";
  ndbout << szClass << "->" << szMethod << " : ";
  ndbout << pC->getNdbError();
  ndbout << endl;
}
#endif /* TRACE */


#ifdef DEBUG

#define VerifyMethodInt(c, m) (ReportMethodInt(c->m, c, #c, #m, __FILE__, __LINE__))
#define VerifyMethodPtr(v, c, m) (v=ReportMethodPtr(c->m, c, #v, #c, #m, __FILE__, __LINE__))
#define VerifyMethodVoid(c, m) (c->m, ReportMethodVoid(c, #c, #m, __FILE__, __LINE__))

int ReportMethodInt(int iRes, NdbConnection* pNdbConnection, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  if(iRes<0) {
    ndbout << szFile << "(" << iLine << ") : ";
    ndbout << szClass << "->" << szMethod << " return " << iRes << " : ";
    ndbout << pNdbConnection->getNdbError();
    NdbOperation* pNdbOperation = pNdbConnection->getNdbErrorOperation();
    if(pNdbOperation) {
      ndbout << " : " << pNdbOperation->getNdbError();
    }
    ndbout << " : " << pNdbConnection->getNdbErrorLine();
    ndbout << " : ";
    ndbout << endl;
  }
  return iRes;
}

template <class C>
int ReportMethodInt(int iRes, C* pC, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  if(iRes<0) {
    ndbout << szFile << "(" << iLine << ") : ";
    ndbout << szClass << "->" << szMethod << " return " << iRes << " : ";
    ndbout << pC->getNdbError();
    ndbout << endl;
  }
  return iRes;
}

template <class R, class C>
R* ReportMethodPtr(R* pR, C* pC, const char* szVariable, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  if(!pR) {
    ndbout << szFile << "(" << iLine << ") : ";
    ndbout << szVariable << " = " << szClass << "->" << szMethod << " return " << " : ";
    ndbout << pC->getNdbError();
    ndbout << endl;
  }
  return pR;
}

template <class C>
void ReportMethodVoid(C* pC, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  if(pC->getNdbError().code) {
    ndbout << szFile << "(" << iLine << ") : ";
    ndbout << szClass << "->" << szMethod << " : ";
    ndbout << pC->getNdbError();
    ndbout << endl;
  }
}


#endif /* DEBUG */


#ifdef RELEASE

#define VerifyMethodInt(c, m) (c->m)
#define VerifyMethodPtr(v, c, m) (v=(c->m))
#define VerifyMethodVoid(c, m) (c->m)

int ReportMethodInt(int iRes, NdbConnection* pNdbConnection, const char* szClass, const char* szMethod, const char* szFile, const int iLine)
{
  if(iRes<0) {
    ndbout << szFile << "(" << iLine << ") : ";
    ndbout << szClass << "->" << szMethod << " return " << iRes << " : ";
    ndbout << pNdbConnection->getNdbError();
    NdbOperation* pNdbOperation = pNdbConnection->getNdbErrorOperation();
    if(pNdbOperation) {
      ndbout << " : " << pNdbOperation->getNdbError();
    }
    ndbout << " : " << pNdbConnection->getNdbErrorLine();
    ndbout << endl;
  }
  return iRes;
}

#endif /* RELEASE */

// epaulsa =>
#ifndef NODE_REC
#define CHK_TR(p)
#else
#define CHK_TR(p) if(!p){													\
			ndbout <<"startTransaction failed, returning now." << endl ;	\
			delete pNdb ;													\
			pNdb = NULL ;													\
			return 0 ;														\
		}																	
#endif // NODE_REC
// <= epaulsa

const char* c_szWarehouse = "WAREHOUSE";
const char* c_szWarehouseNumber = "W_ID";
const char* c_szWarehouseSum = "W_SUM";
const char* c_szWarehouseCount = "W_CNT";
const char* c_szDistrict = "DISTRICT";
const char* c_szDistrictWarehouseNumber = "D_W_ID";
const char* c_szDistrictNumber = "D_ID";
const char* c_szDistrictSum = "D_SUM";
const char* c_szDistrictCount = "D_CNT";

Uint32 g_nWarehouseCount = 10;
Uint32 g_nDistrictPerWarehouse = 10;
Uint32 g_nThreadCount = 1;
NdbMutex* g_pNdbMutex = 0;

extern "C" void* NdbThreadFuncInsert(void* pArg)
{
  myRandom48Init((long int)NdbTick_CurrentMillisecond());
  unsigned nSucc = 0;
  unsigned nFail = 0;
  Ndb* pNdb = NULL ;
  pNdb = new Ndb("TEST_DB");
  VerifyMethodInt(pNdb, init());
  VerifyMethodInt(pNdb, waitUntilReady());

  while(NdbMutex_Trylock(g_pNdbMutex)) {
    Uint32 nWarehouse = myRandom48(g_nWarehouseCount);
    NdbConnection* pNdbConnection = NULL ;
    VerifyMethodPtr(pNdbConnection, pNdb, startTransaction());
    CHK_TR(pNdbConnection);
    NdbOperation* pNdbOperationW = NULL ;
    VerifyMethodPtr(pNdbOperationW, pNdbConnection, getNdbOperation(c_szWarehouse));
    VerifyMethodInt(pNdbOperationW, insertTuple());
    VerifyMethodInt(pNdbOperationW, equal(c_szWarehouseNumber, nWarehouse));
    VerifyMethodInt(pNdbOperationW, setValue(c_szWarehouseCount, Uint32(1)));
    Uint32 nWarehouseSum = 0;
    for(Uint32 nDistrict=0; nDistrict<g_nDistrictPerWarehouse; ++nDistrict) {
      NdbOperation* pNdbOperationD = NULL ;
      VerifyMethodPtr(pNdbOperationD, pNdbConnection, getNdbOperation(c_szDistrict));
      VerifyMethodInt(pNdbOperationD, insertTuple());
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictWarehouseNumber, nWarehouse));
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictNumber, nDistrict));
      VerifyMethodInt(pNdbOperationD, setValue(c_szDistrictCount, Uint32(1)));
      Uint32 nDistrictSum = myRandom48(100);
      nWarehouseSum += nDistrictSum;
      VerifyMethodInt(pNdbOperationD, setValue(c_szDistrictSum, nDistrictSum));
    }
    VerifyMethodInt(pNdbOperationW, setValue(c_szWarehouseSum, nWarehouseSum));
    int iExec = pNdbConnection->execute(Commit);
    int iError = pNdbConnection->getNdbError().code;

    if(iExec<0 && iError!=0 && iError!=266 && iError!=630) {
      ReportMethodInt(iExec, pNdbConnection, "pNdbConnection", "execute(Commit)", __FILE__, __LINE__);
    }
    if(iExec==0) {
      ++nSucc;
    } else {
      ++nFail;
    }
    VerifyMethodVoid(pNdb, closeTransaction(pNdbConnection));
  }
  ndbout << "insert: " << nSucc << " succeeded, " << nFail << " failed " << endl;
  NdbMutex_Unlock(g_pNdbMutex);
  delete pNdb;
  pNdb = NULL ;
  return NULL;
}


extern "C" void* NdbThreadFuncUpdate(void* pArg)
{
  myRandom48Init((long int)NdbTick_CurrentMillisecond());
  unsigned nSucc = 0;
  unsigned nFail = 0;
  Ndb* pNdb = NULL ;
  pNdb = new Ndb("TEST_DB");
  VerifyMethodInt(pNdb, init());
  VerifyMethodInt(pNdb, waitUntilReady());

  while(NdbMutex_Trylock(g_pNdbMutex)) {
    Uint32 nWarehouse = myRandom48(g_nWarehouseCount);
    NdbConnection* pNdbConnection = NULL ;
    VerifyMethodPtr(pNdbConnection, pNdb, startTransaction());
    CHK_TR(pNdbConnection) ; // epaulsa
    NdbOperation* pNdbOperationW = NULL ;
    VerifyMethodPtr(pNdbOperationW, pNdbConnection, getNdbOperation(c_szWarehouse));
    VerifyMethodInt(pNdbOperationW, interpretedUpdateTuple());
    VerifyMethodInt(pNdbOperationW, equal(c_szWarehouseNumber, nWarehouse));
    VerifyMethodInt(pNdbOperationW, incValue(c_szWarehouseCount, Uint32(1)));
    Uint32 nWarehouseSum = 0;
    for(Uint32 nDistrict=0; nDistrict<g_nDistrictPerWarehouse; ++nDistrict) {
      NdbOperation* pNdbOperationD = NULL ;
      VerifyMethodPtr(pNdbOperationD, pNdbConnection, getNdbOperation(c_szDistrict));
      VerifyMethodInt(pNdbOperationD, interpretedUpdateTuple());
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictWarehouseNumber, nWarehouse));
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictNumber, nDistrict));
      VerifyMethodInt(pNdbOperationD, incValue(c_szDistrictCount, Uint32(1)));
      Uint32 nDistrictSum = myRandom48(100);
      nWarehouseSum += nDistrictSum;
      VerifyMethodInt(pNdbOperationD, setValue(c_szDistrictSum, nDistrictSum));
    }
    VerifyMethodInt(pNdbOperationW, setValue(c_szWarehouseSum, nWarehouseSum));
    int iExec = pNdbConnection->execute(Commit);
    int iError = pNdbConnection->getNdbError().code;

    if(iExec<0 && iError!=0 && iError!=266 && iError!=626) {
      ReportMethodInt(iExec, pNdbConnection, "pNdbConnection", "execute(Commit)", __FILE__, __LINE__);
    }
    if(iExec==0) {
      ++nSucc;
    } else {
      ++nFail;
    }
    VerifyMethodVoid(pNdb, closeTransaction(pNdbConnection));
  }
  ndbout << "update: " << nSucc << " succeeded, " << nFail << " failed " << endl;
  NdbMutex_Unlock(g_pNdbMutex);
  delete pNdb;
  pNdb = NULL ;
  return NULL;
}


extern "C" void* NdbThreadFuncDelete(void* pArg)
{
  myRandom48Init((long int)NdbTick_CurrentMillisecond());	
  unsigned nSucc = 0;
  unsigned nFail = 0;
  Ndb* pNdb = NULL ;
  pNdb = new Ndb("TEST_DB");
  VerifyMethodInt(pNdb, init());
  VerifyMethodInt(pNdb, waitUntilReady());

  while(NdbMutex_Trylock(g_pNdbMutex)) {
    Uint32 nWarehouse = myRandom48(g_nWarehouseCount);
    NdbConnection* pNdbConnection = NULL ;
    VerifyMethodPtr(pNdbConnection, pNdb, startTransaction());
    CHK_TR(pNdbConnection) ; // epaulsa
    NdbOperation* pNdbOperationW = NULL ;
    VerifyMethodPtr(pNdbOperationW, pNdbConnection, getNdbOperation(c_szWarehouse));
    VerifyMethodInt(pNdbOperationW, deleteTuple());
    VerifyMethodInt(pNdbOperationW, equal(c_szWarehouseNumber, nWarehouse));
    for(Uint32 nDistrict=0; nDistrict<g_nDistrictPerWarehouse; ++nDistrict) {
      NdbOperation* pNdbOperationD = NULL ;
      VerifyMethodPtr(pNdbOperationD, pNdbConnection, getNdbOperation(c_szDistrict));
      VerifyMethodInt(pNdbOperationD, deleteTuple());
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictWarehouseNumber, nWarehouse));
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictNumber, nDistrict));
    }
    int iExec = pNdbConnection->execute(Commit);
    int iError = pNdbConnection->getNdbError().code;

    if(iExec<0 && iError!=0 && iError!=266 && iError!=626) {
      ReportMethodInt(iExec, pNdbConnection, "pNdbConnection", "execute(Commit)", __FILE__, __LINE__);
    }
    if(iExec==0) {
      ++nSucc;
    } else {
      ++nFail;
    }
    VerifyMethodVoid(pNdb, closeTransaction(pNdbConnection));
  }
  ndbout << "delete: " << nSucc << " succeeded, " << nFail << " failed " << endl;
  NdbMutex_Unlock(g_pNdbMutex);
  delete pNdb;
  pNdb = NULL ;
  return NULL;
}


extern "C" void* NdbThreadFuncRead(void* pArg)
{
  myRandom48Init((long int)NdbTick_CurrentMillisecond());
  unsigned nSucc = 0;
  unsigned nFail = 0;
  NdbRecAttr** ppNdbRecAttrDSum = new NdbRecAttr*[g_nDistrictPerWarehouse];
  NdbRecAttr** ppNdbRecAttrDCnt = new NdbRecAttr*[g_nDistrictPerWarehouse];
  Ndb* pNdb = NULL ;
  pNdb = new Ndb("TEST_DB");
  VerifyMethodInt(pNdb, init());
  VerifyMethodInt(pNdb, waitUntilReady());

  while(NdbMutex_Trylock(g_pNdbMutex)) {
    Uint32 nWarehouse = myRandom48(g_nWarehouseCount);
    NdbConnection* pNdbConnection = NULL ;
    VerifyMethodPtr(pNdbConnection, pNdb, startTransaction());
    CHK_TR(pNdbConnection) ; // epaulsa
    NdbOperation* pNdbOperationW = NULL ;
    VerifyMethodPtr(pNdbOperationW, pNdbConnection, getNdbOperation(c_szWarehouse));
    VerifyMethodInt(pNdbOperationW, readTuple());
    VerifyMethodInt(pNdbOperationW, equal(c_szWarehouseNumber, nWarehouse));
    NdbRecAttr* pNdbRecAttrWSum;
    VerifyMethodPtr(pNdbRecAttrWSum, pNdbOperationW, getValue(c_szWarehouseSum, 0));
    NdbRecAttr* pNdbRecAttrWCnt;
    VerifyMethodPtr(pNdbRecAttrWCnt, pNdbOperationW, getValue(c_szWarehouseCount, 0));
    for(Uint32 nDistrict=0; nDistrict<g_nDistrictPerWarehouse; ++nDistrict) {
      NdbOperation* pNdbOperationD = NULL ;
      VerifyMethodPtr(pNdbOperationD, pNdbConnection, getNdbOperation(c_szDistrict));
      VerifyMethodInt(pNdbOperationD, readTuple());
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictWarehouseNumber, nWarehouse));
      VerifyMethodInt(pNdbOperationD, equal(c_szDistrictNumber, nDistrict));
      VerifyMethodPtr(ppNdbRecAttrDSum[nDistrict], pNdbOperationD, getValue(c_szDistrictSum, 0));
      VerifyMethodPtr(ppNdbRecAttrDCnt[nDistrict], pNdbOperationD, getValue(c_szDistrictCount, 0));
    }
    int iExec = pNdbConnection->execute(Commit);
    int iError = pNdbConnection->getNdbError().code;
    
    if(iExec<0 && iError!=0 && iError!=266 && iError!=626) {
      ReportMethodInt(iExec, pNdbConnection, "pNdbConnection", "execute(Commit)", __FILE__, __LINE__);
    }
    if(iExec==0) {
      Uint32 nSum = 0;
      Uint32 nCnt = 0;
      for(Uint32 nDistrict=0; nDistrict<g_nDistrictPerWarehouse; ++nDistrict) {
        nSum += ppNdbRecAttrDSum[nDistrict]->u_32_value();
        nCnt += ppNdbRecAttrDCnt[nDistrict]->u_32_value();
      }
      if(nSum!=pNdbRecAttrWSum->u_32_value()
         || nCnt!=g_nDistrictPerWarehouse*pNdbRecAttrWCnt->u_32_value()) {
        ndbout << "INCONSISTENT!" << endl;
        ndbout << "iExec==" << iExec << endl;
        ndbout << "iError==" << iError << endl;
        ndbout << endl;
        ndbout << c_szWarehouseSum << "==" << pNdbRecAttrWSum->u_32_value() << ", ";
        ndbout << c_szWarehouseCount << "==" << pNdbRecAttrWCnt->u_32_value() << endl;
        ndbout << "nSum==" << nSum << ", nCnt=" << nCnt << endl;
        for(Uint32 nDistrict=0; nDistrict<g_nDistrictPerWarehouse; ++nDistrict) {
          ndbout << c_szDistrictSum << "[" << nDistrict << "]==" << ppNdbRecAttrDSum[nDistrict]->u_32_value() << ", ";
          ndbout << c_szDistrictCount << "[" << nDistrict << "]==" << ppNdbRecAttrDCnt[nDistrict]->u_32_value() << endl;
        }
        VerifyMethodVoid(pNdb, closeTransaction(pNdbConnection));
        delete pNdb; pNdb = NULL ;
        delete[] ppNdbRecAttrDSum; ppNdbRecAttrDSum = NULL ;
        delete[] ppNdbRecAttrDCnt; ppNdbRecAttrDCnt = NULL ;
        NDBT_ProgramExit(NDBT_FAILED);
      }
      ++nSucc;
    } else {
      ++nFail;
    }
    VerifyMethodVoid(pNdb, closeTransaction(pNdbConnection));
  }
  ndbout << "read: " << nSucc << " succeeded, " << nFail << " failed " << endl;
  NdbMutex_Unlock(g_pNdbMutex);
  delete pNdb; pNdb = NULL ;
  delete[] ppNdbRecAttrDSum; ppNdbRecAttrDSum = NULL ;
  delete[] ppNdbRecAttrDCnt; ppNdbRecAttrDCnt = NULL ;
  return NULL;
}


NDB_COMMAND(acid, "acid", "acid", "acid", 65535)
{
  ndb_init();
  long nSeconds = 60;
  int rc = NDBT_OK;

  for(int i=1; i<argc; ++i) {
    if(argv[i][0]=='-' || argv[i][0]=='/') {
      switch(argv[i][1]) {
      case 'w': g_nWarehouseCount=atol(argv[i]+2); break;
      case 'd': g_nDistrictPerWarehouse=atol(argv[i]+2); break;
      case 's': nSeconds=atol(argv[i]+2); break;
      case 't': g_nThreadCount=atol(argv[i]+2); break;
      default: ndbout << "invalid option" << endl; return 1;
      }
    } else {
      ndbout << "invalid operand" << endl;
      return 1;
    }
  }
  ndbout << argv[0];
  ndbout << " -w" << g_nWarehouseCount;
  ndbout << " -d" << g_nDistrictPerWarehouse;
  ndbout << " -s" << (int)nSeconds;
  ndbout << " -t" << g_nThreadCount;
  ndbout << endl;

  Ndb* pNdb = NULL ;
  pNdb = new Ndb("TEST_DB");
  VerifyMethodInt(pNdb, init());
  VerifyMethodInt(pNdb, waitUntilReady());

  NdbSchemaCon* pNdbSchemaCon= NdbSchemaCon::startSchemaTrans(pNdb);
  if(!pNdbSchemaCon){
    ndbout <<"startSchemaTransaction failed, exiting now" << endl ;
    delete pNdb ;
    NDBT_ProgramExit(NDBT_FAILED) ;
  }
  NdbSchemaOp* pNdbSchemaOp = NULL ;
  VerifyMethodPtr(pNdbSchemaOp, pNdbSchemaCon, getNdbSchemaOp());
  VerifyMethodInt(pNdbSchemaOp, createTable(
                                            c_szWarehouse, 
                                            (4+4+4+12)*1.02*g_nWarehouseCount/1024+1, 
                                            TupleKey, 
                                            (4+14)*g_nWarehouseCount/8/1024+1));

  VerifyMethodInt(pNdbSchemaOp, createAttribute(c_szWarehouseNumber, TupleKey, 32, 1, UnSigned, MMBased, false));
  VerifyMethodInt(pNdbSchemaOp, createAttribute(c_szWarehouseSum, NoKey, 32, 1, UnSigned, MMBased, false));
  VerifyMethodInt(pNdbSchemaOp, createAttribute(c_szWarehouseCount, NoKey, 32, 1, UnSigned, MMBased, false));
  VerifyMethodInt(pNdbSchemaCon, execute());
  NdbSchemaCon::closeSchemaTrans(pNdbSchemaCon);

  pNdbSchemaCon= NdbSchemaCon::startSchemaTrans(pNdb);
  VerifyMethodPtr(pNdbSchemaOp, pNdbSchemaCon, getNdbSchemaOp());
  VerifyMethodInt(pNdbSchemaOp, createTable(
                                            c_szDistrict, 
                                            (4+4+4+4+12)*1.02*g_nWarehouseCount*g_nDistrictPerWarehouse/1024+1, 
                                            TupleKey, 
                                            (4+4+14)*g_nWarehouseCount*g_nDistrictPerWarehouse/8/1024+1));


  VerifyMethodInt(pNdbSchemaOp, createAttribute(c_szDistrictWarehouseNumber, TupleKey, 32, 1, UnSigned, MMBased, false));
  VerifyMethodInt(pNdbSchemaOp, createAttribute(c_szDistrictNumber, TupleKey, 32, 1, UnSigned, MMBased, false));
  VerifyMethodInt(pNdbSchemaOp, createAttribute(c_szDistrictSum, NoKey, 32, 1, UnSigned, MMBased, false));
  VerifyMethodInt(pNdbSchemaOp, createAttribute(c_szDistrictCount, NoKey, 32, 1, UnSigned, MMBased, false));
  VerifyMethodInt(pNdbSchemaCon, execute());
  NdbSchemaCon::closeSchemaTrans(pNdbSchemaCon);
  g_pNdbMutex = NdbMutex_Create();
  NdbMutex_Lock(g_pNdbMutex);

  NdbThread** ppNdbThread = new NdbThread*[g_nThreadCount*4];
  for(Uint32 nThread=0; nThread<g_nThreadCount; ++nThread) {
    ppNdbThread[nThread*4+0] = NdbThread_Create(NdbThreadFuncInsert, 0, 65535, "insert",
                                                NDB_THREAD_PRIO_LOW);
    ppNdbThread[nThread*4+1] = NdbThread_Create(NdbThreadFuncUpdate, 0, 65535, "update",
                                                NDB_THREAD_PRIO_LOW);
    ppNdbThread[nThread*4+2] = NdbThread_Create(NdbThreadFuncDelete, 0, 65535, "delete",
                                                NDB_THREAD_PRIO_LOW);
    ppNdbThread[nThread*4+3] = NdbThread_Create(NdbThreadFuncRead, 0, 65535, "read",
                                                NDB_THREAD_PRIO_LOW);
  }

  NdbSleep_SecSleep(nSeconds);
  NdbMutex_Unlock(g_pNdbMutex);

  void* pStatus;
  for(Uint32 nThread=0; nThread<g_nThreadCount; ++nThread) {
    NdbThread_WaitFor(ppNdbThread[nThread*4+0], &pStatus);
    NdbThread_WaitFor(ppNdbThread[nThread*4+1], &pStatus);      
    NdbThread_WaitFor(ppNdbThread[nThread*4+2], &pStatus);      
    NdbThread_WaitFor(ppNdbThread[nThread*4+3], &pStatus);      
  }

  NdbMutex_Destroy(g_pNdbMutex);
  delete[] ppNdbThread;
  delete pNdb;
  return NDBT_ProgramExit(rc);
}

