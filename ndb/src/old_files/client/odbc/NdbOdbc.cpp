/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <NdbUnistd.h>
#include <odbcinst.h>

#include "driver.cpp"


BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}


BOOL INSTAPI ConfigDSN(
     HWND     hwndParent,
     WORD     fRequest,
     LPCSTR     lpszDriver,
     LPCSTR     lpszAttributes)
{
	const char* szDSN = "NDB";

	switch(fRequest)
	{
	case ODBC_ADD_DSN: 
		SQLWriteDSNToIni(szDSN, lpszDriver);
		break;

	case ODBC_CONFIG_DSN:
		break;

	case ODBC_REMOVE_DSN:
		SQLRemoveDSNFromIni(szDSN);
		break;
	}

	return TRUE;
}


int FAR PASCAL
DriverConnectProc(HWND hdlg, WORD wMsg, WPARAM wParam, LPARAM lParam)
{
  return FALSE;
}

void __declspec( dllexport) FAR PASCAL LoadByOrdinal(void);
/* Entry point to cause DM to load using ordinals */
void __declspec( dllexport) FAR PASCAL LoadByOrdinal(void)
{
}

