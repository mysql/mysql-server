#if !defined(AFX_ToolSql_H__826CB2FC_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
#define AFX_ToolSql_H__826CB2FC_8B6D_11D1_AEC1_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ToolSqlQuery.h"
#include "ToolSqlResults.h"
#include "ToolSqlStatus.h"
#include "cresource.h"
#include <my_global.h>
#include "my_sys.h"
#include "mysql.h"


/////////////////////////////////////////////////////////////////////////////
// CToolSql dialog

class CToolSql : public CDialog
{
// Construction
public:
	CToolSql(CWnd* pParent = NULL,CResource* pServer=NULL,CResource* pResource=NULL);
	~CToolSql();

// Dialog Data
	//{{AFX_DATA(CToolSql)
	enum { IDD = IDD_TOOL_SQL };
	CButton	m_ctl_Stop;
	CButton	m_ctl_Start;
	CComboBox	m_ctl_Server;
	CTabCtrl	m_tabs;
	int		m_nIntervalTimerSeconds;
	BOOL	m_bClear;
	//}}AFX_DATA

	CBitmapButton	m_btn_QueryExec;
	CBitmapButton	m_btn_Font;
   CBitmapButton  m_btn_QueryDatabases;

#ifdef _WIN64
	__int64				m_ui_timer;
#else
	UINT				m_ui_timer;
#endif

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CToolSql)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation

   void  ActivateTab ( int tab );
	void 	DoProcessListQuery();

   CToolSqlQuery*    m_pQuery;
   CToolSqlResults*  m_pResults;
   CToolSqlStatus*   m_pStatus;

   CResource*        m_pServer;
   CResource*        m_pResource;
   MYSQL*            m_pdb;
   CFont             m_font;
   LOGFONT           m_lf;
   CRect             m_rectTab[2];
   CRect             m_rectDlg[2];

protected:

	// Generated message map functions
	//{{AFX_MSG(CToolSql)
	virtual BOOL OnInitDialog();
	afx_msg void OnQueryPb();
	afx_msg void OnQueryDatabases();
	afx_msg void OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFontPb();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnStartPb();
	afx_msg void OnStopPb();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnClear();
	afx_msg void OnChangeTimerSecs();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

   void DoOnSize(UINT nType, int cx, int cy) ;

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ToolSql_H__826CB2FC_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
