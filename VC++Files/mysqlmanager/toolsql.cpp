// ToolSql.cpp : implementation file
//

#include "stdafx.h"
#include "MySqlManager.h"
#include "ToolSql.h"

#define WINDOW_COORDS 0
#define CLIENT_COORDS 1

#define MY_TIMER_ID	0x1234

#ifdef _DEBUG
   #define new DEBUG_NEW
   #undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

CToolSql::CToolSql(CWnd* pParent,CResource* pServer,CResource* pResource)
: CDialog(CToolSql::IDD, pParent)
, m_pQuery(0)
, m_pResults(0)
, m_pStatus(0)
, m_pServer(pServer)
, m_pResource(pResource)
, m_ui_timer(0)
{
   //{{AFX_DATA_INIT(CToolSql)
	m_nIntervalTimerSeconds = 10;
	m_bClear = FALSE;
	//}}AFX_DATA_INIT
   memset ( & m_lf, 0,sizeof(m_lf) );
}

/////////////////////////////////////////////////////////////////////////////

CToolSql::~CToolSql()
{

	if (m_ui_timer)
	{
		KillTimer(MY_TIMER_ID);
	}

	if (m_pdb)
   {
      mysql_close(m_pdb);
   }
   if (m_pQuery)
   {
      m_pQuery->DestroyWindow();
      delete m_pQuery;
   }
   if (m_pResults)
   {
      m_pResults->DestroyWindow();
      delete m_pResults;
   }
   if (m_pStatus)
   {
      m_pStatus->DestroyWindow();
      delete m_pStatus;
   }
}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::DoDataExchange(CDataExchange* pDX)
{
   CDialog::DoDataExchange(pDX);
   //{{AFX_DATA_MAP(CToolSql)
	DDX_Control(pDX, IDC_STOP_PB, m_ctl_Stop);
	DDX_Control(pDX, IDC_START_PB, m_ctl_Start);
	DDX_Control(pDX, IDC_SERVER_CB, m_ctl_Server);
   DDX_Control(pDX, IDC_TAB1, m_tabs);
	DDX_Text(pDX, IDC_TIMER_SECS, m_nIntervalTimerSeconds);
	DDV_MinMaxInt(pDX, m_nIntervalTimerSeconds, 1, 120);
	DDX_Check(pDX, IDC_CLEAR, m_bClear);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CToolSql, CDialog)
//{{AFX_MSG_MAP(CToolSql)
	ON_BN_CLICKED(IDC_QUERY_PB, OnQueryPb)
	ON_BN_CLICKED(IDC_DATABASES_PB, OnQueryDatabases)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnSelchangeTab1)
	ON_BN_CLICKED(IDC_FONT_PB, OnFontPb)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_START_PB, OnStartPb)
	ON_BN_CLICKED(IDC_STOP_PB, OnStopPb)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_CLEAR, OnClear)
	ON_COMMAND(IDM_QUERY_EXEC, OnQueryPb)
	ON_COMMAND(IDM_QUERY_DATABASES, OnQueryDatabases)
	ON_EN_CHANGE(IDC_TIMER_SECS, OnChangeTimerSecs)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

BOOL CToolSql::OnInitDialog()
{

   CDialog::OnInitDialog();

	m_ctl_Start . EnableWindow(TRUE);
	m_ctl_Stop  . EnableWindow(FALSE);

   CString strTitle;

   strTitle.Format ("mySql Query to %s on %s",m_pServer->GetDisplayName(),m_pServer->GetHostName());

	m_ctl_Server.AddString ( m_pServer->GetDisplayName() );
	m_ctl_Server.SetCurSel (0);

   SetWindowText(strTitle);

   CWaitCursor x;

   m_btn_QueryExec.AutoLoad ( IDC_QUERY_PB, this );
   m_btn_QueryDatabases.AutoLoad ( IDC_DATABASES_PB, this );
   m_btn_Font.AutoLoad ( IDC_FONT_PB, this );

   m_tabs.GetWindowRect ( m_rectTab[WINDOW_COORDS] );
   GetWindowRect        ( m_rectDlg[WINDOW_COORDS] );

   m_tabs.GetClientRect ( m_rectTab[CLIENT_COORDS] );
   GetClientRect        ( m_rectDlg[CLIENT_COORDS] );

   CMenu* pSysMenu = GetSystemMenu(FALSE);
   if (pSysMenu != NULL)
   {
      CString strText;
      strText.LoadString(IDS_QUERY_EXEC);
      if (!strText.IsEmpty())
      {
         pSysMenu->AppendMenu(MF_SEPARATOR);
         pSysMenu->AppendMenu(MF_STRING, IDM_QUERY_EXEC, strText);
      }
      strText.LoadString(IDS_QUERY_DATABASES);
      if (!strText.IsEmpty())
      {
         pSysMenu->AppendMenu(MF_STRING, IDM_QUERY_DATABASES, strText);
      }
   }


   m_pdb = new MYSQL;

   CString strQuery   ( "Query" );
   CString strResults ( "Results" );
   CString strStatus  ( "Status" );

   TC_ITEM tc1 = { TCIF_TEXT, 0,0, (LPSTR)(LPCTSTR)strQuery,   strQuery.GetLength(), 0,0};
   TC_ITEM tc2 = { TCIF_TEXT, 0,0, (LPSTR)(LPCTSTR)strResults, strResults.GetLength(), 0,0};
   TC_ITEM tc3 = { TCIF_TEXT, 0,0, (LPSTR)(LPCTSTR)strStatus,  strStatus.GetLength(), 0,0};

   m_tabs.InsertItem ( 0,&tc1 );
   m_tabs.InsertItem ( 1,&tc2 );
   m_tabs.InsertItem ( 2,&tc3 );

   m_pQuery   = new CToolSqlQuery ( NULL );
   m_pResults = new CToolSqlResults ( NULL );
   m_pStatus  = new CToolSqlStatus ( NULL );

   try
   {

//   OpenDatabase();
//
//   m_pSelection->SetDatabase ( & m_db );
//   m_pScript->SetDatabase ( & m_db );
//   m_pLog->SetDatabase ( & m_db );

      m_pQuery   -> Create ( (LPCTSTR)IDD_TOOL_SQL_QUERY,   &m_tabs );
      m_pResults -> Create ( (LPCTSTR)IDD_TOOL_SQL_RESULTS, &m_tabs );
      m_pStatus  -> Create ( (LPCTSTR)IDD_TOOL_SQL_STATUS,  &m_tabs );

      ActivateTab ( 0 );

      m_pQuery   -> SetWindowPos(NULL,20,24,0,0,SWP_NOZORDER|SWP_NOSIZE);
      m_pResults -> SetWindowPos(NULL,20,24,0,0,SWP_NOZORDER|SWP_NOSIZE);
      m_pStatus  -> SetWindowPos(NULL,20,24,0,0,SWP_NOZORDER|SWP_NOSIZE);

      DoOnSize( SIZE_RESTORED, m_rectDlg[CLIENT_COORDS].Width(), m_rectDlg[CLIENT_COORDS].Height() );

   }
   catch (...)
   {
   }

   mysql_init(m_pdb);
   if (!mysql_real_connect(m_pdb,m_pServer->GetHostName(), m_pServer->GetUserName(),m_pServer->GetPassword(),0,m_pServer->GetPortNumber(), NullS,0))
   {
//     my_printf_error(0,"connect to server at '%s' failed; error: '%s'",
//     MYF(ME_BELL), pResource->GetHostName(), mysql_error(&mysql));
      CString strText;
      strText.Format ( "connect to server at '%s' failed; error: '%s'", m_pServer->GetHostName(), mysql_error(m_pdb));
      AfxMessageBox(strText);
      EndDialog(IDCANCEL);
      return FALSE;
   }

   if ( m_pResource && m_pResource->GetType() == CResource::eDatabase )
   {
      CString strDB = m_pResource->GetDisplayName();
      strDB.TrimRight();
      if (mysql_select_db(m_pdb,strDB))
      {
      }
   }

   return FALSE;  // return TRUE unless you set the focus to a control
   // EXCEPTION: OCX Property Pages should return FALSE
}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::ActivateTab ( int tab )

{
   switch (tab)
   {
      case 0:   ;
         m_pResults-> ShowWindow(SW_HIDE);
         m_pStatus-> ShowWindow(SW_HIDE);
         m_pQuery-> ShowWindow(SW_SHOW);
         m_pQuery->m_ctl_edit.SetFocus();
         break;
      case 1:   ;
         m_pQuery-> ShowWindow(SW_HIDE);
         m_pStatus-> ShowWindow(SW_HIDE);
         m_pResults-> ShowWindow(SW_SHOW);
         m_pResults->m_ctl_edit.SetFocus();
         break;
      case 2:   ;
         m_pResults-> ShowWindow(SW_HIDE);
         m_pQuery-> ShowWindow(SW_HIDE);
         m_pStatus-> ShowWindow(SW_SHOW);
         m_pStatus->m_ctl_edit.SetFocus();
         break;
      default:
         break;
   }

}

/////////////////////////////////////////////////////////////////////////////

void CalculateFontSize ( CEdit& ed, CSize& sizeRet )

{

   CDC* pdc = ed.GetDC();

   int nAveWidth , nAveHeight;
   int i ;

   CSize size ;

   static BOOL bFirstTime = TRUE;
   static char rgchAlphabet [54] ;

   if ( bFirstTime )
   {
      bFirstTime = false;
      for ( i = 0 ; i <= 25 ; i++)
      {
         rgchAlphabet[i]    = (char)(i+(int)'a') ;
         rgchAlphabet[i+26] = (char)(i+(int)'A') ;
      }
      rgchAlphabet[52] = 0x20;
      rgchAlphabet[53] = 0x20;
   }

   CFont* pf = ed.GetFont();
   LOGFONT lf;
   pf->GetLogFont(&lf);
   pdc->SelectObject (pf);
   GetTextExtentPoint32 ( pdc->m_hDC, (LPSTR) rgchAlphabet, 54, & size ) ;

   nAveWidth = size.cx / 54 ;

   if ( size.cx % 54 )
   {
      nAveWidth++;
   }

   nAveHeight = size.cy; //6 * size.cy / 4;

   sizeRet.cx = nAveWidth;
   sizeRet.cy = nAveHeight; // tm.tmHeight;

   ed.ReleaseDC(pdc);

}

///////////////////////////////////////////////////////////////////////////////
int ProcessYieldMessage ()
{

   CWinApp* pApp = AfxGetApp();

   if ( pApp )
   {
      MSG msgx;
      while (::PeekMessage(&msgx, NULL, NULL, NULL, PM_NOREMOVE))
         try
         {
            if (!pApp->PumpMessage())
            {
//       ExitProcess(1);
            }
         }
         catch (...)
         {
         }
   }

   return 0;

}


/////////////////////////////////////////////////////////////////////////////

void print_table_data(MYSQL_RES *result,CString& str,CEdit& ed,LOGFONT& lf)
{
   MYSQL_ROW    cur;
   uint         length;
   MYSQL_FIELD* field;
   bool*        num_flag;
   my_ulonglong nRows  =  mysql_num_rows(result);
   uint         nFields = mysql_num_fields(result);
   int*         rgi = new int [nFields];
   memset ( rgi, 0,   nFields*sizeof(int) );
   num_flag=(bool*) my_alloca(sizeof(bool)*nFields);

   ed.SetLimitText(65535*16);

   CSize sizeFont;
   CalculateFontSize ( ed, sizeFont );
   uint index = 0;
   rgi[index++]=0;
   CString separator("");

   mysql_field_seek(result,0);

   for (uint off=0; (field = mysql_fetch_field(result)) ; off++)
   {
      uint length= (uint) strlen(field->name);
      length=max(length,field->max_length);
      if (length < 4 && !IS_NOT_NULL(field->flags))
         length=4;               // Room for "NULL"
      field->max_length=length+1;
      int n=length+2;
      for (uint i=lstrlen(field->name); i-- > 0 ; ) separator+="-";
      if ( index!= nFields )
      {
         int o = rgi[index-1];
         rgi[index++]=o+((n+1)*sizeFont.cx)/2;
      }
      separator+='\t';
      str += field->name;
      str += "\t";
      num_flag[off]= IS_NUM(field->type);
   }
   separator += "\r\n";
   str += "\r\n";
   str += separator;
   ed.SetSel(-1,-1);
   ed.ReplaceSel(str);

   if ( 1 || nRows > 100 )
   {
      while ((cur = mysql_fetch_row(result)))
      {
         ProcessYieldMessage ();
         mysql_field_seek(result,0);
         str.Empty();
         ed.SetSel(-1,-1);
         for (uint off=0 ; off < mysql_num_fields(result); off++)
         {
            field = mysql_fetch_field(result);
            length=field->max_length;
            CString strText;
            strText.Format ("%s", cur[off] ? (char*) cur[off] : "NULL");
            str += strText;
            str += "\t";
         }
         str += "\r\n";
         ed.SetSel(-1,-1);
         ed.ReplaceSel(str);
      }
   }
   else
   {
      while ((cur = mysql_fetch_row(result)))
      {
         mysql_field_seek(result,0);
         for (uint off=0 ; off < mysql_num_fields(result); off++)
         {
            field = mysql_fetch_field(result);
            length=field->max_length;
            CString strText;
            strText.Format ("%s", cur[off] ? (char*) cur[off] : "NULL");
            str += strText;
            str += "\t";
         }
         str += "\r\n";
      }
   }
   my_afree((gptr) num_flag);
   str += "\r\n";
   ed.SetTabStops(nFields,rgi);
   delete [] rgi;
}



/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnQueryPb()
{

   CWaitCursor x;
//   mysql_select_db(m_pdb,"mysql");

   if ( m_pResource && m_pResource->GetType() == CResource::eDatabase )
   {
      CString strDB = m_pResource->GetDisplayName();
      strDB.TrimRight();
      if (mysql_select_db(m_pdb,strDB))
      {
      }
   }

   m_pQuery->UpdateData();
   m_pResults->m_edit.Empty();
   CString str = m_pQuery->m_edit;
   if ( mysql_real_query(m_pdb,str,str.GetLength())==0 )
   {
      MYSQL_RES *result;
      if ((result=mysql_store_result(m_pdb)))
      {
         my_ulonglong nRows = mysql_num_rows(result);
         m_pResults->UpdateData(FALSE);
         m_tabs.SetCurSel(1);
         ActivateTab ( 1 );
         print_table_data(result,m_pResults->m_edit,m_pResults->m_ctl_edit,m_lf);
//       m_pResults->UpdateData(FALSE);
         m_pResults->m_ctl_edit.SetSel(-1,-1);
         CString strText;
         strText.Format ( "\r\n(%d row(s) affected)\r\n", nRows );
         m_pResults->m_ctl_edit.ReplaceSel(strText);
			mysql_free_result(result);
      }
      else
      {
         m_pResults->m_edit = mysql_error(m_pdb);
         m_pResults->UpdateData(FALSE);
      }
   }
   else
   {
      m_pResults->m_edit = mysql_error(m_pdb);
      m_pResults->UpdateData(FALSE);
   }

   m_tabs.SetCurSel(1);
   ActivateTab ( 1 );

}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnQueryDatabases()
{
   CWaitCursor x;
   MYSQL_RES *result;
   m_pResults->m_edit.Empty();
   if ((result=mysql_list_dbs(m_pdb,0)))
   {
      my_ulonglong nRows = mysql_num_rows(result);
      print_table_data(result,m_pResults->m_edit,m_pResults->m_ctl_edit,m_lf);
      //m_pResults->UpdateData(FALSE);
      mysql_free_result(result);
   }
   else
   {
      m_pResults->m_edit = mysql_error(m_pdb);
      m_pResults->UpdateData(FALSE);
   }

   m_tabs.SetCurSel(1);
   ActivateTab ( 1 );

}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult)
{
   ActivateTab ( m_tabs.GetCurSel() );
   *pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnFontPb()
{

   CFontDialog FontDlg ( & m_lf );

   if ( FontDlg.DoModal ( ) == IDOK )
   {
      if (m_font.GetSafeHandle())
         m_font.DeleteObject();
      m_lf = *FontDlg.m_cf.lpLogFont;
      m_font.CreateFontIndirect(FontDlg.m_cf.lpLogFont);
      m_pQuery->SetFont(&m_font);
      m_pResults->SetFont(&m_font);
      m_pStatus->SetFont(&m_font);
   }

}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::DoOnSize(UINT nType, int cx, int cy)
{

   int nx = cx - ( m_rectDlg[CLIENT_COORDS].Width  ( ) - m_rectTab[CLIENT_COORDS].Width  ( ) );
   int ny = cy - ( m_rectDlg[CLIENT_COORDS].Height ( ) - m_rectTab[CLIENT_COORDS].Height ( ) );

   if (IsWindow(m_tabs.GetSafeHwnd()))
   {
      m_tabs.SetWindowPos ( NULL
                            , 0
                            , 0
                            , nx
                            , ny
                            , SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW );

      if (m_pResults&&IsWindow(m_pResults->GetSafeHwnd()))
         m_pResults -> SetWindowPos(NULL,20,24,nx-40,ny-48,SWP_NOZORDER | SWP_NOMOVE );
      if (m_pQuery&&IsWindow(m_pQuery->GetSafeHwnd()))
         m_pQuery -> SetWindowPos(NULL,20,24,nx-40,ny-48,SWP_NOZORDER | SWP_NOMOVE );
      if (m_pStatus&&IsWindow(m_pStatus->GetSafeHwnd()))
         m_pStatus -> SetWindowPos(NULL,20,24,nx-40,ny-48,SWP_NOZORDER | SWP_NOMOVE );
//     switch ( m_tabs.GetCurSel() )
//     {
//        case 0:
//        {
//          if (m_pResults&&IsWindow(m_pResults->GetSafeHwnd()))
//             m_pResults -> SetWindowPos(NULL,20,24,nx-40,ny-48,SWP_NOZORDER | SWP_NOMOVE | SWP_HIDEWINDOW );
//          if (m_pQuery&&IsWindow(m_pQuery->GetSafeHwnd()))
//             m_pQuery -> SetWindowPos(NULL,20,24,nx-40,ny-48,SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW );
//          break;
//        }
//        case 1:
//        {
//          if (m_pQuery&&IsWindow(m_pQuery->GetSafeHwnd()))
//             m_pQuery -> SetWindowPos(NULL,20,24,nx-40,ny-48,SWP_NOZORDER | SWP_NOMOVE | SWP_HIDEWINDOW );
//          if (m_pResults&&IsWindow(m_pResults->GetSafeHwnd()))
//             m_pResults -> SetWindowPos(NULL,20,24,nx-40,ny-48,SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW );
//          break;
//        }
//     }
   }

}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnSize(UINT nType, int cx, int cy)
{

   CDialog::OnSize(nType, cx, cy);

   DoOnSize ( nType, cx, cy );

}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnOK()
{
   CDialog::OnOK();
}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnCancel()
{
   CDialog::OnCancel();
}

/////////////////////////////////////////////////////////////////////////////

BOOL CToolSql::PreTranslateMessage(MSG* pMsg)
{
   return CDialog::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::DoProcessListQuery()
{

	MYSQL_RES *result;
	if (result=mysql_list_processes(m_pdb))
	{
		if (m_bClear)
		{
			m_pStatus->m_edit.Empty();
			m_pStatus->UpdateData(FALSE);
		}
		print_table_data(result,m_pStatus->m_edit,m_pStatus->m_ctl_edit,m_lf);
		mysql_free_result(result);
	}
	else
	{
//		my_printf_error(0,"process list failed; error: '%s'",MYF(ME_BELL),mysql_error(mysql));
	}

}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnStartPb()
{
	UpdateData();
	if (m_ui_timer) return;
	if (m_nIntervalTimerSeconds<1) return;
	ActivateTab ( 2 );
	m_ui_timer = SetTimer( MY_TIMER_ID, m_nIntervalTimerSeconds*1000, NULL );
	m_ctl_Start . EnableWindow(FALSE);
	m_ctl_Stop  . EnableWindow(TRUE);
	DoProcessListQuery();
}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnStopPb()
{
	UpdateData();
	if (m_ui_timer)
	{
		KillTimer(MY_TIMER_ID);
		m_ui_timer = 0;
	}
	m_ctl_Start . EnableWindow(TRUE);
	m_ctl_Stop  . EnableWindow(FALSE);
}

/////////////////////////////////////////////////////////////////////////////

void CToolSql::OnTimer(UINT nIDEvent)
{
	DoProcessListQuery();
	CDialog::OnTimer(nIDEvent);
}

void CToolSql::OnDestroy()
{
	if (m_ui_timer)
	{
		KillTimer(MY_TIMER_ID);
		m_ui_timer = 0;
	}
	CDialog::OnDestroy();
}

void CToolSql::OnClear()
{
	UpdateData();
}

void CToolSql::OnChangeTimerSecs()
{
	UpdateData();
}
