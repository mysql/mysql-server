#if !defined(AFX_REGISTERSERVER_H__826CB2FF_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
#define AFX_REGISTERSERVER_H__826CB2FF_8B6D_11D1_AEC1_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// RegisterServer.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRegisterServer dialog

class CRegisterServer : public CDialog
{
// Construction
public:
	CRegisterServer(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CRegisterServer)
	enum { IDD = IDD_REGISTER_SERVER };
	CString	m_strServer;
	CString	m_strHost;
	CString	m_strUser;
	CString	m_strPassword;
	CString	m_strPort;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRegisterServer)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CRegisterServer)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REGISTERSERVER_H__826CB2FF_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
