// MySqlManager.h : main header file for the MYSQLMANAGER application
//

#if !defined(AFX_MYSQLMANAGER_H__826CB2EA_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
#define AFX_MYSQLMANAGER_H__826CB2EA_8B6D_11D1_AEC1_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CMySqlManagerApp:
// See MySqlManager.cpp for the implementation of this class
//

class CMySqlManagerApp : public CWinApp
{
public:
	CMySqlManagerApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMySqlManagerApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CMySqlManagerApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MYSQLMANAGER_H__826CB2EA_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
