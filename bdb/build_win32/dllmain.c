/*
 *  --------------------------------------------------------------------------
 *	Copyright (C) 1997 Netscape Communications Corporation
 *  --------------------------------------------------------------------------
 *
 *  dllmain.c
 *
 *  $Id: dllmain.c,v 1.3 2000/10/26 21:58:48 bostic Exp $
 */

#define	WIN32_LEAN_AND_MEAN
#include <windows.h>

static int				ProcessesAttached = 0;
static HINSTANCE		Instance;		/* Global library instance handle. */

/*
 * The following declaration is for the VC++ DLL entry point.
 */

BOOL APIENTRY		DllMain (HINSTANCE hInst,
			    DWORD reason, LPVOID reserved);

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Borland to invoke the
 *	initialization code for Tcl.  It simply calls the DllMain
 *	routine.
 *
 * Results:
 *	See DllMain.
 *
 * Side effects:
 *	See DllMain.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    return DllMain(hInst, reason, reserved);
}

/*
 *----------------------------------------------------------------------
 *
 * DllMain --
 *
 *	This routine is called by the VC++ C run time library init
 *	code, or the DllEntryPoint routine.  It is responsible for
 *	initializing various dynamically loaded libraries.
 *
 * Results:
 *	TRUE on sucess, FALSE on failure.
 *
 * Side effects:
 *	Establishes 32-to-16 bit thunk and initializes sockets library.
 *
 *----------------------------------------------------------------------
 */
BOOL APIENTRY
DllMain(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:

	/*
	 * Registration of UT need to be done only once for first
	 * attaching process.  At that time set the tclWin32s flag
	 * to indicate if the DLL is executing under Win32s or not.
	 */

	if (ProcessesAttached++) {
	    return FALSE;         /* Not the first initialization. */
	}

	Instance = hInst;
	return TRUE;

    case DLL_PROCESS_DETACH:

	ProcessesAttached--;
	break;
    }

    return TRUE;
}
