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

#include <ndb_global.h>
#include "NdbMem.h"

#if 0
struct AWEINFO
{
    SIZE_T dwSizeInBytesRequested;
    ULONG_PTR nNumberOfPagesRequested;
    ULONG_PTR nNumberOfPagesActual;
    ULONG_PTR nNumberOfPagesFreed;
    ULONG_PTR* pnPhysicalMemoryPageArray;
    void* pRegionReserved;
};

const size_t cNdbMem_nMaxAWEinfo = 256;
size_t gNdbMem_nAWEinfo = 0;

struct AWEINFO* gNdbMem_pAWEinfo = 0;


void ShowLastError(const char* szContext, const char* szFunction)
{
    DWORD dwError = GetLastError();
    LPVOID lpMsgBuf;
    FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR)&lpMsgBuf,
        0,
        NULL 
        );
    printf("%s : %s failed : %lu : %s\n", szContext, szFunction, dwError, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}



void NdbMem_Create()
{
    // Address Windowing Extensions
    struct PRIVINFO
    {
        DWORD Count;
        LUID_AND_ATTRIBUTES Privilege[1];
    } Info;

    HANDLE hProcess = GetCurrentProcess();
    HANDLE hToken;
    if(!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        ShowLastError("NdbMem_Create", "OpenProcessToken");
    }
    
    Info.Count = 1;
    Info.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
    if(!LookupPrivilegeValue(0, SE_LOCK_MEMORY_NAME, &(Info.Privilege[0].Luid)))
    {
        ShowLastError("NdbMem_Create", "LookupPrivilegeValue");
    }
    
    if(!AdjustTokenPrivileges(hToken, FALSE, (PTOKEN_PRIVILEGES)&Info, 0, 0, 0))
    {
        ShowLastError("NdbMem_Create", "AdjustTokenPrivileges");
    }
    
    if(!CloseHandle(hToken))
    {
        ShowLastError("NdbMem_Create", "CloseHandle");
    }
    
    return;
}

void NdbMem_Destroy()
{
    /* Do nothing */
    return;
}

void* NdbMem_Allocate(size_t size)
{
    // Address Windowing Extensions
    struct AWEINFO* pAWEinfo;
    HANDLE hProcess;
    SYSTEM_INFO sysinfo;

    if(!gNdbMem_pAWEinfo)
    {
        gNdbMem_pAWEinfo = VirtualAlloc(0, 
            sizeof(struct AWEINFO)*cNdbMem_nMaxAWEinfo, 
            MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    }

    assert(gNdbMem_nAWEinfo < cNdbMem_nMaxAWEinfo);
    pAWEinfo = gNdbMem_pAWEinfo+gNdbMem_nAWEinfo++;

    hProcess = GetCurrentProcess();
    GetSystemInfo(&sysinfo);
    pAWEinfo->nNumberOfPagesRequested = (size+sysinfo.dwPageSize-1)/sysinfo.dwPageSize;
    pAWEinfo->pnPhysicalMemoryPageArray = VirtualAlloc(0, 
        sizeof(ULONG_PTR)*pAWEinfo->nNumberOfPagesRequested, 
        MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    pAWEinfo->nNumberOfPagesActual = pAWEinfo->nNumberOfPagesRequested;
    if(!AllocateUserPhysicalPages(hProcess, &(pAWEinfo->nNumberOfPagesActual), pAWEinfo->pnPhysicalMemoryPageArray))
    {
        ShowLastError("NdbMem_Allocate", "AllocateUserPhysicalPages");
        return 0;
    }
    if(pAWEinfo->nNumberOfPagesRequested != pAWEinfo->nNumberOfPagesActual)
    {
        ShowLastError("NdbMem_Allocate", "nNumberOfPagesRequested != nNumberOfPagesActual");
        return 0;
    }
    
    pAWEinfo->dwSizeInBytesRequested = size;
    pAWEinfo->pRegionReserved = VirtualAlloc(0, pAWEinfo->dwSizeInBytesRequested, MEM_RESERVE | MEM_PHYSICAL, PAGE_READWRITE);
    if(!pAWEinfo->pRegionReserved)
    {
        ShowLastError("NdbMem_Allocate", "VirtualAlloc");
        return 0;
    }
    
    if(!MapUserPhysicalPages(pAWEinfo->pRegionReserved, pAWEinfo->nNumberOfPagesActual, pAWEinfo->pnPhysicalMemoryPageArray))
    {
        ShowLastError("NdbMem_Allocate", "MapUserPhysicalPages");
        return 0;
    }

    /*
    printf("allocate AWE memory: %lu bytes, %lu pages, address %lx\n", 
        pAWEinfo->dwSizeInBytesRequested, 
        pAWEinfo->nNumberOfPagesActual,
        pAWEinfo->pRegionReserved);
    */
    return pAWEinfo->pRegionReserved;
}


void* NdbMem_AllocateAlign(size_t size, size_t alignment)
{
    /*
    return (void*)memalign(alignment, size);
    TEMP fix
    */
    return NdbMem_Allocate(size);
}


void NdbMem_Free(void* ptr)
{
    // VirtualFree(ptr, 0, MEM_DECOMMIT|MEM_RELEASE);
    
    // Address Windowing Extensions
    struct AWEINFO* pAWEinfo = 0;
    size_t i;
    HANDLE hProcess;

    for(i=0; i<gNdbMem_nAWEinfo; ++i)
    {
        if(ptr==gNdbMem_pAWEinfo[i].pRegionReserved)
        {
            pAWEinfo = gNdbMem_pAWEinfo+i;
        }
    }
    if(!pAWEinfo)
    {
        ShowLastError("NdbMem_Free", "ptr is not AWE memory");
    }

    hProcess = GetCurrentProcess();
    if(!MapUserPhysicalPages(ptr, pAWEinfo->nNumberOfPagesActual, 0))
    {
        ShowLastError("NdbMem_Free", "MapUserPhysicalPages");
    }
    
    if(!VirtualFree(ptr, 0, MEM_RELEASE))
    {
        ShowLastError("NdbMem_Free", "VirtualFree");
    }
    
    pAWEinfo->nNumberOfPagesFreed = pAWEinfo->nNumberOfPagesActual;
    if(!FreeUserPhysicalPages(hProcess, &(pAWEinfo->nNumberOfPagesFreed), pAWEinfo->pnPhysicalMemoryPageArray))
    {
        ShowLastError("NdbMem_Free", "FreeUserPhysicalPages");
    }
    
    VirtualFree(pAWEinfo->pnPhysicalMemoryPageArray, 0, MEM_DECOMMIT|MEM_RELEASE);
}


int NdbMem_MemLockAll()
{
    /*
    HANDLE hProcess = GetCurrentProcess();
    SIZE_T nMinimumWorkingSetSize;
    SIZE_T nMaximumWorkingSetSize;
    GetProcessWorkingSetSize(hProcess, &nMinimumWorkingSetSize, &nMaximumWorkingSetSize);
    ndbout << "nMinimumWorkingSetSize=" << nMinimumWorkingSetSize << ", nMaximumWorkingSetSize=" << nMaximumWorkingSetSize << endl;

    SetProcessWorkingSetSize(hProcess, 50000000, 100000000);
  
    GetProcessWorkingSetSize(hProcess, &nMinimumWorkingSetSize, &nMaximumWorkingSetSize);
    ndbout << "nMinimumWorkingSetSize=" << nMinimumWorkingSetSize << ", nMaximumWorkingSetSize=" << nMaximumWorkingSetSize << endl;
    */
    return -1;
}

int NdbMem_MemUnlockAll()
{
    //VirtualUnlock();
    return -1;
}

#endif

void NdbMem_Create()
{
  /* Do nothing */
  return;
}

void NdbMem_Destroy()
{
  /* Do nothing */
  return;
}


void* NdbMem_Allocate(size_t size)
{
  void* mem_allocated;
  assert(size > 0);
  mem_allocated= (void*)malloc(size);
  return mem_allocated;
}

void* NdbMem_AllocateAlign(size_t size, size_t alignment)
{
  (void)alignment; /* remove warning for unused parameter */
  /*
    return (void*)memalign(alignment, size);
    TEMP fix
  */
  return (void*)malloc(size);
}


void NdbMem_Free(void* ptr)
{
  free(ptr);
}

 
int NdbMem_MemLockAll()
{
  return 0;
}

int NdbMem_MemUnlockAll()
{
  return 0;
}

