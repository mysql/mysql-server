// MySqlManagerView.h : interface of the CMySqlManagerView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MYSQLMANAGERVIEW_H__826CB2F4_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
#define AFX_MYSQLMANAGERVIEW_H__826CB2F4_8B6D_11D1_AEC1_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <afxcview.h>
#include "cresource.h"

class CToolSql;

class CMySqlManagerView : public CTreeView
{
protected: // create from serialization only
	CMySqlManagerView();
	DECLARE_DYNCREATE(CMySqlManagerView)

// Attributes
public:
	CMySqlManagerDoc* GetDocument();

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMySqlManagerView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	//}}AFX_VIRTUAL

// Implementation

   CResource*        GetSelectedResource(HTREEITEM* phItemRet=NULL);
   CResourceServer*  GetServerResource(HTREEITEM hItem);

   HTREEITEM   AddResource ( HTREEITEM hParent, CResource* pRes, HTREEITEM hLastItem = TVI_FIRST ) ;
   void        ProcessResultSet ( HTREEITEM hItem, LPVOID result, CResource* pResource );

public:
	virtual ~CMySqlManagerView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

	CTreeCtrl*	   m_pTree;
	CImageList*	   m_pImages;
	CBitmap*	      m_pbmBmp;
	CToolSql*      m_pTool;

// Generated message map functions
protected:
	//{{AFX_MSG(CMySqlManagerView)
	afx_msg void OnDblclk(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSqlToolQuery();
	afx_msg void OnRefresh();
	afx_msg void OnRegisterServer();
   afx_msg void OnServerProperties();
	afx_msg void OnRclick(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in MySqlManagerView.cpp
inline CMySqlManagerDoc* CMySqlManagerView::GetDocument()
   { return (CMySqlManagerDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MYSQLMANAGERVIEW_H__826CB2F4_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
