// ToolSqlResults.cpp : implementation file
//

#include "stdafx.h"
#include "MySqlManager.h"
#include "ToolSqlResults.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

CToolSqlResults::CToolSqlResults(CWnd* pParent /*=NULL*/)
	: CDialog(CToolSqlResults::IDD, pParent)
{
	//{{AFX_DATA_INIT(CToolSqlResults)
	m_edit = _T("");
	//}}AFX_DATA_INIT
}


/////////////////////////////////////////////////////////////////////////////

void CToolSqlResults::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CToolSqlResults)
	DDX_Control(pDX, IDC_EDIT, m_ctl_edit);
	DDX_Text(pDX, IDC_EDIT, m_edit);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CToolSqlResults, CDialog)
	//{{AFX_MSG_MAP(CToolSqlResults)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

void CToolSqlResults::SetFont(CFont* pFont, BOOL bRedraw)
{
	m_ctl_edit.SetFont(pFont,bRedraw);
	m_ctl_edit.Invalidate();
	m_ctl_edit.UpdateWindow();
}


/////////////////////////////////////////////////////////////////////////////

void CToolSqlResults::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
   if (IsWindow(m_ctl_edit.GetSafeHwnd()))
	   m_ctl_edit.SetWindowPos(NULL,20,24,cx-40,cy-48,SWP_NOZORDER | SWP_NOMOVE);
}

/////////////////////////////////////////////////////////////////////////////

void CToolSqlResults::OnCancel()
{
}

/////////////////////////////////////////////////////////////////////////////

void CToolSqlResults::OnClose()
{
}
