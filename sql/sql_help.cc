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
  { "help_topic", "name", 0},
  { "help_topic","description", 0},
  { "help_topic","example", 0},
  { "help_topic", "help_topic_id", 0},
  { "help_category","name", 0},
  { "help_category","help_category_id", 0},
  { "help_relation","help_topic_id", 0},
  { "help_relation","help_category_id", 0}
};

enum enum_used_fields
{
  help_topic_name=0, help_topic_description, help_topic_example,
  help_topic_help_topic_id,
  help_category_name, help_category_help_category_id,
  help_relation_help_topic_id, help_relation_help_category_id
};

/*
  Fill local used field structure with pointer to fields */

static bool init_fields(THD *thd, TABLE_LIST *tables,
			struct st_find_field *find_field,
			uint count)
{
  for (; count-- ; find_field++)
  {
    TABLE_LIST *not_used;
    /* We have to use 'new' here as field will be re_linked on free */
    Item_field *field= new Item_field("mysql", find_field->table_name,
				     find_field->field_name);
    if (!(find_field->field= find_field_in_tables(thd, field, tables,
						  &not_used,
						  TRUE)))
      return 1;
  }
  return 0;
}


#define help_charset &my_charset_latin1

/*
  Look for topics by mask

  SYNOPSIS
    search_topics()
    thd 		Thread handler
    topics		Table of topic
    select		Function to test for if matching help topic.
			Normally 'help_topic.name like 'bit%'
    pfname		Pointer to Field structure for field "name"
    names		List of founded topic's names (out)
    name		Name of founded topic (out),
			Only set if founded exactly one topic)
    description		Description of founded topic (out)
                        Only set if founded exactly one topic.
    example		Example for founded topic (out)
			Only if founded exactly one topic.
  RETURN VALUES
    #   number of topics founded
*/

int search_topics(THD *thd, TABLE *topics, struct st_find_field *find_field,
		  SQL_SELECT *select, List<char> *names,
		  char **name, char **description, char **example)
{
  DBUG_ENTER("search_functions");
  int count= 0;
  
  READ_RECORD read_record_info;
  init_read_record(&read_record_info, thd, topics, select,1,0);
  while (!read_record_info.read_record(&read_record_info))
  {
    if (!select->cond->val_int())		// Dosn't match like
      continue;

    char *lname= get_field(&thd->mem_root, find_field[help_topic_name].field);
    count++;
    if (count > 2)
    {
      names->push_back(lname);
    } 
    else if (count == 1)
    {
      *description= get_field(&thd->mem_root,
			      find_field[help_topic_description].field);
      *example= get_field(&thd->mem_root,
			  find_field[help_topic_example].field);
      *name= lname;
    }
    else
    {
      names->push_back(*name);
      names->push_back(lname);
      *name= 0;
      *description= 0;
      *example= 0;
    }
  }
  end_read_record(&read_record_info);
  DBUG_RETURN(count);
}

/*
  Look for categories by mask

  SYNOPSIS
    search_categories()
    thd			THD for init_read_record
    categories		Table of categories
    select		Function to test for if matching help topic.
			Normally 'help_topic.name like 'bit%'
    names		List of founded topic's names (out)
    res_id		Primary index of founded category (only if
			founded exactly one category)

  RETURN VALUES
    #			Number of categories founded
*/

int search_categories(THD *thd, TABLE *categories,
		      struct st_find_field *find_fields,
		      SQL_SELECT *select, List<char> *names, int16 *res_id)
{
  Field *pfname= find_fields[help_category_name].field;
  DBUG_ENTER("search_categories");
  int count= 0;
  
  READ_RECORD read_record_info;  
  init_read_record(&read_record_info, thd, categories, select,1,0);
  while (!read_record_info.read_record(&read_record_info))
  {
    if (select && !select->cond->val_int())
      continue;
    char *lname= get_field(&thd->mem_root,pfname);
    if (++count == 1 && res_id)
    {
      Field *pcat_id= find_fields[help_category_help_category_id].field;
      *res_id= (int16) pcat_id->val_int();
    }
    names->push_back(lname);
  }
  end_read_record(&read_record_info);
  
  DBUG_RETURN(count);
}


/*
  Send to client rows in format:
   column1 : <name>
   column2 : <is_it_category>

  SYNOPSIS
    send_variant_2_list()
    protocol		Protocol for sending
    names 		List of names 
    cat			Value of the column <is_it_category>

  RETURN VALUES
    -1 	Writing fail
    0	Data was successefully send
*/

int send_variant_2_list(Protocol *protocol, List<char> *names,
			const char *cat)
{
  DBUG_ENTER("send_names");
  
  List_iterator<char> it(*names);
  const char *cur_name;
  while ((cur_name= it++))
  {
    protocol->prepare_for_resend();
    protocol->store(cur_name, system_charset_info);
    protocol->store(cat, system_charset_info);
    if (protocol->write())
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}


/*
  Look for all topics of category

  SYNOPSIS
    get_all_topics_for_category()
    thd				Thread handler
    topics			Table of topics
    relations			Table of m:m relation "topic/category"
    cat_id			Primary index looked for category 
    res				List of founded topic's names (out)

  RETURN VALUES
    -1	corrupt database
    0	succesefull 
*/

int get_all_topics_for_category(THD *thd, TABLE *topics, TABLE *relations, 
				struct st_find_field *find_fields,
				int16 cat_id, List<char> *res)
{
  char buff[8];					// Max int length
  DBUG_ENTER("get_all_topics_for_category");
  
  int iindex_topic, iindex_relations;
  Field *rtopic_id, *rcat_id;
  
  if ((iindex_topic= find_type((char*) "PRIMARY",
			       &topics->keynames, 1+2)-1)<0 ||
      (iindex_relations= find_type((char*) "PRIMARY",
				   &relations->keynames, 1+2)-1)<0)
  {
    send_error(thd,ER_CORRUPT_HELP_DB);
    DBUG_RETURN(-1);
  }
  rtopic_id= find_fields[help_relation_help_topic_id].field;
  rcat_id=   find_fields[help_relation_help_category_id].field;
  
  topics->file->index_init(iindex_topic);
  relations->file->index_init(iindex_relations);
  
  rcat_id->store((longlong) cat_id);
  rcat_id->get_key_image(buff, rcat_id->pack_length(), help_charset,
			 Field::itRAW);
  int key_res= relations->file->index_read(relations->record[0],
					   (byte *)buff, rcat_id->pack_length(),
					   HA_READ_KEY_EXACT);
  
  for ( ; !key_res && cat_id == (int16) rcat_id->val_int() ;
	key_res= relations->file->index_next(relations->record[0]))
  {
    char topic_id_buff[8];
    longlong topic_id= rtopic_id->val_int();
    Field *field= find_fields[help_topic_help_topic_id].field;
    field->store((longlong) topic_id);
    field->get_key_image(topic_id_buff, field->pack_length(), help_charset,
			 Field::itRAW);

    if (!topics->file->index_read(topics->record[0], (byte *)topic_id_buff,
				  field->pack_length(),
				  HA_READ_KEY_EXACT))
      res->push_back(get_field(&thd->mem_root,
			       find_fields[help_topic_name].field));
  }
  DBUG_RETURN(0);
}


/*
  Send to client answer for help request
  
  SYNOPSIS
    send_answer_1()
    protocol - protocol for sending
    s1 - value of column "Name"
    s2 - value of column "Category"
    s3 - value of column "Description"
    s4 - value of column "Example"

  IMPLEMENTATION
   Format used:
   +----------+---------+------------+------------+
   |Name:     |Category |Description |Example     |
   +----------+---------+------------+------------+
   |String(64)|String(1)|String(1000)|String(1000)|
   +----------+---------+------------+------------+
   with exactly one row!

  RETURN VALUES
    1		Writing of head failed
    -1		Writing of row failed
    0		Successeful send
*/

int send_answer_1(Protocol *protocol, const char *s1, const char *s2, 
		  const char *s3, const char *s4)
{
  DBUG_ENTER("send_answer_1");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Name",64));
  field_list.push_back(new Item_empty_string("Category",1));
  field_list.push_back(new Item_empty_string("Description",1000));
  field_list.push_back(new Item_empty_string("Example",1000));
  
  if (protocol->send_fields(&field_list,1))
    DBUG_RETURN(1);
  
  protocol->prepare_for_resend();
  protocol->store(s1, system_charset_info);
  protocol->store(s2, system_charset_info);
  protocol->store(s3, system_charset_info);
  protocol->store(s4, system_charset_info);
  if (protocol->write())
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}


/*
  Send to client help header

  SYNOPSIS
   send_header_2()
    protocol - protocol for sending

  IMPLEMENTATION
    +----------+---------+
    |Name:     |Category |
    +----------+---------+
    |String(64)|String(1)|
    +----------+---------+

  RETURN VALUES
    result of protocol->send_fields
*/

int send_header_2(Protocol *protocol)
{
  DBUG_ENTER("send_header2");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Name",64));
  field_list.push_back(new Item_empty_string("Category",1));
  DBUG_RETURN(protocol->send_fields(&field_list,1));
}


/*
  Server-side function 'help'

  SYNOPSIS
    mysqld_help()
    thd			Thread handler

  RETURN VALUES
    0		Success
    1		Error and send_error already commited
    -1		error && send_error should be issued (normal case)
*/

int mysqld_help(THD *thd, const char *mask)
{
  Protocol *protocol= thd->protocol;
  SQL_SELECT *select= 0, *select_cat= 0;
  Item *cond_topic, *cond_cat;
  st_find_field used_fields[array_elements(init_used_fields)];
  DBUG_ENTER("mysqld_help");
  
  TABLE_LIST tables[3];
  bzero((gptr)tables,sizeof(tables));
  tables[0].alias= tables[0].real_name= (char*) "help_topic";
  tables[0].lock_type= TL_READ;
  tables[0].db= (char*) "mysql";
  tables[0].next= &tables[1];
  tables[1].alias= tables[1].real_name= (char*) "help_category";
  tables[1].lock_type= TL_READ;
  tables[1].db= (char*) "mysql";
  tables[1].next= &tables[2];
  tables[2].alias= tables[2].real_name= (char*) "help_relation";
  tables[2].lock_type= TL_READ;
  tables[2].db= (char*) "mysql";
  tables[2].next= 0;
  
  List<char> function_list, categories_list;
  char *name, *description, *example;
  int res, count_topics, count_categories, error;
  
  if (open_and_lock_tables(thd, tables))
  {
    res= -1;
    goto end;
  }
  /* Init tables and fields to be usable from items */
  setup_tables(tables);
  memcpy((char*) used_fields, (char*) init_used_fields, sizeof(used_fields)); 
  if (init_fields(thd, tables, used_fields, array_elements(used_fields)))
  {
    res= -1;
    goto end;
  }
  
  /* TODO: Find out why these are needed (should not be) */
  tables[0].table->file->init_table_handle_for_HANDLER();
  tables[1].table->file->init_table_handle_for_HANDLER();
  tables[2].table->file->init_table_handle_for_HANDLER();
  
  cond_topic= new Item_func_like(new Item_field(used_fields[help_topic_name].
						field),
				 new Item_string(mask, strlen(mask),
						 help_charset),
				 (char*) "\\");
  cond_topic->fix_fields(thd, tables, &cond_topic);	// can never fail
  select= make_select(tables[0].table,0,0,cond_topic,&error);
  if (error || (select && select->check_quick(0, HA_POS_ERROR)))
  {
    res= -1;
    goto end;
  }

  cond_cat= new Item_func_like(new Item_field(used_fields[help_category_name].
					      field),
			       new Item_string(mask, strlen(mask),
					       help_charset),
			       (char*) "\\");
  cond_cat->fix_fields(thd, tables, &cond_topic);	// can never fail
  select_cat= make_select(tables[1].table,0,0,cond_cat,&error);
  if (error || (select_cat && select_cat->check_quick(0, HA_POS_ERROR)))
  {
    res= -1;
    goto end;
  }

  res= 1;
  count_topics= search_topics(thd,tables[0].table, used_fields, select,
			      &function_list, &name, &description, &example);
  if (count_topics == 0)
  {
    int16 category_id;
    Item *cond=
      new Item_func_like(new
			 Item_field(used_fields[help_category_name].field),
			 new Item_string(mask, strlen(mask),
					 help_charset),
			 (char*) "\\");
    (void) cond->fix_fields(thd, tables, &cond);	// can never fail

    count_categories= search_categories(thd, tables[1].table, used_fields,
					select_cat, &categories_list,
					&category_id);
    if (count_categories == 1)
    {
      if (get_all_topics_for_category(thd,tables[0].table,
				      tables[2].table, used_fields,
				      category_id, &function_list))
      {
	res= -1;
	goto end;
      }
      List_iterator<char> it(function_list);
      char *cur_topic;
      char buff[1024];
      String example(buff, sizeof(buff), help_charset);
      example.length(0);

      while ((cur_topic= it++))
      {
	example.append(cur_topic);
	example.append("\n",1);
      }
      if ((send_answer_1(protocol, categories_list.head(),
			 "Y","",example.ptr())))
	goto end;
    }
    else	
    {
      if (send_header_2(protocol))
	goto end;
      if (count_categories == 0)
	search_categories(thd,tables[1].table, used_fields, (SQL_SELECT *) 0,
			  &categories_list, 0);
      if (send_variant_2_list(protocol,&categories_list,"Y"))
	goto end;
    }
  }
  else if (count_topics == 1)
  {
    if (send_answer_1(protocol,name,"N",description, example))
      goto end;
  }
  else
  {
    /* First send header and functions */
    if (send_header_2(protocol) ||
	send_variant_2_list(protocol, &function_list, "N"))
      goto end;
    search_categories(thd, tables[1].table, used_fields, select_cat,
		      &categories_list, 0);
    /* Then send categories */
    if (send_variant_2_list(protocol, &categories_list, "Y"))
      goto end;
  }
  res= 0;
  
  send_eof(thd);
end:
  delete select;
  delete select_cat;
  DBUG_RETURN(res);
}
