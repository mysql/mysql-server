#if !defined(AFX_TOOLSQLSTATUS_H__40C861B4_9E5A_11D1_AED0_00600806E071__INCLUDED_)
#define AFX_TOOLSQLSTATUS_H__40C861B4_9E5A_11D1_AED0_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ToolSqlStatus.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CToolSqlStatus dialog

class CToolSqlStatus : public CDialog
{
// Construction
public:
	CToolSqlStatus(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CToolSqlStatus)
	enum { IDD = IDD_TOOL_SQL_STATUS };
	CEdit	m_ctl_edit;
	CString	m_edit;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CToolSqlStatus)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CToolSqlStatus)
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TOOLSQLSTATUS_H__40C861B4_9E5A_11D1_AED0_00600806E071__INCLUDED_)
