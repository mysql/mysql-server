// MySqlManagerView.cpp : implementation of the CMySqlManagerView class
//

#include "stdafx.h"
#include "MySqlManager.h"
#include "MySqlManagerDoc.h"
#include "MySqlManagerView.h"
#include "mainfrm.h"
#include "ToolSql.h"
#include "RegisterServer.h"

class XStatus
{
public:
   XStatus ( LPCSTR fmt, ... )
   {
      char buf [2048];
      va_list args;
      va_start(args, fmt);
      int ret = vsprintf(buf, fmt, args);
      MainFrame->StatusMsg ( "%s", buf );
      va_end(args);
   }
   ~XStatus()
   {
      MainFrame->StatusMsg ( " ");
   }
private:
   XStatus();
};

#ifdef _DEBUG
   #define new DEBUG_NEW
   #undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CMySqlManagerView, CTreeView)

BEGIN_MESSAGE_MAP(CMySqlManagerView, CTreeView)
//{{AFX_MSG_MAP(CMySqlManagerView)
ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblclk)
ON_COMMAND(IDM_SQL_TOOL_QUERY, OnSqlToolQuery)
ON_COMMAND(IDM_REFRESH, OnRefresh)
ON_COMMAND(IDM_TOOLS_SERVER_PROPERTIES,OnServerProperties)
ON_COMMAND(IDM_TOOLS_REGISTER_SERVER, OnRegisterServer)
ON_NOTIFY_REFLECT(NM_RCLICK, OnRclick)
//}}AFX_MSG_MAP
// Standard printing commands
ON_COMMAND(ID_FILE_PRINT, CTreeView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_DIRECT, CTreeView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_PREVIEW, CTreeView::OnFilePrintPreview)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

CMySqlManagerView::CMySqlManagerView()
: m_pTree(0)
, m_pImages(0)
, m_pbmBmp(0)
, m_pTool(0)
{
}

/////////////////////////////////////////////////////////////////////////////

CMySqlManagerView::~CMySqlManagerView()
{
   if ( m_pbmBmp ) delete m_pbmBmp;
   if ( m_pImages ) delete m_pImages;
   if ( m_pTool )
   {
      m_pTool->DestroyWindow();
      delete m_pTool;
   }
}

/////////////////////////////////////////////////////////////////////////////

BOOL CMySqlManagerView::PreCreateWindow(CREATESTRUCT& cs)
{
   return CTreeView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnDraw(CDC* pDC)
{
   CMySqlManagerDoc* pDoc = GetDocument();
   ASSERT_VALID(pDoc);
}

/////////////////////////////////////////////////////////////////////////////

BOOL CMySqlManagerView::OnPreparePrinting(CPrintInfo* pInfo)
{
   return DoPreparePrinting(pInfo);
}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
}

/////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
void CMySqlManagerView::AssertValid() const
{
   CTreeView::AssertValid();
}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::Dump(CDumpContext& dc) const
{
   CTreeView::Dump(dc);
}

/////////////////////////////////////////////////////////////////////////////

CMySqlManagerDoc* CMySqlManagerView::GetDocument() // non-debug version is inline
{
   ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMySqlManagerDoc)));
   return (CMySqlManagerDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnInitialUpdate()
{

   CTreeView::OnInitialUpdate();

   m_pTree = & GetTreeCtrl();
   m_pImages = new CImageList;
   m_pImages->Create( 16, 16, FALSE, 0, 10 );
   m_pbmBmp = new CBitmap;
   m_pbmBmp->LoadBitmap( IDB_BITMAP1 );
   m_pImages->Add( m_pbmBmp, (COLORREF)0 );
   m_pTree->SetImageList( m_pImages, TVSIL_NORMAL );

   HTREEITEM h = AddResource ( TVI_ROOT, new CResourceServer ( "MySQL", "localhost", "root", "" ) );
//   AddResource ( h, new CResourceProcesslist () );
   h = AddResource ( TVI_ROOT, new CResourceServer ( "Test", "localhost", "test", "" ) );
//   AddResource ( h, new CResourceProcesslist () );

   m_pTree->ModifyStyle(0, TVS_HASLINES|TVS_HASBUTTONS);

}

/////////////////////////////////////////////////////////////////////////////

HTREEITEM CMySqlManagerView::AddResource ( HTREEITEM hParent, CResource* pRes, HTREEITEM hLastItem )
{

   TV_INSERTSTRUCT ItemStruct;
   memset( &ItemStruct, 0, sizeof(ItemStruct) );
   ItemStruct.hParent = hParent;
   ItemStruct.hInsertAfter = hLastItem;
   ItemStruct.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_IMAGE;
   ItemStruct.item.hItem = 0;
   ItemStruct.item.state = 0;
   ItemStruct.item.stateMask = 0;
   ItemStruct.item.pszText = (LPSTR) pRes->GetDisplayName();
   ItemStruct.item.cchTextMax =  (int) strlen( ItemStruct.item.pszText );
   ItemStruct.item.iImage = 2;
   ItemStruct.item.iSelectedImage = 3;
   ItemStruct.item.cChildren = 0;
   ItemStruct.item.lParam = (long) pRes;
   hLastItem = m_pTree->InsertItem( &ItemStruct );
   return hLastItem;
}

//int InsertNetResources( LPNETRESOURCE lpNetResource, CTreeCtrl *pTreeCtrl, HTREEITEM hParent, int *pnCount )
//{
//
//   DWORD Erc;
//   NETRESOURCE   *pNetRes;
//   HANDLE hEnum;
//
//   if( !pTreeCtrl ) return -1;
//   if( pnCount ) *pnCount = 0;
//   Erc = WNetOpenEnum(
//                  RESOURCE_GLOBALNET,//DWORD dwScope,   // scope of enumeration
//                  RESOURCETYPE_ANY,//DWORD dwType,   // resource types to list
//                  0,//DWORD dwUsage,   // resource usage to list
//                  lpNetResource,//LPNETRESOURCE lpNetResource,   // pointer to resource structure
//                  &hEnum//LPHANDLE lphEnum    // pointer to enumeration handle buffer
//                  );
//   if( Erc )
//   {
//      ShowError( Erc );
//      return Erc;
//   }
//
//
//   DWORD dwBufferSize = 1024;
//   pNetRes = (NETRESOURCE *)malloc( dwBufferSize );
//
//   while( TRUE )
//   {
//      DWORD dwCount = 0xFFFFFFFF;
//      Erc = WNetEnumResource(
//                        hEnum,//HANDLE hEnum,   // handle to enumeration
//                        &dwCount,//LPDWORD lpcCount,   // pointer to entries to list
//                        pNetRes,//LPVOID lpBuffer,   // pointer to buffer for results
//                        &dwBufferSize//LPDWORD lpBufferSize    // pointer to buffer size variable
//                        );
//      if( Erc == ERROR_NO_MORE_ITEMS ) return 0;
//      if( Erc )
//      {
//         free( pNetRes );
//         pNetRes = (NETRESOURCE *)malloc( dwBufferSize );
//         Erc = WNetEnumResource(
//                           hEnum,//HANDLE hEnum,   // handle to enumeration
//                           &dwCount,//LPDWORD lpcCount,   // pointer to entries to list
//                           pNetRes,//LPVOID lpBuffer,   // pointer to buffer for results
//                           &dwBufferSize//LPDWORD lpBufferSize    // pointer to buffer size variable
//                           );
//      }
//      if( Erc ){ ShowError( Erc ); return Erc; }
//
//      TV_INSERTSTRUCT ItemStruct;
//      HTREEITEM hLastItem = TVI_FIRST;
//      DWORD i;
//
//      if( pnCount ) *pnCount += dwCount;
//      for( i=0; i<dwCount; i++ )
//      {
//         memset( &ItemStruct, 0, sizeof(ItemStruct) );
//         ItemStruct.hParent = hParent;
//         ItemStruct.hInsertAfter = hLastItem;
//         ItemStruct.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_IMAGE;
//         ItemStruct.item.hItem = 0;
//         ItemStruct.item.state = 0;
//         ItemStruct.item.stateMask = 0;
//         ItemStruct.item.pszText = pNetRes[i].lpRemoteName;
//         ItemStruct.item.cchTextMax = strlen( ItemStruct.item.pszText );
//         ItemStruct.item.iImage = 2;
//         ItemStruct.item.iSelectedImage = 3;
//         ItemStruct.item.cChildren = 0;
//         ItemStruct.item.lParam = (long) (new CNetResource( &pNetRes[i] ));
//
//         hLastItem = pTreeCtrl->InsertItem( &ItemStruct );
//      }
//   }//end while()
//
//   WNetCloseEnum( hEnum );
//   free( pNetRes );
//   return Erc;
//}

/////////////////////////////////////////////////////////////////////////////

static void print_top(MYSQL_RES *result)
{
   uint length;
   MYSQL_FIELD *field;
   mysql_field_seek(result,0);
   while ((field = mysql_fetch_field(result)))
   {
      if ((length= (uint) strlen(field->name)) > field->max_length)
         field->max_length=length;
      else
         length=field->max_length;
   }
}

/////////////////////////////////////////////////////////////////////////////

static void print_header(MYSQL_RES *result,CStringArray& rg)
{
   MYSQL_FIELD *field;
   print_top(result);
   mysql_field_seek(result,0);
   while ((field = mysql_fetch_field(result)))
   {
//    printf(" %-*s|",field->max_length+1,field->name);
      rg.Add(field->name);
   }
   print_top(result);
}


/////////////////////////////////////////////////////////////////////////////

static void print_row(MYSQL_RES *result,MYSQL_ROW row,CStringArray& rg)
{
   uint i,length;
   MYSQL_FIELD *field;
   mysql_field_seek(result,0);
   for (i=0 ; i < mysql_num_fields(result); i++)
   {
      field = mysql_fetch_field(result);
      length=field->max_length;
      rg.Add(row[i] ? (char*) row[i] : "");
//    printf(" %-*s|",length+1,row[i] ? (char*) row[i] : "");
   }
}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::ProcessResultSet ( HTREEITEM hItem, LPVOID r, CResource* pResource )
{

   MYSQL_RES* result = (MYSQL_RES *) r;
   MYSQL_ROW row;

   switch (pResource->GetType())
   {
      case CResource::eProcesslist:
         {
            CResourceProcesslist* p = (CResourceProcesslist*) pResource;
            CResourceProcesslistItem* pi = new CResourceProcesslistItem ();
            CString strText;
            print_header(result,p->m_rgFields);
            for (int i = 0; i<p->m_rgFields.GetSize(); i++ )
            {
               strText += p->m_rgFields[i];
               strText += " ";
            }
            pi->m_strName = strText;
            AddResource ( hItem, pi );
            for (int index=0;(row=mysql_fetch_row(result));index++)
            {
               pi = new CResourceProcesslistItem ();
               print_row(result,row,pi->m_rgFields);
               strText.Empty();
               for (int i = 0; i<pi->m_rgFields.GetSize(); i++ )
               {
                  strText += pi->m_rgFields[i];
                  strText += " ";
               }
               pi->m_strName = strText;
               AddResource ( hItem, pi );
            }
            print_top(result);
            break;
         }
      case CResource::eServer:
         {
            CResourceServer* p = (CResourceServer*) pResource;
            CResourceDatabase* pi = new CResourceDatabase ();
            CString strText;
            /* print_header(result,p->m_rgFields); */
            for (int i = 0; i<p->m_rgFields.GetSize(); i++ )
            {
               strText += p->m_rgFields[i];
               strText += " ";
            }
            pi->m_strName = strText;
            /* AddResource ( hItem, pi ); */
            for (int index=0;(row=mysql_fetch_row(result));index++)
            {
               pi = new CResourceDatabase ();
               print_row(result,row,pi->m_rgFields);
               strText.Empty();
               for (int i = 0; i<pi->m_rgFields.GetSize(); i++ )
               {
                  strText += pi->m_rgFields[i];
                  strText += " ";
               }
               pi->m_strName = strText;
               AddResource ( hItem, pi );
            }
            print_top(result);
            break;
         }
      case CResource::eDatabase:
         {
            CResourceDatabase* p = (CResourceDatabase*) pResource;
            CResourceTable* pi = new CResourceTable ();
            CString strText;
            /* print_header(result,p->m_rgFields); */
            for (int i = 0; i<p->m_rgFields.GetSize(); i++ )
            {
               strText += p->m_rgFields[i];
               strText += " ";
            }
            pi->m_strName = strText;
            /* AddResource ( hItem, pi ); */
            for (int index=0;(row=mysql_fetch_row(result));index++)
            {
               pi = new CResourceTable ();
               print_row(result,row,pi->m_rgFields);
               strText.Empty();
               for (int i = 0; i<pi->m_rgFields.GetSize(); i++ )
               {
                  strText += pi->m_rgFields[i];
                  strText += " ";
               }
               pi->m_strName = strText;
               AddResource ( hItem, pi );
            }
            print_top(result);
            break;
         }
      case CResource::eTable:
         {
            CResourceTable* p = (CResourceTable*) pResource;
            CResourceField* pi = new CResourceField ();
            CString strText;
            /* print_header(result,p->m_rgFields); */
            for (int i = 0; i<p->m_rgFields.GetSize(); i++ )
            {
               strText += p->m_rgFields[i];
               strText += " ";
            }
            pi->m_strName = strText;
            /* AddResource ( hItem, pi ); */
            for (int index=0;(row=mysql_fetch_row(result));index++)
            {
               pi = new CResourceField ();
               print_row(result,row,pi->m_rgFields);
               strText.Empty();
               for (int i = 0; i<pi->m_rgFields.GetSize(); i++ )
               {
                  strText += pi->m_rgFields[i];
                  strText += " ";
               }
               pi->m_strName = strText;
               AddResource ( hItem, pi );
            }
            print_top(result);
            break;
         }
   }


}

/////////////////////////////////////////////////////////////////////////////

CResource* CMySqlManagerView::GetSelectedResource(HTREEITEM* phItemRet)
{
   CResource* pResource = NULL;
   HTREEITEM  hItem = m_pTree->GetSelectedItem();
   if ( hItem )
   {
      TV_ITEM item;
      memset( &item, 0, sizeof(TV_ITEM) );
      item.hItem = hItem;
      item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
      m_pTree->GetItem( &item );
      if ( item.lParam )
      {
         pResource = (CResource*) item.lParam;
      }
   }
   if (phItemRet)
   {
      *phItemRet = hItem;
   }
   return pResource;
}

/////////////////////////////////////////////////////////////////////////////

CResourceServer* CMySqlManagerView::GetServerResource(HTREEITEM hItem)
{

   TV_ITEM item;

   memset( &item, 0, sizeof(TV_ITEM) );
   item.hItem = hItem;
   item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
   m_pTree->GetItem( &item );
   if ( !item.lParam ) return NULL;

   CResource* pResource = (CResource*) item.lParam;

   switch (pResource->GetType())
   {
      case CResource::eServer:
         {
            return (CResourceServer*) pResource;
         }
      case CResource::eDatabase:
         {
            HTREEITEM hParent = m_pTree->GetParentItem(hItem);
            memset( &item, 0, sizeof(TV_ITEM) );
            item.hItem = hParent;
            item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
            m_pTree->GetItem( &item );
            if ( !item.lParam ) return NULL;
            return (CResourceServer*) item.lParam;
         }
      case CResource::eTable:
         {
            HTREEITEM hParent = m_pTree->GetParentItem(m_pTree->GetParentItem(hItem));
            memset( &item, 0, sizeof(TV_ITEM) );
            item.hItem = hParent;
            item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
            m_pTree->GetItem( &item );
            if ( !item.lParam ) return NULL;
            return (CResourceServer*) item.lParam;
         }
   }

   return NULL;

}
/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnDblclk(NMHDR* pNMHDR, LRESULT* pResult)
{
   HTREEITEM hItem;
   hItem = m_pTree->GetSelectedItem();
   *pResult = 0;
   if ( !hItem ) return;

   TV_ITEM item;
   memset( &item, 0, sizeof(TV_ITEM) );
   item.hItem = hItem;
   item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
   m_pTree->GetItem( &item );

   if ( ! item.lParam ) return;

   if ( item.cChildren ) return; //if has got children expand only

   CWaitCursor x;

   CResource* pResource = (CResource*) item.lParam;

   MYSQL mysql;
   MYSQL_RES *result;

   switch (pResource->GetType())
   {
      case CResource::eProcesslist:
         {
            XStatus x ( "Connecting to server %s on host %s..."
                        , (LPCTSTR) pResource->GetDisplayName()
                        , (LPCTSTR) pResource->GetHostName()
                      );
            mysql_init(&mysql);
            if (!mysql_real_connect(&mysql,pResource->GetHostName(), pResource->GetUserName(),pResource->GetPassword(),0,pResource->GetPortNumber(), NullS,0))
            {
					PostMessage(WM_COMMAND,IDM_TOOLS_SERVER_PROPERTIES);
               return;
            }
            mysql.reconnect= 1;
            if (!(result=mysql_list_processes(&mysql)))
            {
               return;
            }
            ProcessResultSet ( hItem, result, pResource );
            mysql_free_result(result);
            mysql_close(&mysql);
            break;
         }
      case CResource::eServer:
         {
            MainFrame->StatusMsg ( "Connecting to server %s on host %s..."
                                   , (LPCTSTR) pResource->GetDisplayName()
                                   , (LPCTSTR) pResource->GetHostName()
                                 );
            mysql_init(&mysql);
            if (!mysql_real_connect(&mysql,pResource->GetHostName(), pResource->GetUserName(),pResource->GetPassword(),0,pResource->GetPortNumber(), NullS,0))
            {
					PostMessage(WM_COMMAND,IDM_TOOLS_SERVER_PROPERTIES);
               MainFrame->StatusMsg ( "Error: Connecting to server %s... (%s)"
                                      , (LPCTSTR) pResource->GetDisplayName()
                                      , mysql_error(&mysql)
                                    );
               return;
            }
            mysql.reconnect= 1;
            if (!(result=mysql_list_dbs(&mysql,0)))
            {
            }
            ProcessResultSet ( hItem, result, pResource );
            mysql_free_result(result);
            mysql_close(&mysql);
            MainFrame->StatusMsg ( " " );
            break;
         }
      case CResource::eDatabase:
         {
            CResourceServer* pServer = GetServerResource(hItem);
            if (!pServer) return;
            MainFrame->StatusMsg ( "Connecting to server %s on host %s..."
                                   , (LPCTSTR) pServer->GetDisplayName()
                                   , (LPCTSTR) pServer->GetHostName()
                                 );
            mysql_init(&mysql);
            if (!mysql_real_connect(&mysql,pServer->GetHostName(), pServer->GetUserName(),pServer->GetPassword(),0,pServer->GetPortNumber(), NullS,0))
            {
					PostMessage(WM_COMMAND,IDM_TOOLS_SERVER_PROPERTIES);
               MainFrame->StatusMsg ( "Error: Connecting to server %s... (%s)"
                                      , (LPCTSTR) pServer->GetDisplayName()
                                      , mysql_error(&mysql)
                                    );
               return;
            }
            mysql.reconnect= 1;
            CResourceDatabase* pRes = (CResourceDatabase*) pResource;
            CString strDB = pResource->GetDisplayName();
            strDB.TrimRight();
            if (mysql_select_db(&mysql,strDB))
            {
               MainFrame->StatusMsg ( "Error: Selecting database %s... (%s)"
                                      , (LPCTSTR) strDB
                                      , mysql_error(&mysql)
                                    );
               return;
            }
            if (!(result=mysql_list_tables(&mysql,0)))
            {
            }
            ProcessResultSet ( hItem, result, pRes );
            mysql_free_result(result);
            mysql_close(&mysql);
            MainFrame->StatusMsg ( " " );
            break;
         }
      case CResource::eTable:
         {
            CResourceServer* pServer = GetServerResource(hItem);
            if (!pServer) return;
            MainFrame->StatusMsg ( "Connecting to server %s on host %s..."
                                   , (LPCTSTR) pServer->GetDisplayName()
                                   , (LPCTSTR) pServer->GetHostName()
                                 );
            mysql_init(&mysql);
            if (!mysql_real_connect(&mysql,pServer->GetHostName(), pServer->GetUserName(),pServer->GetPassword(),0,pServer->GetPortNumber(), NullS,0))
            {
					PostMessage(WM_COMMAND,IDM_TOOLS_SERVER_PROPERTIES);
               MainFrame->StatusMsg ( "Error: Connecting to server %s... (%s)"
                                      , (LPCTSTR) pServer->GetDisplayName()
                                      , mysql_error(&mysql)
                                    );
               return;
            }
            mysql.reconnect= 1;
            HTREEITEM hParent = m_pTree->GetParentItem(hItem);
            memset( &item, 0, sizeof(TV_ITEM) );
            item.hItem = hParent;
            item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
            m_pTree->GetItem( &item );
            if ( item.lParam )
            {
               CResourceDatabase* pResDatabase = (CResourceDatabase*) item.lParam;
               CResourceTable* pRes = (CResourceTable*) pResource;
               CString strDB = pResDatabase->GetDisplayName();
               CString strTable = pResource->GetDisplayName();
               strDB.TrimRight();
               strTable.TrimRight();
               if (mysql_select_db(&mysql,strDB))
               {
                  return;
               }
               CString str; str.Format("show fields from %s",(LPCTSTR)strTable);
               if ( mysql_query(&mysql,str)==0 )
               {
                  MYSQL_RES *result;
                  if ((result=mysql_store_result(&mysql)))
                  {
                     ProcessResultSet ( hItem, result, pRes );
                     mysql_free_result(result);
                  }
               }
            }
            mysql_close(&mysql);
            break;
         }
   }

//  InsertNetResources( (LPNETRESOURCE)pTvItem->lParam,
//                 &m_TreeCtrl,
//                 hItem,
//                 &pTvItem->cChildren );
//  pTvItem->mask = TVIF_CHILDREN;
//  m_TreeCtrl.SetItem( pTvItem );

}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnRefresh()
{
   HTREEITEM hItem = NULL;
   CResource* pResource = GetSelectedResource(&hItem);
   if (pResource&&hItem)
   {
      switch (pResource->GetType())
      {
         case CResource::eTable:
            {

               TV_ITEM item;
               MYSQL mysql;
//             MYSQL_RES *result;

               HTREEITEM hParent = m_pTree->GetParentItem(hItem);

               HTREEITEM hChild = m_pTree->GetChildItem(hItem);
               while (hChild)
               {
                  HTREEITEM h = m_pTree->GetNextSiblingItem(hChild);
                  BOOL b = m_pTree->DeleteItem(hChild);
                  hChild = h;
               }
               mysql_init(&mysql);
               if (!mysql_real_connect(&mysql,pResource->GetHostName(), pResource->GetUserName(),pResource->GetPassword(),0,pResource->GetPortNumber(), NullS,0))
               {
                  return;
               }
               mysql.reconnect= 1;
               memset( &item, 0, sizeof(TV_ITEM) );
               item.hItem = hParent;
               item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
               m_pTree->GetItem( &item );
               if ( item.lParam )
               {
                  CResourceDatabase* pResDatabase = (CResourceDatabase*) item.lParam;
                  CResourceTable* pRes = (CResourceTable*) pResource;
                  CString strDB = pResDatabase->GetDisplayName();
                  CString strTable = pResource->GetDisplayName();
                  strDB.TrimRight();
                  strTable.TrimRight();
                  if (mysql_select_db(&mysql,strDB))
                  {
                     return;
                  }
                  CString str; str.Format("show fields from %s",(LPCTSTR)strTable);
                  if ( mysql_query(&mysql,str)==0 )
                  {
                     MYSQL_RES *result;
                     if ((result=mysql_store_result(&mysql)))
                     {
                        ProcessResultSet ( hItem, result, pRes );
                        mysql_free_result(result);
                     }
                  }
               }
               mysql_close(&mysql);
               break;
            }
      }
   }
}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnRegisterServer()
{
   CRegisterServer dlg;
   if (dlg.DoModal()!=IDOK) return;
   AddResource (
               TVI_ROOT,
               new CResourceServer ( dlg.m_strServer, dlg.m_strHost, dlg.m_strUser, dlg.m_strPassword, dlg.m_strPort )
               );
}

/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnServerProperties()
{
   HTREEITEM hItem;
   CResource* pRes = GetSelectedResource(&hItem);
   if (!pRes) return;
   if (pRes->GetType()!=CResource::eServer) return;
   CResourceServer* pResource = (CResourceServer*)pRes;
   CRegisterServer dlg;
   dlg.m_strHost       = pResource->GetHostName();
   dlg.m_strUser       = pResource->GetUserName();
   dlg.m_strPassword   = pResource->GetPassword();
   dlg.m_strPort       = pResource->GetPortName();
   if (dlg.DoModal()!=IDOK) return;
   pResource->m_strHost     = dlg.m_strHost    ;
   pResource->m_strUser     = dlg.m_strUser    ;
   pResource->m_strPassword = dlg.m_strPassword;
   pResource->m_strPort     = dlg.m_strPort    ;
   TV_ITEM item;
   memset( &item, 0, sizeof(TV_ITEM) );
   item.hItem = hItem;
   item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_CHILDREN | TVIF_PARAM ;
   m_pTree->GetItem( &item );
}


/////////////////////////////////////////////////////////////////////////////

void CMySqlManagerView::OnSqlToolQuery()
{

   HTREEITEM hItem;

   CResource* pResource = GetSelectedResource(&hItem);

   if (!pResource) return;

   CResourceServer* pServer = GetServerResource(hItem);
   if (!pServer) return; /* Avoid bug when selecting field */

   m_pTool = new CToolSql ( AfxGetMainWnd(), pServer, pResource );

   if ( ! m_pTool->Create(IDD_TOOL_SQL,this) )
   {
      delete m_pTool;
      m_pTool = 0;
		PostMessage(WM_COMMAND,IDM_TOOLS_SERVER_PROPERTIES);
   }
   else
   {
      m_pTool->ShowWindow(SW_SHOW);
   }

}


/////////////////////////////////////////////////////////////////////////////

BOOL CMySqlManagerView::PreTranslateMessage(MSG* pMsg)
{
   if (m_pTool && m_pTool->PreTranslateMessage(pMsg))
      return TRUE;
   return CTreeView::PreTranslateMessage(pMsg);
}

void CMySqlManagerView::OnRclick(NMHDR* pNMHDR, LRESULT* pResult)
{

   POINT pt;

   GetCursorPos ( & pt );

   CMenu menu;

   menu.CreatePopupMenu ();

   menu.AppendMenu ( MF_ENABLED , IDM_SQL_TOOL_QUERY,          "SQL Query" );
   menu.AppendMenu ( MF_ENABLED , IDM_REFRESH,                 "Refresh active item(s)" );
   menu.AppendMenu ( MF_ENABLED , IDM_TOOLS_REGISTER_SERVER,   "Register server" );
   menu.AppendMenu ( MF_ENABLED , IDM_TOOLS_SERVER_PROPERTIES, "Properties" );

   menu.TrackPopupMenu ( TPM_LEFTALIGN | TPM_RIGHTBUTTON , pt.x, pt.y, CWnd::GetParent(), NULL );

   *pResult = 0;

}
