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

#define ljam() { jamLine(32000 + __LINE__); }
#define ljamEntry() { jamEntryLine(32000 + __LINE__); }


void Dbtup::init_list_sizes(void)
{
  c_min_list_size[0]= 200;
  c_max_list_size[0]= 499;

  c_min_list_size[1]= 500;
  c_max_list_size[1]= 999;

  c_min_list_size[2]= 1000;
  c_max_list_size[2]= 4079;

  c_min_list_size[3]= 4080;
  c_max_list_size[3]= 8159;

  c_min_list_size[4]= 0;
  c_max_list_size[4]= 199;
}

#if 0
void
Dbtup::free_separate_var_part(Fragrecord* const regFragPtr,
                              Tablerec* const regTabPtr,
                              Tuple_header* tuple_header)
{
  Uint32 page_ref, page_index;
  PagePtr page_ptr;
  page_ref= tuple_header->m_data[regTabPtr->var_offset];
  page_index= page_ref & MAX_TUPLES_PER_PAGE;
  page_ptr.i= page_ref >> MAX_TUPLES_BITS;
  ptrCheckGuard(page_ptr, cnoOfPage, cpage);
  free_var_rec(regFragPtr,
               regTabPtr,
               (Var_page*)page_ptr.p,
               page_index);
}


void
Dbtup::abort_separate_var_part(Uint32 var_page_ref,
                               const Uint32* copy_var_part,
                               Uint32 copy_var_size)
{
  Uint32 page_index;
  PagePtr var_page_ptr;
  page_index= var_page_ref & MAX_TUPLES_PER_PAGE;
  var_page_ptr.i= var_page_ref >> MAX_TUPLES_BITS;
  ptrCheckGuard(var_page_ptr, cnoOfPage, cpage);
  Uint32 *ptr= ((Var_page*)var_page_ptr.p)->get_ptr(page_index);
  MEMCOPY_NO_WORDS(ptr, copy_var_part, copy_var_size);
}

void
Dbtup::shrink_entry(Fragrecord* const regFragPtr,
                    Var_page* const page_ptr,
                    Uint32 page_index,
                    Uint32 new_size)
{
  
  page_ptr->shrink_entry(page_index, new_size);
  update_free_page_list(regFragPtr, page_ptr);
}

void
Dbtup::check_entry_size(KeyReqStruct* req_struct,
                        Operationrec* regOperPtr,
			Fragrecord* const regFragPtr,
                        Tablerec* const regTabPtr)
{
#if 0
  Uint32 vp_index, no_var_attr, total_var_size, add_size, new_size, entry_len;
  Uint32 vp_offset, tuple_size, var_part_local;
  Uint32 *var_data_part, *var_link;
  PagePtr var_page_ptr;
  Uint32* tuple_ptr= req_struct->m_tuple_ptr;
  Uint32 page_index= regOperPtr->m_tuple_location.m_page_idx;
  tuple_size= regTabPtr->tupheadsize;
  no_var_attr= regTabPtr->no_var_attr;
  var_part_local= get_var_part_local(* (tuple_ptr+1));
  add_size= regTabPtr->var_array_wsize;
  var_link= tuple_ptr+tuple_size;
  if (var_part_local == 1) {
    ljam();
    var_data_part= var_link;
    var_page_ptr.p= req_struct->fix_page_ptr.p;
    add_size+= tuple_size;
    vp_index= regOperPtr->m_tuple_location.m_page_idx;
  } else {
    ljam();
    entry_len= get_entry_len(req_struct->var_page_ptr, page_index);
    if (entry_len > (tuple_size + 1)) {
      ljam();
      shrink_entry(regFragPtr,
                   req_struct->fix_page_ptr,
                   page_index,
                   tuple_size + 1);
    } else {
      ndbassert(entry_len == (tuple_size + 1));
    }
    set_up_var_page(*var_link,
                    regFragPtr,
                    var_page_ptr,
                    vp_index,
                    vp_offset);
    var_data_part= &var_page_ptr.p->pageWord[vp_offset];
  }
  total_var_size= calculate_total_var_size((uint16*)var_data_part,
                                           no_var_attr);
  new_size= total_var_size + add_size;
  entry_len= get_entry_len(var_page_ptr.p, vp_index);
  if (new_size < entry_len) {
    ljam();
    shrink_entry(regFragPtr,
                 var_page_ptr.p,
                 vp_index,
                 new_size);
  } else {
    ndbassert(entry_len == new_size);
  }
#endif
}

inline
void
Dbtup::grow_entry(Fragrecord* const regFragPtr,
                  Var_page* page_header,
                  Uint32 page_index,
                  Uint32 growth_len)
{
  page_header->grow_entry(page_index, growth_len);
  update_free_page_list(regFragPtr, page_header);
}


void
Dbtup::setup_varsize_part(KeyReqStruct* req_struct,
			  Operationrec* const regOperPtr,
			  Tablerec* const regTabPtr)
{
  Uint32 num_var_attr;
  Uint32 var_data_wsize;
  Uint32* var_data_ptr;
  Uint32* var_data_start;
  
  Uint32 page_index= regOperPtr->m_tuple_location.m_page_idx;
  if (regTabPtr->var_sized_record) {
    ljam();
    num_var_attr= regTabPtr->no_var_attr;
    if (!(req_struct->m_tuple_ptr->m_header_bits & Tuple_header::CHAINED_ROW))
    {
      ljam();
      var_data_ptr= req_struct->m_tuple_ptr->m_data+regTabPtr->var_offset;
      req_struct->var_page_ptr.i = req_struct->fix_page_ptr.i;
      req_struct->var_page_ptr.p = (Var_page*)req_struct->fix_page_ptr.p;
      req_struct->vp_index= page_index;
    } else {
      Uint32 var_link= req_struct->m_tuple_ptr->m_data[regTabPtr->var_offset];
      ljam();
      
      Uint32 vp_index= var_link & MAX_TUPLES_PER_PAGE;
      PagePtr var_page_ptr;
      var_page_ptr.i= var_link >> MAX_TUPLES_BITS;
      ptrCheckGuard(var_page_ptr, cnoOfPage, cpage);
      
      req_struct->vp_index= vp_index;
      req_struct->var_page_ptr.i= var_page_ptr.i;
      req_struct->var_page_ptr.p= (Var_page*)var_page_ptr.p;
      
      var_data_ptr= ((Var_page*)var_page_ptr.p)->get_ptr(vp_index);
      req_struct->fix_var_together= false;
    }
    var_data_start= &var_data_ptr[regTabPtr->var_array_wsize];
    req_struct->var_len_array= (Uint16*)var_data_ptr;
    req_struct->var_data_start= var_data_start;
    var_data_wsize= init_var_pos_array(req_struct->var_len_array,
                                       &req_struct->var_pos_array[0],
                                       num_var_attr);
    req_struct->var_data_end= &var_data_start[var_data_wsize];
  } 
}


bool
Dbtup::compress_var_sized_part_after_update(KeyReqStruct *req_struct,
                                            Operationrec* const regOperPtr,
                                            Fragrecord* const regFragPtr,
                                            Tablerec* const regTabPtr)
{
  Uint32 entry_len, old_var_len, new_size, total_size;
  Uint32* used_var_data_start= req_struct->var_data_start;
  total_size= calculate_total_var_size(req_struct->var_len_array,
                                       regTabPtr->no_var_attr);
  entry_len= req_struct->var_page_ptr.p->get_entry_len(req_struct->vp_index);
  if (req_struct->fix_var_together) {
    ljam();
    old_var_len= entry_len -
                  (regTabPtr->tupheadsize + regTabPtr->var_array_wsize);
  } else {
    ljam();
    old_var_len= entry_len - regTabPtr->var_array_wsize;
  }
  if (total_size > old_var_len) {
    ljam();
    /**
     * The new total size of the variable part is greater than it was before
     * the update. We will need to increase the size of the record or split
     * it into a fixed part and a variable part.
     */
    if (! handle_growth_after_update(req_struct,
                                     regFragPtr,
                                     regTabPtr,
                                     (total_size - old_var_len))) {
      ljam();
      return false;
    }
  } else if (total_size < old_var_len) {
    ljam();
    /**
     * The new total size is smaller than what it was before we started.
     * In one case we can shrink immediately and this is after an initial
     * insert since we allocate in this case a full sized tuple and there
     * is no problem in shrinking this already before committing.
     *
     * For all other cases we need to keep the space to ensure that we
     * can safely abort (which means in this case to grow back to
     * original size). Thus shrink cannot be done before commit occurs
     * in those cases.
     */
    if (regOperPtr->op_struct.op_type == ZINSERT &&
        regOperPtr->prevActiveOp == RNIL &&
        regOperPtr->nextActiveOp == RNIL) {
      ljam();
      new_size= entry_len - (old_var_len - total_size);
      shrink_entry(regFragPtr,
                   req_struct->var_page_ptr.p,
                   req_struct->vp_index,
                   new_size);
    }
  }
  reset_req_struct_data(regTabPtr,
                        req_struct,
                        regOperPtr->m_tuple_location.m_page_idx);
  copy_back_var_attr(req_struct, regTabPtr, used_var_data_start);
  return true;
}

void
Dbtup::reset_req_struct_data(Tablerec* const regTabPtr,
                             KeyReqStruct* req_struct,
                             Uint32 fix_index)
{
  Var_page *var_page_ptr, *fix_page_ptr;
  Uint32 vp_index;

  fix_page_ptr= (Var_page*)req_struct->fix_page_ptr.p;
  var_page_ptr= req_struct->var_page_ptr.p;
  vp_index= req_struct->vp_index;

  req_struct->m_tuple_ptr= (Tuple_header*)fix_page_ptr->get_ptr(fix_index);
  
  Uint32 vp_len= var_page_ptr->get_entry_len(vp_index);
  
  Uint32 *var_ptr;
  if (req_struct->fix_var_together) 
  {
    ljam();
    var_ptr= req_struct->m_tuple_ptr->m_data+regTabPtr->var_offset;
  }
  else
  {
    var_ptr= var_page_ptr->get_ptr(vp_index);
  }

  req_struct->var_len_array= (Uint16*)(var_ptr);
  req_struct->var_data_start= var_ptr+regTabPtr->var_array_wsize;
  req_struct->var_data_end= var_ptr+regTabPtr->var_array_wsize+vp_len;
}

void
Dbtup::copy_back_var_attr(KeyReqStruct *req_struct,
                          Tablerec* const regTabPtr,
                          Uint32 *source_rec)
{
  Uint32 i, dest_index, vpos_index, byte_size, word_size, num_var_attr;
  Uint32 *dest_rec, max_var_size, entry_len;
  Uint32 total_word_size= 0;

#ifdef VM_TRACE
  entry_len= req_struct->var_page_ptr.p->get_entry_len(req_struct->vp_index);
  if (req_struct->fix_var_together) {
    ljam();
    max_var_size= entry_len - (regTabPtr->tupheadsize +
                               regTabPtr->var_array_wsize);
  } else {
    ljam();
    max_var_size= entry_len - regTabPtr->var_array_wsize;
  }
#endif
  dest_rec= req_struct->var_data_start;
  num_var_attr= regTabPtr->no_var_attr;
  ljam();
  for (i= 0; i < num_var_attr; i++) {
    dest_index= total_word_size;
    byte_size= req_struct->var_len_array[i];
    vpos_index= req_struct->var_pos_array[i];
    word_size= convert_byte_to_word_size(byte_size);
    total_word_size+= word_size;
    req_struct->var_pos_array[i]= total_word_size;
    MEMCOPY_NO_WORDS(&dest_rec[vpos_index],
                     &source_rec[dest_index],
                     word_size);
    ndbassert((vpos_index + word_size) <= max_var_size);
  }
  ndbassert(total_word_size <= max_var_size);
  req_struct->var_pos_array[num_var_attr]= total_word_size;
  req_struct->var_data_end= &req_struct->var_data_start[total_word_size];
}


void
Dbtup::copy_out_var_attr(KeyReqStruct *req_struct,
                         Tablerec* const regTabPtr)
{
  Uint32 i, source_index, byte_size, vpos_index, word_size, last_pos_array;
  Uint32 num_var_attr= regTabPtr->no_var_attr;
  Uint16 copy_pos_array[MAX_ATTRIBUTES_IN_TABLE + 1];
  init_var_len_array(&copy_pos_array[0], regTabPtr);
  init_var_pos_array(&copy_pos_array[0],
                     &copy_pos_array[0],
                     regTabPtr->no_var_attr);
  
  Uint32 *source_rec= req_struct->var_data_start;
  Uint32 *dest_rec= &ctemp_var_record[0];
  Uint32 total_word_size= 0;
  ljam();
  for (i= 0; i < num_var_attr; i++) {
    source_index= total_word_size;
    byte_size= req_struct->var_len_array[i];
    vpos_index= copy_pos_array[i];
    word_size= convert_byte_to_word_size(byte_size);
    total_word_size+= word_size;
    req_struct->var_pos_array[i]= copy_pos_array[i];
    MEMCOPY_NO_WORDS(&dest_rec[source_index],
                     &source_rec[vpos_index],
                     word_size);
  }
  last_pos_array= copy_pos_array[num_var_attr];
  req_struct->var_data_start= dest_rec;
  req_struct->var_data_end= &dest_rec[last_pos_array];
  req_struct->var_part_updated= true;
  req_struct->var_pos_array[num_var_attr]= last_pos_array;
}


Uint32
Dbtup::calculate_total_var_size(Uint16* var_len_array,
                                Uint32 num_var_attr)
{
  Uint32 i, byte_size, word_size, total_size;
  total_size= 0;
  for (i= 0; i < num_var_attr; i++) {
    byte_size= var_len_array[i];
    word_size= convert_byte_to_word_size(byte_size);
    total_size+= word_size;
  }
  return total_size;
}

Uint32
Dbtup::init_var_pos_array(Uint16* var_len_array,
                          Uint16* var_pos_array,
                          Uint32 num_var_attr)
{
  Uint32 i, real_len, word_len;
  Uint32 curr_pos= 0;
  for (i= 0, curr_pos= 0; i < num_var_attr; i++) {
    real_len= var_len_array[i];
    var_pos_array[i]= curr_pos;
    word_len= convert_byte_to_word_size(real_len);
    curr_pos+= word_len;
  }
  var_pos_array[num_var_attr]= curr_pos;
  return curr_pos;
}

void
Dbtup::init_var_len_array(Uint16 *var_len_array, Tablerec *tab_ptr)
{
  Uint32 array_ind= 0;
  Uint32 attr_descr, i;
  Uint32 no_of_attr= tab_ptr->noOfAttr;
  Uint32 descr_start= tab_ptr->tabDescriptor;
  TableDescriptor *tab_descr= &tableDescriptor[descr_start];
  ndbrequire(descr_start + (no_of_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);
  for (i= 0; i < no_of_attr; i++) {
    attr_descr= tab_descr[i * ZAD_SIZE].tabDescr;
    if (AttributeDescriptor::getArrayType(attr_descr) == 0) {
      Uint32 bits_used= AttributeDescriptor::getArraySize(attr_descr) *
                        (1 << AttributeDescriptor::getSize(attr_descr));
      Uint32 no_attr_bytes= ((bits_used + 7) >> 3);
      var_len_array[array_ind++]= no_attr_bytes;
    }
  }
}

#endif

/*
  Allocator for variable sized segments
  Part of the external interface for variable sized segments

  This method is used to allocate and free variable sized tuples and
  parts of tuples. This part can be used to implement variable sized
  attributes without wasting memory. It can be used to support small
  BLOB's attached to the record. It can also be used to support adding
  and dropping attributes without the need to copy the entire table.

  SYNOPSIS
    frag_ptr         A pointer to the fragment description
    tab_ptr          A pointer to the table description
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
Uint32* Dbtup::alloc_var_rec(Fragrecord* const frag_ptr,
			     Tablerec* const tab_ptr,
			     Uint32 alloc_size,
			     Local_key* key,
			     Uint32 * out_frag_page_id,
			     Uint32 base)
{
  Var_page* page_header;
  PagePtr page_ptr;
  page_ptr.i= get_alloc_page(frag_ptr, (alloc_size + 1));
  if (page_ptr.i == RNIL) { 
    ljam();
    if ((page_ptr.i= getEmptyPage(frag_ptr)) == RNIL) {
      ljam();
      return 0;
    }
    ptrCheckGuard(page_ptr, cnoOfPage, cpage);
    page_header= (Var_page*)page_ptr.p;
    page_header->init();
    insert_free_page(frag_ptr, page_header, MAX_FREE_LIST - 1);
    /*
     * Tup scan and index build check ZEMPTY_MM to skip un-init()ed
     * page.  Change state here.  For varsize it means "page in use".
     */
    page_ptr.p->page_state = ZTH_MM_FREE;
  } else {
    ptrCheckGuard(page_ptr, cnoOfPage, cpage);
    ljam();
    page_header= (Var_page*)page_ptr.p;
  }
  Uint32 idx= page_header->alloc_record(alloc_size, 
					(Var_page*)ctemp_page, base);
  
  key->m_page_no= page_ptr.i;
  key->m_page_idx= idx;
  *out_frag_page_id= page_header->frag_page_id;
  update_free_page_list(frag_ptr, page_header);
  return page_header->get_ptr(idx);
}

/*
  Deallocator for variable sized segments
  Part of the external interface for variable sized segments

  SYNOPSIS
    frag_ptr         A pointer to the fragment description
    tab_ptr          A pointer to the table description
    signal           The signal object to be used if a signal needs to
                     be sent
    page_ptr         A reference to the page of the variable sized
                     segment
    free_page_index  Page index on page of variable sized segment
                     which is freed
  RETURN VALUES
    Returns true if deallocation was successful otherwise false
*/
void
Dbtup::free_var_part(Fragrecord* frag_ptr, Tablerec* tab_ptr, 
		     Var_part_ref ref, Uint32 chain)
{
  Local_key tmp;
  PagePtr pagePtr;
  tmp.m_page_idx= ref.m_ref & MAX_TUPLES_PER_PAGE;
  pagePtr.i= tmp.m_page_no= ref.m_ref >> MAX_TUPLES_BITS;
  
  ptrCheckGuard(pagePtr, cnoOfPage, cpage);
  free_var_part(frag_ptr, tab_ptr, &tmp, (Var_page*)pagePtr.p, chain);
}

void Dbtup::free_var_part(Fragrecord* const frag_ptr,
			  Tablerec* const tab_ptr,
			  Local_key* key,
			  Var_page* const page_header,
			  Uint32 chain)
{
  
  Uint32 page_idx= key->m_page_idx;
  page_header->free_record(page_idx, chain);
  
  ndbassert(page_header->free_space <= Var_page::DATA_WORDS);
  if (page_header->free_space == Var_page::DATA_WORDS - 1)
  {
    ljam();
    /*
     This code could be used when we release pages.
    remove_free_page(signal,frag_ptr,page_header,page_header->list_index);
    return_empty_page(frag_ptr, page_header);
    */
    update_free_page_list(frag_ptr, page_header);
  } else {
    ljam();
    update_free_page_list(frag_ptr, page_header);
  }
  return;
}


#if 0
/*
  This method is called whenever the variable part has been updated and
  has grown beyond its original size. This means that more space needs to
  be allocated to the record. If possible this space should be in the
  same page but we might have to allocate more space in a new page.
  In the case of a new page we must still keep the old page and the
  page index since this is the entrance to the record. In this case the
  record might have to be split into a fixed part and a variable part.

  This routine uses cinBuffer as temporary copy buffer. This is no longer
  used since it contains the interpreted program to use in the update
  and this has completed when this function is called.

  SYNOPSIS
  req_struct        The structure for temporary content
  signal            The signal object
  regOperPtr        The operation record
  regFragPtr        The fragment record
  regTabPtr         The table record

  RETURN VALUES
  bool              false if failed due to lack of memory
 */
bool
Dbtup::handle_growth_after_update(KeyReqStruct* req_struct,
                                  Fragrecord* const regFragPtr,
                                  Tablerec* const regTabPtr,
                                  Uint32 growth_len)
{
  Uint32 vp_index, alloc_size, entry_len, curr_var_len;
  Uint32 new_vp_index, new_vp_offset, new_page_ref;
  Uint32 *copy_record= &cinBuffer[0];
  Ptr<Var_page> var_page= req_struct->var_page_ptr;
  Var_page* page_header= var_page.p;
  vp_index= req_struct->vp_index;
  entry_len= var_page.p->get_entry_len(vp_index);
  if (page_header->free_space >= growth_len) {
    /**
     * We will be able to handle the growth without changing the page
     * and page index.
     */
    if (page_header->largest_frag_size() >= entry_len + growth_len) {
      ljam();
      /**
       * In this case we need to copy the entry to the free space area of
       * the page, it is not necessary to reorganise the page.
       */
      MEMCOPY_NO_WORDS(page_header->get_free_space_ptr(),
                       page_header->get_ptr(vp_index),
                       entry_len);
      page_header->set_entry_offset(vp_index, page_header->insert_pos);
      page_header->insert_pos+= entry_len;
    } else {
      ljam();
      /**
       * In this case we need to reorganise the page to fit. To ensure we
       * don't complicate matters we make a little trick here where we
       * fool the reorg_page to avoid copying the entry at hand and copy
       * that separately at the end. This means we need to copy it out of
       * the page before reorg_page to save the entry contents.
       */
      MEMCOPY_NO_WORDS(copy_record,
                       page_header->get_ptr(vp_index),
                       entry_len);
      page_header->set_entry_len(vp_index, 0);
      page_header->free_space+= entry_len;
      reorg_page(page_header);
      MEMCOPY_NO_WORDS(page_header->get_free_space_ptr(),
                       copy_record,
                       entry_len);
      page_header->set_entry_offset(vp_index, page_header->insert_pos);
      growth_len+= entry_len;
    }
    grow_entry(regFragPtr,
	       page_header,
	       vp_index,
               growth_len);
    return true;
  } else {
    /**
     * It is necessary to allocate a segment from a new page.
     */
    if (req_struct->fix_var_together) {
      ljam();
      alloc_size= (entry_len + growth_len) - regTabPtr->tupheadsize;
      curr_var_len= alloc_size - regTabPtr->var_array_wsize;
    } else {
      ljam();
      curr_var_len= entry_len - regTabPtr->var_array_wsize;
      alloc_size= entry_len + growth_len;
    }
    Uint32* ptr, frag_page_id;
    Local_key key;
    if ((ptr= alloc_var_rec(regFragPtr,
			    regTabPtr,
			    alloc_size,
			    &key, &frag_page_id)) == 0)
    {
      /**
       * No space existed for this growth. We need to abort the update.
       */
      ljam();
      terrorCode= ZMEM_NOMEM_ERROR;
      return false;
    }

    /*
     * I need to be careful to copy the var_len_array before freeing it.
     * The data part will be copied by copy_back_var_attr immediately
     * after returning from this method.
     * The updated var part is always in ctemp_var_record since I can
     * never arrive here after a first insert. Thus no danger of the
     * var part written being released.
     */
    MEMCOPY_NO_WORDS(ptr,
                     req_struct->var_len_array,
                     regTabPtr->var_array_wsize);
    req_struct->var_len_array= (Uint16*)ptr;
    if (! req_struct->fix_var_together) {
      ljam();
      /*
       * We need to deallocate the old variable part. This new one will
       * remain the variable part even if we abort the transaction.
       * We don't keep multiple references to the variable parts.
       * The copy data for abort is still kept in the copy record.
       */
      free_separate_var_part(regFragPtr, regTabPtr, req_struct->m_tuple_ptr);
    } else {
      ljam();
      req_struct->fix_var_together= false;
    }
    page_header= (Var_page*)var_page.p;
    new_page_ref= (key.m_page_no << MAX_TUPLES_BITS) + key.m_page_idx;
    req_struct->m_tuple_ptr->m_data[regTabPtr->var_offset] = new_page_ref;
    Uint32 bits= req_struct->m_tuple_ptr->m_header_bits;
    req_struct->m_tuple_ptr->m_header_bits |= Tuple_header::CHAINED_ROW;
    req_struct->var_page_ptr= var_page;
    req_struct->vp_index= key.m_page_idx;
  }
  return true;
}
#endif


/* ------------------------------------------------------------------------ */
// Get a page from one of free lists. If the desired free list is empty we
// try with the next until we have tried all possible lists.
/* ------------------------------------------------------------------------ */
Uint32 Dbtup::get_alloc_page(Fragrecord* const frag_ptr, Uint32 alloc_size)
{
  Uint32 i, start_index, loop_count= 0;
  PagePtr page_ptr;

  start_index= calculate_free_list_impl(alloc_size);
  if (start_index == (MAX_FREE_LIST - 1)) {
    ljam();
  } else {
    ljam();
    ndbrequire(start_index < (MAX_FREE_LIST - 1));
    start_index++;
  }
  for (i= start_index; i < MAX_FREE_LIST; i++) {
    ljam();
    if (frag_ptr->free_var_page_array[i] != RNIL) {
      ljam();
      return frag_ptr->free_var_page_array[i];
    }
  }
  ndbrequire(start_index > 0);
  i= start_index - 1;
  page_ptr.i= frag_ptr->free_var_page_array[i];
  while ((page_ptr.i != RNIL) && (loop_count++ < 16)) {
    ljam();
    ptrCheckGuard(page_ptr, cnoOfPage, cpage);
    Var_page* page_header= (Var_page*)page_ptr.p;
    if (page_header->free_space >= alloc_size) {
      ljam();
      return page_ptr.i;
    }
    page_ptr.i= page_header->next_page;
  }
  return RNIL;
}


/* ------------------------------------------------------------------------ */
// Check if the page needs to go to a new free page list.
/* ------------------------------------------------------------------------ */
void Dbtup::update_free_page_list(Fragrecord* const frag_ptr,
                                  Var_page* page_header)
{
  Uint32 free_space, list_index;
  free_space= page_header->free_space;
  list_index= page_header->list_index;
  if ((free_space < c_min_list_size[list_index]) ||
      (free_space > c_max_list_size[list_index])) {
    Uint32 new_list_index= calculate_free_list_impl(free_space);
    if (list_index != MAX_FREE_LIST) {
      ljam();
      /*
       * Only remove it from its list if it is in a list
       */
      remove_free_page(frag_ptr, page_header, list_index);
    }
    if (free_space < c_min_list_size[new_list_index]) {
      /*
      We have not sufficient amount of free space to put it into any
      free list. Thus the page will not be available for new inserts.
      This can only happen for the free list with least guaranteed free space.
      */
      ljam();
      ndbrequire(new_list_index == 0);
      page_header->list_index= MAX_FREE_LIST;
    } else {
      ljam();
      insert_free_page(frag_ptr, page_header, new_list_index);
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
    ljam();
    if (free_space_size <= c_max_list_size[i]) {
      ljam();
      return i;
    }
  }
  ndbrequire(false);
  return 0;
}


/* ------------------------------------------------------------------------ */
// Remove a page from its current free list
/* ------------------------------------------------------------------------ */
void Dbtup::remove_free_page(Fragrecord* frag_ptr,
                             Var_page* page_header,
                             Uint32 index)
{
  Var_page* tmp_page_header;
  if (page_header->prev_page == RNIL) {
    ljam();
    ndbassert(index < MAX_FREE_LIST);
    frag_ptr->free_var_page_array[index]= page_header->next_page;
  } else {
    ljam();
    PagePtr prev_page_ptr;
    prev_page_ptr.i= page_header->prev_page;
    ptrCheckGuard(prev_page_ptr, cnoOfPage, cpage);
    tmp_page_header= (Var_page*)prev_page_ptr.p;
    tmp_page_header->next_page= page_header->next_page;
  }
  if (page_header->next_page != RNIL) {
    ljam();
    PagePtr next_page_ptr;
    next_page_ptr.i= page_header->next_page;
    ptrCheckGuard(next_page_ptr, cnoOfPage, cpage);
    tmp_page_header= (Var_page*) next_page_ptr.p;
    tmp_page_header->prev_page= page_header->prev_page;
  }
}


/* ------------------------------------------------------------------------ */
// Insert a page into a free list on the fragment
/* ------------------------------------------------------------------------ */
void Dbtup::insert_free_page(Fragrecord* frag_ptr,
                             Var_page* page_header,
                             Uint32 index)
{
  Var_page* tmp_page_header;
  Uint32 current_head= frag_ptr->free_var_page_array[index]; 
  Uint32 pagePtrI = page_header->physical_page_id;
  page_header->next_page= current_head;
  ndbassert(index < MAX_FREE_LIST);
  frag_ptr->free_var_page_array[index]= pagePtrI;
  page_header->prev_page= RNIL;
  page_header->list_index= index;
  if (current_head != RNIL) {
    ljam();
    PagePtr head_page_ptr;
    head_page_ptr.i= current_head;
    ptrCheckGuard(head_page_ptr, cnoOfPage, cpage);
    tmp_page_header= (Var_page*)head_page_ptr.p;
    tmp_page_header->prev_page= pagePtrI;
  }
}

