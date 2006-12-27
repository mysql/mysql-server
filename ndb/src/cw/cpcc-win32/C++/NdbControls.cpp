/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "stdafx.h"
#include "NdbControls.h"


/**
*			CNdbControl implementation
*/

BOOL CNdbControl::GetRect(LPRECT lprc) const {
	
	_ASSERT(this) ;

	return GetClientRect(m_hControl, lprc) ;

} 

BOOL CNdbControl::Resize(LONG x, LONG y, LONG w, LONG h) const {

	_ASSERT(this) ;

	if(!MoveWindow(m_hControl, x, y, w, h, TRUE))
		return FALSE ;
	if(m_bVisible){
		ShowWindow(m_hControl, SW_SHOW) ;
		UpdateWindow(m_hControl) ;
	}
	return TRUE ;

}

BOOL CNdbControl::Show(BOOL bShow) {

	_ASSERT(this) ;
	
	if(bShow){
		ShowWindow(m_hControl, SW_SHOW);
		m_bVisible = TRUE ;
	}else{
		ShowWindow(m_hControl, SW_HIDE);
		m_bVisible = FALSE ;
	}
	EnableWindow(m_hControl, bShow) ;
	UpdateWindow(m_hControl) ;
	
	return TRUE ;
}



CNdbControl::~CNdbControl(){

	DestroyWindow(m_hControl) ;
	if(m_hMenu)
	DestroyMenu(m_hMenu) ;

}


/**
*			CNdbListView implementation
*/

BOOL CNdbListView::Create(HINSTANCE hInst, HWND hParent, DWORD dwId, NDB_ITEM_TYPE enType, PNDB_LV pstH, DWORD dwWidth) {

	if(!pstH)
		return FALSE ;

	LV_COLUMN		lvC ;
	m_hInstance	= hInst ;
	m_hParent		= hParent ;
	m_dwId			= dwId ;
	m_dwWidth		= dwWidth ;
	m_dwWidth		= 100 ;
	m_enType			= enType;
	char* szLabels[MAX_LV_HEADERS] ;
	int count		= 0 ;

	m_hControl = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, WC_LISTVIEW, TEXT(""), 
		WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT,
								0, 0, 0, 0,	m_hParent, (HMENU)m_dwId, hInst, NULL ); 

	if(!m_hControl)
		return FALSE ;

	lvC.mask	= LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvC.fmt		= LVCFMT_LEFT;
	
	switch(enType){
		case ITEM_COMPR_ROOT:
			szLabels[0] = pstH->szComputer ;
			szLabels[1] = pstH->szHostname ;
			szLabels[2] = pstH->szStatus ;
			count = 3 ;
			break ;
		case ITEM_DB_ROOT:
			szLabels[0] = pstH->szDatabase ;
			szLabels[1] = pstH->szStatus ;
			count = 2 ;
			break ;
		case ITEM_COMPR:
			szLabels[0] = pstH->szProcess ;
			szLabels[1] = pstH->szDatabase;
			szLabels[2] = pstH->szOwner ;
			szLabels[3] = pstH->szStatus ;
			count = 4 ;
		case ITEM_DB:
			szLabels[0] = pstH->szProcess ;
			szLabels[1] = pstH->szComputer;
			szLabels[2] = pstH->szOwner ;
			szLabels[3] = pstH->szStatus ;
			count = 4 ;
			break ;
		NDB_DEFAULT_UNREACHABLE ;
		}

	for(int j = 0 ; j < count ; ++j){
		lvC.iSubItem	= j ;
		lvC.cx				= m_dwWidth ;
		lvC.pszText		= szLabels[j] ;
		if(0xFFFFFFFF == ListView_InsertColumn(m_hControl, j, &lvC))
			return FALSE ;
	}

	SendMessage(m_hControl, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, 
		LVS_EX_FULLROWSELECT );

	ShowWindow(m_hControl, SW_SHOW) ;

	return TRUE ;

}



/**
*			CNdbToolBar implementation
*/



/**
*			CNdbTreeView implementation
*/

BOOL CNdbTreeView::Create(HINSTANCE hInst, HWND hParent, DWORD dwMenuId, DWORD dwId){
		
		if(!CreateTreeView(hInst, hParent, dwId))
			return FALSE ;

		m_hMenu = LoadMenu(m_hInstance,MAKEINTRESOURCE(dwMenuId)) ;
		if(!m_hMenu)
			return FALSE ;

		return TRUE ;
}


BOOL CNdbTreeView::CreateTreeView(HINSTANCE hInst, HWND hParent, DWORD dwId){
			

	m_hInstance		= hInst ;
	m_hParent			= hParent ;
	m_dwId				= dwId ;
	HIMAGELIST		himl ;
	HBITMAP				hbmp ;
	DWORD dwCount	= 0 ;
 
	m_hControl = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, WC_TREEVIEW, "Tree View", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | 
		TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SINGLEEXPAND, 
        0, 0, 0, 0, m_hParent, (HMENU)m_dwId, m_hInstance, NULL) ;

	if(!m_hControl)
		return FALSE ;
						
	if((himl = ImageList_Create(nX, nY, ILC_MASK | ILC_COLOR8, 4, 0)) == NULL) 
        return FALSE ; 
	
	hbmp = LoadBitmap(m_hInstance, MAKEINTRESOURCE(IDI_OPEN)); 
	hbmp = (HBITMAP)LoadImage(m_hInstance, MAKEINTRESOURCE(IDB_OPEN), IMAGE_BITMAP, nX, 0, LR_DEFAULTSIZE); 
	m_nOpen = ImageList_AddMasked(himl, hbmp, clr); 
	DeleteObject(hbmp); 
	hbmp = (HBITMAP)LoadImage(m_hInstance, MAKEINTRESOURCE(IDB_CLOSED), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE); 
	m_nClosed = ImageList_AddMasked(himl, hbmp, clr); 
	DeleteObject(hbmp); 
	hbmp = (HBITMAP)LoadImage(m_hInstance, MAKEINTRESOURCE(IDB_COMPUTER),IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE); 
	m_nComputer = ImageList_AddMasked(himl, hbmp, clr); 
	DeleteObject(hbmp); 
	hbmp = (HBITMAP)LoadImage(m_hInstance, MAKEINTRESOURCE(IDB_DATABASE), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE); 
	m_nDatabase = ImageList_AddMasked(himl, hbmp, clr); 
	DeleteObject(hbmp);  

	if(ImageList_GetImageCount(himl) < 4) 
        return FALSE ;
			
	TreeView_SetImageList(m_hControl, himl, TVSIL_NORMAL); 

	ShowWindow(m_hControl, SW_SHOW) ;
			
	return TRUE ;

}



HTREEITEM CNdbTreeView::AddItem(LPSTR szText, NDB_ITEM_TYPE enType, DWORD dwLVId){

  TVITEM						tvi ; 
  TVINSERTSTRUCT		tvins ; 
	HTREEITEM					hti ;
	HTREEITEM					hTemp ;
	int nImage				= m_nClosed ;
 
    tvi.mask = TVIF_TEXT | TVIF_IMAGE 
        | TVIF_SELECTEDIMAGE | TVIF_PARAM; 
 
    tvi.pszText = szText; 
    tvi.cchTextMax = lstrlen(szText); 
 
	switch(enType){

		case ITEM_COMPR_ROOT:
			nImage = m_nClosed ;
			if(!m_hPrevRoot)
				tvins.hParent = TVI_ROOT;
			else
				tvins.hInsertAfter = m_hPrevRoot ;
			break ;

		case ITEM_DB_ROOT:
			if(!m_hPrevRoot)
				tvins.hParent = TVI_ROOT;
			else
				tvins.hInsertAfter = m_hPrevRoot ;
			break ;

		case ITEM_COMPR:
			nImage = m_nComputer ;
			if(!m_hPrevComputersChild || !m_hComputersRoot)
				return 0 ;
			else
				tvins.hInsertAfter = m_hPrevComputersChild ;
				tvins.hParent = m_hComputersRoot ;
			break ;

		case ITEM_DB:
			nImage = m_nDatabase ;
			if(!m_hPrevComputersChild || !m_hComputersRoot)
				return 0 ;
			else
				tvins.hInsertAfter = m_hPrevDatabasesChild ;
				tvins.hParent = m_hDatabasesRoot ;
			break ;

		NDB_DEFAULT_UNREACHABLE ;
		
	}

    tvi.iImage = nImage ;
    tvi.iSelectedImage = nImage ; 
    tvi.lParam = (LPARAM) dwLVId ; 
    tvins.item = tvi ; 
  
    hTemp = TreeView_InsertItem(m_hControl, &tvins);
		if(!hTemp)
			return NULL ;
		
	switch(enType){

		case ITEM_COMPR_ROOT:
			m_hComputersRoot = hTemp ;
			break ;

		case ITEM_DB_ROOT:
			m_hDatabasesRoot = hTemp ;
			break ;

		case ITEM_COMPR:
			m_hPrevComputersChild = hTemp ;
			break ;

		case ITEM_DB:
			m_hPrevComputersChild = hTemp ;
			break ;

	NDB_DEFAULT_UNREACHABLE ;

	}
   
    if (ITEM_COMPR_ROOT != enType && ITEM_DB_ROOT != enType) { 

        hti = TreeView_GetParent(m_hControl, hTemp); 
        tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE; 
        tvi.hItem = hti; 
        tvi.iImage = m_nClosed; 
        tvi.iSelectedImage = m_nClosed; 
        TreeView_SetItem(m_hControl, &tvi); 

    } 
    
		return hTemp ; 
} 


BOOL CNdbControls::Create(HINSTANCE hInst, HWND hParent){

		m_hInstance = hInst ;
		m_hParent		= hParent ;
		m_tb.Create(m_hInstance, m_hParent, ID_TOOLBAR, IDB_TOOLBAR) ;
		m_sb.Create(m_hInstance, m_hParent, ID_STATUSBAR) ;
		m_tv.Create(m_hInstance, m_hParent, IDM_TREEVIEW, ID_TREEVIEW) ;
		_assert(AddView("Computers", ITEM_COMPR_ROOT)) ;
		_assert(AddView("Databases", ITEM_DB_ROOT)) ;

		return TRUE ;
}

BOOL CNdbControls::AddListView(NDB_ITEM_TYPE enType, DWORD dwId){

	int						count ;
	CNdbListView*	plv ;
	PNDB_LV				pst ;

	plv = new CNdbListView ;

	if(!plv)
		return FALSE ;

	count = m_map_lvc.GetCount() + m_dwFirstId_lv ;

	switch(enType){
		case ITEM_COMPR_ROOT:
			pst = &m_stlvcRoot ;
			break ;
		case ITEM_DB_ROOT:
			pst = &m_stlvdRoot ;
			break ;
		case ITEM_COMPR:
			pst = &m_stlvc ;
			break ;
		case ITEM_DB:
			pst = &m_stlvd ;
			break ;
		NDB_DEFAULT_UNREACHABLE ;
	}

	plv->Create(m_hInstance, m_hParent, dwId, enType, pst, LV_HEADER_WIDTH) ;
		
	m_map_lvc[count] = plv ;

	return TRUE ;
}

BOOL CNdbControls::AddView(LPSTR szText, NDB_ITEM_TYPE enType){
	
	DWORD dwId_lv = m_dwNextId_lv ;
	
	if(AddListView(enType, dwId_lv) && m_tv.AddItem(szText, enType, dwId_lv))
		m_dwNextId_lv++ ;
	else
		return FALSE ;

	return TRUE ;
};


VOID CNdbControls::ToggleListViews(LPNMTREEVIEW pnmtv){
		
		CNdbListView* plv ;
		int count = m_map_lvc.GetCount() + m_dwFirstId_lv ;
    
		for(int c = FIRST_ID_LV ; c < count; ++c){
					_assert(m_map_lvc.Lookup(c, plv)) ;
					if(pnmtv->itemNew.lParam == (c))
							plv->Show(TRUE) ;
					else
							plv->Show(FALSE) ;
		}
}



VOID CNdbControls::Resize(){

	RECT rc, rcTB, rcSB ;
	LONG tw, sw, lx, ly, lw, lh, tvw, tvh ;
	CNdbListView* plv ;
	int count ; //, id ;
			
	GetClientRect(m_hParent, &rc) ;
	m_tb.GetRect(&rcTB) ;
	m_sb.GetRect(&rcSB) ;
	
	sw = rcSB.bottom ;
	tw = rcTB.bottom ;

	m_tb.Resize(0, 0, rc.right, tw) ;

	tvw = rc.right / 4 ;
	tvh = rc.bottom - sw - tw - BORDER ;

	m_tv.Resize(0, tw + BORDER, tvw, tvh) ;
	
	m_sb.Resize(0, tvh, rc.left, sw) ;

	lx = tvw + BORDER - 2  ;
	ly = tw + BORDER ;
	lw = rc.right - tvw - BORDER + 1 ;
	lh = tvh ;

	count = m_map_lvc.GetCount() + FIRST_ID_LV ;

	for(int c = FIRST_ID_LV ; c < count; ++c){
      _assert(m_map_lvc.Lookup(c, plv)) ;
      plv->Resize(lx, ly, lw, lh) ;
	}

	return ;

}
