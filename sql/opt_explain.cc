/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/** @file "EXPLAIN <command>" implementation */ 

#include "opt_explain.h"
#include "sql_select.h"
#include "sql_partition.h" // for make_used_partitions_str()

static bool mysql_explain_unit(THD *thd, SELECT_LEX_UNIT *unit,
                               select_result *result);

/**
  A base for all Explain_* classes

  Explain_* classes collect and output EXPLAIN data.

  This class hierarchy is a successor of the old select_describe() function of 5.5.
*/

class Explain
{
private:
  List<Item> items; ///< item list to feed select_result::send_data()
  Item_null *nil; ///< pre-allocated NULL item to fill empty columns in EXPLAIN

protected:
  /**
    Helper class to keep string data in MEM_ROOT before passing to Item_string

    Since Item_string constructors doesn't copy input string parameter data 
    in the most cases, those input strings must have the same lifetime as
    Item_string objects, i.e. lifetime of MEM_ROOT.
    This class allocates input parameters for Item_string objects in MEM_ROOT.
  */
  struct mem_root_str
  {
    MEM_ROOT *const mem_root;
    const char *str;
    size_t length;
    
    mem_root_str(THD *thd) : mem_root(thd->mem_root) { cleanup(); }
    void cleanup()
    {
      str= NULL;
      length= 0;
    }
    bool is_empty() const { return str == NULL; }

    bool set(const char *str_arg)
    {
      return set(str_arg, strlen(str_arg));
    }
    bool set(const String &s)
    {
      return set(s.ptr(), s.length());
    }
    /**
      Make a copy of the string in MEM_ROOT
      
      @param str_arg    string to copy
      @param length_arg input string length

      @return false if success, true if error
    */
    bool set(const char *str_arg, size_t length_arg)
    {
      if (!(str= static_cast<char *>(memdup_root(mem_root,str_arg,length_arg))))
        return true; /* purecov: inspected */
      length= length_arg;
      return false;
    }
    /**
      Make a copy of string constant

      Variant of set() usable when the str_arg argument lives longer
      than the mem_root_str instance.
    */
    void set_const(const char *str_arg)
    {
      str= str_arg;
      length= strlen(str_arg);
    }
  };

  /*
    Next "col_*" fields are intended to be filling by "explain_*()" functions.
    Then the make_list() function links these Items into "items" list.

    NOTE: NULL value or mem_root_str.is_empty()==true means that Item_null object
          will be pushed into "items" list instead.
  */
  Item_uint   *col_id; ///< "id" column: seq. number of SELECT withing the query
  mem_root_str col_select_type; ///< "select_type" column
  mem_root_str col_table_name; ///< "table" to which the row of output refers
  mem_root_str col_partitions; ///< "partitions" column
  mem_root_str col_join_type; ///< "type" column, see join_type_str array
  mem_root_str col_possible_keys; ///< "possible_keys": comma-separated list
  mem_root_str col_key; ///< "key" column: index that is actually decided to use
  mem_root_str col_key_len; ///< "key_length" column: length of the "key" above
  mem_root_str col_ref; ///< "ref":columns/constants which are compared to "key"
  Item_int    *col_rows; ///< "rows": estimated number of examined table rows
  Item_float  *col_filtered; ///< "filtered": % of rows filtered by condition
  mem_root_str col_extra; ///< "extra" column: additional information

  THD *const thd; ///< cached THD pointer
  const CHARSET_INFO *const cs; ///< cached pointer to system_charset_info
  JOIN *const join; ///< top-level JOIN (if any) provided by caller

  select_result *const external_result; ///< stream (if any) provided by caller

protected:
  explicit Explain(THD *thd_arg, JOIN *join_arg= NULL)
  : nil(NULL),
    col_select_type(thd_arg),
    col_table_name(thd_arg),
    col_partitions(thd_arg),
    col_join_type(thd_arg),
    col_possible_keys(thd_arg),
    col_key(thd_arg),
    col_key_len(thd_arg),
    col_ref(thd_arg),
    col_extra(thd_arg),
    thd(thd_arg),
    cs(system_charset_info),
    join(join_arg),
    external_result(join ? join->result : NULL)
  {
    init_columns();
  }
  virtual ~Explain() {}

public:
  bool send();

private:
  void init_columns();
  bool make_list();
  bool push(Item *item) { return items.push_back(item ? item : nil); }
  bool push(const mem_root_str &s)
  {
    if (s.is_empty())
      return items.push_back(nil);
    Item_string *item= new Item_string(s.str, s.length, cs);
    return item == NULL || items.push_back(item);
  }

protected:
  bool describe(uint8 mask) { return thd->lex->describe & mask; }

  SELECT_LEX *select_lex() const
  {
    return join ? join->select_lex : &thd->lex->select_lex;
  }

  /**
    Prepare the self-allocated result object

    For queries with top-level JOIN the caller provides pre-allocated
    select_send object. Then that JOIN object prepares the select_send
    object calling result->prepare() in JOIN::prepare(),
    result->initalize_tables() in JOIN::optimize() and result->prepare2()
    in JOIN::exec().
    However without the presence of the top-level JOIN we have to
    prepare/initialize select_send object manually.
  */
  bool prepare(select_result *result)
  {
    DBUG_ASSERT(join == NULL);
    List<Item> dummy;
    return result->prepare(dummy, select_lex()->master_unit()) ||
           result->prepare2();
  }

  virtual bool send_to(select_result *to);

  /*
    Rest of the functions are overloadable functions, those calculate and fill
    "col_*" fields with Items for further sending as EXPLAIN columns.

    "explain_*" functions return false on success and true on error (usually OOM).
  */
  virtual bool explain_id();
  virtual bool explain_select_type();
  virtual bool explain_table_name() { return false; }
  virtual bool explain_partitions() { return false; }
  virtual bool explain_join_type() { return false; }
  virtual bool explain_possible_keys() { return false; }
  /** fill col_key and and col_key_len fields together */
  virtual bool explain_key_and_len() { return false; }
  virtual bool explain_ref() { return false; }
  /** fill col_rows and col_filtered fields together */
  virtual bool explain_rows_and_filtered() { return false; }
  virtual bool explain_extra();
};


/**
  Explain_no_table class outputs a trivial EXPLAIN row with "extra" column

  This class is intended for simple cases to produce EXPLAIN output
  with "No tables used", "No matching records" etc.
  Optionally it can output number of estimated rows in the "row"
  column.

  @note This class also produces EXPLAIN rows for inner units (if any).
*/

class Explain_no_table: public Explain
{
private:
  const char *message; ///< cached "message" argument
  const ha_rows rows; ///< HA_POS_ERROR or cached "rows" argument

public:
  Explain_no_table(THD *thd_arg, JOIN *join_arg, const char *message_arg)
  : Explain(thd_arg, join_arg), message(message_arg), rows(HA_POS_ERROR)
  {}

  Explain_no_table(THD *thd_arg, const char *message_arg,
                   ha_rows rows_arg= HA_POS_ERROR)
  : Explain(thd_arg), message(message_arg), rows(rows_arg)
  {}

protected:
  virtual bool explain_rows_and_filtered();
  virtual bool explain_extra();
};


/**
  Explain_union class outputs EXPLAIN row for UNION
*/

class Explain_union : public Explain
{
public:
  Explain_union(THD *thd_arg, JOIN *join_arg) : Explain(thd_arg, join_arg)
  {
    /* it's a UNION: */
    DBUG_ASSERT(join_arg->select_lex == join_arg->unit->fake_select_lex);
  }

protected:
  virtual bool explain_id();
  virtual bool explain_table_name();
  virtual bool explain_join_type();
  virtual bool explain_extra();
};



/**
  Common base class for Explain_join and Explain_table
*/

class Explain_table_base : public Explain {
protected:
  const TABLE *table;
  key_map usable_keys;

  Explain_table_base(THD *const thd_arg, JOIN *const join_arg)
  : Explain(thd_arg, join_arg), table(NULL)
  {}

  Explain_table_base(THD *const thd_arg, TABLE *const table_arg)
  : Explain(thd_arg), table(table_arg)
  {}

  virtual bool explain_partitions();
  virtual bool explain_possible_keys();

  bool explain_key_and_len_quick(const SQL_SELECT *select);
  bool explain_key_and_len_index(int key);
  bool explain_key_and_len_index(int key, uint key_length);
  void explain_extra_common(const SQL_SELECT *select,
                            const JOIN_TAB *tab,
                            int quick_type,
                            uint keyno,
                            String *str_extra);
  void explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                     bool need_sort_arg,
                                     String *str_extra);
};


/**
  Explain_join class produces EXPLAIN output for JOINs
*/

class Explain_join : public Explain_table_base
{
private:
  bool need_tmp_table; ///< add "Using temporary" to "extra" if true
  bool need_order; ///< add "Using filesort"" to "extra" if true
  const bool distinct; ///< add "Distinct" string to "extra" column if true

  uint tabnum; ///< current tab number in join->join_tab[]
  JOIN_TAB *tab; ///< current JOIN_TAB
  int quick_type; ///< current quick type, see anon. enum at QUICK_SELECT_I
  table_map used_tables; ///< accumulate used tables bitmap
  uint last_sjm_table; ///< last materialized semi-joined table

public:
  Explain_join(THD *thd_arg, JOIN *join_arg,
               bool need_tmp_table_arg, bool need_order_arg,
               bool distinct_arg)
  : Explain_table_base(thd_arg, join_arg), need_tmp_table(need_tmp_table_arg),
    need_order(need_order_arg), distinct(distinct_arg),
    tabnum(0), used_tables(0), last_sjm_table(MAX_TABLES)
  {
    /* it is not UNION: */
    DBUG_ASSERT(join_arg->select_lex != join_arg->unit->fake_select_lex);
  }

protected:
  virtual bool send_to(select_result *to);
  virtual bool explain_table_name();
  virtual bool explain_join_type();
  virtual bool explain_key_and_len();
  virtual bool explain_ref();
  virtual bool explain_rows_and_filtered();
  virtual bool explain_extra();
};


/**
  Explain_table class produce EXPLAIN output for queries without top-level JOIN

  This class is a simplified version of the Explain_join class. It works in the
  context of queries which implementation lacks top-level JOIN object (EXPLAIN
  single-table UPDATE and DELETE).
*/

class Explain_table: public Explain_table_base
{
private:
  const SQL_SELECT *const select; ///< cached "select" argument
  const uint       key;        ///< cached "key" number argument
  const ha_rows    limit;      ///< HA_POS_ERROR or cached "limit" argument
  const bool       need_tmp_table; ///< cached need_tmp_table argument
  const bool       need_sort;  ///< cached need_sort argument

public:
  Explain_table(THD *const thd_arg, TABLE *const table_arg,
                const SQL_SELECT *select_arg,
                uint key_arg, ha_rows limit_arg,
                bool need_tmp_table_arg, bool need_sort_arg)
  : Explain_table_base(thd_arg, table_arg), select(select_arg), key(key_arg),
    limit(limit_arg),
    need_tmp_table(need_tmp_table_arg), need_sort(need_sort_arg)
  {
    usable_keys= table->keys_in_use_for_query;
  }

private:
  virtual bool explain_table_name();
  virtual bool explain_join_type();
  virtual bool explain_key_and_len();
  virtual bool explain_rows_and_filtered();
  virtual bool explain_extra();
};


static join_type calc_join_type(int quick_type)
{
  if ((quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE) ||
      (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT) ||
      (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION))
    return JT_INDEX_MERGE;
  else
    return JT_RANGE;
}


/* Explain class functions ****************************************************/


/**
  Explain class main function

  This function:
    a) allocates a select_send object (if no one pre-allocated available),
    b) calculates and sends whole EXPLAIN data.

  @return false if success, true if error
*/

bool Explain::send()
{
  DBUG_ENTER("Explain::send");
  /* Don't log this into the slow query log */
  thd->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                         SERVER_QUERY_NO_GOOD_INDEX_USED);

  select_result *result;
  if (external_result == NULL)
  {
    /* Create select_result object if the caller doesn't provide one: */
    if (!(result= new select_send))
      DBUG_RETURN(true); /* purecov: inspected */
    if (thd->send_explain_fields(result) || prepare(result))
    {
      delete result;
      DBUG_RETURN(true); /* purecov: inspected */
    }
  }
  else
  {
    result= external_result;
    external_result->reset_offset_limit_cnt();
  }

  if (nil == NULL && !(nil= new Item_null))
    DBUG_RETURN(true); /* purecov: inspected */
  bool ret= send_to(result);

  if (ret && join)
    join->error= 1; /* purecov: inspected */

  for (SELECT_LEX_UNIT *unit= select_lex()->first_inner_unit();
       unit && !ret;
       unit= unit->next_unit())
    ret= mysql_explain_unit(thd, unit, result);

  if (external_result == NULL)
  {
    if (ret)
      result->abort_result_set(); /* purecov: inspected */
    else
      result->send_eof();
    delete result;
  }
  DBUG_RETURN(ret);
}


/**
  Reset all "col_*" fields
*/

void Explain::init_columns()
{
  col_id= NULL;
  col_select_type.cleanup();
  col_table_name.cleanup();
  col_partitions.cleanup();
  col_join_type.cleanup();
  col_possible_keys.cleanup();
  col_key.cleanup();
  col_key_len.cleanup();
  col_ref.cleanup();
  col_rows= NULL;
  col_filtered= NULL;
  col_extra.cleanup();
}


/**
  Calculate EXPLAIN column values and link them into "items" list

  @return false if success, true if error
*/

bool Explain::make_list()
{
  if (explain_id() ||
      explain_select_type() ||
      explain_table_name() ||
      explain_partitions() ||
      explain_join_type() ||
      explain_possible_keys() ||
      explain_key_and_len() ||
      explain_ref() ||
      explain_rows_and_filtered() ||
      explain_extra())
    return true; /* purecov: inspected */

  /*
    NOTE: the number/types of items pushed into item_list must be in sync with
    EXPLAIN column types as they're "defined" in THD::send_explain_fields()
  */
  return push(col_id) ||
         push(col_select_type) ||
         push(col_table_name) ||
         (describe(DESCRIBE_PARTITIONS) && push(col_partitions)) ||
         push(col_join_type) ||
         push(col_possible_keys) ||
         push(col_key) ||
         push(col_key_len) ||
         push(col_ref) ||
         push(col_rows) ||
         (describe(DESCRIBE_EXTENDED) && push(col_filtered)) ||
         push(col_extra);
}


/**
  Make "items" list and send it to select_result output stream

  @note An overloaded Explain_join::send_to() function sends
        one item list per each JOIN::join_tab[] element.

  @return false if success, true if error
*/

bool Explain::send_to(select_result *to)
{
  const bool ret= make_list() || to->send_data(items);
  items.empty();
  init_columns();
  return ret;
}


bool Explain::explain_id()
{
  col_id= new Item_uint(select_lex()->select_number);
  return col_id == NULL;
}


bool Explain::explain_select_type()
{
  if (select_lex()->type)
    col_select_type.set(select_lex()->type);
  else if (select_lex()->first_inner_unit() || select_lex()->next_select())
    col_select_type.set_const("PRIMARY");
  else
    col_select_type.set_const("SIMPLE");
  return col_select_type.is_empty();
}


bool Explain::explain_extra()
{
  col_extra.set_const("");
  return false;
}


/* Explain_no_table class functions *******************************************/


bool Explain_no_table::explain_rows_and_filtered()
{
  if (rows == HA_POS_ERROR)
    return false;
  col_rows= new Item_int(rows, MY_INT64_NUM_DECIMAL_DIGITS);
  return col_rows == NULL;
}


bool Explain_no_table::explain_extra()
{
  return col_extra.set(message);
}


/* Explain_union class functions **********************************************/


bool Explain_union::explain_id()
{
  return false;
}


bool Explain_union::explain_table_name()
{
  SELECT_LEX *last_select= join->unit->first_select()->last_select();
  // # characters needed to print select_number of last select
  int last_length= (int)log10((double)last_select->select_number)+1;

  SELECT_LEX *sl= join->unit->first_select();
  uint len= 6, lastop= 0;
  char table_name_buffer[NAME_LEN];
  memcpy(table_name_buffer, STRING_WITH_LEN("<union"));
  /*
    - len + lastop: current position in table_name_buffer
    - 6 + last_length: the number of characters needed to print
      '...,'<last_select->select_number>'>\0'
  */
  for (;
       sl && len + lastop + 6 + last_length < NAME_CHAR_LEN;
       sl= sl->next_select())
  {
    len+= lastop;
    lastop= my_snprintf(table_name_buffer + len, NAME_CHAR_LEN - len,
                        "%u,", sl->select_number);
  }
  if (sl || len + lastop >= NAME_CHAR_LEN)
  {
    memcpy(table_name_buffer + len, STRING_WITH_LEN("...,"));
    len+= 4;
    lastop= my_snprintf(table_name_buffer + len, NAME_CHAR_LEN - len,
                        "%u,", last_select->select_number);
  }
  len+= lastop;
  table_name_buffer[len - 1]= '>';  // change ',' to '>'

  return col_table_name.set(table_name_buffer, len);
}


bool Explain_union::explain_join_type()
{
  col_join_type.set_const(join_type_str[JT_ALL]);
  return false;
}


bool Explain_union::explain_extra()
{
  /*
    here we assume that the query will return at least two rows, so we
    show "filesort" in EXPLAIN. Of course, sometimes we'll be wrong
    and no filesort will be actually done, but executing all selects in
    the UNION to provide precise EXPLAIN information will hardly be
    appreciated :)
  */
  if (join->unit->global_parameters->order_list.first)
  {
    col_extra.set_const("Using filesort");
    return false;
  }
  return Explain::explain_extra();
}


/* Explain_table_base class functions *****************************************/


bool Explain_table_base::explain_partitions()
{
#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (!table->derived_select_number && table->part_info)
  {
    String s;
    make_used_partitions_str(table->part_info, &s);
    return col_partitions.set(s);
  }
#endif
  return false;
}


bool Explain_table_base::explain_possible_keys()
{
  if (usable_keys.is_clear_all())
    return false;

  StringBuffer<512> str_possible_keys(cs);

  for (uint j= 0 ; j < table->s->keys ; j++)
  {
    if (usable_keys.is_set(j))
    {
      if (str_possible_keys.length())
        str_possible_keys.append(',');
      str_possible_keys.append(table->key_info[j].name,
                               strlen(table->key_info[j].name), cs);
    }
  }
  if (str_possible_keys.length())
    return col_possible_keys.set(str_possible_keys);
  return false;
}


bool Explain_table_base::explain_key_and_len_quick(const SQL_SELECT *select)
{
  DBUG_ASSERT(select && select->quick);

  StringBuffer<512> str_key(cs);
  StringBuffer<512> str_key_len(cs);

  select->quick->add_keys_and_lengths(&str_key, &str_key_len);
  return col_key.set(str_key) || col_key_len.set(str_key_len);
}


bool Explain_table_base::explain_key_and_len_index(int key)
{
  DBUG_ASSERT(key != MAX_KEY);
  return explain_key_and_len_index(key, table->key_info[key].key_length);
}


bool Explain_table_base::explain_key_and_len_index(int key, uint key_length)
{
  DBUG_ASSERT(key != MAX_KEY);

  const KEY *key_info= table->key_info + key;
  char buff_key_len[24];
  const int length= longlong2str(key_length, buff_key_len, 10) - buff_key_len;
  return col_key.set(key_info->name) || col_key_len.set(buff_key_len, length);
}


void Explain_table_base::explain_extra_common(const SQL_SELECT *select,
                                              const JOIN_TAB *tab,
                                              int quick_type,
                                              uint keyno,
                                              String *str_extra)
{
  if ((keyno != MAX_KEY && keyno == table->file->pushed_idx_cond_keyno &&
       table->file->pushed_idx_cond) || (tab && tab->cache_idx_cond))
    str_extra->append(STRING_WITH_LEN("; Using index condition"));

  switch (quick_type) {
  case QUICK_SELECT_I::QS_TYPE_ROR_UNION:
  case QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT:
  case QUICK_SELECT_I::QS_TYPE_INDEX_MERGE:
    str_extra->append(STRING_WITH_LEN("; Using "));
    select->quick->add_info_string(str_extra);
    break;
  default: ;
  }

  if (select)
  {
    if (tab && tab->use_quick == QS_DYNAMIC_RANGE)
    {
      /* 4 bits per 1 hex digit + terminating '\0' */
      char buf[MAX_KEY / 4 + 1];
      str_extra->append(STRING_WITH_LEN("; Range checked for each "
                                        "record (index map: 0x"));
      str_extra->append(tab->keys.print(buf));
      str_extra->append(')');
    }
    else if (select->cond)
    {
      const Item *pushed_cond= table->file->pushed_cond;

      if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
          pushed_cond)
      {
        str_extra->append(STRING_WITH_LEN("; Using where with pushed condition"));
        if (describe(DESCRIBE_EXTENDED))
        {
          str_extra->append(STRING_WITH_LEN(": "));
          ((Item *)pushed_cond)->print(str_extra, QT_ORDINARY);
        }
      }
      else
        str_extra->append(STRING_WITH_LEN("; Using where"));
    }
  }
  if (table->reginfo.not_exists_optimize)
    str_extra->append(STRING_WITH_LEN("; Not exists"));

  if (quick_type == QUICK_SELECT_I::QS_TYPE_RANGE &&
      !(((QUICK_RANGE_SELECT*)(select->quick))->mrr_flags &
       (HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED)))
  {
    /*
      During normal execution of a query, multi_range_read_init() is
      called to initialize MRR. If HA_MRR_SORTED is set at this point,
      multi_range_read_init() for any native MRR implementation will
      revert to default MRR because they cannot produce sorted output
      currently.
      Calling multi_range_read_init() can potentially be costly, so it
      is not done when executing an EXPLAIN. We therefore make the
      assumption that HA_MRR_SORTED means no MRR. If some MRR native
      implementation will support sorted output in the future, a
      function "bool mrr_supports_sorted()" should be added in the
      handler.
    */
    str_extra->append(STRING_WITH_LEN("; Using MRR"));
  }
}

void Explain_table_base::explain_tmptable_and_filesort(bool need_tmp_table_arg,
                                                       bool need_sort_arg,
                                                       String *str_extra)
{
  if (need_tmp_table_arg)
    str_extra->append(STRING_WITH_LEN("; Using temporary"));
  if (need_sort_arg)
    str_extra->append(STRING_WITH_LEN("; Using filesort"));
}


/* Explain_join class functions ***********************************************/


bool Explain_join::send_to(select_result *to)
{
  for (; tabnum < join->tables; tabnum++)
  {
    tab= join->join_tab + tabnum;
    table= tab->table;
    usable_keys= tab->keys;
    quick_type= -1;

    if (tab->type == JT_ALL && tab->select && tab->select->quick)
    {
      quick_type= tab->select->quick->get_type();
      tab->type= calc_join_type(quick_type);
    }

    if (Explain_table_base::send_to(external_result))
      return true; /* purecov: inspected */

    used_tables|= table->map;
  }
  return false;
}


bool Explain_join::explain_table_name()
{
  if (table->derived_select_number)
  {
    /* Derived table name generation */
    char table_name_buffer[NAME_LEN];
    const size_t len= my_snprintf(table_name_buffer,
                                  sizeof(table_name_buffer) - 1,
                                  "<derived%u>", table->derived_select_number);
    return col_table_name.set(table_name_buffer, len);
  }
  else
    return col_table_name.set(table->pos_in_table_list->alias);
}


bool Explain_join::explain_join_type()
{
  col_join_type.set_const(join_type_str[tab->type]);
  return false;
}


bool Explain_join::explain_key_and_len()
{
  if (tab->ref.key_parts)
    return explain_key_and_len_index(tab->ref.key, tab->ref.key_length);
  else if (tab->type == JT_INDEX_SCAN)
    return explain_key_and_len_index(tab->index);
  else if (tab->select && tab->select->quick)
    return explain_key_and_len_quick(tab->select);
  else
  {
    const TABLE_LIST *table_list= table->pos_in_table_list;
    if (table_list->schema_table &&
        table_list->schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
    {
      StringBuffer<512> str_key(cs);
      const char *f_name;
      int f_idx;
      if (table_list->has_db_lookup_value)
      {
        f_idx= table_list->schema_table->idx_field1;
        f_name= table_list->schema_table->fields_info[f_idx].field_name;
        str_key.append(f_name, strlen(f_name), cs);
      }
      if (table_list->has_table_lookup_value)
      {
        if (table_list->has_db_lookup_value)
          str_key.append(',');
        f_idx= table_list->schema_table->idx_field2;
        f_name= table_list->schema_table->fields_info[f_idx].field_name;
        str_key.append(f_name, strlen(f_name), cs);
      }
      if (str_key.length())
        return col_key.set(str_key);
    }
  }
  return false;
}


bool Explain_join::explain_ref()
{
  if (tab->ref.key_parts)
  {
    StringBuffer<512> str_ref(cs);

    for (uint part_no= 0; part_no < tab->ref.key_parts; part_no++)
    {
      const store_key *const s_key= tab->ref.key_copy[part_no];
      if (s_key == NULL)
        continue;

      if (str_ref.length())
        str_ref.append(',');
      str_ref.append(s_key->name(), strlen(s_key->name()), cs);
    }
    return col_ref.set(str_ref);
  }
  return false;
}


bool Explain_join::explain_rows_and_filtered()
{
  if (table->pos_in_table_list->schema_table)
    return false;

  double examined_rows;
  if (tab->select && tab->select->quick)
    examined_rows= rows2double(tab->select->quick->records);
  else if (tab->type == JT_INDEX_SCAN || tab->type == JT_ALL)
  {
    if (tab->limit)
      examined_rows= rows2double(tab->limit);
    else
    {
      table->pos_in_table_list->fetch_number_of_rows();
      examined_rows= rows2double(table->file->stats.records);
    }
  }
  else
    examined_rows= join->best_positions[tabnum].records_read;

  col_rows= new Item_int((longlong) (ulonglong) examined_rows,
                         MY_INT64_NUM_DECIMAL_DIGITS);
  if (col_rows == NULL)
    return true; /* purecov: inspected */

  /* Add "filtered" field */
  if (describe(DESCRIBE_EXTENDED))
  {
    float f= 0.0;
    if (examined_rows)
      f= 100.0 * join->best_positions[tabnum].records_read / examined_rows;
    col_filtered= new Item_float(f, 2);
    return col_filtered == NULL;
  }
  return false;
}


bool Explain_join::explain_extra()
{
  StringBuffer<512> str_extra(cs);

  if (tab->info)
    col_extra.set(tab->info);
  else if (tab->packed_info & TAB_INFO_HAVE_VALUE)
  {
    if (tab->packed_info & TAB_INFO_USING_INDEX)
      str_extra.append(STRING_WITH_LEN("; Using index"));
    if (tab->packed_info & TAB_INFO_USING_WHERE)
      str_extra.append(STRING_WITH_LEN("; Using where"));
    if (tab->packed_info & TAB_INFO_FULL_SCAN_ON_NULL)
      str_extra.append(STRING_WITH_LEN("; Full scan on NULL key"));
    /* Skip initial "; "*/
    const char *str= str_extra.ptr();
    uint32 len= str_extra.length();
    if (len)
    {
      str += 2;
      len -= 2;
    }
    col_extra.set(str, len);
  }
  else
  {
    const SQL_SELECT *select= tab->select;
    uint keyno= MAX_KEY;
    if (tab->ref.key_parts)
      keyno= tab->ref.key;
    else if (select && select->quick)
      keyno = select->quick->index;

    explain_extra_common(select, tab, quick_type, keyno, &str_extra);

    const TABLE_LIST *table_list= table->pos_in_table_list;
    if (table_list->schema_table &&
        table_list->schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
    {
      if (!table_list->table_open_method)
        str_extra.append(STRING_WITH_LEN("; Skip_open_table"));
      else if (table_list->table_open_method == OPEN_FRM_ONLY)
        str_extra.append(STRING_WITH_LEN("; Open_frm_only"));
      else
        str_extra.append(STRING_WITH_LEN("; Open_full_table"));
      if (table_list->has_db_lookup_value &&
          table_list->has_table_lookup_value)
        str_extra.append(STRING_WITH_LEN("; Scanned 0 databases"));
      else if (table_list->has_db_lookup_value ||
               table_list->has_table_lookup_value)
        str_extra.append(STRING_WITH_LEN("; Scanned 1 database"));
      else
        str_extra.append(STRING_WITH_LEN("; Scanned all databases"));
    }
    if (((tab->type == JT_INDEX_SCAN || tab->type == JT_CONST) &&
         table->covering_keys.is_set(tab->index)) ||
        (quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT &&
         !((QUICK_ROR_INTERSECT_SELECT*) select->quick)->need_to_fetch_row) ||
        table->key_read)
    {
      if (quick_type == QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX)
      {
        QUICK_GROUP_MIN_MAX_SELECT *qgs=
          (QUICK_GROUP_MIN_MAX_SELECT *) select->quick;
        str_extra.append(STRING_WITH_LEN("; Using index for group-by"));
        qgs->append_loose_scan_type(&str_extra);
      }
      else
        str_extra.append(STRING_WITH_LEN("; Using index"));
    }

    explain_tmptable_and_filesort(need_tmp_table, need_order, &str_extra);
    need_tmp_table= need_order= false;

    if (distinct && test_all_bits(used_tables, thd->lex->used_tables))
      str_extra.append(STRING_WITH_LEN("; Distinct"));

    if (tab->loosescan_match_tab)
      str_extra.append(STRING_WITH_LEN("; LooseScan"));

    if (tab->flush_weedout_table)
      str_extra.append(STRING_WITH_LEN("; Start temporary"));
    if (tab->check_weed_out_table)
      str_extra.append(STRING_WITH_LEN("; End temporary"));
    else if (tab->do_firstmatch)
    {
      if (tab->do_firstmatch == join->join_tab - 1)
        str_extra.append(STRING_WITH_LEN("; FirstMatch"));
      else
      {
        str_extra.append(STRING_WITH_LEN("; FirstMatch("));
        TABLE *prev_table= tab->do_firstmatch->table;
        if (prev_table->derived_select_number)
        {
          char namebuf[NAME_LEN];
          /* Derived table name generation */
          int len= my_snprintf(namebuf, sizeof(namebuf)-1,
                               "<derived%u>",
                               prev_table->derived_select_number);
          str_extra.append(namebuf, len);
        }
        else
          str_extra.append(prev_table->pos_in_table_list->alias);
        str_extra.append(STRING_WITH_LEN(")"));
      }
    }
    uint sj_strategy= join->best_positions[tabnum].sj_strategy;
    if (sj_is_materialize_strategy(sj_strategy))
    {
      if (join->best_positions[tabnum].n_sj_tables == 1)
        str_extra.append(STRING_WITH_LEN("; Materialize"));
      else
      {
        last_sjm_table= tabnum + join->best_positions[tabnum].n_sj_tables - 1;
        str_extra.append(STRING_WITH_LEN("; Start materialize"));
      }
      if (sj_strategy == SJ_OPT_MATERIALIZE_SCAN)
          str_extra.append(STRING_WITH_LEN("; Scan"));
    }
    else if (last_sjm_table == tabnum)
    {
      str_extra.append(STRING_WITH_LEN("; End materialize"));
    }

    if (tab->has_guarded_conds())
      str_extra.append(STRING_WITH_LEN("; Full scan on NULL key"));

    if (tabnum > 0 && tab[-1].next_select == sub_select_cache)
    {
      str_extra.append(STRING_WITH_LEN("; Using join buffer ("));
      if ((tab->use_join_cache & JOIN_CACHE::ALG_BNL))
        str_extra.append(STRING_WITH_LEN("Block Nested Loop)"));
      else if ((tab->use_join_cache & JOIN_CACHE::ALG_BKA))
        str_extra.append(STRING_WITH_LEN("Batched Key Access)"));
      else if ((tab->use_join_cache & JOIN_CACHE::ALG_BKA_UNIQUE))
        str_extra.append(STRING_WITH_LEN("Batched Key Access (unique))"));
      else
        DBUG_ASSERT(0); /* purecov: inspected */
    }

    /* Skip initial "; "*/
    const char *str= str_extra.ptr();
    uint32 len= str_extra.length();
    if (len)
    {
      str += 2;
      len -= 2;
    }
    col_extra.set(str, len);
  }
  return col_extra.is_empty();
}


/* Explain_table class functions **********************************************/


bool Explain_table::explain_table_name()
{
  return col_table_name.set(table->alias);
}


bool Explain_table::explain_join_type()
{
  join_type jt;
  if (select && select->quick)
    jt= calc_join_type(select->quick->get_type());
  else
    jt= JT_ALL;

  col_join_type.set_const(join_type_str[jt]);
  return false;
}


bool Explain_table::explain_key_and_len()
{
  if (key != MAX_KEY)
    return explain_key_and_len_index(key);
  if (select && select->quick)
    return explain_key_and_len_quick(select);
  return false;
}


bool Explain_table::explain_rows_and_filtered()
{
  double examined_rows;
  if (select && select->quick)
    examined_rows= rows2double(select->quick->records);
  else if (!select && !need_sort && limit != HA_POS_ERROR)
    examined_rows= rows2double(limit);
  else
  {
    table->pos_in_table_list->fetch_number_of_rows();
    examined_rows= rows2double(table->file->stats.records);

  }
  col_rows= new Item_int((longlong) (ulonglong) examined_rows,
                         MY_INT64_NUM_DECIMAL_DIGITS);
  if (col_rows == NULL)
    return true; /* purecov: inspected */

  if (describe(DESCRIBE_EXTENDED))
  {
    col_filtered= new Item_float(100.0, 2);
    if (col_filtered == NULL)
      return true; /* purecov: inspected */
  }
  return false;
}


bool Explain_table::explain_extra()
{
  StringBuffer<512> str_extra(cs);
  const uint keyno= (select && select->quick) ? select->quick->index : key;
  const int quick_type= (select && select->quick) ? select->quick->get_type() 
                                                  : -1;
  explain_extra_common(select, NULL, quick_type, keyno, &str_extra);
  explain_tmptable_and_filesort(need_tmp_table, need_sort, &str_extra);

  /* Skip initial "; "*/
  const char *str= str_extra.ptr();
  uint32 len= str_extra.length();
  if (len)
  {
    str += 2;
    len -= 2;
  }
  return col_extra.set(str, len);
}


/**
  EXPLAIN functionality for insert_select, multi_update and multi_delete

  This class objects substitute insert_select, multi_update and multi_delete
  data interceptor objects to implement EXPLAIN for INSERT, REPLACE and
  multi-table UPDATE and DELETE queries.
  explain_send class object initializes tables like insert_select, multi_update
  or multi_delete data interceptor do, but it suppress table data modification
  by the underlying interceptor object.
  Thus, we can use explain_send object in the context of EXPLAIN INSERT/
  REPLACE/UPDATE/DELETE query like we use select_send in the context of
  EXPLAIN SELECT command:
    1) in presence of lex->describe flag we pass explain_send object to the
       mysql_select() function,
    2) it call prepare(), prepare2() and initialize_tables() functions to
       mark modified tables etc.

*/

class explain_send : public select_send {
protected:
  /*
    As far as we use explain_send object in a place of select_send, explain_send
    have to pass multiple invocation of its prepare(), prepare2() and
    initialize_tables() functions, since JOIN::exec() of subqueries runs
    these functions of select_send multiple times by design.
    insert_select, multi_update and multi_delete class functions are not intended
    for multiple invocations, so "prepared", "prepared2" and "initialized" flags
    guard data interceptor object from function re-invocation.
  */
  bool prepared;    ///< prepare() is done
  bool prepared2;   ///< prepare2() is done
  bool initialized; ///< initialize_tables() is done
  
  /**
    Pointer to underlying insert_select, multi_update or multi_delete object
  */
  select_result_interceptor *interceptor;

public:
  explain_send(select_result_interceptor *interceptor_arg)
  : prepared(false), prepared2(false), initialized(false),
    interceptor(interceptor_arg)
  {}

protected:
  virtual int prepare(List<Item> &list, SELECT_LEX_UNIT *u)
  {
    if (prepared)
      return false;
    prepared= true;
    return select_send::prepare(list, u) || interceptor->prepare(list, u);
  }

  virtual int prepare2(void)
  {
    if (prepared2)
      return false;
    prepared2= true;
    return select_send::prepare2() || interceptor->prepare2();
  }

  virtual bool initialize_tables(JOIN *join)
  {
    if (initialized)
      return false;
    initialized= true;
    return select_send::initialize_tables(join) ||
           interceptor->initialize_tables(join);
  }

  virtual void cleanup()
  {
    select_send::cleanup();
    interceptor->cleanup();
  }
};


/******************************************************************************
  External function implementations
******************************************************************************/


/**
  Send a message as an "extra" column value

  This function forms the 1st row of the QEP output with a simple text message.
  This is useful to explain such trivial cases as "No tables used" etc.

  @note Also this function explains the rest of QEP (subqueries or joined
        tables if any).

  @param thd      current THD
  @param join     JOIN
  @param message  text message for the "extra" column.

  @return false if success, true if error
*/

bool explain_no_table(THD *thd, JOIN *join, const char *message)
{
  DBUG_ENTER("explain_no_table");
  const bool ret= Explain_no_table(thd, join, message).send();
  DBUG_RETURN(ret);
}


/**
  Send a message as an "extra" column value

  This function forms the 1st row of the QEP output with a simple text message.
  This is useful to explain such trivial cases as "No tables used" etc.

  @note Also this function explains the rest of QEP (subqueries if any).

  @param thd      current THD
  @param message  text message for the "extra" column.
  @param rows     HA_POS_ERROR or a value for the "rows" column.

  @return false if success, true if error
*/

bool explain_no_table(THD *thd, const char *message, ha_rows rows)
{
  DBUG_ENTER("explain_no_table");
  const bool ret= Explain_no_table(thd, message, rows).send();
  DBUG_RETURN(ret);

}


/**
  EXPLAIN handling for single-table UPDATE and DELETE queries

  Send to the client a QEP data set for single-table EXPLAIN UPDATE/DELETE
  queries. As far as single-table UPDATE/DELETE are implemented without
  the regular JOIN tree, we can't reuse mysql_explain_union() directly,
  thus we deal with this single table in a special way and then call
  mysql_explain_union() for subqueries (if any).

  @param thd            current THD
  @param table          TABLE object to update/delete rows in the UPDATE/DELETE
                        query.
  @param select         SQL_SELECT object that represents quick access functions
                        and WHERE clause.
  @param key            MAX_KEY or and index number of the key that was chosen
                        to access table data.
  @param limit          HA_POS_ERROR or LIMIT value.
  @param need_tmp_table true if it requires temporary table -- "Using temporary"
                        string in the "extra" column.
  @param need_sort      true if it requires filesort() -- "Using filesort"
                        string in the "extra" column.

  @return false if success, true if error
*/

bool explain_single_table_modification(THD *thd,
                                       TABLE *table,
                                       const SQL_SELECT *select,
                                       uint key,
                                       ha_rows limit,
                                       bool need_tmp_table,
                                       bool need_sort)
{
  DBUG_ENTER("explain_single_table_modification");
  const bool ret= Explain_table(thd, table, select, key, limit,
                                need_tmp_table, need_sort).send();
  DBUG_RETURN(ret);
}


/**
  EXPLAIN handling for EXPLAIN SELECT queries

  Send QEP to the client.

  @param thd             current THD
  @param join            JOIN
  @param need_tmp_table  true if it requires a temporary table --
                         "Using temporary" string in the "extra" column.
  @param need_order      true if it requires filesort() -- "Using filesort"
                         string in the "extra" column.
  @param distinct        true if there is the DISTINCT clause (not optimized
                         out) -- "Distinct" string in the "extra" column.

  @return false if success, true if error
*/

bool explain_query_specification(THD *thd, JOIN *join, bool need_tmp_table,
                                 bool need_order, bool distinct)
{
  DBUG_ENTER("explain_query_specification");
  DBUG_PRINT("info", ("Select %p, type %s",
		      join->select_lex, join->select_lex->type));
  bool ret;
  if (join->select_lex == join->unit->fake_select_lex)
    ret= Explain_union(thd, join).send();
  else
    ret= Explain_join(thd, join, need_tmp_table, need_order, distinct).send();
  DBUG_RETURN(ret);
}


/**
  EXPLAIN handling for INSERT, REPLACE and multi-table UPDATE/DELETE queries

  Send to the client a QEP data set for data-modifying commands those have a
  regular JOIN tree (INSERT...SELECT, REPLACE...SELECT and multi-table
  UPDATE and DELETE queries) like mysql_select() does for SELECT queries in
  the "describe" mode.

  @note @see explain_single_table_modification() for single-table
        UPDATE/DELETE EXPLAIN handling.

  @note Unlike the mysql_select function, explain_multi_table_modification
        calls abort_result_set() itself in the case of failure (OOM etc.)
        since explain_multi_table_modification() uses internally created
        select_result stream.

  @param thd     current THD
  @param result  pointer to select_insert, multi_delete or multi_update object:
                 the function uses it to call result->prepare(),
                 result->prepare2() and result->initialize_tables() only but
                 not to modify table data or to send a result to client.
  @return false if success, true if error
*/

bool explain_multi_table_modification(THD *thd,
                                      select_result_interceptor *result)
{
  DBUG_ENTER("explain_multi_table_modification");
  explain_send explain(result);
  bool res= explain_query_expression(thd, &explain);
  DBUG_RETURN(res);
}


/**
  EXPLAIN handling for SELECT and table-modifying queries that have JOIN

  Send to the client a QEP data set for SELECT or data-modifying commands
  those have a regular JOIN tree (INSERT...SELECT, REPLACE...SELECT and
  multi-table UPDATE and DELETE queries) like mysql_select() does for SELECT
  queries in the "describe" mode.

  @note @see explain_single_table_modification() for single-table
        UPDATE/DELETE EXPLAIN handling.

  @note explain_query_expression() calls abort_result_set() itself in the
        case of failure (OOM etc.) since explain_multi_table_modification()
        uses internally created select_result stream.

  @param thd     current THD
  @param result  pointer to select_result, select_insert, multi_delete or
                 multi_update object: the function uses it to call
                 result->prepare(), result->prepare2() and
                 result->initialize_tables() only but not to modify table data
                 or to send a result to client.
  @return false if success, true if error
*/

bool explain_query_expression(THD *thd, select_result *result)
{
  DBUG_ENTER("explain_query_expression");
  const bool res= thd->send_explain_fields(result) ||
                  mysql_explain_unit(thd, &thd->lex->unit, result) ||
                  thd->is_error();
  /*
    The code which prints the extended description is not robust
    against malformed queries, so skip it if we have an error.
  */
  if (!res && (thd->lex->describe & DESCRIBE_EXTENDED) &&
      thd->lex->sql_command == SQLCOM_SELECT) // TODO: implement for INSERT/etc
  {
    StringBuffer<1024> str;
    /*
      The warnings system requires input in utf8, @see mysqld_show_warnings().
    */
    thd->lex->unit.print(&str, enum_query_type(QT_TO_SYSTEM_CHARSET |
                                               QT_SHOW_SELECT_NUMBER));
    str.append('\0');
    push_warning(thd, Sql_condition::WARN_LEVEL_NOTE, ER_YES, str.ptr());
  }
  if (res)
    result->abort_result_set();
  else
    result->send_eof();
  DBUG_RETURN(res);
}


/**
  Explain UNION or subqueries of the unit

  If the unit is a UNION, explain it as a UNION. Otherwise explain nested
  subselects.

  @param thd            thread object
  @param unit           unit object
  @param result         result stream to send QEP dataset

  @return false if success, true if error
*/
bool mysql_explain_unit(THD *thd, SELECT_LEX_UNIT *unit, select_result *result)
{
  DBUG_ENTER("mysql_explain_unit");
  bool res= 0;
  SELECT_LEX *first= unit->first_select();

  for (SELECT_LEX *sl= first;
       sl;
       sl= sl->next_select())
  {
    // drop UNCACHEABLE_EXPLAIN, because it is for internal usage only
    uint8 uncacheable= (sl->uncacheable & ~UNCACHEABLE_EXPLAIN);
    sl->type= (((&thd->lex->select_lex)==sl)?
	       (sl->first_inner_unit() || sl->next_select() ? 
		"PRIMARY" : "SIMPLE"):
	       ((sl == first)?
		((sl->linkage == DERIVED_TABLE_TYPE) ?
		 "DERIVED":
		 ((uncacheable & UNCACHEABLE_DEPENDENT) ?
		  "DEPENDENT SUBQUERY":
		  (uncacheable?"UNCACHEABLE SUBQUERY":
		   "SUBQUERY"))):
		((uncacheable & UNCACHEABLE_DEPENDENT) ?
		 "DEPENDENT UNION":
		 uncacheable?"UNCACHEABLE UNION":
		 "UNION")));
    sl->options|= SELECT_DESCRIBE;
  }
  if (unit->is_union())
  {
    unit->fake_select_lex->select_number= UINT_MAX; // just for initialization
    unit->fake_select_lex->type= "UNION RESULT";
    unit->fake_select_lex->options|= SELECT_DESCRIBE;
    res= unit->prepare(thd, result, SELECT_NO_UNLOCK | SELECT_DESCRIBE) ||
         unit->optimize();
    if (!res)
      unit->explain();
  }
  else
  {
    thd->lex->current_select= first;
    unit->set_limit(unit->global_parameters);
    res= mysql_select(thd,
                      first->table_list.first,
                      first->with_wild, first->item_list,
                      first->where,
                      first->order_list.elements +
                      first->group_list.elements,
                      first->order_list.first,
                      first->group_list.first,
                      first->having,
                      thd->lex->proc_list.first,
                      first->options | thd->variables.option_bits | SELECT_DESCRIBE,
                      result, unit, first);
  }
  DBUG_RETURN(res || thd->is_error());
}


