/****************************************************************************
   MySqlShutdown - shutdown MySQL on system shutdown (Win95/98)
 ----------------------------------------------------------------------------
 Revision History :
 Version  Author   Date         Description
 001.00   Irena    21-12-99
*****************************************************************************/
#include <windows.h>

//-----------------------------------------------------------------------
// Local  data
//-----------------------------------------------------------------------
static char szAppName[] = "MySqlShutdown";
static HINSTANCE hInstance;

#define MYWM_NOTIFYICON		(WM_APP+100)

//-----------------------------------------------------------------------
// Exported functions
//-----------------------------------------------------------------------
LRESULT CALLBACK MainWindowProc (HWND, UINT, WPARAM, LPARAM);

//-----------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------
static BOOL InitAppClass (HINSTANCE hInstance);

BOOL TrayMessageAdd(HWND hWnd, DWORD dwMessage)
{
    BOOL res;
    HICON hIcon =LoadIcon (hInstance, "MySql");
    char *szTip="MySql Shutdown";
	NOTIFYICONDATA tnd;

	tnd.cbSize		= sizeof(NOTIFYICONDATA);
	tnd.hWnd		= hWnd;
	tnd.uID			= 101;

	tnd.uFlags		= NIF_MESSAGE|NIF_ICON|NIF_TIP;
	tnd.uCallbackMessage	= MYWM_NOTIFYICON;
	tnd.hIcon		= hIcon;
    strcpy(tnd.szTip, szTip);
	res = Shell_NotifyIcon(dwMessage, &tnd);

	if (hIcon) DestroyIcon(hIcon);

	return res;
}

//-----------------------------------------------------------------------
//   Name:      WinMain
//   Purpose: Main application entry point
//-----------------------------------------------------------------------

int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int nCmdShow)
{   HWND  hWnd;
    MSG   Msg;

    hInstance=hInst;
    // Register application class if needed
    if (InitAppClass (hInstance) == FALSE) return (0);


    hWnd = CreateWindow (szAppName, "MySql",
                        WS_OVERLAPPEDWINDOW|WS_MINIMIZE,
                        0, 0,
                        GetSystemMetrics(SM_CXSCREEN)/4,
                        GetSystemMetrics(SM_CYSCREEN)/4,
                        0, 0, hInstance, NULL);

    if(!hWnd)
    {
        return (0);
    }
    ShowWindow (hWnd, SW_HIDE);
    UpdateWindow (hWnd);
    while (GetMessage (&Msg, 0, 0, 0))
    {  TranslateMessage (&Msg);
        DispatchMessage (&Msg);
    }
    return ((int) (Msg.wParam));
}

//-----------------------------------------------------------------------
//   Name:    InitAppClass
//   Purpose: Register the main application window class
//-----------------------------------------------------------------------
static BOOL InitAppClass (HINSTANCE hInstance)
{
    WNDCLASS cls;

    if (GetClassInfo (hInstance, szAppName, &cls) == 0)
    {
        cls.style          = CS_HREDRAW | CS_VREDRAW ;;
        cls.lpfnWndProc    = (WNDPROC) MainWindowProc;
        cls.cbClsExtra     = 0;
        cls.cbWndExtra     = sizeof(HWND);
        cls.hInstance      = hInstance;
        cls.hIcon          = LoadIcon (hInstance, "MySql");
        cls.hCursor        = LoadCursor (NULL, IDC_ARROW);
        cls.hbrBackground  = GetStockObject (WHITE_BRUSH) ;
        cls.lpszMenuName   = 0; //szAppName;
        cls.lpszClassName  = szAppName;
        return RegisterClass (&cls);
    }
    return (TRUE);
}
//-----------------------------------------------------------------------
//   Name:      MainWindowProc
//   Purpose: Window procedure for main application window.
//-----------------------------------------------------------------------
LRESULT CALLBACK MainWindowProc (HWND hWnd, UINT Msg,WPARAM wParam, LPARAM lParam)
{
 static RECT   rect ;
 HDC           hdc ;
 PAINTSTRUCT   ps ;
 static BOOL   bShutdown=FALSE;

    switch (Msg)
    {
          case WM_CREATE:
               TrayMessageAdd(hWnd, NIM_ADD);
               return TRUE;
/***************
          case WM_SYSCOMMAND:
               if(wParam==SC_CLOSE)
               { HANDLE hEventShutdown;

                bShutdown=TRUE;
                InvalidateRect(hWnd,NULL,TRUE);
                ShowWindow (hWnd, SW_NORMAL);
                UpdateWindow(hWnd);
                hEventShutdown=OpenEvent(EVENT_MODIFY_STATE, 0, "MySqlShutdown");
                if(hEventShutdown)
                {
                  SetEvent(hEventShutdown);
                  CloseHandle(hEventShutdown);
                  Sleep(1000);
                  MessageBox(hWnd,"Shutdown", "MySql", MB_OK);
                }
               TrayMessageAdd(hWnd, NIM_DELETE);
               }
               break;
**************/
          case WM_DESTROY:
               TrayMessageAdd(hWnd, NIM_DELETE);
               PostQuitMessage (0);
               return 0;
          case WM_SIZE:
               GetClientRect (hWnd, &rect) ;
               return 0 ;

          case WM_PAINT:
               hdc = BeginPaint (hWnd, &ps) ;
               if(bShutdown)
                 DrawText (hdc, "MySql shutdown in progress...",
                                -1, &rect, DT_WORDBREAK) ;
               EndPaint (hWnd, &ps) ;
               return 0 ;
          case WM_QUERYENDSESSION: //Shutdown MySql
               { HANDLE hEventShutdown;

                bShutdown=TRUE;
                InvalidateRect(hWnd,NULL,TRUE);
                ShowWindow (hWnd, SW_NORMAL);
                UpdateWindow(hWnd);
                hEventShutdown=OpenEvent(EVENT_MODIFY_STATE, 0, "MySqlShutdown");
                if(hEventShutdown)
                {
                  SetEvent(hEventShutdown);
                  CloseHandle(hEventShutdown);
                  Sleep(1000);
                  MessageBox(hWnd,"Shutdown", "MySql", MB_OK);
                }
               }
               return 1;

	      case MYWM_NOTIFYICON:
		   switch (lParam)
		   {
		     case WM_LBUTTONDOWN:
		     case WM_RBUTTONDOWN:
			      ShowWindow(hWnd, SW_SHOWNORMAL);
			      SetForegroundWindow(hWnd);	// make us come to the front
			      break;
		     default:
			      break;
		     }
		     break;

    }
    return DefWindowProc (hWnd, Msg, wParam, lParam);
}


// ----------------------- The end ------------------------------------------


