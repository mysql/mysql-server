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
#include "sql_select.h"                         // For select_describe
#include "sql_acl.h"

/***************************************************************************
** Get help on string
***************************************************************************/

MI_INFO *open_help_file(THD *thd, const char *name)
{
  char path[FN_REFLEN];
  (void) sprintf(path,"%s/mysql_help/%s",mysql_data_home,name);
  MI_INFO *res= 0;
  if (!(res= mi_open(path,O_RDONLY,HA_OPEN_WAIT_IF_LOCKED)))
  {
    send_error(thd,ER_CORRUPT_HELP_DB);
    return 0;
  }
  mi_extra(res,HA_EXTRA_WAIT_LOCK,0);
  return res;
}

#define size_hf_func_id 4                 /* func_id int unsigned,    */
#define size_hf_name 64                   /* name varchar(64),        */
#define size_hf_url 128                   /* url varchar(128),        */
#define size_hf_description sizeof(char*) /* description text,        */
#define size_hf_example sizeof(char*)     /* example text,            */
#define size_hf_min_args 16               /* min_args tinyint,        */
#define size_hf_max_args 16               /* max_args tinyint,        */
#define size_hf_date_created 8            /* date_created datetime,   */
#define size_hf_last_modified 8           /* last_modified timestamp, */

#define offset_hf_func_id 1
#define offset_hf_name (offset_hf_func_id+size_hf_func_id)
#define offset_hf_url (offset_hf_name+size_hf_name)
#define offset_hf_description (offset_hf_url+size_hf_url)
#define offset_hf_example (offset_hf_description+size_hf_description)
#define offset_hf_min_args (offset_hf_example+size_hf_example)
#define offset_hf_max_args (offset_hf_min_args+size_hf_min_args)
#define offset_hf_date_created (offset_hf_max_args+size_hf_max_args)
#define offset_hf_last_modified (offset_hf_date_created+size_hf_date_created)

#define HELP_LEAF_SIZE (offset_hf_last_modified+size_hf_last_modified)

class help_leaf{
public:
  char record[HELP_LEAF_SIZE];

  inline const char *get_name()
    {
      return &record[offset_hf_name];
    }

  inline const char *get_description()
    {
      return *((char**)&record[199/*offset_hf_description*/]);
    }

  inline const char *get_example()
    {
      return *((char**)&record[209/*offset_hf_example*/]);
    }

  void prepare_fields()
    {
      const char *name= get_name();
      const char *c= name + size_hf_name - 1;
      while (*c==' ') c--;
      int len= c-name+1;
      ((char*)name)[len]= '\0';
    }
};

int search_functions(MI_INFO *file_leafs, const char *mask, 
		     List<String> *names, 
		     String **name, String **description, String **example)
{
  DBUG_ENTER("search_functions");
  int count= 0;

  if(mi_scan_init(file_leafs))
    DBUG_RETURN(-1);

  help_leaf leaf;

  while (!mi_scan(file_leafs,(byte*)&leaf))
  {
    leaf.prepare_fields();

    const char *lname= leaf.get_name();
    if (wild_case_compare(system_charset_info,lname,mask))
      continue;
    count++;

    if (count>2)
    {
      String *s= new String(lname,system_charset_info);
      if (!s->copy())
	names->push_back(s);
    } 
    else if (count==1)
    {
      *description= new String(leaf.get_description(),system_charset_info);
      *example= new String(leaf.get_example(),system_charset_info);
      *name= new String(lname,system_charset_info);
      (*description)->copy();
      (*example)->copy();
      (*name)->copy();
    }
    else
    {
      names->push_back(*name);
      delete *description;
      delete *example;
      *name= 0;
      *description= 0;
      *example= 0;
      
      String *s= new String(lname,system_charset_info);
      if (!s->copy())
	names->push_back(s);
    }
  }
  
  DBUG_RETURN(count);
}

#define size_hc_cat_id 2        /*  cat_id smallint,         */
#define size_hc_name 64         /*  name varchar(64),        */
#define size_hc_url 128         /*  url varchar(128),        */
#define size_hc_date_created 8  /*  date_created datetime,   */
#define size_hc_last_modified 8 /*  last_modified timestamp, */

#define offset_hc_cat_id        0
#define offset_hc_name          (offset_hc_cat_id+size_hc_cat_id)
#define offset_hc_url           (offset_hc_name+size_hc_name)
#define offset_hc_date_created  (offset_hc_url+size_hc_url)
#define offset_hc_last_modified (offset_hc_date_created+size_hc_date_created)

#define HELP_CATEGORY_SIZE (offset_hc_last_modified+size_hc_last_modified)

class help_category{
public:
  char record[HELP_CATEGORY_SIZE];

  inline int16 get_cat_id()
    {
      return sint2korr(&record[offset_hc_cat_id]);
    }

  inline const char *get_name()
    {
      return &record[offset_hc_name];
    }

  void prepare_fields()
    {
      const char *name= get_name();
      const char *c= name + size_hc_name - 1;
      while (*c==' ') c--;
      int len= c-name+1;
      ((char*)name)[len]= '\0';
    }
};

int search_categories(THD *thd, 
		      const char *mask, List<String> *names, int16 *res_id)
{
  DBUG_ENTER("search_categories");
  int count= 0;

  MI_INFO *file_categories= 0;
  if (!(file_categories= open_help_file(thd,"function_category_name")))
    DBUG_RETURN(-1);

  if(mi_scan_init(file_categories))
  {
    mi_close(file_categories);
    DBUG_RETURN(-1);
  }

  help_category category;


  while (!mi_scan(file_categories,(byte*)&category))
  {
    category.prepare_fields();

    const char *lname= category.get_name();
    if (mask && wild_case_compare(system_charset_info,lname,mask))
      continue;
    count++;

    if (count==1 && res_id)
      *res_id= category.get_cat_id();
      
    String *s= new String(lname,system_charset_info);
    if (!s->copy())
      names->push_back(s);
  }

  mi_close(file_categories);
  DBUG_RETURN(count);
}

int send_variant_2_list(THD *thd, List<String> *names, my_bool is_category)
{
  DBUG_ENTER("send_names");

  List_iterator<String> it(*names);
  String *cur_name;
  String *packet= &thd->packet;
  while ((cur_name = it++))
  {
    packet->length(0);
    net_store_data(packet, cur_name->ptr());
    net_store_data(packet, is_category ? "Y" : "N");
    if (SEND_ROW(thd,2,(char*) packet->ptr(),packet->length()))
      DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

#define size_hcn_cat_id 2  /* cat_id smallint, */
#define size_hcn_func_id 4 /* func_id int,     */

#define offset_hcn_cat_id 1
#define offset_hcn_func_id (offset_hcn_cat_id+size_hcn_cat_id)

#define HELP_CATEGORY_NAME_SIZE (offset_hcn_func_id + size_hcn_func_id)

class help_category_leaf{
public:
  char record[HELP_CATEGORY_NAME_SIZE];

  inline int16 get_cat_id()
    {
      return sint2korr(&record[offset_hcn_cat_id]);
    }

  inline int get_func_id()
    {
      return sint3korr(&record[offset_hcn_func_id]);
    }
};

int get_all_names_for_category(THD *thd,MI_INFO *file_leafs, 
			       int16 cat_id, List<String> *res)
{
  DBUG_ENTER("get_all_names_for_category");

  MI_INFO *file_names_categories= 0;
  if (!(file_names_categories= open_help_file(thd,"function_category")))
    DBUG_RETURN(1);

  help_category_leaf cat_leaf;
  help_leaf leaf;
  int key_res= mi_rkey(file_names_categories, (byte*)&cat_leaf, 0, 
		       (const byte*)&cat_id,2,HA_READ_KEY_EXACT);

  while (!key_res && cat_leaf.get_cat_id()==cat_id)
  {
    int leaf_id= cat_leaf.get_func_id();

    if (!mi_rkey(file_leafs, (byte*)&leaf, 0, 
		 (const byte*)&leaf_id,4,HA_READ_KEY_EXACT))
    {
      leaf.prepare_fields();
      String *s= new String(leaf.get_name(),system_charset_info);
      if (!s->copy())
	res->push_back(s);
    }
    
    key_res= mi_rnext(file_names_categories, (byte*)&cat_leaf, 0);
  }

  mi_close(file_names_categories);

  DBUG_RETURN(0);
}

int send_answer_1(THD *thd, const char *s1, const char *s2, 
		  const char *s3, const char *s4)
{
  DBUG_ENTER("send_answer_1");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("name",64));
  field_list.push_back(new Item_empty_string("is_category",1));
  field_list.push_back(new Item_empty_string("description",1000));
  field_list.push_back(new Item_empty_string("example",1000));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  String *packet= &thd->packet;
  packet->length(0);
  net_store_data(packet, s1);
  net_store_data(packet, s2);
  net_store_data(packet, s3);
  net_store_data(packet, s4);
  
  if (SEND_ROW(thd,field_list.elements,(char*) packet->ptr(),packet->length()))
    DBUG_RETURN(-1);

  DBUG_RETURN(0);
}

int send_header_2(THD *thd)
{
  DBUG_ENTER("send_header2");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("name",64));
  field_list.push_back(new Item_empty_string("is_category",1));
  DBUG_RETURN(send_fields(thd,field_list,1));
}

int mysqld_help (THD *thd, const char *mask)
{
  DBUG_ENTER("mysqld_help");

  MI_INFO *file_leafs= 0;
  if (!(file_leafs= open_help_file(thd,"function")))
    DBUG_RETURN(1);

  List<String> function_list, categories_list;
  String *name, *description, *example;
  int res;

  int count= search_functions(file_leafs, mask,
			      &function_list,&name,&description,&example);
  if (count<0)
  {
    res= 1;
    goto end;
  }
  else if (count==0)
  {
    int16 category_id;
    count= search_categories(thd, mask, &categories_list, &category_id);
    if (count<0)
    {
      res= 1;
      goto end;
    }
    else if (count==1)
    {
      if ((res= get_all_names_for_category(thd, file_leafs,
				     category_id,&function_list)))
	goto end;
      List_iterator<String> it(function_list);
      String *cur_leaf, example;
      while ((cur_leaf = it++))
      {
	example.append(*cur_leaf);
	example.append("\n",1);
      }
      if ((res= send_answer_1(thd, categories_list.head()->ptr(),
				       "Y","",example.ptr())))
	goto end;
    }
    else	
    {
      if ((res= send_header_2(thd)) ||
	  (count==0 && 
	   (search_categories(thd, 0, &categories_list, 0)<0 && 
	    (res= 1))) ||
	  (res= send_variant_2_list(thd,&categories_list,true)))
	goto end;
    }
  }
  else if (count==1)
  {
    if ((res= send_answer_1(thd,name->ptr(),"N",
			   description->ptr(), example->ptr())))
      goto end;
  }
  else if((res= send_header_2(thd)) ||
	  (res= send_variant_2_list(thd,&function_list,false)) ||
	  (search_categories(thd, mask, &categories_list, 0)<0 && 
	   (res=1)) ||
	  (res= send_variant_2_list(thd,&categories_list,true)))
  {
    goto end;
  }

  send_eof(thd);

end:
  mi_close(file_leafs);
  DBUG_RETURN(res);
}
