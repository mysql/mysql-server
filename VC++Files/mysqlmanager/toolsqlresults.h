#if !defined(AFX_TOOLSQLRESULTS_H__826CB2FE_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
#define AFX_TOOLSQLRESULTS_H__826CB2FE_8B6D_11D1_AEC1_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ToolSqlResults.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CToolSqlResults dialog

class CToolSqlResults : public CDialog
{
// Construction
public:
	CToolSqlResults(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CToolSqlResults)
	enum { IDD = IDD_TOOL_SQL_RESULTS };
	CEdit	m_ctl_edit;
	CString	m_edit;
	//}}AFX_DATA

	void SetFont(CFont* pFont, BOOL bRedraw = TRUE);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CToolSqlResults)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation

protected:

	// Generated message map functions
	//{{AFX_MSG(CToolSqlResults)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCancel();
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TOOLSQLRESULTS_H__826CB2FE_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
