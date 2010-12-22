/* Copyright (C) 2000-2006 MySQL AB

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

/**
  @file

  @brief
  join cache optimizations

  @defgroup Query_Optimizer  Query Optimizer
  @{
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"
#include "opt_subselect.h"

#define NO_MORE_RECORDS_IN_BUFFER  (uint)(-1)


/*****************************************************************************
 *  Join cache module
******************************************************************************/

/* 
  Fill in the descriptor of a flag field associated with a join cache    

  SYNOPSIS
    add_field_flag_to_join_cache()
      str           position in a record buffer to copy the field from/to
      length        length of the field 
      field  IN/OUT pointer to the field descriptor to fill in 

  DESCRIPTION
    The function fill in the descriptor of a cache flag field to which
    the parameter 'field' points to. The function uses the first two
    parameters to set the position in the record buffer from/to which 
    the field value is to be copied and the length of the copied fragment. 
    Before returning the result the function increments the value of
    *field by 1.
    The function ignores the fields 'blob_length' and 'ofset' of the
    descriptor.

  RETURN VALUE
    the length of the field  
*/

static
uint add_flag_field_to_join_cache(uchar *str, uint length, CACHE_FIELD **field)
{
  CACHE_FIELD *copy= *field;
  copy->str= str;
  copy->length= length;
  copy->type= 0;
  copy->field= 0;
  copy->referenced_field_no= 0;
  (*field)++;
  return length;    
}


/* 
  Fill in the descriptors of table data fields associated with a join cache    

  SYNOPSIS
    add_table_data_fields_to_join_cache()
      tab              descriptors of fields from this table are to be filled
      field_set        descriptors for only these fields are to be created
      field_cnt IN/OUT     counter of data fields  
      descr  IN/OUT        pointer to the first descriptor to be filled
      field_ptr_cnt IN/OUT counter of pointers to the data fields
      descr_ptr IN/OUT     pointer to the first pointer to blob descriptors 

  DESCRIPTION
    The function fills in the descriptors of cache data fields from the table
    'tab'. The descriptors are filled only for the fields marked in the 
    bitmap 'field_set'. 
    The function fills the descriptors starting from the position pointed
    by 'descr'. If an added field is of a BLOB type then a pointer to the 
    its descriptor is added to the array descr_ptr.   
    At the return 'descr' points to the position after the last added
    descriptor  while 'descr_ptr' points to the position right after the
    last added pointer.  

  RETURN VALUE
    the total length of the added fields  
*/

static
uint add_table_data_fields_to_join_cache(JOIN_TAB *tab, 
                                         MY_BITMAP *field_set,
                                         uint *field_cnt, 
                                         CACHE_FIELD **descr,
                                         uint *field_ptr_cnt,
                                         CACHE_FIELD ***descr_ptr)
{
  Field **fld_ptr;
  uint len= 0;
  CACHE_FIELD *copy= *descr;
  CACHE_FIELD **copy_ptr= *descr_ptr;
  uint used_fields= bitmap_bits_set(field_set);
  for (fld_ptr= tab->table->field; used_fields; fld_ptr++)
  {
    if (bitmap_is_set(field_set, (*fld_ptr)->field_index))
    {
      len+= (*fld_ptr)->fill_cache_field(copy);
      if (copy->type == CACHE_BLOB)
      {
        *copy_ptr= copy;
        copy_ptr++;
        (*field_ptr_cnt)++;
      }
      copy->field= *fld_ptr;
      copy->referenced_field_no= 0;
      copy++;
      (*field_cnt)++;
      used_fields--;
    }
  }
  *descr= copy;
  *descr_ptr= copy_ptr;
  return len;
}

/* 
  Get the next table whose records are stored in the join buffer of this cache

  SYNOPSIS
    get_next_table()
      tab     the table for which the next table is to be returned

  DESCRIPTION
    For a given table whose records are stored in this cache the function
    returns the next such table if there is any.
    The function takes into account that the tables whose records are
    are stored in the same cache now can interleave with tables from
    materialized semijoin subqueries.

  TODO
    This function should be modified/simplified after the new code for
     materialized semijoins is merged.

  RETURN
    The next join table whose records are stored in the buffer of this cache
    if such table exists, 0 - otherwise
*/

JOIN_TAB *JOIN_CACHE::get_next_table(JOIN_TAB *tab)
{
  
  if (++tab == join_tab)
    return NULL;
  if (join_tab->first_sjm_sibling)
    return tab;
  uint i= tab-join->join_tab;
  while (sj_is_materialize_strategy(join->best_positions[i].sj_strategy) &&
         i < join->tables)
    i+= join->best_positions[i].n_sj_tables;
  return join->join_tab+i < join_tab ? join->join_tab+i : NULL; 
}


/* 
  Determine different counters of fields associated with a record in the cache  

  SYNOPSIS
    calc_record_fields()

  DESCRIPTION
    The function counts the number of total fields stored in a record
    of the cache and saves this number in the 'fields' member. It also
    determines the number of flag fields and the number of blobs.
    The function sets 'with_match_flag' on if 'join_tab' needs a match flag
    i.e. if it is the first inner table of an outer join or a semi-join.  

  RETURN VALUE
    none 
*/

void JOIN_CACHE::calc_record_fields()
{
  JOIN_TAB *tab = prev_cache ? prev_cache->join_tab :
                                (join_tab->first_sjm_sibling ?
			         join_tab->first_sjm_sibling :
			         join->join_tab+join->const_tables);
  tables= join_tab-tab;

  fields= 0;
  blobs= 0;
  flag_fields= 0;
  data_field_count= 0;
  data_field_ptr_count= 0;
  referenced_fields= 0;

  for ( ; tab ; tab= get_next_table(tab))
  {	    
    tab->calc_used_field_length(FALSE);
    flag_fields+= test(tab->used_null_fields || tab->used_uneven_bit_fields);
    flag_fields+= test(tab->table->maybe_null);
    fields+= tab->used_fields;
    blobs+= tab->used_blobs;

    fields+= tab->check_rowid_field();
  }
  if ((with_match_flag= join_tab->use_match_flag()))
    flag_fields++;
  fields+= flag_fields;
}


/* 
  Collect information on join key arguments  

  SYNOPSIS
    collect_info_on_key_args()

  DESCRIPTION
    The function traverses the ref expressions that are used to access the
    joined table join_tab. For each table 'tab' whose fields are to be stored
    in the join buffer of the cache the function finds the fields from 'tab'
    that occur in the ref expressions and marks these fields in the bitmap
    tab->table->tmp_set. The function counts the number of them stored
    in this cache and the total number of them stored in the previous caches
    and saves the results of the counting in 'local_key_arg_fields' and               'external_key_arg_fields' respectively.

  NOTES
    The function does not do anything if no key is used to join the records
    from join_tab.
    
  RETURN VALUE
    none 
*/  

void JOIN_CACHE::collect_info_on_key_args()
{
  JOIN_TAB *tab;
  JOIN_CACHE *cache;
  local_key_arg_fields= 0;
  external_key_arg_fields= 0;

  if (!is_key_access())
    return;

  TABLE_REF *ref= &join_tab->ref;
  cache= this;
  do
  {
    for (tab= cache->join_tab-cache->tables; tab ;
         tab= cache->get_next_table(tab))
    { 
      uint key_args;
      bitmap_clear_all(&tab->table->tmp_set);
      for (uint i= 0; i < ref->key_parts; i++)
      {
        Item *ref_item= ref->items[i]; 
        if (!(tab->table->map & ref_item->used_tables()))
	  continue;
	 ref_item->walk(&Item::add_field_to_set_processor, 1,
                        (uchar *) tab->table);
      }
      if ((key_args= bitmap_bits_set(&tab->table->tmp_set)))
      {
        if (cache == this)
          local_key_arg_fields+= key_args;
        else
          external_key_arg_fields+= key_args;
      }
    }
    cache= cache->prev_cache;
  } 
  while (cache);

  return;
}


/* 
  Allocate memory for descriptors and pointers to them associated with the cache  

  SYNOPSIS
    alloc_fields()

  DESCRIPTION
    The function allocates memory for the array of fields descriptors
    and the array of pointers to the field descriptors used to copy
    join record data from record buffers into the join buffer and
    backward. Some pointers refer to the field descriptor associated
    with previous caches. They are placed at the beginning of the array
    of pointers and its total number is stored in external_key_arg_fields.
    The pointer of the first array is assigned to field_descr and the number
    of the elements in it is precalculated by the function calc_record_fields. 
    The allocated arrays are adjacent.
  
  NOTES
    The memory is allocated in join->thd->memroot

  RETURN VALUE
    pointer to the first array  
*/

int JOIN_CACHE::alloc_fields()
{
  uint ptr_cnt= external_key_arg_fields+blobs+1;
  uint fields_size= sizeof(CACHE_FIELD)*fields;
  field_descr= (CACHE_FIELD*) sql_alloc(fields_size +
                                        sizeof(CACHE_FIELD*)*ptr_cnt);
  blob_ptr= (CACHE_FIELD **) ((uchar *) field_descr + fields_size);
  return (field_descr == NULL);
}  


/* 
  Create descriptors of the record flag fields stored in the join buffer 

  SYNOPSIS
    create_flag_fields()

  DESCRIPTION
    The function creates descriptors of the record flag fields stored
    in the join buffer. These are descriptors for:
    - an optional match flag field,
    - table null bitmap fields, 
    - table null row fields.
    The match flag field is created when 'join_tab' is the first inner
    table of an outer join our a semi-join. A null bitmap field is
    created for any table whose fields are to be stored in the join
    buffer if at least one of these fields is nullable or is a BIT field
    whose bits are partially stored with null bits. A null row flag
    is created for any table assigned to the cache if it is an inner
    table of an outer join.
    The descriptor for flag fields are placed one after another at the
    beginning of the array of field descriptors 'field_descr' that
    contains 'fields' elements. If there is a match flag field the 
    descriptor for it is always first in the sequence of flag fields.
    The descriptors for other flag fields can follow in an arbitrary
    order. 
    The flag field values follow in a record stored in the join buffer
    in the same order as field descriptors, with the match flag always
    following first.
    The function sets the value of 'flag_fields' to the total number
    of the descriptors created for the flag fields.
    The function sets the value of 'length' to the total length of the
    flag fields.
  
  RETURN VALUE
    none
*/

void JOIN_CACHE::create_flag_fields()
{
  CACHE_FIELD *copy;
  JOIN_TAB *tab;

  copy= field_descr;

  length=0;

  /* If there is a match flag the first field is always used for this flag */ 
  if (with_match_flag)
    length+= add_flag_field_to_join_cache((uchar*) &join_tab->found,
                                          sizeof(join_tab->found),
	                                  &copy);

  /* Create fields for all null bitmaps and null row flags that are needed */
  for (tab= join_tab-tables; tab; tab= get_next_table(tab))
  {
    TABLE *table= tab->table;

    /* Create a field for the null bitmap from table if needed */
    if (tab->used_null_fields || tab->used_uneven_bit_fields)			    
      length+= add_flag_field_to_join_cache(table->null_flags,
                                            table->s->null_bytes,
                                            &copy);
 
    /* Create table for the null row flag if needed */
    if (table->maybe_null)
      length+= add_flag_field_to_join_cache((uchar*) &table->null_row,
                                            sizeof(table->null_row),
                                            &copy);
  }

  /* Theoretically the new value of flag_fields can be less than the old one */   
  flag_fields= copy-field_descr;
}


/* 
  Create descriptors of the fields used to build access keys to the joined table

  SYNOPSIS
    create_key_arg_fields()

  DESCRIPTION
    The function creates descriptors of the record fields stored in the join
    buffer that are used to build access keys to the joined table. These
    fields are put into the buffer ahead of other records fields stored in
    the buffer. Such placement helps to optimize construction of access keys.
    For each field that is used to build access keys to the joined table but
    is stored in some other join cache buffer the function saves a pointer
    to the the field descriptor. The array of such pointers are placed in the
    the join cache structure just before the array of pointers to the
    blob fields blob_ptr.
    Any field stored in a join cache buffer that is used to construct keys
    to access tables associated with other join caches is called a referenced
    field. It receives a unique number that is saved by the function in the
    member 'referenced_field_no' of the CACHE_FIELD descriptor for the field.
    This number is used as index to the array of offsets to the referenced
    fields that are saved and put in the join cache buffer after all record
    fields.
    The function also finds out whether that the keys to access join_tab
    can be considered as embedded and, if so, sets the flag 'use_emb_key' in
    this join cache appropriately. 
     
  NOTES.
    When a key to access the joined table 'join_tab' is constructed the array
    of pointers to the field descriptors for the external fields is looked
    through. For each of this pointers we find out in what previous key cache
    the referenced field is stored. The value of 'referenced_field_no'
    provides us with the index into the array of offsets for referenced 
    fields stored in the join cache. The offset read by the the index allows
    us to read the field without reading all other fields of the record 
    stored the join cache buffer. This optimizes the construction of keys
    to access 'join_tab' when some key arguments are stored in the previous
    join caches.  

  NOTES
    The function does not do anything if no key is used to join the records
    from join_tab.
 
  RETURN VALUE
    none
*/
void JOIN_CACHE::create_key_arg_fields()
{
  JOIN_TAB *tab;
  JOIN_CACHE *cache;

  if (!is_key_access())
    return;

  /* 
    Save pointers to the cache fields in previous caches
    that  are used to build keys for this key access.
  */
  cache= this;
  uint ext_key_arg_cnt= external_key_arg_fields;
  CACHE_FIELD *copy;
  CACHE_FIELD **copy_ptr= blob_ptr;
  while (ext_key_arg_cnt)
  {
    cache= cache->prev_cache;
    for (tab= cache->join_tab-cache->tables; tab;
         tab= cache->get_next_table(tab))
    { 
      CACHE_FIELD *copy_end;
      MY_BITMAP *key_read_set= &tab->table->tmp_set;
      /* key_read_set contains the bitmap of tab's fields referenced by ref */ 
      if (bitmap_is_clear_all(key_read_set))
        continue;
      copy_end= cache->field_descr+cache->fields;
      for (copy= cache->field_descr+cache->flag_fields; copy < copy_end; copy++)
      {
        /*
          (1) - when we store rowids for DuplicateWeedout, they have
                copy->field==NULL
        */
        if (copy->field &&  // (1)
            copy->field->table == tab->table &&
            bitmap_is_set(key_read_set, copy->field->field_index))
        {
          *copy_ptr++= copy; 
          ext_key_arg_cnt--;
          if (!copy->referenced_field_no)
          {
            /* 
              Register the referenced field 'copy': 
              - set the offset number in copy->referenced_field_no,
              - adjust the value of the flag 'with_length',
              - adjust the values of 'pack_length' and 
                of 'pack_length_with_blob_ptrs'.
	    */
            copy->referenced_field_no= ++cache->referenced_fields;
            if (!cache->with_length)
            {
              cache->with_length= TRUE;
              uint sz= cache->get_size_of_rec_length();
              cache->base_prefix_length+= sz;
              cache->pack_length+= sz;
              cache->pack_length_with_blob_ptrs+= sz;
            }
	    cache->pack_length+= cache->get_size_of_fld_offset();
            cache->pack_length_with_blob_ptrs+= cache->get_size_of_fld_offset();
          }        
        }
      }
    } 
  }
  /* After this 'blob_ptr' shall not be be changed */ 
  blob_ptr= copy_ptr;
  
  /* Now create local fields that are used to build ref for this key access */
  copy= field_descr+flag_fields;
  for (tab= join_tab-tables; tab; tab= get_next_table(tab))
  {
    length+= add_table_data_fields_to_join_cache(tab, &tab->table->tmp_set,
                                                 &data_field_count, &copy,
                                                 &data_field_ptr_count, 
                                                 &copy_ptr);
  }

  use_emb_key= check_emb_key_usage();

  return;
}


/* 
  Create descriptors of all remaining data fields stored in the join buffer    

  SYNOPSIS
    create_remaining_fields()

  DESCRIPTION
    The function creates descriptors for all remaining data fields of a
    record from the join buffer. If the value returned by is_key_access() is
    false the function creates fields for all read record fields that
    comprise the partial join record joined with join_tab. Otherwise, 
    for each table tab, the set of the read fields for which the descriptors
    have to be added is determined as the difference between all read fields
    and and those for which the descriptors have been already created.
    The latter are supposed to be marked in the bitmap tab->table->tmp_set.
    The function increases the value of 'length' to the the total length of
    the added fields.
   
  NOTES
    If is_key_access() returns true the function modifies the value of
    tab->table->tmp_set for a each table whose fields are stored in the cache.
    The function calls the method Field::fill_cache_field to figure out
    the type of the cache field and the maximal length of its representation
    in the join buffer. If this is a blob field then additionally a pointer
    to this field is added as an element of the array blob_ptr. For a blob
    field only the size of the length of the blob data is taken into account.
    It is assumed that 'data_field_count' contains the number of descriptors
    for data fields that have been already created and 'data_field_ptr_count'
    contains the number of the pointers to such descriptors having been
    stored up to the moment.

  RETURN VALUE
    none 
*/

void JOIN_CACHE:: create_remaining_fields()
{
  JOIN_TAB *tab;
  bool all_read_fields= !is_key_access();
  CACHE_FIELD *copy= field_descr+flag_fields+data_field_count;
  CACHE_FIELD **copy_ptr= blob_ptr+data_field_ptr_count;

  for (tab= join_tab-tables; tab; tab= get_next_table(tab))
  {
    MY_BITMAP *rem_field_set;
    TABLE *table= tab->table;

    if (all_read_fields)
      rem_field_set= table->read_set;
    else
    {
      bitmap_invert(&table->tmp_set);
      bitmap_intersect(&table->tmp_set, table->read_set);
      rem_field_set= &table->tmp_set;
    }  

    length+= add_table_data_fields_to_join_cache(tab, rem_field_set,
                                                 &data_field_count, &copy,
                                                 &data_field_ptr_count,
                                                 &copy_ptr);
  
    /* SemiJoinDuplicateElimination: allocate space for rowid if needed */
    if (tab->keep_current_rowid)
    {
      copy->str= table->file->ref;
      copy->length= table->file->ref_length;
      copy->type= 0;
      copy->field= 0;
      copy->referenced_field_no= 0;
      length+= copy->length;
      data_field_count++;
      copy++;
    }
  }
}



/* 
  Calculate and set all cache constants      

  SYNOPSIS
    set_constants()

  DESCRIPTION
    The function calculates and set all precomputed constants that are used
    when writing records into the join buffer and reading them from it.
    It calculates the size of offsets of a record within the join buffer
    and of a field within a record. It also calculates the number of bytes
    used to store record lengths.
    The function also calculates the maximal length of the representation
    of record in the cache excluding blob_data. This value is used when
    making a dicision whether more records should be added into the join
    buffer or not.
  
  RETURN VALUE
    none 
*/

void JOIN_CACHE::set_constants()
{ 
  /* 
    Any record from a BKA cache is prepended with the record length.
    We use the record length when reading the buffer and building key values
    for each record. The length allows us not to read the fields that are
    not needed for keys.
    If a record has match flag it also may be skipped when the match flag
    is on. It happens if the cache is used for a semi-join operation or
    for outer join when the 'not exist' optimization can be applied.
    If some of the fields are referenced from other caches then
    the record length allows us to easily reach the saved offsets for
    these fields since the offsets are stored at the very end of the record.
    However at this moment we don't know whether we have referenced fields for
    the cache or not. Later when a referenced field is registered for the cache
    we adjust the value of the flag 'with_length'.
  */ 
  with_length= is_key_access() || 
               join_tab->is_inner_table_of_semi_join_with_first_match() ||
               join_tab->is_inner_table_of_outer_join();
  /* 
     At this moment we don't know yet the value of 'referenced_fields',
     but in any case it can't be greater than the value of 'fields'.
  */
  uint len= length + fields*sizeof(uint)+blobs*sizeof(uchar *) +
            (prev_cache ? prev_cache->get_size_of_rec_offset() : 0) +
            sizeof(ulong);
  buff_size= max(join->thd->variables.join_buff_size, 2*len);
  size_of_rec_ofs= offset_size(buff_size);
  size_of_rec_len= blobs ? size_of_rec_ofs : offset_size(len); 
  size_of_fld_ofs= size_of_rec_len;
  base_prefix_length= (with_length ? size_of_rec_len : 0) +
                      (prev_cache ? prev_cache->get_size_of_rec_offset() : 0);
  /* 
    The size of the offsets for referenced fields will be added later.
    The values of 'pack_length' and 'pack_length_with_blob_ptrs' are adjusted
    every time when the first reference to the referenced field is registered.
  */
  pack_length= (with_length ? size_of_rec_len : 0) +
               (prev_cache ? prev_cache->get_size_of_rec_offset() : 0) + 
               length;
  pack_length_with_blob_ptrs= pack_length + blobs*sizeof(uchar *);
}


/* 
  Get maximum total length of all affixes of a record in the join cache buffer

  SYNOPSIS
    get_record_max_affix_length()

  DESCRIPTION
    The function calculates the maximum possible total length of all affixes
    of a record in the join cache buffer, that is made of:
      - the length of all prefixes used in this cache,
      - the length of the match flag if it's needed
      - the total length of the maximum possible offsets to the fields of
        a record in the buffer.

  RETURN VALUE
    The maximum total length of all affixes of a record in the join buffer  
*/ 
     
uint JOIN_CACHE::get_record_max_affix_length()
{
  uint len= get_prefix_length() +
            test(with_match_flag) + 
            size_of_fld_ofs * data_field_count;
  return len;
}


/* 
  Get the minimum possible size of the cache join buffer 

  SYNOPSIS
    get_min_join_buffer_size()

  DESCRIPTION
    At the first its invocation for the cache the function calculates the
    minimum possible size of the join buffer of the cache. This value depends
    on the minimal number of records 'min_records' to be stored in the join
    buffer. The number is supposed to be determined by the procedure that 
    chooses the best access path to the joined table join_tab in the execution
    plan. After the calculation of the interesting size the function saves it
    in the field 'min_buff_size' in order to use it directly at the next     
    invocations of the function.

  NOTES
    Currently the number of minimal records is just set to 1.

  RETURN VALUE
    The minimal possible size of the join buffer of this cache 
*/

ulong JOIN_CACHE::get_min_join_buffer_size()
{
  if (!min_buff_size)
  {
    ulong len= 0;
    for (JOIN_TAB *tab= join_tab-tables; tab < join_tab; tab++)
      len+= tab->get_max_used_fieldlength();
    len+= get_record_max_affix_length() + get_max_key_addon_space_per_record();  
    ulong min_sz= len*min_records;
    ulong add_sz= 0;
    for (uint i=0; i < min_records; i++)
      add_sz+= join_tab_scan->aux_buffer_incr(i+1);
    avg_aux_buffer_incr= add_sz/min_records;
    min_sz+= add_sz;
    min_sz+= pack_length_with_blob_ptrs;
    min_buff_size= min_sz;
  }
  return min_buff_size;
}


/* 
  Get the maximum possible size of the cache join buffer 

  SYNOPSIS
    get_max_join_buffer_size()

  DESCRIPTION
    At the first its invocation for the cache the function calculates the
    maximum possible size of join buffer for the cache. This value does not
    exceed the estimate of the number of records 'max_records' in the partial
    join that joins tables from the first one through join_tab. This value
    is also capped off by the value of join_tab->join_buffer_size_limit, if it
    has been set a to non-zero value, and by the value of the system parameter 
    join_buffer_size - otherwise. After the calculation of the interesting size
    the function saves the value in the field 'max_buff_size' in order to use
    it directly at the next  invocations of the function.

  NOTES
    Currently the value of join_tab->join_buffer_size_limit is initialized
    to 0 and is never reset.

  RETURN VALUE
    The maximum possible size of the join buffer of this cache 
*/

ulong JOIN_CACHE::get_max_join_buffer_size()
{
  if (!max_buff_size)
  {
    ulong max_sz;
    ulong min_sz= get_min_join_buffer_size(); 
    ulong len= 0;
    for (JOIN_TAB *tab= join_tab-tables; tab < join_tab; tab++)
      len+= tab->get_used_fieldlength();
    len+= get_record_max_affix_length();
    avg_record_length= len;
    len+= get_max_key_addon_space_per_record() + avg_aux_buffer_incr;
    space_per_record= len;
    
    ulong limit_sz= join->thd->variables.join_buff_size;
    if (join_tab->join_buffer_size_limit)
      set_if_smaller(limit_sz, join_tab->join_buffer_size_limit);
    if (limit_sz / max_records > space_per_record)
      max_sz= space_per_record * max_records;
    else
      max_sz= limit_sz;
    max_sz+= pack_length_with_blob_ptrs;
    set_if_smaller(max_sz, limit_sz);
    set_if_bigger(max_sz, min_sz);
    max_buff_size= max_sz;
  }
  return max_buff_size;
}    
      

/* 
  Allocate memory for a join buffer      

  SYNOPSIS
    alloc_buffer()

  DESCRIPTION
    The function allocates a lump of memory for the cache join buffer. 
    Initially the function sets the size of the buffer buff_size equal to
    the value returned by get_max_join_buffer_size(). If the total size of
    the space intended to be used for the join buffers employed by the
    tables from the first one through join_tab exceeds the value of the
    system parameter join_buff_space_limit, then the function first tries
    to shrink the used buffers to make the occupied space fit the maximum
    memory allowed to be used for all join buffers in total. After
    this the function tries to allocate a join buffer for join_tab.
    If it fails to do so, it decrements the requested size of the join
    buffer, shrinks proportionally the join buffers used for the previous
    tables and tries to allocate a buffer for join_tab. In the case of a
    failure the function repeats its attempts with smaller and smaller
    requested sizes of the buffer, but not more than 4 times.
  
  RETURN VALUE
    0   if the memory has been successfully allocated
    1   otherwise
*/

int JOIN_CACHE::alloc_buffer()
{
  JOIN_TAB *tab;
  JOIN_CACHE *cache;
  ulonglong curr_buff_space_sz= 0;
  ulonglong curr_min_buff_space_sz= 0;
  ulonglong join_buff_space_limit=
    join->thd->variables.join_buff_space_limit;
  double partial_join_cardinality=  (join_tab-1)->get_partial_join_cardinality();
  buff= NULL;
  min_buff_size= 0;
  max_buff_size= 0;
  min_records= 1;
  max_records= partial_join_cardinality <= join_buff_space_limit ?
                 (ulonglong) partial_join_cardinality : join_buff_space_limit;
  set_if_bigger(max_records, 10);
  min_buff_size= get_min_join_buffer_size();
  buff_size= get_max_join_buffer_size();
  for (tab= join->join_tab+join->const_tables; tab <= join_tab; tab++)
  {
    cache= tab->cache;
    if (cache)
    {
      curr_min_buff_space_sz+= cache->get_min_join_buffer_size();
      curr_buff_space_sz+= cache->get_join_buffer_size();
    }
  }

  if (curr_min_buff_space_sz > join_buff_space_limit ||
      (curr_buff_space_sz > join_buff_space_limit &&
       join->shrink_join_buffers(join_tab, curr_buff_space_sz,
                                 join_buff_space_limit)))
    goto fail;
                               
  for (ulong buff_size_decr= (buff_size-min_buff_size)/4 + 1; ; )
  {
    ulong next_buff_size;

    if ((buff= (uchar*) my_malloc(buff_size, MYF(0))))
      break;

    next_buff_size= buff_size > buff_size_decr ? buff_size-buff_size_decr : 0;
    if (next_buff_size < min_buff_size ||
        join->shrink_join_buffers(join_tab, curr_buff_space_sz,
                                  curr_buff_space_sz-buff_size_decr))
      goto fail;
    buff_size= next_buff_size;

    curr_buff_space_sz= 0;
    for (tab= join->join_tab+join->const_tables; tab <= join_tab; tab++)
    {
      cache= tab->cache;
      if (cache)
        curr_buff_space_sz+= cache->get_join_buffer_size();
    } 
  }
  return 0;

fail:
  buff_size= 0;
  return 1;
}

 
/*
  Shrink the size if the cache join buffer in a given ratio

  SYNOPSIS
    shrink_join_buffer_in_ratio()
      n           nominator of the ratio to shrink the buffer in
      d           denominator if the ratio

  DESCRIPTION
    The function first deallocates the join buffer of the cache. Then
    it allocates a buffer that is (n/d) times smaller.
    
  RETURN VALUE
    FALSE   on success with allocation of the smaller join buffer 
    TRUE    otherwise       
*/

bool JOIN_CACHE::shrink_join_buffer_in_ratio(ulonglong n, ulonglong d)
{
  ulonglong next_buff_size;
  if (n < d)
    return FALSE;
  next_buff_size= (ulonglong) ((double) buff_size / n * d);
  set_if_bigger(next_buff_size, min_buff_size);
  buff_size= next_buff_size;
  return realloc_buffer();
}  


/*
  Reallocate the join buffer of a join cache
 
  SYNOPSIS
    realloc_buffer()

  DESCRITION
    The function reallocates the join buffer of the join cache. After this
    it resets the buffer for writing.

  NOTES
    The function assumes that buff_size contains the new value for the join
    buffer size.  

  RETURN VALUE
    0   if the buffer has been successfully reallocated
    1   otherwise
*/

int JOIN_CACHE::realloc_buffer()
{
  int rc;
  free();
  rc= test(!(buff= (uchar*) my_malloc(buff_size, MYF(0))));
  reset(TRUE);
  return rc;   	
}
  

/* 
  Initialize a join cache       

  SYNOPSIS
    init()

  DESCRIPTION
    The function initializes the join cache structure. It supposed to be called
    by init methods for classes derived from the JOIN_CACHE.
    The function allocates memory for the join buffer and for descriptors of
    the record fields stored in the buffer.

  NOTES
    The code of this function should have been included into the constructor
    code itself. However the new operator for the class JOIN_CACHE would
    never fail while memory allocation for the join buffer is not absolutely
    unlikely to fail. That's why this memory allocation has to be placed in a
    separate function that is called in a couple with a cache constructor.
    It is quite natural to put almost all other constructor actions into
    this function.     
  
  RETURN VALUE
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE::init()
{
  DBUG_ENTER("JOIN_CACHE::init");

  calc_record_fields();

  collect_info_on_key_args();

  if (alloc_fields())
    DBUG_RETURN(1);

  create_flag_fields();

  create_key_arg_fields();

  create_remaining_fields();

  set_constants();

  if (alloc_buffer())
    DBUG_RETURN(1); 
  
  reset(TRUE); 

  DBUG_RETURN(0);
}


/* 
  Check the possibility to read the access keys directly from the join buffer       
  SYNOPSIS
    check_emb_key_usage()

  DESCRIPTION
    The function checks some conditions at which the key values can be read
    directly from the join buffer. This is possible when the key values can be
    composed by concatenation of the record fields stored in the join buffer.
    Sometimes when the access key is multi-component the function has to re-order
    the fields written into the join buffer to make keys embedded. If key 
    values for the key access are detected as embedded then 'use_emb_key'
    is set to TRUE.

  EXAMPLE
    Let table t2 has an index defined on the columns a,b . Let's assume also
    that the columns t2.a, t2.b as well as the columns t1.a, t1.b are all
    of the integer type. Then if the query
      SELECT COUNT(*) FROM t1, t2 WHERE t1.a=t2.a and t1.b=t2.b  
    is executed with a join cache in such a way that t1 is the driving
    table then the key values to access table t2 can be read directly
    from the join buffer.
  
  NOTES
    In some cases key values could be read directly from the join buffer but
    we still do not consider them embedded. In the future we'll expand the
    the class of keys which we identify as embedded.

  NOTES
    The function returns FALSE if no key is used to join the records
    from join_tab.

  RETURN VALUE
    TRUE    key values will be considered as embedded,
    FALSE   otherwise.
*/

bool JOIN_CACHE::check_emb_key_usage()
{

  if (!is_key_access())
    return FALSE;

  uint i;
  Item *item; 
  KEY_PART_INFO *key_part;
  CACHE_FIELD *copy;
  CACHE_FIELD *copy_end;
  uint len= 0;
  TABLE *table= join_tab->table;
  TABLE_REF *ref= &join_tab->ref;
  KEY *keyinfo= table->key_info+ref->key;

  /* 
    If some of the key arguments are not from the local cache the key
    is not considered as embedded.
    TODO:
    Expand it to the case when ref->key_parts=1 and local_key_arg_fields=0.
  */  
  if (external_key_arg_fields != 0)
    return FALSE;
  /* 
    If the number of the local key arguments is not equal to the number
    of key parts the key value cannot be read directly from the join buffer.   
  */
  if (local_key_arg_fields != ref->key_parts)
    return FALSE;

  /* 
    A key is not considered embedded if one of the following is true:
    - one of its key parts is not equal to a field
    - it is a partial key
    - definition of the argument field does not coincide with the
      definition of the corresponding key component
    - some of the key components are nullable
  */  
  for (i=0; i < ref->key_parts; i++)
  {
    item= ref->items[i]->real_item();
    if (item->type() != Item::FIELD_ITEM)
      return FALSE;
    key_part= keyinfo->key_part+i;
    if (key_part->key_part_flag & HA_PART_KEY_SEG)
      return FALSE;
    if (!key_part->field->eq_def(((Item_field *) item)->field))
      return FALSE;
    if (key_part->field->maybe_null())
      return FALSE;
  }
  
  copy= field_descr+flag_fields;
  copy_end= copy+local_key_arg_fields;
  for ( ; copy < copy_end; copy++)
  {
    /* 
      If some of the key arguments are of variable length the key
      is not considered as embedded.
    */
    if (copy->type != 0)
      return FALSE;
    /* 
      If some of the key arguments are bit fields whose bits are partially
      stored with null bits the key is not considered as embedded.
    */
    if (copy->field->type() == MYSQL_TYPE_BIT &&
	 ((Field_bit*) (copy->field))->bit_len)
      return FALSE;
    len+= copy->length;
  }

  emb_key_length= len;

  /* 
    Make sure that key fields follow the order of the corresponding
    key components these fields are equal to. For this the descriptors
    of the fields that comprise the key might be re-ordered.
  */
  for (i= 0; i < ref->key_parts; i++)
  {
    uint j;
    Item *item= ref->items[i]->real_item();
    Field *fld= ((Item_field *) item)->field;
    CACHE_FIELD *init_copy= field_descr+flag_fields+i; 
    for (j= i, copy= init_copy; i < local_key_arg_fields;  i++, copy++)
    {
      if (fld->eq(copy->field))
      {
        if (j != i)
        {
          CACHE_FIELD key_part_copy= *copy;
          *copy= *init_copy;
          *init_copy= key_part_copy;
        }
        break;
      }
    }
  }

  return TRUE;
}    


/* 
  Write record fields and their required offsets into the join cache buffer

  SYNOPSIS
    write_record_data()
      link        a reference to the associated info in the previous cache
      is_full OUT true if it has been decided that no more records will be
                  added to the join buffer

  DESCRIPTION
    This function put into the cache buffer the following info that it reads
    from the join record buffers or computes somehow:
    (1) the length of all fields written for the record (optional)
    (2) an offset to the associated info in the previous cache (if there is any)
        determined by the link parameter
    (3) all flag fields of the tables whose data field are put into the cache:
        - match flag (optional),
        - null bitmaps for all tables,
        - null row flags for all tables
    (4) values of all data fields including
        - full images of those fixed legth data fields that cannot have 
          trailing spaces
        - significant part of fixed length fields that can have trailing spaces
          with the prepanded length 
        - data of non-blob variable length fields with the prepanded data length  
        - blob data from blob fields with the prepanded data length
    (5) record offset values for the data fields that are referred to from 
        other caches
 
    The record is written at the current position stored in the field 'pos'.
    At the end of the function 'pos' points at the position right after the 
    written record data.
    The function increments the number of records in the cache that is stored
    in the 'records' field by 1. The function also modifies the values of
    'curr_rec_pos' and 'last_rec_pos' to point to the written record.
    The 'end_pos' cursor is modified accordingly.
    The 'last_rec_blob_data_is_in_rec_buff' is set on if the blob data 
    remains in the record buffers and not copied to the join buffer. It may
    happen only to the blob data from the last record added into the cache.
    If on_precond is attached to join_tab and it is not evaluated to TRUE
    then MATCH_IMPOSSIBLE is placed in the match flag field of the record
    written into the join buffer.
       
  RETURN VALUE
    length of the written record data
*/

uint JOIN_CACHE::write_record_data(uchar * link, bool *is_full)
{
  uint len;
  bool last_record;
  CACHE_FIELD *copy;
  CACHE_FIELD *copy_end;
  uchar *flags_pos;
  uchar *cp= pos;
  uchar *init_pos= cp;
  uchar *rec_len_ptr= 0;
  uint key_extra= extra_key_length();
 
  records++;  /* Increment the counter of records in the cache */

  len= pack_length + key_extra;

  /* Make an adjustment for the size of the auxiliary buffer if there is any */
  uint incr= aux_buffer_incr(records);
  ulong rem= rem_space();
  aux_buff_size+= len+incr < rem ? incr : rem;

  /*
    For each blob to be put into cache save its length and a pointer
    to the value in the corresponding element of the blob_ptr array.
    Blobs with null values are skipped.
    Increment 'len' by the total length of all these blobs. 
  */    
  if (blobs)
  {
    CACHE_FIELD **copy_ptr= blob_ptr;
    CACHE_FIELD **copy_ptr_end= copy_ptr+blobs;
    for ( ; copy_ptr < copy_ptr_end; copy_ptr++)
    {
      Field_blob *blob_field= (Field_blob *) (*copy_ptr)->field;
      if (!blob_field->is_null())
      {
        uint blob_len= blob_field->get_length();
        (*copy_ptr)->blob_length= blob_len;
        len+= blob_len;
        blob_field->get_ptr(&(*copy_ptr)->str);
      }
    }
  }

  /*
    Check whether we won't be able to add any new record into the cache after
    this one because the cache will be full. Set last_record to TRUE if it's so.
    The assume that the cache will be full after the record has been written
    into it if either the remaining space of the cache is not big enough for the 
    record's blob values or if there is a chance that not all non-blob fields
    of the next record can be placed there.
    This function is called only in the case when there is enough space left in
    the cache to store at least non-blob parts of the current record.
  */
  last_record= (len+pack_length_with_blob_ptrs+key_extra) > rem_space();
  
  /* 
    Save the position for the length of the record in the cache if it's needed.
    The length of the record will be inserted here when all fields of the record
    are put into the cache.  
  */
  if (with_length)
  {
    rec_len_ptr= cp;   
    cp+= size_of_rec_len;
  }

  /*
    Put a reference to the fields of the record that are stored in the previous
    cache if there is any. This reference is passed by the 'link' parameter.     
  */
  if (prev_cache)
  {
    cp+= prev_cache->get_size_of_rec_offset();
    prev_cache->store_rec_ref(cp, link);
  } 

  curr_rec_pos= cp;
  
  /* If the there is a match flag set its value to 0 */
  copy= field_descr;
  if (with_match_flag)
    *copy[0].str= 0;

  /* First put into the cache the values of all flag fields */
  copy_end= field_descr+flag_fields;
  flags_pos= cp;
  for ( ; copy < copy_end; copy++)
  {
    memcpy(cp, copy->str, copy->length);
    cp+= copy->length;
  } 
  
  /* Now put the values of the remaining fields as soon as they are not nulls */ 
  copy_end= field_descr+fields;
  for ( ; copy < copy_end; copy++)
  {
    Field *field= copy->field;
    if (field && field->maybe_null() && field->is_null())
    {
      /* Do not copy a field if its value is null */
      if (copy->referenced_field_no)
        copy->offset= 0;
      continue;              
    }
    /* Save the offset of the field to put it later at the end of the record */ 
    if (copy->referenced_field_no)
      copy->offset= cp-curr_rec_pos;

    if (copy->type == CACHE_BLOB)
    {
      Field_blob *blob_field= (Field_blob *) copy->field;
      if (last_record)
      {
        last_rec_blob_data_is_in_rec_buff= 1;
        /* Put down the length of the blob and the pointer to the data */  
	blob_field->get_image(cp, copy->length+sizeof(char*),
                              blob_field->charset());
	cp+= copy->length+sizeof(char*);
      }
      else
      {
        /* First put down the length of the blob and then copy the data */ 
	blob_field->get_image(cp, copy->length, 
			      blob_field->charset());
	memcpy(cp+copy->length, copy->str, copy->blob_length);               
	cp+= copy->length+copy->blob_length;
      }
    }
    else
    {
      switch (copy->type) {
      case CACHE_VARSTR1:
        /* Copy the significant part of the short varstring field */ 
        len= (uint) copy->str[0] + 1;
        memcpy(cp, copy->str, len);
        cp+= len;
        break;
      case CACHE_VARSTR2:
        /* Copy the significant part of the long varstring field */
        len= uint2korr(copy->str) + 2;
        memcpy(cp, copy->str, len);
        cp+= len;
        break;
      case CACHE_STRIPPED:
      {
        /* 
          Put down the field value stripping all trailing spaces off.
          After this insert the length of the written sequence of bytes.
        */ 
	uchar *str, *end;
	for (str= copy->str, end= str+copy->length;
	     end > str && end[-1] == ' ';
	     end--) ;
	len=(uint) (end-str);
        int2store(cp, len);
	memcpy(cp+2, str, len);
	cp+= len+2;
        break;
      }
      default:      
        /* Copy the entire image of the field from the record buffer */
	memcpy(cp, copy->str, copy->length);
	cp+= copy->length;
      }
    }
  }
  
  /* Add the offsets of the fields that are referenced from other caches */ 
  if (referenced_fields)
  {
    uint cnt= 0;
    for (copy= field_descr+flag_fields; copy < copy_end ; copy++)
    {
      if (copy->referenced_field_no)
      {
        store_fld_offset(cp+size_of_fld_ofs*(copy->referenced_field_no-1),
                         copy->offset);
        cnt++;
      }
    }
    cp+= size_of_fld_ofs*cnt;
  }

  if (rec_len_ptr)
    store_rec_length(rec_len_ptr, (ulong) (cp-rec_len_ptr-size_of_rec_len));
  last_rec_pos= curr_rec_pos; 
  end_pos= pos= cp;
  *is_full= last_record;

  last_written_is_null_compl= 0;   
  if (!join_tab->first_unmatched && join_tab->on_precond)
  { 
    join_tab->found= 0;
    join_tab->not_null_compl= 1;
    if (!join_tab->on_precond->val_int())
    {
      flags_pos[0]= MATCH_IMPOSSIBLE;     
      last_written_is_null_compl= 1;
    }
  } 
      
  return (uint) (cp-init_pos);
}


/* 
  Reset the join buffer for reading/writing: default implementation

  SYNOPSIS
    reset()
      for_writing  if it's TRUE the function reset the buffer for writing

  DESCRIPTION
    This default implementation of the virtual function reset() resets 
    the join buffer for reading or writing.
    If the buffer is reset for reading only the 'pos' value is reset
    to point to the very beginning of the join buffer. If the buffer is
    reset for writing additionally: 
    - the counter of the records in the buffer is set to 0,
    - the the value of 'last_rec_pos' gets pointing at the position just
      before the buffer, 
    - 'end_pos' is set to point to the beginning of the join buffer,
    - the size of the auxiliary buffer is reset to 0,
    - the flag 'last_rec_blob_data_is_in_rec_buff' is set to 0.
    
  RETURN VALUE
    none
*/

void JOIN_CACHE::reset(bool for_writing)
{
  pos= buff;
  curr_rec_link= 0;
  if (for_writing)
  {
    records= 0;
    last_rec_pos= buff;
    aux_buff_size= 0;
    end_pos= pos;
    last_rec_blob_data_is_in_rec_buff= 0;
  }
}


/* 
  Add a record into the join buffer: the default implementation

  SYNOPSIS
    put_record()

  DESCRIPTION
    This default implementation of the virtual function put_record writes
    the next matching record into the join buffer.
    It also links the record having been written into the join buffer with
    the matched record in the previous cache if there is any.
    The implementation assumes that the function get_curr_link() 
    will return exactly the pointer to this matched record.

  RETURN VALUE
    TRUE    if it has been decided that it should be the last record
            in the join buffer,
    FALSE   otherwise
*/

bool JOIN_CACHE::put_record()
{
  bool is_full;
  uchar *link= 0;
  if (prev_cache)
    link= prev_cache->get_curr_rec_link();
  write_record_data(link, &is_full);
  return is_full;
}
  

/* 
  Read the next record from the join buffer: the default implementation

  SYNOPSIS
    get_record()

  DESCRIPTION
    This default implementation of the virtual function get_record
    reads fields of the next record from the join buffer of this cache.
    The function also reads all other fields associated with this record
    from the the join buffers of the previous caches. The fields are read
    into the corresponding record buffers.
    It is supposed that 'pos' points to the position in the buffer 
    right after the previous record when the function is called.
    When the function returns the 'pos' values is updated to point
    to the position after the read record.
    The value of 'curr_rec_pos' is also updated by the function to
    point to the beginning of the first field of the record in the
    join buffer.    

  RETURN VALUE
    TRUE    there are no more records to read from the join buffer
    FALSE   otherwise
*/

bool JOIN_CACHE::get_record()
{ 
  bool res;
  uchar *prev_rec_ptr= 0;
  if (with_length)
    pos+= size_of_rec_len;
  if (prev_cache)
  {
    pos+= prev_cache->get_size_of_rec_offset();
    prev_rec_ptr= prev_cache->get_rec_ref(pos);
  }
  curr_rec_pos= pos;
  if (!(res= read_all_record_fields() == NO_MORE_RECORDS_IN_BUFFER))
  {
    pos+= referenced_fields*size_of_fld_ofs;
    if (prev_cache)
      prev_cache->get_record_by_pos(prev_rec_ptr);
  } 
  return res; 
}


/* 
  Read a positioned record from the join buffer: the default implementation

  SYNOPSIS
    get_record_by_pos()
      rec_ptr  position of the first field of the record in the join buffer

  DESCRIPTION
    This default implementation of the virtual function get_record_pos
    reads the fields of the record positioned at 'rec_ptr' from the join buffer.
    The function also reads all other fields associated with this record 
    from the the join buffers of the previous caches. The fields are read
    into the corresponding record buffers.

  RETURN VALUE
    none
*/

void JOIN_CACHE::get_record_by_pos(uchar *rec_ptr)
{
  uchar *save_pos= pos;
  pos= rec_ptr;
  read_all_record_fields();
  pos= save_pos;
  if (prev_cache)
  {
    uchar *prev_rec_ptr= prev_cache->get_rec_ref(rec_ptr);
    prev_cache->get_record_by_pos(prev_rec_ptr);
  }
}


/* 
  Get the match flag from the referenced record: the default implementation

  SYNOPSIS
    get_match_flag_by_pos()
      rec_ptr  position of the first field of the record in the join buffer

  DESCRIPTION
    This default implementation of the virtual function get_match_flag_by_pos
    get the match flag for the record pointed by the reference at the position
    rec_ptr. If the match flag is placed in one of the previous buffers the
    function first reaches the linked record fields in this buffer.

  RETURN VALUE
    match flag for the record at the position rec_ptr
*/

enum JOIN_CACHE::Match_flag JOIN_CACHE::get_match_flag_by_pos(uchar *rec_ptr)
{
  Match_flag match_fl= MATCH_NOT_FOUND;
  if (with_match_flag)
  {
    match_fl= (enum Match_flag) rec_ptr[0];
    return match_fl;
  }
  if (prev_cache)
  {
    uchar *prev_rec_ptr= prev_cache->get_rec_ref(rec_ptr);
    return prev_cache->get_match_flag_by_pos(prev_rec_ptr);
  } 
  DBUG_ASSERT(0);
  return match_fl;
}


/* 
  Calculate the increment of the auxiliary buffer for a record write

  SYNOPSIS
    aux_buffer_incr()
      recno   the number of the record the increment to be calculated for

  DESCRIPTION
    This function calls the aux_buffer_incr the method of the
    companion member join_tab_scan to calculate the growth of the
    auxiliary buffer when the recno-th record is added to the
    join_buffer of this cache.

  RETURN VALUE
    the number of bytes in the increment 
*/

uint JOIN_CACHE::aux_buffer_incr(ulong recno)
{ 
  return join_tab_scan->aux_buffer_incr(recno);
}

/* 
  Read all flag and data fields of a record from the join buffer

  SYNOPSIS
    read_all_record_fields()

  DESCRIPTION
    The function reads all flag and data fields of a record from the join
    buffer into the corresponding record buffers.
    The fields are read starting from the position 'pos' which is
    supposed to point to the beginning og the first record field.
    The function increments the value of 'pos' by the length of the
    read data. 

  RETURN VALUE
    (-1)   if there is no more records in the join buffer
    length of the data read from the join buffer - otherwise
*/

uint JOIN_CACHE::read_all_record_fields()
{
  uchar *init_pos= pos;
  
  if (pos > last_rec_pos || !records)
    return NO_MORE_RECORDS_IN_BUFFER;

  /* First match flag, read null bitmaps and null_row flag for each table */
  read_flag_fields();
 
  /* Now read the remaining table fields if needed */
  CACHE_FIELD *copy= field_descr+flag_fields;
  CACHE_FIELD *copy_end= field_descr+fields;
  bool blob_in_rec_buff= blob_data_is_in_rec_buff(init_pos);
  for ( ; copy < copy_end; copy++)
    read_record_field(copy, blob_in_rec_buff);

  return (uint) (pos-init_pos);
}


/* 
  Read all flag fields of a record from the join buffer

  SYNOPSIS
    read_flag_fields()

  DESCRIPTION
    The function reads all flag fields of a record from the join
    buffer into the corresponding record buffers.
    The fields are read starting from the position 'pos'.
    The function increments the value of 'pos' by the length of the
    read data. 

  RETURN VALUE
    length of the data read from the join buffer
*/

uint JOIN_CACHE::read_flag_fields()
{
  uchar *init_pos= pos;
  CACHE_FIELD *copy= field_descr;
  CACHE_FIELD *copy_end= copy+flag_fields;
  if (with_match_flag)
  {
    copy->str[0]= test((Match_flag) pos[0] == MATCH_FOUND);
    pos+= copy->length;
    copy++;    
  } 
  for ( ; copy < copy_end; copy++)
  {
    memcpy(copy->str, pos, copy->length);
    pos+= copy->length;
  }
  return (pos-init_pos);
}


/* 
  Read a data record field from the join buffer

  SYNOPSIS
    read_record_field()
      copy             the descriptor of the data field to be read
      blob_in_rec_buff indicates whether this is the field from the record
                       whose blob data are in record buffers

  DESCRIPTION
    The function reads the data field specified by the parameter copy
    from the join buffer into the corresponding record buffer. 
    The field is read starting from the position 'pos'.
    The data of blob values is not copied from the join buffer.
    The function increments the value of 'pos' by the length of the
    read data. 

  RETURN VALUE
    length of the data read from the join buffer
*/

uint JOIN_CACHE::read_record_field(CACHE_FIELD *copy, bool blob_in_rec_buff)
{
  uint len;
  /* Do not copy the field if its value is null */ 
  if (copy->field && copy->field->maybe_null() && copy->field->is_null())
    return 0;           
  if (copy->type == CACHE_BLOB)
  {
    Field_blob *blob_field= (Field_blob *) copy->field;
    /* 
      Copy the length and the pointer to data but not the blob data 
      itself to the record buffer
    */ 
    if (blob_in_rec_buff)
    {
      blob_field->set_image(pos, copy->length+sizeof(char*),
			    blob_field->charset());
      len= copy->length+sizeof(char*);
    }
    else
    {
      blob_field->set_ptr(pos, pos+copy->length);
      len= copy->length+blob_field->get_length();
    }
  }
  else
  {
    switch (copy->type) {
    case CACHE_VARSTR1:
      /* Copy the significant part of the short varstring field */
      len= (uint) pos[0] + 1;
      memcpy(copy->str, pos, len);
      break;
    case CACHE_VARSTR2:
      /* Copy the significant part of the long varstring field */
      len= uint2korr(pos) + 2;
      memcpy(copy->str, pos, len);
      break;
    case CACHE_STRIPPED:
      /* Pad the value by spaces that has been stripped off */
      len= uint2korr(pos);
      memcpy(copy->str, pos+2, len);
      memset(copy->str+len, ' ', copy->length-len);
      len+= 2;
      break;
    default:
      /* Copy the entire image of the field from the record buffer */
      len= copy->length;
      memcpy(copy->str, pos, len);
    }
  }
  pos+= len;
  return len;
}


/* 
  Read a referenced field from the join buffer

  SYNOPSIS
    read_referenced_field()
      copy         pointer to the descriptor of the referenced field
      rec_ptr      pointer to the record that may contain this field
      len  IN/OUT  total length of the record fields 

  DESCRIPTION
    The function checks whether copy points to a data field descriptor
    for this cache object. If it does not then the function returns
    FALSE. Otherwise the function reads the field of the record in
    the join buffer pointed by 'rec_ptr' into the corresponding record
    buffer and returns TRUE.
    If the value of *len is 0 then the function sets it to the total
    length of the record fields including possible trailing offset
    values. Otherwise *len is supposed to provide this value that
    has been obtained earlier. 

  NOTE
    If the value of the referenced field is null then the offset
    for the value is set to 0. If the value of a field can be null
    then the value of flag_fields is always positive. So the offset
    for any non-null value cannot be 0 in this case. 

  RETURN VALUE
    TRUE   'copy' points to a data descriptor of this join cache
    FALSE  otherwise
*/

bool JOIN_CACHE::read_referenced_field(CACHE_FIELD *copy,
                                       uchar *rec_ptr, 
                                       uint *len)
{
  uchar *ptr;
  uint offset;
  if (copy < field_descr || copy >= field_descr+fields)
    return FALSE;
  if (!*len)
  {
    /* Get the total length of the record fields */ 
    uchar *len_ptr= rec_ptr;
    if (prev_cache)
      len_ptr-= prev_cache->get_size_of_rec_offset();
    *len= get_rec_length(len_ptr-size_of_rec_len);
  }
  
  ptr= rec_ptr-(prev_cache ? prev_cache->get_size_of_rec_offset() : 0);  
  offset= get_fld_offset(ptr+ *len - 
                         size_of_fld_ofs*
                         (referenced_fields+1-copy->referenced_field_no));  
  bool is_null= FALSE;
  Field *field= copy->field;
  if (offset == 0 && flag_fields)
    is_null= TRUE;
  if (is_null)
  {
    field->set_null();
    if (!field->real_maybe_null())
      field->table->null_row= 1;
  }
  else
  {
    uchar *save_pos= pos;
    field->set_notnull(); 
    if (!field->real_maybe_null())
      field->table->null_row= 0;
    pos= rec_ptr+offset;
    read_record_field(copy, blob_data_is_in_rec_buff(rec_ptr));
    pos= save_pos;
  }
  return TRUE;
}
   

/* 
  Skip record from join buffer if's already matched: default implementation

  SYNOPSIS
    skip_if_matched()

  DESCRIPTION
    This default implementation of the virtual function skip_if_matched
    skips the next record from the join buffer if its  match flag is set to 
    MATCH_FOUND.
    If the record is skipped the value of 'pos' is set to point to the position
    right after the record.

  RETURN VALUE
    TRUE   the match flag is set to MATCH_FOUND and the record has been skipped
    FALSE  otherwise
*/

bool JOIN_CACHE::skip_if_matched()
{
  DBUG_ASSERT(with_length);
  uint offset= size_of_rec_len;
  if (prev_cache)
    offset+= prev_cache->get_size_of_rec_offset();
  /* Check whether the match flag is MATCH_FOUND */
  if (get_match_flag_by_pos(pos+offset) == MATCH_FOUND)
  {
    pos+= size_of_rec_len + get_rec_length(pos);
    return TRUE;
  }
  return FALSE;
}      


/* 
  Skip record from join buffer if the match isn't needed: default implementation

  SYNOPSIS
    skip_if_not_needed_match()

  DESCRIPTION
    This default implementation of the virtual function skip_if_not_needed_match
    skips the next record from the join buffer if its match flag is not 
    MATCH_NOT_FOUND, and, either its value is MATCH_FOUND and join_tab is the
    first inner table of an inner join, or, its value is MATCH_IMPOSSIBLE
    and join_tab is the first inner table of an outer join.
    If the record is skipped the value of 'pos' is set to point to the position
    right after the record.

  RETURN VALUE
    TRUE    the record has to be skipped
    FALSE   otherwise 
*/

bool JOIN_CACHE::skip_if_not_needed_match()
{
  DBUG_ASSERT(with_length);
  enum Match_flag match_fl;
  uint offset= size_of_rec_len;
  if (prev_cache)
    offset+= prev_cache->get_size_of_rec_offset();

  if ((match_fl= get_match_flag_by_pos(pos+offset)) != MATCH_NOT_FOUND &&
      (join_tab->check_only_first_match() == (match_fl == MATCH_FOUND)) )
  {
    pos+= size_of_rec_len + get_rec_length(pos);
    return TRUE;
  }
  return FALSE;
}      


/* 
  Restore the fields of the last record from the join buffer
 
  SYNOPSIS
    restore_last_record()

  DESCRIPTION
    This function restore the values of the fields of the last record put
    into join buffer in record buffers. The values most probably have been
    overwritten by the field values from other records when they were read
    from the join buffer into the record buffer in order to check pushdown
    predicates.

  RETURN
    none
*/

void JOIN_CACHE::restore_last_record()
{
  if (records)
    get_record_by_pos(last_rec_pos);
}


/*
  Join records from the join buffer with records from the next join table    

  SYNOPSIS
    join_records()
      skip_last    do not find matches for the last record from the buffer

  DESCRIPTION
    The functions extends all records from the join buffer by the matched
    records from join_tab. In the case of outer join operation it also
    adds null complementing extensions for the records from the join buffer
    that have no match. 
    No extensions are generated for the last record from the buffer if
    skip_last is true.  

  NOTES
    The function must make sure that if linked join buffers are used then
    a join buffer cannot be refilled again until all extensions in the
    buffers chained to this one are generated.
    Currently an outer join operation with several inner tables always uses
    at least two linked buffers with the match join flags placed in the
    first buffer. Any record composed of rows of the inner tables that
    matches a record in this buffer must refer to the position of the
    corresponding match flag.

  IMPLEMENTATION
    When generating extensions for outer tables of an outer join operation
    first we generate all extensions for those records from the join buffer
    that have matches, after which null complementing extension for all
    unmatched records from the join buffer are generated.  
      
  RETURN VALUE
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/ 

enum_nested_loop_state JOIN_CACHE::join_records(bool skip_last)
{
  JOIN_TAB *tab;
  enum_nested_loop_state rc= NESTED_LOOP_OK;
  bool outer_join_first_inner= join_tab->is_first_inner_for_outer_join();

  if (outer_join_first_inner && !join_tab->first_unmatched)
    join_tab->not_null_compl= TRUE;   

  if (!join_tab->first_unmatched)
  {
    /* Find all records from join_tab that match records from join buffer */
    rc= join_matching_records(skip_last);   
    if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
      goto finish;
    if (outer_join_first_inner)
    {
      if (next_cache)
      {
        /* 
          Ensure that all matches for outer records from join buffer are to be
          found. Now we ensure that all full records are found for records from
          join buffer. Generally this is an overkill.
          TODO: Ensure that only matches of the inner table records have to be
          found for the records from join buffer.
	*/ 
        rc= next_cache->join_records(skip_last);
        if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
          goto finish;
      }
      join_tab->not_null_compl= FALSE;
      /* Prepare for generation of null complementing extensions */
      for (tab= join_tab->first_inner; tab <= join_tab->last_inner; tab++)
        tab->first_unmatched= join_tab->first_inner;
    }
  }
  if (join_tab->first_unmatched)
  {
    if (is_key_access())
      restore_last_record();

    /* 
      Generate all null complementing extensions for the records from
      join buffer that don't have any matching rows from the inner tables.
    */
    reset(FALSE);
    rc= join_null_complements(skip_last);   
    if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
      goto finish;
  }
  if(next_cache)
  {
    /* 
      When using linked caches we must ensure the records in the next caches
      that refer to the records in the join buffer are fully extended.
      Otherwise we could have references to the records that have been
      already erased from the join buffer and replaced for new records. 
    */ 
    rc= next_cache->join_records(skip_last);
    if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
      goto finish;
  }
  if (outer_join_first_inner)
  {
    /* 
      All null complemented rows have been already generated for all
      outer records from join buffer. Restore the state of the
      first_unmatched values to 0 to avoid another null complementing.
    */
    for (tab= join_tab->first_inner; tab <= join_tab->last_inner; tab++)
      tab->first_unmatched= 0;
  } 
 
  if (skip_last)
  {
    DBUG_ASSERT(!is_key_access());
    /*
       Restore the last record from the join buffer to generate
       all extentions for it.
    */
    get_record();		               
  }

finish:
  restore_last_record();
  reset(TRUE);
  return rc;
}


/*   
  Find matches from the next table for records from the join buffer 

  SYNOPSIS
    join_matching_records()
      skip_last    do not look for matches for the last partial join record 

  DESCRIPTION
    The function retrieves rows of the join_tab table and checks whether they
    match partial join records from the join buffer. If a match is found
    the function will call the sub_select function trying to look for matches
    for the remaining join operations.
    This function currently is called only from the function join_records.    
    If the value of skip_last is true the function writes the partial join
    record from the record buffer into the join buffer to save its value for
    the future processing in the caller function.

  NOTES
    If employed by BNL or BNLH join algorithms the function performs a full
    scan of join_tab for each refill of the join buffer. If BKA or BKAH
    algorithms are used then the function iterates only over those records
    from join_tab that can be accessed by keys built over records in the join
    buffer. To apply a proper method of iteration the function just calls
    virtual iterator methods (open, next, close) of the member join_tab_scan.
    The member can be either of the JOIN_TAB_SCAN or JOIN_TAB_SCAN_MMR type.
    The class JOIN_TAB_SCAN provides the iterator methods for BNL/BNLH join
    algorithms. The class JOIN_TAB_SCAN_MRR provides the iterator methods
    for BKA/BKAH join algorithms.
    When the function looks for records from the join buffer that would
    match a record from join_tab it iterates either over all records in
    the buffer or only over selected records. If BNL join operation is
    performed all records are checked for the match. If BNLH or BKAH
    algorithm is employed to join join_tab then the function looks only
    through the records with the same join key as the record from join_tab.
    With the BKA join algorithm only one record from the join buffer is checked
    for a match for any record from join_tab. To iterate over the candidates
    for a match the virtual function get_next_candidate_for_match is used,
    while the virtual function prepare_look_for_matches is called to prepare
    for such iteration proccess.     

  NOTES
    The function produces all matching extensions for the records in the 
    join buffer following the path of the employed blocked algorithm. 
    When an outer join operation is performed all unmatched records from
    the join buffer must be extended by null values. The function 
    'join_null_complements' serves this purpose.  
      
  RETURN VALUE
    return one of enum_nested_loop_state
*/ 

enum_nested_loop_state JOIN_CACHE::join_matching_records(bool skip_last)
{
  int error;
  enum_nested_loop_state rc= NESTED_LOOP_OK;
  join_tab->table->null_row= 0;
  bool check_only_first_match= join_tab->check_only_first_match();
  bool outer_join_first_inner= join_tab->is_first_inner_for_outer_join();

  /* Return at once if there are no records in the join buffer */
  if (!records)     
    return NESTED_LOOP_OK;   
 
  /* 
    When joining we read records from the join buffer back into record buffers.
    If matches for the last partial join record are found through a call to
    the sub_select function then this partial join record must be saved in the
    join buffer in order to be restored just before the sub_select call.
  */             
  if (skip_last)     
    put_record();     
 
  if (join_tab->use_quick == 2 && join_tab->select->quick)
  { 
    /* A dynamic range access was used last. Clean up after it */
    delete join_tab->select->quick;
    join_tab->select->quick= 0;
  }

  /* Prepare to retrieve all records of the joined table */
  if ((error= join_tab_scan->open())) 
    goto finish;

  while (!(error= join_tab_scan->next()))   
  {
    if (join->thd->killed)
    {
      /* The user has aborted the execution of the query */
      join->thd->send_kill_message();
      rc= NESTED_LOOP_KILLED;
      goto finish; 
    }

    if (join_tab->keep_current_rowid)
      join_tab->table->file->position(join_tab->table->record[0]);
    
    /* Prepare to read matching candidates from the join buffer */
    if (prepare_look_for_matches(skip_last))
      continue;

    uchar *rec_ptr;
    /* Read each possible candidate from the buffer and look for matches */
    while ((rec_ptr= get_next_candidate_for_match()))
    { 
      /* 
        If only the first match is needed, and, it has been already found for
        the next record read from the join buffer, then the record is skipped.
        Also those records that must be null complemented are not considered
        as candidates for matches.
      */
      if ((!check_only_first_match && !outer_join_first_inner) ||
          !skip_next_candidate_for_match(rec_ptr))
      {
	read_next_candidate_for_match(rec_ptr);
        rc= generate_full_extensions(rec_ptr);
        if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
	  goto finish;   
      }
    }
  }

finish: 
  if (error)                 
    rc= error < 0 ? NESTED_LOOP_NO_MORE_ROWS: NESTED_LOOP_ERROR;
  join_tab_scan->close();
  return rc;
}


/*
  Set match flag for a record in join buffer if it has not been set yet    

  SYNOPSIS
    set_match_flag_if_none()
      first_inner     the join table to which this flag is attached to
      rec_ptr         pointer to the record in the join buffer 

  DESCRIPTION
    If the records of the table are accumulated in a join buffer the function
    sets the match flag for the record in the buffer that is referred to by
    the record from this cache positioned at 'rec_ptr'. 
    The function also sets the match flag 'found' of the table first inner
    if it has not been set before. 

  NOTES
    The function assumes that the match flag for any record in any cache
    is placed in the first byte occupied by the record fields. 

  RETURN VALUE
    TRUE   the match flag is set by this call for the first time
    FALSE  the match flag has been set before this call
*/ 

bool JOIN_CACHE::set_match_flag_if_none(JOIN_TAB *first_inner,
                                        uchar *rec_ptr)
{
  if (!first_inner->cache)
  {
    /* 
      Records of the first inner table to which the flag is attached to
      are not accumulated in a join buffer.
    */
    if (first_inner->found)
      return FALSE;
    else
    {
      first_inner->found= 1;
      return TRUE;
    }
  }
  JOIN_CACHE *cache= this;
  while (cache->join_tab != first_inner)
  {
    cache= cache->prev_cache;
    DBUG_ASSERT(cache);
    rec_ptr= cache->get_rec_ref(rec_ptr);
  } 
  if ((Match_flag) rec_ptr[0] != MATCH_FOUND)
  {
    rec_ptr[0]= MATCH_FOUND;
    first_inner->found= 1;
    return TRUE;  
  }
  return FALSE;
}


/*
  Generate all full extensions for a partial join record in the buffer    

  SYNOPSIS
    generate_full_extensions()
      rec_ptr     pointer to the record from join buffer to generate extensions 

  DESCRIPTION
    The function first checks whether the current record of 'join_tab' matches
    the partial join record from join buffer located at 'rec_ptr'. If it is the
    case the function calls the join_tab->next_select method to generate
    all full extension for this partial join match.
      
  RETURN VALUE
    return one of enum_nested_loop_state.
*/ 

enum_nested_loop_state JOIN_CACHE::generate_full_extensions(uchar *rec_ptr)
{
  enum_nested_loop_state rc= NESTED_LOOP_OK;
  
  /*
    Check whether the extended partial join record meets
    the pushdown conditions. 
  */
  if (check_match(rec_ptr))
  {    
    int res= 0;

    if (!join_tab->check_weed_out_table || 
        !(res= do_sj_dups_weedout(join->thd, join_tab->check_weed_out_table)))
    {
      set_curr_rec_link(rec_ptr);
      rc= (join_tab->next_select)(join, join_tab+1, 0);
      if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
      {
        reset(TRUE);
        return rc;
      }
    }
    if (res == -1)
    {
      rc= NESTED_LOOP_ERROR;
      return rc;
    }
  }
  return rc;
}


/*
  Check matching to a partial join record from the join buffer    

  SYNOPSIS
    check_match()
      rec_ptr     pointer to the record from join buffer to check matching to 

  DESCRIPTION
    The function checks whether the current record of 'join_tab' matches
    the partial join record from join buffer located at 'rec_ptr'. If this is
    the case and 'join_tab' is the last inner table of a semi-join or an outer
    join the function turns on the match flag for the 'rec_ptr' record unless
    it has been already set.

  NOTES
    Setting the match flag on can trigger re-evaluation of pushdown conditions
    for the record when join_tab is the last inner table of an outer join.
      
  RETURN VALUE
    TRUE   there is a match
    FALSE  there is no match
*/ 

inline bool JOIN_CACHE::check_match(uchar *rec_ptr)
{
  /* Check whether pushdown conditions are satisfied */
  if (join_tab->select && join_tab->select->skip_record(join->thd) <= 0)
    return FALSE;

  if (!join_tab->is_last_inner_table())
    return TRUE;

  /* 
     This is the last inner table of an outer join,
     and maybe of other embedding outer joins, or
     this is the last inner table of a semi-join.
  */
  JOIN_TAB *first_inner= join_tab->get_first_inner_table();
  do
  {
    set_match_flag_if_none(first_inner, rec_ptr);
    if (first_inner->check_only_first_match() &&
        !join_tab->first_inner)
      return TRUE;
    /* 
      This is the first match for the outer table row.
      The function set_match_flag_if_none has turned the flag
      first_inner->found on. The pushdown predicates for
      inner tables must be re-evaluated with this flag on.
      Note that, if first_inner is the first inner table 
      of a semi-join, but is not an inner table of an outer join
      such that 'not exists' optimization can  be applied to it, 
      the re-evaluation of the pushdown predicates is not needed.
    */      
    for (JOIN_TAB *tab= first_inner; tab <= join_tab; tab++)
    {
      if (tab->select && tab->select->skip_record(join->thd) <= 0)
        return FALSE;
    }
  }
  while ((first_inner= first_inner->first_upper) &&
         first_inner->last_inner == join_tab);
  
  return TRUE;
} 


/*
  Add null complements for unmatched outer records from join buffer    

  SYNOPSIS
    join_null_complements()
      skip_last    do not add null complements for the last record 

  DESCRIPTION
    This function is called only for inner tables of outer joins.
    The function retrieves all rows from the join buffer and adds null
    complements for those of them that do not have matches for outer
    table records.
    If the 'join_tab' is the last inner table of the embedding outer 
    join and the null complemented record satisfies the outer join
    condition then the the corresponding match flag is turned on
    unless it has been set earlier. This setting may trigger
    re-evaluation of pushdown conditions for the record. 

  NOTES
    The same implementation of the virtual method join_null_complements
    is used for BNL/BNLH/BKA/BKA join algorthm.
      
  RETURN VALUE
    return one of enum_nested_loop_state.
*/ 

enum_nested_loop_state JOIN_CACHE::join_null_complements(bool skip_last)
{
  uint cnt; 
  enum_nested_loop_state rc= NESTED_LOOP_OK;
  bool is_first_inner= join_tab == join_tab->first_unmatched;
 
  /* Return at once if there are no records in the join buffer */
  if (!records)
    return NESTED_LOOP_OK;
  
  cnt= records - (is_key_access() ? 0 : test(skip_last));

  /* This function may be called only for inner tables of outer joins */ 
  DBUG_ASSERT(join_tab->first_inner);

  for ( ; cnt; cnt--)
  {
    if (join->thd->killed)
    {
      /* The user has aborted the execution of the query */
      join->thd->send_kill_message();
      rc= NESTED_LOOP_KILLED;
      goto finish;
    }
    /* Just skip the whole record if a match for it has been already found */
    if (!is_first_inner || !skip_if_matched())
    {
      get_record();
      /* The outer row is complemented by nulls for each inner table */
      restore_record(join_tab->table, s->default_values);
      mark_as_null_row(join_tab->table);  
      rc= generate_full_extensions(get_curr_rec());
      if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
        goto finish;
    }
  }

finish:
  return rc;
}


/*
  Add a comment on the join algorithm employed by the join cache 

  SYNOPSIS
    print_explain_comment()
      str  string to add the comment on the employed join algorithm to

  DESCRIPTION
    This function adds info on the type of the used join buffer (flat or
    incremental) and on the type of the the employed join algorithm (BNL,
    BNLH, BKA or BKAH) to the the end of the sring str.

  RETURN VALUE
    none
*/ 

void JOIN_CACHE::print_explain_comment(String *str)
{
  str->append(STRING_WITH_LEN(" ("));
  const char *buffer_type= prev_cache ? "incremental" : "flat";
  str->append(buffer_type);
  str->append(STRING_WITH_LEN(", "));
  
  const char *join_alg="";
  switch (get_join_alg()) {
  case BNL_JOIN_ALG:
    join_alg= "BNL";
    break;
  case BNLH_JOIN_ALG:
    join_alg= "BNLH";
    break;
  case BKA_JOIN_ALG:
    join_alg= "BKA";
    break;
  case BKAH_JOIN_ALG:
    join_alg= "BKAH";
    break;
  default:
    DBUG_ASSERT(0);
  }

  str->append(join_alg);
  str->append(STRING_WITH_LEN(" join"));
  str->append(STRING_WITH_LEN(")"));
 }
   

/* 
  Initialize a hashed join cache       

  SYNOPSIS
    init()

  DESCRIPTION
    The function initializes the cache structure with a hash table in it.
    The hash table will be used to store key values for the records from
    the join buffer.
    The function allocates memory for the join buffer and for descriptors of
    the record fields stored in the buffer.
    The function also initializes a hash table for record keys within the join
    buffer space.

  NOTES VALUE
    The function is supposed to be called by the init methods of the classes 
    derived from JOIN_CACHE_HASHED.
  
  RETURN VALUE
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_HASHED::init()
{
  int rc= 0;
  TABLE_REF *ref= &join_tab->ref;

  DBUG_ENTER("JOIN_CACHE_HASHED::init");

  hash_table= 0;
  key_entries= 0;

  key_length= ref->key_length;

  if ((rc= JOIN_CACHE::init()))
    DBUG_RETURN (rc);

  if (!(key_buff= (uchar*) sql_alloc(key_length)))
    DBUG_RETURN(1);

  /* Take into account a reference to the next record in the key chain */
  pack_length+= get_size_of_rec_offset(); 
  pack_length_with_blob_ptrs+= get_size_of_rec_offset();

  ref_key_info= join_tab->table->key_info+join_tab->ref.key;
  ref_used_key_parts= join_tab->ref.key_parts;

  hash_func= &JOIN_CACHE_HASHED::get_hash_idx_simple;
  hash_cmp_func= &JOIN_CACHE_HASHED::equal_keys_simple;

  KEY_PART_INFO *key_part= ref_key_info->key_part;
  KEY_PART_INFO *key_part_end= key_part+ref_used_key_parts;
  for ( ; key_part < key_part_end; key_part++)
  {
    if (!key_part->field->eq_cmp_as_binary())
    {
      hash_func= &JOIN_CACHE_HASHED::get_hash_idx_complex;
      hash_cmp_func= &JOIN_CACHE_HASHED::equal_keys_complex;
      break;
    }
  }
      
  init_hash_table();

  rec_fields_offset= get_size_of_rec_offset()+get_size_of_rec_length()+
                     (prev_cache ? prev_cache->get_size_of_rec_offset() : 0);

  data_fields_offset= 0;
  if (use_emb_key)
  {
    CACHE_FIELD *copy= field_descr;
    CACHE_FIELD *copy_end= copy+flag_fields;
    for ( ; copy < copy_end; copy++)
      data_fields_offset+= copy->length;
  } 

  DBUG_RETURN(rc);
}


/* 
  Initialize the hash table of a hashed join cache 

  SYNOPSIS
    init_hash_table()

  DESCRIPTION
    The function estimates the number of hash table entries in the hash
    table to be used and initializes this hash table within the join buffer
    space.

  RETURN VALUE
    Currently the function always returns 0;
*/

int JOIN_CACHE_HASHED::init_hash_table()
{
  hash_table= 0;
  key_entries= 0;

  /* Calculate the minimal possible value of size_of_key_ofs greater than 1 */
  uint max_size_of_key_ofs= max(2, get_size_of_rec_offset());  
  for (size_of_key_ofs= 2;
       size_of_key_ofs <= max_size_of_key_ofs;
       size_of_key_ofs+= 2)
  {    
    key_entry_length= get_size_of_rec_offset() + // key chain header
                      size_of_key_ofs +          // reference to the next key 
                      (use_emb_key ?  get_size_of_rec_offset() : key_length);

    ulong space_per_rec= avg_record_length +
                         avg_aux_buffer_incr +
                         key_entry_length+size_of_key_ofs;
    uint n= buff_size / space_per_rec;

    /*
      TODO: Make a better estimate for this upper bound of
            the number of records in in the join buffer.
    */
    uint max_n= buff_size / (pack_length-length+
                             key_entry_length+size_of_key_ofs);

    hash_entries= (uint) (n / 0.7);
    set_if_bigger(hash_entries, 1);
    
    if (offset_size(max_n*key_entry_length) <=
        size_of_key_ofs)
      break;
  }
   
  /* Initialize the hash table */ 
  hash_table= buff + (buff_size-hash_entries*size_of_key_ofs);
  cleanup_hash_table();
  curr_key_entry= hash_table;

  return 0;
}


/*
  Reallocate the join buffer of a hashed join cache
 
  SYNOPSIS
    realloc_buffer()

  DESCRITION
    The function reallocates the join buffer of the hashed join cache.
    After this it initializes a hash table within the buffer space and
    resets the join cache for writing.

  NOTES
    The function assumes that buff_size contains the new value for the join
    buffer size.  

  RETURN VALUE
    0   if the buffer has been successfully reallocated
    1   otherwise
*/

int JOIN_CACHE_HASHED::realloc_buffer()
{
  int rc;
  free();
  rc= test(!(buff= (uchar*) my_malloc(buff_size, MYF(0))));
  init_hash_table();
  reset(TRUE);
  return rc;   	
}

/*
  Get maximum size of the additional space per record used for record keys

  SYNOPSYS
    get_max_key_addon_space_per_record()
  
  DESCRIPTION
    The function returns the size of the space occupied by one key entry
    and one hash table entry.

  RETURN VALUE
    maximum size of the additional space per record that is used to store
    record keys in the hash table
*/

uint JOIN_CACHE_HASHED::get_max_key_addon_space_per_record()
{
  ulong len;
  TABLE_REF *ref= &join_tab->ref;
  /* 
    The total number of hash entries in the hash tables is bounded by
    ceiling(N/0.7) where N is the maximum number of records in the buffer.
    That's why the multiplier 2 is used in the formula below. 
  */ 
  len= (use_emb_key ?  get_size_of_rec_offset() : ref->key_length) +
        size_of_rec_ofs +    // size of the key chain header
        size_of_rec_ofs +    // >= size of the reference to the next key 
        2*size_of_rec_ofs;   // >= 2*( size of hash table entry)
  return len; 
}    


/* 
  Reset the buffer of a hashed join cache for reading/writing

  SYNOPSIS
    reset()
      for_writing  if it's TRUE the function reset the buffer for writing

  DESCRIPTION
    This implementation of the virtual function reset() resets the join buffer
    of the JOIN_CACHE_HASHED class for reading or writing.
    Additionally to what the default implementation does this function
    cleans up the hash table allocated within the buffer.  
    
  RETURN VALUE
    none
*/
 
void JOIN_CACHE_HASHED::reset(bool for_writing)
{
  this->JOIN_CACHE::reset(for_writing);
  if (for_writing && hash_table)
    cleanup_hash_table();
  curr_key_entry= hash_table;
}


/* 
  Add a record into the buffer of a hashed join cache

  SYNOPSIS
    put_record()

  DESCRIPTION
    This implementation of the virtual function put_record writes the next
    matching record into the join buffer of the JOIN_CACHE_HASHED class.
    Additionally to what the default implementation does this function
    performs the following. 
    It extracts from the record the key value used in lookups for matching
    records and searches for this key in the hash tables from the join cache.
    If it finds the key in the hash table it joins the record to the chain
    of records with this key. If the key is not found in the hash table the
    key is placed into it and a chain containing only the newly added record 
    is attached to the key entry. The key value is either placed in the hash 
    element added for the key or, if the use_emb_key flag is set, remains in
    the record from the partial join.
    If the match flag field of a record contains MATCH_IMPOSSIBLE the key is
    not created for this record. 
    
  RETURN VALUE
    TRUE    if it has been decided that it should be the last record
            in the join buffer,
    FALSE   otherwise
*/

bool JOIN_CACHE_HASHED::put_record()
{
  bool is_full;
  uchar *key;
  uint key_len= key_length;
  uchar *key_ref_ptr;
  uchar *link= 0;
  TABLE_REF *ref= &join_tab->ref;
  uchar *next_ref_ptr= pos;

  pos+= get_size_of_rec_offset();
  /* Write the record into the join buffer */  
  if (prev_cache)
    link= prev_cache->get_curr_rec_link();
  write_record_data(link, &is_full);

  if (last_written_is_null_compl)
    return is_full;    

  if (use_emb_key)
    key= get_curr_emb_key();
  else
  {
    /* Build the key over the fields read into the record buffers */ 
    cp_buffer_from_ref(join->thd, join_tab->table, ref);
    key= ref->key_buff;
  }

  /* Look for the key in the hash table */
  if (key_search(key, key_len, &key_ref_ptr))
  {
    uchar *last_next_ref_ptr;
    /* 
      The key is found in the hash table. 
      Add the record to the circular list of the records attached to this key.
      Below 'rec' is the record to be added into the record chain for the found
      key, 'key_ref' points to a flatten representation of the st_key_entry 
      structure that contains the key and the head of the record chain.
    */
    last_next_ref_ptr= get_next_rec_ref(key_ref_ptr+get_size_of_key_offset());
    /* rec->next_rec= key_entry->last_rec->next_rec */
    memcpy(next_ref_ptr, last_next_ref_ptr, get_size_of_rec_offset());
    /* key_entry->last_rec->next_rec= rec */ 
    store_next_rec_ref(last_next_ref_ptr, next_ref_ptr);
    /* key_entry->last_rec= rec */
    store_next_rec_ref(key_ref_ptr+get_size_of_key_offset(), next_ref_ptr);
  }
  else
  {
    /* 
      The key is not found in the hash table.
      Put the key into the join buffer linking it with the keys for the
      corresponding hash entry. Create a circular list with one element
      referencing the record and attach the list to the key in the buffer.
    */
    uchar *cp= last_key_entry;
    cp-= get_size_of_rec_offset()+get_size_of_key_offset();
    store_next_key_ref(key_ref_ptr, cp);
    store_null_key_ref(cp);
    store_next_rec_ref(next_ref_ptr, next_ref_ptr);
    store_next_rec_ref(cp+get_size_of_key_offset(), next_ref_ptr);
    if (use_emb_key)
    {
      cp-= get_size_of_rec_offset();
      store_emb_key_ref(cp, key);
    }
    else
    {
      cp-= key_len;
      memcpy(cp, key, key_len);
    }
    last_key_entry= cp;
    DBUG_ASSERT(last_key_entry >= end_pos);
    /* Increment the counter of key_entries in the hash table */ 
    key_entries++;
  }  
  return is_full;
}


/*
  Read the next record from the buffer of a hashed join cache

  SYNOPSIS
    get_record()

  DESCRIPTION
    Additionally to what the default implementation of the virtual 
    function get_record does this implementation skips the link element
    used to connect the records with the same key into a chain. 

  RETURN VALUE
    TRUE    there are no more records to read from the join buffer
    FALSE   otherwise
*/

bool JOIN_CACHE_HASHED::get_record()
{ 
  pos+= get_size_of_rec_offset();
  return this->JOIN_CACHE::get_record();
}


/* 
  Skip record from a hashed join buffer if its match flag is set to MATCH_FOUND

  SYNOPSIS
    skip_if_matched()

  DESCRIPTION
    This implementation of the virtual function skip_if_matched does
    the same as the default implementation does, but it takes into account
    the link element used to connect the records with the same key into a chain. 

  RETURN VALUE
    TRUE    the match flag is MATCH_FOUND  and the record has been skipped
    FALSE   otherwise 
*/

bool JOIN_CACHE_HASHED::skip_if_matched()
{
  uchar *save_pos= pos;
  pos+= get_size_of_rec_offset();
  if (!this->JOIN_CACHE::skip_if_matched())
  {
    pos= save_pos;
    return FALSE;
  }
  return TRUE;
}


/* 
  Skip record from a hashed join buffer if its match flag dictates to do so

  SYNOPSIS
    skip_if_uneeded_match()

  DESCRIPTION
    This implementation of the virtual function skip_if_not_needed_match does
    the same as the default implementation does, but it takes into account
    the link element used to connect the records with the same key into a chain. 

  RETURN VALUE
    TRUE    the match flag dictates to skip the record
    FALSE   the match flag is off 
*/

bool JOIN_CACHE_HASHED::skip_if_not_needed_match()
{
  uchar *save_pos= pos;
  pos+= get_size_of_rec_offset();
  if (!this->JOIN_CACHE::skip_if_not_needed_match())
  {
    pos= save_pos;
    return FALSE;
  }
  return TRUE;
}


/* 
  Search for a key in the hash table of the join buffer

  SYNOPSIS
    key_search()
      key             pointer to the key value
      key_len         key value length
      key_ref_ptr OUT position of the reference to the next key from 
                      the hash element for the found key , or
                      a position where the reference to the the hash 
                      element for the key is to be added in the
                      case when the key has not been found
      
  DESCRIPTION
    The function looks for a key in the hash table of the join buffer.
    If the key is found the functionreturns the position of the reference
    to the next key from  to the hash element for the given key. 
    Otherwise the function returns the position where the reference to the
    newly created hash element for the given key is to be added.  

  RETURN VALUE
    TRUE    the key is found in the hash table
    FALSE   otherwise
*/

bool JOIN_CACHE_HASHED::key_search(uchar *key, uint key_len,
                                   uchar **key_ref_ptr) 
{
  bool is_found= FALSE;
  uint idx= (this->*hash_func)(key, key_length);
  uchar *ref_ptr= hash_table+size_of_key_ofs*idx;
  while (!is_null_key_ref(ref_ptr))
  {
    uchar *next_key;
    ref_ptr= get_next_key_ref(ref_ptr);
    next_key= use_emb_key ? get_emb_key(ref_ptr-get_size_of_rec_offset()) :
                            ref_ptr-key_length;

    if ((this->*hash_cmp_func)(next_key, key, key_len))
    {
      is_found= TRUE;
      break;
    }
  }
  *key_ref_ptr= ref_ptr;
  return is_found;
} 


/* 
  Hash function that considers a key in the hash table as byte array

  SYNOPSIS
    get_hash_idx_simple()
      key             pointer to the key value
      key_len         key value length
      
  DESCRIPTION
    The function calculates an index of the hash entry in the hash table
    of the join buffer for the given key. It considers the key just as
    a sequence of bytes of the length key_len.

  RETURN VALUE
    the calculated index of the hash entry for the given key  
*/

inline
uint JOIN_CACHE_HASHED::get_hash_idx_simple(uchar* key, uint key_len)
{
  ulong nr= 1;
  ulong nr2= 4;
  uchar *pos= key;
  uchar *end= key+key_len;
  for (; pos < end ; pos++)
  {
    nr^= (ulong) ((((uint) nr & 63)+nr2)*((uint) *pos))+ (nr << 8);
    nr2+= 3;
  }
  return nr % hash_entries;
}


/* 
  Hash function that takes into account collations of the components of the key  

  SYNOPSIS
    get_hash_idx_complex()
      key             pointer to the key value
      key_len         key value length
      
  DESCRIPTION
    The function calculates an index of the hash entry in the hash table
    of the join buffer for the given key. It takes into account that the
    components of the key may be of a varchar type with different collations.
    The function guarantees that the same hash value for any two equal
    keys that may differ as byte sequences.
    The function takes the info about the components of the key, their
    types and used collations from the class member ref_key_info containing
    a pointer to the descriptor of the index that can be used for the join
    operation.

  RETURN VALUE
    the calculated index of the hash entry for the given key  
*/

inline
uint JOIN_CACHE_HASHED::get_hash_idx_complex(uchar *key, uint key_len)
{
  return 
    (uint) (key_hashnr(ref_key_info, ref_used_key_parts, key) % hash_entries);
}


/* 
  Compare two key entries in the hash table as sequence of bytes

  SYNOPSIS
    equal_keys_simple()
      key1            pointer to the first key entry
      key2            pointer to the second key entry 
      key_len         the length of the key values
      
  DESCRIPTION
    The function compares two key entries in the hash table key1 and key2
    as two sequences bytes of the length key_len

  RETURN VALUE
    TRUE       key1 coincides with key2
    FALSE      otherwise
*/

inline
bool JOIN_CACHE_HASHED::equal_keys_simple(uchar *key1, uchar *key2,
                                          uint key_len)
{
  return memcmp(key1, key2, key_len) == 0;
}


/* 
  Compare two key entries taking into account the used collation

  SYNOPSIS
    equal_keys_complex()
      key1            pointer to the first key entry
      key2            pointer to the second key entry 
      key_len         the length of the key values
      
  DESCRIPTION
    The function checks whether two key entries in the hash table
    key1 and key2 are equal as, possibly, compound keys of a certain
    structure whose components may be of a varchar type and may
    employ different collations.
    The descriptor of the key structure is taken from the class
    member ref_key_info.

  RETURN VALUE
    TRUE       key1 is equal tokey2
    FALSE      otherwise
*/

inline
bool JOIN_CACHE_HASHED::equal_keys_complex(uchar *key1, uchar *key2,
                                          uint key_len)
{
  return key_buf_cmp(ref_key_info, ref_used_key_parts, key1, key2) == 0;
}


/* 
  Clean up the hash table of the join buffer

  SYNOPSIS
    cleanup_hash_table()
      key             pointer to the key value
      key_len         key value length
      
  DESCRIPTION
    The function cleans up the hash table in the join buffer removing all
    hash elements from the table. 

  RETURN VALUE
    none  
*/

void JOIN_CACHE_HASHED:: cleanup_hash_table()
{
  last_key_entry= hash_table;
  bzero(hash_table, (buff+buff_size)-hash_table);
  key_entries= 0;
}


/*
  Check whether all records in a key chain have their match flags set on   

  SYNOPSIS
    check_all_match_flags_for_key()
      key_chain_ptr     

  DESCRIPTION
    This function retrieves records in the given circular chain and checks
    whether their match flags are set on. The parameter key_chain_ptr shall
    point to the position in the join buffer storing the reference to the
    last element of this chain. 
            
  RETURN VALUE
    TRUE   if each retrieved record has its match flag set to MATCH_FOUND
    FALSE  otherwise 
*/

bool JOIN_CACHE_HASHED::check_all_match_flags_for_key(uchar *key_chain_ptr)
{
  uchar *last_rec_ref_ptr= get_next_rec_ref(key_chain_ptr);
  uchar *next_rec_ref_ptr= last_rec_ref_ptr;
  do
  {
    next_rec_ref_ptr= get_next_rec_ref(next_rec_ref_ptr);
    uchar *rec_ptr= next_rec_ref_ptr+rec_fields_offset;
    if (get_match_flag_by_pos(rec_ptr) != MATCH_FOUND)
      return FALSE;
  }
  while (next_rec_ref_ptr != last_rec_ref_ptr);
  return TRUE;
}
  

/* 
  Get the next key built for the records from the buffer of a hashed join cache

  SYNOPSIS
    get_next_key()
      key    pointer to the buffer where the key value is to be placed

  DESCRIPTION
    The function reads the next key value stored in the hash table of the
    join buffer. Depending on the value of the use_emb_key flag of the
    join cache the value is read either from the table itself or from
    the record field where it occurs. 

  RETURN VALUE
    length of the key value - if the starting value of 'cur_key_entry' refers
    to the position after that referred by the the value of 'last_key_entry',    
    0 - otherwise.     
*/

uint JOIN_CACHE_HASHED::get_next_key(uchar ** key)
{  
  if (curr_key_entry == last_key_entry)
    return 0;

  curr_key_entry-= key_entry_length;

  *key = use_emb_key ? get_emb_key(curr_key_entry) : curr_key_entry;

  DBUG_ASSERT(*key >= buff && *key < hash_table);

  return key_length;
}


/* 
  Initiate an iteration process over records in the joined table

  SYNOPSIS
    open()

  DESCRIPTION
    The function initiates the process of iteration over records from the 
    joined table recurrently performed by the BNL/BKLH join algorithm.  

  RETURN VALUE   
    0            the initiation is a success 
    error code   otherwise     
*/

int JOIN_TAB_SCAN::open()
{
  for (JOIN_TAB *tab= join->join_tab; tab != join_tab ; tab++)
  {
    tab->status= tab->table->status;
    tab->table->status= 0;
  }
  is_first_record= TRUE;
  return join_init_read_record(join_tab);
}


/* 
  Read the next record that can match while scanning the joined table

  SYNOPSIS
    next()

  DESCRIPTION
    The function reads the next record from the joined table that can
    match some records in the buffer of the join cache 'cache'. To do
    this the function calls the function that scans table records and
    looks for the next one that meets the condition pushed to the
    joined table join_tab.

  NOTES
    The function catches the signal that kills the query.

  RETURN VALUE   
    0            the next record exists and has been successfully read 
    error code   otherwise     
*/

int JOIN_TAB_SCAN::next()
{
  int err= 0;
  int skip_rc;
  READ_RECORD *info= &join_tab->read_record;
  SQL_SELECT *select= join_tab->cache_select;
  if (is_first_record)
    is_first_record= FALSE;
  else
    err= info->read_record(info);
  if (!err)
    update_virtual_fields(join->thd, join_tab->table);
  while (!err && select && (skip_rc= select->skip_record(join->thd)) <= 0)
  {
    if (join->thd->killed || skip_rc < 0) 
      return 1;
    /* 
      Move to the next record if the last retrieved record does not
      meet the condition pushed to the table join_tab.
    */
    err= info->read_record(info);
    if (!err)
      update_virtual_fields(join->thd, join_tab->table);
  } 
  return err; 
}


/* 
  Perform finalizing actions for a scan over the table records

  SYNOPSIS
    close()

  DESCRIPTION
    The function performs the necessary restoring actions after
    the table scan over the joined table has been finished.

  RETURN VALUE   
    none      
*/

void JOIN_TAB_SCAN::close()
{
  for (JOIN_TAB *tab= join->join_tab; tab != join_tab ; tab++)
    tab->table->status= tab->status;
}


/*
  Prepare to iterate over the BNL join cache buffer to look for matches 

  SYNOPSIS
    prepare_look_for_matches()
      skip_last   <-> ignore the last record in the buffer

  DESCRIPTION
    The function prepares the join cache for an iteration over the
    records in the join buffer. The iteration is performed when looking
    for matches for the record from the joined table join_tab that 
    has been placed into the record buffer of the joined table.
    If the value of the parameter skip_last is TRUE then the last
    record from the join buffer is ignored.
    The function initializes the counter of the records that have been
    not iterated over yet.
    
  RETURN VALUE   
    TRUE    there are no records in the buffer to iterate over 
    FALSE   otherwise
*/
    
bool JOIN_CACHE_BNL::prepare_look_for_matches(bool skip_last)
{
  if (!records)
    return TRUE;
  reset(FALSE);
  rem_records= records-test(skip_last);
  return rem_records == 0;
}


/*
  Get next record from the BNL join cache buffer when looking for matches 

  SYNOPSIS
    get_next_candidate_for_match

  DESCRIPTION
    This method is used for iterations over the records from the join
    cache buffer when looking for matches for records from join_tab.
    The methods performs the necessary preparations to read the next record
    from the join buffer into the record buffer by the method
    read_next_candidate_for_match, or, to skip the next record from the join 
    buffer by the method skip_recurrent_candidate_for_match.    
    This implementation of the virtual method get_next_candidate_for_match
    just  decrements the counter of the records that are to be iterated over
    and returns the current value of the cursor 'pos' as the position of 
    the record to be processed. 
    
  RETURN VALUE    
    pointer to the position right after the prefix of the current record
    in the join buffer if the there is another record to iterate over,
    0 - otherwise.  
*/

uchar *JOIN_CACHE_BNL::get_next_candidate_for_match()
{
  if (!rem_records)
    return 0;
  rem_records--;
  return pos+base_prefix_length;
} 


/*
  Check whether the matching record from the BNL cache is to be skipped 

  SYNOPSIS
    skip_next_candidate_for_match
    rec_ptr  pointer to the position in the join buffer right after the prefix 
             of the current record

  DESCRIPTION
    This implementation of the virtual function just calls the
    method skip_if_not_needed_match to check whether the record referenced by
    ref_ptr has its match flag set either to MATCH_FOUND and join_tab is the
    first inner table of a semi-join, or it's set to MATCH_IMPOSSIBLE and
    join_tab is the first inner table of an outer join.
    If so, the function just skips this record setting the value of the
    cursor 'pos' to the position right after it.

  RETURN VALUE    
    TRUE   the record referenced by rec_ptr has been skipped
    FALSE  otherwise  
*/

bool JOIN_CACHE_BNL::skip_next_candidate_for_match(uchar *rec_ptr)
{
  pos= rec_ptr-base_prefix_length; 
  return skip_if_not_needed_match();
}


/*
  Read next record from the BNL join cache buffer when looking for matches 

  SYNOPSIS
    read_next_candidate_for_match
    rec_ptr  pointer to the position in the join buffer right after the prefix
             the current record.

  DESCRIPTION
    This implementation of the virtual method read_next_candidate_for_match
    calls the method get_record to read the record referenced by rec_ptr from
    the join buffer into the record buffer. If this record refers to the
    fields in the other join buffers the call of get_record ensures that
    these fields are read into the corresponding record buffers as well.
    This function is supposed to be called after a successful call of
    the method get_next_candidate_for_match.
    
  RETURN VALUE   
    none
*/

void JOIN_CACHE_BNL::read_next_candidate_for_match(uchar *rec_ptr)
{
  pos= rec_ptr-base_prefix_length;
  get_record();
} 


/*
  Initialize the BNL join cache 

  SYNOPSIS
    init

  DESCRIPTION
    The function initializes the cache structure. It is supposed to be called
    right after a constructor for the JOIN_CACHE_BNL.

  NOTES
    The function first constructs a companion object of the type JOIN_TAB_SCAN,
    then it calls the init method of the parent class.
    
  RETURN VALUE  
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_BNL::init()
{
  DBUG_ENTER("JOIN_CACHE_BNL::init");

  if (!(join_tab_scan= new JOIN_TAB_SCAN(join, join_tab)))
    DBUG_RETURN(1);

  DBUG_RETURN(JOIN_CACHE::init());
}


/*
  Get the chain of records from buffer matching the current candidate for join

  SYNOPSIS
    get_matching_chain_by_join_key()

  DESCRIPTION
    This function first build a join key for the record of join_tab that
    currently is in the join buffer for this table. Then it looks for
    the key entry with this key in the hash table of the join cache.
    If such a key entry is found the function returns the pointer to
    the head of the chain of records in the join_buffer that match this
    key.

  RETURN VALUE
    The pointer to the corresponding circular list of records if
    the key entry with the join key is found, 0 - otherwise.
*/  

uchar *JOIN_CACHE_BNLH::get_matching_chain_by_join_key()
{
  uchar *key_ref_ptr;
  TABLE *table= join_tab->table;
  TABLE_REF *ref= &join_tab->ref;
  KEY *keyinfo= table->key_info+ref->key;
  /* Build the join key value out of the record in the record buffer */
  key_copy(key_buff, table->record[0], keyinfo, key_length);
  /* Look for this key in the join buffer */
  if (!key_search(key_buff, key_length, &key_ref_ptr))
    return 0;
  return key_ref_ptr+get_size_of_key_offset();
}


/*
  Prepare to iterate over the BNLH join cache buffer to look for matches 

  SYNOPSIS
    prepare_look_for_matches()
      skip_last   <-> ignore the last record in the buffer

  DESCRIPTION
    The function prepares the join cache for an iteration over the
    records in the join buffer. The iteration is performed when looking
    for matches for the record from the joined table join_tab that 
    has been placed into the record buffer of the joined table.
    If the value of the parameter skip_last is TRUE then the last
    record from the join buffer is ignored.
    The function builds the hashed key from the join fields of join_tab
    and uses this key to look in the hash table of the join cache for
    the chain of matching records in in the join buffer. If it finds
    such a chain it sets  the member last_rec_ref_ptr to point to the
    last link of the chain while setting the member next_rec_ref_po 0.
    
  RETURN VALUE    
    TRUE    there are no matching records in the buffer to iterate over 
    FALSE   otherwise
*/
    
bool JOIN_CACHE_BNLH::prepare_look_for_matches(bool skip_last)
{
  uchar *curr_matching_chain;
  last_matching_rec_ref_ptr= next_matching_rec_ref_ptr= 0;
  if (!(curr_matching_chain= get_matching_chain_by_join_key()))
    return 1;
  last_matching_rec_ref_ptr= get_next_rec_ref(curr_matching_chain); 
  return 0;
}


/*
  Get next record from the BNLH join cache buffer when looking for matches 

  SYNOPSIS
    get_next_candidate_for_match

  DESCRIPTION
    This method is used for iterations over the records from the join
    cache buffer when looking for matches for records from join_tab.
    The methods performs the necessary preparations to read the next record
    from the join buffer into the record buffer by the method
    read_next_candidate_for_match, or, to skip the next record from the join 
    buffer by the method skip_next_candidate_for_match.    
    This implementation of the virtual method moves to the next record
    in the chain of all records from the join buffer that are to be
    equi-joined with the current record from join_tab.
    
  RETURN VALUE   
    pointer to the beginning of the record fields in the join buffer
    if the there is another record to iterate over, 0 - otherwise.  
*/

uchar *JOIN_CACHE_BNLH::get_next_candidate_for_match()
{
  if (next_matching_rec_ref_ptr == last_matching_rec_ref_ptr)
    return 0;
  next_matching_rec_ref_ptr= get_next_rec_ref(next_matching_rec_ref_ptr ?
                                                next_matching_rec_ref_ptr :
                                                last_matching_rec_ref_ptr);
  return next_matching_rec_ref_ptr+rec_fields_offset; 
} 


/*
  Check whether the matching record from the BNLH cache is to be skipped 

  SYNOPSIS
    skip_next_candidate_for_match
    rec_ptr  pointer to the position in the join buffer right after 
             the previous record

  DESCRIPTION
    This implementation of the virtual function just calls the
    method get_match_flag_by_pos to check whether the record referenced
    by ref_ptr has its match flag set to MATCH_FOUND.

  RETURN VALUE    
    TRUE   the record referenced by rec_ptr has its match flag set to 
           MATCH_FOUND
    FALSE  otherwise  
*/

bool JOIN_CACHE_BNLH::skip_next_candidate_for_match(uchar *rec_ptr)
{
 return  join_tab->check_only_first_match() &&
          (get_match_flag_by_pos(rec_ptr) == MATCH_FOUND);
}


/*
  Read next record from the BNLH join cache buffer when looking for matches 

  SYNOPSIS
    read_next_candidate_for_match
    rec_ptr  pointer to the position in the join buffer right after 
             the previous record

  DESCRIPTION
    This implementation of the virtual method read_next_candidate_for_match
    calls the method get_record_by_pos to read the record referenced by rec_ptr
    from the join buffer into the record buffer. If this record refers to
    fields in the other join buffers the call of get_record_by_po ensures that
    these fields are read into the corresponding record buffers as well.
    This function is supposed to be called after a successful call of
    the method get_next_candidate_for_match.
    
  RETURN VALUE   
    none
*/

void JOIN_CACHE_BNLH::read_next_candidate_for_match(uchar *rec_ptr)
{
  get_record_by_pos(rec_ptr);
} 


/*
  Initialize the BNLH join cache 

  SYNOPSIS
    init

  DESCRIPTION
    The function initializes the cache structure. It is supposed to be called
    right after a constructor for the JOIN_CACHE_BNLH.

  NOTES
    The function first constructs a companion object of the type JOIN_TAB_SCAN,
    then it calls the init method of the parent class.
    
  RETURN VALUE  
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_BNLH::init()
{
  DBUG_ENTER("JOIN_CACHE_BNLH::init");

  if (!(join_tab_scan= new JOIN_TAB_SCAN(join, join_tab)))
    DBUG_RETURN(1);

  DBUG_RETURN(JOIN_CACHE_HASHED::init());
}


/* 
  Calculate the increment of the MRR buffer for a record write       

  SYNOPSIS
    aux_buffer_incr()

  DESCRIPTION
    This implementation of the virtual function aux_buffer_incr determines
    for how much the size of the MRR buffer should be increased when another
    record is added to the cache.   

  RETURN VALUE
    the increment of the size of the MRR buffer for the next record
*/

uint JOIN_TAB_SCAN_MRR::aux_buffer_incr(ulong recno)
{
  uint incr= 0;
  TABLE_REF *ref= &join_tab->ref;
  TABLE *tab= join_tab->table;
  uint rec_per_key= tab->key_info[ref->key].rec_per_key[ref->key_parts-1];
  set_if_bigger(rec_per_key, 1);
  if (recno == 1)
    incr=  ref->key_length + tab->file->ref_length;
  incr+= tab->file->stats.mrr_length_per_rec * rec_per_key;
  return incr; 
}


/* 
  Initiate iteration over records returned by MRR for the current join buffer

  SYNOPSIS
    open()

  DESCRIPTION
    The function initiates the process of iteration over the records from 
    join_tab returned by the MRR interface functions for records from
    the join buffer. Such an iteration is performed by the BKA/BKAH join
    algorithm for each new refill of the join buffer.
    The function calls the MRR handler function multi_range_read_init to
    initiate this process.

  RETURN VALUE   
    0            the initiation is a success 
    error code   otherwise     
*/

int JOIN_TAB_SCAN_MRR::open()
{
  handler *file= join_tab->table->file;

  join_tab->table->null_row= 0;


  /* Dynamic range access is never used with BKA */
  DBUG_ASSERT(join_tab->use_quick != 2);

  for (JOIN_TAB *tab =join->join_tab; tab != join_tab ; tab++)
  {
    tab->status= tab->table->status;
    tab->table->status= 0;
  }

  init_mrr_buff();

  /* 
    Prepare to iterate over keys from the join buffer and to get
    matching candidates obtained with MMR handler functions.
  */ 
  if (!file->inited)
    file->ha_index_init(join_tab->ref.key, 1);
  ranges= cache->get_number_of_ranges_for_mrr();
  if (!join_tab->cache_idx_cond)
    range_seq_funcs.skip_index_tuple= 0;
  return file->multi_range_read_init(&range_seq_funcs, (void*) cache,
                                     ranges, mrr_mode, &mrr_buff);
}


/* 
  Read the next record returned by MRR for the current join buffer

  SYNOPSIS
    next()

  DESCRIPTION
    The function reads the next record from the joined table join_tab
    returned by the MRR handler function multi_range_read_next for
    the current refill of the join buffer. The record is read into
    the record buffer used for join_tab records in join operations.

  RETURN VALUE   
    0            the next record exists and has been successfully read 
    error code   otherwise     
*/

int JOIN_TAB_SCAN_MRR::next()
{
  char **ptr= (char **) cache->get_curr_association_ptr();
  int rc= join_tab->table->file->multi_range_read_next(ptr) ? -1 : 0;
  if (!rc)
  {
    /* 
      If a record in in an incremental cache contains no fields then the
      association for the last record in cache will be equal to cache->end_pos
    */ 
    DBUG_ASSERT(cache->buff <= (uchar *) (*ptr) &&
                (uchar *) (*ptr) <= cache->end_pos);
    update_virtual_fields(join->thd, join_tab->table);
  }
  return rc;
}


/*
  Initialize retrieval of range sequence for BKA join algorithm
    
  SYNOPSIS
    bka_range_seq_init()
     init_params   pointer to the BKA join cache object
     n_ranges      the number of ranges obtained 
     flags         combination of MRR flags

  DESCRIPTION
    The function interprets init_param as a pointer to a JOIN_CACHE_BKA
    object. The function prepares for an iteration over the join keys
    built for all records from the cache join buffer.

  NOTE
    This function are used only as a callback function.    

  RETURN VALUE
    init_param value that is to be used as a parameter of bka_range_seq_next()
*/    

static 
range_seq_t bka_range_seq_init(void *init_param, uint n_ranges, uint flags)
{
  DBUG_ENTER("bka_range_seq_init");
  JOIN_CACHE_BKA *cache= (JOIN_CACHE_BKA *) init_param;
  cache->reset(0);
  DBUG_RETURN((range_seq_t) init_param);
}


/*
  Get the next range/key over records from the join buffer used by a BKA cache
    
  SYNOPSIS
    bka_range_seq_next()
      seq        the value returned by  bka_range_seq_init
      range  OUT reference to the next range
  
  DESCRIPTION
    The function interprets seq as a pointer to a JOIN_CACHE_BKA
    object. The function returns a pointer to the range descriptor
    for the key built over the next record from the join buffer.

  NOTE
    This function are used only as a callback function.
   
  RETURN VALUE
    0   ok, the range structure filled with info about the next range/key
    1   no more ranges
*/    

static 
uint bka_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  DBUG_ENTER("bka_range_seq_next");
  JOIN_CACHE_BKA *cache= (JOIN_CACHE_BKA *) rseq;
  TABLE_REF *ref= &cache->join_tab->ref;
  key_range *start_key= &range->start_key;
  if ((start_key->length= cache->get_next_key((uchar **) &start_key->key)))
  {
    start_key->keypart_map= (1 << ref->key_parts) - 1;
    start_key->flag= HA_READ_KEY_EXACT;
    range->end_key= *start_key;
    range->end_key.flag= HA_READ_AFTER_KEY;
    range->ptr= (char *) cache->get_curr_rec();
    range->range_flag= EQ_RANGE;
    DBUG_RETURN(0);
  } 
  DBUG_RETURN(1);
}


/*
  Check whether range_info orders to skip the next record from BKA buffer

  SYNOPSIS
    bka_range_seq_skip_record()
      seq              value returned by bka_range_seq_init()
      range_info       information about the next range
      rowid [NOT USED] rowid of the record to be checked 

    
  DESCRIPTION
    The function interprets seq as a pointer to a JOIN_CACHE_BKA object.
    The function returns TRUE if the record with this range_info 
    is to be filtered out from the stream of records returned by 
    multi_range_read_next(). 

  NOTE
    This function are used only as a callback function.

  RETURN VALUE
    1    record with this range_info is to be filtered out from the stream
         of records returned by multi_range_read_next()
    0    the record is to be left in the stream
*/ 

static 
bool bka_range_seq_skip_record(range_seq_t rseq, char *range_info, uchar *rowid)
{
  DBUG_ENTER("bka_range_seq_skip_record");
  JOIN_CACHE_BKA *cache= (JOIN_CACHE_BKA *) rseq;
  bool res= cache->get_match_flag_by_pos((uchar *) range_info) ==
            JOIN_CACHE::MATCH_FOUND;
  DBUG_RETURN(res);
}


/*
  Check if the record combination from BKA cache matches the index condition

  SYNOPSIS
    bka_skip_index_tuple()
      rseq             value returned by bka_range_seq_init()
      range_info       record chain for the next range/key returned by MRR
    
  DESCRIPTION
    This is wrapper for JOIN_CACHE_BKA::skip_index_tuple method,
    see comments there.

  NOTE
    This function is used as a RANGE_SEQ_IF::skip_index_tuple callback.
 
  RETURN VALUE
    0    The record combination satisfies the index condition
    1    Otherwise
*/

static 
bool bka_skip_index_tuple(range_seq_t rseq, char *range_info)
{
  DBUG_ENTER("bka_skip_index_tuple");
  JOIN_CACHE_BKA *cache= (JOIN_CACHE_BKA *) rseq;
  DBUG_RETURN(cache->skip_index_tuple(range_info));
}


/*
  Prepare to read the record from BKA cache matching the current joined record   

  SYNOPSIS
    prepare_look_for_matches()
      skip_last <-> ignore the last record in the buffer (always unused here)

  DESCRIPTION
    The function prepares to iterate over records in the join cache buffer
    matching the record loaded into the record buffer for join_tab when
    performing join operation by BKA join algorithm. With BKA algorithms the
    record loaded into the record buffer for join_tab always has a direct
    reference to the matching records from the join buffer. When the regular
    BKA join algorithm is employed the record from join_tab can refer to
    only one such record.   
    The function sets the counter of the remaining records from the cache 
    buffer that would match the current join_tab record to 1.
    
  RETURN VALUE   
    TRUE    there are no records in the buffer to iterate over 
    FALSE   otherwise
*/
    
bool JOIN_CACHE_BKA::prepare_look_for_matches(bool skip_last)
{
  if (!records)
    return TRUE;
  rem_records= 1;
  return FALSE;
}


/*
  Get the record from the BKA cache matching the current joined record   

  SYNOPSIS
    get_next_candidate_for_match

  DESCRIPTION
    This method is used for iterations over the records from the join
    cache buffer when looking for matches for records from join_tab.
    The method performs the necessary preparations to read the next record
    from the join buffer into the record buffer by the method
    read_next_candidate_for_match, or, to skip the next record from the join 
    buffer by the method skip_if_not_needed_match.    
    This implementation of the virtual method get_next_candidate_for_match
    just  decrements the counter of the records that are to be iterated over
    and returns the value of curr_association as a reference to the position
    of the beginning of the record fields in the buffer.
    
  RETURN VALUE   
    pointer to the start of the record fields in the join buffer
    if the there is another record to iterate over, 0 - otherwise.  
*/

uchar *JOIN_CACHE_BKA::get_next_candidate_for_match()
{
  if (!rem_records)
    return 0;
  rem_records--;
  return curr_association;
} 


/*
  Check whether the matching record from the BKA cache is to be skipped 

  SYNOPSIS
    skip_next_candidate_for_match
    rec_ptr  pointer to the position in the join buffer right after 
             the previous record

  DESCRIPTION
    This implementation of the virtual function just calls the
    method get_match_flag_by_pos to check whether the record referenced
    by ref_ptr has its match flag set to MATCH_FOUND.

  RETURN VALUE   
    TRUE   the record referenced by rec_ptr has its match flag set to
           MATCH_FOUND
    FALSE  otherwise  
*/

bool JOIN_CACHE_BKA::skip_next_candidate_for_match(uchar *rec_ptr)
{
  return join_tab->check_only_first_match() && 
         (get_match_flag_by_pos(rec_ptr) == MATCH_FOUND);
}


/*
  Read the next record from the BKA join cache buffer when looking for matches 

  SYNOPSIS
    read_next_candidate_for_match
    rec_ptr  pointer to the position in the join buffer right after 
             the previous record

  DESCRIPTION
    This implementation of the virtual method read_next_candidate_for_match
    calls the method get_record_by_pos to read the record referenced by rec_ptr
    from the join buffer into the record buffer. If this record refers to
    fields in the other join buffers the call of get_record_by_po ensures that
    these fields are read into the corresponding record buffers as well.
    This function is supposed to be called after a successful call of
    the method get_next_candidate_for_match.
    
  RETURN VALUE   
    none
*/

void JOIN_CACHE_BKA::read_next_candidate_for_match(uchar *rec_ptr)
{
  get_record_by_pos(rec_ptr);
} 


/*
  Initialize the BKA join cache 

  SYNOPSIS
    init

  DESCRIPTION
    The function initializes the cache structure. It is supposed to be called
    right after a constructor for the JOIN_CACHE_BKA.

  NOTES
    The function first constructs a companion object of the type 
    JOIN_TAB_SCAN_MRR, then it calls the init method of the parent class.
    
  RETURN VALUE   
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_BKA::init()
{
  bool check_only_first_match= join_tab->check_only_first_match();

  RANGE_SEQ_IF rs_funcs= { bka_range_seq_init, 
                           bka_range_seq_next,
                           check_only_first_match ?
                             bka_range_seq_skip_record : 0,
                           bka_skip_index_tuple };

  DBUG_ENTER("JOIN_CACHE_BKA::init");

  if (!(join_tab_scan= new JOIN_TAB_SCAN_MRR(join, join_tab, 
                                             mrr_mode, rs_funcs)))
    DBUG_RETURN(1);

  DBUG_RETURN(JOIN_CACHE::init());
}


/* 
  Get the key built over the next record from BKA join buffer

  SYNOPSIS
    get_next_key()
      key    pointer to the buffer where the key value is to be placed

  DESCRIPTION
    The function reads key fields from the current record in the join buffer.
    and builds the key value out of these fields that will be used to access
    the 'join_tab' table. Some of key fields may belong to previous caches.
    They are accessed via record references to the record parts stored in the
    previous join buffers. The other key fields always are placed right after
    the flag fields of the record.
    If the key is embedded, which means that its value can be read directly
    from the join buffer, then *key is set to the beginning of the key in
    this buffer. Otherwise the key is built in the join_tab->ref->key_buff.
    The function returns the length of the key if it succeeds ro read it.
    If is assumed that the functions starts reading at the position of
    the record length which is provided for each records in a BKA cache.
    After the key is built the 'pos' value points to the first position after
    the current record.
    The function just skips the records with MATCH_IMPOSSIBLE in the
    match flag field if there is any. 
    The function returns 0 if the initial position is after the beginning
    of the record fields for last record from the join buffer. 

  RETURN VALUE
    length of the key value - if the starting value of 'pos' points to
    the position before the fields for the last record,
    0 - otherwise.     
*/

uint JOIN_CACHE_BKA::get_next_key(uchar ** key)
{
  uint len;
  uint32 rec_len;
  uchar *init_pos;
  JOIN_CACHE *cache;
  
start:

  /* Any record in a BKA cache is prepended with its length */
  DBUG_ASSERT(with_length);
   
  if ((pos+size_of_rec_len) > last_rec_pos || !records)
    return 0;

  /* Read the length of the record */
  rec_len= get_rec_length(pos);
  pos+= size_of_rec_len; 
  init_pos= pos;

  /* Read a reference to the previous cache if any */
  if (prev_cache)
    pos+= prev_cache->get_size_of_rec_offset();

  curr_rec_pos= pos;

  /* Read all flag fields of the record */
  read_flag_fields();

  if (with_match_flag && 
      (Match_flag) curr_rec_pos[0] == MATCH_IMPOSSIBLE )
  {
    pos= init_pos+rec_len;
    goto start;
  }
 
  if (use_emb_key)
  {
    /* An embedded key is taken directly from the join buffer */
    *key= pos;
    len= emb_key_length;
  }
  else
  {
    /* Read key arguments from previous caches if there are any such fields */
    if (external_key_arg_fields)
    {
      uchar *rec_ptr= curr_rec_pos;
      uint key_arg_count= external_key_arg_fields;
      CACHE_FIELD **copy_ptr= blob_ptr-key_arg_count;
      for (cache= prev_cache; key_arg_count; cache= cache->prev_cache)
      { 
        uint len= 0;
        DBUG_ASSERT(cache);
        rec_ptr= cache->get_rec_ref(rec_ptr);
        while (!cache->referenced_fields)
        {
          cache= cache->prev_cache;
          DBUG_ASSERT(cache);
          rec_ptr= cache->get_rec_ref(rec_ptr);
        }
        while (key_arg_count && 
               cache->read_referenced_field(*copy_ptr, rec_ptr, &len))
        {
          copy_ptr++;
          --key_arg_count;
        }
      }
    }
    
    /* 
      Read the other key arguments from the current record. The fields for
      these arguments are always first in the sequence of the record's fields.
    */     
    CACHE_FIELD *copy= field_descr+flag_fields;
    CACHE_FIELD *copy_end= copy+local_key_arg_fields;
    bool blob_in_rec_buff= blob_data_is_in_rec_buff(curr_rec_pos);
    for ( ; copy < copy_end; copy++)
      read_record_field(copy, blob_in_rec_buff);
    
    /* Build the key over the fields read into the record buffers */ 
    TABLE_REF *ref= &join_tab->ref;
    cp_buffer_from_ref(join->thd, join_tab->table, ref);
    *key= ref->key_buff;
    len= ref->key_length;
  }

  pos= init_pos+rec_len;

  return len;
} 


/*
  Check the index condition of the joined table for a record from the BKA cache

  SYNOPSIS
    skip_index_tuple()
      range_info       pointer to the record returned by MRR 
    
  DESCRIPTION
    This function is invoked from MRR implementation to check if an index
    tuple matches the index condition. It is used in the case where the index
    condition actually depends on both columns of the used index and columns
    from previous tables.
   
  NOTES 
    Accessing columns of the previous tables requires special handling with
    BKA. The idea of BKA is to collect record combinations in a buffer and 
    then do a batch of ref access lookups, i.e. by the time we're doing a
    lookup its previous-records-combination is not in prev_table->record[0]
    but somewhere in the join buffer.    
    We need to get it from there back into prev_table(s)->record[0] before we
    can evaluate the index condition, and that's why we need this function
    instead of regular IndexConditionPushdown.

  NOTES
    Possible optimization:
    Before we unpack the record from a previous table
    check if this table is used in the condition.
    If so then unpack the record otherwise skip the unpacking.
    This should be done by a special virtual method
    get_partial_record_by_pos().

  RETURN VALUE
    1    the record combination does not satisfies the index condition
    0    otherwise
*/

bool JOIN_CACHE_BKA::skip_index_tuple(char *range_info)
{
  DBUG_ENTER("JOIN_CACHE_BKA::skip_index_tuple");
  get_record_by_pos((uchar*)range_info);
  DBUG_RETURN(!join_tab->cache_idx_cond->val_int());
}



/*
  Initialize retrieval of range sequence for the BKAH join algorithm
    
  SYNOPSIS
    bkah_range_seq_init()
      init_params   pointer to the BKAH join cache object
      n_ranges      the number of ranges obtained 
      flags         combination of MRR flags

  DESCRIPTION
    The function interprets init_param as a pointer to a JOIN_CACHE_BKAH
    object. The function prepares for an iteration over distinct join keys
    built over the records from the cache join buffer.

  NOTE
    This function are used only as a callback function.    

  RETURN VALUE
    init_param    value that is to be used as a parameter of 
                  bkah_range_seq_next()
*/    

static 
range_seq_t bkah_range_seq_init(void *init_param, uint n_ranges, uint flags)
{
  DBUG_ENTER("bkah_range_seq_init");
  JOIN_CACHE_BKAH *cache= (JOIN_CACHE_BKAH *) init_param;
  cache->reset(0);
  DBUG_RETURN((range_seq_t) init_param);
}


/*
  Get the next range/key over records from the join buffer of a BKAH cache  
    
  SYNOPSIS
    bkah_range_seq_next()
      seq        value returned by  bkah_range_seq_init()
      range  OUT reference to the next range
  
  DESCRIPTION
    The function interprets seq as a pointer to a JOIN_CACHE_BKAH 
    object. The function returns a pointer to the range descriptor
    for the next unique key built over records from the join buffer.

  NOTE
    This function are used only as a callback function.
   
  RETURN VALUE
    0    ok, the range structure filled with info about the next range/key
    1    no more ranges
*/    

static 
uint bkah_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  DBUG_ENTER("bkah_range_seq_next");
  JOIN_CACHE_BKAH *cache= (JOIN_CACHE_BKAH *) rseq;
  TABLE_REF *ref= &cache->join_tab->ref;
  key_range *start_key= &range->start_key;
  if ((start_key->length= cache->get_next_key((uchar **) &start_key->key)))
  {
    start_key->keypart_map= (1 << ref->key_parts) - 1;
    start_key->flag= HA_READ_KEY_EXACT;
    range->end_key= *start_key;
    range->end_key.flag= HA_READ_AFTER_KEY;
    range->ptr= (char *) cache->get_curr_key_chain();
    range->range_flag= EQ_RANGE;
    DBUG_RETURN(0);
  } 
  DBUG_RETURN(1);
}


/*
  Check whether range_info orders to skip the next record from BKAH join buffer

  SYNOPSIS
    bkah_range_seq_skip_record()
      seq              value returned by bkah_range_seq_init()
      range_info       information about the next range/key returned by MRR
      rowid [NOT USED] rowid of the record to be checked (not used)
    
  DESCRIPTION
    The function interprets seq as a pointer to a JOIN_CACHE_BKAH
    object. The function returns TRUE if the record with this range_info
    is to be filtered out from the stream of records returned by
    multi_range_read_next(). 

  NOTE
    This function are used only as a callback function.

  RETURN VALUE
    1    record with this range_info is to be filtered out from the stream
         of records returned by multi_range_read_next()
    0    the record is to be left in the stream
*/ 

static 
bool bkah_range_seq_skip_record(range_seq_t rseq, char *range_info,
                                uchar *rowid)
{
  DBUG_ENTER("bkah_range_seq_skip_record");
  JOIN_CACHE_BKAH *cache= (JOIN_CACHE_BKAH *) rseq;
  bool res= cache->check_all_match_flags_for_key((uchar *) range_info);
  DBUG_RETURN(res);
}

 
/*
  Check if the record combination from BKAH cache matches the index condition

  SYNOPSIS
    bkah_skip_index_tuple()
      rseq             value returned by bka_range_seq_init()
      range_info       record chain for the next range/key returned by MRR
    
  DESCRIPTION
    This is wrapper for JOIN_CACHE_BKA_UNIQUE::skip_index_tuple method,
    see comments there.

  NOTE
    This function is used as a RANGE_SEQ_IF::skip_index_tuple callback.
 
  RETURN VALUE
    0    some records from the chain satisfy the index condition
    1    otherwise
*/

static 
bool bkah_skip_index_tuple(range_seq_t rseq, char *range_info)
{
  DBUG_ENTER("bka_unique_skip_index_tuple");
  JOIN_CACHE_BKAH *cache= (JOIN_CACHE_BKAH *) rseq;
  DBUG_RETURN(cache->skip_index_tuple(range_info));
}


/*
  Prepare to read record from BKAH cache matching the current joined record   

  SYNOPSIS
    prepare_look_for_matches()
      skip_last <-> ignore the last record in the buffer (always unused here)

  DESCRIPTION
    The function prepares to iterate over records in the join cache buffer
    matching the record loaded into the record buffer for join_tab when
    performing join operation by BKAH join algorithm. With BKAH algorithm, if
    association labels are used, then record loaded into the record buffer 
    for join_tab always has a direct reference to the chain of the mathing
    records from the join buffer. If association labels are not used then
    then the chain of the matching records is obtained by the call of the
    get_key_chain_by_join_key function.
    
  RETURN VALUE   
    TRUE    there are no records in the buffer to iterate over 
    FALSE   otherwise
*/
    
bool JOIN_CACHE_BKAH::prepare_look_for_matches(bool skip_last)
{
  last_matching_rec_ref_ptr= next_matching_rec_ref_ptr= 0;
  if (no_association &&
      (curr_matching_chain= get_matching_chain_by_join_key()))
    return 1;
  last_matching_rec_ref_ptr= get_next_rec_ref(curr_matching_chain);
  return 0;
}

/*
  Initialize the BKAH join cache 

  SYNOPSIS
    init

  DESCRIPTION
    The function initializes the cache structure. It is supposed to be called
    right after a constructor for the JOIN_CACHE_BKAH.

  NOTES
    The function first constructs a companion object of the type 
    JOIN_TAB_SCAN_MRR, then it calls the init method of the parent class.
    
  RETURN VALUE   
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_BKAH::init()
{
  bool check_only_first_match= join_tab->check_only_first_match();

  no_association= test(mrr_mode & HA_MRR_NO_ASSOCIATION);

  RANGE_SEQ_IF rs_funcs= { bkah_range_seq_init,
                           bkah_range_seq_next,
                           check_only_first_match && !no_association ?
                             bkah_range_seq_skip_record : 0,
                           bkah_skip_index_tuple };

  DBUG_ENTER("JOIN_CACHE_BKAH::init");

  if (!(join_tab_scan= new JOIN_TAB_SCAN_MRR(join, join_tab, 
                                             mrr_mode, rs_funcs)))
    DBUG_RETURN(1);

  DBUG_RETURN(JOIN_CACHE_HASHED::init());
}


/*
  Check the index condition of the joined table for a record from the BKA cache

  SYNOPSIS
    skip_index_tuple()
      range_info       record chain returned by MRR 
    
  DESCRIPTION
    See JOIN_CACHE_BKA::skip_index_tuple().
    This function is the variant for use with rhe class JOIN_CACHE_BKAH.
    The difference from JOIN_CACHE_BKA case is that there may be multiple
    previous table record combinations that share the same key(MRR range).
    As a consequence, we need to loop through the chain of all table record
    combinations that match the given MRR range key range_info until we find
    one that satisfies the index condition.

  NOTE
    Possible optimization:
    Before we unpack the record from a previous table
    check if this table is used in the condition.
    If so then unpack the record otherwise skip the unpacking.
    This should be done by a special virtual method
    get_partial_record_by_pos().

  RETURN VALUE
    1    any record combination from the chain referred by range_info
         does not satisfy the index condition
    0    otherwise


*/

bool JOIN_CACHE_BKAH::skip_index_tuple(char *range_info)
{
  uchar *last_rec_ref_ptr= get_next_rec_ref((uchar*) range_info);
  uchar *next_rec_ref_ptr= last_rec_ref_ptr;
  DBUG_ENTER("JOIN_CACHE_BKAH::skip_index_tuple");
  do
  {
    next_rec_ref_ptr= get_next_rec_ref(next_rec_ref_ptr);
    uchar *rec_ptr= next_rec_ref_ptr + rec_fields_offset;
    get_record_by_pos(rec_ptr);
    if (join_tab->cache_idx_cond->val_int())
      DBUG_RETURN(FALSE);
  } while(next_rec_ref_ptr != last_rec_ref_ptr);
  DBUG_RETURN(TRUE);
}
