/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>

#include <NdbApi.hpp>
#include <NdbSchemaCon.hpp>
#include <NdbCondition.h>
#include <NdbMutex.h>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <NdbTick.h>
#include <NdbOut.hpp>

const char* const c_szDatabaseName = "TEST_DB";

const char* const c_szTableNameStored = "CCStored";
const char* const c_szTableNameTemp = "CCTemp";

const char* const c_szContextId = "ContextId";
const char* const c_szVersion = "Version";
const char* const c_szLockFlag = "LockFlag";
const char* const c_szLockTime = "LockTime";
const char* const c_szLockTimeUSec = "LockTimeUSec";
const char* const c_szContextData = "ContextData";

const char* g_szTableName = c_szTableNameStored;


#ifdef NDB_WIN32
HANDLE hShutdownEvent = 0;
#else
bool bShutdownEvent = false;
#endif
long g_nMaxContextIdPerThread = 5000;
long g_nNumThreads = 0;
long g_nMaxCallsPerSecond = 0;
long g_nMaxRetry = 50;
bool g_bWriteTuple = false;
bool g_bInsertInitial = false;
bool g_bVerifyInitial = false;

Ndb_cluster_connection* theConnection = 0;
NdbMutex* g_pNdbMutexPrintf = 0;
NdbMutex* g_pNdbMutexIncrement = 0;
long g_nNumCallsProcessed = 0;
Uint64 g_tStartTime = 0;
Uint64 g_tEndTime = 0;

long g_nNumberOfInitialInsert = 0;
long g_nNumberOfInitialVerify = 0;

const long c_nMaxMillisecForAllCall = 5000;
long* g_plCountMillisecForCall = 0;
const long c_nMaxMillisecForAllTrans = 5000;
long* g_plCountMillisecForTrans = 0;
bool g_bReport = false;
bool g_bReportPlus = false;


// data for CALL_CONTEXT and GROUP_RESOURCE
static char STATUS_DATA[]= 
"000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F"
"101112131415161718191A1B1C1D1E1F000102030405060708090A0B0C0D0E0F"
"202122232425262728292A2B2C2D2E2F000102030405060708090A0B0C0D0E0F"
"303132333435363738393A3B3C3D3E3F000102030405060708090A0B0C0D0E0F"
"404142434445464748494A4B4C4D4E4F000102030405060708090A0B0C0D0E0F"
"505152535455565758595A5B5C5D5E5F000102030405060708090A0B0C0D0E0F"
"606162636465666768696A6B6C6D6E6F000102030405060708090A0B0C0D0E0F"
"707172737475767778797A7B7C7D7E7F000102030405060708090A0B0C0D0E0F"
"808182838485868788898A8B8C8D8E8F000102030405060708090A0B0C0D0E0F"
"909192939495969798999A9B9C9D9E9F000102030405060708090A0B0C0D0E0F"
"10010110210310410510610710810910A000102030405060708090A0B0C0D0EF"
"10B10C10D10E10F110111112113114115000102030405060708090A0B0C0D0EF"
"11611711811911A11B11C11D11E11F120000102030405060708090A0B0C0D0EF"
"12112212312412512612712812912A12B000102030405060708090A0B0C0D0EF"
"12C12D12E12F130131132134135136137000102030405060708090A0B0C0D0EF"
"13813913A13B13C13D13E13F140141142000102030405060708090A0B0C0D0EF"
"14314414514614714814914A14B14C14D000102030405060708090A0B0C0D0EF"
"14E14F150151152153154155156157158000102030405060708090A0B0C0D0EF"
"15915A15B15C15D15E15F160161162163000102030405060708090A0B0C0D0EF"
"16416516616716816916A16B16C16D16E000102030405060708090A0B0C0D0EF"
"16F170171172173174175176177178179000102030405060708090A0B0C0D0EF"
"17A17B17C17D17E17F180181182183184000102030405060708090A0B0C0D0EF"
"18518618718818918A18B18C18D18E18F000102030405060708090A0B0C0D0EF"
"19019119219319419519619719819919A000102030405060708090A0B0C0D0EF"
"19B19C19D19E19F200201202203204205000102030405060708090A0B0C0D0EF"
"20620720820920A20B20C20D20F210211000102030405060708090A0B0C0D0EF"
"21221321421521621721821921A21B21C000102030405060708090A0B0C0D0EF"
"21D21E21F220221222223224225226227000102030405060708090A0B0C0D0EF"
"22822922A22B22C22D22E22F230231232000102030405060708090A0B0C0D0EF"
"23323423523623723823923A23B23C23D000102030405060708090A0B0C0D0EF"
"23E23F240241242243244245246247248000102030405060708090A0B0C0D0EF"
"24924A24B24C24D24E24F250251252253000102030405060708090A0B0C0D0EF"
"101112131415161718191A1B1C1D1E1F000102030405060708090A0B0C0D0E0F"
"202122232425262728292A2B2C2D2E2F000102030405060708090A0B0C0D0E0F"
"303132333435363738393A3B3C3D3E3F000102030405060708090A0B0C0D0E0F"
"404142434445464748494A4B4C4D4E4F000102030405060708090A0B0C0D0E0F"
"505152535455565758595A5B5C5D5E5F000102030405060708090A0B0C0D0E0F"
"606162636465666768696A6B6C6D6E6F000102030405060708090A0B0C0D0E0F"
"707172737475767778797A7B7C7D7E7F000102030405060708090A0B0C0D0E0F"
"808182838485868788898A8B8C8D8E8F000102030405060708090A0B0C0D0E0F"
"909192939495969798999A9B9C9D9E9F000102030405060708090A0B0C0D0E0F"
"10010110210310410510610710810910A000102030405060708090A0B0C0D0EF"
"10B10C10D10E10F110111112113114115000102030405060708090A0B0C0D0EF"
"11611711811911A11B11C11D11E11F120000102030405060708090A0B0C0D0EF"
"12112212312412512612712812912A12B000102030405060708090A0B0C0D0EF"
"12C12D12E12F130131132134135136137000102030405060708090A0B0C0D0EF"
"13813913A13B13C13D13E13F140141142000102030405060708090A0B0C0D0EF"
"14314414514614714814914A14B14C14D000102030405060708090A0B0C0D0EF"
"14E14F150151152153154155156157158000102030405060708090A0B0C0D0EF"
"15915A15B15C15D15E15F160161162163000102030405060708090A0B0C0D0EF"
"16416516616716816916A16B16C16D16E000102030405060708090A0B0C0D0EF"
"16F170171172173174175176177178179000102030405060708090A0B0C0D0EF"
"17A17B17C17D17E17F180181182183184000102030405060708090A0B0C0D0EF"
"18518618718818918A18B18C18D18E18F000102030405060708090A0B0C0D0EF"
"19019119219319419519619719819919A000102030405060708090A0B0C0D0EF"
"19B19C19D19E19F200201202203204205000102030405060708090A0B0C0D0EF"
"20620720820920A20B20C20D20F210211000102030405060708090A0B0C0D0EF"
"21221321421521621721821921A21B21C000102030405060708090A0B0C0D0EF"
"21D21E21F220221222223224225226227000102030405060708090A0B0C0D0EF"
"22822922A22B22C22D22E22F230231232000102030405060708090A0B0C0D0EF"
"23323423523623723823923A23B23C23D000102030405060708090A0B0C0D0EF"
"2366890FE1438751097E7F6325DC0E6326F"
"25425525625725825925A25B25C25D25E25F000102030405060708090A0B0C0F";     

long g_nStatusDataSize = sizeof(STATUS_DATA);


// Thread function for Call Context Inserts


#ifdef NDB_WIN32

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    if(CTRL_C_EVENT == dwCtrlType)
    {
        SetEvent(hShutdownEvent);
        return TRUE;
    }
    return FALSE;
}

#else

void CtrlCHandler(int)
{
    bShutdownEvent = true;
}

#endif



void ReportNdbError(const char* szMsg, const NdbError& err)
{
    NdbMutex_Lock(g_pNdbMutexPrintf);
    printf("%s: %d: %s\n", szMsg, err.code, (err.message ? err.message : ""));
    NdbMutex_Unlock(g_pNdbMutexPrintf);
}


void
ReportCallsPerSecond(long nNumCallsProcessed, 
                     Uint64 tStartTime, 
                     Uint64 tEndTime)
{
    Uint64 tElapsed = tEndTime - tStartTime;
    long lCallsPerSec;
    if(tElapsed>0)
        lCallsPerSec = (long)((1000*nNumCallsProcessed)/tElapsed);
    else
        lCallsPerSec = 0;

    NdbMutex_Lock(g_pNdbMutexPrintf);
    printf("Time Taken for %ld Calls is %ld msec (= %ld calls/sec)\n",
        nNumCallsProcessed, (long)tElapsed, lCallsPerSec);
    NdbMutex_Unlock(g_pNdbMutexPrintf);
}


#ifndef NDB_WIN32
void InterlockedIncrement(long* lp)             // expensive
{
    NdbMutex_Lock(g_pNdbMutexIncrement);
    (*lp)++;
    NdbMutex_Unlock(g_pNdbMutexIncrement);
}
#endif


void InterlockedIncrementAndReport(void)
{
    NdbMutex_Lock(g_pNdbMutexIncrement);
    ++g_nNumCallsProcessed;
    if((g_nNumCallsProcessed%1000)==0) 
    {
        g_tEndTime = NdbTick_CurrentMillisecond();
        if(g_tStartTime) 
            ReportCallsPerSecond(1000, g_tStartTime, g_tEndTime);

        g_tStartTime = g_tEndTime;
    }
    NdbMutex_Unlock(g_pNdbMutexIncrement);
}


void SleepOneCall(void)
{
    int iMillisecToSleep;
    if(g_nMaxCallsPerSecond>0)
        iMillisecToSleep = (1000*g_nNumThreads)/g_nMaxCallsPerSecond;
    else
        iMillisecToSleep = 50;

    if(iMillisecToSleep>0)
        NdbSleep_MilliSleep(iMillisecToSleep);

}



int QueryTransaction(Ndb* pNdb, 
                     long iContextId,                       
                     long* piVersion, 
                     long* piLockFlag, 
                     long* piLockTime, 
                     long* piLockTimeUSec, 
                     char* pchContextData, 
                     NdbError& err)
{
    int iRes = -1;
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    //0, (const char*)&iContextId, 4);
    if(pNdbConnection)
    {
        NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTableName);
        if(pNdbOperation)
        {
            NdbRecAttr* pNdbRecAttrVersion;
            NdbRecAttr* pNdbRecAttrLockFlag;
            NdbRecAttr* pNdbRecAttrLockTime;
            NdbRecAttr* pNdbRecAttrLockTimeUSec;
            NdbRecAttr* pNdbRecAttrContextData;
            if(!pNdbOperation->readTuple()
            && !pNdbOperation->equal(c_szContextId, (Int32)iContextId)
            && (pNdbRecAttrVersion=pNdbOperation->getValue(c_szVersion, (char*)piVersion))
            && (pNdbRecAttrLockFlag=pNdbOperation->getValue(c_szLockFlag, (char*)piLockFlag))
            && (pNdbRecAttrLockTime=pNdbOperation->getValue(c_szLockTime, (char*)piLockTime))
            && (pNdbRecAttrLockTimeUSec=pNdbOperation->getValue(c_szLockTimeUSec, (char*)piLockTimeUSec))
            && (pNdbRecAttrContextData=pNdbOperation->getValue(c_szContextData, pchContextData)))
            {
                if(!pNdbConnection->execute(Commit))
                    iRes = 0;
                else 
                    err = pNdbConnection->getNdbError();
            } 
            else 
                err = pNdbOperation->getNdbError();
        } 
        else 
            err = pNdbConnection->getNdbError();

        pNdb->closeTransaction(pNdbConnection);
    } 
    else 
        err = pNdb->getNdbError();
    
    return iRes;
}


int RetryQueryTransaction(Ndb* pNdb, 
                          long iContextId, 
                          long* piVersion, 
                          long* piLockFlag, 
                          long* piLockTime, 
                          long* piLockTimeUSec, 
                          char* pchContextData, 
                          NdbError& err, 
                          int& nRetry)
{
    int iRes = -1;
    nRetry = 0;
    bool bRetry = true;
    while(bRetry && nRetry<g_nMaxRetry)
    {
        if(!QueryTransaction(pNdb, iContextId, piVersion, piLockFlag, 
            piLockTime, piLockTimeUSec, pchContextData, err))
        {
            iRes = 0;
            bRetry = false;
        }
        else
        {
            switch(err.status)
            {
            case NdbError::TemporaryError:
            case NdbError::UnknownResult:
                SleepOneCall();
                ++nRetry;
                break;
            
            case NdbError::PermanentError:
            default:
                bRetry = false;
                break;
            }
        }
    }
    return iRes;
}


int DeleteTransaction(Ndb* pNdb, long iContextId, NdbError& err)
{
    int iRes = -1;
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    //0, (const char*)&iContextId, 4);
    if(pNdbConnection)
    {
        NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTableName);
        if(pNdbOperation)
        {
            if(!pNdbOperation->deleteTuple()
            && !pNdbOperation->equal(c_szContextId, (Int32)iContextId)) 
            {
                if(pNdbConnection->execute(Commit) == 0) 
                    iRes = 0;
                else 
                    err = pNdbConnection->getNdbError();
            } 
            else 
                err = pNdbOperation->getNdbError();
        } 
        else 
            err = pNdbConnection->getNdbError();

        pNdb->closeTransaction(pNdbConnection);
    } 
    else 
        err = pNdb->getNdbError();

    return iRes;
}



int RetryDeleteTransaction(Ndb* pNdb, long iContextId, NdbError& err, int& nRetry)
{
    int iRes = -1;
    nRetry = 0;
    bool bRetry = true;
    bool bUnknown = false;
    while(bRetry && nRetry<g_nMaxRetry)
    {
        if(!DeleteTransaction(pNdb, iContextId, err))
        {
            iRes = 0;
            bRetry = false;
        }
        else
        {
            switch(err.status)
            {
            case NdbError::UnknownResult:
                bUnknown = true;
                ++nRetry;
                break;

            case NdbError::TemporaryError:
                bUnknown = false;
                SleepOneCall();
                ++nRetry;
                break;
            
            case NdbError::PermanentError:
                if(err.code==626 && bUnknown)
                    iRes = 0;
                bRetry = false;
                break;

            default:
                bRetry = false;
                break;
            }
        }
    }
    return iRes;
}



int InsertTransaction(Ndb* pNdb, 
                      long iContextID, 
                      long iVersion, 
                      long iLockFlag, 
                      long iLockTime, 
                      long iLockTimeUSec, 
                      const char* pchContextData, 
                      NdbError& err)
{
    int iRes = -1;
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    //0, (const char*)&iContextID, 4);
    if(pNdbConnection)
    {
        NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTableName);
        if(pNdbOperation) 
        {
            if(!(g_bWriteTuple ? pNdbOperation->writeTuple() : pNdbOperation->insertTuple())
                && !pNdbOperation->equal(c_szContextId, (Int32)iContextID)
                && !pNdbOperation->setValue(c_szVersion, (Int32)iVersion)
                && !pNdbOperation->setValue(c_szLockFlag, (Int32)iLockFlag)
                && !pNdbOperation->setValue(c_szLockTime, (Int32)iLockTime)
                && !pNdbOperation->setValue(c_szLockTimeUSec, (Int32)iLockTimeUSec)
                && !pNdbOperation->setValue(c_szContextData, pchContextData, g_nStatusDataSize))  
            {
                if(!pNdbConnection->execute(Commit))
                    iRes = 0;
                else 
                    err = pNdbConnection->getNdbError();
            }
            else 
                err = pNdbOperation->getNdbError();
        } 
        else 
            err = pNdbConnection->getNdbError();

        pNdb->closeTransaction(pNdbConnection);
    } 
    else 
        err = pNdb->getNdbError();

    return iRes;
}



int RetryInsertTransaction(Ndb* pNdb, 
                           long iContextId, 
                           long iVersion, 
                           long iLockFlag, 
                           long iLockTime, 
                           long iLockTimeUSec, 
                           const char* pchContextData, 
                           NdbError& err, int& nRetry)
{
    int iRes = -1;
    nRetry = 0;
    bool bRetry = true;
    bool bUnknown = false;
    while(bRetry && nRetry<g_nMaxRetry)
    {
        if(!InsertTransaction(pNdb, iContextId, iVersion, iLockFlag, 
            iLockTime, iLockTimeUSec, pchContextData, err))
        {
            iRes = 0;
            bRetry = false;
        }
        else
        {
            switch(err.status)
            {
            case NdbError::UnknownResult:
                bUnknown = true;
                ++nRetry;
                break;

            case NdbError::TemporaryError:
                bUnknown = false;
                SleepOneCall();
                ++nRetry;
                break;
            
            case NdbError::PermanentError:
                if(err.code==630 && bUnknown)
                    iRes = 0;
                bRetry = false;
                break;

            default:
                bRetry = false;
                break;
            }
        }
    }
    return iRes;
}


int UpdateTransaction(Ndb* pNdb, long iContextId, NdbError& err)
{
    int iRes = -1;
    NdbConnection* pNdbConnection = pNdb->startTransaction();
    //0, (const char*)&iContextId, 4);
    if(pNdbConnection)
    {
        NdbOperation* pNdbOperation = pNdbConnection->getNdbOperation(g_szTableName);
        if(pNdbOperation)
        {
            if(!pNdbOperation->updateTuple()
            && !pNdbOperation->equal(c_szContextId, (Int32)iContextId)
            && !pNdbOperation->setValue(c_szContextData, STATUS_DATA, g_nStatusDataSize))
            {
                if(!pNdbConnection->execute(Commit))
                    iRes = 0;
                else 
                    err = pNdbConnection->getNdbError();
            }
            else 
                err = pNdbOperation->getNdbError();
        } 
        else 
            err = pNdbConnection->getNdbError();

        pNdb->closeTransaction(pNdbConnection);
    } 
    else 
        err = pNdb->getNdbError();

    return iRes;
}


int RetryUpdateTransaction(Ndb* pNdb, long iContextId, NdbError& err, int& nRetry)
{
    int iRes = -1;
    nRetry = 0;
    bool bRetry = true;
    while(bRetry && nRetry<g_nMaxRetry)
    {
        if(!UpdateTransaction(pNdb, iContextId, err))
        {
            iRes = 0;
            bRetry = false;
        }
        else
        {
            switch(err.status)
            {
            case NdbError::TemporaryError:
            case NdbError::UnknownResult:
                SleepOneCall();
                ++nRetry;
                break;
            
            case NdbError::PermanentError:
            default:
                bRetry = false;
                break;
            }
        }
    }
    return iRes;
}



int InsertInitialRecords(Ndb* pNdb, long nInsert, long nSeed)
{
    int iRes = -1;
    char szMsg[100];
    for(long i=0; i<nInsert; ++i) 
    {
        int iContextID = i+nSeed;
        int nRetry = 0;
        NdbError err;
        memset(&err, 0, sizeof(err));
        Uint64 tStartTrans = NdbTick_CurrentMillisecond();
        iRes = RetryInsertTransaction(pNdb, iContextID, nSeed, iContextID,
            (long)(tStartTrans/1000), (long)((tStartTrans%1000)*1000), 
            STATUS_DATA, err, nRetry);
        Uint64 tEndTrans = NdbTick_CurrentMillisecond();
        long lMillisecForThisTrans = (long)(tEndTrans-tStartTrans);
        if(nRetry>0)
        {
            sprintf(szMsg, "insert retried %d times, time %ld msec.", 
                nRetry, lMillisecForThisTrans);
            ReportNdbError(szMsg, err);
        }
        if(iRes)
        {
            ReportNdbError("Insert initial record failed", err);
            return iRes;
        }
        InterlockedIncrement(&g_nNumberOfInitialInsert);
    }
    return iRes;
}



int VerifyInitialRecords(Ndb* pNdb, long nVerify, long nSeed)
{
    int iRes = -1;
    char* pchContextData = new char[g_nStatusDataSize];
    char szMsg[100];
    long iPrevLockTime = -1;
    long iPrevLockTimeUSec = -1;
    for(long i=0; i<nVerify; ++i) 
    {
        int iContextID = i+nSeed;
        long iVersion = 0;
        long iLockFlag = 0;
        long iLockTime = 0;
        long iLockTimeUSec = 0;
        int nRetry = 0;
        NdbError err;
        memset(&err, 0, sizeof(err));
        Uint64 tStartTrans = NdbTick_CurrentMillisecond();
        iRes = RetryQueryTransaction(pNdb, iContextID, &iVersion, &iLockFlag, 
                    &iLockTime, &iLockTimeUSec, pchContextData, err, nRetry);
        Uint64 tEndTrans = NdbTick_CurrentMillisecond();
        long lMillisecForThisTrans = (long)(tEndTrans-tStartTrans);
        if(nRetry>0)
        {
            sprintf(szMsg, "verify retried %d times, time %ld msec.", 
                nRetry, lMillisecForThisTrans);
            ReportNdbError(szMsg, err);
        }
        if(iRes)
        {
            ReportNdbError("Read initial record failed", err);
            delete[] pchContextData;
            return iRes;
        }
        if(memcmp(pchContextData, STATUS_DATA, g_nStatusDataSize))
        {
            sprintf(szMsg, "wrong context data in tuple %d", iContextID);
            ReportNdbError(szMsg, err);
            delete[] pchContextData;
            return -1;
        }
        if(iVersion!=nSeed 
            || iLockFlag!=iContextID 
            || iLockTime<iPrevLockTime 
            || (iLockTime==iPrevLockTime && iLockTimeUSec<iPrevLockTimeUSec))
        {
            sprintf(szMsg, "wrong call data in tuple %d", iContextID);
            ReportNdbError(szMsg, err);
            delete[] pchContextData;
            return -1;
        }
        iPrevLockTime = iLockTime;
        iPrevLockTimeUSec = iLockTimeUSec;
        InterlockedIncrement(&g_nNumberOfInitialVerify);
    }
    delete[] pchContextData;
    return iRes;
}





void* RuntimeCallContext(void* lpParam)
{
    long nNumCallsProcessed = 0;
    int nStartingRecordID = *(int*)lpParam;
    
    Ndb* pNdb;
    char* pchContextData = new char[g_nStatusDataSize];
    char szMsg[100];
    
    int iRes;
    const char* szOp;
    long iVersion;
    long iLockFlag;
    long iLockTime;
    long iLockTimeUSec;
    
    pNdb = new Ndb(theConnection, "TEST_DB");
    if(!pNdb)
    {
        NdbMutex_Lock(g_pNdbMutexPrintf);
        printf("new Ndb failed\n");
        NdbMutex_Unlock(g_pNdbMutexPrintf);
        delete[] pchContextData;
        return 0;
    }
    
    if(pNdb->init(1) || pNdb->waitUntilReady())
    {
        ReportNdbError("init of Ndb failed", pNdb->getNdbError());
        delete pNdb;
        delete[] pchContextData;
        return 0;
    }

    if(g_bInsertInitial)
    {
        if(InsertInitialRecords(pNdb, g_nMaxContextIdPerThread, -nStartingRecordID-g_nMaxContextIdPerThread))
        {
            delete pNdb;
            delete[] pchContextData;
            return 0;
        }
    }

    if(g_bVerifyInitial)
    {
        NdbError err;
        memset(&err, 0, sizeof(err));
        if(VerifyInitialRecords(pNdb, g_nMaxContextIdPerThread, -nStartingRecordID-g_nMaxContextIdPerThread))
        {
            delete pNdb;
            delete[] pchContextData;
            return 0;
        }
    }
    if(g_bInsertInitial || g_bVerifyInitial)
    {
        delete[] pchContextData;
        return 0;
    }

    long nContextID = nStartingRecordID;
#ifdef NDB_WIN32
    while(WaitForSingleObject(hShutdownEvent,0) != WAIT_OBJECT_0)
#else
    while(!bShutdownEvent)
#endif
    {
        ++nContextID;
        nContextID %= g_nMaxContextIdPerThread;
        nContextID += nStartingRecordID;

        bool bTimeLatency = (nContextID==100);
        
        Uint64 tStartCall = NdbTick_CurrentMillisecond();
        for (int i=0; i < 20; i++)
        {
            int nRetry = 0;
            NdbError err;
            memset(&err, 0, sizeof(err));
            Uint64 tStartTrans = NdbTick_CurrentMillisecond();
            switch(i)
            {
            case 3:
            case 6:
            case 9: 
            case 11: 
            case 12: 
            case 15: 
            case 18:   // Query Record
                szOp = "Read";
                iRes = RetryQueryTransaction(pNdb, nContextID, &iVersion, &iLockFlag, 
                    &iLockTime, &iLockTimeUSec, pchContextData, err, nRetry);
                break;
                
            case 19:    // Delete Record
                szOp = "Delete";
                iRes = RetryDeleteTransaction(pNdb, nContextID, err, nRetry);
                break;
                
            case 0: // Insert Record
                szOp = "Insert";
                iRes = RetryInsertTransaction(pNdb, nContextID, 1, 1, 1, 1, STATUS_DATA, err, nRetry);
                break;
                
            default:    // Update Record
                szOp = "Update";
                iRes = RetryUpdateTransaction(pNdb, nContextID, err, nRetry);
                break;
            }
            Uint64 tEndTrans = NdbTick_CurrentMillisecond();
            long lMillisecForThisTrans = (long)(tEndTrans-tStartTrans);

            if(g_bReport)
            {
              require(lMillisecForThisTrans>=0 && lMillisecForThisTrans<c_nMaxMillisecForAllTrans);
              InterlockedIncrement(g_plCountMillisecForTrans+lMillisecForThisTrans);
            }

            if(nRetry>0)
            {
                sprintf(szMsg, "%s retried %d times, time %ld msec.", 
                    szOp, nRetry, lMillisecForThisTrans);
                ReportNdbError(szMsg, err);
            }
            else if(bTimeLatency)
            {
                NdbMutex_Lock(g_pNdbMutexPrintf);
                printf("%s = %ld msec.\n", szOp, lMillisecForThisTrans);
                NdbMutex_Unlock(g_pNdbMutexPrintf);
            }

            if(iRes)
            {
                sprintf(szMsg, "%s failed after %ld calls, terminating thread", 
                    szOp, nNumCallsProcessed);
                ReportNdbError(szMsg, err);
                delete pNdb;
                delete[] pchContextData;
                return 0;
            }
        }
        Uint64 tEndCall = NdbTick_CurrentMillisecond();
        long lMillisecForThisCall = (long)(tEndCall-tStartCall);

        if(g_bReport)
        {
          require(lMillisecForThisCall>=0 && lMillisecForThisCall<c_nMaxMillisecForAllCall);
          InterlockedIncrement(g_plCountMillisecForCall+lMillisecForThisCall);
        }

        if(bTimeLatency)
        {
            NdbMutex_Lock(g_pNdbMutexPrintf);
            printf("Total time for call is %ld msec.\n", (long)lMillisecForThisCall);
            NdbMutex_Unlock(g_pNdbMutexPrintf);
        }
        
        nNumCallsProcessed++;
        InterlockedIncrementAndReport();
        if(g_nMaxCallsPerSecond>0)
        {
            int iMillisecToSleep = (1000*g_nNumThreads)/g_nMaxCallsPerSecond;
            iMillisecToSleep -= lMillisecForThisCall;
            if(iMillisecToSleep>0)
            {
                NdbSleep_MilliSleep(iMillisecToSleep);
            }
        }
    }

    NdbMutex_Lock(g_pNdbMutexPrintf);
    printf("Terminating thread after %ld calls\n", nNumCallsProcessed);
    NdbMutex_Unlock(g_pNdbMutexPrintf);
    
    delete pNdb;
    delete[] pchContextData;
    return 0;
}


int CreateCallContextTable(Ndb* pNdb, const char* szTableName, bool bStored)
{
    int iRes = -1;
    NdbError err;
    memset(&err, 0, sizeof(err));

    NdbSchemaCon* pNdbSchemaCon = NdbSchemaCon::startSchemaTrans(pNdb);
    if(pNdbSchemaCon)
    {
        NdbSchemaOp* pNdbSchemaOp = pNdbSchemaCon->getNdbSchemaOp();
        if(pNdbSchemaOp)
        {
            if(!pNdbSchemaOp->createTable(szTableName, 8, TupleKey, 2, 
                All, 6, 78, 80, 1, bStored)
                && !pNdbSchemaOp->createAttribute(c_szContextId, TupleKey, 32, 1, Signed)
                && !pNdbSchemaOp->createAttribute(c_szVersion, NoKey, 32, 1, Signed)
                && !pNdbSchemaOp->createAttribute(c_szLockFlag, NoKey, 32, 1, Signed)
                && !pNdbSchemaOp->createAttribute(c_szLockTime, NoKey, 32, 1, Signed)
                && !pNdbSchemaOp->createAttribute(c_szLockTimeUSec, NoKey, 32, 1, Signed)
                && !pNdbSchemaOp->createAttribute(c_szContextData, NoKey, 8, g_nStatusDataSize, String)) 
            {
                if(!pNdbSchemaCon->execute()) 
                    iRes = 0;
                else 
                    err = pNdbSchemaCon->getNdbError();
            } 
            else 
                err = pNdbSchemaOp->getNdbError();
        } 
        else 
            err = pNdbSchemaCon->getNdbError();

        NdbSchemaCon::closeSchemaTrans(pNdbSchemaCon);
    }
    else 
        err = pNdb->getNdbError();

    if(iRes)
    {
        ReportNdbError("create call context table failed", err);
    }
    return iRes;
}



void ReportResponseTimeStatistics(const char* szStat, long* plCount, const long lSize)
{
  long lCount = 0;
  Int64 llSum = 0;
  Int64 llSum2 = 0;
  long lMin = -1;
  long lMax = -1;

  for(long l=0; l<lSize; ++l)
  {
    if(plCount[l]>0)
    {
      lCount += plCount[l];
      llSum += (Int64)l*(Int64)plCount[l];
      llSum2 += (Int64)l*(Int64)l*(Int64)plCount[l];
      if(lMin==-1 || l<lMin)
      {
        lMin = l;
      }
      if(lMax==-1 || l>lMax)
      {
        lMax = l;
      }
    }
  }

  long lAvg = long(llSum/lCount);
  double dblVar = ((double)lCount*(double)llSum2 - (double)llSum*(double)llSum)/((double)lCount*(double)(lCount-1));
  long lStd = long(sqrt(dblVar));

  long lMed = -1;
  long l95 = -1;
  long lSel = -1;
  for(long l=lMin; l<=lMax; ++l)
  {
    if(plCount[l]>0)
    {
      lSel += plCount[l];
      if(lMed==-1 && lSel>=(lCount/2))
      {
        lMed = l;
      }
      if(l95==-1 && lSel>=((lCount*95)/100))
      {
        l95 = l;
      }
      if(g_bReportPlus)
      {
        printf("%ld\t%ld\n", l, plCount[l]);
      }
    }
  }

  printf("%s: Count=%ld, Min=%ld, Max=%ld, Avg=%ld, Std=%ld, Med=%ld, 95%%=%ld\n",
    szStat, lCount, lMin, lMax, lAvg, lStd, lMed, l95);
}



void ShowHelp(const char* szCmd)
{
    printf("%s -t<threads> [-s<seed>] [-b<batch>] [-c<maxcps>] [-m<size>] [-d] [-i] [-v] [-f] [-w] [-r[+]]\n", szCmd);
    printf("%s -?\n", szCmd);
    puts("-d\t\tcreate the table");
    puts("-i\t\tinsert initial records");
    puts("-v\t\tverify initial records");
    puts("-t<threads>\tnumber of threads making calls");
    puts("-s<seed>\toffset for primary key");
    puts("-b<batch>\tbatch size per thread");
    puts("-c<maxcps>\tmax number of calls per second for this process");
    puts("-m<size>\tsize of context data");
    puts("-f\t\tno checkpointing and no logging");
    puts("-w\t\tuse writeTuple instead of insertTuple");
    puts("-r\t\treport response time statistics");
    puts("-r+\t\treport response time distribution");
    puts("-?\t\thelp");
}


int main(int argc, char* argv[])
{
    ndb_init();
    g_nNumThreads = 0;
    g_nMaxCallsPerSecond = 0;
    long nSeed = 0;
    bool bStoredTable = true;
    bool bCreateTable = false;
    g_bWriteTuple = false;
    g_bReport = false;
    g_bReportPlus = false;
    
    for(int i=1; i<argc; ++i)
    {
        if(argv[i][0]=='-' || argv[i][0]=='/')
        {
            switch(argv[i][1])
            {
            case 't': 
                g_nNumThreads = atol(argv[i]+2); 
                break;
            case 's': 
                nSeed = atol(argv[i]+2); 
                break;
            case 'b': 
                g_nMaxContextIdPerThread = atol(argv[i]+2); 
                break;
            case 'm': 
                g_nStatusDataSize = atol(argv[i]+2); 
                if(g_nStatusDataSize> (int) sizeof(STATUS_DATA))
                {
                    g_nStatusDataSize = sizeof(STATUS_DATA);
                }
                break;
            case 'i': 
                g_bInsertInitial = true;
                break;
            case 'v': 
                g_bVerifyInitial = true;
                break;
            case 'd':
                bCreateTable = true;
                break;
            case 'f': 
                bStoredTable = false;
                break;
            case 'w': 
                g_bWriteTuple = true;
                break;
            case 'r': 
                g_bReport = true;
                if(argv[i][2]=='+')
                {
                  g_bReportPlus = true;
                }
                break;
            case 'c':
                g_nMaxCallsPerSecond = atol(argv[i]+2);
                break;
            case '?':
            default:
                ShowHelp(argv[0]);
                return -1;
            }
        }
        else
        {
            ShowHelp(argv[0]);
            return -1;
        }
    }
    if(bCreateTable)
        puts("-d\tcreate the table");
    if(g_bInsertInitial)
        printf("-i\tinsert initial records\n");
    if(g_bVerifyInitial)
        printf("-v\tverify initial records\n");
    if(g_nNumThreads>0)
        printf("-t%ld\tnumber of threads making calls\n", g_nNumThreads);
    if(g_nNumThreads>0)
    {
        printf("-s%ld\toffset for primary key\n", nSeed);
        printf("-b%ld\tbatch size per thread\n", g_nMaxContextIdPerThread);
    }
    if(g_nMaxCallsPerSecond>0)
        printf("-c%ld\tmax number of calls per second for this process\n", g_nMaxCallsPerSecond);
    if(!bStoredTable)
        puts("-f\tno checkpointing and no logging to disk");
    if(g_bWriteTuple)
        puts("-w\tuse writeTuple instead of insertTuple");
    if(g_bReport)
        puts("-r\treport response time statistics");
    if(g_bReportPlus)
        puts("-r+\treport response time distribution");

    if(!bCreateTable && g_nNumThreads<=0)
    {
        ShowHelp(argv[0]);
        return -1;
    }
    printf("-m%ld\tsize of context data\n", g_nStatusDataSize);

    g_szTableName = (bStoredTable ? c_szTableNameStored : c_szTableNameTemp);
    
#ifdef NDB_WIN32
    SetConsoleCtrlHandler(ConsoleCtrlHandler, true); 
#else
    signal(SIGINT, CtrlCHandler);
#endif

    if(g_bReport)
    {
      g_plCountMillisecForCall = new long[c_nMaxMillisecForAllCall];
      memset(g_plCountMillisecForCall, 0, c_nMaxMillisecForAllCall*sizeof(long));
      g_plCountMillisecForTrans = new long[c_nMaxMillisecForAllTrans];
      memset(g_plCountMillisecForTrans, 0, c_nMaxMillisecForAllTrans*sizeof(long));
    }
    
    g_pNdbMutexIncrement = NdbMutex_Create();
    g_pNdbMutexPrintf = NdbMutex_Create();
#ifdef NDB_WIN32
    hShutdownEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
#endif
    
    theConnection= new Ndb_cluster_connection();
    if (theConnection->connect(12, 5, 1) != 0)
    {
      ndbout << "Unable to connect to managment server." << endl;
      return -1;
    }
    if (theConnection->wait_until_ready(30,0) < 0)
    {
      ndbout << "Cluster nodes not ready in 30 seconds." << endl;
      return -1;
    }

    Ndb* pNdb = new Ndb(theConnection, c_szDatabaseName);
    if(!pNdb)
    {
        printf("could not construct ndb\n");
        return 1;
    }
    
    if(pNdb->init(1) || pNdb->waitUntilReady())
    {
        ReportNdbError("could not initialize ndb\n", pNdb->getNdbError());
        delete pNdb;
        return 2;
    }

    if(bCreateTable)
    {
        printf("Create CallContext table\n");
	if (bStoredTable)
	{
	  if (CreateCallContextTable(pNdb, c_szTableNameStored, true))
	  {
            printf("Create table failed\n");
            delete pNdb;
            return 3;	    
	  }
	}
	else
	{
	  if (CreateCallContextTable(pNdb, c_szTableNameTemp, false))
	  {
            printf("Create table failed\n");
            delete pNdb;
            return 3;
	  }
	}
    }
    
    if(g_nNumThreads>0) 
    {
        printf("creating %d threads\n", (int)g_nNumThreads);
        if(g_bInsertInitial)
        {
            printf("each thread will insert %ld initial records, total %ld inserts\n", 
                g_nMaxContextIdPerThread, g_nNumThreads*g_nMaxContextIdPerThread);
        }
        if(g_bVerifyInitial)
        {
            printf("each thread will verify %ld initial records, total %ld reads\n", 
                g_nMaxContextIdPerThread, g_nNumThreads*g_nMaxContextIdPerThread);
        }

        g_nNumberOfInitialInsert = 0;
        g_nNumberOfInitialVerify = 0;

        Uint64 tStartTime = NdbTick_CurrentMillisecond();
        NdbThread* pThreads[256];
        int pnStartingRecordNum[256];
        int ij;
        for(ij=0;ij<g_nNumThreads;ij++) 
        {
            pnStartingRecordNum[ij] = (ij*g_nMaxContextIdPerThread) + nSeed;
        }
        
        for(ij=0;ij<g_nNumThreads;ij++) 
        {
            pThreads[ij] = NdbThread_Create(RuntimeCallContext, 
                (void**)(pnStartingRecordNum+ij), 
                0, "RuntimeCallContext", NDB_THREAD_PRIO_LOW);
        }
        
        //Wait for the threads to finish
        for(ij=0;ij<g_nNumThreads;ij++) 
        {
            void* status;
            NdbThread_WaitFor(pThreads[ij], &status);
        }
        Uint64 tEndTime = NdbTick_CurrentMillisecond();
        
        //Print time taken
        printf("Time Taken for %ld Calls is %ld msec (= %ld calls/sec)\n",
            g_nNumCallsProcessed, 
            (long)(tEndTime-tStartTime), 
            (long)((1000*g_nNumCallsProcessed)/(tEndTime-tStartTime)));

        if(g_bInsertInitial)
            printf("successfully inserted %ld tuples\n", g_nNumberOfInitialInsert);
        if(g_bVerifyInitial)
            printf("successfully verified %ld tuples\n", g_nNumberOfInitialVerify);
    }
    
    delete pNdb;

#ifdef NDB_WIN32
    CloseHandle(hShutdownEvent);
#endif
    NdbMutex_Destroy(g_pNdbMutexIncrement);
    NdbMutex_Destroy(g_pNdbMutexPrintf);

    if(g_bReport)
    {
      ReportResponseTimeStatistics("Calls", g_plCountMillisecForCall, c_nMaxMillisecForAllCall);
      ReportResponseTimeStatistics("Transactions", g_plCountMillisecForTrans, c_nMaxMillisecForAllTrans);

      delete[] g_plCountMillisecForCall;
      delete[] g_plCountMillisecForTrans;
    }

    return 0;
}

