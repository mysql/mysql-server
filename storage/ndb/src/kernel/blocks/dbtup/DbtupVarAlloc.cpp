/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBTUP_C
#define DBTUP_VAR_ALLOC_CPP
#include "Dbtup.hpp"
#include "../dblqh/Dblqh.hpp"

#define JAM_FILE_ID 405


void Dbtup::init_list_sizes(void)
{
  c_min_list_size[0]= 200;
  c_max_list_size[0]= 499;

  c_min_list_size[1]= 500;
  c_max_list_size[1]= 999;

  c_min_list_size[2]= 1000;
  c_max_list_size[2]= 4079;

  c_min_list_size[3]= 4080;
  c_max_list_size[3]= 7783;

  /* The last free list must guarantee space for biggest possible column
   * size.
   * Assume varsize may take up the whole row (a slight exaggeration).
   */
  static_assert(MAX_EXPANDED_TUPLE_SIZE_IN_WORDS <= 7784);
  c_min_list_size[4]= 7784;
  c_max_list_size[4]= 8159;

  static_assert(MAX_FREE_LIST == 5);
  c_min_list_size[5]= 0;
  c_max_list_size[5]= 199;
}

/*
  Allocator for variable sized segments
  Part of the external interface for variable sized segments

  This method is used to allocate and free variable sized tuples and
  parts of tuples. This part can be used to implement variable sized
  attributes without wasting memory. It can be used to support small
  BLOB's attached to the record. It can also be used to support adding
  and dropping attributes without the need to copy the entire table.

  SYNOPSIS
    fragPtr         A pointer to the fragment description
    tabPtr          A pointer to the table description
    alloc_size       Size of the allocated record
    signal           The signal object to be used if a signal needs to
                     be sent
  RETURN VALUES
    Returns true if allocation was successful otherwise false

    page_offset      Page offset of allocated record
    page_index       Page index of allocated record
    page_ptr         The i and p value of the page where the record was
                     allocated
*/
Uint32* Dbtup::alloc_var_rec(Uint32 * err,
                             Fragrecord* fragPtr,
			     Tablerec* tabPtr,
			     Uint32 alloc_size,
			     Local_key* key,
			     Uint32 * out_frag_page_id)
{
  /**
   * TODO alloc fix+var part
   */
  Uint32 *ptr = alloc_fix_rec(jamBuffer(), err, fragPtr, tabPtr, key,
                              out_frag_page_id);
  if (unlikely(ptr == 0))
  {
    return 0;
  }
  
  Local_key varref;
  Tuple_header* tuple = (Tuple_header*)ptr;
  Var_part_ref* dst = tuple->get_var_part_ref_ptr(tabPtr);
  if (alloc_size)
  {
    if (likely(alloc_var_part(err, fragPtr, tabPtr, alloc_size, &varref) != 0))
    {
      dst->assign(&varref);
      return ptr;
    }
  }
  else
  {
    varref.m_page_no = RNIL;
    dst->assign(&varref);
    return ptr;
  }
  
  PagePtr pagePtr;
  ndbrequire(c_page_pool.getPtr(pagePtr, key->m_page_no));
  free_fix_rec(fragPtr, tabPtr, key, (Fix_page*)pagePtr.p);
  release_frag_mutex(fragPtr, *out_frag_page_id);
  return 0;
}

Uint32*
Dbtup::alloc_var_part(Uint32 * err,
                      Fragrecord* fragPtr,
		      Tablerec* tabPtr,
		      Uint32 alloc_size,
		      Local_key* key)
{
  PagePtr pagePtr;
  pagePtr.i= get_alloc_page(fragPtr, (alloc_size + 1));
  if (pagePtr.i == RNIL) { 
    jam();
    if ((pagePtr.i= get_empty_var_page(fragPtr)) == RNIL) {
      jam();
      * err = ZMEM_NOMEM_ERROR;
      return 0;
    }
    c_page_pool.getPtr(pagePtr);
    ((Var_page*)pagePtr.p)->init();
    fragPtr->m_varWordsFree += ((Var_page*)pagePtr.p)->free_space;
    pagePtr.p->list_index = MAX_FREE_LIST - 1;
    Local_Page_list list(c_page_pool,
			   fragPtr->free_var_page_array[MAX_FREE_LIST-1]);
    list.addFirst(pagePtr);
  } else {
    c_page_pool.getPtr(pagePtr);
    jam();
  }  
  /*
    First we remove the current free space on this page from fragment total.
    Then we calculate a new free space value for the page. Finally we call
    update_free_page_list() which adds this new value to the fragment total.
  */
  ndbassert(fragPtr->m_varWordsFree >= ((Var_page*)pagePtr.p)->free_space);
  fragPtr->m_varWordsFree -= ((Var_page*)pagePtr.p)->free_space;
  Uint32 idx= ((Var_page*)pagePtr.p)
    ->alloc_record(alloc_size, (Var_page*)ctemp_page, Var_page::CHAIN);

  fragPtr->m_varElemCount++;
  key->m_page_no = pagePtr.i;
  key->m_page_idx = idx;
  
  update_free_page_list(fragPtr, pagePtr);  
  return ((Var_page*)pagePtr.p)->get_ptr(idx);
}

/*
  free_var_part is used to free the variable length storage associated
  with the passed local key.
  It is not assumed that there is a corresponding fixed-length part.
  // TODO : Any need for tabPtr?
*/
void Dbtup::free_var_part(Fragrecord* fragPtr,
                          Tablerec* tabPtr,
                          Local_key* key)
{
  Ptr<Page> pagePtr;
  if (key->m_page_no != RNIL)
  {
    ndbrequire(c_page_pool.getPtr(pagePtr, key->m_page_no));
    ndbassert(fragPtr->m_varWordsFree >= ((Var_page*)pagePtr.p)->free_space);
    fragPtr->m_varWordsFree -= ((Var_page*)pagePtr.p)->free_space;
    ((Var_page*)pagePtr.p)->free_record(key->m_page_idx, Var_page::CHAIN);
    ndbassert(fragPtr->m_varElemCount > 0);
    fragPtr->m_varElemCount--;

    ndbassert(pagePtr.p->free_space <= Var_page::DATA_WORDS);
    if (pagePtr.p->free_space == Var_page::DATA_WORDS - 1)
    {
      jam();
      Uint32 idx = pagePtr.p->list_index;
      Local_Page_list list(c_page_pool, fragPtr->free_var_page_array[idx]);
      list.remove(pagePtr);
      returnCommonArea(pagePtr.i, 1);
      fragPtr->noOfVarPages --;
    } else {
      jam();
      // Adds the new free space value for the page to the fragment total.
      update_free_page_list(fragPtr, pagePtr);
    }
    ndbassert(fragPtr->verifyVarSpace());
  }
  return;
}

/*
  Deallocator for variable sized segments
  Part of the external interface for variable sized segments

  SYNOPSIS
    fragPtr         A pointer to the fragment description
    tabPtr          A pointer to the table description
    signal           The signal object to be used if a signal needs to
                     be sent
    page_ptr         A reference to the page of the variable sized
                     segment
    free_page_index  Page index on page of variable sized segment
                     which is freed
  RETURN VALUES
    Returns true if deallocation was successful otherwise false
*/
void Dbtup::free_var_rec(Fragrecord* fragPtr,
			 Tablerec* tabPtr,
			 Local_key* key,
			 Ptr<Page> pagePtr)
{
  /**
   * TODO free fix + var part
   */
  Uint32 *ptr = ((Fix_page*)pagePtr.p)->get_ptr(key->m_page_idx, 0);
  Tuple_header* tuple = (Tuple_header*)ptr;

  Local_key ref;
  Var_part_ref * varref = tuple->get_var_part_ref_ptr(tabPtr);
  varref->copyout(&ref);

  free_fix_rec(fragPtr, tabPtr, key, (Fix_page*)pagePtr.p);

  if (ref.m_page_no != RNIL)
  {
    jam();
    ndbrequire(c_page_pool.getPtr(pagePtr, ref.m_page_no));
    free_var_part(fragPtr, pagePtr, ref.m_page_idx);
  }
  return;
}

void
Dbtup::free_var_part(Fragrecord* fragPtr, PagePtr pagePtr, Uint32 page_idx)
{
  ndbassert(fragPtr->m_varWordsFree >= ((Var_page*)pagePtr.p)->free_space);
  fragPtr->m_varWordsFree -= ((Var_page*)pagePtr.p)->free_space;
  ((Var_page*)pagePtr.p)->free_record(page_idx, Var_page::CHAIN);
  ndbassert(fragPtr->m_varElemCount > 0);
  fragPtr->m_varElemCount--;

  ndbassert(pagePtr.p->free_space <= Var_page::DATA_WORDS);
  if (pagePtr.p->free_space == Var_page::DATA_WORDS - 1)
  {
    jam();
    Uint32 idx = pagePtr.p->list_index;
    Local_Page_list list(c_page_pool, fragPtr->free_var_page_array[idx]);
    list.remove(pagePtr);
    returnCommonArea(pagePtr.i, 1);
    fragPtr->noOfVarPages --;
  }
  else
  {
    jam();
    // Adds the new free space value for the page to the fragment total.
    update_free_page_list(fragPtr, pagePtr);
  }
  ndbassert(fragPtr->verifyVarSpace());
}

Uint32 *
Dbtup::realloc_var_part(Uint32 * err,
                        Fragrecord* fragPtr, Tablerec* tabPtr, PagePtr pagePtr,
			Var_part_ref* refptr, Uint32 oldsz, Uint32 newsz)
{
  Uint32 add = newsz - oldsz;
  Uint32 *new_var_ptr;
  Var_page* pageP = (Var_page*)pagePtr.p;
  Local_key oldref;
  refptr->copyout(&oldref);
  
  ndbassert(newsz);
  ndbassert(add);

  if (oldsz && pageP->free_space >= add)
  {
    jam();
    new_var_ptr= pageP->get_ptr(oldref.m_page_idx);
    {
      if(0) printf("extra reorg");
      jam();
      /**
       * In this case we need to reorganise the page to fit. To ensure we
       * don't complicate matters we make a little trick here where we
       * fool the reorg_page to avoid copying the entry at hand and copy
       * that separately at the end. This means we need to copy it out of
       * the page before reorg_page to save the entry contents.
       */
      Uint32* copyBuffer= cinBuffer;
      memcpy(copyBuffer, new_var_ptr, 4*oldsz);
      pageP->set_entry_len(oldref.m_page_idx, 0);
      pageP->free_space += oldsz;
      fragPtr->m_varWordsFree += oldsz;
      pageP->reorg((Var_page*)ctemp_page);
      new_var_ptr= pageP->get_free_space_ptr();
      memcpy(new_var_ptr, copyBuffer, 4*oldsz);
      pageP->set_entry_offset(oldref.m_page_idx, pageP->insert_pos);
      add += oldsz;
    }
    ndbassert(fragPtr->m_varWordsFree >= pageP->free_space);
    fragPtr->m_varWordsFree -= pageP->free_space;

    pageP->grow_entry(oldref.m_page_idx, add);
    // Adds the new free space value for the page to the fragment total.
    update_free_page_list(fragPtr, pagePtr);
  }
  else
  {
    jam();
    Local_key newref;
    new_var_ptr = alloc_var_part(err, fragPtr, tabPtr, newsz, &newref);
    if (unlikely(new_var_ptr == 0))
      return NULL;

    if (oldsz)
    {
      jam();
      Uint32 *src = pageP->get_ptr(oldref.m_page_idx);
      ndbassert(oldref.m_page_no != newref.m_page_no);
      ndbassert(pageP->get_entry_len(oldref.m_page_idx) == oldsz);
      memcpy(new_var_ptr, src, 4*oldsz);
      free_var_part(fragPtr, pagePtr, oldref.m_page_idx);
    }

    refptr->assign(&newref);
  }
  
  return new_var_ptr;
}

void
Dbtup::move_var_part(Fragrecord* fragPtr,
                     Tablerec* tabPtr,
                     PagePtr pagePtr,
                     Var_part_ref* refptr,
                     Uint32 size,
                     Tuple_header *org)
{
  jam();

  ndbassert(size);
  Var_page* pageP = (Var_page*)pagePtr.p;
  Local_key oldref;
  refptr->copyout(&oldref);

  /**
   * to find destination page index of free list
   */
  Uint32 new_index = calculate_free_list_impl(size);

  /**
   * do not move tuple from big-free-size page list
   * to small-free-size page list
   */
  if (new_index > pageP->list_index)
  {
    jam();
    return;
  }

  PagePtr new_pagePtr;
  new_pagePtr.i = get_alloc_page(fragPtr, size + 1);

  if (new_pagePtr.i == RNIL)
  {
    jam();
    return;
  }

  /**
   * do not move varpart if new var part page is same as old
   */
  if (new_pagePtr.i == pagePtr.i)
  {
    jam();
    return;
  }

  c_page_pool.getPtr(new_pagePtr);

  ndbassert(fragPtr->m_varWordsFree >= ((Var_page*)new_pagePtr.p)->free_space);
  fragPtr->m_varWordsFree -= ((Var_page*)new_pagePtr.p)->free_space;

  Uint32 idx= ((Var_page*)new_pagePtr.p)
    ->alloc_record(size,(Var_page*)ctemp_page, Var_page::CHAIN);

  /**
   * update new page into new free list after alloc_record
   */
  update_free_page_list(fragPtr, new_pagePtr);

  Uint32 *dst = ((Var_page*)new_pagePtr.p)->get_ptr(idx);
  const Uint32 *src = pageP->get_ptr(oldref.m_page_idx);

  /**
   * copy old varpart to new position
   */
  memcpy(dst, src, 4*size);

  /**
   * At his point we need to upgrade to exclusive fragment access.
   * The variable sized part might be used for reading in query
   * thread at this point in time. To avoid having to use a mutex
   * to protect reads of rows we ensure that all places where we
   * reorganize pages and rows are done with exclusive fragment
   * access.
   *
   * Since we change the reference to the variable part we also
   * need to recalculate while being in exclusive mode.
   */
  c_lqh->upgrade_to_exclusive_frag_access();
  fragPtr->m_varElemCount++;
  /**
   * remove old var part of tuple (and decrement m_varElemCount).
   */
  free_var_part(fragPtr, pagePtr, oldref.m_page_idx);
  /**
   * update var part ref of fix part tuple to newref
   */
  Local_key newref;
  newref.m_page_no = new_pagePtr.i;
  newref.m_page_idx = idx;
  refptr->assign(&newref);
  setChecksum(org, tabPtr);
  c_lqh->downgrade_from_exclusive_frag_access();
}

/* ------------------------------------------------------------------------ */
// Get a page from one of free lists. If the desired free list is empty we
// try with the next until we have tried all possible lists.
/* ------------------------------------------------------------------------ */
Uint32
Dbtup::get_alloc_page(Fragrecord* fragPtr, Uint32 alloc_size)
{
  Uint32 start_index;
  PagePtr pagePtr;
  
  start_index= calculate_free_list_for_alloc(alloc_size);
  ndbassert(start_index < MAX_FREE_LIST);
  for (Uint32 i = start_index; i < MAX_FREE_LIST; i++)
  {
    jam();
    if (!fragPtr->free_var_page_array[i].isEmpty()) 
    {
      jam();
      return fragPtr->free_var_page_array[i].getFirst();
    }
  }
  /* If no list with enough guaranteed size of free space is empty, fallback
   * checking the first 16 entries in the free list which may have an entry
   * with enough free space.
   */
  if (start_index == 0)
  {
    jam();
    return RNIL;
  }
  start_index--;
  Local_Page_list list(c_page_pool, fragPtr->free_var_page_array[start_index]);
  list.first(pagePtr);
  for(Uint32 loop = 0; !pagePtr.isNull() && loop < 16; loop++)
  {
    jam();
    if (pagePtr.p->free_space >= alloc_size)
    {
      jam();
      return pagePtr.i;
    }
    list.next(pagePtr);
  }
  return RNIL;
}

Uint32
Dbtup::get_empty_var_page(Fragrecord* fragPtr)
{
  PagePtr ptr;
  Uint32 cnt;
  allocConsPages(jamBuffer(), 1, cnt, ptr.i);
  fragPtr->noOfVarPages+= cnt;
  if (unlikely(cnt == 0))
  {
    return RNIL;
  }

  c_page_pool.getPtr(ptr);
  ptr.p->physical_page_id = ptr.i;
  ptr.p->page_state = ~0;
  ptr.p->nextList = RNIL;
  ptr.p->prevList = RNIL;
  ptr.p->frag_page_id = RNIL;
  
  return ptr.i;
}

/* ------------------------------------------------------------------------ */
// Check if the page needs to go to a new free page list.
/* ------------------------------------------------------------------------ */
void Dbtup::update_free_page_list(Fragrecord* fragPtr,
                                  Ptr<Page> pagePtr)
{
  Uint32 free_space, list_index;
  free_space= pagePtr.p->free_space;
  list_index= pagePtr.p->list_index;
  fragPtr->m_varWordsFree+= free_space;
  ndbassert(fragPtr->verifyVarSpace());

  if ((free_space < c_min_list_size[list_index]) ||
      (free_space > c_max_list_size[list_index])) {
    Uint32 new_list_index= calculate_free_list_impl(free_space);

    {
      /**
       * Remove from free list
       */
      Local_Page_list
        list(c_page_pool, fragPtr->free_var_page_array[list_index]);
      list.remove(pagePtr);
    }
    if (free_space < c_min_list_size[new_list_index])
    {
      /*
	We have not sufficient amount of free space to put it into any
	free list. Thus the page will not be available for new inserts.
	This can only happen for the free list with least guaranteed 
	free space.

        Put in on MAX_FREE_LIST-list (i.e full pages)
      */
      jam();
      ndbrequire(new_list_index == 0);
      new_list_index = MAX_FREE_LIST;
    }

    {
      Local_Page_list list(c_page_pool,
                             fragPtr->free_var_page_array[new_list_index]);
      list.addFirst(pagePtr);
      pagePtr.p->list_index = new_list_index;
    }
  }
}

/* ------------------------------------------------------------------------ */
// Given size of free space, calculate the free list to put it into
/* ------------------------------------------------------------------------ */
Uint32 Dbtup::calculate_free_list_impl(Uint32 free_space_size) const
{
  Uint32 i;
  for (i = 0; i < MAX_FREE_LIST; i++) {
    jam();
    if (free_space_size <= c_max_list_size[i]) {
      jam();
      return i;
    }
  }
  ndbabort();
  return 0;
}

Uint32 Dbtup::calculate_free_list_for_alloc(Uint32 alloc_size) const
{
  ndbassert(alloc_size <= MAX_EXPANDED_TUPLE_SIZE_IN_WORDS);
  for (Uint32 i = 0; i < MAX_FREE_LIST; i++)
  {
    jam();
    if (alloc_size <= c_min_list_size[i])
    {
      jam();
      return i;
    }
  }
  /* Allocation too big, last free list page should always have space for
   * biggest possible allocation.
   */
  ndbabort();
}

Uint64 Dbtup::calculate_used_var_words(Fragrecord* fragPtr)
{
  /* Loop over all VarSize pages in this fragment, summing
   * their used space
   */
  Uint64 totalUsed= 0;
  for (Uint32 freeList= 0; freeList <= MAX_FREE_LIST; freeList++)
  {
    Local_Page_list list(c_page_pool,
                           fragPtr->free_var_page_array[freeList]);
    Ptr<Page> pagePtr;

    if (list.first(pagePtr))
    {
      do
      {
        totalUsed+= (Tup_varsize_page::DATA_WORDS - pagePtr.p->free_space);
      } while (list.next(pagePtr));
    };
  };

  return totalUsed;
}

Uint32* 
Dbtup::alloc_var_rowid(Uint32 * err,
                       Fragrecord* fragPtr,
		       Tablerec* tabPtr,
		       Uint32 alloc_size,
		       Local_key* key,
		       Uint32 * out_frag_page_id)
{
  Uint32 *ptr = alloc_fix_rowid(err, fragPtr, tabPtr, key, out_frag_page_id);
  if (unlikely(ptr == 0))
  {
    return 0;
  }

  Local_key varref;
  Tuple_header* tuple = (Tuple_header*)ptr;
  Var_part_ref* dst = (Var_part_ref*)tuple->get_var_part_ref_ptr(tabPtr);

  if (alloc_size)
  {
    if (likely(alloc_var_part(err, fragPtr, tabPtr, alloc_size, &varref) != 0))
    {
      dst->assign(&varref);
      return ptr;
    }
  }
  else
  {
    varref.m_page_no = RNIL;
    dst->assign(&varref);
    return ptr;
  }
  
  PagePtr pagePtr;
  ndbrequire(c_page_pool.getPtr(pagePtr, key->m_page_no));
  free_fix_rec(fragPtr, tabPtr, key, (Fix_page*)pagePtr.p);
  release_frag_mutex(fragPtr, *out_frag_page_id);
  return 0;
}
