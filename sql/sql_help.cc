/* Copyright (C) 2000 MySQL AB

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

#include "mysql_priv.h"

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
    1           one of the fileds didn't finded
*/

static bool init_fields(THD *thd, TABLE_LIST *tables,
			struct st_find_field *find_fields, uint count)
{
  DBUG_ENTER("init_fields");
  for (; count-- ; find_fields++)
  {
    /* We have to use 'new' here as field will be re_linked on free */
    Item_field *field= new Item_field("mysql", find_fields->table_name,
                                      find_fields->field_name);
    if (!(find_fields->field= find_field_in_tables(thd, field, tables,
						   0, TRUE, 1)))
      DBUG_RETURN(1);
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
  MEM_ROOT *mem_root= &thd->mem_root;
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
    String *new_name= new (&thd->mem_root) String;
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
  DBUG_ENTER("search_topics");
  int count= 0;

  READ_RECORD read_record_info;
  init_read_record(&read_record_info, thd, topics, select,1,0);
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
  DBUG_ENTER("search_keyword");
  int count= 0;

  READ_RECORD read_record_info;
  init_read_record(&read_record_info, thd, keywords, select,1,0);
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
  char buff[8];	// Max int length
  int count= 0;
  int iindex_topic, iindex_relations;
  Field *rtopic_id, *rkey_id;

  DBUG_ENTER("get_topics_for_keyword");

  if ((iindex_topic= find_type((char*) primary_key_name,
			       &topics->keynames, 1+2)-1)<0 ||
      (iindex_relations= find_type((char*) primary_key_name,
				   &relations->keynames, 1+2)-1)<0)
  {
    my_error(ER_CORRUPT_HELP_DB, 0);
    DBUG_RETURN(-1);
  }
  rtopic_id= find_fields[help_relation_help_topic_id].field;
  rkey_id=   find_fields[help_relation_help_keyword_id].field;

  topics->file->ha_index_init(iindex_topic);
  relations->file->ha_index_init(iindex_relations);

  rkey_id->store((longlong) key_id);
  rkey_id->get_key_image(buff, rkey_id->pack_length(), rkey_id->charset(),
			 Field::itRAW);
  int key_res= relations->file->index_read(relations->record[0],
					   (byte *)buff, rkey_id->pack_length(),
					   HA_READ_KEY_EXACT);

  for ( ;
        !key_res && key_id == (int16) rkey_id->val_int() ;
	key_res= relations->file->index_next(relations->record[0]))
  {
    char topic_id_buff[8];
    longlong topic_id= rtopic_id->val_int();
    Field *field= find_fields[help_topic_help_topic_id].field;
    field->store((longlong) topic_id);
    field->get_key_image(topic_id_buff, field->pack_length(), field->charset(),
			 Field::itRAW);

    if (!topics->file->index_read(topics->record[0], (byte *)topic_id_buff,
				  field->pack_length(), HA_READ_KEY_EXACT))
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

  init_read_record(&read_record_info, thd, categories, select,1,0);
  while (!read_record_info.read_record(&read_record_info))
  {
    if (select && !select->cond->val_int())
      continue;
    String *lname= new (&thd->mem_root) String;
    get_field(&thd->mem_root,pfname,lname);
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
  DBUG_ENTER("get_all_items_for_category");

  READ_RECORD read_record_info;
  init_read_record(&read_record_info, thd, items, select,1,0);
  while (!read_record_info.read_record(&read_record_info))
  {
    if (!select->cond->val_int())
      continue;
    String *name= new (&thd->mem_root) String();
    get_field(&thd->mem_root,pfname,name);
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

  if (protocol->send_fields(&field_list,
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
    result of protocol->send_fields
*/

int send_header_2(Protocol *protocol, bool for_category)
{
  DBUG_ENTER("send_header_2");
  List<Item> field_list;
  if (for_category)
    field_list.push_back(new Item_empty_string("source_category_name",64));
  field_list.push_back(new Item_empty_string("name",64));
  field_list.push_back(new Item_empty_string("is_it_category",1));
  DBUG_RETURN(protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
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
  for (pos= pointers; pos!=end; (*pos++= it++));

  qsort(pointers,names->elements,sizeof(String*),string_ptr_cmp);

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
    tables   list of tables, used in WHERE
    table    goal table

    error    code of error (out)

  RETURN VALUES
    #  created SQL_SELECT
*/

SQL_SELECT *prepare_simple_select(THD *thd, Item *cond, TABLE_LIST *tables,
				  TABLE *table, int *error)
{
  cond->fix_fields(thd, tables, &cond);	// can never fail
  SQL_SELECT *res= make_select(table,0,0,cond,error);
  if (*error || (res && res->check_quick(thd, 0, HA_POS_ERROR)))
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
				 new Item_string("\\",1,&my_charset_latin1));
  if (thd->is_fatal_error)
    return 0;					// OOM
  return prepare_simple_select(thd,cond,tables,table,error);
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
  DBUG_ENTER("mysqld_help");

  TABLE_LIST tables[4];
  bzero((gptr)tables,sizeof(tables));
  tables[0].alias= tables[0].real_name= (char*) "help_topic";
  tables[0].lock_type= TL_READ;
  tables[0].next_global= tables[0].next_local= &tables[1];
  tables[1].alias= tables[1].real_name= (char*) "help_category";
  tables[1].lock_type= TL_READ;
  tables[1].next_global= tables[1].next_local= &tables[2];
  tables[2].alias= tables[2].real_name= (char*) "help_relation";
  tables[2].lock_type= TL_READ;
  tables[2].next_global= tables[2].next_local= &tables[3];
  tables[3].alias= tables[3].real_name= (char*) "help_keyword";
  tables[3].lock_type= TL_READ;
  tables[0].db= tables[1].db= tables[2].db= tables[3].db= (char*) "mysql";

  List<String> topics_list, categories_list, subcategories_list;
  String name, description, example;
  int res, count_topics, count_categories, error;
  uint mlen= strlen(mask);
  MEM_ROOT *mem_root= &thd->mem_root;

  if (open_and_lock_tables(thd, tables))
    goto error;
  /*
    Init tables and fields to be usable from items

    tables do not contain VIEWs => we can pass 0 as conds
  */
  setup_tables(thd, tables, 0);
  memcpy((char*) used_fields, (char*) init_used_fields, sizeof(used_fields));
  if (init_fields(thd, tables, used_fields, array_elements(used_fields)))
    goto error;
  size_t i;
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
    int key_id;
    if (!(select=
          prepare_select_for_name(thd,mask,mlen,tables,tables[3].table,
                                  used_fields[help_keyword_name].field,&error)))
      goto error;

    count_topics=search_keyword(thd,tables[3].table,used_fields,select,&key_id);
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
                                  used_fields[help_category_name].field,&error)))
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
      if (!(select= prepare_simple_select(thd,cond_topic_by_cat,
                                          tables,tables[0].table,&error)))
        goto error;
      get_all_items_for_category(thd,tables[0].table,
				 used_fields[help_topic_name].field,
				 select,&topics_list);
      delete select;
      if (!(select= prepare_simple_select(thd,cond_cat_by_cat,tables,
						     tables[1].table,&error)))
        goto error;
      get_all_items_for_category(thd,tables[1].table,
				 used_fields[help_category_name].field,
				 select,&subcategories_list);
      delete select;
      String *cat= categories_list.head();
      if (send_header_2(protocol, true) ||
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
  send_eof(thd);

end:
  DBUG_RETURN(FALSE);
error:
  DBUG_RETURN(TRUE);
}

