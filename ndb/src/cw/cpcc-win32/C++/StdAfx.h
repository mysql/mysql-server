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

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#ifdef _DEBUG
#define NDB_DEFAULT_UNREACHABLE default: _ASSERT(0); break
#elif _MSC_VER >= 1200
#define NDB_DEFAULT_UNREACHABLE default: __assume(0); break
#else
#define NDB_DEFAULT_UNREACHABLE default: break
#endif;


#ifdef _DEBUG
#define _assert _ASSERT
#else
#define _assert(expr) expr 
#endif


#include <afx.h>
#include <afxtempl.h>

// C RunTime Header Files
#include <ndb_global.h>
#include <memory.h>
#include <tchar.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <crtdbg.h>

// Local Header Files
#include "resource.h"
#include "NdbControls.h"
#include "CPC_GUI.h"


// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
