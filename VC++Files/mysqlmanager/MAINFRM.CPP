// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "MySqlManager.h"
#include "MainFrm.h"

CMainFrame* CMainFrame::g_pMainFrame = NULL;

#ifdef _DEBUG
   #define new DEBUG_NEW
   #undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
//{{AFX_MSG_MAP(CMainFrame)
// NOTE - the ClassWizard will add and remove mapping macros here.
//    DO NOT EDIT what you see in these blocks of generated code !
	ON_WM_CREATE()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

static UINT indicators[] =
{
   ID_SEPARATOR,           // status line indicator
   ID_INDICATOR_CAPS,
   ID_INDICATOR_NUM,
   ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
   // TODO: add member initialization code here

}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
   if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
      return -1;

   if (!m_wndToolBar.Create(this) ||
       !m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
   {
      TRACE0("Failed to create toolbar\n");
      return -1;      // fail to create
   }

   if (!m_wndStatusBar.Create(this) ||
       !m_wndStatusBar.SetIndicators(indicators,
                                     sizeof(indicators)/sizeof(UINT)))
   {
      TRACE0("Failed to create status bar\n");
      return -1;      // fail to create
   }

   // TODO: Remove this if you don't want tool tips or a resizeable toolbar
   m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
                            CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);

   // TODO: Delete these three lines if you don't want the toolbar to
   //  be dockable
   m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
   EnableDocking(CBRS_ALIGN_ANY);
   DockControlBar(&m_wndToolBar);

   return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
   // TODO: Modify the Window class or styles here by modifying
   //  the CREATESTRUCT cs

   return CMDIFrameWnd::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
   CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
   CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

int CMainFrame::StatusMsg ( LPCSTR fmt, ... )

{

   char buf [2048];
   va_list args;
   va_start(args, fmt);
   int ret = vsprintf(buf, fmt, args);

   if ( this != NULL )
   {
      static char g_StatusMsg_Buffer_TT [ 2048 ];
      memcpy ( g_StatusMsg_Buffer_TT, buf, sizeof(g_StatusMsg_Buffer_TT) );
      m_wndStatusBar.SetPaneText ( 0, buf );
      m_wndStatusBar.UpdateWindow ();
   }

   va_end(args);
   return ( ret );

}


BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
   g_pMainFrame = this;
   return CMDIFrameWnd::OnCreateClient(lpcs, pContext);
}
