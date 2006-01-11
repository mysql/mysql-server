/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTUP_C
#include "Dbtup.hpp"

Dbtup::Disk_alloc_info::Disk_alloc_info(const Tablerec* tabPtrP, 
					Uint32 extent_size)
{
  m_extent_size = extent_size;
  m_curr_extent_info_ptr_i = RNIL; 
  if (tabPtrP->m_no_of_disk_attributes == 0)
    return;
  
  Uint32 min_size= 4*tabPtrP->m_offsets[DD].m_fix_header_size;
  Uint32 var_size= tabPtrP->m_offsets[DD].m_max_var_offset;
  
  if (tabPtrP->m_attributes[DD].m_no_of_varsize == 0)
  {
    Uint32 recs_per_page= (4*Tup_fixsize_page::DATA_WORDS)/min_size;
    Uint32 pct_free= 0;
    m_page_free_bits_map[0] = recs_per_page; // 100% free
    m_page_free_bits_map[1] = 1;
    m_page_free_bits_map[2] = 0;
    m_page_free_bits_map[3] = 0;
    
    Uint32 max= recs_per_page * extent_size;
    for(Uint32 i = 0; i<EXTENT_SEARCH_MATRIX_ROWS; i++)
    {
      m_total_extent_free_space_thresholds[i] = 
	(EXTENT_SEARCH_MATRIX_ROWS - i - 1)*max/EXTENT_SEARCH_MATRIX_ROWS;
    }
  }
  else
  {
    abort();
  }

  
}

Uint32
Dbtup::Disk_alloc_info::find_extent(Uint32 sz) const
{
  /**
   * Find an extent with sufficient space for sz
   * Find the biggest available (with most free space)
   * Return position in matrix
   */
  Uint32 mask= EXTENT_SEARCH_MATRIX_COLS - 1;
  for(Uint32 i= 0; i<EXTENT_SEARCH_MATRIX_SIZE; i++)
  {
    // Check that it can cater for request
    if (m_extent_search_matrix[i] < sz)
    {
      i = (i + mask) & ~mask;
      continue;
    }
    
    if (!m_free_extents[i].isEmpty())
    {
      return i;
    }
  }
  
  return RNIL;
}

Uint32
Dbtup::Disk_alloc_info::calc_extent_pos(const Extent_info* extP) const
{
  Uint32 free= extP->m_free_space;
  Uint32 mask= EXTENT_SEARCH_MATRIX_COLS - 1;
  
  Uint32 col= 0, row=0;
  
  /**
   * Find correct row based on total free space
   *   if zero (or very small free space) put 
   *     absolutly last
   */
  {
    
    printf("free space %d free_page_thresholds ", free);
    for(Uint32 i = 0; i<EXTENT_SEARCH_MATRIX_ROWS; i++)
      printf("%d ", m_total_extent_free_space_thresholds[i]);
    ndbout_c("");
    
    const Uint32 *arr= m_total_extent_free_space_thresholds;
    for(; free < * arr++; row++)
      assert(row < EXTENT_SEARCH_MATRIX_ROWS);
  }

  /**
   * Find correct col based on largest available chunk
   */
  {
    const Uint16 *arr= extP->m_free_page_count;
    for(; col < EXTENT_SEARCH_MATRIX_COLS && * arr++ == 0; col++);
  }

  /**
   * NOTE
   *
   *   If free space on extent is small or zero,
   *     col will be = EXTENT_SEARCH_MATRIX_COLS
   *     row will be = EXTENT_SEARCH_MATRIX_ROWS
   *   in that case pos will be col * row = max pos
   *   (as fixed by + 1 in declaration)
   */
  Uint32 pos= (row * (mask + 1)) + (col & mask);
  
  printf("free space %d free_page_count ", free);
  for(Uint32 i = 0; i<EXTENT_SEARCH_MATRIX_COLS; i++)
    printf("%d ", extP->m_free_page_count[i]);
  ndbout_c(" -> row: %d col: %d -> pos= %d", row, col, pos);

  assert(pos < EXTENT_SEARCH_MATRIX_SIZE);
  return pos;
}

/**
 * - Page free bits -
 * 0 = 00 - free - 100% free
 * 1 = 01 - atleast 70% free, 70= pct_free + 2 * (100 - pct_free) / 3
 * 2 = 10 - atleast 40% free, 40= pct_free + (100 - pct_free) / 3
 * 3 = 11 - full - less than pct_free% free, pct_free=10%
 *
 */

int
Dbtup::disk_page_prealloc(Signal* signal, 
			  Ptr<Fragrecord> fragPtr,
			  Local_key* key, Uint32 sz)
{
  int err;
  Uint32 i, ptrI;
  Ptr<Page_request> req;
  Fragrecord* fragPtrP = fragPtr.p; 
  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;
  Uint32 idx= alloc.calc_page_free_bits(sz);
  Tablespace_client tsman(signal, c_tsman,
			  fragPtrP->fragTableId,
			  fragPtrP->fragmentId,
			  fragPtrP->m_tablespace_id);
  
  /**
   * 1) search current dirty pages
   */
  for(i= 0; i <= idx; i++)
  {
    if (!alloc.m_dirty_pages[i].isEmpty())
    {
      ptrI= alloc.m_dirty_pages[i].firstItem;
      Ptr<GlobalPage> page;
      m_global_page_pool.getPtr(page, ptrI);
      
      disk_page_prealloc_dirty_page(alloc, *(PagePtr*)&page, i, sz);
      key->m_page_no= ((Page*)page.p)->m_page_no;
      key->m_file_no= ((Page*)page.p)->m_file_no;
      return 0; // Page in memory
    }
  }
  
  /**
   * Search outanding page requests
   *   callback does not need to access page request again
   *   as it's not the first request to this page
   */
  for(i= 0; i <= idx; i++)
  {
    if (!alloc.m_page_requests[i].isEmpty())
    {
      ptrI= alloc.m_page_requests[i].firstItem;
      Ptr<Page_request> req;
      c_page_request_pool.getPtr(req, ptrI);

      disk_page_prealloc_transit_page(alloc, req, i, sz);
      * key = req.p->m_key;
      //ndbout_c("found transit page");
      return 0;
    }
  }
  
  /**
   * We need to request a page...
   */
  if (!c_page_request_pool.seize(req))
  {
    err= 1;
    //XXX set error code
    ndbout_c("no free request");
    return -err;
  }

  new (req.p) Page_request();

  req.p->m_ref_count= 1;
  req.p->m_frag_ptr_i= fragPtr.i;
  req.p->m_uncommitted_used_space= sz;
  
  int pageBits; // received
  Ptr<Extent_info> ext;
  const Uint32 bits= alloc.calc_page_free_bits(sz); // required
  bool found= false;

  /**
   * Do we have a current extent
   */
  if ((ext.i= alloc.m_curr_extent_info_ptr_i) != RNIL)
  {
    jam();
    c_extent_pool.getPtr(ext);
    if ((pageBits= tsman.alloc_page_from_extent(&ext.p->m_key, bits)) >= 0) 
    {
      jam();
      found= true;
    }
    else
    {
      jam();
      /**
       * The current extent is not in a free list
       *   and since it couldn't accomadate the request
       *   we put it on the free list
       */
      Uint32 pos= alloc.calc_extent_pos(ext.p);
      LocalDLList<Extent_info> list(c_extent_pool, alloc.m_free_extents[pos]);
      list.add(ext);
    }
  }
  
  if (!found)
  {
    Uint32 pos;
    if ((pos= alloc.find_extent(sz)) != RNIL)
    {
      jam();
      LocalDLList<Extent_info> list(c_extent_pool, alloc.m_free_extents[pos]);
      list.first(ext);
      list.remove(ext);
    }
    else
    {
      jam();
      /**
       * We need to alloc an extent
       */
      if (!c_extent_pool.seize(ext))
      {
	//XXX
	err= 2;
	c_page_request_pool.release(req);
	ndbout_c("no free extent info");
	return -err;
      }

      if ((err= tsman.alloc_extent(&ext.p->m_key)) < 0)
      {
	//XXX
	c_extent_pool.release(ext);
	c_page_request_pool.release(req);
	ndbout_c("no free extent");
	return -err;
      }
      
      int pages= err;
      ndbout << "allocated " << pages << " pages: " << ext.p->m_key << endl;
      ext.p->m_first_page_no = ext.p->m_key.m_page_no;
      bzero(ext.p->m_free_page_count, sizeof(ext.p->m_free_page_count));
      ext.p->m_free_space= alloc.m_page_free_bits_map[0] * pages; 
      ext.p->m_free_page_count[0]= pages; // All pages are "free"-est
      c_extent_hash.add(ext);

      LocalSLList<Extent_info, Extent_list_t> 
	list1(c_extent_pool, alloc.m_extent_list);
      list1.add(ext);
    }
    alloc.m_curr_extent_info_ptr_i= ext.i;
    ext.p->m_free_matrix_pos= RNIL;
    pageBits= tsman.alloc_page_from_extent(&ext.p->m_key, bits);
    ndbassert(pageBits >= 0);
  }
  
  /**
   * We have a page from an extent
   */
  *key= req.p->m_key= ext.p->m_key;

  /**
   * We don't know exact free space of page
   *   but we know what page free bits it has.
   *   compute free space based on them
   */
  Uint32 size= alloc.calc_page_free_space((Uint32)pageBits);
  
  ndbassert(size >= sz);
  Uint32 new_size = size - sz;   // Subtract alloc rec
  req.p->m_estimated_free_space= new_size; // Store on page request

  Uint32 newPageBits= alloc.calc_page_free_bits(new_size);
  if (newPageBits != (Uint32)pageBits)
  {
    ndbassert(ext.p->m_free_page_count[pageBits] > 0);
    ext.p->m_free_page_count[pageBits]--;
    ext.p->m_free_page_count[newPageBits]++;
  }
  ndbassert(ext.p->m_free_space >= sz);
  ext.p->m_free_space -= sz;
  
  // And put page request in correct free list
  idx= alloc.calc_page_free_bits(new_size);
  {
    LocalDLList<Page_request> list(c_page_request_pool, 
				   alloc.m_page_requests[idx]);
    
    list.add(req);
  }
  req.p->m_list_index= idx;
  req.p->m_extent_info_ptr= ext.i;

  Page_cache_client::Request preq;
  preq.m_page = *key;
  preq.m_callback.m_callbackData= req.i;
  preq.m_callback.m_callbackFunction = 
    safe_cast(&Dbtup::disk_page_prealloc_callback);
  
  int flags= Page_cache_client::ALLOC_REQ;
  if (pageBits == 0)
  {
    //XXX empty page -> fast to map
    flags |= Page_cache_client::EMPTY_PAGE | Page_cache_client::NO_HOOK;
    preq.m_callback.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_prealloc_initial_callback);
  }
  
  int res= m_pgman.get_page(signal, preq, flags);
  switch(res)
  {
  case 0:
    break;
  case -1:
    ndbassert(false);
    break;
  default:
    execute(signal, preq.m_callback, res); // run callback
  }
  
  return res;
}

void
Dbtup::disk_page_prealloc_dirty_page(Disk_alloc_info & alloc,
				     Ptr<Page> pagePtr, 
				     Uint32 old_idx, Uint32 sz)
{
  ndbassert(pagePtr.p->list_index == old_idx);

  Uint32 free= pagePtr.p->free_space;
  Uint32 used= pagePtr.p->uncommitted_used_space + sz;
  Uint32 ext= pagePtr.p->m_extent_info_ptr;
  
  ndbassert(free >= used);
  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, ext);

  Uint32 new_idx= alloc.calc_page_free_bits(free - used);
  ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;

  if (old_idx != new_idx)
  {
    LocalDLList<Page> old_list(*pool, alloc.m_dirty_pages[old_idx]);
    LocalDLList<Page> new_list(*pool, alloc.m_dirty_pages[new_idx]);
    old_list.remove(pagePtr);
    new_list.add(pagePtr);

    ndbassert(extentPtr.p->m_free_page_count[old_idx]);
    extentPtr.p->m_free_page_count[old_idx]--;
    extentPtr.p->m_free_page_count[new_idx]++;
    pagePtr.p->list_index= new_idx;  
  }

  pagePtr.p->uncommitted_used_space = used;
  ndbassert(extentPtr.p->m_free_space >= sz);
  extentPtr.p->m_free_space -= sz;
  Uint32 old_pos= extentPtr.p->m_free_matrix_pos;
  if (old_pos != RNIL) // Current extent
  {
    jam();
    Uint32 new_pos= alloc.calc_extent_pos(extentPtr.p);
    if (old_pos != new_pos)
    {
      jam();
      Extent_list old_list(c_extent_pool, alloc.m_free_extents[old_pos]);
      Extent_list new_list(c_extent_pool, alloc.m_free_extents[new_pos]);
      old_list.remove(extentPtr);
      new_list.add(extentPtr);
      extentPtr.p->m_free_matrix_pos= new_pos;
    }
  }
}


void
Dbtup::disk_page_prealloc_transit_page(Disk_alloc_info& alloc,
				       Ptr<Page_request> req, 
				       Uint32 old_idx, Uint32 sz)
{
  ndbassert(req.p->m_list_index == old_idx);

  Uint32 free= req.p->m_estimated_free_space;
  Uint32 used= req.p->m_uncommitted_used_space + sz;
  Uint32 ext= req.p->m_extent_info_ptr;
  
  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, ext);

  ndbassert(free >= sz);
  Uint32 new_idx= alloc.calc_page_free_bits(free - sz);
  
  if (old_idx != new_idx)
  {
    DLList<Page_request>::Head *lists = alloc.m_page_requests;
    LocalDLList<Page_request> old_list(c_page_request_pool, lists[old_idx]);
    LocalDLList<Page_request> new_list(c_page_request_pool, lists[new_idx]);
    old_list.remove(req);
    new_list.add(req);

    ndbassert(extentPtr.p->m_free_page_count[old_idx]);
    extentPtr.p->m_free_page_count[old_idx]--;
    extentPtr.p->m_free_page_count[new_idx]++;
    req.p->m_list_index= new_idx;  
  }

  req.p->m_uncommitted_used_space = used;
  req.p->m_estimated_free_space = free - sz;
  ndbassert(extentPtr.p->m_free_space >= sz);
  extentPtr.p->m_free_space -= sz;
  Uint32 old_pos= extentPtr.p->m_free_matrix_pos;
  if (old_pos != RNIL) // Current extent
  {
    jam();
    Uint32 new_pos= alloc.calc_extent_pos(extentPtr.p);
    if (old_pos != new_pos)
    {
      jam();
      Extent_list old_list(c_extent_pool, alloc.m_free_extents[old_pos]);
      Extent_list new_list(c_extent_pool, alloc.m_free_extents[new_pos]);
      old_list.remove(extentPtr);
      new_list.add(extentPtr);
      extentPtr.p->m_free_matrix_pos= new_pos;
    }
  }
}


void
Dbtup::disk_page_prealloc_callback(Signal* signal, 
				Uint32 page_request, Uint32 page_id)
{
  //ndbout_c("disk_alloc_page_callback id: %d", page_id);

  Ptr<Page_request> req;
  c_page_request_pool.getPtr(req, page_request);

  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);

  Ptr<Fragrecord> fragPtr;
  fragPtr.i= req.p->m_frag_ptr_i;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  
  disk_page_prealloc_callback_common(signal, req, fragPtr, gpage);
}

void
Dbtup::disk_page_prealloc_initial_callback(Signal*signal, 
					   Uint32 page_request, 
					   Uint32 page_id)
{
  //ndbout_c("disk_alloc_page_callback_initial id: %d", page_id);  
  /**
   * 1) lookup page request
   * 2) lookup page
   * 3) lookup table
   * 4) init page (according to page type)
   * 5) call ordinary callback
   */
  Ptr<Page_request> req;
  c_page_request_pool.getPtr(req, page_request);

  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);

  Ptr<Fragrecord> fragPtr;
  fragPtr.i= req.p->m_frag_ptr_i;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  Ptr<Tablerec> tabPtr;
  tabPtr.i = fragPtr.p->fragTableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, req.p->m_extent_info_ptr);

  Page* page= (Page*)gpage.p;
  page->m_page_no= req.p->m_key.m_page_no;
  page->m_file_no= req.p->m_key.m_file_no;
  page->m_table_id= fragPtr.p->fragTableId;
  page->m_fragment_id = fragPtr.p->fragmentId;
  page->m_extent_no = extentPtr.p->m_key.m_page_idx; // logical extent no
  page->m_extent_info_ptr= req.p->m_extent_info_ptr;
  page->m_restart_seq = globalData.m_restart_seq;
  page->list_index = 0x8000;
  page->uncommitted_used_space = 0;
  page->nextList = page->prevList = RNIL;
  
  if (tabPtr.p->m_attributes[DD].m_no_of_varsize == 0)
  {
    convertThPage((Fix_page*)gpage.p, tabPtr.p, DD);
  }
  else
  {
    abort();
  }
  disk_page_prealloc_callback_common(signal, req, fragPtr, gpage);
}

void
Dbtup::disk_page_prealloc_callback_common(Signal* signal, 
					  Ptr<Page_request> req, 
					  Ptr<Fragrecord> fragPtr, 
					  Ptr<GlobalPage> pagePtr)
{
  Page* page= (Page*)pagePtr.p;

  /**
   * 1) remove page request from Disk_alloc_info.m_page_requests
   * 2) Add page to Disk_alloc_info.m_dirty_pages
   * 3) register callback in pgman (unmap callback)
   * 4) inform pgman about current users
   */
  ndbassert((page->list_index & 0x8000) == 0x8000);
  ndbassert(page->m_extent_info_ptr == req.p->m_extent_info_ptr);
  ndbassert(page->m_page_no == req.p->m_key.m_page_no);
  ndbassert(page->m_file_no == req.p->m_key.m_file_no);
  Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
  
  Uint32 old_idx = req.p->m_list_index;
  Uint32 free= req.p->m_estimated_free_space;
  Uint32 ext = req.p->m_extent_info_ptr;
  Uint32 used= req.p->m_uncommitted_used_space;
  Uint32 real_free = page->free_space;
  Uint32 real_used = used + page->uncommitted_used_space;
 
  ndbassert(real_free >= free);
  ndbassert(real_free >= real_used);
  ndbassert(alloc.calc_page_free_bits(free) == old_idx);
  Uint32 new_idx= alloc.calc_page_free_bits(real_free - real_used);
  
  /**
   * Add to dirty pages
   */
  ArrayPool<Page> *cheat_pool= (ArrayPool<Page>*)&m_global_page_pool;
  LocalDLList<Page> list(* cheat_pool, alloc.m_dirty_pages[new_idx]);
  list.add(*(Ptr<Page>*)&pagePtr);
  page->uncommitted_used_space = real_used;
  page->list_index = new_idx;

  if (old_idx != new_idx || free != real_free)
  {
    Ptr<Extent_info> extentPtr;
    c_extent_pool.getPtr(extentPtr, ext);

    extentPtr.p->m_free_space += (real_free - free);
    
    if (old_idx != new_idx)
    {
      ndbassert(extentPtr.p->m_free_page_count[old_idx]);
      extentPtr.p->m_free_page_count[old_idx]--;
      extentPtr.p->m_free_page_count[new_idx]++;
    }
    
    Uint32 old_pos= extentPtr.p->m_free_matrix_pos;
    if (old_pos != RNIL) // Current extent
    {
      jam();
      Uint32 new_pos= alloc.calc_extent_pos(extentPtr.p);
      if (old_pos != new_pos)
      {
	jam();
	Extent_list old_list(c_extent_pool, alloc.m_free_extents[old_pos]);
	Extent_list new_list(c_extent_pool, alloc.m_free_extents[new_pos]);
	old_list.remove(extentPtr);
	new_list.add(extentPtr);
	extentPtr.p->m_free_matrix_pos= new_pos;
      }
    }
  }
  
  {
    Page_request_list list(c_page_request_pool, 
			   alloc.m_page_requests[old_idx]);
    list.release(req);
  }
}

int
Dbtup::disk_page_load_hook(Uint32 page_id)
{
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  
  PagePtr pagePtr= *(PagePtr*)&gpage;
  Uint32 type = pagePtr.p->m_page_header.m_page_type;
  if (unlikely(type != File_formats::PT_Tup_fixsize_page &&
	       type != File_formats::PT_Tup_varsize_page))
  {
    ndbassert(false);
    return 0;
  }
  
  pagePtr.p->list_index |= 0x8000;
  pagePtr.p->nextList = pagePtr.p->prevList = RNIL;

  Local_key key;
  key.m_page_no = pagePtr.p->m_page_no;
  key.m_file_no = pagePtr.p->m_file_no;

  if (unlikely(pagePtr.p->m_restart_seq != globalData.m_restart_seq))
  {
    pagePtr.p->m_restart_seq = globalData.m_restart_seq;
    pagePtr.p->uncommitted_used_space = 0;
    
    Ptr<Tablerec> tabPtr;
    tabPtr.i= pagePtr.p->m_table_id;
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
    
    Ptr<Fragrecord> fragPtr;
    getFragmentrec(fragPtr, pagePtr.p->m_fragment_id, tabPtr.p);
    
    Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
    Uint32 idx= alloc.calc_page_free_bits(pagePtr.p->free_space);
    
    pagePtr.p->list_index = idx | 0x8000;    

    Extent_info key;
    key.m_key.m_file_no = pagePtr.p->m_file_no;
    key.m_key.m_page_idx = pagePtr.p->m_extent_no;
    Ptr<Extent_info> extentPtr;
    ndbrequire(c_extent_hash.find(extentPtr, key));
    pagePtr.p->m_extent_info_ptr = extentPtr.i;
    return 1;
  }
  
  return 0;
}

void
Dbtup::disk_page_unmap_callback(Uint32 page_id)
{
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  PagePtr pagePtr= *(PagePtr*)&gpage;

  Uint32 type = pagePtr.p->m_page_header.m_page_type;
  if (unlikely(type != File_formats::PT_Tup_fixsize_page &&
	       type != File_formats::PT_Tup_varsize_page))
  {
    return ;
  }
  
  Uint32 i = pagePtr.p->list_index;

  Local_key key;
  key.m_page_no = pagePtr.p->m_page_no;
  key.m_file_no = pagePtr.p->m_file_no;

  if ((i & 0x8000) == 0)
  {
    Ptr<Tablerec> tabPtr;
    tabPtr.i= pagePtr.p->m_table_id;
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
    
    Ptr<Fragrecord> fragPtr;
    getFragmentrec(fragPtr, pagePtr.p->m_fragment_id, tabPtr.p);
    
    Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;

    ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
    LocalDLList<Page> old(*pool, alloc.m_dirty_pages[i]);
    old.remove(pagePtr);

    if (pagePtr.p->uncommitted_used_space == 0)
    {
      Tablespace_client tsman(0, c_tsman,
			      fragPtr.p->fragTableId,
			      fragPtr.p->fragmentId,
			      fragPtr.p->m_tablespace_id);
      
      tsman.unmap_page(&key);
    }
  }
  pagePtr.p->list_index = i | 0x8000;
}

void
Dbtup::disk_page_alloc(Signal* signal, 
		       Tablerec* tabPtrP, Fragrecord* fragPtrP, 
		       Local_key* key, PagePtr pagePtr, Uint32 gci)
{
  Uint32 logfile_group_id= fragPtrP->m_logfile_group_id;

  Uint64 lsn;
  Uint32 old_free = pagePtr.p->free_space;
  Uint32 old_bits= fragPtrP->m_disk_alloc_info.calc_page_free_bits(old_free);
  if (tabPtrP->m_attributes[DD].m_no_of_varsize == 0)
  {
    ndbassert(pagePtr.p->uncommitted_used_space > 0);
    pagePtr.p->uncommitted_used_space--;
    key->m_page_idx= ((Fix_page*)pagePtr.p)->alloc_record();
    lsn= disk_page_undo_alloc(pagePtr.p, key, 1, gci, logfile_group_id);
  }
  else
  {
    Uint32 sz= key->m_page_idx;
    ndbassert(pagePtr.p->uncommitted_used_space >= sz);
    pagePtr.p->uncommitted_used_space -= sz;
    key->m_page_idx= ((Var_page*)pagePtr.p)->
      alloc_record(sz, (Var_page*)ctemp_page, 0);
    
    lsn= disk_page_undo_alloc(pagePtr.p, key, sz, gci, logfile_group_id);
  }

  Uint32 new_free = pagePtr.p->free_space;
  Uint32 new_bits= fragPtrP->m_disk_alloc_info.calc_page_free_bits(new_free);
  
  if (old_bits != new_bits)
  {
    Tablespace_client tsman(signal, c_tsman,
			    fragPtrP->fragTableId,
			    fragPtrP->fragmentId,
			    fragPtrP->m_tablespace_id);

    tsman.update_page_free_bits(key, new_bits, lsn);
  }
}

void
Dbtup::disk_page_free(Signal *signal, 
		      Tablerec *tabPtrP, Fragrecord * fragPtrP,
		      Local_key* key, PagePtr pagePtr, Uint32 gci)
{
  Uint32 page_idx= key->m_page_idx;
  Uint32 logfile_group_id= fragPtrP->m_logfile_group_id;
  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;
  Uint32 old_free= pagePtr.p->free_space;
  Uint32 old_bits= alloc.calc_page_free_bits(old_free);

  Uint32 sz;
  Uint64 lsn;
  if (tabPtrP->m_attributes[DD].m_no_of_varsize == 0)
  {
    sz = 1;
    const Uint32 *src= ((Fix_page*)pagePtr.p)->get_ptr(page_idx, 0);
    lsn= disk_page_undo_free(pagePtr.p, key,
			     src, tabPtrP->m_offsets[DD].m_fix_header_size,
			     gci, logfile_group_id);
    
    ((Fix_page*)pagePtr.p)->free_record(page_idx);
  }
  else
  {
    const Uint32 *src= ((Var_page*)pagePtr.p)->get_ptr(page_idx);
    sz= ((Var_page*)pagePtr.p)->get_entry_len(page_idx);
    lsn= disk_page_undo_free(pagePtr.p, key,
			     src, sz,
			     gci, logfile_group_id);
    
    ((Var_page*)pagePtr.p)->free_record(page_idx, 0);
  }    
  
  Uint32 new_free = pagePtr.p->free_space;
  Uint32 new_bits = alloc.calc_page_free_bits(new_free);
  
  if (old_bits != new_bits)
  {
    Tablespace_client tsman(signal, c_tsman,
			    fragPtrP->fragTableId,
			    fragPtrP->fragmentId,
			    fragPtrP->m_tablespace_id);

    tsman.update_page_free_bits(key, new_bits, lsn);
  }

  Uint32 ext = pagePtr.p->m_extent_info_ptr;
  Uint32 used = pagePtr.p->uncommitted_used_space;
  ndbassert(old_free >= used);
  ndbassert(new_free >= used);
  ndbassert(new_free >= old_free);
  page_idx = pagePtr.p->list_index;
  Uint32 old_idx = page_idx & 0x7FFF;
  Uint32 new_idx = alloc.calc_page_free_bits(new_free - used);
  ndbassert(alloc.calc_page_free_bits(old_free - used) == old_idx);

  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, ext);

  if (old_idx != new_idx)
  {
    ndbassert(extentPtr.p->m_free_page_count[old_idx]);
    extentPtr.p->m_free_page_count[old_idx]--;
    extentPtr.p->m_free_page_count[new_idx]++;

    if (old_idx == page_idx)
    {
      ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
      LocalDLList<Page> old_list(*pool, alloc.m_dirty_pages[old_idx]);
      LocalDLList<Page> new_list(*pool, alloc.m_dirty_pages[new_idx]);
      old_list.remove(pagePtr);
      new_list.add(pagePtr);
      pagePtr.p->list_index = new_idx;
    }
    else
    {
      pagePtr.p->list_index = new_idx | 0x8000;
    }
  }
  
  extentPtr.p->m_free_space += sz;
  Uint32 old_pos = extentPtr.p->m_free_matrix_pos;
  if (old_pos != RNIL)
  {
    Uint32 pos= alloc.calc_extent_pos(extentPtr.p);
    
    if (pos != old_pos)
    {
      Extent_list old_list(c_extent_pool, alloc.m_free_extents[old_pos]);
      Extent_list new_list(c_extent_pool, alloc.m_free_extents[pos]);
      old_list.remove(extentPtr);
      new_list.add(extentPtr);
      extentPtr.p->m_free_matrix_pos= pos;
    }
  }
}

void
Dbtup::disk_page_abort_prealloc(Signal *signal, Fragrecord* fragPtrP, 
				Local_key* key, Uint32 sz)
{
  Page_cache_client::Request req;
  req.m_callback.m_callbackData= sz;
  req.m_callback.m_callbackFunction = 
    safe_cast(&Dbtup::disk_page_abort_prealloc_callback);
  
  int flags= Page_cache_client::DIRTY_REQ;
  memcpy(&req.m_page, key, sizeof(Local_key));

  int res= m_pgman.get_page(signal, req, flags);
  switch(res)
  {
  case 0:
  case -1:
    break;
  default:
    Ptr<GlobalPage> page;
    m_global_page_pool.getPtr(page, (Uint32)res);
    disk_page_abort_prealloc_callback_1(signal, fragPtrP, *(PagePtr*)&page, 
					sz);
  }
}

void
Dbtup::disk_page_abort_prealloc_callback(Signal* signal, 
					 Uint32 sz, Uint32 page_id)
{
  //ndbout_c("disk_alloc_page_callback id: %d", page_id);
  
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  
  PagePtr pagePtr= *(PagePtr*)&gpage;
  
  Ptr<Tablerec> tabPtr;
  tabPtr.i= pagePtr.p->m_table_id;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  Ptr<Fragrecord> fragPtr;
  getFragmentrec(fragPtr, pagePtr.p->m_fragment_id, tabPtr.p);

  disk_page_abort_prealloc_callback_1(signal, fragPtr.p, pagePtr, sz);
}

void
Dbtup::disk_page_abort_prealloc_callback_1(Signal* signal, 
					   Fragrecord* fragPtrP,
					   PagePtr pagePtr,
					   Uint32 sz)
{
  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;
  Uint32 page_idx = pagePtr.p->list_index;
  Uint32 used = pagePtr.p->uncommitted_used_space;
  Uint32 free = pagePtr.p->free_space;
  Uint32 ext = pagePtr.p->m_extent_info_ptr;

  Uint32 old_idx = page_idx & 0x7FFF;
  ndbassert(free >= used);
  ndbassert(used >= sz);
  ndbassert(alloc.calc_page_free_bits(free - used) == old_idx);
  Uint32 new_idx = alloc.calc_page_free_bits(free - used + sz);

  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, ext);
  if (old_idx != new_idx)
  {
    ndbassert(extentPtr.p->m_free_page_count[old_idx]);
    extentPtr.p->m_free_page_count[old_idx]--;
    extentPtr.p->m_free_page_count[new_idx]++;

    if (old_idx == page_idx)
    {
      ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
      LocalDLList<Page> old_list(*pool, alloc.m_dirty_pages[old_idx]);
      LocalDLList<Page> new_list(*pool, alloc.m_dirty_pages[new_idx]);
      old_list.remove(pagePtr);
      new_list.add(pagePtr);
      pagePtr.p->list_index = new_idx;
    }
    else
    {
      pagePtr.p->list_index = new_idx | 0x8000;
    }
  }
  
  pagePtr.p->uncommitted_used_space = used - sz;
  extentPtr.p->m_free_space += sz;

  Uint32 old_pos = extentPtr.p->m_free_matrix_pos;
  if (old_pos != RNIL)
  {
    Uint32 pos= alloc.calc_extent_pos(extentPtr.p);
    
    if (pos != old_pos)
    {
      Extent_list old_list(c_extent_pool, alloc.m_free_extents[old_pos]);
      Extent_list new_list(c_extent_pool, alloc.m_free_extents[pos]);
      old_list.remove(extentPtr);
      new_list.add(extentPtr);
      extentPtr.p->m_free_matrix_pos= pos;
    }
  }
}

Uint64
Dbtup::disk_page_undo_alloc(Page* page, const Local_key* key,
			    Uint32 sz, Uint32 gci, Uint32 logfile_group_id)
{
  Logfile_client lsman(this, c_lgman, logfile_group_id);

  Disk_undo::Alloc alloc;
  alloc.m_type_length= (Disk_undo::UNDO_ALLOC << 16) | (sizeof(alloc) >> 2);
  alloc.m_page_no = key->m_page_no;
  alloc.m_file_no_page_idx= key->m_file_no << 16 | key->m_page_idx;
  
  Logfile_client::Change c[1] = {{ &alloc, sizeof(alloc) >> 2 } };
  
  Uint64 lsn= lsman.add_entry(c, 1);
  m_pgman.update_lsn(* key, lsn);

  return lsn;
}

Uint64
Dbtup::disk_page_undo_update(Page* page, const Local_key* key,
			     const Uint32* src, Uint32 sz,
			     Uint32 gci, Uint32 logfile_group_id)
{
  Logfile_client lsman(this, c_lgman, logfile_group_id);

  Disk_undo::Update update;
  update.m_page_no = key->m_page_no;
  update.m_file_no_page_idx= key->m_file_no << 16 | key->m_page_idx;
  update.m_gci= gci;
  
  update.m_type_length= 
    (Disk_undo::UNDO_UPDATE << 16) | (sz + (sizeof(update) >> 2) - 1);

  Logfile_client::Change c[3] = {
    { &update, 3 },
    { src, sz },
    { &update.m_type_length, 1 }
  };

  ndbassert(4*(3 + sz + 1) == (sizeof(update) + 4*sz - 4));
    
  Uint64 lsn= lsman.add_entry(c, 3);
  m_pgman.update_lsn(* key, lsn);

  return lsn;
}
  
Uint64
Dbtup::disk_page_undo_free(Page* page, const Local_key* key,
			   const Uint32* src, Uint32 sz,
			   Uint32 gci, Uint32 logfile_group_id)
{
  Logfile_client lsman(this, c_lgman, logfile_group_id);

  Disk_undo::Free free;
  free.m_page_no = key->m_page_no;
  free.m_file_no_page_idx= key->m_file_no << 16 | key->m_page_idx;
  free.m_gci= gci;
  
  free.m_type_length= 
    (Disk_undo::UNDO_FREE << 16) | (sz + (sizeof(free) >> 2) - 1);
  
  Logfile_client::Change c[3] = {
    { &free, 3 },
    { src, sz },
    { &free.m_type_length, 1 }
  };
  
  ndbassert(4*(3 + sz + 1) == (sizeof(free) + 4*sz - 4));
  
  Uint64 lsn= lsman.add_entry(c, 3);
  m_pgman.update_lsn(* key, lsn);

  return lsn;
}
  
int
Dbtup::disk_restart_alloc_extent(Uint32 tableId, Uint32 fragId, 
				 const Local_key* key, Uint32 pages)
{
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  if (tabPtr.p->tableStatus == DEFINED)
  {
    getFragmentrec(fragPtr, fragId, tabPtr.p);
    if (!fragPtr.isNull())
    {
      Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
      
      Ptr<Extent_info> ext;
      ndbrequire(c_extent_pool.seize(ext));
      
      ext.p->m_key = *key;
      ndbout << "allocated " << pages << " pages: " << ext.p->m_key << endl;
      ext.p->m_first_page_no = ext.p->m_key.m_page_no;
      bzero(ext.p->m_free_page_count, sizeof(ext.p->m_free_page_count));
      ext.p->m_free_space= alloc.m_page_free_bits_map[0] * pages; 
      ext.p->m_free_page_count[0]= pages; // All pages are "free"-est
      
      if (alloc.m_curr_extent_info_ptr_i != RNIL)
      {
	Ptr<Extent_info> old;
	c_extent_pool.getPtr(old, alloc.m_curr_extent_info_ptr_i);
	ndbassert(old.p->m_free_matrix_pos == RNIL);
	Uint32 pos= alloc.calc_extent_pos(old.p);
	Extent_list new_list(c_extent_pool, alloc.m_free_extents[pos]);
	new_list.add(old);
	old.p->m_free_matrix_pos= pos;
      }
      
      alloc.m_curr_extent_info_ptr_i = ext.i;
      ext.p->m_free_matrix_pos = RNIL;
      c_extent_hash.add(ext);

      LocalSLList<Extent_info, Extent_list_t> 
	list1(c_extent_pool, alloc.m_extent_list);
      list1.add(ext);
      return 0;
    }
  }

  return -1;
}

void
Dbtup::disk_restart_page_bits(Uint32 tableId, Uint32 fragId,
			      const Local_key*, Uint32 old_bits, Uint32 bits)
{
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  getFragmentrec(fragPtr, fragId, tabPtr.p);
  Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
  
  Ptr<Extent_info> ext;
  c_extent_pool.getPtr(ext, alloc.m_curr_extent_info_ptr_i);
  
  Uint32 size= alloc.calc_page_free_space(bits);  
  Uint32 old_size= alloc.calc_page_free_space(old_bits);

  if (bits != old_bits)
  {
    ndbassert(ext.p->m_free_page_count[old_bits] > 0);
    ndbassert(ext.p->m_free_space >= old_size);

    ext.p->m_free_page_count[bits]++;
    ext.p->m_free_page_count[old_bits]--;
    
    ext.p->m_free_space += size;
    ext.p->m_free_space -= old_size;

    Uint32 old_pos = ext.p->m_free_matrix_pos;
    if (old_pos != RNIL)
    {
      Uint32 pos= alloc.calc_extent_pos(ext.p);

      if (pos != old_pos)
      {
	Extent_list old_list(c_extent_pool, alloc.m_free_extents[old_pos]);
	Extent_list new_list(c_extent_pool, alloc.m_free_extents[pos]);
	old_list.remove(ext);
	new_list.add(ext);
	ext.p->m_free_matrix_pos= pos;
      }
    }
  }
}

#include <signaldata/LgmanContinueB.hpp>

static Dbtup::Apply_undo f_undo;

void
Dbtup::disk_restart_undo(Signal* signal, Uint64 lsn,
			 Uint32 type, const Uint32 * ptr, Uint32 len)
{
  f_undo.m_lsn= lsn;
  f_undo.m_ptr= ptr;
  f_undo.m_len= len;
  f_undo.m_type = type;

  Page_cache_client::Request preq;
  switch(f_undo.m_type){
  case File_formats::Undofile::UNDO_LCP_FIRST:
  case File_formats::Undofile::UNDO_LCP:
  {
    ndbrequire(len == 3);
    Uint32 tableId = ptr[1] >> 16;
    Uint32 fragId = ptr[1] & 0xFFFF;
    disk_restart_undo_lcp(tableId, fragId);
    disk_restart_undo_next(signal);
    return;
  }
  case File_formats::Undofile::UNDO_TUP_ALLOC:
  {
    Disk_undo::Alloc* rec= (Disk_undo::Alloc*)ptr;
    preq.m_page.m_page_no = rec->m_page_no;
    preq.m_page.m_file_no  = rec->m_file_no_page_idx >> 16;
    preq.m_page.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_UPDATE:
  {
    Disk_undo::Update* rec= (Disk_undo::Update*)ptr;
    preq.m_page.m_page_no = rec->m_page_no;
    preq.m_page.m_file_no  = rec->m_file_no_page_idx >> 16;
    preq.m_page.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_FREE:
  {
    Disk_undo::Free* rec= (Disk_undo::Free*)ptr;
    preq.m_page.m_page_no = rec->m_page_no;
    preq.m_page.m_file_no  = rec->m_file_no_page_idx >> 16;
    preq.m_page.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_CREATE:
    /**
     * 
     */
  {
    Disk_undo::Create* rec= (Disk_undo::Create*)ptr;
    Ptr<Tablerec> tabPtr;
    tabPtr.i= rec->m_table;
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
    for(Uint32 i = 0; i<MAX_FRAG_PER_NODE; i++)
      if (tabPtr.p->fragrec[i] != RNIL)
	disk_restart_undo_lcp(tabPtr.i, tabPtr.p->fragid[i]);
    disk_restart_undo_next(signal);
    return;
  }
  default:
    ndbrequire(false);
  }

  f_undo.m_key = preq.m_page;
  preq.m_callback.m_callbackFunction = 
    safe_cast(&Dbtup::disk_restart_undo_callback);
  
  int flags = Page_cache_client::NO_HOOK;
  int res= m_pgman.get_page(signal, preq, flags);
  switch(res)
  {
  case 0:
    break; // Wait for callback
  case -1:
    ndbrequire(false);
    break;
  default:
    execute(signal, preq.m_callback, res); // run callback
  }
}

void
Dbtup::disk_restart_undo_next(Signal* signal)
{
  signal->theData[0] = LgmanContinueB::EXECUTE_UNDO_RECORD;
  sendSignal(LGMAN_REF, GSN_CONTINUEB, signal, 1, JBB);
}

void
Dbtup::disk_restart_undo_lcp(Uint32 tableId, Uint32 fragId)
{
  Ptr<Tablerec> tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  if (tabPtr.p->tableStatus == DEFINED)
  {
    FragrecordPtr fragPtr;
    getFragmentrec(fragPtr, fragId, tabPtr.p);
    if (!fragPtr.isNull())
    {
      fragPtr.p->m_undo_complete = true;
    }
  }
}

void
Dbtup::disk_restart_undo_callback(Signal* signal,
				  Uint32 id, 
				  Uint32 page_id)
{
  jamEntry();
  Ptr<GlobalPage> page;
  m_global_page_pool.getPtr(page, page_id);

  Page* pageP = (Page*)page.p;

  bool update = false;
  if (! (pageP->list_index & 0x8000) ||
     pageP->nextList != RNIL ||
     pageP->prevList != RNIL)
  {
    update = true;
    pageP->list_index |= 0x8000;
    pageP->nextList = pageP->prevList = RNIL;
  }

  Ptr<Tablerec> tabPtr;
  tabPtr.i= pageP->m_table_id;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  if (tabPtr.p->tableStatus != DEFINED)
  {
    disk_restart_undo_next(signal);
    return;
  }

  Ptr<Fragrecord> fragPtr;
  getFragmentrec(fragPtr, pageP->m_fragment_id, tabPtr.p);
  if(fragPtr.isNull())
  {
    disk_restart_undo_next(signal);
    return;
  }
    
  Local_key key;
  key.m_page_no = pageP->m_page_no;
  key.m_file_no = pageP->m_file_no;

  if (pageP->m_restart_seq != globalData.m_restart_seq)
  {
    {
      Extent_info key;
      key.m_key.m_file_no = pageP->m_file_no;
      key.m_key.m_page_idx = pageP->m_extent_no;
      Ptr<Extent_info> extentPtr;
      if (c_extent_hash.find(extentPtr, key))
      {
	pageP->m_extent_info_ptr = extentPtr.i;
      }
      else
      {
	/**
	 * Extent was not allocated at start of LCP
	 *  (or was freed during)
	 * I.e page does not need to be undoed as it's
	 *   really free
	 */
	disk_restart_undo_next(signal);
	return;
      }
    }
    
    update= true;
    pageP->m_restart_seq = globalData.m_restart_seq;
    pageP->uncommitted_used_space = 0;
    
    Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
    Uint32 idx= alloc.calc_page_free_bits(pageP->free_space);
    
    pageP->list_index = idx | 0x8000;    
    
  }
  
  Uint64 lsn = 0;
  lsn += pageP->m_page_header.m_page_lsn_hi; lsn <<= 32;
  lsn += pageP->m_page_header.m_page_lsn_lo;
  
  if (f_undo.m_lsn <= lsn)
  {
    Uint32 tableId= pageP->m_table_id;
    Uint32 fragId = pageP->m_fragment_id;
    
    f_undo.m_table_ptr.i= tableId;
    if (tableId < cnoOfTablerec)
    {
      ptrCheckGuard(f_undo.m_table_ptr, cnoOfTablerec, tablerec);

      if (f_undo.m_table_ptr.p->tableStatus == DEFINED)
      {
	getFragmentrec(f_undo.m_fragment_ptr, fragId, f_undo.m_table_ptr.p);
	if (!f_undo.m_fragment_ptr.isNull())
	{
	  if (!f_undo.m_fragment_ptr.p->m_undo_complete)
	  {
	    f_undo.m_page_ptr.i = page_id;
	    f_undo.m_page_ptr.p = pageP;
	    
	    update = true;
	    ndbout_c("applying %lld", f_undo.m_lsn);
	    /**
	     * Apply undo record
	     */
	    switch(f_undo.m_type){
	    case File_formats::Undofile::UNDO_TUP_ALLOC:
	      disk_restart_undo_alloc(&f_undo);
	      break;
	    case File_formats::Undofile::UNDO_TUP_UPDATE:
	      disk_restart_undo_update(&f_undo);
	      break;
	    case File_formats::Undofile::UNDO_TUP_FREE:
	      disk_restart_undo_free(&f_undo);
	      break;
	    default:
	      ndbrequire(false);
	    }
	    
	    disk_restart_undo_page_bits(&f_undo);
	    
	    lsn = f_undo.m_lsn - 1; // make sure undo isn't run again...
	  }
	  else
	  {
	    ndbout_c("lsn %lld frag undo complete", f_undo.m_lsn);
	  }
	}
	else
	{
	  ndbout_c("lsn %lld table not defined", f_undo.m_lsn);
	}
      }
      else
      {
	ndbout_c("lsn %lld no such table", f_undo.m_lsn);
      }
    }
    else
    {
      ndbout_c("f_undo.m_lsn %lld > lsn %lld -> skip",
	       f_undo.m_lsn, lsn);
    }
    
    if (update)
    {
      m_pgman.update_lsn(f_undo.m_key, lsn);
    }
  }

  disk_restart_undo_next(signal);
}

void
Dbtup::disk_restart_undo_alloc(Apply_undo* undo)
{
  ndbassert(undo->m_page_ptr.p->m_file_no == undo->m_key.m_file_no);
  ndbassert(undo->m_page_ptr.p->m_page_no == undo->m_key.m_page_no);
  if (undo->m_table_ptr.p->m_attributes[DD].m_no_of_varsize == 0)
  {
    ((Fix_page*)undo->m_page_ptr.p)->free_record(undo->m_key.m_page_idx);
  }
  else
    ((Var_page*)undo->m_page_ptr.p)->free_record(undo->m_key.m_page_idx, 0);
}

void
Dbtup::disk_restart_undo_update(Apply_undo* undo)
{
  Uint32* ptr;
  Uint32 len= undo->m_len - 4;
  if (undo->m_table_ptr.p->m_attributes[DD].m_no_of_varsize == 0)
  {
    ptr= ((Fix_page*)undo->m_page_ptr.p)->get_ptr(undo->m_key.m_page_idx, len);
    ndbrequire(len == undo->m_table_ptr.p->m_offsets[DD].m_fix_header_size);
  }
  else
  {
    ptr= ((Var_page*)undo->m_page_ptr.p)->get_ptr(undo->m_key.m_page_idx);
    abort();
  }  
  
  const Disk_undo::Update *update = (const Disk_undo::Update*)undo->m_ptr;
  const Uint32* src= update->m_data;
  memcpy(ptr, src, 4 * len);
}

void
Dbtup::disk_restart_undo_free(Apply_undo* undo)
{
  Uint32* ptr, idx = undo->m_key.m_page_idx;
  Uint32 len= undo->m_len - 4;
  if (undo->m_table_ptr.p->m_attributes[DD].m_no_of_varsize == 0)
  {
    ndbrequire(len == undo->m_table_ptr.p->m_offsets[DD].m_fix_header_size);
    idx= ((Fix_page*)undo->m_page_ptr.p)->alloc_record(idx);
    ptr= ((Fix_page*)undo->m_page_ptr.p)->get_ptr(idx, len);
  }
  else
  {
    abort();
  }  
  
  ndbrequire(idx == undo->m_key.m_page_idx);
  const Disk_undo::Free *free = (const Disk_undo::Free*)undo->m_ptr;
  const Uint32* src= free->m_data;
  memcpy(ptr, src, 4 * len);
}

void
Dbtup::disk_restart_undo_page_bits(Apply_undo* undo)
{
  Fragrecord* fragPtrP = undo->m_fragment_ptr.p;
  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;
  
  /**
   * Set alloc.m_curr_extent_info_ptr_i to
   *   current this extent (and move old extend into free matrix)
   */
  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, undo->m_page_ptr.p->m_extent_info_ptr);
  
  Uint32 currExtI = alloc.m_curr_extent_info_ptr_i;
  if (extentPtr.i != currExtI && currExtI != RNIL)
  {
    Ptr<Extent_info> currExtPtr;
    c_extent_pool.getPtr(currExtPtr, currExtI);
    ndbrequire(currExtPtr.p->m_free_matrix_pos == RNIL);
    
    Uint32 pos= alloc.calc_extent_pos(currExtPtr.p);
    Extent_list new_list(c_extent_pool, alloc.m_free_extents[pos]);
    new_list.add(currExtPtr);
    currExtPtr.p->m_free_matrix_pos= pos;
    //ndbout_c("moving extent from %d to %d", old_pos, new_pos);
  }
  
  if (extentPtr.i != currExtI)
  {
    Uint32 old_pos = extentPtr.p->m_free_matrix_pos;
    Extent_list old_list(c_extent_pool, alloc.m_free_extents[old_pos]);
    old_list.remove(extentPtr);
    alloc.m_curr_extent_info_ptr_i = extentPtr.i;
    extentPtr.p->m_free_matrix_pos = RNIL;
  }
  else
  {
    ndbrequire(extentPtr.p->m_free_matrix_pos == RNIL);
  }

  /**
   * Compute and update free bits for this page
   */
  Uint32 free = undo->m_page_ptr.p->free_space;
  Uint32 bits = alloc.calc_page_free_bits(free);
  
  Tablespace_client tsman(0, c_tsman,
			  fragPtrP->fragTableId,
			  fragPtrP->fragmentId,
			  fragPtrP->m_tablespace_id);

  tsman.restart_undo_page_free_bits(&undo->m_key, bits, undo->m_lsn);
}
