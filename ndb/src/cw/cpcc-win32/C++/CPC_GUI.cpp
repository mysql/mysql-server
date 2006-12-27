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

#include "stdafx.h"

HINSTANCE			hInst ;
TCHAR					szTitle[MAX_LOADSTRING] ;
TCHAR					szWindowClass[MAX_LOADSTRING] ;

static CNdbControls controls ;

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow){
	MSG		msg;
	HACCEL	hAccelTable;

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING) ;
	LoadString(hInstance, IDC_CPC_GUI, szWindowClass, MAX_LOADSTRING) ;
	NdbRegisterClass(hInstance);

	if (!InitInstance (hInstance, nCmdShow)) {
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_CPC_GUI);

	while (GetMessage(&msg, NULL, 0, 0)){

		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)){

			TranslateMessage(&msg);
			DispatchMessage(&msg);

		}
	
	}

	return msg.wParam;
}


ATOM NdbRegisterClass(HINSTANCE hInstance){
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW ;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_CPC_GUI);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName	= (LPCSTR)IDC_CPC_GUI;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow){
   
	HWND hWnd;

   hInst = hInstance;

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	 InitCommonControls(); 

   if (!hWnd) return FALSE ;

   ShowWindow(hWnd, nCmdShow) ;
   UpdateWindow(hWnd) ;

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	int c = 0 ;
	
	switch (message) 
	{
		
		case WM_CREATE:
			_assert(controls.Create(hInst, hWnd)) ;
			return 0 ;
	
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 

			switch (wmId){
				case IDM_ABOUT:
				   DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				   break;
				case IDM_EXIT:
				   DestroyWindow(hWnd);
				   break;
				default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;

		case WM_NOTIFY:
			switch (((LPNMHDR) lParam)->code) { 
				case TTN_GETDISPINFO: {
					
					LPTOOLTIPTEXT lpttt; 
					lpttt = (LPTOOLTIPTEXT) lParam; 
					lpttt->hinst = hInst;

					int idButton = lpttt->hdr.idFrom; 
        
					switch (idButton){ 
						case IDM_NEW: 
							lpttt->lpszText = MAKEINTRESOURCE(IDS_TIP_NEW); 
							break; 
						case IDM_DELETE: 
							lpttt->lpszText = MAKEINTRESOURCE(IDS_TIP_DELETE); 
							break; 
						case IDM_PROPS: 
							lpttt->lpszText = MAKEINTRESOURCE(IDS_TIP_PROPS); 
							break; 
					} 
					break; 
				}
				case TVN_SELCHANGED: {
					LPNMTREEVIEW pnmtv ;
					
					pnmtv = (LPNMTREEVIEW) lParam ;
					controls.ToggleListViews(pnmtv) ;
					
					break ;
				}

				case NM_RCLICK: {
					LPNMHDR lpnmh ;
					lpnmh = (LPNMHDR) lParam ;
					switch(lpnmh->idFrom){
					case ID_TREEVIEW:
						break;
					default:
						break ;
					}
				}

        default: 
            break; 
    } 


		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps) ;
			EndPaint(hWnd, &ps);
			break;

		case WM_SIZE:
			controls.Resize() ;
      return 0 ;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}


LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){
	
		switch (message){

		case WM_INITDIALOG:
				return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL){
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}





