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
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbMutex.h>

#include "TraceNdbApi.hpp"
#include "VerifyNdbApi.hpp"


#define Ndb CTraceNdb
#define NdbSchemaCon CTraceNdbSchemaCon
#define NdbSchemaOp CTraceNdbSchemaOp
#define NdbConnection CTraceNdbConnection
#define NdbOperation CTraceNdbOperation
#define NdbIndexOperation CTraceNdbIndexOperation
#define NdbRecAttr CTraceNdbRecAttr
#define Table CTraceTable
#define Index CTraceIndex
#define Column CTraceColumn
#define NdbDictionary CTraceNdbDictionary

/*
#define Ndb CVerifyNdb
#define NdbSchemaCon CVerifyNdbSchemaCon
#define NdbSchemaOp CVerifyNdbSchemaOp
#define NdbConnection CVerifyNdbConnection
#define NdbOperation CVerifyNdbOperation
#define NdbIndexOperation CVerifyNdbIndexOperation
#define NdbRecAttr CVerifyNdbRecAttr
#define Table CVerifyTable
#define Index CVerifyIndex
#define Column CVerifyColumn
#define NdbDictionary CVerifyNdbDictionary
*/

NdbMutex* g_pNdbMutexStop = 0;
Uint32 g_nPart = 1;
Uint32 g_nTable = 1;
Uint32 g_nTuple = 1;
Uint32 g_nAttribute = 1;
char* g_szTable = 0;
char* g_szIndex = 0;
char* g_szAttribute = 0;
bool g_bVerify = false;
bool g_bUseIndex = false;



#define N 624
#define M 397
#define MATRIX_A 0x9908b0df
#define UPPER_MASK 0x80000000
#define LOWER_MASK 0x7fffffff

#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)


class MT19937
{
public:
    MT19937(void);
    void sgenrand(unsigned long seed);
    unsigned long genrand(void);

private:
    unsigned long mt[N];
    int mti;
    unsigned long mag01[2];
};


MT19937::MT19937(void)
{
    mti = N+1;
    mag01[0] = 0x0;
    mag01[1] = MATRIX_A;
    sgenrand(4357);
}


void MT19937::sgenrand(unsigned long seed)
{
    mt[0]= seed & 0xffffffff;
    for (mti=1; mti<N; mti++)
        mt[mti] = (69069 * mt[mti-1]) & 0xffffffff;
}


unsigned long MT19937::genrand(void)
{
    unsigned long y;
    if (mti >= N) {
        int kk;
        if (mti == N+1)
        {
            sgenrand(4357);
        }
        for (kk=0;kk<N-M;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        for (;kk<N-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1];
        mti = 0;
    }
    y = mt[mti++];
    y ^= TEMPERING_SHIFT_U(y);
    y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
    y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
    y ^= TEMPERING_SHIFT_L(y);
    return y; 
}





void CreateTables(Ndb* pNdb)
{
    for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
    {
        NdbDictionary::Dictionary* pDictionary = pNdb->getDictionary();

        NdbDictionary::Table table;
        table.setName(g_szTable+iTable*4);

        NdbDictionary::Index index;
        index.setName(g_szIndex+iTable*4);
        index.setTable(table.getName());
        index.setType(NdbDictionary::Index::UniqueHashIndex);

        NdbDictionary::Column columnPK;
        columnPK.setName("PK");
        columnPK.setTupleKey(true);
        table.addColumn(columnPK);
        index.addIndexColumn(columnPK.getName());

        for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
        {
            NdbDictionary::Column columnAttr;
            columnAttr.setName(g_szAttribute+iAttr*4);
            columnAttr.setTupleKey(false);
            table.addColumn(columnAttr);
        }

        pDictionary->createTable(table);
        pDictionary->createIndex(index);

        /*
        NdbSchemaCon* pNdbSchemaCon = pNdb->startSchemaTransaction();
        NdbSchemaOp* pNdbSchemaOp = pNdbSchemaCon->getNdbSchemaOp();
        pNdbSchemaOp->createTable(g_szTable+iTable*4);
        pNdbSchemaOp->createAttribute("PK", TupleKey);
        for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
        {
        pNdbSchemaOp->createAttribute(g_szAttribute+iAttr*4, NoKey);
        }

        pNdbSchemaCon->execute();
        pNdb->closeSchemaTransaction(pNdbSchemaCon);
        */
    }
}


int InsertTransaction(Ndb* pNdb, const Uint32 iPart, const bool bIndex)
{
    int iExec = -1;
    int iCode = -1;
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    if(pNdbConnection)
    {
        for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
        {
            for(Uint32 iTuple=0; iTuple<g_nTuple; ++iTuple)
            {
                if(bIndex)
                {
                    NdbIndexOperation* pNdbIndexOperation = pNdbConnection->getNdbIndexOperation(g_szIndex+iTable*4, g_szTable+iTable*4);
                    pNdbIndexOperation->insertTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbIndexOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        Uint32 nValue = ((iPart*g_nTable+iTable)*g_nTuple+iTuple)*g_nAttribute+iAttr;
                        pNdbIndexOperation->setValue(g_szAttribute+iAttr*4, nValue);
                    }
                }
                else
                {
                    NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTable+iTable*4);
                    pNdbOperation->insertTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        Uint32 nValue = ((iPart*g_nTable+iTable)*g_nTuple+iTuple)*g_nAttribute+iAttr;
                        pNdbOperation->setValue(g_szAttribute+iAttr*4, nValue);
                    }
                }
            }
        }
        iExec = pNdbConnection->execute_ok(Commit);
        if (iExec == -1) 
        {
            ndbout << pNdbConnection->getNdbError() << endl;
        }
        pNdb->closeTransaction(pNdbConnection);
    }
    return 0;
}


int UpdateGetAndSetTransaction(Ndb* pNdb, const Uint32 iPart, const bool bIndex)
{
    int iExec = -1;
    int iCode = -1;
    NdbRecAttr** ppNdbRecAttr = new NdbRecAttr*[g_nTable*g_nTuple*g_nAttribute];
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    if(pNdbConnection)
    {
        for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
        {
            for(Uint32 iTuple=0; iTuple<g_nTuple; ++iTuple)
            {
                if(bIndex)
                {
                    NdbIndexOperation* pNdbIndexOperation = pNdbConnection->getNdbIndexOperation(g_szIndex+iTable*4, g_szTable+iTable*4);
                    pNdbIndexOperation->readTupleExclusive();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbIndexOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        ppNdbRecAttr[(iTable*g_nTuple+iTuple)*g_nAttribute+iAttr] 
                        = pNdbIndexOperation->getValue(g_szAttribute+iAttr*4);
                    }
                }
                else
                {
                    NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTable+iTable*4);
                    pNdbOperation->readTupleExclusive();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        ppNdbRecAttr[(iTable*g_nTuple+iTuple)*g_nAttribute+iAttr] 
                        = pNdbOperation->getValue(g_szAttribute+iAttr*4);
                    }
                }
            }
        }
        iExec = pNdbConnection->execute_ok(NoCommit);
        if( iExec == -1)
        {
            ndbout << pNdbConnection->getNdbError() << endl;
        }
    }
    iCode = pNdbConnection->getNdbError().code;
    if(iExec==0)
    {
        for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
        {
            for(Uint32 iTuple=0; iTuple<g_nTuple; ++iTuple)
            {
                if(bIndex)
                {
                    NdbIndexOperation* pNdbIndexOperation = pNdbConnection->getNdbIndexOperation(g_szIndex+iTable*4, g_szTable+iTable*4);
                    pNdbIndexOperation->updateTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbIndexOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        NdbRecAttr* pNdbRecAttr
                            = ppNdbRecAttr[(iTable*g_nTuple+iTuple)*g_nAttribute+iAttr];
                        Uint32 nValue = pNdbRecAttr->u_32_value() + 1;
                        pNdbIndexOperation->setValue(g_szAttribute+iAttr*4, nValue);
                    }
                }
                else
                {
                    NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTable+iTable*4);
                    pNdbOperation->updateTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        NdbRecAttr* pNdbRecAttr
                            = ppNdbRecAttr[(iTable*g_nTuple+iTuple)*g_nAttribute+iAttr];
                        Uint32 nValue = pNdbRecAttr->u_32_value() + 1;
                        pNdbOperation->setValue(g_szAttribute+iAttr*4, nValue);
                    }
                }
            }
        }
        iExec = pNdbConnection->execute(Commit);
        if (iExec == -1) 
        {
            ndbout << pNdbConnection->getNdbError() << endl;
        }
        pNdb->closeTransaction(pNdbConnection);
    }
    delete[] ppNdbRecAttr;
    return 0;
}


int UpdateInterpretedTransaction(Ndb* pNdb, const Uint32 iPart, const bool bIndex)
{
    int iExec = -1;
    int iCode = -1;
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    if(pNdbConnection)
    {
        for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
        {
            for(Uint32 iTuple=0; iTuple<g_nTuple; ++iTuple)
            {
                if(bIndex)
                {
                    NdbIndexOperation* pNdbIndexOperation = pNdbConnection->getNdbIndexOperation(g_szIndex+iTable*4, g_szTable+iTable*4);
                    pNdbIndexOperation->interpretedUpdateTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbIndexOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        pNdbIndexOperation->incValue(g_szAttribute+iAttr*4, (Uint32)1);
                    }
                }
                else
                {
                    NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTable+iTable*4);
                    pNdbOperation->interpretedUpdateTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        pNdbOperation->incValue(g_szAttribute+iAttr*4, (Uint32)1);
                    }
                }
            }
        }
        iExec = pNdbConnection->execute_ok(Commit);

        if (iExec == -1) 
        {
            ndbout << pNdbConnection->getNdbError() << endl;
        }
        pNdb->closeTransaction(pNdbConnection);
    }
    return 0;
}


void ReportInconsistency (const Uint32 iPart,
                          const Uint32 iTable,
                          const Uint32 iTuple,
                          const Uint32 iAttr,
                          const Uint32 nValue,
                          const Uint32 nExpected )
{
    ndbout << "INCONSISTENCY: ";
    ndbout << "Part " << iPart;
    ndbout << ", Table " << iTable;
    ndbout << ", Tuple " << iTuple;
    ndbout << ", Attr " << iAttr;
    ndbout << ", Value " << nValue;
    ndbout << ", Expected " << nExpected;
    ndbout << endl;
}


int ReadTransaction(Ndb* pNdb, const Uint32 iPart, const bool bIndex)
{
    int iExec = -1;
    int iCode = -1;
    NdbRecAttr** ppNdbRecAttr = new NdbRecAttr*[g_nTable*g_nTuple*g_nAttribute];
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    if(pNdbConnection)
    {
        for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
        {
            for(Uint32 iTuple=0; iTuple<g_nTuple; ++iTuple)
            {
                if(bIndex)
                {
                    NdbIndexOperation* pNdbIndexOperation = pNdbConnection->getNdbIndexOperation(g_szIndex+iTable*4, g_szTable+iTable*4);
                    pNdbIndexOperation->readTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbIndexOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        ppNdbRecAttr[(iTable*g_nTuple+iTuple)*g_nAttribute+iAttr]
                        = pNdbIndexOperation->getValue(g_szAttribute+iAttr*4);
                    }
                }
                else
                {
                    NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTable+iTable*4);
                    pNdbOperation->readTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbOperation->equal("PK", nPK);
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        ppNdbRecAttr[(iTable*g_nTuple+iTuple)*g_nAttribute+iAttr]
                        = pNdbOperation->getValue(g_szAttribute+iAttr*4);
                    }
                }
            }
        }
        iExec = pNdbConnection->execute_ok(Commit);
        if (iExec == -1) 
        {
            ndbout << pNdbConnection->getNdbError() << endl;
        }
        if(iExec==0)
        {
            Uint32 nValue0 = ppNdbRecAttr[0]->u_32_value();
            for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
            {
                for(Uint32 iTuple=0; iTuple<g_nTuple; ++iTuple)
                {
                    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
                    {
                        Uint32 nValue = ppNdbRecAttr[(iTable*g_nTuple+iTuple)*g_nAttribute+iAttr]->u_32_value();
                        Uint32 nExpected = nValue0 + (iTable*g_nTuple+iTuple)*g_nAttribute+iAttr;
                        if(nValue!=nExpected)
                        {
                            ReportInconsistency(iPart, iTable, iTuple, iAttr, nValue, nExpected);
                        }
                    }
                }
            }
        }
        pNdb->closeTransaction(pNdbConnection);
    }
    return 0;
}


int DeleteTransaction(Ndb* pNdb, const Uint32 iPart, const bool bIndex)
{
    int iExec = -1;
    int iCode = -1;
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    if(pNdbConnection)
    {
        for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
        {
            for(Uint32 iTuple=0; iTuple<g_nTuple; ++iTuple)
            {
                if(bIndex)
                {
                    NdbIndexOperation* pNdbIndexOperation = pNdbConnection->getNdbIndexOperation(g_szIndex+iTable*4, g_szTable+iTable*4);
                    pNdbIndexOperation->deleteTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbIndexOperation->equal("PK", nPK);
                }
                else
                {
                    NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTable+iTable*4);
                    pNdbOperation->deleteTuple();
                    Uint32 nPK = iPart*g_nTuple + iTuple;
                    pNdbOperation->equal("PK", nPK);
                }
            }
        }
        iExec = pNdbConnection->execute_ok(Commit);

        if (iExec == -1) 
        {
            ndbout << pNdbConnection->getNdbError() << endl;
        }
        pNdb->closeTransaction(pNdbConnection);
    }
    return 0;
}


extern "C" void* ThreadFunc(void*)
{
    Ndb* pNdb = new Ndb("TEST_DB");
    pNdb->init();
    pNdb->waitUntilReady();

    MT19937 rndgen;
    rndgen.sgenrand((unsigned long)pNdb);

    Uint32 nInsertError = 0;
    Uint32 nInsertCommit = 0;
    Uint32 nInsertRollback = 0;
    Uint32 nUpdateGetAndSetError = 0;
    Uint32 nUpdateGetAndSetCommit = 0;
    Uint32 nUpdateGetAndSetRollback = 0;
    Uint32 nReadError = 0;
    Uint32 nReadCommit = 0;
    Uint32 nReadRollback = 0;
    Uint32 nUpdateInterpretedError = 0;
    Uint32 nUpdateInterpretedCommit = 0;
    Uint32 nUpdateInterpretedRollback = 0;
    Uint32 nDeleteError = 0;
    Uint32 nDeleteCommit = 0;
    Uint32 nDeleteRollback = 0;

    if (g_bVerify)
    {
        for (Uint32 iPart = 0; iPart < g_nPart; iPart++)
        {
            switch(ReadTransaction(pNdb, iPart, false))
            {
            case -1: ++nReadError; break;
            case 0: ++nReadCommit; break;
            case 1: ++nReadRollback; break;
            }
        }
    }
    else
        while(NdbMutex_Trylock(g_pNdbMutexStop))
        {
            Uint32 iPart = rndgen.genrand() % g_nPart;
            Uint32 iTrans = rndgen.genrand() % 5;
            bool bIndex = ((rndgen.genrand() & 1) ? true : false);
            switch(iTrans)
            {
            case 0: 
                switch(InsertTransaction(pNdb, iPart, bIndex))
                {
                case -1: ++nInsertError; break;
                case 0: ++nInsertCommit; break;
                case 1: ++nInsertRollback; break;
                }
                break;

            case 1: 
                switch(UpdateGetAndSetTransaction(pNdb, iPart, bIndex))
                {
                case -1: ++nUpdateGetAndSetError; break;
                case 0: ++nUpdateGetAndSetCommit; break;
                case 1: ++nUpdateGetAndSetRollback; break;
                }
                break;

            case 2: 
                switch(ReadTransaction(pNdb, iPart, bIndex))
                {
                case -1: ++nReadError; break;
                case 0: ++nReadCommit; break;
                case 1: ++nReadRollback; break;
                }
                break;

            case 3: 
                switch(UpdateInterpretedTransaction(pNdb, iPart, bIndex))
                {
                case -1: ++nUpdateInterpretedError; break;
                case 0: ++nUpdateInterpretedCommit; break;
                case 1: ++nUpdateInterpretedRollback; break;
                }
                break;

            case 4: 
                switch(DeleteTransaction(pNdb, iPart, bIndex))
                {
                case -1: ++nDeleteError; break;
                case 0: ++nDeleteCommit; break;
                case 1: ++nDeleteRollback; break;
                }
                break;
            }
        }

        ndbout << "I:" << nInsertError << ":" << nInsertCommit << ":" << nInsertRollback;
        ndbout << " UG:" << nUpdateGetAndSetError << ":" << nUpdateGetAndSetCommit << ":" << nUpdateGetAndSetRollback;
        ndbout << " R:" << nReadError << ":" << nReadCommit << ":" << nReadRollback;
        ndbout << " UI:" << nUpdateInterpretedError << ":" << nUpdateInterpretedCommit << ":" << nUpdateInterpretedRollback;
        ndbout << " D:" << nDeleteError << ":" << nDeleteCommit << ":" << nDeleteRollback << endl;
        ndbout << endl;

        NdbMutex_Unlock(g_pNdbMutexStop);
        delete pNdb;
        return 0;
}


int main(int argc, char* argv[])
{
    ndb_init();
    Uint32 nSeconds = 1;
    Uint32 nThread = 1;

    for(int iArg=1; iArg<argc; ++iArg)
    {
        if(argv[iArg][0]=='-')
        {
            switch(argv[iArg][1])
            {
            case 'p': g_nPart = atol(argv[iArg]+2); break;
            case 'b': g_nTable = atol(argv[iArg]+2); break;
            case 'u': g_nTuple = atol(argv[iArg]+2); break;
            case 'a': g_nAttribute = atol(argv[iArg]+2); break;
            case 'v': g_bVerify = true; break;
            case 't': nThread = atol(argv[iArg]+2); break;
            case 's': nSeconds = atol(argv[iArg]+2); break;
            case 'i': g_bUseIndex = true; break;
            }
        }
    }
    ndbout << argv[0];
    ndbout << " -p" << g_nPart;
    ndbout << " -b" << g_nTable;
    ndbout << " -u" << g_nTuple;
    ndbout << " -a" << g_nAttribute;
    if (g_bVerify)
        ndbout << " -v";
    ndbout << " -t" << nThread;
    ndbout << " -s" << nSeconds;
    ndbout << endl;

    g_szTable = new char[g_nTable*4];
    for(Uint32 iTable=0; iTable<g_nTable; ++iTable)
    {
        sprintf(g_szTable+iTable*4, "T%02d", iTable);
    }

    g_szIndex = new char[g_nTable*4];
    for(Uint32 iIndex=0; iIndex<g_nTable; ++iIndex)
    {
        sprintf(g_szIndex+iIndex*4, "I%02d", iIndex);
    }

    g_szAttribute = new char[g_nAttribute*4];
    for(Uint32 iAttr=0; iAttr<g_nAttribute; ++iAttr)
    {
        sprintf(g_szAttribute+iAttr*4, "A%02d", iAttr);
    }


    Ndb* pNdb = new Ndb("TEST_DB");
    pNdb->init();
    pNdb->waitUntilReady();

    if (!g_bVerify) CreateTables(pNdb);
    g_pNdbMutexStop = NdbMutex_Create();
    NdbMutex_Lock(g_pNdbMutexStop);

    NdbThread_SetConcurrencyLevel(nThread+1);
    NdbThread** ppNdbThread = new NdbThread*[nThread];
    for(Uint32 iThread=0; iThread<nThread; ++iThread)
    {
        ppNdbThread[iThread] = NdbThread_Create(ThreadFunc, 0, 0, "ThreadFunc", NDB_THREAD_PRIO_MEAN);
    }

    NdbSleep_SecSleep(nSeconds);
    NdbMutex_Unlock(g_pNdbMutexStop);

    for(Uint32 iThread=0; iThread<nThread; ++iThread)
    {
        void* status;
        NdbThread_WaitFor(ppNdbThread[iThread], &status);
    }

    NdbMutex_Destroy(g_pNdbMutexStop);
    g_pNdbMutexStop = 0;
    delete pNdb;
}


