/* Copyright (C) 2000-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation.

   There are special exceptions to the terms and conditions of the GPL as it
   is applied to this software. View the full text of the exception in file
   EXCEPTIONS-CLIENT in the directory of this software distribution.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
** Handling initialization of the dll library
*/

#include <my_global.h>
#include <my_sys.h>
#include <my_pthread.h>

static my_bool libmysql_inited=0;

void libmysql_init(void)
{
  if (libmysql_inited)
    return;
  libmysql_inited=1;
  my_init();
  {
    DBUG_ENTER("libmysql_init");
#ifdef LOG_ALL
    DBUG_PUSH("d:t:S:O,c::\\tmp\\libmysql.log");
#else
    if (getenv("LIBMYSQL_LOG") != NULL)
      DBUG_PUSH(getenv("LIBMYSQL_LOG"));
#endif
    DBUG_VOID_RETURN;
  }
}

#ifdef __WIN__

static int inited=0,threads=0;
HINSTANCE NEAR s_hModule;	/* Saved module handle */
DWORD main_thread;

BOOL APIENTRY LibMain(HANDLE hInst,DWORD ul_reason_being_called,
		      LPVOID lpReserved)
{
  switch (ul_reason_being_called) {
  case DLL_PROCESS_ATTACH:	/* case of libentry call in win 3.x */
    if (!inited++)
    {
      s_hModule=hInst;
      libmysql_init();
      main_thread=GetCurrentThreadId();
    }
    break;
  case DLL_THREAD_ATTACH:
    threads++;
    my_thread_init();
    break;
  case DLL_PROCESS_DETACH:	/* case of wep call in win 3.x */
     if (!--inited)		/* Safety */
     {
       /* my_thread_init() */	/* This may give extra safety */
       my_end(0);
     }
    break;
  case DLL_THREAD_DETACH:
    /* Main thread will free by my_end() */
    threads--;
    if (main_thread != GetCurrentThreadId())
      my_thread_end();
    break;
  default:
    break;
  } /* switch */

  return TRUE;

  UNREFERENCED_PARAMETER(lpReserved);
} /* LibMain */


static BOOL do_libmain;
int __stdcall DllMain(HANDLE hInst,DWORD ul_reason_being_called,LPVOID lpReserved)
{
  /*
    Unless environment variable LIBMYSQL_DLLINIT is set, do nothing.
    The environment variable is checked once, during the first call to DllMain()
    (in DLL_PROCESS_ATTACH hook).
  */
  if (ul_reason_being_called == DLL_PROCESS_ATTACH)
    do_libmain = (getenv("LIBMYSQL_DLLINIT") != NULL);
  if (do_libmain)
    return LibMain(hInst,ul_reason_being_called,lpReserved);
  return TRUE;
}

#elif defined(WINDOWS)

/****************************************************************************
**	This routine is called by LIBSTART.ASM at module load time.  All it
**	does in this sample is remember the DLL module handle.	The module
**	handle is needed if you want to do things like load stuff from the
**	resource file (for instance string resources).
****************************************************************************/

int _export FAR PASCAL libmain(HANDLE hModule,short cbHeapSize,
			       UCHAR FAR *lszCmdLine)
{
  s_hModule = hModule;
  libmysql_init();
  return TRUE;
}

#endif
