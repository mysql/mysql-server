// MySqlManagerDoc.h : interface of the CMySqlManagerDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MYSQLMANAGERDOC_H__826CB2F2_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
#define AFX_MYSQLMANAGERDOC_H__826CB2F2_8B6D_11D1_AEC1_00600806E071__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000


class CMySqlManagerDoc : public CDocument
{
protected: // create from serialization only
	CMySqlManagerDoc();
	DECLARE_DYNCREATE(CMySqlManagerDoc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMySqlManagerDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMySqlManagerDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CMySqlManagerDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MYSQLMANAGERDOC_H__826CB2F2_8B6D_11D1_AEC1_00600806E071__INCLUDED_)
