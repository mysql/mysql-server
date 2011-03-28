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


#include "stdafx.h"
#import "C:\Program Files\Common Files\System\ADO\msado15.dll" \
    no_namespace rename("EOF", "EndOfFile")


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

    HRESULT hr = CoInitialize(NULL);
	if(FAILED(hr))
	{
		printf("Error Initializing COM Library\n");
		return (int)hr;
	}

	_ConnectionPtr cn = NULL;
	_CommandPtr cmdUpdate = NULL, cmdInsert = NULL, cmdDelete = NULL, cmdSelect = NULL;
	_RecordsetPtr rs = NULL;
	_ParameterPtr paramContextID = NULL;
    _ParameterPtr paramVersion = NULL;
	_ParameterPtr paramLockFlag = NULL;
	_ParameterPtr ttparamLockFlag = NULL;
	_ParameterPtr paramLockTime = NULL;
	_ParameterPtr paramLockTimeUSec = NULL;
	_ParameterPtr paramContextData = NULL;
	_variant_t vtVersion;
	_variant_t vtLockFlag;
	_variant_t vtLockTime;
	_variant_t vtLockTimeUSec;
	_variant_t vtContextData;
	// Initialize Values
    vtVersion = CALL_CONTEXT_VERSION;
	vtLockFlag = CALL_CONTEXT_LOCK_FLAG;
	vtLockTime = CALL_CONTEXT_LOCK_TIME;
	vtLockTimeUSec = CALL_CONTEXT_LOCK_TIME_USEC;
	vtContextData = STATUS_DATA;

	LARGE_INTEGER freq;

	DWORD dwStartTime, dwEndTime;
	LARGE_INTEGER liStartTime, liEndTime;

    try
    {
        cn.CreateInstance(__uuidof(Connection));
        cn->ConnectionString = _T("DSN=TTTelcoCS;");
        cn->Open(_T(""),_T(""),_T(""),adConnectUnspecified);

		cmdUpdate.CreateInstance(__uuidof(Command));
        cmdInsert.CreateInstance(__uuidof(Command));
        cmdDelete.CreateInstance(__uuidof(Command));
        cmdSelect.CreateInstance(__uuidof(Command));

		TCHAR tszInsert[10000], tszUpdate[10000];
		memset(tszInsert, 0, sizeof(tszInsert));
		memset(tszUpdate, 0, sizeof(tszUpdate));
		strcpy(tszInsert, "INSERT INTO dbo.CallContext(ContextId,Version,LockFlag,LockTime,LockTimeUSec,ContextData) VALUES(?,?,?,?,?,'");
		strcat(tszInsert, STATUS_DATA);
		strcat(tszInsert, "')");
		
        cmdInsert->CommandText= tszInsert;
		cmdInsert->ActiveConnection = cn;
        cmdInsert->Prepared = TRUE;

		
		strcpy(tszUpdate, "UPDATE dbo.CallContext SET ContextData = '");
		strcat(tszUpdate, STATUS_DATA);
		strcat(tszUpdate, "' WHERE ContextId = ?");
        cmdUpdate->CommandText= tszUpdate;
		cmdUpdate->ActiveConnection = cn;
        cmdUpdate->Prepared = TRUE;

        cmdDelete->CommandText=_T("DELETE FROM dbo.CallContext WHERE ContextId = ?");
		cmdDelete->ActiveConnection = cn;
        cmdDelete->Prepared = TRUE;

        cmdSelect->CommandText=_T("SELECT ContextData FROM dbo.CallContext WHERE ContextId = ?");
		cmdSelect->ActiveConnection = cn;
        cmdSelect->Prepared = TRUE;


		//Create params
		paramContextID = cmdInsert->CreateParameter(_T("ContextID"),adInteger,adParamInput,sizeof(int),nStartingRecordID);
		paramVersion = cmdInsert->CreateParameter(_T("Version"),adInteger,adParamInput,sizeof(int),1);//vtVersion);
		paramLockFlag = cmdInsert->CreateParameter(_T("LockFlag"),adInteger,adParamInput,sizeof(int),1);//vtLockFlag);
		ttparamLockFlag = cmdUpdate->CreateParameter(_T("LockFlag"),adInteger,adParamInput,sizeof(int),1);//vtLockFlag);
		paramLockTime = cmdInsert->CreateParameter(_T("LockTime"),adInteger,adParamInput,sizeof(int),1);//vtLockTime);
		paramLockTimeUSec = cmdInsert->CreateParameter(_T("LockTimeUSec"),adInteger,adParamInput,sizeof(int),1);//vtLockTimeUSec);
		paramContextData = cmdInsert->CreateParameter(_T("ContextData"), adBSTR, adParamInput, SysStringByteLen(vtContextData.bstrVal), vtContextData);
		//paramContextData->put_Value(vtContextData);
		
		

		//Append params
		cmdInsert->Parameters->Append(paramContextID);
		cmdInsert->Parameters->Append(paramVersion);
		cmdInsert->Parameters->Append(paramLockFlag);
		cmdInsert->Parameters->Append(paramLockTime);
		cmdInsert->Parameters->Append(paramLockTimeUSec);
		//cmdInsert->Parameters->Append(paramContextData);


        cmdUpdate->Parameters->Append(paramContextID);
		//cmdUpdate->Parameters->Append(paramContextID);

        cmdSelect->Parameters->Append(paramContextID);

        cmdDelete->Parameters->Append(paramContextID);

        while(WaitForSingleObject(pData->hShutdownEvent,0) != WAIT_OBJECT_0)
        {
            paramContextID->Value = nStartingRecordID++;
			
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

                        cmdSelect->Execute(NULL, NULL, -1);
						if (bTimeLatency)
						{
							QueryPerformanceCounter(&liEndTime);
							printf("Read = %d msec.\n", (liEndTime.QuadPart - liStartTime.QuadPart) / (freq.QuadPart/1000));
						}
                        break;
                    case 19:    // Delete Record
						if (bTimeLatency)
						   QueryPerformanceCounter(&liStartTime);
                        cmdDelete->Execute(NULL,NULL,adExecuteNoRecords);
						if (bTimeLatency)
						{
							QueryPerformanceCounter(&liEndTime);
							printf("Delete = %d msec.\n", (liEndTime.QuadPart - liStartTime.QuadPart) / (freq.QuadPart/1000));
						}
                        break;
                    case 0: // Insert Record
						if (bTimeLatency)
						   QueryPerformanceCounter(&liStartTime);
		                cmdInsert->Execute(NULL,NULL,adExecuteNoRecords);
						if (bTimeLatency)
						{
							QueryPerformanceCounter(&liEndTime);
							printf("Insert = %d msec.\n", (liEndTime.QuadPart - liStartTime.QuadPart) / (freq.QuadPart/1000));
						}
                        break;
                    default:    // Update Record
						if (bTimeLatency)
						   QueryPerformanceCounter(&liStartTime);
                        cmdUpdate->Execute(NULL,NULL,adExecuteNoRecords);
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

		cn->Close();
    }
    catch(_com_error &e)
    {
        printf("%d: \n\t%s\n\t%s\n", 
			e.Error(), 
			e.ErrorMessage(), 
			e.Source());

    }

    return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
	long nNumThreads=4;
	long nSeed = 0;
	if(lstrcmp(argv[1],_T("/?")) == 0)
	{
		_tprintf(_T("InsertRecs [No.Of Threads] [Record Seed No.]\n"));
		return 0;
	}

	if(argc > 1) 
    	nNumThreads = _ttol(argv[1]);
	else
		nNumThreads = 4;
	if (argc > 2)
		nSeed = _ttol(argv[2]);
	_tprintf(_T("Num of Threads = %d, Seed = %d"), nNumThreads, nSeed);

	long nNumCallsProcessed = 0;

    SetConsoleCtrlHandler(ConsoleCtrlHandler,true); 
	hShutdownEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	
	DWORD dwStartTime  = GetTickCount();
	
	DWORD dwThreadID = 0;
	HANDLE hThreads[50];

	struct _ParamStruct params[50];


	for(int ij=0;ij<nNumThreads;ij++) {
		params[ij].hShutdownEvent = hShutdownEvent;
		params[ij].nStartingRecordNum = (ij*5000) + nSeed;
		params[ij].pnNumCallsProcessed = &nNumCallsProcessed;
	}


	for(int ij=0;ij<nNumThreads;ij++) {
		hThreads[ij] = CreateThread(NULL,NULL,RuntimeCallContext,&params[ij],0,&dwThreadID);
	}

	//Wait for the threads to finish
    WaitForMultipleObjects(nNumThreads,hThreads,TRUE,INFINITE);	
	DWORD dwEndTime  = GetTickCount();

	CloseHandle(hShutdownEvent);
	
	//Print time taken
	_tprintf(_T("Time Taken for %d Calls is %ld msec (= %ld calls/sec\n"),
		nNumCallsProcessed,dwEndTime-dwStartTime, (1000*nNumCallsProcessed/(dwEndTime-dwStartTime)));
	return 0;

}
