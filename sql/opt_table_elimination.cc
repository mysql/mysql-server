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

  This file contains table elimination module. The idea behind table
  elimination is as follows: suppose we have a left join
 
    SELECT * FROM t1 LEFT JOIN 
      (t2 JOIN t3) ON t3.primary_key=t1.col AND 
                      t4.primary_key=t2.col
  such that
  * columns of the inner tables are not used anywhere ouside the outer join
    (not in WHERE, not in GROUP/ORDER BY clause, not in select list etc etc),
  * inner side of the outer join is guaranteed to produce at most one matching
    record combination for each record combination of outer tables.
  
  then the inner side of the outer join can be removed from the query, as it 
  will always produce only one record combination (either real or 
  null-complemented one) and we don't care about what that record combination 
  is.

  MODULE INTERFACE

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

  TABLE ELIMINATION ALGORITHM

  As said above, we can remove inner side of an outer join if it is 

    1. not referred to from any other parts of the query
    2. always produces one matching record combination.

  We check #1 by doing a recursive descent down the join->join_list while 
  maintaining a union of used_tables() attribute of all expressions we've seen
  "elsewhere". When we encounter an outer join, we check if the bitmap of 
  tables on its inner side has an intersection with tables that are used 
  elsewhere. No intersection means that inner side of the outer join could 
  potentially be eliminated.

  In order to check #2, one needs to prove that inner side of an outer join 
  is functionally dependent on the outside. We prove dependency by proving
  functional dependency of intermediate objects:

  - Inner side of outer join is functionally dependent when each of its tables
    are functionally dependent. (We assume a table is functionally dependent 
    when its dependencies allow to uniquely identify one table record, or no
    records).

  - Table is functionally dependent when it has got a unique key whose columns
    are functionally dependent.

  - A column is functionally dependent when we could locate an AND-part of a
    certain ON clause in form 
      
      tblX.columnY= expr 
    
    where expr is functionally-depdendent.

  Apparently the above rules can be applied recursively. Also, certain entities
  depend on multiple other entities. We model this by a bipartite graph which
  has two kinds of nodes:

  Value nodes:
   - Table column values (each is a value of tblX.columnY)
   - Table nodes (each node represents a table inside an eliminable join nest).
  each value is either bound (i.e. functionally dependent) or not.

  Module nodes:
   - Nodes representing tblX.colY=expr equalities. Equality node has 
      = incoming edges from columns used in expr 
      = outgoing edge to tblX.colY column.
   - Nodes representing unique keys. Unique key has
      = incoming edges from key component value nodes
      = outgoing edge to key's table node
   - Inner side of outer join node. Outer join node has
      = incoming edges from table value nodes
      = No outgoing edges. Once we reach it, we know we can eliminate the 
        outer join.
  A module may depend on multiple values, and hence its primary attribute is
  the number of its depedencies that are not bound. 

  The algorithm starts with equality nodes that don't have any incoming edges
  (their expressions are either constant or depend only on tables that are
  outside of any outer joins) and proceeds to traverse dependency->dependant
  edges until we've other traversed everything (TODO rephrase elaborate), or
  we've reached the point where all outer join modules have zero unsatisfied
  dependencies.
*/

class Value_dep;
  class Field_value;
  class Table_value;
 

class Module_dep;
  class Equality_module;
  class Outer_join_module;
  class Key_module;

class Table_elimination;


/*
  A value, something that can be bound or not bound. Also, values can be linked
  in a list.
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


/*
  A table field value. There is exactly only one such object for any tblX.fieldY
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
  A table value. There is one Table_value object for every table that can
  potentially be eliminated.
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
  A 'module'. Module has dependencies
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
  Outer_join_module(//TABLE_LIST *table_list_arg, 
  uint n_children)  
  //  table_list(table_list_arg)
  {
    type= Module_dep::MODULE_OUTER_JOIN;
    unknown_args= n_children;
  }
  /* 
    Outer join we're representing. This can be a join nest or one table that
    is outer join'ed.
  */
//  TABLE_LIST *table_list;
};


/*
  Table elimination context
*/
class Table_elimination
{
public:
  Table_elimination(JOIN *join_arg) : join(join_arg)
  {
    bzero(table_deps, sizeof(table_deps));
  }

  JOIN *join;
  /* Array of equality dependencies */
  Equality_module *equality_deps;
  uint n_equality_deps; /* Number of elements in the array */

  /* tablenr -> Table_value* mapping. */
  Table_value *table_deps[MAX_KEY];

  /* Bitmap of how expressions depend on bits */
  MY_BITMAP expr_deps;
};

void eliminate_tables(JOIN *join);

static bool
eliminate_tables_for_list(Table_elimination *te, 
                          List<TABLE_LIST> *join_list,
                          table_map tables_in_list,
                          Item *on_expr,
                          table_map tables_used_elsewhere);
static bool 
check_func_dependency(Table_elimination *te, table_map tables, Item* cond);

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
static Outer_join_module *get_outer_join_dep(Table_elimination *te, 
                                             //TABLE_LIST *outer_join, 
                                             table_map deps_map);
static 
bool run_elimination_wave(Table_elimination *te, Module_dep *bound_modules);
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
                            Equality_module represent "tbl.col=expr" and we'll
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
    elements have level==and_level. Now, remove all unusable elements:
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
  Add an Equality_module element for left=right condition

  SYNOPSIS
    add_eq_dep()
      te                Table elimination context
      eq_mod     INOUT  Store created Equality_module here and increment ptr if
                        you do so
      and_level         AND-level ()
      cond              Condition we've inferred the left=right equality from.
      left              Left expression
      right             Right expression
      usable_tables     Create Equality_module only if Left_expression's table 
                        belongs to this set.

  DESCRIPTION 
    Check if the passed equality means that 'left' expr is functionally dependent on
    the 'right', and if yes, create an Equality_module object for it.

  RETURN
    FALSE OK
    TRUE  Out of memory
*/

static 
bool add_eq_dep(Table_elimination *te, Equality_module **eq_mod,
                uint and_level, Item_func *cond, Item *left, Item *right,
                table_map usable_tables)
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
          We can't assume there's a functional dependency if the effective 
          collation of the operation differ from the field collation.
        */
        if (field->cmp_type() == STRING_RESULT &&
            ((Field_str*)field)->charset() != cond->compare_collation())
          return FALSE;
      }
    }

    (*eq_mod)->type=  Module_dep::MODULE_EXPRESSION; //psergey-todo;
    if (!((*eq_mod)->field= get_field_value(te, field)))
      return TRUE;
    (*eq_mod)->expression= right;
    (*eq_mod)->level= and_level;
    (*eq_mod)++;
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
                                     // TABLE_LIST *outer_join, 
                                      table_map deps_map)
{
  Outer_join_module *oj_dep;
  if (!(oj_dep= new Outer_join_module(/*outer_join, */my_count_bits(deps_map))))
    return NULL;
  
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
    table_dep->outer_join_dep= oj_dep;
  }
  return oj_dep;
}


/*
  This is used to analyze expressions in "tbl.col=expr" dependencies so
  that we can figure out which fields the expression depends on.
*/

class Field_dependency_recorder : public Field_enumerator
{
public:
  Field_dependency_recorder(Table_elimination *te_arg): te(te_arg)
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
  DESCRIPTION
  RETURN
    
*/

static 
bool setup_equality_deps(Table_elimination *te, Module_dep **bound_deps_list)
{
  DBUG_ENTER("setup_equality_deps");
  
  if (!te->n_equality_deps)
    DBUG_RETURN(TRUE);
  /*
    Count Field_value objects and assign each of them a unique bitmap_offset.
  */
  uint offset= 0;
  for (Table_value **tbl_dep=te->table_deps; 
       tbl_dep < te->table_deps + MAX_TABLES;
       tbl_dep++) // psergey-todo: TODO change to Table_map_iterator
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
  Field_dependency_recorder deps_recorder(te);
  for (Equality_module *eq_dep= te->equality_deps; 
       eq_dep < te->equality_deps + te->n_equality_deps;
       eq_dep++)
  {
    deps_recorder.expr_offset= eq_dep - te->equality_deps;
    eq_dep->unknown_args= 0;
    eq_dep->expression->walk(&Item::check_column_usage_processor, FALSE, 
                             (uchar*)&deps_recorder);
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

  DESCRIPTION
    This is the entry point for table elimination. Grep for MODULE INTERFACE
    section in this file for calling convention.

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
    //psergey-todo: move allocs to somewhere else.
    Table_elimination te(join);
    uint m= max(thd->lex->current_select->max_equal_elems,1);
    uint max_elems= ((thd->lex->current_select->cond_count+1)*2 +
                      thd->lex->current_select->between_count)*m + 1 + 10; 
    if (!(te.equality_deps= new Equality_module[max_elems]))
      DBUG_VOID_RETURN;

    eliminate_tables_for_list(&te, join->join_list, all_tables, NULL,
                              used_tables);
  }
  DBUG_VOID_RETURN;
}


/*
  Perform table elimination in a given join list

  SYNOPSIS
    eliminate_tables_for_list()
      te                      Table elimination context
      join_list               Join list to work on
      list_tables             Bitmap of tables embedded in the join_list.
      on_expr                 ON expression, if the join list is the inner side
                              of an outer join.
                              NULL means it's not an outer join but rather a
                              top-level join list.
      tables_used_elsewhere   Bitmap of tables that are referred to from
                              somewhere outside of the join list (e.g.
                              select list, HAVING, other ON expressions, etc).

  DESCRIPTION
    Perform table elimination in a given join list.
    
  RETURN
    TRUE  The entire join list eliminated
    FALSE Join list wasn't eliminated (but some of its possibly were)
*/

static bool
eliminate_tables_for_list(Table_elimination *te, List<TABLE_LIST> *join_list,
                          table_map list_tables, Item *on_expr,
                          table_map tables_used_elsewhere)
{
  TABLE_LIST *tbl;
  List_iterator<TABLE_LIST> it(*join_list);
  table_map tables_used_on_left= 0;
  bool all_eliminated= TRUE;

  while ((tbl= it++))
  {
    if (tbl->on_expr)
    {
      table_map outside_used_tables= tables_used_elsewhere | 
                                     tables_used_on_left;
      if (tbl->nested_join)
      {
        /* This is  "... LEFT JOIN (join_nest) ON cond" */
        if (eliminate_tables_for_list(te,
                                      &tbl->nested_join->join_list, 
                                      tbl->nested_join->used_tables, 
                                      tbl->on_expr,
                                      outside_used_tables))
        {
          mark_as_eliminated(te->join, tbl);
        }
        else
          all_eliminated= FALSE;
      }
      else
      {
        /* This is  "... LEFT JOIN tbl ON cond" */
        if (!(tbl->table->map & outside_used_tables) &&
            check_func_dependency(te, tbl->table->map, tbl->on_expr))
        {
          mark_as_eliminated(te->join, tbl);
        }
        else
          all_eliminated= FALSE;
      }
      tables_used_on_left |= tbl->on_expr->used_tables();
    }
    else
    {
      DBUG_ASSERT(!tbl->nested_join);
    }
  }

  /* Try eliminating the nest we're called for */
  if (all_eliminated && on_expr && !(list_tables & tables_used_elsewhere))
  {
    return check_func_dependency(te, list_tables & ~te->join->eliminated_tables,
                                 on_expr);
  }
  return FALSE; /* not eliminated */
}


/*
  Check if condition makes the a set of tables functionally-dependent

  SYNOPSIS
    check_func_dependency()
      te       Table elimination context
      tables   Set of tables we want to be functionally dependent 
      cond     Condition to use

  DESCRIPTION
    Check if condition allows to conclude that the table set is functionally
    dependent on everything else.

  RETURN 
    TRUE  - Yes, functionally dependent
    FALSE - No, or error
*/

static
bool check_func_dependency(Table_elimination *te, table_map tables, Item* cond)
{
  uint and_level=0;
  Equality_module* eq_dep= te->equality_deps;
  if (build_eq_deps_for_cond(te, &eq_dep, &and_level, cond, tables))
    return TRUE;
  te->n_equality_deps= eq_dep - te->equality_deps;
  Module_dep *bound_modules;
  if (!get_outer_join_dep(te, tables) &&
      !setup_equality_deps(te, &bound_modules) &&
      run_elimination_wave(te, bound_modules))
  {
    return TRUE; /* eliminated */
  }
  return FALSE;
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


/* 
  Run the wave.
  All Func_dep-derived objects are divided into three classes:
  - Those that have bound=FALSE
  - Those that have bound=TRUE 
  - Those that have bound=TRUE and are in the list..
*/

static 
bool run_elimination_wave(Table_elimination *te, Module_dep *bound_modules)
{
  Value_dep *bound_values= NULL;
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
          DBUG_PRINT("info", ("Outer join eliminated"));
          return TRUE;
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
          DBUG_PRINT("info", ("field %s.%s is now bound",
                               field_dep->field->table->alias,
                               field_dep->field->field_name));

          for (Key_module *key_dep= field_dep->table->keys; key_dep;
               key_dep= key_dep->next_table_key)
          {
            if (field_dep->field->part_of_key.is_set(key_dep->keyno) && 
                key_dep->unknown_args && !--key_dep->unknown_args)
            {
              DBUG_PRINT("info", ("key %s.%s is now bound",
                                  key_dep->table->table->alias, 
                                  key_dep->table->table->key_info[key_dep->keyno].name));
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
            Table is known means that
            - one more element in outer join nest is known
            - all its fields are known
          */
          Outer_join_module *outer_join_dep= table_dep->outer_join_dep;
          if (outer_join_dep->unknown_args && 
              !--outer_join_dep->unknown_args)
          {
            /* Mark as bound and add to the list */
            outer_join_dep->next= bound_modules;
            bound_modules= outer_join_dep;
            break;
          }

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
          break;
        }
        default: 
          DBUG_ASSERT(0);
      }
    }
  }
  return FALSE;
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

