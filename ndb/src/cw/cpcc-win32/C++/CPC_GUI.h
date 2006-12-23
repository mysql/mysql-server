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


#if !defined(AFX_CPC_GUI_H__EA01C861_C56D_48F1_856F_4935E20620B1__INCLUDED_)
#define AFX_CPC_GUI_H__EA01C861_C56D_48F1_856F_4935E20620B1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#define MAX_LOADSTRING 100



#define TV_ROOT_ITEMS 2


// Global Variables

ATOM							NdbRegisterClass(HINSTANCE) ;
BOOL							InitInstance(HINSTANCE, int) ;
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM) ;
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

#endif // !defined(AFX_CPC_GUI_H__EA01C861_C56D_48F1_856F_4935E20620B1__INCLUDED_)
