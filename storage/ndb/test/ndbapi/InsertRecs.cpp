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

// InsertRecs.cpp : Defines the entry point for the console application.
//


#include <NdbApi.hpp>
#include <windows.h>
#include <tchar.h>


// data for CALL_CONTEXT and GROUP_RESOURCE
static TCHAR STATUS_DATA[]=_T("000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F")
						   _T("101112131415161718191A1B1C1D1E1F000102030405060708090A0B0C0D0E0F")
						   _T("202122232425262728292A2B2C2D2E2F000102030405060708090A0B0C0D0E0F")
						   _T("303132333435363738393A3B3C3D3E3F000102030405060708090A0B0C0D0E0F")
						   _T("404142434445464748494A4B4C4D4E4F000102030405060708090A0B0C0D0E0F")
						   _T("505152535455565758595A5B5C5D5E5F000102030405060708090A0B0C0D0E0F")
						   _T("606162636465666768696A6B6C6D6E6F000102030405060708090A0B0C0D0E0F")
						   _T("707172737475767778797A7B7C7D7E7F000102030405060708090A0B0C0D0E0F")
						   _T("808182838485868788898A8B8C8D8E8F000102030405060708090A0B0C0D0E0F")
						   _T("909192939495969798999A9B9C9D9E9F000102030405060708090A0B0C0D0E0F")
						   _T("10010110210310410510610710810910A000102030405060708090A0B0C0D0EF")
						   _T("10B10C10D10E10F110111112113114115000102030405060708090A0B0C0D0EF")
						   _T("11611711811911A11B11C11D11E11F120000102030405060708090A0B0C0D0EF")
						   _T("12112212312412512612712812912A12B000102030405060708090A0B0C0D0EF")
						   _T("12C12D12E12F130131132134135136137000102030405060708090A0B0C0D0EF")
						   _T("13813913A13B13C13D13E13F140141142000102030405060708090A0B0C0D0EF")
						   _T("14314414514614714814914A14B14C14D000102030405060708090A0B0C0D0EF")
						   _T("14E14F150151152153154155156157158000102030405060708090A0B0C0D0EF")
						   _T("15915A15B15C15D15E15F160161162163000102030405060708090A0B0C0D0EF")
						   _T("16416516616716816916A16B16C16D16E000102030405060708090A0B0C0D0EF")
						   _T("16F170171172173174175176177178179000102030405060708090A0B0C0D0EF")
						   _T("17A17B17C17D17E17F180181182183184000102030405060708090A0B0C0D0EF")
						   _T("18518618718818918A18B18C18D18E18F000102030405060708090A0B0C0D0EF")
						   _T("19019119219319419519619719819919A000102030405060708090A0B0C0D0EF")
						   _T("19B19C19D19E19F200201202203204205000102030405060708090A0B0C0D0EF")
						   _T("20620720820920A20B20C20D20F210211000102030405060708090A0B0C0D0EF")
						   _T("21221321421521621721821921A21B21C000102030405060708090A0B0C0D0EF")
						   _T("21D21E21F220221222223224225226227000102030405060708090A0B0C0D0EF")
						   _T("22822922A22B22C22D22E22F230231232000102030405060708090A0B0C0D0EF")
						   _T("23323423523623723823923A23B23C23D000102030405060708090A0B0C0D0EF")
						   _T("23E23F240241242243244245246247248000102030405060708090A0B0C0D0EF")
						   _T("24924A24B24C24D24E24F250251252253000102030405060708090A0B0C0D0EF")
						   _T("101112131415161718191A1B1C1D1E1F000102030405060708090A0B0C0D0E0F")
						   _T("202122232425262728292A2B2C2D2E2F000102030405060708090A0B0C0D0E0F")
						   _T("303132333435363738393A3B3C3D3E3F000102030405060708090A0B0C0D0E0F")
						   _T("404142434445464748494A4B4C4D4E4F000102030405060708090A0B0C0D0E0F")
						   _T("505152535455565758595A5B5C5D5E5F000102030405060708090A0B0C0D0E0F")
						   _T("606162636465666768696A6B6C6D6E6F000102030405060708090A0B0C0D0E0F")
						   _T("707172737475767778797A7B7C7D7E7F000102030405060708090A0B0C0D0E0F")
						   _T("808182838485868788898A8B8C8D8E8F000102030405060708090A0B0C0D0E0F")
						   _T("909192939495969798999A9B9C9D9E9F000102030405060708090A0B0C0D0E0F")
						   _T("10010110210310410510610710810910A000102030405060708090A0B0C0D0EF")
						   _T("10B10C10D10E10F110111112113114115000102030405060708090A0B0C0D0EF")
						   _T("11611711811911A11B11C11D11E11F120000102030405060708090A0B0C0D0EF")
						   _T("12112212312412512612712812912A12B000102030405060708090A0B0C0D0EF")
						   _T("12C12D12E12F130131132134135136137000102030405060708090A0B0C0D0EF")
						   _T("13813913A13B13C13D13E13F140141142000102030405060708090A0B0C0D0EF")
						   _T("14314414514614714814914A14B14C14D000102030405060708090A0B0C0D0EF")
						   _T("14E14F150151152153154155156157158000102030405060708090A0B0C0D0EF")
						   _T("15915A15B15C15D15E15F160161162163000102030405060708090A0B0C0D0EF")
						   _T("16416516616716816916A16B16C16D16E000102030405060708090A0B0C0D0EF")
						   _T("16F170171172173174175176177178179000102030405060708090A0B0C0D0EF")
						   _T("17A17B17C17D17E17F180181182183184000102030405060708090A0B0C0D0EF")
						   _T("18518618718818918A18B18C18D18E18F000102030405060708090A0B0C0D0EF")
						   _T("19019119219319419519619719819919A000102030405060708090A0B0C0D0EF")
						   _T("19B19C19D19E19F200201202203204205000102030405060708090A0B0C0D0EF")
						   _T("20620720820920A20B20C20D20F210211000102030405060708090A0B0C0D0EF")
						   _T("21221321421521621721821921A21B21C000102030405060708090A0B0C0D0EF")
						   _T("21D21E21F220221222223224225226227000102030405060708090A0B0C0D0EF")
						   _T("22822922A22B22C22D22E22F230231232000102030405060708090A0B0C0D0EF")
						   _T("23323423523623723823923A23B23C23D000102030405060708090A0B0C0D0EF")
						   _T("2366890FE1438751097E7F6325DC0E6326F")
						   _T("25425525625725825925A25B25C25D25E25F000102030405060708090A0B0C0F");	
// Thread function for Call Context Inserts

struct _ParamStruct
{
	HANDLE hShutdownEvent;
	int nStartingRecordNum;
	long* pnNumCallsProcessed;
};

HANDLE hShutdownEvent = 0;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	if(CTRL_C_EVENT == dwCtrlType)
	{
		SetEvent(hShutdownEvent);
		return TRUE;
	}
	return FALSE;
}




DWORD WINAPI RuntimeCallContext(LPVOID lpParam)
{
    long nNumCallsProcessed = 0;

	struct _ParamStruct* pData = (struct _ParamStruct*)lpParam;
    int nStartingRecordID = pData->nStartingRecordNum;

    Ndb* pNdb;
    NdbConnection* pNdbConnection;
    NdbOperation* pNdbOperation;
    NdbRecAttr* pNdbRecAttrContextData;

    char pchContextData[4008];

	LARGE_INTEGER freq;
	LARGE_INTEGER liStartTime, liEndTime;

    pNdb = new Ndb("TEST_DB");
    if(!pNdb)
    {
        printf("new Ndb failed\n");
        return 0;
    }

    try
    {
        if(pNdb->init(1)
            || pNdb->waitUntilReady())
        {
            throw pNdb;
        }

        while(WaitForSingleObject(pData->hShutdownEvent,0) != WAIT_OBJECT_0)
        {
            nStartingRecordID++;
			
			bool bTimeLatency = (nStartingRecordID == 100) ? TRUE : FALSE;

			if (bTimeLatency)
			{
				BOOL bSuccess = QueryPerformanceFrequency(&freq);
				if (!bSuccess)
					printf("Error retrieving frequency: %d\n", GetLastError());

			}
			
            for (int i=0; i < 20; i++)
            {
                switch(i)
                {
                    case 3:
                    case 6:
                    case 9: 
                    case 11: 
                    case 12: 
                    case 15: 
                    case 18:   // Query Record
						if (bTimeLatency)
						   QueryPerformanceCounter(&liStartTime);

                        pNdbConnection = pNdb->startTransaction((Uint32)0, (const char*)&nStartingRecordID, (Uint32)4);
                        if(!pNdbConnection)
                        {
                            throw pNdb;
                        }
                        pNdbOperation = pNdbConnection->getNdbOperation(_T("CallContext"));
                        if(!pNdbOperation)
                        {
                            throw pNdbConnection;
                        }
                        if(pNdbOperation->readTuple()
                            || pNdbOperation->equal(_T("ContextId"), nStartingRecordID))
                        {
                            throw pNdbOperation;
                        }
                        pNdbRecAttrContextData = pNdbOperation->getValue(_T("ContextData"), pchContextData);
                        if(!pNdbRecAttrContextData)
                        {
                            throw pNdbOperation;
                        }
                        if(pNdbConnection->execute(Commit))
                        {
                            throw pNdbConnection;
                        }
                        pNdb->closeTransaction(pNdbConnection);

						if (bTimeLatency)
						{
							QueryPerformanceCounter(&liEndTime);
							printf("Read = %d msec.\n", (liEndTime.QuadPart - liStartTime.QuadPart) / (freq.QuadPart/1000));
						}
                        break;

                    case 19:    // Delete Record
                        if (bTimeLatency)
                            QueryPerformanceCounter(&liStartTime);
                        
                        pNdbConnection = pNdb->startTransaction((Uint32)0, (const char*)&nStartingRecordID, (Uint32)4);
                        if(!pNdbConnection)
                        {
                            throw pNdb;
                        }
                        pNdbOperation = pNdbConnection->getNdbOperation(_T("CallContext"));
                        if(!pNdbOperation)
                        {
                            throw pNdbConnection;
                        }
                        if(pNdbOperation->deleteTuple()
                            || pNdbOperation->equal(_T("ContextId"), nStartingRecordID))
                        {
                            throw pNdbOperation;
                        }
                        if(pNdbConnection->execute(Commit))
                        {
                            throw pNdbConnection;
                        }
                        pNdb->closeTransaction(pNdbConnection);
                        
						if (bTimeLatency)
						{
							QueryPerformanceCounter(&liEndTime);
							printf("Delete = %d msec.\n", (liEndTime.QuadPart - liStartTime.QuadPart) / (freq.QuadPart/1000));
						}
                        break;

                    case 0: // Insert Record
						if (bTimeLatency)
						   QueryPerformanceCounter(&liStartTime);

                        pNdbConnection = pNdb->startTransaction((Uint32)0, (const char*)&nStartingRecordID, (Uint32)4);
                        if(!pNdbConnection)
                        {
                            throw pNdb;
                        }
                        pNdbOperation = pNdbConnection->getNdbOperation(_T("CallContext"));
                        if(!pNdbOperation)
                        {
                            throw pNdbConnection;
                        }
                        if(pNdbOperation->insertTuple()
                            || pNdbOperation->equal(_T("ContextId"), nStartingRecordID)
                        || pNdbOperation->setValue(_T("Version"), Int32(1))
                        || pNdbOperation->setValue(_T("LockFlag"), Int32(1))
                        || pNdbOperation->setValue(_T("LockTime"), Int32(1))
                        || pNdbOperation->setValue(_T("LockTimeUSec"), Int32(1))
                        || pNdbOperation->setValue(_T("ContextData"), STATUS_DATA, sizeof(STATUS_DATA)))
                        {
                            throw pNdbOperation;
                        }
                        if(pNdbConnection->execute(Commit))
                        {
                            throw pNdbConnection;
                        }
                        pNdb->closeTransaction(pNdbConnection);
                        
						if (bTimeLatency)
						{
							QueryPerformanceCounter(&liEndTime);
							printf("Insert = %d msec.\n", (liEndTime.QuadPart - liStartTime.QuadPart) / (freq.QuadPart/1000));
						}
                        break;

                    default:    // Update Record
						if (bTimeLatency)
						   QueryPerformanceCounter(&liStartTime);

                        pNdbConnection = pNdb->startTransaction((Uint32)0, (const char*)&nStartingRecordID, (Uint32)4);
                        if(!pNdbConnection)
                        {
                            throw pNdb;
                        }
                        pNdbOperation = pNdbConnection->getNdbOperation(_T("CallContext"));
                        if(!pNdbOperation)
                        {
                            throw pNdbConnection;
                        }
                        if(pNdbOperation->updateTuple())
                        {
                            throw pNdbOperation;
                        }
                        if(pNdbOperation->equal(_T("ContextId"), nStartingRecordID)
                            || pNdbOperation->setValue(_T("ContextData"), STATUS_DATA, sizeof(STATUS_DATA)))
                        {
                            throw pNdbOperation;
                        }
                        if(pNdbConnection->execute(Commit))
                        {
                            throw pNdbConnection;
                        }
                        pNdb->closeTransaction(pNdbConnection);

						if (bTimeLatency)
						{
							QueryPerformanceCounter(&liEndTime);
							printf("Update = %d msec.\n", (liEndTime.QuadPart - liStartTime.QuadPart) / (freq.QuadPart/1000));
						}
                        
						break;
                }
            }

			nNumCallsProcessed++;

            InterlockedIncrement(pData->pnNumCallsProcessed);
        }

        delete pNdb;
    }
    catch(Ndb* pNdb)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdb->getNdbError(), 
            pNdb->getNdbErrorString(), 
            "Ndb");
        delete pNdb;
    }
    catch(NdbConnection* pNdbConnection)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdbConnection->getNdbError(), 
            pNdbConnection->getNdbErrorString(), 
            "NdbConnection");
        pNdb->closeTransaction(pNdbConnection);
        delete pNdb;
    }
    catch(NdbOperation* pNdbOperation)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdbOperation->getNdbError(), 
            pNdbOperation->getNdbErrorString(), 
            "NdbOperation");
        pNdb->closeTransaction(pNdbConnection);
        delete pNdb;
    }

    return 0;
}


void Initialize(Ndb* pNdb, long nInsert, bool bStoredTable)
{
    NdbSchemaCon* pNdbSchemaCon;
    NdbSchemaOp* pNdbSchemaOp;
    NdbConnection* pNdbConnection;
    NdbOperation* pNdbOperation;

    try
    {
        _tprintf(_T("Create CallContext table\n"));

        pNdbSchemaCon = pNdb->startSchemaTransaction();
        if(!pNdbSchemaCon)
        {
            throw pNdb;
        }
        pNdbSchemaOp = pNdbSchemaCon->getNdbSchemaOp();
        if(!pNdbSchemaOp)
        {
            throw pNdbSchemaCon;
        }
        if(pNdbSchemaOp->createTable(_T("CallContext"), 8, TupleKey, 2, All, 6, 78, 80, 1, bStoredTable)
            || pNdbSchemaOp->createAttribute(_T("ContextId"), TupleKey, 32, 1, Signed)
            || pNdbSchemaOp->createAttribute(_T("Version"), NoKey, 32, 1, Signed)
            || pNdbSchemaOp->createAttribute(_T("LockFlag"), NoKey, 32, 1, Signed)
            || pNdbSchemaOp->createAttribute(_T("LockTime"), NoKey, 32, 1, Signed)
            || pNdbSchemaOp->createAttribute(_T("LockTimeUSec"), NoKey, 32, 1, Signed)
            || pNdbSchemaOp->createAttribute(_T("ContextData"), NoKey, 8, 4004, String))
        {
            throw pNdbSchemaOp;
        }
        if(pNdbSchemaCon->execute())
        {
            throw pNdbSchemaCon;
        }
        pNdb->closeSchemaTransaction(pNdbSchemaCon);
        
        _tprintf(_T("Insert %d tuples in the CallContext table\n"), nInsert);
        for(long i=0; i<nInsert; ++i)
        {
            long iContextId = -i;
            pNdbConnection = pNdb->startTransaction((Uint32)0, (const char*)&iContextId, (Uint32)4);
            if(!pNdbConnection)
            {
                throw pNdb;
            }
            pNdbOperation = pNdbConnection->getNdbOperation(_T("CallContext"));
            if(!pNdbOperation)
            {
                throw pNdbConnection;
            }
            if(pNdbOperation->insertTuple()
                || pNdbOperation->equal(_T("ContextId"), iContextId)
                || pNdbOperation->setValue(_T("Version"), Int32(1))
                || pNdbOperation->setValue(_T("LockFlag"), Int32(1))
                || pNdbOperation->setValue(_T("LockTime"), Int32(1))
                || pNdbOperation->setValue(_T("LockTimeUSec"), Int32(1))
                || pNdbOperation->setValue(_T("ContextData"), STATUS_DATA, sizeof(STATUS_DATA)))
            {
                throw pNdbOperation;
            }
            if(pNdbConnection->execute(Commit))
            {
                throw pNdbConnection;
            }
            pNdb->closeTransaction(pNdbConnection);
        }
        _tprintf(_T("initialisation done\n"));
    }
    catch(Ndb* pNdb)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdb->getNdbError(), 
            pNdb->getNdbErrorString(), 
            "Ndb");
        delete pNdb;
    }
    catch(NdbConnection* pNdbConnection)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdbConnection->getNdbError(), 
            pNdbConnection->getNdbErrorString(), 
            "NdbConnection");
        pNdb->closeTransaction(pNdbConnection);
        delete pNdb;
    }
    catch(NdbOperation* pNdbOperation)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdbOperation->getNdbError(), 
            pNdbOperation->getNdbErrorString(), 
            "NdbOperation");
        pNdb->closeTransaction(pNdbConnection);
        delete pNdb;
    }
    catch(NdbSchemaCon* pNdbSchemaCon)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdbSchemaCon->getNdbError(), 
            pNdbSchemaCon->getNdbErrorString(), 
            "pNdbSchemaCon");
        pNdb->closeSchemaTransaction(pNdbSchemaCon);
        delete pNdb;
    }
    catch(NdbSchemaOp* pNdbSchemaOp)
    {
        printf("%d: \n\t%s\n\t%s\n",
            pNdbSchemaOp->getNdbError(), 
            pNdbSchemaOp->getNdbErrorString(), 
            "pNdbSchemaOp");
        pNdb->closeTransaction(pNdbConnection);
        delete pNdb;
    }
}


int _tmain(int argc, _TCHAR* argv[])
{
	long nNumThreads=4;
	long nSeed = 0;
    long nInsert = 0;
    bool bStoredTable = true;
	if(lstrcmp(argv[1],_T("/?")) == 0)
	{
		_tprintf(_T("InsertRecs [No.Of Threads] [Record Seed No.] [Init no. of rec.] [Stored?]\n"));
		return 0;
	}

	if(argc > 1) 
    	nNumThreads = _ttol(argv[1]);
	else
		nNumThreads = 4;
	if (argc > 2)
		nSeed = _ttol(argv[2]);
	_tprintf(_T("Num of Threads = %d, Seed = %d"), nNumThreads, nSeed);

    if(argc>3)
        nInsert = _ttol(argv[3]);
    if(argc>4)
        bStoredTable = (_ttol(argv[4])!=0);

	long nNumCallsProcessed = 0;

    SetConsoleCtrlHandler(ConsoleCtrlHandler,true); 
	hShutdownEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
   
    // initiate windows sockets
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    wVersionRequested = MAKEWORD( 2, 2 );
    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {
        _tprintf(_T("could not find a usable WinSock DLL\n"));
        return 0;
    }
    if ( LOBYTE( wsaData.wVersion ) != 2 
        || HIBYTE( wsaData.wVersion ) != 2 ) 
    {
        _tprintf(_T("could not find a usable WinSock DLL\n"));
        WSACleanup();
        return 0;
    }

    Ndb* pNdb = new Ndb("TEST_DB");
    if(!pNdb)
    {
        _tprintf(_T("could not construct ndb\n"));
        return 0;
    }
    if(pNdb->init(1)
        || pNdb->waitUntilReady())
    {
        _tprintf(_T("could not initialize ndb\n"));
        return 0;
    }
    
    if(nInsert>0)
    {
        Initialize(pNdb, nInsert, bStoredTable);
    }
    
    if(nNumThreads>0)
    {
        _tprintf(_T("creating %d threads\n"), nNumThreads);
        DWORD dwStartTime  = GetTickCount();
        
        DWORD dwThreadID = 0;
        HANDLE hThreads[50];
        
        struct _ParamStruct params[50];
        
        for(int ij=0;ij<nNumThreads;ij++) {
            params[ij].hShutdownEvent = hShutdownEvent;
            params[ij].nStartingRecordNum = (ij*5000) + nSeed;
            params[ij].pnNumCallsProcessed = &nNumCallsProcessed;
        }
        
        for(ij=0;ij<nNumThreads;ij++) {
            hThreads[ij] = CreateThread(NULL,NULL,RuntimeCallContext,&params[ij],0,&dwThreadID);
        }
        
        //Wait for the threads to finish
        WaitForMultipleObjects(nNumThreads,hThreads,TRUE,INFINITE);	
        DWORD dwEndTime  = GetTickCount();
        
        //Print time taken
        _tprintf(_T("Time Taken for %d Calls is %ld msec (= %ld calls/sec\n"),
            nNumCallsProcessed,dwEndTime-dwStartTime, (1000*nNumCallsProcessed/(dwEndTime-dwStartTime)));
    }

    delete pNdb;
    WSACleanup();
	CloseHandle(hShutdownEvent);
	
    return 0;
}


