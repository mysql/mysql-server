// RegisterServer.cpp : implementation file
//

#include "stdafx.h"
#include "mysqlmanager.h"
#include "RegisterServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRegisterServer dialog


CRegisterServer::CRegisterServer(CWnd* pParent /*=NULL*/)
	: CDialog(CRegisterServer::IDD, pParent)
	, m_strServer("servername")
	, m_strHost("localhost")
	, m_strUser("root")
	, m_strPassword("")
{
	//{{AFX_DATA_INIT(CRegisterServer)
	m_strPort = _T("3306");
	//}}AFX_DATA_INIT
}


void CRegisterServer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRegisterServer)
	DDX_CBString(pDX, ID_SERVER_CB, m_strServer);
	DDX_CBString(pDX, ID_HOST_CB, m_strHost);
	DDX_Text(pDX, ID_USER, m_strUser);
	DDX_Text(pDX, ID_PASSWORD, m_strPassword);
	DDX_CBString(pDX, ID_PORT_CB, m_strPort);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRegisterServer, CDialog)
	//{{AFX_MSG_MAP(CRegisterServer)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRegisterServer message handlers
