// ToolSqlStatus.cpp : implementation file
//

#include "stdafx.h"
#include "mysqlmanager.h"
#include "ToolSqlStatus.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CToolSqlStatus dialog


CToolSqlStatus::CToolSqlStatus(CWnd* pParent /*=NULL*/)
	: CDialog(CToolSqlStatus::IDD, pParent)
{
	//{{AFX_DATA_INIT(CToolSqlStatus)
	m_edit = _T("");
	//}}AFX_DATA_INIT
}


void CToolSqlStatus::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CToolSqlStatus)
	DDX_Control(pDX, IDC_EDIT, m_ctl_edit);
	DDX_Text(pDX, IDC_EDIT, m_edit);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CToolSqlStatus, CDialog)
	//{{AFX_MSG_MAP(CToolSqlStatus)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToolSqlStatus message handlers

void CToolSqlStatus::OnDestroy()
{
	CDialog::OnDestroy();

}
