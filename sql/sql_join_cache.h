/*
   Copyright (c) 2011, 2012, Monty Program Ab

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

/*
  This file contains declarations for implementations
  of block based join algorithms
*/

#define JOIN_CACHE_INCREMENTAL_BIT           1
#define JOIN_CACHE_HASHED_BIT                2
#define JOIN_CACHE_BKA_BIT                   4

/* 
  Categories of data fields of variable length written into join cache buffers.
  The value of any of these fields is written into cache together with the
  prepended length of the value.     
*/
#define CACHE_BLOB      1        /* blob field  */
#define CACHE_STRIPPED  2        /* field stripped of trailing spaces */
#define CACHE_VARSTR1   3        /* short string value (length takes 1 byte) */ 
#define CACHE_VARSTR2   4        /* long string value (length takes 2 bytes) */
#define CACHE_ROWID     5        /* ROWID field */

/*
  The CACHE_FIELD structure used to describe fields of records that
  are written into a join cache buffer from record buffers and backward.
*/
typedef struct st_cache_field {
  uchar *str;   /**< buffer from/to where the field is to be copied */ 
  uint length;  /**< maximal number of bytes to be copied from/to str */
  /* 
    Field object for the moved field
    (0 - for a flag field, see JOIN_CACHE::create_flag_fields).
  */
  Field *field;
  uint type;    /**< category of the of the copied field (CACHE_BLOB et al.) */
  /* 
    The number of the record offset value for the field in the sequence
    of offsets placed after the last field of the record. These
    offset values are used to access fields referred to from other caches.
    If the value is 0 then no offset for the field is saved in the
    trailing sequence of offsets.
  */ 
  uint referenced_field_no; 
  /* The remaining structure fields are used as containers for temp values */
  uint blob_length; /**< length of the blob to be copied */
  uint offset;      /**< field offset to be saved in cache buffer */
} CACHE_FIELD;


class JOIN_TAB_SCAN;


/*
  JOIN_CACHE is the base class to support the implementations of 
  - Block Nested Loop (BNL) Join Algorithm,
  - Block Nested Loop Hash (BNLH) Join Algorithm,
  - Batched Key Access (BKA) Join Algorithm.
  The first algorithm is supported by the derived class JOIN_CACHE_BNL,
  the second algorithm is supported by the derived class JOIN_CACHE_BNLH,
  while the third algorithm is implemented in two variant supported by
  the classes JOIN_CACHE_BKA and JOIN_CACHE_BKAH.
  These three algorithms have a lot in common. Each of them first accumulates
  the records of the left join operand in a join buffer and then searches for
  matching rows of the second operand for all accumulated records.
  For the first two algorithms this strategy saves on logical I/O operations:
  the entire set of records from the join buffer requires only one look-through
  of the records provided by the second operand. 
  For the third algorithm the accumulation of records allows to optimize
  fetching rows of the second operand from disk for some engines (MyISAM, 
  InnoDB), or to minimize the number of round-trips between the Server and
  the engine nodes (NDB Cluster).        
*/ 

class JOIN_CACHE :public Sql_alloc
{

private:

  /* Size of the offset of a record from the cache */   
  uint size_of_rec_ofs;    
  /* Size of the length of a record in the cache */
  uint size_of_rec_len;
  /* Size of the offset of a field within a record in the cache */   
  uint size_of_fld_ofs;

protected:
       
  /* 3 functions below actually do not use the hidden parameter 'this' */ 

  /* Calculate the number of bytes used to store an offset value */
  uint offset_size(uint len)
  { return (len < 256 ? 1 : len < 256*256 ? 2 : 4); }

  /* Get the offset value that takes ofs_sz bytes at the position ptr */
  ulong get_offset(uint ofs_sz, uchar *ptr)
  {
    switch (ofs_sz) {
    case 1: return uint(*ptr);
    case 2: return uint2korr(ptr);
    case 4: return uint4korr(ptr);
    }
    return 0;
  }

  /* Set the offset value ofs that takes ofs_sz bytes at the position ptr */ 
  void store_offset(uint ofs_sz, uchar *ptr, ulong ofs)
  {
    switch (ofs_sz) {
    case 1: *ptr= (uchar) ofs; return;
    case 2: int2store(ptr, (uint16) ofs); return;
    case 4: int4store(ptr, (uint32) ofs); return;
    }
  }
  
  /* 
    The maximum total length of the fields stored for a record in the cache.
    For blob fields only the sizes of the blob lengths are taken into account. 
  */
  uint length;

  /* 
    Representation of the executed multi-way join through which all needed
    context can be accessed.  
  */   
  JOIN *join;  

  /*
    JOIN_TAB of the first table that can have it's fields in the join cache. 
    That is, tables in the [start_tab, tab) range can have their fields in the
    join cache. 
    If a join tab in the range represents an SJM-nest, then all tables from the
    nest can have their fields in the join cache, too.
  */
  JOIN_TAB *start_tab;

  /* 
    The total number of flag and data fields that can appear in a record
    written into the cache. Fields with null values are always skipped 
    to save space. 
  */
  uint fields;

  /* 
    The total number of flag fields in a record put into the cache. They are
    used for table null bitmaps, table null row flags, and an optional match
    flag. Flag fields go before other fields in a cache record with the match
    flag field placed always at the very beginning of the record.
  */
  uint flag_fields;

  /* The total number of blob fields that are written into the cache */ 
  uint blobs;

  /* 
    The total number of fields referenced from field descriptors for other join
    caches. These fields are used to construct key values.
    When BKA join algorithm is employed the constructed key values serve to
    access matching rows with index lookups.
    The key values are put into a hash table when the BNLH join algorithm
    is employed and when BKAH is used for the join operation. 
  */   
  uint referenced_fields;
   
  /* 
    The current number of already created data field descriptors.
    This number can be useful for implementations of the init methods.  
  */
  uint data_field_count; 

  /* 
    The current number of already created pointers to the data field
    descriptors. This number can be useful for implementations of
    the init methods.  
  */
  uint data_field_ptr_count;
 
  /* 
    Array of the descriptors of fields containing 'fields' elements.
    These are all fields that are stored for a record in the cache. 
  */
  CACHE_FIELD *field_descr;

  /* 
    Array of pointers to the blob descriptors that contains 'blobs' elements.
  */
  CACHE_FIELD **blob_ptr;

  /* 
    This flag indicates that records written into the join buffer contain
    a match flag field. The flag must be set by the init method. 
  */
  bool with_match_flag; 
  /*
    This flag indicates that any record is prepended with the length of the
    record which allows us to skip the record or part of it without reading.
  */
  bool with_length;

  /* 
    The maximal number of bytes used for a record representation in
    the cache excluding the space for blob data. 
    For future derived classes this representation may contains some
    redundant info such as a key value associated with the record.     
  */
  uint pack_length;
  /* 
    The value of pack_length incremented by the total size of all 
    pointers of a record in the cache to the blob data. 
  */
  uint pack_length_with_blob_ptrs;

  /* 
    The total size of the record base prefix. The base prefix of record may
    include the following components:
     - the length of the record
     - the link to a record in a previous buffer.
    Each record in the buffer are supplied with the same set of the components.
  */
  uint base_prefix_length;

  /*
    The expected length of a record in the join buffer together with     
    all prefixes and postfixes
  */
  size_t avg_record_length;

  /* The expected size of the space per record in the auxiliary buffer */
  size_t avg_aux_buffer_incr;

  /* Expected join buffer space used for one record */
  size_t space_per_record; 

  /* Pointer to the beginning of the join buffer */
  uchar *buff;         
  /* 
    Size of the entire memory allocated for the join buffer.
    Part of this memory may be reserved for the auxiliary buffer.
  */ 
  size_t buff_size;
  /* The minimal join buffer size when join buffer still makes sense to use */
  size_t min_buff_size;
  /* The maximum expected size if the join buffer to be used */
  size_t max_buff_size;
  /* Size of the auxiliary buffer */ 
  size_t aux_buff_size;

  /* The number of records put into the join buffer */ 
  size_t records;
  /* 
    The number of records in the fully refilled join buffer of
    the minimal size equal to min_buff_size
  */
  size_t min_records;
  /*
    The maximum expected number of records to be put in the join buffer
    at one refill 
  */
  size_t max_records;

  /* 
    Pointer to the current position in the join buffer.
    This member is used both when writing to buffer and
    when reading from it.
  */
  uchar *pos;
  /* 
    Pointer to the first free position in the join buffer,
    right after the last record into it.
  */
  uchar *end_pos; 

  /* 
    Pointer to the beginning of the first field of the current read/write
    record from the join buffer. The value is adjusted by the 
    get_record/put_record functions.
  */
  uchar *curr_rec_pos;
  /* 
    Pointer to the beginning of the first field of the last record
    from the join buffer.
  */
  uchar *last_rec_pos;

  /* 
    Flag is set if the blob data for the last record in the join buffer
    is in record buffers rather than in the join cache.
  */
  bool last_rec_blob_data_is_in_rec_buff;

  /* 
    Pointer to the position to the current record link. 
    Record links are used only with linked caches. Record links allow to set
    connections between parts of one join record that are stored in different
    join buffers.
    In the simplest case a record link is just a pointer to the beginning of
    the record stored in the buffer.
    In a more general case a link could be a reference to an array of pointers
    to records in the buffer.
  */
  uchar *curr_rec_link;

  /* 
    This flag is set to TRUE if join_tab is the first inner table of an outer
    join and  the latest record written to the join buffer is detected to be
    null complemented after checking on conditions over the outer tables for
    this outer join operation
  */ 
  bool last_written_is_null_compl;

  /*
    The number of fields put in the join buffer of the join cache that are
    used in building keys to access the table join_tab
  */
  uint local_key_arg_fields;
  /* 
    The total number of the fields in the previous caches that are used
    in building keys to access the table join_tab
  */
  uint external_key_arg_fields;

  /* 
    This flag indicates that the key values will be read directly from the join
    buffer. It will save us building key values in the key buffer.
  */
  bool use_emb_key;
  /* The length of an embedded key value */ 
  uint emb_key_length;

  /*
    This object provides the methods to iterate over records of
    the joined table join_tab when looking for join matches between
    records from join buffer and records from join_tab.
    BNL and BNLH join algorithms retrieve all records from join_tab,
    while BKA/BKAH algorithm iterates only over those records from
    join_tab that can be accessed by look-ups with join keys built
    from records in join buffer.  
  */
  JOIN_TAB_SCAN *join_tab_scan;

  void calc_record_fields();     
  void collect_info_on_key_args();
  int alloc_fields();
  void create_flag_fields();
  void create_key_arg_fields();
  void create_remaining_fields();
  void set_constants();
  int alloc_buffer();

  /* Shall reallocate the join buffer */
  virtual int realloc_buffer();
  
  /* Check the possibility to read the access keys directly from join buffer */ 
  bool check_emb_key_usage();

  uint get_size_of_rec_offset() { return size_of_rec_ofs; }
  uint get_size_of_rec_length() { return size_of_rec_len; }
  uint get_size_of_fld_offset() { return size_of_fld_ofs; }

  uchar *get_rec_ref(uchar *ptr)
  {
    return buff+get_offset(size_of_rec_ofs, ptr-size_of_rec_ofs);
  }
  ulong get_rec_length(uchar *ptr)
  { 
    return (ulong) get_offset(size_of_rec_len, ptr);
  }
  ulong get_fld_offset(uchar *ptr)
  { 
    return (ulong) get_offset(size_of_fld_ofs, ptr);
  }

  void store_rec_ref(uchar *ptr, uchar* ref)
  {
    store_offset(size_of_rec_ofs, ptr-size_of_rec_ofs, (ulong) (ref-buff));
  }
  void store_rec_length(uchar *ptr, ulong len)
  {
    store_offset(size_of_rec_len, ptr, len);
  }
  void store_fld_offset(uchar *ptr, ulong ofs)
  {
    store_offset(size_of_fld_ofs, ptr, ofs);
  }

  /* Write record fields and their required offsets into the join buffer */ 
  uint write_record_data(uchar *link, bool *is_full);

  /* Get the total length of all prefixes of a record in the join buffer */ 
  virtual uint get_prefix_length() { return base_prefix_length; }
  /* Get maximum total length of all affixes of a record in the join buffer */
  virtual uint get_record_max_affix_length(); 

  /* 
    Shall get maximum size of the additional space per record used for
    record keys
  */
  virtual uint get_max_key_addon_space_per_record() { return 0; }

  /* 
    This method must determine for how much the auxiliary buffer should be
    incremented when a new record is added to the join buffer.
    If no auxiliary buffer is needed the function should return 0.
  */
  virtual uint aux_buffer_incr(ulong recno);

  /* Shall calculate how much space is remaining in the join buffer */ 
  virtual size_t rem_space() 
  { 
    return max(buff_size-(end_pos-buff)-aux_buff_size,0);
  }

  /* 
    Shall calculate how much space is taken by allocation of the key
    for a record in the join buffer
  */
  virtual uint extra_key_length() { return 0; }

  /*  Read all flag and data fields of a record from the join buffer */
  uint read_all_record_fields();
  
  /* Read all flag fields of a record from the join buffer */
  uint read_flag_fields();

  /* Read a data record field from the join buffer */
  uint read_record_field(CACHE_FIELD *copy, bool last_record);

  /* Read a referenced field from the join buffer */
  bool read_referenced_field(CACHE_FIELD *copy, uchar *rec_ptr, uint *len);

  /* 
    Shall skip record from the join buffer if its match flag
    is set to MATCH_FOUND
 */
  virtual bool skip_if_matched();

  /* 
    Shall skip record from the join buffer if its match flag
    commands to do so
  */
  virtual bool skip_if_not_needed_match();

  /* 
    True if rec_ptr points to the record whose blob data stay in
    record buffers
  */
  bool blob_data_is_in_rec_buff(uchar *rec_ptr)
  {
    return rec_ptr == last_rec_pos && last_rec_blob_data_is_in_rec_buff;
  }

  /* Find matches from the next table for records from the join buffer */
  virtual enum_nested_loop_state join_matching_records(bool skip_last);

  /* Shall set an auxiliary buffer up (currently used only by BKA joins) */
  virtual int setup_aux_buffer(HANDLER_BUFFER &aux_buff) 
  {
    DBUG_ASSERT(0);
    return 0;
  }

  /*
    Shall get the number of ranges in the cache buffer passed
    to the MRR interface
  */  
  virtual uint get_number_of_ranges_for_mrr() { return 0; };

  /* 
    Shall prepare to look for records from the join cache buffer that would
    match the record of the joined table read into the record buffer
  */ 
  virtual bool prepare_look_for_matches(bool skip_last)= 0;
  /* 
    Shall return a pointer to the record from join buffer that is checked
    as the next candidate for a match with the current record from join_tab.
    Each implementation of this virtual function should bare in mind
    that the record position it returns shall be exactly the position
    passed as the parameter to the implementations of the virtual functions 
    skip_next_candidate_for_match and read_next_candidate_for_match.
  */   
  virtual uchar *get_next_candidate_for_match()= 0;
  /*
    Shall check whether the given record from the join buffer has its match
    flag settings commands to skip the record in the buffer.
  */
  virtual bool skip_next_candidate_for_match(uchar *rec_ptr)= 0;
  /*
    Shall read the given record from the join buffer into the
    the corresponding record buffer
  */
  virtual void read_next_candidate_for_match(uchar *rec_ptr)= 0;

  /* 
    Shall return the location of the association label returned by 
    the multi_read_range_next function for the current record loaded
    into join_tab's record buffer
  */
  virtual uchar **get_curr_association_ptr() { return 0; };

  /* Add null complements for unmatched outer records from the join buffer */
  virtual enum_nested_loop_state join_null_complements(bool skip_last);

  /* Restore the fields of the last record from the join buffer */
  virtual void restore_last_record();

  /* Set match flag for a record in join buffer if it has not been set yet */
  bool set_match_flag_if_none(JOIN_TAB *first_inner, uchar *rec_ptr);

  enum_nested_loop_state generate_full_extensions(uchar *rec_ptr);

  /* Check matching to a partial join record from the join buffer */
  bool check_match(uchar *rec_ptr);

  /* 
    This constructor creates an unlinked join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
  */   
  JOIN_CACHE(JOIN *j, JOIN_TAB *tab)
  {
    join= j;
    join_tab= tab;
    prev_cache= next_cache= 0;
    buff= 0;
  }

  /* 
    This constructor creates a linked join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked.
  */   
  JOIN_CACHE(JOIN *j, JOIN_TAB *tab, JOIN_CACHE *prev)   
  {  
    join= j;
    join_tab= tab;
    next_cache= 0;
    prev_cache= prev;
    buff= 0;
    if (prev)
      prev->next_cache= this;
  }

public:
 
  /*
    The enumeration type Join_algorithm includes a mnemonic constant for
    each join algorithm that employs join buffers
  */

  enum Join_algorithm
  { 
    BNL_JOIN_ALG,     /* Block Nested Loop Join algorithm                  */
    BNLH_JOIN_ALG,    /* Block Nested Loop Hash Join algorithm             */
    BKA_JOIN_ALG,     /* Batched Key Access Join algorithm                 */
    BKAH_JOIN_ALG    /* Batched Key Access with Hash Table Join Algorithm */
  };

  /* 
    The enumeration type Match_flag describes possible states of the match flag
    field  stored for the records of the first inner tables of outer joins and
    semi-joins in the cases when the first match strategy is used for them.
    When a record with match flag field is written into the join buffer the
    state of the field usually is MATCH_NOT_FOUND unless this is a record of the
    first inner table of the outer join for which the on precondition (the
    condition from on expression over outer tables)  has turned out not to be 
    true. In the last case the state of the match flag is MATCH_IMPOSSIBLE.
    The state of the match flag field is changed to MATCH_FOUND as soon as
    the first full matching combination of inner tables of the outer join or
    the semi-join is discovered. 
  */
  enum Match_flag { MATCH_NOT_FOUND, MATCH_FOUND, MATCH_IMPOSSIBLE };

  /* Table to be joined with the partial join records from the cache */ 
  JOIN_TAB *join_tab;

  /* Pointer to the previous join cache if there is any */
  JOIN_CACHE *prev_cache;
  /* Pointer to the next join cache if there is any */
  JOIN_CACHE *next_cache;

  /* Shall initialize the join cache structure */ 
  virtual int init();

  /* Get the current size of the cache join buffer */ 
  size_t get_join_buffer_size() { return buff_size; }
  /* Set the size of the cache join buffer to a new value */
  void set_join_buffer_size(size_t sz) { buff_size= sz; }

  /* Get the minimum possible size of the cache join buffer */
  virtual ulong get_min_join_buffer_size();
  /* Get the maximum possible size of the cache join buffer */ 
  virtual ulong get_max_join_buffer_size(bool optimize_buff_size);

  /* Shrink the size if the cache join buffer in a given ratio */
  bool shrink_join_buffer_in_ratio(ulonglong n, ulonglong d);

  /*  Shall return the type of the employed join algorithm */
  virtual enum Join_algorithm get_join_alg()= 0;

  /* 
    The function shall return TRUE only when there is a key access
    to the join table
  */
  virtual bool is_key_access()= 0;

  /* Shall reset the join buffer for reading/writing */
  virtual void reset(bool for_writing);

  /* 
    This function shall add a record into the join buffer and return TRUE
    if it has been decided that it should be the last record in the buffer.
  */ 
  virtual bool put_record();

  /* 
    This function shall read the next record into the join buffer and return
    TRUE if there is no more next records.
  */ 
  virtual bool get_record();

  /* 
    This function shall read the record at the position rec_ptr
    in the join buffer
  */ 
  virtual void get_record_by_pos(uchar *rec_ptr);

  /* Shall return the value of the match flag for the positioned record */
  virtual enum Match_flag get_match_flag_by_pos(uchar *rec_ptr);

  /* Shall return the position of the current record */
  virtual uchar *get_curr_rec() { return curr_rec_pos; }

  /* Shall set the current record link */
  virtual void set_curr_rec_link(uchar *link) { curr_rec_link= link; }

  /* Shall return the current record link */
  virtual uchar *get_curr_rec_link()
  { 
    return (curr_rec_link ? curr_rec_link : get_curr_rec());
  }
     
  /* Join records from the join buffer with records from the next join table */ 
  enum_nested_loop_state join_records(bool skip_last);

  /* Add a comment on the join algorithm employed by the join cache */
  virtual void print_explain_comment(String *str);

  THD *thd();

  virtual ~JOIN_CACHE() {}
  void reset_join(JOIN *j) { join= j; }
  void free()
  { 
    my_free(buff);
    buff= 0;
  }   
  
  friend class JOIN_CACHE_HASHED;
  friend class JOIN_CACHE_BNL;
  friend class JOIN_CACHE_BKA;
  friend class JOIN_TAB_SCAN;
  friend class JOIN_TAB_SCAN_MRR;

};


/*
  The class JOIN_CACHE_HASHED is the base class for the classes
  JOIN_CACHE_HASHED_BNL and JOIN_CACHE_HASHED_BKA. The first of them supports
  an implementation of Block Nested Loop Hash (BNLH) Join Algorithm,
  while the second is used for a variant of the BKA Join algorithm that performs
  only one lookup for any records from join buffer with the same key value. 
  For a join cache of this class the records from the join buffer that have
  the same access key are linked into a chain attached to a key entry structure
  that either itself contains the key value, or, in the case when the keys are
  embedded, refers to its occurrence in one of the records from the chain.
  To build the chains with the same keys a hash table is employed. It is placed
  at the very end of the join buffer. The array of hash entries is allocated
  first at the very bottom of the join buffer, while key entries are placed
  before this array.
  A hash entry contains a header of the list of the key entries with the same
  hash value. 
  Each key entry is a structure of the following type:
    struct st_join_cache_key_entry {
      union { 
        uchar[] value;
        cache_ref *value_ref; // offset from the beginning of the buffer
      } hash_table_key;
      key_ref next_key; // offset backward from the beginning of hash table
      cache_ref *last_rec // offset from the beginning of the buffer
    }
  The references linking the records in a chain are always placed at the very
  beginning of the record info stored in the join buffer. The records are 
  linked in a circular list. A new record is always added to the end of this 
  list.

  The following picture represents a typical layout for the info stored in the
  join buffer of a join cache object of the JOIN_CACHE_HASHED class.
    
  buff
  V
  +----------------------------------------------------------------------------+
  |     |[*]record_1_1|                                                        |
  |     ^ |                                                                    |
  |     | +--------------------------------------------------+                 |
  |     |                           |[*]record_2_1|          |                 |
  |     |                           ^ |                      V                 |
  |     |                           | +------------------+   |[*]record_1_2|   |
  |     |                           +--------------------+-+   |               |
  |+--+ +---------------------+                          | |   +-------------+ |
  ||  |                       |                          V |                 | |
  |||[*]record_3_1|         |[*]record_1_3|              |[*]record_2_2|     | |
  ||^                       ^                            ^                   | |
  ||+----------+            |                            |                   | |
  ||^          |            |<---------------------------+-------------------+ |
  |++          | | ... mrr  |   buffer ...           ... |     |               |
  |            |            |                            |                     |
  |      +-----+--------+   |                      +-----|-------+             |
  |      V     |        |   |                      V     |       |             |
  ||key_3|[/]|[*]|      |   |                |key_2|[/]|[*]|     |             |
  |                   +-+---|-----------------------+            |             |
  |                   V |   |                       |            |             |
  |             |key_1|[*]|[*]|         |   | ... |[*]|   ...  |[*]|  ...  |   |
  +----------------------------------------------------------------------------+
                                        ^           ^            ^
                                        |           i-th entry   j-th entry
                                        hash table

  i-th hash entry:
    circular record chain for key_1:
      record_1_1
      record_1_2
      record_1_3 (points to record_1_1)
    circular record chain for key_3:
      record_3_1 (points to itself)

  j-th hash entry:
    circular record chain for key_2:
      record_2_1
      record_2_2 (points to record_2_1)

*/

class JOIN_CACHE_HASHED: public JOIN_CACHE
{

  typedef uint (JOIN_CACHE_HASHED::*Hash_func) (uchar *key, uint key_len);
  typedef bool (JOIN_CACHE_HASHED::*Hash_cmp_func) (uchar *key1, uchar *key2,
                                                    uint key_len);
  
private:

  /* Size of the offset of a key entry in the hash table */
  uint size_of_key_ofs;

  /* 
    Length of the key entry in the hash table.
    A key entry either contains the key value, or it contains a reference
    to the key value if use_emb_key flag is set for the cache.
  */ 
  uint key_entry_length;
 
  /* The beginning of the hash table in the join buffer */
  uchar *hash_table;
  /* Number of hash entries in the hash table */
  uint hash_entries;


  /* The position of the currently retrieved key entry in the hash table */
  uchar *curr_key_entry;

  /* The offset of the data fields from the beginning of the record fields */
  uint data_fields_offset;

  inline uint get_hash_idx_simple(uchar *key, uint key_len);
  inline uint get_hash_idx_complex(uchar *key, uint key_len);

  inline bool equal_keys_simple(uchar *key1, uchar *key2, uint key_len);
  inline bool equal_keys_complex(uchar *key1, uchar *key2, uint key_len);

  int init_hash_table();
  void cleanup_hash_table();
  
protected:

  /* 
    Index info on the TABLE_REF object used by the hash join
    to look for matching records
  */    
  KEY *ref_key_info;
  /* 
    Number of the key parts the TABLE_REF object used by the hash join
    to look for matching records
  */    
  uint ref_used_key_parts;

  /*
    The hash function used in the hash table,
    usually set by the init() method
  */ 
  Hash_func hash_func;
  /*
    The function to check whether two key entries in the hash table
    are equal or not, usually set by the init() method
  */ 
  Hash_cmp_func hash_cmp_func;

  /* 
    Length of a key value.
    It is assumed that all key values have the same length.
  */
  uint key_length;
  /* Buffer to store key values for probing */
  uchar *key_buff;

  /* Number of key entries in the hash table (number of distinct keys) */
  uint key_entries;

  /* The position of the last key entry in the hash table */
  uchar *last_key_entry;

  /* 
    The offset of the record fields from the beginning of the record
    representation. The record representation starts with a reference to
    the next record in the key record chain followed by the length of
    the trailing record data followed by a reference to the record segment
    in the previous cache, if any, followed by the record fields.
  */ 
  uint rec_fields_offset;

  uint get_size_of_key_offset() { return size_of_key_ofs; }

  /* 
    Get the position of the next_key_ptr field pointed to by 
    a linking reference stored at the position key_ref_ptr. 
    This reference is actually the offset backward from the
    beginning of hash table.
  */  
  uchar *get_next_key_ref(uchar *key_ref_ptr)
  {
    return hash_table-get_offset(size_of_key_ofs, key_ref_ptr);
  }

  /* 
    Store the linking reference to the next_key_ptr field at 
    the position key_ref_ptr. The position of the next_key_ptr
    field is pointed to by ref. The stored reference is actually
    the offset backward from the beginning of the hash table.
  */  
  void store_next_key_ref(uchar *key_ref_ptr, uchar *ref)
  {
    store_offset(size_of_key_ofs, key_ref_ptr, (ulong) (hash_table-ref));
  }     
  
  /* 
    Check whether the reference to the next_key_ptr field at the position
    key_ref_ptr contains  a nil value.
  */
  bool is_null_key_ref(uchar *key_ref_ptr)
  {
    ulong nil= 0;
    return memcmp(key_ref_ptr, &nil, size_of_key_ofs ) == 0;
  } 

  /* 
    Set the reference to the next_key_ptr field at the position
    key_ref_ptr equal to nil.
  */
  void store_null_key_ref(uchar *key_ref_ptr)
  {
    ulong nil= 0;
    store_offset(size_of_key_ofs, key_ref_ptr, nil);
  } 

  uchar *get_next_rec_ref(uchar *ref_ptr)
  {
    return buff+get_offset(get_size_of_rec_offset(), ref_ptr);
  }

  void store_next_rec_ref(uchar *ref_ptr, uchar *ref)
  {
    store_offset(get_size_of_rec_offset(), ref_ptr, (ulong) (ref-buff));
  } 

  /*
    Get the position of the embedded key value for the current
    record pointed to by get_curr_rec().
  */ 
  uchar *get_curr_emb_key()
  {
    return get_curr_rec()+data_fields_offset;
  }

  /*
    Get the position of the embedded key value pointed to by a reference
    stored at ref_ptr. The stored reference is actually the offset from
    the beginning of the join buffer.
  */  
  uchar *get_emb_key(uchar *ref_ptr)
  {
    return buff+get_offset(get_size_of_rec_offset(), ref_ptr);
  }

  /* 
    Store the reference to an embedded key at the position key_ref_ptr.
    The position of the embedded key is pointed to by ref. The stored
    reference is actually the offset from the beginning of the join buffer.
  */  
  void store_emb_key_ref(uchar *ref_ptr, uchar *ref)
  {
    store_offset(get_size_of_rec_offset(), ref_ptr, (ulong) (ref-buff));
  }
  
  /* Get the total length of all prefixes of a record in hashed join buffer */ 
  uint get_prefix_length() 
  { 
    return base_prefix_length + get_size_of_rec_offset();
  }

  /* 
    Get maximum size of the additional space per record used for
    the hash table with record keys
  */
  uint get_max_key_addon_space_per_record();

  /* 
    Calculate how much space in the buffer would not be occupied by
    records, key entries and additional memory for the MMR buffer.
  */ 
  size_t rem_space() 
  { 
    return max(last_key_entry-end_pos-aux_buff_size,0);
  }

  /* 
    Calculate how much space is taken by allocation of the key
    entry for a record in the join buffer
  */
  uint extra_key_length() { return key_entry_length; }

  /* 
    Skip record from a hashed join buffer if its match flag
    is set to MATCH_FOUND
  */
  bool skip_if_matched();

  /*
    Skip record from a hashed join buffer if its match flag setting 
    commands to do so
  */
  bool skip_if_not_needed_match();

  /* Search for a key in the hash table of the join buffer */
  bool key_search(uchar *key, uint key_len, uchar **key_ref_ptr);

  /* Reallocate the join buffer of a hashed join cache */
  int realloc_buffer();

  /* 
    This constructor creates an unlinked hashed join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
  */   
  JOIN_CACHE_HASHED(JOIN *j, JOIN_TAB *tab) :JOIN_CACHE(j, tab) {}

  /* 
    This constructor creates a linked hashed join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked.
  */   
  JOIN_CACHE_HASHED(JOIN *j, JOIN_TAB *tab, JOIN_CACHE *prev) 
		    :JOIN_CACHE(j, tab, prev) {}

public:

  /* Initialize a hashed join cache */       
  int init();

  /* Reset the buffer of a hashed join cache for reading/writing */
  void reset(bool for_writing);

  /* Add a record into the buffer of a hashed join cache */
  bool put_record();

  /* Read the next record from the buffer of a hashed join cache */
  bool get_record();

  /*
    Shall check whether all records in a key chain have 
    their match flags set on
  */   
  virtual bool check_all_match_flags_for_key(uchar *key_chain_ptr);

  uint get_next_key(uchar **key); 
  
  /* Get the head of the record chain attached to the current key entry */ 
  uchar *get_curr_key_chain()
  {
    return get_next_rec_ref(curr_key_entry+key_entry_length-
                            get_size_of_rec_offset());
  }
  
};


/*
  The class JOIN_TAB_SCAN is a companion class for the classes JOIN_CACHE_BNL
  and JOIN_CACHE_BNLH. Actually the class implements the iterator over the
  table joinded by BNL/BNLH join algorithm.
  The virtual functions open, next and close are called for any iteration over
  the table. The function open is called to initiate the process of the 
  iteration. The function next shall read the next record from the joined
  table. The record is read into the record buffer of the joined table.
  The record is to be matched with records from the join cache buffer. 
  The function close shall perform the finalizing actions for the iteration.
*/
   
class JOIN_TAB_SCAN: public Sql_alloc
{

private:
  /* TRUE if this is the first record from the joined table to iterate over */
  bool is_first_record;

protected:

  /* The joined table to be iterated over */
  JOIN_TAB *join_tab;
  /* The join cache used to join the table join_tab */ 
  JOIN_CACHE *cache;
  /* 
    Representation of the executed multi-way join through which
    all needed context can be accessed.  
  */   
  JOIN *join;

public:
  
  JOIN_TAB_SCAN(JOIN *j, JOIN_TAB *tab)
  {
    join= j;
    join_tab= tab;
    cache= join_tab->cache;
  }

  virtual ~JOIN_TAB_SCAN() {}
 
  /* 
    Shall calculate the increment of the auxiliary buffer for a record
    write if such a buffer is used by the table scan object 
  */
  virtual uint aux_buffer_incr(ulong recno) { return 0; }

  /* Initiate the process of iteration over the joined table */
  virtual int open();
  /* 
    Shall read the next candidate for matches with records from 
    the join buffer.
  */
  virtual int next();
  /* 
    Perform the finalizing actions for the process of iteration
    over the joined_table.
  */ 
  virtual void close();

};

/*
  The class JOIN_CACHE_BNL is used when the BNL join algorithm is
  employed to perform a join operation   
*/

class JOIN_CACHE_BNL :public JOIN_CACHE
{
private:
  /* 
    The number of the records in the join buffer that have to be
    checked yet for a match with the current record of join_tab 
    read into the record buffer.
  */
  uint rem_records;

protected:

  bool prepare_look_for_matches(bool skip_last);

  uchar *get_next_candidate_for_match();

  bool skip_next_candidate_for_match(uchar *rec_ptr);

  void read_next_candidate_for_match(uchar *rec_ptr);

public:

  /* 
    This constructor creates an unlinked BNL join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
  */   
  JOIN_CACHE_BNL(JOIN *j, JOIN_TAB *tab) :JOIN_CACHE(j, tab) {}

  /* 
    This constructor creates a linked BNL join cache. The cache is to be 
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked.
  */   
  JOIN_CACHE_BNL(JOIN *j, JOIN_TAB *tab, JOIN_CACHE *prev) 
    :JOIN_CACHE(j, tab, prev) {}

  /* Initialize the BNL cache */       
  int init();

  enum Join_algorithm get_join_alg() { return BNL_JOIN_ALG; }

  bool is_key_access() { return FALSE; }

};


/*
  The class JOIN_CACHE_BNLH is used when the BNLH join algorithm is
  employed to perform a join operation   
*/

class JOIN_CACHE_BNLH :public JOIN_CACHE_HASHED
{

protected:

  /* 
    The pointer to the last record from the circular list of the records
    that  match the join key built out of the record in the join buffer for
    the join_tab table
  */
  uchar *last_matching_rec_ref_ptr;
  /*
    The pointer to the next current  record from the circular list of the
    records that match the join key built out of the record in the join buffer
    for the join_tab table. This pointer is used by the class method 
    get_next_candidate_for_match to iterate over records from the circular
    list.
  */
  uchar *next_matching_rec_ref_ptr;

  /*
    Get the chain of records from buffer matching the current candidate
    record for join
  */
  uchar *get_matching_chain_by_join_key();

  bool prepare_look_for_matches(bool skip_last);

  uchar *get_next_candidate_for_match();

  bool skip_next_candidate_for_match(uchar *rec_ptr);

  void read_next_candidate_for_match(uchar *rec_ptr);

public:

  /* 
    This constructor creates an unlinked BNLH join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
  */   
  JOIN_CACHE_BNLH(JOIN *j, JOIN_TAB *tab) : JOIN_CACHE_HASHED(j, tab) {}

  /* 
    This constructor creates a linked BNLH join cache. The cache is to be 
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked.
  */   
  JOIN_CACHE_BNLH(JOIN *j, JOIN_TAB *tab, JOIN_CACHE *prev) 
    : JOIN_CACHE_HASHED(j, tab, prev) {}

  /* Initialize the BNLH cache */       
  int init();

  enum Join_algorithm get_join_alg() { return BNLH_JOIN_ALG; }

  bool is_key_access() { return TRUE; }

};


/*
  The class JOIN_TAB_SCAN_MRR is a companion class for the classes
  JOIN_CACHE_BKA and JOIN_CACHE_BKAH. Actually the class implements the
  iterator over the records from join_tab selected by BKA/BKAH join
  algorithm as the candidates to be joined. 
  The virtual functions open, next and close are called for any iteration over
  join_tab record candidates. The function open is called to initiate the
  process of the iteration. The function next shall read the next record from
  the set of the record candidates. The record is read into the record buffer
  of the joined table. The function close shall perform the finalizing actions
  for the iteration.
*/
   
class JOIN_TAB_SCAN_MRR: public JOIN_TAB_SCAN
{
  /* Interface object to generate key ranges for MRR */
  RANGE_SEQ_IF range_seq_funcs;

  /* Number of ranges to be processed by the MRR interface */
  uint ranges;

  /* Flag to to be passed to the MRR interface */ 
  uint mrr_mode;

  /* MRR buffer assotiated with this join cache */
  HANDLER_BUFFER mrr_buff;

  /* Shall initialize the MRR buffer */
  virtual void init_mrr_buff()
  {
    cache->setup_aux_buffer(mrr_buff);
  }

public:

  JOIN_TAB_SCAN_MRR(JOIN *j, JOIN_TAB *tab, uint flags, RANGE_SEQ_IF rs_funcs)
    :JOIN_TAB_SCAN(j, tab), range_seq_funcs(rs_funcs), mrr_mode(flags) {}

  uint aux_buffer_incr(ulong recno);

  int open();
 
  int next();

  friend class JOIN_CACHE_BKA; /* it needs to add an mrr_mode flag after JOIN_CACHE::init() call */
};

/*
  The class JOIN_CACHE_BKA is used when the BKA join algorithm is
  employed to perform a join operation   
*/

class JOIN_CACHE_BKA :public JOIN_CACHE
{
private:

  /* Flag to to be passed to the companion JOIN_TAB_SCAN_MRR object */
  uint mrr_mode;

  /* 
    This value is set to 1 by the class prepare_look_for_matches method
    and back to 0 by the class get_next_candidate_for_match method
  */
  uint rem_records;

  /*
    This field contains the current association label set by a call of
    the multi_range_read_next handler function.
    See the function JOIN_CACHE_BKA::get_curr_key_association()
  */
  uchar *curr_association;

protected:

  /* 
    Get the number of ranges in the cache buffer passed to the MRR
    interface. For each record its own range is passed.
  */
  uint get_number_of_ranges_for_mrr() { return (uint)records; }

 /*
   Setup the MRR buffer as the space between the last record put
   into the join buffer and the very end of the join buffer 
 */
  int setup_aux_buffer(HANDLER_BUFFER &aux_buff)
  {
    aux_buff.buffer= end_pos;
    aux_buff.buffer_end= buff+buff_size;
    return 0;
  }

  bool prepare_look_for_matches(bool skip_last);

  uchar *get_next_candidate_for_match();

  bool skip_next_candidate_for_match(uchar *rec_ptr);

  void read_next_candidate_for_match(uchar *rec_ptr);

public:

  /* 
    This constructor creates an unlinked BKA join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKA(JOIN *j, JOIN_TAB *tab, uint flags)
    :JOIN_CACHE(j, tab), mrr_mode(flags) {}
  /* 
    This constructor creates a linked BKA join cache. The cache is to be 
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKA(JOIN *j, JOIN_TAB *tab, uint flags, JOIN_CACHE *prev)
    :JOIN_CACHE(j, tab, prev), mrr_mode(flags) {}
  
  uchar **get_curr_association_ptr() { return &curr_association; }

  /* Initialize the BKA cache */       
  int init();

  enum Join_algorithm get_join_alg() { return BKA_JOIN_ALG; }

  bool is_key_access() { return TRUE; }

  /* Get the key built over the next record from the join buffer */
  uint get_next_key(uchar **key);

  /* Check index condition of the joined table for a record from BKA cache */
  bool skip_index_tuple(range_id_t range_info);

  void print_explain_comment(String *str);
};



/*
  The class JOIN_CACHE_BKAH is used when the BKAH join algorithm is
  employed to perform a join operation   
*/

class JOIN_CACHE_BKAH :public JOIN_CACHE_BNLH
{

private:
  /* Flag to to be passed to the companion JOIN_TAB_SCAN_MRR object */
  uint mrr_mode;

  /* 
    This flag is set to TRUE if the implementation of the MRR interface cannot
    handle range association labels and does not return them to the caller of
    the multi_range_read_next handler function. E.g. the implementation of
    the MRR inteface for the Falcon engine could not return association
    labels to the caller of multi_range_read_next.
    The flag is set by JOIN_CACHE_BKA::init() and is not ever changed.
  */       
  bool no_association;

  /* 
    This field contains the association label returned by the 
    multi_range_read_next function.
    See the function JOIN_CACHE_BKAH::get_curr_key_association()
  */
  uchar *curr_matching_chain;

protected:

  uint get_number_of_ranges_for_mrr() { return key_entries; }

  /* 
    Initialize the MRR buffer allocating some space within the join buffer.
    The entire space between the last record put into the join buffer and the
    last key entry added to the hash table is used for the MRR buffer.
  */
  int setup_aux_buffer(HANDLER_BUFFER &aux_buff)
  {
    aux_buff.buffer= end_pos;
    aux_buff.buffer_end= last_key_entry;
    return 0;
  }

  bool prepare_look_for_matches(bool skip_last);

  /*
    The implementations of the methods
    - get_next_candidate_for_match
    - skip_recurrent_candidate_for_match
    - read_next_candidate_for_match
    are inherited from the JOIN_CACHE_BNLH class
  */

public:

  /* 
    This constructor creates an unlinked BKAH join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKAH(JOIN *j, JOIN_TAB *tab, uint flags) 
    :JOIN_CACHE_BNLH(j, tab), mrr_mode(flags) {}

  /* 
    This constructor creates a linked BKAH join cache. The cache is to be 
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKAH(JOIN *j, JOIN_TAB *tab, uint flags, JOIN_CACHE *prev)
    :JOIN_CACHE_BNLH(j, tab, prev), mrr_mode(flags)  {}

  uchar **get_curr_association_ptr() { return &curr_matching_chain; }

  /* Initialize the BKAH cache */       
  int init();

  enum Join_algorithm get_join_alg() { return BKAH_JOIN_ALG; }

  /* Check index condition of the joined table for a record from BKAH cache */
  bool skip_index_tuple(range_id_t range_info);

  void print_explain_comment(String *str);
};
