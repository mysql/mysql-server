// MySqlManagerDoc.cpp : implementation of the CMySqlManagerDoc class
//

#include "stdafx.h"
#include "MySqlManager.h"

#include "MySqlManagerDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMySqlManagerDoc

IMPLEMENT_DYNCREATE(CMySqlManagerDoc, CDocument)

BEGIN_MESSAGE_MAP(CMySqlManagerDoc, CDocument)
	//{{AFX_MSG_MAP(CMySqlManagerDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMySqlManagerDoc construction/destruction

CMySqlManagerDoc::CMySqlManagerDoc()
{
	// TODO: add one-time construction code here

}

CMySqlManagerDoc::~CMySqlManagerDoc()
{
}

BOOL CMySqlManagerDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CMySqlManagerDoc serialization

void CMySqlManagerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMySqlManagerDoc diagnostics

#ifdef _DEBUG
void CMySqlManagerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CMySqlManagerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMySqlManagerDoc commands
