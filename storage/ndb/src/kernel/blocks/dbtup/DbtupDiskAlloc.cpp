/*
   Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBTUP_C
#define DBTUP_DISK_ALLOC_CPP
#include "Dbtup.hpp"

static
NdbOut&
operator<<(NdbOut& out, const Ptr<Dbtup::Page> & ptr)
{
  out << "[ Page: ptr.i: " << ptr.i 
      << " [ m_file_no: " << ptr.p->m_file_no
      << " m_page_no: " << ptr.p->m_page_no << "]"
      << " list_index: " << ptr.p->list_index 
      << " free_space: " << ptr.p->free_space
      << " uncommitted_used_space: " << ptr.p->uncommitted_used_space
      << " ]";
  return out;
}

static
NdbOut&
operator<<(NdbOut& out, const Ptr<Dbtup::Page_request> & ptr)
{
  out << "[ Page_request: ptr.i: " << ptr.i
      << " " << ptr.p->m_key
      << " m_original_estimated_free_space: " << ptr.p->m_original_estimated_free_space
      << " m_list_index: " << ptr.p->m_list_index
      << " m_frag_ptr_i: " << ptr.p->m_frag_ptr_i
      << " m_extent_info_ptr: " << ptr.p->m_extent_info_ptr
      << " m_ref_count: " << ptr.p->m_ref_count
      << " m_uncommitted_used_space: " << ptr.p->m_uncommitted_used_space
      << " ]";
  
  return out;
}

static
NdbOut&
operator<<(NdbOut& out, const Ptr<Dbtup::Extent_info> & ptr)
{
  out << "[ Extent_info: ptr.i " << ptr.i
      << " " << ptr.p->m_key
      << " m_first_page_no: " << ptr.p->m_first_page_no
      << " m_free_space: " << ptr.p->m_free_space
      << " m_free_matrix_pos: " << ptr.p->m_free_matrix_pos
      << " m_free_page_count: [";

  for(Uint32 i = 0; i<Dbtup::EXTENT_SEARCH_MATRIX_COLS; i++)
    out << " " << ptr.p->m_free_page_count[i];
  out << " ] ]";

  return out;
}

#if NOT_YET_FREE_EXTENT
static
inline
bool
check_free(const Dbtup::Extent_info* extP)
{
  Uint32 res = 0;
  for (Uint32 i = 1; i<MAX_FREE_LIST; i++)
    res += extP->m_free_page_count[i];
  return res;
}
#error "Code for deallocting extents when they get empty"
#error "This code is not yet complete"
#endif

#if NOT_YET_UNDO_ALLOC_EXTENT
#error "This is needed for deallocting extents when they get empty"
#error "This code is not complete yet"
#endif

void 
Dbtup::dump_disk_alloc(Dbtup::Disk_alloc_info & alloc)
{
  const Uint32 limit = 512;
  ndbout_c("dirty pages");
  for(Uint32 i = 0; i<MAX_FREE_LIST; i++)
  {
    printf("  %d : ", i);
    PagePtr ptr;
    ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
    LocalDLList<Page> list(*pool, alloc.m_dirty_pages[i]);
    Uint32 c = 0;
    for (list.first(ptr); c < limit && !ptr.isNull(); c++, list.next(ptr))
    {
      ndbout << ptr << " ";
    }
    if (c == limit)
    {
      ndbout << "MAXLIMIT ";
    }
    ndbout_c(" ");
  }
  ndbout_c("page requests");
  for(Uint32 i = 0; i<MAX_FREE_LIST; i++)
  {
    printf("  %d : ", i);
    Ptr<Page_request> ptr;
    Local_page_request_list list(c_page_request_pool, 
				 alloc.m_page_requests[i]);
    Uint32 c = 0;
    for (list.first(ptr); c < limit && !ptr.isNull(); c++, list.next(ptr))
    {
      ndbout << ptr << " ";
    }
    if (c == limit)
    {
      ndbout << "MAXLIMIT ";
    }
    ndbout_c(" ");
  }

  ndbout_c("Extent matrix");
  for(Uint32 i = 0; i<alloc.SZ; i++)
  {
    printf("  %d : ", i);
    Ptr<Extent_info> ptr;
    Local_extent_info_list list(c_extent_pool, alloc.m_free_extents[i]);
    Uint32 c = 0;
    for (list.first(ptr); c < limit && !ptr.isNull(); c++, list.next(ptr))
    {
      ndbout << ptr << " ";
    }
    if (c == limit)
    {
      ndbout << "MAXLIMIT ";
    }
    ndbout_c(" ");
  }

  if (alloc.m_curr_extent_info_ptr_i != RNIL)
  {
    Ptr<Extent_info> ptr;
    c_extent_pool.getPtr(ptr, alloc.m_curr_extent_info_ptr_i);
    ndbout << "current extent: " << ptr << endl;
  }
}

#if defined VM_TRACE || 1
#define ddassert(x) do { if(unlikely(!(x))) { dump_disk_alloc(alloc); ndbrequire(false); } } while(0)
#else
#define ddassert(x)
#endif

Dbtup::Disk_alloc_info::Disk_alloc_info(const Tablerec* tabPtrP, 
					Uint32 extent_size)
{
  m_extent_size = extent_size;
  m_curr_extent_info_ptr_i = RNIL; 
  if (tabPtrP->m_no_of_disk_attributes == 0)
    return;
  
  Uint32 min_size= 4*tabPtrP->m_offsets[DD].m_fix_header_size;
  
  if (tabPtrP->m_attributes[DD].m_no_of_varsize == 0)
  {
    Uint32 recs_per_page= (4*Tup_fixsize_page::DATA_WORDS)/min_size;
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
  Uint32 col = calc_page_free_bits(sz);
  Uint32 mask= EXTENT_SEARCH_MATRIX_COLS - 1;
  for(Uint32 i= 0; i<EXTENT_SEARCH_MATRIX_SIZE; i++)
  {
    // Check that it can cater for request
    if (!m_free_extents[i].isEmpty())
    {
      return i;
    }
    
    if ((i & mask) >= col)
    {
      i = (i & ~mask) + mask;
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
  
  assert(pos < EXTENT_SEARCH_MATRIX_SIZE);
  return pos;
}

void
Dbtup::update_extent_pos(Disk_alloc_info& alloc, 
                         Ptr<Extent_info> extentPtr,
                         Int32 delta)
{
  if (delta < 0)
  {
    jam();
    Uint32 sub = Uint32(- delta);
    ddassert(extentPtr.p->m_free_space >= sub);
    extentPtr.p->m_free_space -= sub;
  }
  else
  {
    jam();
    extentPtr.p->m_free_space += delta;
    ndbassert(Uint32(delta) <= alloc.calc_page_free_space(0));
  }

#ifdef VM_TRACE
  Uint32 cnt = 0;
  Uint32 sum = 0;
  for(Uint32 i = 0; i<MAX_FREE_LIST; i++)
  {
    cnt += extentPtr.p->m_free_page_count[i];
    sum += extentPtr.p->m_free_page_count[i] * alloc.calc_page_free_space(i);
  }
  if (extentPtr.p->m_free_page_count[0] == cnt)
  {
    ddassert(extentPtr.p->m_free_space == cnt*alloc.m_page_free_bits_map[0]);
  }
  else
  {
    ddassert(extentPtr.p->m_free_space < cnt*alloc.m_page_free_bits_map[0]);
  }
  ddassert(extentPtr.p->m_free_space >= sum);
  ddassert(extentPtr.p->m_free_space <= cnt*alloc.m_page_free_bits_map[0]);
#endif
  
  Uint32 old = extentPtr.p->m_free_matrix_pos;
  if (old != RNIL)
  {
    Uint32 pos = alloc.calc_extent_pos(extentPtr.p);
    if (old != pos)
    {
      jam();
      Local_extent_info_list old_list(c_extent_pool, alloc.m_free_extents[old]);
      Local_extent_info_list new_list(c_extent_pool, alloc.m_free_extents[pos]);
      old_list.remove(extentPtr);
      new_list.add(extentPtr);
      extentPtr.p->m_free_matrix_pos= pos;
    }
  }
  else
  {
    ddassert(alloc.m_curr_extent_info_ptr_i == extentPtr.i);
  }
}

void
Dbtup::restart_setup_page(Disk_alloc_info& alloc, PagePtr pagePtr,
                          Int32 estimate)
{
  jam();
  /**
   * Link to extent, clear uncommitted_used_space
   */
  pagePtr.p->uncommitted_used_space = 0;
  pagePtr.p->m_restart_seq = globalData.m_restart_seq;
  
  Extent_info key;
  key.m_key.m_file_no = pagePtr.p->m_file_no;
  key.m_key.m_page_idx = pagePtr.p->m_extent_no;
  Ptr<Extent_info> extentPtr;
  ndbrequire(c_extent_hash.find(extentPtr, key));
  pagePtr.p->m_extent_info_ptr = extentPtr.i;

  Uint32 real_free = pagePtr.p->free_space;
  const bool prealloc = estimate >= 0;
  Uint32 estimated;
  if (prealloc)
  {
    jam();
    /**
     * If this is during prealloc, use estimate from there
     */
    estimated = (Uint32)estimate;
  }
  else
  {
    jam();
    /**
     * else use the estimate based on the actual free space
     */
    estimated =alloc.calc_page_free_space(alloc.calc_page_free_bits(real_free));
  }

#ifdef VM_TRACE
  {
    Local_key page;
    page.m_file_no = pagePtr.p->m_file_no;
    page.m_page_no = pagePtr.p->m_page_no;

    D("Tablespace_client - restart_setup_page");
    Tablespace_client tsman(0, this, c_tsman,
			    0, 0, 0);
    unsigned uncommitted, committed;
    uncommitted = committed = ~(unsigned)0;
    (void) tsman.get_page_free_bits(&page, &uncommitted, &committed);
    jamEntry();
    
    ddassert(alloc.calc_page_free_bits(real_free) == committed);
    if (prealloc)
    {
      /**
       * tsman.alloc_page sets the uncommitted-bits to MAX_FREE_LIST -1
       *   to avoid page being preallocated several times
       */
      ddassert(uncommitted == MAX_FREE_LIST - 1);
    }
    else
    {
      ddassert(committed == uncommitted);
    }
  }
#endif

  ddassert(real_free >= estimated);

  if (real_free != estimated)
  {
    jam();
    Uint32 delta = (real_free-estimated);
    update_extent_pos(alloc, extentPtr, delta);
  }
}

/**
 * - Page free bits -
 * 0 = 00 - free - 100% free
 * 1 = 01 - atleast 70% free, 70= pct_free + 2 * (100 - pct_free) / 3
 * 2 = 10 - atleast 40% free, 40= pct_free + (100 - pct_free) / 3
 * 3 = 11 - full - less than pct_free% free, pct_free=10%
 *
 */

#define DBG_DISK 0

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
  D("Tablespace_client - disk_page_prealloc");
  Tablespace_client tsman(signal, this, c_tsman,
			  fragPtrP->fragTableId,
			  fragPtrP->fragmentId,
			  fragPtrP->m_tablespace_id);
  
  if (DBG_DISK)
    ndbout << "disk_page_prealloc";

  /**
   * 1) search current dirty pages
   */
  for(i= 0; i <= idx; i++)
  {
    if (!alloc.m_dirty_pages[i].isEmpty())
    {
      ptrI= alloc.m_dirty_pages[i].firstItem;
      Ptr<GlobalPage> gpage;
      m_global_page_pool.getPtr(gpage, ptrI);
      
      PagePtr tmp;
      tmp.i = gpage.i;
      tmp.p = reinterpret_cast<Page*>(gpage.p);
      disk_page_prealloc_dirty_page(alloc, tmp, i, sz);
      key->m_page_no= tmp.p->m_page_no;
      key->m_file_no= tmp.p->m_file_no;
      if (DBG_DISK)
	ndbout << " found dirty page " << *key << endl;
      jam();
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
      if (DBG_DISK)
	ndbout << " found transit page " << *key << endl;
      jam();
      return 0;
    }
  }
  
  /**
   * We need to request a page...
   */
  if (!c_page_request_pool.seize(req))
  {
    jam();
    err= 1;
    //XXX set error code
    ndbout_c("no free request");
    return -err;
  }

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
      jamEntry();
      found= true;
    }
    else
    {
      jamEntry();
      /**
       * The current extent is not in a free list
       *   and since it couldn't accomadate the request
       *   we put it on the free list
       */
      alloc.m_curr_extent_info_ptr_i = RNIL;
      Uint32 pos= alloc.calc_extent_pos(ext.p);
      ext.p->m_free_matrix_pos = pos;
      Local_extent_info_list list(c_extent_pool, alloc.m_free_extents[pos]);
      list.add(ext);
    }
  }
  
  if (!found)
  {
    Uint32 pos;
    if ((pos= alloc.find_extent(sz)) != RNIL)
    {
      jam();
      Local_extent_info_list list(c_extent_pool, alloc.m_free_extents[pos]);
      list.first(ext);
      list.remove(ext);
    }
    else 
    {
      jam();
      /**
       * We need to alloc an extent
       */
#if NOT_YET_UNDO_ALLOC_EXTENT
      Uint32 logfile_group_id = fragPtr.p->m_logfile_group_id;

      err = c_lgman->alloc_log_space(logfile_group_id,
				     sizeof(Disk_undo::AllocExtent)>>2);
      jamEntry();
      if(unlikely(err))
      {
	return -err;
      }
#endif

      if (!c_extent_pool.seize(ext))
      {
	jam();
	//XXX
	err= 2;
#if NOT_YET_UNDO_ALLOC_EXTENT
	c_lgman->free_log_space(logfile_group_id, 
				sizeof(Disk_undo::AllocExtent)>>2);
#endif
	c_page_request_pool.release(req);
	ndbout_c("no free extent info");
	return -err;
      }
      
      if ((err= tsman.alloc_extent(&ext.p->m_key)) < 0)
      {
	jamEntry();
#if NOT_YET_UNDO_ALLOC_EXTENT
	c_lgman->free_log_space(logfile_group_id, 
				sizeof(Disk_undo::AllocExtent)>>2);
#endif
	c_extent_pool.release(ext);
	c_page_request_pool.release(req);
	return err;
      }

      int pages= err;
#if NOT_YET_UNDO_ALLOC_EXTENT
      {
	/**
	 * Do something here
	 */
	{
	  Callback cb;
	  cb.m_callbackData= ext.i;
	  cb.m_callbackFunction = 
	    safe_cast(&Dbtup::disk_page_alloc_extent_log_buffer_callback);
	  Uint32 sz= sizeof(Disk_undo::AllocExtent)>>2;
	  
	  Logfile_client lgman(this, c_lgman, logfile_group_id);
	  int res= lgman.get_log_buffer(signal, sz, &cb);
          jamEntry();
	  switch(res){
	  case 0:
	    break;
	  case -1:
	    ndbrequire("NOT YET IMPLEMENTED" == 0);
	    break;
	  default:
	    execute(signal, cb, res);	    
	  }
	}
      }
#endif
      
#ifdef VM_TRACE
      ndbout << "allocated " << pages << " pages: " << ext.p->m_key 
	     << " table: " << fragPtr.p->fragTableId 
	     << " fragment: " << fragPtr.p->fragmentId << endl;
#endif
      ext.p->m_first_page_no = ext.p->m_key.m_page_no;
      memset(ext.p->m_free_page_count, 0, sizeof(ext.p->m_free_page_count));
      ext.p->m_free_space= alloc.m_page_free_bits_map[0] * pages; 
      ext.p->m_free_page_count[0]= pages; // All pages are "free"-est
      ext.p->m_empty_page_no = 0;
      c_extent_hash.add(ext);

      Local_fragment_extent_list list1(c_extent_pool, alloc.m_extent_list);
      list1.add(ext);
    }      
    
    alloc.m_curr_extent_info_ptr_i= ext.i;
    ext.p->m_free_matrix_pos= RNIL;
    pageBits= tsman.alloc_page_from_extent(&ext.p->m_key, bits);
    jamEntry();
    ddassert(pageBits >= 0);
  }
  
  /**
   * We have a page from an extent
   */
  *key= req.p->m_key= ext.p->m_key;

  if (DBG_DISK)
    ndbout << " allocated page " << *key << endl;
  
  /**
   * We don't know exact free space of page
   *   but we know what page free bits it has.
   *   compute free space based on them
   */
  Uint32 size= alloc.calc_page_free_space((Uint32)pageBits);
  
  ddassert(size >= sz);
  req.p->m_original_estimated_free_space = size;

  Uint32 new_size = size - sz;   // Subtract alloc rec
  Uint32 newPageBits= alloc.calc_page_free_bits(new_size);
  if (newPageBits != (Uint32)pageBits)
  {
    jam();
    ddassert(ext.p->m_free_page_count[pageBits] > 0);
    ext.p->m_free_page_count[pageBits]--;
    ext.p->m_free_page_count[newPageBits]++;
  }
  update_extent_pos(alloc, ext, -Int32(sz));

  // And put page request in correct free list
  idx= alloc.calc_page_free_bits(new_size);
  {
    Local_page_request_list list(c_page_request_pool, 
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
    jam();

    if (ext.p->m_first_page_no + ext.p->m_empty_page_no == key->m_page_no)
    {
      jam();
      flags |= Page_cache_client::EMPTY_PAGE;
      //ndbout << "EMPTY_PAGE " << ext.p->m_empty_page_no << " " << *key << endl;
      ext.p->m_empty_page_no++;
    }

    preq.m_callback.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_prealloc_initial_callback);
  }
  
  Page_cache_client pgman(this, c_pgman);
  int res= pgman.get_page(signal, preq, flags);
  m_pgman_ptr = pgman.m_ptr;
  jamEntry();
  switch(res)
  {
  case 0:
    jam();
    break;
  case -1:
    ndbassert(false);
    break;
  default:
    jam();
    execute(signal, preq.m_callback, res); // run callback
  }
  
  return res;
}

void
Dbtup::disk_page_prealloc_dirty_page(Disk_alloc_info & alloc,
				     PagePtr pagePtr, 
				     Uint32 old_idx, Uint32 sz)
{
  jam();
  ddassert(pagePtr.p->list_index == old_idx);

  Uint32 free= pagePtr.p->free_space;
  Uint32 used= pagePtr.p->uncommitted_used_space + sz;
  Uint32 ext= pagePtr.p->m_extent_info_ptr;
  
  ddassert(free >= used);
  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, ext);

  Uint32 new_idx= alloc.calc_page_free_bits(free - used);

  if (old_idx != new_idx)
  {
    jam();
    disk_page_move_dirty_page(alloc, extentPtr, pagePtr, old_idx, new_idx);
  }

  pagePtr.p->uncommitted_used_space = used;
  update_extent_pos(alloc, extentPtr, -Int32(sz));
}


void
Dbtup::disk_page_prealloc_transit_page(Disk_alloc_info& alloc,
				       Ptr<Page_request> req, 
				       Uint32 old_idx, Uint32 sz)
{
  jam();
  ddassert(req.p->m_list_index == old_idx);

  Uint32 free= req.p->m_original_estimated_free_space;
  Uint32 used= req.p->m_uncommitted_used_space + sz;
  Uint32 ext= req.p->m_extent_info_ptr;
  
  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, ext);

  ddassert(free >= used);
  Uint32 new_idx= alloc.calc_page_free_bits(free - used);
  
  if (old_idx != new_idx)
  {
    jam();
    disk_page_move_page_request(alloc, extentPtr, req, old_idx, new_idx);
  }

  req.p->m_uncommitted_used_space = used;
  update_extent_pos(alloc, extentPtr, -Int32(sz));
}

void
Dbtup::disk_page_prealloc_callback(Signal* signal, 
				   Uint32 page_request, Uint32 page_id)
{
  jamEntry();
  //ndbout_c("disk_alloc_page_callback id: %d", page_id);

  Ptr<Page_request> req;
  c_page_request_pool.getPtr(req, page_request);

  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);

  Ptr<Fragrecord> fragPtr;
  fragPtr.i= req.p->m_frag_ptr_i;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  PagePtr pagePtr;
  pagePtr.i = gpage.i;
  pagePtr.p = reinterpret_cast<Page*>(gpage.p);

  Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
  if (unlikely(pagePtr.p->m_restart_seq != globalData.m_restart_seq))
  {
    jam();
    D(V(pagePtr.p->m_restart_seq) << V(globalData.m_restart_seq));
    restart_setup_page(alloc, pagePtr, req.p->m_original_estimated_free_space);
  }

  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, req.p->m_extent_info_ptr);

  pagePtr.p->uncommitted_used_space += req.p->m_uncommitted_used_space;
  ddassert(pagePtr.p->free_space >= pagePtr.p->uncommitted_used_space);

  Uint32 free = pagePtr.p->free_space - pagePtr.p->uncommitted_used_space;
  Uint32 idx = req.p->m_list_index;
  Uint32 real_idx = alloc.calc_page_free_bits(free);

  if (idx != real_idx)
  {
    jam();
    ddassert(extentPtr.p->m_free_page_count[idx]);
    extentPtr.p->m_free_page_count[idx]--;
    extentPtr.p->m_free_page_count[real_idx]++;
    update_extent_pos(alloc, extentPtr, 0);
  }

  {
    /**
     * add to dirty list
     */
    pagePtr.p->list_index = real_idx;
    ArrayPool<Page> *cheat_pool= (ArrayPool<Page>*)&m_global_page_pool;
    LocalDLList<Page> list(* cheat_pool, alloc.m_dirty_pages[real_idx]);
    list.add(pagePtr);
  }

  {
    /**
     * release page request
     */
    Local_page_request_list list(c_page_request_pool,
				 alloc.m_page_requests[idx]);
    list.release(req);
  }
}

void
Dbtup::disk_page_move_dirty_page(Disk_alloc_info& alloc,
                                 Ptr<Extent_info> extentPtr,
                                 Ptr<Page> pagePtr,
                                 Uint32 old_idx,
                                 Uint32 new_idx)
{
  ddassert(extentPtr.p->m_free_page_count[old_idx]);
  extentPtr.p->m_free_page_count[old_idx]--;
  extentPtr.p->m_free_page_count[new_idx]++;

  ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
  LocalDLList<Page> new_list(*pool, alloc.m_dirty_pages[new_idx]);
  LocalDLList<Page> old_list(*pool, alloc.m_dirty_pages[old_idx]);
  old_list.remove(pagePtr);
  new_list.add(pagePtr);
  pagePtr.p->list_index = new_idx;
}

void
Dbtup::disk_page_move_page_request(Disk_alloc_info& alloc,
                                   Ptr<Extent_info> extentPtr,
                                   Ptr<Page_request> req,
                                   Uint32 old_idx, Uint32 new_idx)
{
  Page_request_list::Head *lists = alloc.m_page_requests;
  Local_page_request_list old_list(c_page_request_pool, lists[old_idx]);
  Local_page_request_list new_list(c_page_request_pool, lists[new_idx]);
  old_list.remove(req);
  new_list.add(req);

  ddassert(extentPtr.p->m_free_page_count[old_idx]);
  extentPtr.p->m_free_page_count[old_idx]--;
  extentPtr.p->m_free_page_count[new_idx]++;
  req.p->m_list_index= new_idx;
}

void
Dbtup::disk_page_prealloc_initial_callback(Signal*signal, 
					   Uint32 page_request, 
					   Uint32 page_id)
{
  jamEntry();
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
  PagePtr pagePtr;
  pagePtr.i = gpage.i;
  pagePtr.p = reinterpret_cast<Page*>(gpage.p);

  Ptr<Fragrecord> fragPtr;
  fragPtr.i= req.p->m_frag_ptr_i;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  Ptr<Tablerec> tabPtr;
  tabPtr.i = fragPtr.p->fragTableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, req.p->m_extent_info_ptr);

  if (tabPtr.p->m_attributes[DD].m_no_of_varsize == 0)
  {
    convertThPage((Fix_page*)pagePtr.p, tabPtr.p, DD);
  }
  else
  {
    abort();
  }

  pagePtr.p->m_page_no= req.p->m_key.m_page_no;
  pagePtr.p->m_file_no= req.p->m_key.m_file_no;
  pagePtr.p->m_table_id= fragPtr.p->fragTableId;
  pagePtr.p->m_fragment_id = fragPtr.p->fragmentId;
  pagePtr.p->m_extent_no = extentPtr.p->m_key.m_page_idx; // logical extent no
  pagePtr.p->m_extent_info_ptr= req.p->m_extent_info_ptr;
  pagePtr.p->m_restart_seq = globalData.m_restart_seq;
  pagePtr.p->nextList = pagePtr.p->prevList = RNIL;
  pagePtr.p->list_index = req.p->m_list_index;
  pagePtr.p->uncommitted_used_space = req.p->m_uncommitted_used_space;

  Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
  Uint32 idx = req.p->m_list_index;

  {
    Uint32 free = pagePtr.p->free_space - pagePtr.p->uncommitted_used_space;
    ddassert(idx == alloc.calc_page_free_bits(free));
    ddassert(pagePtr.p->free_space == req.p->m_original_estimated_free_space);
  }

  {
    /**
     * add to dirty list
     */
    ArrayPool<Page> *cheat_pool= (ArrayPool<Page>*)&m_global_page_pool;
    LocalDLList<Page> list(* cheat_pool, alloc.m_dirty_pages[idx]);
    list.add(pagePtr);
  }

  {
    /**
     * release page request
     */
    Local_page_request_list list(c_page_request_pool, 
				 alloc.m_page_requests[idx]);
    list.release(req);
  }
}

void
Dbtup::disk_page_set_dirty(PagePtr pagePtr)
{
  jam();
  Uint32 idx = pagePtr.p->list_index;
  if ((pagePtr.p->m_restart_seq == globalData.m_restart_seq) &&
      ((idx & 0x8000) == 0))
  {
    jam();
    /**
     * Already in dirty list
     */
    return ;
  }
  
  Local_key key;
  key.m_page_no = pagePtr.p->m_page_no;
  key.m_file_no = pagePtr.p->m_file_no;

  pagePtr.p->nextList = pagePtr.p->prevList = RNIL;

  if (DBG_DISK)
    ndbout << " disk_page_set_dirty " << key << endl;
  
  Ptr<Tablerec> tabPtr;
  tabPtr.i= pagePtr.p->m_table_id;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  Ptr<Fragrecord> fragPtr;
  getFragmentrec(fragPtr, pagePtr.p->m_fragment_id, tabPtr.p);
  
  Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;

  Uint32 free = pagePtr.p->free_space;
  Uint32 used = pagePtr.p->uncommitted_used_space;
  if (unlikely(pagePtr.p->m_restart_seq != globalData.m_restart_seq))
  {
    jam();
    D(V(pagePtr.p->m_restart_seq) << V(globalData.m_restart_seq));
    restart_setup_page(alloc, pagePtr, -1);
    ndbassert(free == pagePtr.p->free_space);
    idx = alloc.calc_page_free_bits(free);
    used = 0;
  }
  else
  {
    jam();
    idx &= ~0x8000;
    ddassert(idx == alloc.calc_page_free_bits(free - used));
  }
  
  ddassert(free >= used);
  
  D("Tablespace_client - disk_page_set_dirty");
  Tablespace_client tsman(0, this, c_tsman,
			  fragPtr.p->fragTableId,
			  fragPtr.p->fragmentId,
			  fragPtr.p->m_tablespace_id);
  
  pagePtr.p->list_index = idx;
  ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
  LocalDLList<Page> list(*pool, alloc.m_dirty_pages[idx]);
  list.add(pagePtr);
  
  // Make sure no one will allocate it...
  tsman.unmap_page(&key, MAX_FREE_LIST - 1);
  jamEntry();
}

void
Dbtup::disk_page_unmap_callback(Uint32 when,
				Uint32 page_id, Uint32 dirty_count)
{
  jamEntry();
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  PagePtr pagePtr;
  pagePtr.i = gpage.i;
  pagePtr.p = reinterpret_cast<Page*>(gpage.p);
  
  Uint32 type = pagePtr.p->m_page_header.m_page_type;
  if (unlikely((type != File_formats::PT_Tup_fixsize_page &&
		type != File_formats::PT_Tup_varsize_page) ||
	       f_undo_done == false))
  {
    jam();
    D("disk_page_unmap_callback" << V(type) << V(f_undo_done));
    return ;
  }

  Uint32 idx = pagePtr.p->list_index;

  Ptr<Tablerec> tabPtr;
  tabPtr.i= pagePtr.p->m_table_id;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  Ptr<Fragrecord> fragPtr;
  getFragmentrec(fragPtr, pagePtr.p->m_fragment_id, tabPtr.p);
  
  Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
  
  if (when == 0)
  {
    /**
     * Before pageout
     */
    jam();

    if (DBG_DISK)
    {
      Local_key key;
      key.m_page_no = pagePtr.p->m_page_no;
      key.m_file_no = pagePtr.p->m_file_no;
      ndbout << "disk_page_unmap_callback(before) " << key 
	     << " cnt: " << dirty_count << " " << (idx & ~0x8000) << endl;
    }

    ndbassert((idx & 0x8000) == 0);

    ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
    LocalDLList<Page> list(*pool, alloc.m_dirty_pages[idx]);
    LocalDLList<Page> list2(*pool, alloc.m_unmap_pages);
    list.remove(pagePtr);
    list2.add(pagePtr);

    if (dirty_count == 0)
    {
      jam();
      pagePtr.p->list_index = idx | 0x8000;      
      
      Local_key key;
      key.m_page_no = pagePtr.p->m_page_no;
      key.m_file_no = pagePtr.p->m_file_no;
      
      Uint32 free = pagePtr.p->free_space;
      Uint32 used = pagePtr.p->uncommitted_used_space;
      ddassert(free >= used);
      ddassert(alloc.calc_page_free_bits(free - used) == idx);
      
      D("Tablespace_client - disk_page_unmap_callback");
      Tablespace_client tsman(0, this, c_tsman,
			      fragPtr.p->fragTableId,
			      fragPtr.p->fragmentId,
			      fragPtr.p->m_tablespace_id);
      
      tsman.unmap_page(&key, idx);
      jamEntry();
    }
  }
  else if (when == 1)
  {
    /**
     * After page out
     */
    jam();

    Local_key key;
    key.m_page_no = pagePtr.p->m_page_no;
    key.m_file_no = pagePtr.p->m_file_no;
    Uint32 real_free = pagePtr.p->free_space;
    
    if (DBG_DISK)
    {
      ndbout << "disk_page_unmap_callback(after) " << key 
	     << " cnt: " << dirty_count << " " << (idx & ~0x8000) << endl;
    }

    ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
    LocalDLList<Page> list(*pool, alloc.m_unmap_pages);
    list.remove(pagePtr);

    D("Tablespace_client - disk_page_unmap_callback");
    Tablespace_client tsman(0, this, c_tsman,
			    fragPtr.p->fragTableId,
			    fragPtr.p->fragmentId,
			    fragPtr.p->m_tablespace_id);
    
    if (DBG_DISK && alloc.calc_page_free_bits(real_free) != (idx & ~0x8000))
    {
      ndbout << key 
	     << " calc: " << alloc.calc_page_free_bits(real_free)
	     << " idx: " << (idx & ~0x8000)
	     << endl;
    }
    tsman.update_page_free_bits(&key, alloc.calc_page_free_bits(real_free));
    jamEntry();
  }
}

void
Dbtup::disk_page_alloc(Signal* signal, 
		       Tablerec* tabPtrP, Fragrecord* fragPtrP, 
		       Local_key* key, PagePtr pagePtr, Uint32 gci)
{
  jam();
  Uint32 logfile_group_id= fragPtrP->m_logfile_group_id;
  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;

  Uint64 lsn;
  if (tabPtrP->m_attributes[DD].m_no_of_varsize == 0)
  {
    ddassert(pagePtr.p->uncommitted_used_space > 0);
    pagePtr.p->uncommitted_used_space--;
    key->m_page_idx= ((Fix_page*)pagePtr.p)->alloc_record();
    lsn= disk_page_undo_alloc(pagePtr.p, key, 1, gci, logfile_group_id);
  }
  else
  {
    Uint32 sz= key->m_page_idx;
    ddassert(pagePtr.p->uncommitted_used_space >= sz);
    pagePtr.p->uncommitted_used_space -= sz;
    key->m_page_idx= ((Var_page*)pagePtr.p)->
      alloc_record(sz, (Var_page*)ctemp_page, 0);
    
    lsn= disk_page_undo_alloc(pagePtr.p, key, sz, gci, logfile_group_id);
  }
}

void
Dbtup::disk_page_free(Signal *signal, 
		      Tablerec *tabPtrP, Fragrecord * fragPtrP,
		      Local_key* key, PagePtr pagePtr, Uint32 gci)
{
  jam();
  if (DBG_DISK)
    ndbout << " disk_page_free " << *key << endl;
  
  Uint32 page_idx= key->m_page_idx;
  Uint32 logfile_group_id= fragPtrP->m_logfile_group_id;
  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;
  Uint32 old_free= pagePtr.p->free_space;

  Uint32 sz;
  Uint64 lsn;
  if (tabPtrP->m_attributes[DD].m_no_of_varsize == 0)
  {
    sz = 1;
    const Uint32 *src= ((Fix_page*)pagePtr.p)->get_ptr(page_idx, 0);
    ndbassert(* (src + 1) != Tup_fixsize_page::FREE_RECORD);
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
  
  Uint32 ext = pagePtr.p->m_extent_info_ptr;
  Uint32 used = pagePtr.p->uncommitted_used_space;
  Uint32 old_idx = pagePtr.p->list_index;
  ddassert(old_free >= used);
  ddassert(new_free >= used);
  ddassert(new_free >= old_free);
  ddassert((old_idx & 0x8000) == 0);

  Uint32 new_idx = alloc.calc_page_free_bits(new_free - used);
  ddassert(alloc.calc_page_free_bits(old_free - used) == old_idx);
  
  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, ext);
  
  if (old_idx != new_idx)
  {
    jam();
    disk_page_move_dirty_page(alloc, extentPtr, pagePtr, old_idx, new_idx);
  }
  
  update_extent_pos(alloc, extentPtr, sz);
#if NOT_YET_FREE_EXTENT
  if (check_free(extentPtr.p) == 0)
  {
    ndbout_c("free: extent is free");
  }
#endif
}

void
Dbtup::disk_page_abort_prealloc(Signal *signal, Fragrecord* fragPtrP, 
				Local_key* key, Uint32 sz)
{
  jam();

  Page_cache_client::Request req;
  req.m_callback.m_callbackData= sz;
  req.m_callback.m_callbackFunction = 
    safe_cast(&Dbtup::disk_page_abort_prealloc_callback);
  
  int flags= Page_cache_client::DIRTY_REQ;
  memcpy(&req.m_page, key, sizeof(Local_key));

  Page_cache_client pgman(this, c_pgman);
  int res= pgman.get_page(signal, req, flags);
  m_pgman_ptr = pgman.m_ptr;
  jamEntry();
  switch(res)
  {
  case 0:
    jam();
    break;
  case -1:
    ndbrequire(false);
    break;
  default:
    jam();
    Ptr<GlobalPage> gpage;
    m_global_page_pool.getPtr(gpage, (Uint32)res);
    PagePtr pagePtr;
    pagePtr.i = gpage.i;
    pagePtr.p = reinterpret_cast<Page*>(gpage.p);

    disk_page_abort_prealloc_callback_1(signal, fragPtrP, pagePtr, sz);
  }
}

void
Dbtup::disk_page_abort_prealloc_callback(Signal* signal, 
					 Uint32 sz, Uint32 page_id)
{
  //ndbout_c("disk_alloc_page_callback id: %d", page_id);
  jamEntry();  
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  
  PagePtr pagePtr;
  pagePtr.i = gpage.i;
  pagePtr.p = reinterpret_cast<Page*>(gpage.p);

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
  jam();
  disk_page_set_dirty(pagePtr);

  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;

  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, pagePtr.p->m_extent_info_ptr);

  Uint32 idx = pagePtr.p->list_index & 0x7FFF;
  Uint32 used = pagePtr.p->uncommitted_used_space;
  Uint32 free = pagePtr.p->free_space;

  ddassert(free >= used);
  ddassert(used >= sz);
  ddassert(alloc.calc_page_free_bits(free - used) == idx);

  pagePtr.p->uncommitted_used_space = used - sz;

  Uint32 new_idx = alloc.calc_page_free_bits(free - used + sz);

  if (idx != new_idx)
  {
    jam();
    disk_page_move_dirty_page(alloc, extentPtr, pagePtr, idx, new_idx);
  }
  
  update_extent_pos(alloc, extentPtr, sz);
#if NOT_YET_FREE_EXTENT
  if (check_free(extentPtr.p) == 0)
  {
    ndbout_c("abort: extent is free");
  }
#endif
}

#if NOT_YET_UNDO_ALLOC_EXTENT
void
Dbtup::disk_page_alloc_extent_log_buffer_callback(Signal* signal,
						  Uint32 extentPtrI,
						  Uint32 unused)
{
  Ptr<Extent_info> extentPtr;
  c_extent_pool.getPtr(extentPtr, extentPtrI);

  Local_key key = extentPtr.p->m_key;
  Tablespace_client2 tsman(signal, c_tsman, &key);

  Ptr<Tablerec> tabPtr;
  tabPtr.i= tsman.m_table_id;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  Ptr<Fragrecord> fragPtr;
  getFragmentrec(fragPtr, tsman.m_fragment_id, tabPtr.p);

  Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);

  Disk_undo::AllocExtent alloc;
  alloc.m_table = tabPtr.i;
  alloc.m_fragment = tsman.m_fragment_id;
  alloc.m_page_no = key.m_page_no;
  alloc.m_file_no = key.m_file_no;
  alloc.m_type_length = (Disk_undo::UNDO_ALLOC_EXTENT<<16)|(sizeof(alloc)>> 2);
  
  Logfile_client::Change c[1] = {{ &alloc, sizeof(alloc) >> 2 } };
  
  Uint64 lsn= lgman.add_entry(c, 1);
  
  tsman.update_lsn(&key, lsn);
  jamEntry();
}
#endif

Uint64
Dbtup::disk_page_undo_alloc(Page* page, const Local_key* key,
			    Uint32 sz, Uint32 gci, Uint32 logfile_group_id)
{
  jam();
  D("Logfile_client - disk_page_undo_alloc");
  Logfile_client lgman(this, c_lgman, logfile_group_id);
  
  Disk_undo::Alloc alloc;
  alloc.m_type_length= (Disk_undo::UNDO_ALLOC << 16) | (sizeof(alloc) >> 2);
  alloc.m_page_no = key->m_page_no;
  alloc.m_file_no_page_idx= key->m_file_no << 16 | key->m_page_idx;
  
  Logfile_client::Change c[1] = {{ &alloc, sizeof(alloc) >> 2 } };
  
  Uint64 lsn= lgman.add_entry(c, 1);
  jamEntry();
  Page_cache_client pgman(this, c_pgman);
  pgman.update_lsn(* key, lsn);
  jamEntry();

  return lsn;
}

Uint64
Dbtup::disk_page_undo_update(Page* page, const Local_key* key,
			     const Uint32* src, Uint32 sz,
			     Uint32 gci, Uint32 logfile_group_id)
{
  jam();
  D("Logfile_client - disk_page_undo_update");
  Logfile_client lgman(this, c_lgman, logfile_group_id);

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
    
  Uint64 lsn= lgman.add_entry(c, 3);
  jamEntry();
  Page_cache_client pgman(this, c_pgman);
  pgman.update_lsn(* key, lsn);
  jamEntry();

  return lsn;
}
  
Uint64
Dbtup::disk_page_undo_free(Page* page, const Local_key* key,
			   const Uint32* src, Uint32 sz,
			   Uint32 gci, Uint32 logfile_group_id)
{
  jam();
  D("Logfile_client - disk_page_undo_free");
  Logfile_client lgman(this, c_lgman, logfile_group_id);

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
  
  Uint64 lsn= lgman.add_entry(c, 3);
  jamEntry();
  Page_cache_client pgman(this, c_pgman);
  pgman.update_lsn(* key, lsn);
  jamEntry();

  return lsn;
}
  
#include <signaldata/LgmanContinueB.hpp>

#define DBG_UNDO 0

void
Dbtup::disk_restart_undo(Signal* signal, Uint64 lsn,
			 Uint32 type, const Uint32 * ptr, Uint32 len)
{
  f_undo_done = false;
  f_undo.m_lsn= lsn;
  f_undo.m_ptr= ptr;
  f_undo.m_len= len;
  f_undo.m_type = type;

  Page_cache_client::Request preq;
  switch(f_undo.m_type){
  case File_formats::Undofile::UNDO_LCP_FIRST:
  case File_formats::Undofile::UNDO_LCP:
  {
    jam();
    ndbrequire(len == 3);
    Uint32 lcp = ptr[0];
    Uint32 tableId = ptr[1] >> 16;
    Uint32 fragId = ptr[1] & 0xFFFF;
    disk_restart_undo_lcp(tableId, fragId, Fragrecord::UC_LCP, lcp);
    if (!isNdbMtLqh())
      disk_restart_undo_next(signal);
    
    if (DBG_UNDO)
    {
      ndbout_c("UNDO LCP %u (%u, %u)", lcp, tableId, fragId);
    }
    return;
  }
  case File_formats::Undofile::UNDO_TUP_ALLOC:
  {
    jam();
    Disk_undo::Alloc* rec= (Disk_undo::Alloc*)ptr;
    preq.m_page.m_page_no = rec->m_page_no;
    preq.m_page.m_file_no  = rec->m_file_no_page_idx >> 16;
    preq.m_page.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;    
    break;
  }
  case File_formats::Undofile::UNDO_TUP_UPDATE:
  {
    jam();
    Disk_undo::Update* rec= (Disk_undo::Update*)ptr;
    preq.m_page.m_page_no = rec->m_page_no;
    preq.m_page.m_file_no  = rec->m_file_no_page_idx >> 16;
    preq.m_page.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_FREE:
  {
    jam();
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
    jam();
    Disk_undo::Create* rec= (Disk_undo::Create*)ptr;
    Ptr<Tablerec> tabPtr;
    tabPtr.i= rec->m_table;
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
    for(Uint32 i = 0; i<NDB_ARRAY_SIZE(tabPtr.p->fragrec); i++)
      if (tabPtr.p->fragrec[i] != RNIL)
	disk_restart_undo_lcp(tabPtr.i, tabPtr.p->fragid[i], 
			      Fragrecord::UC_CREATE, 0);
    if (!isNdbMtLqh())
      disk_restart_undo_next(signal);

    if (DBG_UNDO)
    {
      ndbout_c("UNDO CREATE (%u)", tabPtr.i);
    }
    return;
  }
  case File_formats::Undofile::UNDO_TUP_DROP:
  {
    jam();
    Disk_undo::Drop* rec = (Disk_undo::Drop*)ptr;
    Ptr<Tablerec> tabPtr;
    tabPtr.i= rec->m_table;
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
    for(Uint32 i = 0; i<NDB_ARRAY_SIZE(tabPtr.p->fragrec); i++)
      if (tabPtr.p->fragrec[i] != RNIL)
	disk_restart_undo_lcp(tabPtr.i, tabPtr.p->fragid[i], 
			      Fragrecord::UC_CREATE, 0);
    if (!isNdbMtLqh())
      disk_restart_undo_next(signal);

    if (DBG_UNDO)
    {
      ndbout_c("UNDO DROP (%u)", tabPtr.i);
    }
    return;
  }
  case File_formats::Undofile::UNDO_TUP_ALLOC_EXTENT:
    jam();
  case File_formats::Undofile::UNDO_TUP_FREE_EXTENT:
    jam();
    disk_restart_undo_next(signal);
    return;

  case File_formats::Undofile::UNDO_END:
    jam();
    f_undo_done = true;
    return;
  default:
    ndbrequire(false);
  }

  f_undo.m_key = preq.m_page;
  preq.m_callback.m_callbackFunction = 
    safe_cast(&Dbtup::disk_restart_undo_callback);
  
  int flags = 0;
  Page_cache_client pgman(this, c_pgman);
  int res= pgman.get_page(signal, preq, flags);
  m_pgman_ptr = pgman.m_ptr;
  jamEntry();
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
Dbtup::disk_restart_lcp_id(Uint32 tableId, Uint32 fragId, Uint32 lcpId)
{
  jamEntry();
  
  if (lcpId == RNIL)
  {
    disk_restart_undo_lcp(tableId, fragId, Fragrecord::UC_CREATE, 0);
    if (DBG_UNDO)
    {
      ndbout_c("mark_no_lcp (%u, %u)", tableId, fragId);
    }
  }
  else
  {
    disk_restart_undo_lcp(tableId, fragId, Fragrecord::UC_SET_LCP, lcpId); 
    if (DBG_UNDO)
    {
      ndbout_c("mark_no_lcp (%u, %u)", tableId, fragId);
    }

  }
}

void
Dbtup::disk_restart_undo_lcp(Uint32 tableId, Uint32 fragId, Uint32 flag, 
			     Uint32 lcpId)
{
  Ptr<Tablerec> tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  if (tabPtr.p->tableStatus == DEFINED && tabPtr.p->m_no_of_disk_attributes)
  {
    jam();
    FragrecordPtr fragPtr;
    getFragmentrec(fragPtr, fragId, tabPtr.p);
    if (!fragPtr.isNull())
    {
      jam();
      switch(flag){
      case Fragrecord::UC_CREATE:
	jam();
	fragPtr.p->m_undo_complete |= flag;
	return;
      case Fragrecord::UC_LCP:
	jam();
	if (fragPtr.p->m_undo_complete == 0 && 
	    fragPtr.p->m_restore_lcp_id == lcpId)
	{
	  jam();
	  fragPtr.p->m_undo_complete |= flag;
	  if (DBG_UNDO)
            ndbout_c("table: %u fragment: %u lcp: %u -> done", 
		     tableId, fragId, lcpId);
	}
	return;
      case Fragrecord::UC_SET_LCP:
      {
	jam();
	if (DBG_UNDO)
           ndbout_c("table: %u fragment: %u restore to lcp: %u",
		    tableId, fragId, lcpId);
	ndbrequire(fragPtr.p->m_undo_complete == 0);
	ndbrequire(fragPtr.p->m_restore_lcp_id == RNIL);
	fragPtr.p->m_restore_lcp_id = lcpId;
	return;
      }
      }
      jamLine(flag);
      ndbrequire(false);
    }
  }
}

void
Dbtup::disk_restart_undo_callback(Signal* signal,
				  Uint32 id, 
				  Uint32 page_id)
{
  jamEntry();
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  PagePtr pagePtr;
  pagePtr.i = gpage.i;
  pagePtr.p = reinterpret_cast<Page*>(gpage.p);

  Apply_undo* undo = &f_undo;
  
  bool update = false;
  if (! (pagePtr.p->list_index & 0x8000) ||
      pagePtr.p->nextList != RNIL ||
      pagePtr.p->prevList != RNIL)
  {
    jam();
    update = true;
    pagePtr.p->list_index |= 0x8000;
    pagePtr.p->nextList = pagePtr.p->prevList = RNIL;
  }
  
  Uint32 tableId= pagePtr.p->m_table_id;
  Uint32 fragId = pagePtr.p->m_fragment_id;
  
  if (tableId >= cnoOfTablerec)
  {
    jam();
    if (DBG_UNDO)
      ndbout_c("UNDO table> %u", tableId);
    disk_restart_undo_next(signal);
    return;
  }
  undo->m_table_ptr.i = tableId;
  ptrCheckGuard(undo->m_table_ptr, cnoOfTablerec, tablerec);
  
  if (! (undo->m_table_ptr.p->tableStatus == DEFINED && 
         undo->m_table_ptr.p->m_no_of_disk_attributes))
  {
    jam();
    if (DBG_UNDO)
      ndbout_c("UNDO !defined (%u) ", tableId);
    disk_restart_undo_next(signal);
    return;
  }
  
  getFragmentrec(undo->m_fragment_ptr, fragId, undo->m_table_ptr.p);
  if(undo->m_fragment_ptr.isNull())
  {
    jam();
    if (DBG_UNDO)
      ndbout_c("UNDO fragment null %u/%u", tableId, fragId);
    disk_restart_undo_next(signal);
    return;
  }

  if (undo->m_fragment_ptr.p->m_undo_complete)
  {
    jam();
    if (DBG_UNDO)
      ndbout_c("UNDO undo complete %u/%u", tableId, fragId);
    disk_restart_undo_next(signal);
    return;
  }
  
  Local_key key = undo->m_key;
//  key.m_page_no = pagePtr.p->m_page_no;
//  key.m_file_no = pagePtr.p->m_file_no;
  
  Uint64 lsn = 0;
  lsn += pagePtr.p->m_page_header.m_page_lsn_hi; lsn <<= 32;
  lsn += pagePtr.p->m_page_header.m_page_lsn_lo;

  undo->m_page_ptr = pagePtr;
  
  if (undo->m_lsn <= lsn)
  {
    jam();
    if (DBG_UNDO)
    {
      ndbout << "apply: " << undo->m_lsn << "(" << lsn << " )" 
	     << key << " type: " << undo->m_type << endl;
    }
    
    update = true;
    if (DBG_UNDO)
      ndbout_c("applying %lld", undo->m_lsn);
    /**
     * Apply undo record
     */
    switch(undo->m_type){
    case File_formats::Undofile::UNDO_TUP_ALLOC:
      jam();
      disk_restart_undo_alloc(undo);
      break;
    case File_formats::Undofile::UNDO_TUP_UPDATE:
      jam();
      disk_restart_undo_update(undo);
      break;
    case File_formats::Undofile::UNDO_TUP_FREE:
      jam();
      disk_restart_undo_free(undo);
      break;
    default:
      ndbrequire(false);
    }

    if (DBG_UNDO)
      ndbout << "disk_restart_undo: " << undo->m_type << " " 
	     << undo->m_key << endl;
    
    lsn = undo->m_lsn - 1; // make sure undo isn't run again...
    
    Page_cache_client pgman(this, c_pgman);
    pgman.update_lsn(undo->m_key, lsn);
    jamEntry();

    disk_restart_undo_page_bits(signal, undo);
  }
  else if (DBG_UNDO)
  {
    jam();
    ndbout << "ignore: " << undo->m_lsn << "(" << lsn << " )" 
	   << key << " type: " << undo->m_type 
	   << " tab: " << tableId << endl;
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
Dbtup::disk_restart_undo_page_bits(Signal* signal, Apply_undo* undo)
{
  Fragrecord* fragPtrP = undo->m_fragment_ptr.p;
  Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;
  
  /**
   * Set alloc.m_curr_extent_info_ptr_i to
   *   current this extent (and move old extend into free matrix)
   */
  Page* pageP = undo->m_page_ptr.p;
  Uint32 free = pageP->free_space;
  Uint32 new_bits = alloc.calc_page_free_bits(free);
  pageP->list_index = 0x8000 | new_bits;

  D("Tablespace_client - disk_restart_undo_page_bits");
  Tablespace_client tsman(signal, this, c_tsman,
			  fragPtrP->fragTableId,
			  fragPtrP->fragmentId,
			  fragPtrP->m_tablespace_id);
  
  tsman.restart_undo_page_free_bits(&undo->m_key, new_bits);
  jamEntry();
}

int
Dbtup::disk_restart_alloc_extent(Uint32 tableId, Uint32 fragId, 
				 const Local_key* key, Uint32 pages)
{
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  if (tabPtr.p->tableStatus == DEFINED && tabPtr.p->m_no_of_disk_attributes)
  {
    getFragmentrec(fragPtr, fragId, tabPtr.p);

    if (!fragPtr.isNull())
    {
      jam();

      if (fragPtr.p->m_undo_complete & Fragrecord::UC_CREATE)
      {
        jam();
        return -1;
      }

      Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
      
      Ptr<Extent_info> ext;
      ndbrequire(c_extent_pool.seize(ext));
#ifdef VM_TRACE
      ndbout << "allocated " << pages << " pages: " << *key 
	     << " table: " << tabPtr.i << " fragment: " << fragId << endl;
#endif      
      ext.p->m_key = *key;
      ext.p->m_first_page_no = ext.p->m_key.m_page_no;
      ext.p->m_free_space= 0;
      ext.p->m_empty_page_no = (1 << 16); // We don't know, so assume none
      memset(ext.p->m_free_page_count, 0, sizeof(ext.p->m_free_page_count));
      
      if (alloc.m_curr_extent_info_ptr_i != RNIL)
      {
	jam();
	Ptr<Extent_info> old;
	c_extent_pool.getPtr(old, alloc.m_curr_extent_info_ptr_i);
	ndbassert(old.p->m_free_matrix_pos == RNIL);
	Uint32 pos= alloc.calc_extent_pos(old.p);
	Local_extent_info_list new_list(c_extent_pool, alloc.m_free_extents[pos]);
	new_list.add(old);
	old.p->m_free_matrix_pos= pos;
      }
      
      alloc.m_curr_extent_info_ptr_i = ext.i;
      ext.p->m_free_matrix_pos = RNIL;
      c_extent_hash.add(ext);

      Local_fragment_extent_list list1(c_extent_pool, alloc.m_extent_list);
      list1.add(ext);
      return 0;
    }
  }

  return -1;
}

void
Dbtup::disk_restart_page_bits(Uint32 tableId, Uint32 fragId,
			      const Local_key* key, Uint32 bits)
{
  jam();
  TablerecPtr tabPtr;
  FragrecordPtr fragPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  if (tabPtr.p->tableStatus == DEFINED && tabPtr.p->m_no_of_disk_attributes)
  {
    jam();
    getFragmentrec(fragPtr, fragId, tabPtr.p);
    Disk_alloc_info& alloc= fragPtr.p->m_disk_alloc_info;
    
    Ptr<Extent_info> ext;
    c_extent_pool.getPtr(ext, alloc.m_curr_extent_info_ptr_i);
    
    Uint32 size= alloc.calc_page_free_space(bits);  
    
    ext.p->m_free_page_count[bits]++;
    update_extent_pos(alloc, ext, size); // actually only to update free_space
    ndbassert(ext.p->m_free_matrix_pos == RNIL);
  }
}

void
Dbtup::disk_page_get_allocated(const Tablerec* tabPtrP,
                               const Fragrecord * fragPtrP,
                               Uint64 res[2])
{
  res[0] = res[1] = 0;
  if (tabPtrP->m_no_of_disk_attributes)
  {
    jam();
    const Disk_alloc_info& alloc= fragPtrP->m_disk_alloc_info;
    Uint64 cnt = 0;
    Uint64 free = 0;

    {
      Disk_alloc_info& tmp = const_cast<Disk_alloc_info&>(alloc);
      Local_fragment_extent_list list(c_extent_pool, tmp.m_extent_list);
      Ptr<Extent_info> extentPtr;
      for (list.first(extentPtr); !extentPtr.isNull(); list.next(extentPtr))
      {
        cnt++;
        free += extentPtr.p->m_free_space;
      }
    }
    res[0] = cnt * alloc.m_extent_size * File_formats::NDB_PAGE_SIZE;
    res[1] = free * 4 * tabPtrP->m_offsets[DD].m_fix_header_size;
  }
}
