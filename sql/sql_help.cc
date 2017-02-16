/* Copyright (c) 2002, 2012, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#include "sql_priv.h"
#include "unireg.h"
#include "sql_help.h"
#include "sql_table.h"                          // primary_key_name
#include "sql_base.h"               // REPORT_ALL_ERRORS, setup_tables
#include "opt_range.h"              // SQL_SELECT
#include "records.h"          // init_read_record, end_read_record

struct st_find_field
{
  const char *table_name, *field_name;
  Field *field;
};

/* Used fields */

static struct st_find_field init_used_fields[]=
{
  { "help_topic",    "help_topic_id",      0},
  { "help_topic",    "name",               0},
  { "help_topic",    "help_category_id",   0},
  { "help_topic",    "description",        0},
  { "help_topic",    "example",            0},

  { "help_category", "help_category_id",   0},
  { "help_category", "parent_category_id", 0},
  { "help_category", "name",               0},

  { "help_keyword",  "help_keyword_id",    0},
  { "help_keyword",  "name",               0},

  { "help_relation", "help_topic_id",      0},
  { "help_relation", "help_keyword_id",    0}
};

enum enum_used_fields
{
  help_topic_help_topic_id= 0,
  help_topic_name,
  help_topic_help_category_id,
  help_topic_description,
  help_topic_example,

  help_category_help_category_id,
  help_category_parent_category_id,
  help_category_name,

  help_keyword_help_keyword_id,
  help_keyword_name,

  help_relation_help_topic_id,
  help_relation_help_keyword_id
};


/*
  Fill st_find_field structure with pointers to fields

  SYNOPSIS
    init_fields()
    thd          Thread handler
    tables       list of all tables for fields
    find_fields  array of structures
    count        size of previous array

  RETURN VALUES
    0           all ok
    1           one of the fileds was not found
*/

static bool init_fields(THD *thd, TABLE_LIST *tables,
			struct st_find_field *find_fields, uint count)
{
  Name_resolution_context *context= &thd->lex->select_lex.context;
  DBUG_ENTER("init_fields");
  context->resolve_in_table_list_only(tables);
  for (; count-- ; find_fields++)
  {
    /* We have to use 'new' here as field will be re_linked on free */
    Item_field *field= new Item_field(context,
                                      "mysql", find_fields->table_name,
                                      find_fields->field_name);
    if (!(find_fields->field= find_field_in_tables(thd, field, tables, NULL,
						   0, REPORT_ALL_ERRORS, 1,
                                                   TRUE)))
      DBUG_RETURN(1);
    bitmap_set_bit(find_fields->field->table->read_set,
                   find_fields->field->field_index);
    /* To make life easier when setting values in keys */
    bitmap_set_bit(find_fields->field->table->write_set,
                   find_fields->field->field_index);
  }
  DBUG_RETURN(0);
}


/*
  Returns variants of found topic for help (if it is just single topic,
    returns description and example, or else returns only names..)

  SYNOPSIS
    memorize_variant_topic()

    thd           Thread handler
    topics        Table of topics
    count         number of alredy found topics
    find_fields   Filled array of information for work with fields

  RETURN VALUES
    names         array of names of found topics (out)

    name          name of found topic (out)
    description   description of found topic (out)
    example       example for found topic (out)

  NOTE
    Field 'names' is set only if more than one topic is found.
    Fields 'name', 'description', 'example' are set only if
    found exactly one topic.
*/

void memorize_variant_topic(THD *thd, TABLE *topics, int count,
			    struct st_find_field *find_fields,
			    List<String> *names,
			    String *name, String *description, String *example)
{
  DBUG_ENTER("memorize_variant_topic");
  MEM_ROOT *mem_root= thd->mem_root;
  if (count==0)
  {
    get_field(mem_root,find_fields[help_topic_name].field,        name);
    get_field(mem_root,find_fields[help_topic_description].field, description);
    get_field(mem_root,find_fields[help_topic_example].field,     example);
  }
  else
  {
    if (count == 1)
      names->push_back(name);
    String *new_name= new (thd->mem_root) String;
    get_field(mem_root,find_fields[help_topic_name].field,new_name);
    names->push_back(new_name);
  }
  DBUG_VOID_RETURN;
}

/*
  Look for topics by mask

  SYNOPSIS
    search_topics()
    thd 	 Thread handler
    topics	 Table of topics
    find_fields  Filled array of info for fields
    select	 Function to test for matching help topic.
		 Normally 'help_topic.name like 'bit%'

  RETURN VALUES
    #   number of topics found

    names        array of names of found topics (out)
    name         name of found topic (out)
    description  description of found topic (out)
    example      example for found topic (out)

  NOTE
    Field 'names' is set only if more than one topic was found.
    Fields 'name', 'description', 'example' are set only if
    exactly one topic was found.

*/

int search_topics(THD *thd, TABLE *topics, struct st_find_field *find_fields,
		  SQL_SELECT *select, List<String> *names,
		  String *name, String *description, String *example)
{
  int count= 0;
  READ_RECORD read_record_info;
  DBUG_ENTER("search_topics");

  /* Should never happen. As this is part of help, we can ignore this */
  if (init_read_record(&read_record_info, thd, topics, select, 1, 0, FALSE))
    DBUG_RETURN(0);

  while (!read_record_info.read_record(&read_record_info))
  {
    if (!select->cond->val_int())		// Doesn't match like
      continue;
    memorize_variant_topic(thd,topics,count,find_fields,
			   names,name,description,example);
    count++;
  }
  end_read_record(&read_record_info);

  DBUG_RETURN(count);
}

/*
  Look for keyword by mask

  SYNOPSIS
    search_keyword()
    thd          Thread handler
    keywords     Table of keywords
    find_fields  Filled array of info for fields
    select       Function to test for matching keyword.
	         Normally 'help_keyword.name like 'bit%'

    key_id       help_keyword_if of found topics (out)

  RETURN VALUES
    0   didn't find any topics matching the mask
    1   found exactly one topic matching the mask
    2   found more then one topic matching the mask
*/

int search_keyword(THD *thd, TABLE *keywords, struct st_find_field *find_fields,
                   SQL_SELECT *select, int *key_id)
{
  int count= 0;
  READ_RECORD read_record_info;
  DBUG_ENTER("search_keyword");
  /* Should never happen. As this is part of help, we can ignore this */
  if (init_read_record(&read_record_info, thd, keywords, select, 1, 0, FALSE))
    DBUG_RETURN(0);

  while (!read_record_info.read_record(&read_record_info) && count<2)
  {
    if (!select->cond->val_int())		// Dosn't match like
      continue;

    *key_id= (int)find_fields[help_keyword_help_keyword_id].field->val_int();

    count++;
  }
  end_read_record(&read_record_info);

  DBUG_RETURN(count);
}

/*
  Look for all topics with keyword

  SYNOPSIS
    get_topics_for_keyword()
    thd		 Thread handler
    topics	 Table of topics
    relations	 Table of m:m relation "topic/keyword"
    find_fields  Filled array of info for fields
    key_id	 Primary index to use to find for keyword

  RETURN VALUES
    #   number of topics found

    names        array of name of found topics (out)

    name         name of found topic (out)
    description  description of found topic (out)
    example      example for found topic (out)

  NOTE
    Field 'names' is set only if more than one topic was found.
    Fields 'name', 'description', 'example' are set only if
    exactly one topic was found.
*/

int get_topics_for_keyword(THD *thd, TABLE *topics, TABLE *relations,
			   struct st_find_field *find_fields, int16 key_id,
			   List<String> *names,
			   String *name, String *description, String *example)
{
  uchar buff[8];	// Max int length
  int count= 0;
  int iindex_topic, iindex_relations;
  Field *rtopic_id, *rkey_id;
  DBUG_ENTER("get_topics_for_keyword");

  if ((iindex_topic=
       find_type(primary_key_name, &topics->s->keynames,
                 FIND_TYPE_NO_PREFIX) - 1) < 0 ||
      (iindex_relations=
       find_type(primary_key_name, &relations->s->keynames,
                 FIND_TYPE_NO_PREFIX) - 1) < 0)
  {
    my_message(ER_CORRUPT_HELP_DB, ER(ER_CORRUPT_HELP_DB), MYF(0));
    DBUG_RETURN(-1);
  }
  rtopic_id= find_fields[help_relation_help_topic_id].field;
  rkey_id=   find_fields[help_relation_help_keyword_id].field;

  if (topics->file->ha_index_init(iindex_topic,1) ||
      relations->file->ha_index_init(iindex_relations,1))
  {
    if (topics->file->inited)
      topics->file->ha_index_end();
    my_message(ER_CORRUPT_HELP_DB, ER(ER_CORRUPT_HELP_DB), MYF(0));
    DBUG_RETURN(-1);
  }

  rkey_id->store((longlong) key_id, TRUE);
  rkey_id->get_key_image(buff, rkey_id->pack_length(), Field::itRAW);
  int key_res= relations->file->ha_index_read_map(relations->record[0],
                                                  buff, (key_part_map) 1,
                                                  HA_READ_KEY_EXACT);

  for ( ;
        !key_res && key_id == (int16) rkey_id->val_int() ;
	key_res= relations->file->ha_index_next(relations->record[0]))
  {
    uchar topic_id_buff[8];
    longlong topic_id= rtopic_id->val_int();
    Field *field= find_fields[help_topic_help_topic_id].field;
    field->store((longlong) topic_id, TRUE);
    field->get_key_image(topic_id_buff, field->pack_length(), Field::itRAW);

    if (!topics->file->ha_index_read_map(topics->record[0], topic_id_buff,
                                         (key_part_map)1, HA_READ_KEY_EXACT))
    {
      memorize_variant_topic(thd,topics,count,find_fields,
			     names,name,description,example);
      count++;
    }
  }
  topics->file->ha_index_end();
  relations->file->ha_index_end();
  DBUG_RETURN(count);
}

/*
  Look for categories by mask

  SYNOPSIS
    search_categories()
    thd			THD for init_read_record
    categories		Table of categories
    find_fields         Filled array of info for fields
    select		Function to test for if matching help topic.
			Normally 'help_vategory.name like 'bit%'
    names		List of found categories names (out)
    res_id		Primary index of found category (only if
			found exactly one category)

  RETURN VALUES
    #			Number of categories found
*/

int search_categories(THD *thd, TABLE *categories,
		      struct st_find_field *find_fields,
		      SQL_SELECT *select, List<String> *names, int16 *res_id)
{
  Field *pfname= find_fields[help_category_name].field;
  Field *pcat_id= find_fields[help_category_help_category_id].field;
  int count= 0;
  READ_RECORD read_record_info;
  DBUG_ENTER("search_categories");

  /* Should never happen. As this is part of help, we can ignore this */
  if (init_read_record(&read_record_info, thd, categories, select,1,0,FALSE))
    DBUG_RETURN(0);
  while (!read_record_info.read_record(&read_record_info))
  {
    if (select && !select->cond->val_int())
      continue;
    String *lname= new (thd->mem_root) String;
    get_field(thd->mem_root,pfname,lname);
    if (++count == 1 && res_id)
      *res_id= (int16) pcat_id->val_int();
    names->push_back(lname);
  }
  end_read_record(&read_record_info);

  DBUG_RETURN(count);
}

/*
  Look for all topics or subcategories of category

  SYNOPSIS
    get_all_items_for_category()
    thd	    Thread handler
    items   Table of items
    pfname  Field "name" in items
    select  "where" part of query..
    res     list of finded names
*/

void get_all_items_for_category(THD *thd, TABLE *items, Field *pfname,
				SQL_SELECT *select, List<String> *res)
{
  READ_RECORD read_record_info;
  DBUG_ENTER("get_all_items_for_category");

  /* Should never happen. As this is part of help, we can ignore this */
  if (init_read_record(&read_record_info, thd, items, select,1,0,FALSE))
    DBUG_VOID_RETURN;

  while (!read_record_info.read_record(&read_record_info))
  {
    if (!select->cond->val_int())
      continue;
    String *name= new (thd->mem_root) String();
    get_field(thd->mem_root,pfname,name);
    res->push_back(name);
  }
  end_read_record(&read_record_info);

  DBUG_VOID_RETURN;
}

/*
  Send to client answer for help request

  SYNOPSIS
    send_answer_1()
    protocol - protocol for sending
    s1 - value of column "Name"
    s2 - value of column "Description"
    s3 - value of column "Example"

  IMPLEMENTATION
   Format used:
   +----------+------------+------------+
   |name      |description |example     |
   +----------+------------+------------+
   |String(64)|String(1000)|String(1000)|
   +----------+------------+------------+
   with exactly one row!

  RETURN VALUES
    1		Writing of head failed
    -1		Writing of row failed
    0		Successeful send
*/

int send_answer_1(Protocol *protocol, String *s1, String *s2, String *s3)
{
  DBUG_ENTER("send_answer_1");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("name",64));
  field_list.push_back(new Item_empty_string("description",1000));
  field_list.push_back(new Item_empty_string("example",1000));

  if (protocol->send_result_set_metadata(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  protocol->prepare_for_resend();
  protocol->store(s1);
  protocol->store(s2);
  protocol->store(s3);
  if (protocol->write())
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}


/*
  Send to client help header

  SYNOPSIS
   send_header_2()
    protocol       - protocol for sending
    is_it_category - need column 'source_category_name'

  IMPLEMENTATION
   +-                    -+
   |+-------------------- | +----------+--------------+
   ||source_category_name | |name      |is_it_category|
   |+-------------------- | +----------+--------------+
   ||String(64)           | |String(64)|String(1)     |
   |+-------------------- | +----------+--------------+
   +-                    -+

  RETURN VALUES
    result of protocol->send_result_set_metadata
*/

int send_header_2(Protocol *protocol, bool for_category)
{
  DBUG_ENTER("send_header_2");
  List<Item> field_list;
  if (for_category)
    field_list.push_back(new Item_empty_string("source_category_name",64));
  field_list.push_back(new Item_empty_string("name",64));
  field_list.push_back(new Item_empty_string("is_it_category",1));
  DBUG_RETURN(protocol->send_result_set_metadata(&field_list, Protocol::SEND_NUM_ROWS |
                                                 Protocol::SEND_EOF));
}

/*
  strcmp for using in qsort

  SYNOPSIS
    strptrcmp()
    ptr1   (const void*)&str1
    ptr2   (const void*)&str2

  RETURN VALUES
    same as strcmp
*/

extern "C" int string_ptr_cmp(const void* ptr1, const void* ptr2)
{
  String *str1= *(String**)ptr1;
  String *str2= *(String**)ptr2;
  return strcmp(str1->c_ptr(),str2->c_ptr());
}

/*
  Send to client rows in format:
   column1 : <name>
   column2 : <is_it_category>

  SYNOPSIS
    send_variant_2_list()
    protocol     Protocol for sending
    names        List of names
    cat	         Value of the column <is_it_category>
    source_name  name of category for all items..

  RETURN VALUES
    -1 	Writing fail
    0	Data was successefully send
*/

int send_variant_2_list(MEM_ROOT *mem_root, Protocol *protocol,
			List<String> *names,
			const char *cat, String *source_name)
{
  DBUG_ENTER("send_variant_2_list");

  String **pointers= (String**)alloc_root(mem_root,
					  sizeof(String*)*names->elements);
  String **pos;
  String **end= pointers + names->elements;

  List_iterator<String> it(*names);
  for (pos= pointers; pos!=end; (*pos++= it++))
    ;

  my_qsort(pointers,names->elements,sizeof(String*),string_ptr_cmp);

  for (pos= pointers; pos!=end; pos++)
  {
    protocol->prepare_for_resend();
    if (source_name)
      protocol->store(source_name);
    protocol->store(*pos);
    protocol->store(cat,1,&my_charset_latin1);
    if (protocol->write())
      DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

/*
  Prepare simple SQL_SELECT table.* WHERE <Item>

  SYNOPSIS
    prepare_simple_select()
    thd      Thread handler
    cond     WHERE part of select
    table    goal table

    error    code of error (out)

  RETURN VALUES
    #  created SQL_SELECT
*/

SQL_SELECT *prepare_simple_select(THD *thd, Item *cond,
				  TABLE *table, int *error)
{
  if (!cond->fixed)
    cond->fix_fields(thd, &cond);	// can never fail

  /* Assume that no indexes cover all required fields */
  table->covering_keys.clear_all();

  SQL_SELECT *res= make_select(table, 0, 0, cond, 0, error);
  if (*error || (res && res->check_quick(thd, 0, HA_POS_ERROR)) ||
      (res && res->quick && res->quick->reset()))
  {
    delete res;
    res=0;
  }
  return res;
}

/*
  Prepare simple SQL_SELECT table.* WHERE table.name LIKE mask

  SYNOPSIS
    prepare_select_for_name()
    thd      Thread handler
    mask     mask for compare with name
    mlen     length of mask
    tables   list of tables, used in WHERE
    table    goal table
    pfname   field "name" in table

    error    code of error (out)

  RETURN VALUES
    #  created SQL_SELECT
*/

SQL_SELECT *prepare_select_for_name(THD *thd, const char *mask, uint mlen,
				    TABLE_LIST *tables, TABLE *table,
				    Field *pfname, int *error)
{
  Item *cond= new Item_func_like(new Item_field(pfname),
				 new Item_string(mask,mlen,pfname->charset()),
				 new Item_string("\\",1,&my_charset_latin1),
                                 FALSE);
  if (thd->is_fatal_error)
    return 0;					// OOM
  return prepare_simple_select(thd, cond, table, error);
}


/*
  Server-side function 'help'

  SYNOPSIS
    mysqld_help()
    thd			Thread handler

  RETURN VALUES
    FALSE Success
    TRUE  Error and send_error already commited
*/

bool mysqld_help(THD *thd, const char *mask)
{
  Protocol *protocol= thd->protocol;
  SQL_SELECT *select;
  st_find_field used_fields[array_elements(init_used_fields)];
  List<TABLE_LIST> leaves;
  TABLE_LIST tables[4];
  List<String> topics_list, categories_list, subcategories_list;
  String name, description, example;
  int count_topics, count_categories, error;
  uint mlen= strlen(mask);
  size_t i;
  MEM_ROOT *mem_root= thd->mem_root;
  DBUG_ENTER("mysqld_help");

  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("help_topic"),
                           "help_topic", TL_READ);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("help_category"),
                           "help_category", TL_READ);
  tables[2].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("help_relation"),
                           "help_relation", TL_READ);
  tables[3].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("help_keyword"),
                           "help_keyword", TL_READ);
  tables[0].next_global= tables[0].next_local= 
    tables[0].next_name_resolution_table= &tables[1];
  tables[1].next_global= tables[1].next_local= 
    tables[1].next_name_resolution_table= &tables[2];
  tables[2].next_global= tables[2].next_local= 
    tables[2].next_name_resolution_table= &tables[3];

  /*
    HELP must be available under LOCK TABLES. 
    Reset and backup the current open tables state to
    make it possible.
  */
  Open_tables_backup open_tables_state_backup;
  if (open_system_tables_for_read(thd, tables, &open_tables_state_backup))
    goto error2;

  /*
    Init tables and fields to be usable from items
    tables do not contain VIEWs => we can pass 0 as conds
  */
  thd->lex->select_lex.context.table_list=
    thd->lex->select_lex.context.first_name_resolution_table= &tables[0];
  if (setup_tables(thd, &thd->lex->select_lex.context,
                   &thd->lex->select_lex.top_join_list,
                   tables, leaves, FALSE, FALSE))
    goto error;
  memcpy((char*) used_fields, (char*) init_used_fields, sizeof(used_fields));
  if (init_fields(thd, tables, used_fields, array_elements(used_fields)))
    goto error;
  for (i=0; i<sizeof(tables)/sizeof(TABLE_LIST); i++)
    tables[i].table->file->init_table_handle_for_HANDLER();

  if (!(select=
	prepare_select_for_name(thd,mask,mlen,tables,tables[0].table,
				used_fields[help_topic_name].field,&error)))
    goto error;

  count_topics= search_topics(thd,tables[0].table,used_fields,
			      select,&topics_list,
			      &name, &description, &example);
  delete select;

  if (count_topics == 0)
  {
    int UNINIT_VAR(key_id);
    if (!(select=
          prepare_select_for_name(thd,mask,mlen,tables,tables[3].table,
                                  used_fields[help_keyword_name].field,
                                  &error)))
      goto error;

    count_topics= search_keyword(thd,tables[3].table, used_fields, select,
                                 &key_id);
    delete select;
    count_topics= (count_topics != 1) ? 0 :
                  get_topics_for_keyword(thd,tables[0].table,tables[2].table,
                                         used_fields,key_id,&topics_list,&name,
                                         &description,&example);
  }

  if (count_topics == 0)
  {
    int16 category_id;
    Field *cat_cat_id= used_fields[help_category_parent_category_id].field;
    if (!(select=
          prepare_select_for_name(thd,mask,mlen,tables,tables[1].table,
                                  used_fields[help_category_name].field,
                                  &error)))
      goto error;

    count_categories= search_categories(thd, tables[1].table, used_fields,
					select,
					&categories_list,&category_id);
    delete select;
    if (!count_categories)
    {
      if (send_header_2(protocol,FALSE))
	goto error;
    }
    else if (count_categories > 1)
    {
      if (send_header_2(protocol,FALSE) ||
	  send_variant_2_list(mem_root,protocol,&categories_list,"Y",0))
	goto error;
    }
    else
    {
      Field *topic_cat_id= used_fields[help_topic_help_category_id].field;
      Item *cond_topic_by_cat=
	new Item_func_equal(new Item_field(topic_cat_id),
			    new Item_int((int32)category_id));
      Item *cond_cat_by_cat=
	new Item_func_equal(new Item_field(cat_cat_id),
			    new Item_int((int32)category_id));
      if (!(select= prepare_simple_select(thd, cond_topic_by_cat,
                                          tables[0].table, &error)))
        goto error;
      get_all_items_for_category(thd,tables[0].table,
				 used_fields[help_topic_name].field,
				 select,&topics_list);
      delete select;
      if (!(select= prepare_simple_select(thd, cond_cat_by_cat,
                                          tables[1].table, &error)))
        goto error;
      get_all_items_for_category(thd,tables[1].table,
				 used_fields[help_category_name].field,
				 select,&subcategories_list);
      delete select;
      String *cat= categories_list.head();
      if (send_header_2(protocol, TRUE) ||
	  send_variant_2_list(mem_root,protocol,&topics_list,       "N",cat) ||
	  send_variant_2_list(mem_root,protocol,&subcategories_list,"Y",cat))
	goto error;
    }
  }
  else if (count_topics == 1)
  {
    if (send_answer_1(protocol,&name,&description,&example))
      goto error;
  }
  else
  {
    /* First send header and functions */
    if (send_header_2(protocol, FALSE) ||
	send_variant_2_list(mem_root,protocol, &topics_list, "N", 0))
      goto error;
    if (!(select=
          prepare_select_for_name(thd,mask,mlen,tables,tables[1].table,
                                  used_fields[help_category_name].field,&error)))
      goto error;
    search_categories(thd, tables[1].table, used_fields,
		      select,&categories_list, 0);
    delete select;
    /* Then send categories */
    if (send_variant_2_list(mem_root,protocol, &categories_list, "Y", 0))
      goto error;
  }
  my_eof(thd);

  close_system_tables(thd, &open_tables_state_backup);
  DBUG_RETURN(FALSE);

error:
  close_system_tables(thd, &open_tables_state_backup);

error2:
  DBUG_RETURN(TRUE);
}

