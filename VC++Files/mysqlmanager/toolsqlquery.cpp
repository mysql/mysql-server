// ToolSqlQuery.cpp : implementation file
//

#include "stdafx.h"
#include "MySqlManager.h"
#include "ToolSqlQuery.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

CToolSqlQuery::CToolSqlQuery(CWnd* pParent /*=NULL*/)
	: CDialog(CToolSqlQuery::IDD, pParent)
{
	//{{AFX_DATA_INIT(CToolSqlQuery)
	m_edit = _T("");
	//}}AFX_DATA_INIT
}


/////////////////////////////////////////////////////////////////////////////

void CToolSqlQuery::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CToolSqlQuery)
	DDX_Control(pDX, IDC_EDIT, m_ctl_edit);
	DDX_Text(pDX, IDC_EDIT, m_edit);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CToolSqlQuery, CDialog)
	//{{AFX_MSG_MAP(CToolSqlQuery)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_COMMAND(IDM_QUERY_EXEC, OnQueryPb)
	ON_COMMAND(IDM_QUERY_DATABASES, OnQueryDatabases)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

void CToolSqlQuery::SetFont(CFont* pFont, BOOL bRedraw)
{
	m_ctl_edit.SetFont(pFont,bRedraw);
	m_ctl_edit.Invalidate();
	m_ctl_edit.UpdateWindow();
}

/////////////////////////////////////////////////////////////////////////////

void CToolSqlQuery::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
   if (IsWindow(m_ctl_edit.GetSafeHwnd()))
	   m_ctl_edit.SetWindowPos(NULL,20,24,cx-40,cy-48,SWP_NOZORDER | SWP_NOMOVE);
}

/////////////////////////////////////////////////////////////////////////////

void CToolSqlQuery::OnCancel()
{
}

/////////////////////////////////////////////////////////////////////////////

void CToolSqlQuery::OnClose()
{
}

/////////////////////////////////////////////////////////////////////////////

void CToolSqlQuery::OnQueryPb()
{
   GetParent()->GetParent()->PostMessage(WM_COMMAND,IDM_QUERY_EXEC);
}

/////////////////////////////////////////////////////////////////////////////

void CToolSqlQuery::OnQueryDatabases()
{
   GetParent()->GetParent()->PostMessage(WM_COMMAND,IDM_QUERY_DATABASES);
}
/////////////////////////////////////////////////////////////////////////////

BOOL CToolSqlQuery::PreTranslateMessage(MSG* pMsg)
{
   if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
   {
      if (::TranslateAccelerator(m_hWnd, m_hAccel, pMsg))
         return TRUE;
   }
	return CDialog::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////

BOOL CToolSqlQuery::OnInitDialog()
{

   CDialog::OnInitDialog();
   m_hAccel = ::LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE ( IDR_MAINFRAME ));
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
