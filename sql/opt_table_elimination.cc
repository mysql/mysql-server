/**
  @file

  @brief
    Table Elimination Module

  @defgroup Table_Elimination Table Elimination Module
  @{
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "my_bit.h"
#include "sql_select.h"

/*
  OVERVIEW

  The module has one entry point - eliminate_tables() function, which one 
  needs to call (once) at some point before the join optimization.
  eliminate_tables() operates over the JOIN structures. Logically, it
  removes the right sides of outer join nests. Physically, it changes the
  following members:

  * Eliminated tables are marked as constant and moved to the front of the
    join order.

  * In addition to this, they are recorded in JOIN::eliminated_tables bitmap.

  * Items that became disused because they were in the ON expression of an 
    eliminated outer join are notified by means of the Item tree walk which 
    calls Item::mark_as_eliminated_processor for every item
    - At the moment the only Item that cares whether it was eliminated is 
      Item_subselect with its Item_subselect::eliminated flag which is used
      by EXPLAIN code to check if the subquery should be shown in EXPLAIN.

  Table elimination is redone on every PS re-execution.
*/

class Value_dep : public Sql_alloc
{
public:
  enum {
    VALUE_FIELD,
    VALUE_TABLE,
  } type; /* Type of the object */
  
  Value_dep(): bound(FALSE), next(NULL)
  {}

  bool bound;
  Value_dep *next;
};

class Field_value;
class Table_value;
class Outer_join_module;
class Key_module;

/*
  A table field. There is only one such object for any tblX.fieldY
  - the field epends on its table and equalities
  - expressions that use the field are its dependencies
*/
class Field_value : public Value_dep
{
public:
  Field_value(Table_value *table_arg, Field *field_arg) :
    table(table_arg), field(field_arg)
  {
    type= Value_dep::VALUE_FIELD;
  }

  Table_value *table; /* Table this field is from */
  Field *field;
  
  /* 
    Field_deps that belong to one table form a linked list. list members are
    ordered by field_index 
  */
  Field_value *next_table_field;
  uint bitmap_offset; /* Offset of our part of the bitmap */
};


/*
  A table.
  - table depends on any of its unique keys
  - has its fields and embedding outer join as dependency.
*/
class Table_value : public Value_dep
{
public:
  Table_value(TABLE *table_arg) : 
    table(table_arg), fields(NULL), keys(NULL), outer_join_dep(NULL)
  {
    type= Value_dep::VALUE_TABLE;
  }
  TABLE *table;
  Field_value *fields; /* Ordered list of fields that belong to this table */
  Key_module *keys; /* Ordered list of Unique keys in this table */
  Outer_join_module *outer_join_dep; /* Innermost eliminable outer join we're in */
};


/*
  A 'module'
*/

class Module_dep : public Sql_alloc
{
public:
  enum {
    MODULE_EXPRESSION,
    MODULE_MULTI_EQUALITY,
    MODULE_UNIQUE_KEY,
    MODULE_OUTER_JOIN
  } type; /* Type of the object */
  
  /* 
    Used to make a linked list of elements that became bound and thus can
    make elements that depend on them bound, too.
  */
  Module_dep *next; 
  uint unknown_args;

  Module_dep() : next(NULL), unknown_args(0) {}
};



/*
  A "tbl.column= expr" equality dependency.  tbl.column depends on fields 
  used in expr.
*/
class Equality_module : public Module_dep
{
public:
  Field_value *field;
  Item  *expression;
  
  /* Used during condition analysis only, similar to KEYUSE::level */
  uint level;
};


/*
  A Unique key.
   - Unique key depends on all of its components
   - Key's table is its dependency
*/
class Key_module: public Module_dep
{
public:
  Key_module(Table_value *table_arg, uint keyno_arg, uint n_parts_arg) :
    table(table_arg), keyno(keyno_arg), next_table_key(NULL)
  {
    type= Module_dep::MODULE_UNIQUE_KEY;
    unknown_args= n_parts_arg;
  }
  Table_value *table; /* Table this key is from */
  uint keyno;
  /* Unique keys form a linked list, ordered by keyno */
  Key_module *next_table_key;
};



/*
  An outer join nest that is subject to elimination
  - it depends on all tables inside it
  - has its parent outer join as dependency
*/
class Outer_join_module: public Module_dep
{
public:
  Outer_join_module(TABLE_LIST *table_list_arg, uint n_children) : 
    table_list(table_list_arg), parent(NULL)
  {
    type= Module_dep::MODULE_OUTER_JOIN;
    unknown_args= n_children;
  }
  /* 
    Outer join we're representing. This can be a join nest or one table that
    is outer join'ed.
  */
  TABLE_LIST *table_list;
  
  /* Parent eliminable outer join, if any */
  Outer_join_module *parent;
};


/*
  Table elimination context
*/
class Table_elimination
{
public:
  Table_elimination(JOIN *join_arg) : join(join_arg), n_outer_joins(0)
  {
    bzero(table_deps, sizeof(table_deps));
  }

  JOIN *join;
  /* Array of equality dependencies */
  Equality_module *equality_deps;
  uint n_equality_deps; /* Number of elements in the array */

  /* tablenr -> Table_value* mapping. */
  Table_value *table_deps[MAX_KEY];

  /* Outer joins that are candidates for elimination */
  List<Outer_join_module> oj_deps;
  uint n_outer_joins;

  /* Bitmap of how expressions depend on bits */
  MY_BITMAP expr_deps;
};

static 
bool build_eq_deps_for_cond(Table_elimination *te, Equality_module **fdeps, 
                            uint *and_level, Item *cond, 
                            table_map usable_tables);
static 
bool add_eq_dep(Table_elimination *te, Equality_module **eq_dep, 
                uint and_level,
                Item_func *cond, Item *left, Item *right, 
                table_map usable_tables);
static 
Equality_module *merge_func_deps(Equality_module *start, Equality_module *new_fields, 
                              Equality_module *end, uint and_level);

static Table_value *get_table_value(Table_elimination *te, TABLE *table);
static Field_value *get_field_value(Table_elimination *te, Field *field);
static 
void run_elimination_wave(Table_elimination *te, Module_dep *bound_modules);
void eliminate_tables(JOIN *join);
static void mark_as_eliminated(JOIN *join, TABLE_LIST *tbl);

#ifndef DBUG_OFF
static void dbug_print_deps(Table_elimination *te);
#endif 
/*******************************************************************************************/

/*
  Produce Eq_dep elements for given condition.

  SYNOPSIS
    build_eq_deps_for_cond()
      te                    Table elimination context 
      fdeps          INOUT  Put produced equality conditions here
      and_level      INOUT  AND-level (like in add_key_fields)
      cond                  Condition to process
      usable_tables         Tables which fields we're interested in. That is,
                            Equality_dep represent "tbl.col=expr" and we'll
                            produce them only if tbl is in usable_tables.
  DESCRIPTION
    This function is modeled after add_key_fields()
*/

static 
bool build_eq_deps_for_cond(Table_elimination *te, Equality_module **fdeps, 
                            uint *and_level, Item *cond, 
                            table_map usable_tables)
{
  if (cond->type() == Item_func::COND_ITEM)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Equality_module *org_key_fields= *fdeps;
    
    /* AND/OR */
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item=li++))
      {
        if (build_eq_deps_for_cond(te, fdeps, and_level, item, usable_tables))
          return TRUE;
      }
      /* 
        TODO: inject here a "if we have {t.col=const AND t.col=smth_else}, then
        remove the second part" logic.
      */
      for (; org_key_fields != *fdeps ; org_key_fields++)
        org_key_fields->level= *and_level;
    }
    else
    {
      (*and_level)++;
      if (build_eq_deps_for_cond(te, fdeps, and_level, li++, usable_tables))
        return TRUE;
      Item *item;
      while ((item=li++))
      {
        Equality_module *start_key_fields= *fdeps;
        (*and_level)++;
        if (build_eq_deps_for_cond(te, fdeps, and_level, item, usable_tables))
          return TRUE;
        *fdeps= merge_func_deps(org_key_fields, start_key_fields, *fdeps,
                                ++(*and_level));
      }
    }
    return FALSE;
  }

  if (cond->type() != Item::FUNC_ITEM)
    return FALSE;

  Item_func *cond_func= (Item_func*) cond;
  Item **args= cond_func->arguments();

  switch (cond_func->functype()) {
  case Item_func::IN_FUNC:
  {
    if (cond_func->argument_count() == 2)
    {
      if (add_eq_dep(te, fdeps, *and_level, cond_func, args[0], args[1], 
                     usable_tables) || 
          add_eq_dep(te, fdeps, *and_level, cond_func, args[1], args[0], 
                 usable_tables))
        return TRUE;
    }
  }
  case Item_func::BETWEEN:
  {
    Item *fld;
    if (!((Item_func_between*)cond)->negated &&
        (fld= args[0]->real_item())->type() == Item::FIELD_ITEM &&
        args[1]->eq(args[2], ((Item_field*)fld)->field->binary()))
    {
      if (add_eq_dep(te, fdeps, *and_level, cond_func, args[0], args[1],
                     usable_tables) || 
          add_eq_dep(te, fdeps, *and_level, cond_func, args[1], args[0],
                     usable_tables))
        return TRUE;
    }
    break;
  }
  case Item_func::EQ_FUNC:
  case Item_func::EQUAL_FUNC:
  {
    add_eq_dep(te, fdeps, *and_level, cond_func, args[0], args[1], 
               usable_tables);
    add_eq_dep(te, fdeps, *and_level, cond_func, args[1], args[0],
               usable_tables);
    break;
  }
  case Item_func::ISNULL_FUNC:
  {
    Item *tmp=new Item_null;
    if (!tmp || add_eq_dep(te, fdeps, *and_level, cond_func, args[0], args[1],
                           usable_tables))
      return TRUE;
    break;
  }
  case Item_func::MULT_EQUAL_FUNC:
  {
    Item_equal *item_equal= (Item_equal *) cond;
    Item *const_item= item_equal->get_const();
    Item_equal_iterator it(*item_equal);
    Item_field *item;
    if (const_item)
    {
      /*
        For each field field1 from item_equal consider the equality 
        field1=const_item as a condition allowing an index access of the table
        with field1 by the keys value of field1.
      */   
      while ((item= it++))
      {
        if (add_eq_dep(te, fdeps, *and_level, cond_func, item, const_item, 
                       usable_tables))
          return TRUE;
      }
    }
    else 
    {
      /*
        Consider all pairs of different fields included into item_equal.
        For each of them (field1, field1) consider the equality 
        field1=field2 as a condition allowing an index access of the table
        with field1 by the keys value of field2.
      */   
      Item_equal_iterator fi(*item_equal);
      while ((item= fi++))
      {
        Field *field= item->field;
        Item_field *item2;
        while ((item2= it++))
        {
          if (!field->eq(item2->field))
          {
            if (add_eq_dep(te, fdeps, *and_level, cond_func, item, item2, 
                           usable_tables))
              return TRUE;
          }
        }
        it.rewind();
      }
    }
    break;
  }
  default:
    break;
  }
  return FALSE;
}


/*
  Perform an OR operation on two (adjacent) Equality_module arrays.

  SYNOPSIS
     merge_func_deps()
       start        Start of left OR-part
       new_fields   Start of right OR-part
       end          End of right OR-part
       and_level    AND-level.

  DESCRIPTION
  This function is invoked for two adjacent arrays of Equality_module elements:

                      $LEFT_PART             $RIGHT_PART
             +-----------------------+-----------------------+
            start                new_fields                 end
         
  The goal is to produce an array which would correspnd to the combined 
  
    $LEFT_PART OR $RIGHT_PART
  
  condition. This is achieved as follows: First, we apply distrubutive law:
  
    (fdep_A_1 AND fdep_A_2 AND ...)  OR  (fdep_B_1 AND fdep_B_2 AND ...) =

     = AND_ij (fdep_A_[i] OR fdep_B_[j])
  
  Then we walk over the obtained "fdep_A_[i] OR fdep_B_[j]" pairs, and 
   - Discard those that that have left and right part referring to different
     columns. We can't infer anything useful from "col1=expr1 OR col2=expr2".
   - When left and right parts refer to the same column,  we check if they are 
     essentially the same. 
     = If they are the same, we keep one copy 
       "t.col=expr OR t.col=expr"  -> "t.col=expr 
     = if they are different , then we discard both
      "t.col=expr1 OR t.col=expr2" -> (nothing useful)

  (no per-table or for-index FUNC_DEPS exist yet at this phase).

  See also merge_key_fields().

  RETURN 
    End of the result array
*/

static 
Equality_module *merge_func_deps(Equality_module *start, Equality_module *new_fields, 
                              Equality_module *end, uint and_level)
{
  if (start == new_fields)
    return start;				// Impossible or
  if (new_fields == end)
    return start;				// No new fields, skip all

  Equality_module *first_free=new_fields;

  for (; new_fields != end ; new_fields++)
  {
    for (Equality_module *old=start ; old != first_free ; old++)
    {
      /* 
         TODO: does it make sense to attempt to merging multiple-equalities? 
         A: YES.  
           (a=b=c) OR (a=b=d)  produce "a=b". 
         QQ: 
           What to use for merging? Trivial N*M algorithm or pre-sort and then
           merge ordered sequences?
      */
      if (old->field == new_fields->field)
      {
	if (!new_fields->expression->const_item())
	{
	  /*
	    If the value matches, we can use the key reference.
	    If not, we keep it until we have examined all new values
	  */
	  if (old->expression->eq(new_fields->expression, old->field->field->binary()))
	  {
	    old->level= and_level;
	  }
	}
	else if (old->expression->eq_by_collation(new_fields->expression, 
                                           old->field->field->binary(),
                                           old->field->field->charset()))
	{
	  old->level= and_level;
	}
	else
	{
          /* The expressions are different. */
	  if (old == --first_free)		// If last item
	    break;
	  *old= *first_free;			// Remove old value
	  old--;				// Retry this value
	}
      }
    }
  }

  /* 
    Ok, the results are within the [start, first_free) range, and the useful
    elements have level==and_level. Now, lets remove all unusable elements:
  */
  for (Equality_module *old=start ; old != first_free ;)
  {
    if (old->level != and_level)
    {						// Not used in all levels
      if (old == --first_free)
	break;
      *old= *first_free;			// Remove old value
      continue;
    }
    old++;
  }
  return first_free;
}


/*
  Add an Equality_module element for a given predicate, if applicable 

  DESCRIPTION 
    This function is modeled after add_key_field().
*/

static 
bool add_eq_dep(Table_elimination *te, Equality_module **eq_dep,
                uint and_level, Item_func *cond, 
                Item *left, Item *right, table_map usable_tables)
{
  if ((left->used_tables() & usable_tables) &&
      !(right->used_tables() & RAND_TABLE_BIT) &&
      left->real_item()->type() == Item::FIELD_ITEM)
  {
    Field *field= ((Item_field*)left->real_item())->field;
    if (field->result_type() == STRING_RESULT)
    {
      if (right->result_type() != STRING_RESULT)
      {
        if (field->cmp_type() != right->result_type())
          return FALSE;
      }
      else
      {
        /*
          We can't use indexes if the effective collation
          of the operation differ from the field collation.
        */
        if (field->cmp_type() == STRING_RESULT &&
            ((Field_str*)field)->charset() != cond->compare_collation())
          return FALSE;
      }
    }

    /* Store possible eq field */
    (*eq_dep)->type=  Module_dep::MODULE_EXPRESSION; //psergey-todo;
    if (!((*eq_dep)->field= get_field_value(te, field)))
      return TRUE;
    (*eq_dep)->expression= right;
    (*eq_dep)->level= and_level;
    (*eq_dep)++;
  }
  return FALSE;
}


/*
  Get a Table_value object for the given table, creating it if necessary.
*/

static Table_value *get_table_value(Table_elimination *te, TABLE *table)
{
  Table_value *tbl_dep;
  if (!(tbl_dep= new Table_value(table)))
    return NULL;

  Key_module **key_list= &(tbl_dep->keys);
  /* Add dependencies for unique keys */
  for (uint i=0; i < table->s->keys; i++)
  {
    KEY *key= table->key_info + i; 
    if ((key->flags & (HA_NOSAME | HA_END_SPACE_KEY)) == HA_NOSAME)
    {
      Key_module *key_dep= new Key_module(tbl_dep, i, key->key_parts);
      *key_list= key_dep;
      key_list= &(key_dep->next_table_key);
    }
  }
  return te->table_deps[table->tablenr]= tbl_dep;
}


/* 
  Get a Field_value object for the given field, creating it if necessary
*/

static Field_value *get_field_value(Table_elimination *te, Field *field)
{
  TABLE *table= field->table;
  Table_value *tbl_dep;

  /* First, get the table*/
  if (!(tbl_dep= te->table_deps[table->tablenr]))
  {
    if (!(tbl_dep= get_table_value(te, table)))
      return NULL;
  }
 
  /* Try finding the field in field list */
  Field_value **pfield= &(tbl_dep->fields);
  while (*pfield && (*pfield)->field->field_index < field->field_index)
  {
    pfield= &((*pfield)->next_table_field);
  }
  if (*pfield && (*pfield)->field->field_index == field->field_index)
    return *pfield;
  
  /* Create the field and insert it in the list */
  Field_value *new_field= new Field_value(tbl_dep, field);
  new_field->next_table_field= *pfield;
  *pfield= new_field;

  return new_field;
}


/*
  Create an Outer_join_module object for the given outer join

  DESCRIPTION
    Outer_join_module objects for children (or further descendants) are always
    created before the parents.
*/

static 
Outer_join_module *get_outer_join_dep(Table_elimination *te, 
                                      TABLE_LIST *outer_join, 
                                      table_map deps_map)
{
  Outer_join_module *oj_dep;
  oj_dep= new Outer_join_module(outer_join, my_count_bits(deps_map));
  te->n_outer_joins++;
  
  /* 
    Collect a bitmap fo tables that we depend on, and also set parent pointer
    for descendant outer join elements.
  */
  Table_map_iterator it(deps_map);
  int idx;
  while ((idx= it.next_bit()) != Table_map_iterator::BITMAP_END)
  {
    Table_value *table_dep;
    if (!(table_dep= te->table_deps[idx]))
    {
      /*
        We get here only when ON expression had no references to inner tables
        and Table_map objects weren't created for them. This is a rare/
        unimportant case so it's ok to do not too efficient searches.
      */
      TABLE *table= NULL;
      for (TABLE_LIST *tlist= te->join->select_lex->leaf_tables; tlist;
           tlist=tlist->next_leaf)
      {
        if (tlist->table->tablenr == (uint)idx)
        {
          table=tlist->table;
          break;
        }
      }
      DBUG_ASSERT(table);
      if (!(table_dep= get_table_value(te, table)))
        return NULL;
    }
    
    /* 
      Walk from the table up to its embedding outer joins. The goal is to
      find the least embedded outer join nest and set its parent pointer to
      point to the newly created Outer_join_module.
      to set the pointer of its near 
    */
    if (!table_dep->outer_join_dep)
      table_dep->outer_join_dep= oj_dep;
    else
    {
      Outer_join_module *oj= table_dep->outer_join_dep;
      while (oj->parent)
        oj= oj->parent;
      if (oj != oj_dep)
        oj->parent=oj_dep;
    }
  }
  return oj_dep;
}


/*
  Build functional dependency graph for elements of given join list

  SYNOPSIS
    collect_funcdeps_for_join_list()
      te                       Table elimination context.
      join_list                Join list to work on 
      build_eq_deps            TRUE <=> build Equality_module elements for all
                               members of the join list, even if they cannot 
                               be individually eliminated
      tables_used_elsewhere    Bitmap of tables that are referred to from
                               somewhere outside of this join list (e.g.
                               select list, HAVING, ON expressions of parent
                               joins, etc).
      eliminable_tables  INOUT Tables that can potentially be eliminated
                               (needed so we know for which tables to build 
                               dependencies for)
      eq_dep             INOUT End of array of equality dependencies.

  DESCRIPTION
    .
*/

static bool
collect_funcdeps_for_join_list(Table_elimination *te,
                               List<TABLE_LIST> *join_list,
                               bool build_eq_deps,
                               table_map tables_used_elsewhere,
                               table_map *eliminable_tables,
                               Equality_module **eq_dep)
{
  TABLE_LIST *tbl;
  List_iterator<TABLE_LIST> it(*join_list);
  table_map tables_used_on_left= 0;

  while ((tbl= it++))
  {
    if (tbl->on_expr)
    {
      table_map outside_used_tables= tables_used_elsewhere | 
                                     tables_used_on_left;
      bool eliminable;
      table_map cur_map;
      if (tbl->nested_join)
      {
        /* This is  "... LEFT JOIN (join_nest) ON cond" */
        cur_map= tbl->nested_join->used_tables; 
        eliminable= !(cur_map & outside_used_tables);
        if (eliminable)
          *eliminable_tables |= cur_map;
        if (collect_funcdeps_for_join_list(te, &tbl->nested_join->join_list,
                                           eliminable || build_eq_deps,
                                           outside_used_tables,
                                           eliminable_tables,
                                           eq_dep))
          return TRUE;
      }
      else
      {
        /* This is  "... LEFT JOIN tbl ON cond" */
        cur_map= tbl->table->map;
        eliminable= !(tbl->table->map & outside_used_tables);
        *eliminable_tables |= cur_map;
      }

      if (eliminable || build_eq_deps)
      {
        // build comp_cond from ON expression
        uint and_level=0;
        build_eq_deps_for_cond(te, eq_dep, &and_level, tbl->on_expr, 
                                *eliminable_tables);
      }

      if (eliminable && !get_outer_join_dep(te, tbl, cur_map))
        return TRUE;

      tables_used_on_left |= tbl->on_expr->used_tables();
    }
  }
  return FALSE;
}


/*
  This is used to analyze expressions in "tbl.col=expr" dependencies so
  that we can figure out which fields the expression depends on.
*/

class Field_dependency_setter : public Field_enumerator
{
public:
  Field_dependency_setter(Table_elimination *te_arg): te(te_arg)
  {}
  
  void see_field(Field *field)
  {
    Table_value *tbl_dep;
    if ((tbl_dep= te->table_deps[field->table->tablenr]))
    {
      for (Field_value *field_dep= tbl_dep->fields; field_dep; 
           field_dep= field_dep->next_table_field)
      {
        if (field->field_index == field_dep->field->field_index)
        {
          uint offs= field_dep->bitmap_offset + expr_offset;
          if (!bitmap_is_set(&te->expr_deps, offs))
            te->equality_deps[expr_offset].unknown_args++;
          bitmap_set_bit(&te->expr_deps, offs);
          return;
        }
      }
      /* 
        We got here if didn't find this field. It's not a part of 
        a unique key, and/or there is no field=expr element for it.
        Bump the dependency anyway, this will signal that this dependency
        cannot be satisfied.
      */
      te->equality_deps[expr_offset].unknown_args++;
    }
  }

  Table_elimination *te;
  /* Offset of the expression we're processing in the dependency bitmap */
  uint expr_offset; 
};


/*
  Setup equality dependencies
  
  SYNOPSIS
    setup_equality_deps()
      te                    Table elimination context
      bound_deps_list  OUT  Start of linked list of elements that were found to
                            be bound (caller will use this to see if that
                            allows to declare further elements bound)
*/

static 
bool setup_equality_deps(Table_elimination *te, Module_dep **bound_deps_list)
{
  DBUG_ENTER("setup_equality_deps");
  
  /*
    Count Field_value objects and assign each of them a unique bitmap_offset.
  */
  uint offset= 0;
  for (Table_value **tbl_dep=te->table_deps; 
       tbl_dep < te->table_deps + MAX_TABLES;
       tbl_dep++)
  {
    if (*tbl_dep)
    {
      for (Field_value *field_dep= (*tbl_dep)->fields;
           field_dep;
           field_dep= field_dep->next_table_field)
      {
        field_dep->bitmap_offset= offset;
        offset += te->n_equality_deps;
      }
    }
  }
 
  void *buf;
  if (!(buf= current_thd->alloc(bitmap_buffer_size(offset))) ||
      bitmap_init(&te->expr_deps, (my_bitmap_map*)buf, offset, FALSE))
  {
    DBUG_RETURN(TRUE);
  }
  bitmap_clear_all(&te->expr_deps);

  /* 
    Analyze all "field=expr" dependencies, and have te->expr_deps encode
    dependencies of expressions from fields.

    Also collect a linked list of equalities that are bound.
  */
  Module_dep *bound_dep= NULL;
  Field_dependency_setter deps_setter(te);
  for (Equality_module *eq_dep= te->equality_deps; 
       eq_dep < te->equality_deps + te->n_equality_deps;
       eq_dep++)
  {
    deps_setter.expr_offset= eq_dep - te->equality_deps;
    eq_dep->unknown_args= 0;
    eq_dep->expression->walk(&Item::check_column_usage_processor, FALSE, 
                      (uchar*)&deps_setter);
    if (!eq_dep->unknown_args)
    {
      eq_dep->next= bound_dep;
      bound_dep= eq_dep;
    }
  }
  *bound_deps_list= bound_dep;

  DBUG_EXECUTE("test", dbug_print_deps(te); );
  DBUG_RETURN(FALSE);
}


/*
  Perform table elimination

  SYNOPSIS
    eliminate_tables()
      join                   Join to work on
      const_tbl_count INOUT  Number of constant tables (this includes
                             eliminated tables)
      const_tables    INOUT  Bitmap of constant tables

  DESCRIPTION
    This function is the entry point for table elimination. 
    The idea behind table elimination is that if we have an outer join:
   
      SELECT * FROM t1 LEFT JOIN 
        (t2 JOIN t3) ON t3.primary_key=t1.col AND 
                        t4.primary_key=t2.col
    such that

    1. columns of the inner tables are not used anywhere ouside the outer
       join (not in WHERE, not in GROUP/ORDER BY clause, not in select list 
       etc etc), and
    2. inner side of the outer join is guaranteed to produce at most one
       record combination for each record combination of outer tables.
    
    then the inner side of the outer join can be removed from the query.
    This is because it will always produce one matching record (either a
    real match or a NULL-complemented record combination), and since there
    are no references to columns of the inner tables anywhere, it doesn't
    matter which record combination it was.

    This function primary handles checking #1. It collects a bitmap of
    tables that are not used in select list/GROUP BY/ORDER BY/HAVING/etc and
    thus can possibly be eliminated.
  
  SIDE EFFECTS
    See the OVERVIEW section at the top of this file.

*/

void eliminate_tables(JOIN *join)
{
  THD* thd= join->thd;
  Item *item;
  table_map used_tables;
  DBUG_ENTER("eliminate_tables");
  
  DBUG_ASSERT(join->eliminated_tables == 0);

  /* If there are no outer joins, we have nothing to eliminate: */
  if (!join->outer_join)
    DBUG_VOID_RETURN;

  /* Find the tables that are referred to from WHERE/HAVING */
  used_tables= (join->conds?  join->conds->used_tables() : 0) | 
               (join->having? join->having->used_tables() : 0);
  
  /* Add tables referred to from the select list */
  List_iterator<Item> it(join->fields_list);
  while ((item= it++))
    used_tables |= item->used_tables();
 
  /* Add tables referred to from ORDER BY and GROUP BY lists */
  ORDER *all_lists[]= { join->order, join->group_list};
  for (int i=0; i < 2; i++)
  {
    for (ORDER *cur_list= all_lists[i]; cur_list; cur_list= cur_list->next)
      used_tables |= (*(cur_list->item))->used_tables();
  }
  
  if (join->select_lex == &thd->lex->select_lex)
  {
    /* Multi-table UPDATE and DELETE: don't eliminate the tables we modify: */
    used_tables |= thd->table_map_for_update;

    /* Multi-table UPDATE: don't eliminate tables referred from SET statement */
    if (thd->lex->sql_command == SQLCOM_UPDATE_MULTI)
    {
      List_iterator<Item> it2(thd->lex->value_list);
      while ((item= it2++))
        used_tables |= item->used_tables();
    }
  }
  
  table_map all_tables= join->all_tables_map();
  if (all_tables & ~used_tables)
  {
    /* There are some tables that we probably could eliminate. Try it. */
    Table_elimination te(join);
    uint m= max(thd->lex->current_select->max_equal_elems,1);
    uint max_elems= ((thd->lex->current_select->cond_count+1)*2 +
                      thd->lex->current_select->between_count)*m + 1 + 10; 
    if (!(te.equality_deps= new Equality_module[max_elems]))
      DBUG_VOID_RETURN;
    Equality_module *eq_deps_end= te.equality_deps;
    table_map eliminable_tables= 0;
    if (collect_funcdeps_for_join_list(&te, join->join_list,
                                       FALSE,
                                       used_tables,
                                       &eliminable_tables,
                                       &eq_deps_end))
      DBUG_VOID_RETURN;
    te.n_equality_deps= eq_deps_end - te.equality_deps;
    
    Module_dep *bound_modules;
    //Value_dep  *bound_values;
    if (setup_equality_deps(&te, &bound_modules))
      DBUG_VOID_RETURN;
    
    run_elimination_wave(&te, bound_modules);
  }
  DBUG_VOID_RETURN;
}


static 
void signal_from_field_to_exprs(Table_elimination* te, Field_value *field_dep,
                                Module_dep **bound_modules)
{
  for (uint i=0; i < te->n_equality_deps; i++)
  {
    if (bitmap_is_set(&te->expr_deps, field_dep->bitmap_offset + i) &&
        te->equality_deps[i].unknown_args &&
        !--te->equality_deps[i].unknown_args)
    {
      /* Mark as bound and add to the list */
      Equality_module* eq_dep= &te->equality_deps[i];
      eq_dep->next= *bound_modules;
      *bound_modules= eq_dep;
    }
  }
}


static 
void run_elimination_wave(Table_elimination *te, Module_dep *bound_modules)
{
  Value_dep *bound_values= NULL;
  /* 
    Run the wave.
    All Func_dep-derived objects are divided into three classes:
    - Those that have bound=FALSE
    - Those that have bound=TRUE 
    - Those that have bound=TRUE and are in the list..

  */
  while (bound_modules)
  {
    for (;bound_modules; bound_modules= bound_modules->next)
    {
      switch (bound_modules->type)
      {
        case Module_dep::MODULE_EXPRESSION:
        {
          /*  It's a field=expr and we got to know the expr, so we know the field */
          Equality_module *eq_dep= (Equality_module*)bound_modules;
          if (!eq_dep->field->bound)
          {
            /* Mark as bound and add to the list */
            eq_dep->field->bound= TRUE;
            eq_dep->field->next= bound_values;
            bound_values= eq_dep->field;
          }
          break;
        }
        case Module_dep::MODULE_UNIQUE_KEY:
        {
          /* Unique key is known means the table is known */
          Table_value *table_dep=((Key_module*)bound_modules)->table;
          if (!table_dep->bound)
          {
            /* Mark as bound and add to the list */
            table_dep->bound= TRUE;
            table_dep->next= bound_values;
            bound_values= table_dep;
          }
          break;
        }
        case Module_dep::MODULE_OUTER_JOIN:
        {
          Outer_join_module *outer_join_dep= (Outer_join_module*)bound_modules;
          mark_as_eliminated(te->join, outer_join_dep->table_list);
          if (!--te->n_outer_joins)
          {
            DBUG_PRINT("info", ("Table elimination eliminated everything" 
                                " it theoretically could"));
            return;
          }
          break;
        }
        case Module_dep::MODULE_MULTI_EQUALITY:
        default:
          DBUG_ASSERT(0);
      }
    }

    for (;bound_values; bound_values=bound_values->next)
    {
      switch (bound_values->type)
      {
        case Value_dep::VALUE_FIELD:
        {
          /*
            Field became known. Check out
            - unique keys we belong to
            - expressions that depend on us.
          */
          Field_value *field_dep= (Field_value*)bound_values;
          for (Key_module *key_dep= field_dep->table->keys; key_dep;
               key_dep= key_dep->next_table_key)
          {
            DBUG_PRINT("info", ("key %s.%s is now bound",
                                key_dep->table->table->alias, 
                                key_dep->table->table->key_info[key_dep->keyno].name));
            if (field_dep->field->part_of_key.is_set(key_dep->keyno) && 
                key_dep->unknown_args && !--key_dep->unknown_args)
            {
              /* Mark as bound and add to the list */
              key_dep->next= bound_modules;
              bound_modules= key_dep;
            }
          }
          signal_from_field_to_exprs(te, field_dep, &bound_modules);
          break;
        }
        case Value_dep::VALUE_TABLE:
        {
          Table_value *table_dep=(Table_value*)bound_values; 
          DBUG_PRINT("info", ("table %s is now bound",
                              table_dep->table->alias));
          /*
            Table is known means
            - all its fields are known
            - one more element in outer join nest is known
          */
          for (Field_value *field_dep= table_dep->fields; field_dep; 
               field_dep= field_dep->next_table_field)
          {
            if (!field_dep->bound)
            {
              /* Mark as bound and add to the list */
              field_dep->bound= TRUE;
              signal_from_field_to_exprs(te, field_dep, &bound_modules);
            }
          }
          for (Outer_join_module *outer_join_dep= table_dep->outer_join_dep;
               outer_join_dep; outer_join_dep= outer_join_dep->parent)
          {
            if (outer_join_dep->unknown_args && 
                !--outer_join_dep->unknown_args)
            {
              /* Mark as bound and add to the list */
              outer_join_dep->next= bound_modules;
              bound_modules= outer_join_dep;
            }
          }
          break;
        }
        default: 
          DBUG_ASSERT(0);
      }
    }
  }
}


/* 
  Mark one table or the whole join nest as eliminated.
*/
static void mark_as_eliminated(JOIN *join, TABLE_LIST *tbl)
{
  TABLE *table;
  /*
    NOTE: there are TABLE_LIST object that have
    tbl->table!= NULL && tbl->nested_join!=NULL and 
    tbl->table == tbl->nested_join->join_list->element(..)->table
  */
  if (tbl->nested_join)
  {
    TABLE_LIST *child;
    List_iterator<TABLE_LIST> it(tbl->nested_join->join_list);
    while ((child= it++))
      mark_as_eliminated(join, child);
  }
  else if ((table= tbl->table))
  {
    JOIN_TAB *tab= tbl->table->reginfo.join_tab;
    if (!(join->const_table_map & tab->table->map))
    {
      DBUG_PRINT("info", ("Eliminated table %s", table->alias));
      tab->type= JT_CONST;
      join->eliminated_tables |= table->map;
      join->const_table_map|= table->map;
      set_position(join, join->const_tables++, tab, (KEYUSE*)0);
    }
  }

  if (tbl->on_expr)
    tbl->on_expr->walk(&Item::mark_as_eliminated_processor, FALSE, NULL);
}


#ifndef DBUG_OFF
static 
void dbug_print_deps(Table_elimination *te)
{
  DBUG_ENTER("dbug_print_deps");
  DBUG_LOCK_FILE;
  
  fprintf(DBUG_FILE,"deps {\n");
  
  /* Start with printing equalities */
  for (Equality_module *eq_dep= te->equality_deps; 
       eq_dep != te->equality_deps + te->n_equality_deps; eq_dep++)
  {
    char buf[128];
    String str(buf, sizeof(buf), &my_charset_bin);
    str.length(0);
    eq_dep->expression->print(&str, QT_ORDINARY);
    fprintf(DBUG_FILE, "  equality%d: %s -> %s.%s\n", 
            eq_dep - te->equality_deps,
            str.c_ptr(),
            eq_dep->field->table->table->alias,
            eq_dep->field->field->field_name);
  }
  fprintf(DBUG_FILE,"\n");

  /* Then tables and their fields */
  for (uint i=0; i < MAX_TABLES; i++)
  {
    Table_value *table_dep;
    if ((table_dep= te->table_deps[i]))
    {
      /* Print table */
      fprintf(DBUG_FILE, "  table %s\n", table_dep->table->alias);
      /* Print fields */
      for (Field_value *field_dep= table_dep->fields; field_dep; 
           field_dep= field_dep->next_table_field)
      {
        fprintf(DBUG_FILE, "    field %s.%s ->", table_dep->table->alias,
                field_dep->field->field_name);
        uint ofs= field_dep->bitmap_offset;
        for (uint bit= ofs; bit < ofs + te->n_equality_deps; bit++)
        {
          if (bitmap_is_set(&te->expr_deps, bit))
            fprintf(DBUG_FILE, " equality%d ", bit - ofs);
        }
        fprintf(DBUG_FILE, "\n");
      }
    }
  }
  fprintf(DBUG_FILE,"\n}\n");
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}

#endif 
/**
  @} (end of group Table_Elimination)
*/

