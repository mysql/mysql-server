#include "mysql_priv.h"


/* Union  of selects */


int mysql_union(THD *thd,LEX *lex,uint no_of_selects) 
{
  SELECT_LEX *sl, *for_order=&lex->select_lex; uint no=0; int res;
  List<Item> fields;     TABLE *table;
  for (;for_order->next;for_order=for_order->next);
  ORDER *some_order = (ORDER *)for_order->order_list.first;
  for (sl=&lex->select_lex;sl;sl=sl->next, no++)
  {
    TABLE_LIST *tables=(TABLE_LIST*) sl->table_list.first;
    if (!no) // First we do CREATE from SELECT
    {
      select_create *result;
      lex->create_info.options=HA_LEX_CREATE_TMP_TABLE;
      if ((result=new select_create(tables->db ? tables->db : thd->db,
				    NULL, &lex->create_info,
				    lex->create_list,
				    lex->key_list,
				    sl->item_list,DUP_IGNORE)))
      {
	res=mysql_select(thd,tables,sl->item_list,
			 sl->where,
			 sl->ftfunc_list,
			 (ORDER*) NULL,
			 (ORDER*) sl->group_list.first,
			 sl->having,
			 (ORDER*) some_order,
			 sl->options | thd->options,
			 result);
	if (res) 
	{
	  result->abort();
	  delete result;
	  return res;
	}
	else
	{
	  table=result->table;
	  List_iterator<Item> it(*(result->fields));
	  Item *item;
	  while ((item= it++))
	    fields.push_back(item);
	}
	delete result;
	if (reopen_table(table)) return 1;
      }
      else
	return -1;
    }
    else // Then we do INSERT from SELECT
    {
      select_result *result;
      if ((result=new select_insert(table, &fields, DUP_IGNORE)))
      {
	res=mysql_select(thd,tables,sl->item_list,
			 sl->where,
                         sl->ftfunc_list,
			 (ORDER*) some_order,
			 (ORDER*) sl->group_list.first,
			 sl->having,
			 (ORDER*) NULL,
			 sl->options | thd->options,
			 result);
	delete result;
	if (res) return 1;
      }
      else
	return -1;
    }
  }
  if (1) // Meaning if not SELECT ... INTO .... which will be done later
  {
    READ_RECORD	info;
    int error=0;
    if (send_fields(thd,fields,1)) return 1;
    SQL_SELECT	*select= new SQL_SELECT;
    select->head=table;
    select->file=*(table->io_cache);
    init_read_record(&info,thd,table,select,1,1);
    while (!(error=info.read_record(&info)) && !thd->killed)
    {
      
      if (error)
      {
	table->file->print_error(error,MYF(0));
	break;
      }
    }
    end_read_record(&info);
    delete select;
  }
  else
  {
  }
  return 0;
}
