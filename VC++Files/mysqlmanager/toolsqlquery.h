#if !defined(AFX_TOOLSQLQUERY_H__826CB2FD_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
#define AFX_TOOLSQLQUERY_H__826CB2FD_8B6D_11D1_AEC1_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ToolSqlQuery.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CToolSqlQuery dialog

class CToolSqlQuery : public CDialog
{
// Construction
public:
	CToolSqlQuery(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CToolSqlQuery)
	enum { IDD = IDD_TOOL_SQL_QUERY };
	CEdit	m_ctl_edit;
	CString	m_edit;
	//}}AFX_DATA

   HACCEL            m_hAccel;

	void SetFont(CFont* pFont, BOOL bRedraw = TRUE);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CToolSqlQuery)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation

protected:

	// Generated message map functions
	//{{AFX_MSG(CToolSqlQuery)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCancel();
	afx_msg void OnClose();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	afx_msg void OnQueryPb();
	afx_msg void OnQueryDatabases();

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TOOLSQLQUERY_H__826CB2FD_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
