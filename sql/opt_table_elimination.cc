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
  needs to call (once) sometime after update_ref_and_keys() but before the
  join optimization.  
  eliminate_tables() operates over the JOIN structures. Logically, it
  removes the right sides of outer join nests. Physically, it changes the
  following members:

  * Eliminated tables are marked as constant and moved to the front of the
    join order.
  * In addition to this, they are recorded in JOIN::eliminated_tables bitmap.

  * All join nests have their NESTED_JOIN::n_tables updated to discount
    the eliminated tables

  * Items that became disused because they were in the ON expression of an 
    eliminated outer join are notified by means of the Item tree walk which 
    calls Item::mark_as_eliminated_processor for every item
    - At the moment the only Item that cares whether it was eliminated is 
      Item_subselect with its Item_subselect::eliminated flag which is used
      by EXPLAIN code to check if the subquery should be shown in EXPLAIN.

  Table elimination is redone on every PS re-execution. (TODO reasons?)
*/

/*
  A structure that represents a functional dependency of something over
  something else. This can be one of:

  1. A "tbl.field = expr" equality. The field depends on the expression.
  
  2. An Item_equal(...) multi-equality. Each participating field depends on
     every other participating field. (TODO???)
  
  3. A UNIQUE_KEY(field1, field2, fieldN). The key depends on the fields that
     it is composed of.

  4. A table (which is within an outer join nest). Table depends on a unique
     key (value of a unique key identifies a table record)

  5. An outer join nest. It depends on all tables it contains.

*/

class Func_dep : public Sql_alloc
{
public:
  enum {
    FD_INVALID,
    FD_EXPRESSION,
    FD_FIELD,
    FD_MULTI_EQUALITY,
    FD_UNIQUE_KEY,
    FD_TABLE,
    FD_OUTER_JOIN
  } type;
  Func_dep *next;
  bool bound;
  Func_dep() : next(NULL), bound(FALSE) {}
};


class Field_dep;
class Table_dep;
class Outer_join_dep;

/*
  An equality
  - Depends on multiple fields (those in its expression), unknown_args is a 
    counter of unsatisfied dependencies.
*/
class Equality_dep : public Func_dep
{
public:
  Field_dep *field;
  Item  *val;
  
  uint level; /* Used during condition analysis only */
  uint unknown_args; /* Number of yet unknown arguments */
};


/*
  A field.
  - Depends on table or equality
  - Has expressions it participates as dependencies

  There is no counter, bound fields are in $list, not bound are not.
*/
class Field_dep : public Func_dep
{
public:
  Field_dep(Table_dep *table_arg, Field *field_arg) :
    table(table_arg), field(field_arg)
  {
    type= Func_dep::FD_FIELD;
  }
  /* Table we're from. It also has pointers to keys that we're part of */
  Table_dep *table;
  Field *field;
  
  Field_dep *next_table_field;
  uint bitmap_offset; /* Offset of our part of the bitmap */
};


/*
  A unique key.
   - Depends on all its components
   - Has its table as dependency
*/
class Key_dep: public Func_dep
{
public:
  Key_dep(Table_dep *table_arg, uint keyno_arg, uint n_parts_arg) :
    table(table_arg), keyno(keyno_arg), n_missing_keyparts(n_parts_arg),
    next_table_key(NULL)
  {
    type= Func_dep::FD_UNIQUE_KEY;
  }
  Table_dep *table; /* Table this key is from */
  uint keyno; // TODO do we care about this
  uint n_missing_keyparts;
  Key_dep *next_table_key;
};


/*
  A table. 
  - Depends on any of its unique keys
  - Has its fields and embedding outer join as dependency.
*/
class Table_dep : public Func_dep
{
public:
  Table_dep(TABLE *table_arg) : 
    table(table_arg), fields(NULL), keys(NULL), outer_join_dep(NULL)
  {
    type= Func_dep::FD_TABLE;
  }
  TABLE *table;
  Field_dep *fields; /* Fields that belong to this table */
  Key_dep *keys; /* Unique keys */
  Outer_join_dep *outer_join_dep;
};


/*
  An outer join nest. 
  - Depends on all tables inside it.
  - (And that's it).
*/
class Outer_join_dep: public Func_dep
{
public:
  Outer_join_dep(TABLE_LIST *table_list_arg, table_map missing_tables_arg) : 
    table_list(table_list_arg), missing_tables(missing_tables_arg),
    all_tables(missing_tables_arg), parent(NULL)
  {
    type= Func_dep::FD_OUTER_JOIN;
  }
  TABLE_LIST *table_list;
  table_map missing_tables;
  table_map all_tables;
  Outer_join_dep *parent;
};


/* TODO need this? */
class Table_elimination
{
public:
  Table_elimination(JOIN *join_arg) : join(join_arg)
  {
    bzero(table_deps, sizeof(table_deps));
  }

  JOIN *join;
  /* Array of equality dependencies */
  Equality_dep *equality_deps;
  uint n_equality_deps; /* Number of elements in the array */

  /* tablenr -> Table_dep* mapping. */
  Table_dep *table_deps[MAX_KEY];

  /* Outer joins that are candidates for elimination */
  List<Outer_join_dep> oj_deps;

  /* Bitmap of how expressions depend on bits */
  MY_BITMAP expr_deps;
};


static 
void build_funcdeps_for_cond(Table_elimination *te, Equality_dep **fdeps, 
                             uint *and_level, Item *cond, 
                             table_map usable_tables);
static 
void add_funcdep(Table_elimination *te, 
                 Equality_dep **eq_dep, uint and_level,
                 Item_func *cond, Field *field,
                 bool eq_func, Item **value,
                 uint num_values, table_map usable_tables);
static 
Equality_dep *merge_func_deps(Equality_dep *start, Equality_dep *new_fields, 
                              Equality_dep *end, uint and_level);

Field_dep *get_field_dep(Table_elimination *te, Field *field);
void eliminate_tables(JOIN *join);
static void mark_as_eliminated(JOIN *join, TABLE_LIST *tbl);

#ifndef DBUG_OFF
static void dbug_print_deps(Table_elimination *te);
#endif 

/*******************************************************************************************/

/*
  Produce FUNC_DEP elements for the given item (i.e. condition) and add them 
    to fdeps array.

  SYNOPSIS
    build_funcdeps_for_cond()
      fdeps  INOUT   Put created FUNC_DEP structures here

  DESCRIPTION
    a

  SEE ALSO
    add_key_fields()

*/
static 
void build_funcdeps_for_cond(Table_elimination *te, 
                             Equality_dep **fdeps, uint *and_level, Item *cond,
                             table_map usable_tables)
{
  if (cond->type() == Item_func::COND_ITEM)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Equality_dep *org_key_fields= *fdeps;
    
    /* AND/OR */
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item=li++))
      {
        build_funcdeps_for_cond(te, fdeps, and_level, item, usable_tables);
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
      build_funcdeps_for_cond(te, fdeps, and_level, li++, usable_tables);
      Item *item;
      while ((item=li++))
      {
        Equality_dep *start_key_fields= *fdeps;
        (*and_level)++;
        build_funcdeps_for_cond(te, fdeps, and_level, item, usable_tables);
        *fdeps= merge_func_deps(org_key_fields, start_key_fields, *fdeps,
                                ++(*and_level));
      }
    }
    return;
  }

  if (cond->type() != Item::FUNC_ITEM)
    return;
  Item_func *cond_func= (Item_func*) cond;
  switch (cond_func->select_optimize()) {
  case Item_func::OPTIMIZE_NONE:
    break;
  case Item_func::OPTIMIZE_KEY:
  {
    Item **values;
    // BETWEEN, IN, NE
    if (cond_func->key_item()->real_item()->type() == Item::FIELD_ITEM &&
       !(cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      values= cond_func->arguments()+1;
      if (cond_func->functype() == Item_func::NE_FUNC &&
          cond_func->arguments()[1]->real_item()->type() == Item::FIELD_ITEM &&
         !(cond_func->arguments()[0]->used_tables() & OUTER_REF_TABLE_BIT))
        values--;
      DBUG_ASSERT(cond_func->functype() != Item_func::IN_FUNC ||
                  cond_func->argument_count() != 2);
      add_funcdep(te, fdeps, *and_level, cond_func,
                  ((Item_field*)(cond_func->key_item()->real_item()))->field,
                  0, values, 
                  cond_func->argument_count()-1,
                  usable_tables);
    }
    if (cond_func->functype() == Item_func::BETWEEN)
    {
      values= cond_func->arguments();
      for (uint i= 1 ; i < cond_func->argument_count() ; i++)
      {
        Item_field *field_item;
        if (cond_func->arguments()[i]->real_item()->type() == Item::FIELD_ITEM
            &&
            !(cond_func->arguments()[i]->used_tables() & OUTER_REF_TABLE_BIT))
        {
          field_item= (Item_field *) (cond_func->arguments()[i]->real_item());
          add_funcdep(te, fdeps, *and_level, cond_func,
                      field_item->field, 0, values, 1, usable_tables);
        }
      }  
    }
    break;
  }
  case Item_func::OPTIMIZE_OP:
  {
    bool equal_func=(cond_func->functype() == Item_func::EQ_FUNC ||
                     cond_func->functype() == Item_func::EQUAL_FUNC);

    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM &&
        !(cond_func->arguments()[0]->used_tables() & OUTER_REF_TABLE_BIT))
    {
      add_funcdep(te, fdeps, *and_level, cond_func,
                  ((Item_field*)(cond_func->arguments()[0])->real_item())->field,
                  equal_func,
                  cond_func->arguments()+1, 1, usable_tables);
    }
    if (cond_func->arguments()[1]->real_item()->type() == Item::FIELD_ITEM &&
        cond_func->functype() != Item_func::LIKE_FUNC &&
        !(cond_func->arguments()[1]->used_tables() & OUTER_REF_TABLE_BIT))
    {
      add_funcdep(te, fdeps, *and_level, cond_func, 
                  ((Item_field*)(cond_func->arguments()[1])->real_item())->field,
                  equal_func,
                  cond_func->arguments(),1,usable_tables);
    }
    break;
  }
  case Item_func::OPTIMIZE_NULL:
    /* column_name IS [NOT] NULL */
    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM &&
        !(cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      Item *tmp=new Item_null;
      if (unlikely(!tmp))                       // Should never be true
        return;
      add_funcdep(te, fdeps, *and_level, cond_func,
                  ((Item_field*)(cond_func->arguments()[0])->real_item())->field,
                  cond_func->functype() == Item_func::ISNULL_FUNC,
                  &tmp, 1, usable_tables);
    }
    break;
  case Item_func::OPTIMIZE_EQUAL:
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
        add_funcdep(te, fdeps, *and_level, cond_func, item->field,
                    TRUE, &const_item, 1, usable_tables);
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
        while ((item= it++))
        {
          if (!field->eq(item->field))
          {
            add_funcdep(te, fdeps, *and_level, cond_func, field/*item*/,
                        TRUE, (Item **) &item, 1, usable_tables);
          }
        }
        it.rewind();
      }
    }
    break;
  }
}

/*
  Perform an OR operation on two (adjacent) FUNC_DEP arrays.

  SYNOPSIS
     merge_func_deps()

  DESCRIPTION

  This function is invoked for two adjacent arrays of FUNC_DEP elements:

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
Equality_dep *merge_func_deps(Equality_dep *start, Equality_dep *new_fields, 
                              Equality_dep *end, uint and_level)
{
  if (start == new_fields)
    return start;				// Impossible or
  if (new_fields == end)
    return start;				// No new fields, skip all

  Equality_dep *first_free=new_fields;

  for (; new_fields != end ; new_fields++)
  {
    for (Equality_dep *old=start ; old != first_free ; old++)
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
	if (!new_fields->val->const_item())
	{
	  /*
	    If the value matches, we can use the key reference.
	    If not, we keep it until we have examined all new values
	  */
	  if (old->val->eq(new_fields->val, old->field->field->binary()))
	  {
	    old->level= and_level;
	  }
	}
	else if (old->val->eq_by_collation(new_fields->val, 
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
  for (Equality_dep *old=start ; old != first_free ;)
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
  Add a funcdep for a given equality.
*/

static 
void add_funcdep(Table_elimination *te, 
                 Equality_dep **eq_dep, uint and_level,
                 Item_func *cond, Field *field,
                 bool eq_func, Item **value,
                 uint num_values, table_map usable_tables)
{
 // Field *field= item_field->field;
  if (!(field->table->map & usable_tables))
    return;

  for (uint i=0; i<num_values; i++)
  {
    if ((value[i])->used_tables() & RAND_TABLE_BIT)
      return;
  }

  /*
    Save the following cases:
    Field op constant
    Field LIKE constant where constant doesn't start with a wildcard
    Field = field2 where field2 is in a different table
    Field op formula
    Field IS NULL
    Field IS NOT NULL
     Field BETWEEN ...
     Field IN ...
  */

  /*
    We can't always use indexes when comparing a string index to a
    number. cmp_type() is checked to allow compare of dates to numbers.
    eq_func is NEVER true when num_values > 1
   */
  if (!eq_func)
  {
    /* 
      Additional optimization: if we're processing "t.key BETWEEN c1 AND c1"
      then proceed as if we were processing "t.key = c1".
    */
    if ((cond->functype() != Item_func::BETWEEN) ||
        ((Item_func_between*) cond)->negated ||
        !value[0]->eq(value[1], field->binary()))
      return;
    eq_func= TRUE;
  }

  if (field->result_type() == STRING_RESULT)
  {
    if ((*value)->result_type() != STRING_RESULT)
    {
      if (field->cmp_type() != (*value)->result_type())
        return;
    }
    else
    {
      /*
        We can't use indexes if the effective collation
        of the operation differ from the field collation.
      */
      if (field->cmp_type() == STRING_RESULT &&
          ((Field_str*)field)->charset() != cond->compare_collation())
        return;
    }
  }

  DBUG_ASSERT(eq_func);
  /* Store possible eq field */
  (*eq_dep)->type=  Func_dep::FD_EXPRESSION; //psergey-todo;
  (*eq_dep)->field= get_field_dep(te, field); 
  (*eq_dep)->val=   *value;
  (*eq_dep)->level= and_level;
  (*eq_dep)++;
}


Table_dep *get_table_dep(Table_elimination *te, TABLE *table)
{
  Table_dep *tbl_dep= new Table_dep(table);
  Key_dep **key_list= &(tbl_dep->keys);

  /* Add dependencies for unique keys */
  for (uint i=0; i < table->s->keys; i++)
  {
    KEY *key= table->key_info + i; 
    if ((key->flags & (HA_NOSAME | HA_END_SPACE_KEY)) == HA_NOSAME)
    {
      Key_dep *key_dep= new Key_dep(tbl_dep, i, key->key_parts);
      *key_list= key_dep;
      key_list= &(key_dep->next_table_key);
    }
  }
  return te->table_deps[table->tablenr] = tbl_dep;
}

/* 
  Given a field, get its dependency element: if it already exists, find it,
  otherwise create it.
*/

Field_dep *get_field_dep(Table_elimination *te, Field *field)
{
  TABLE *table= field->table;
  Table_dep *tbl_dep;

  if (!(tbl_dep= te->table_deps[table->tablenr]))
    tbl_dep= get_table_dep(te, table);

  Field_dep **pfield= &(tbl_dep->fields);
  while (*pfield && (*pfield)->field->field_index < field->field_index)
  {
    pfield= &((*pfield)->next_table_field);
  }
  if (*pfield && (*pfield)->field->field_index == field->field_index)
    return *pfield;
  
  Field_dep *new_field= new Field_dep(tbl_dep, field);
  
  new_field->next_table_field= *pfield;
  *pfield= new_field;
  return new_field;
}


Outer_join_dep *get_outer_join_dep(Table_elimination *te, 
                                   TABLE_LIST *outer_join, table_map deps_map)
{
  Outer_join_dep *oj_dep;
  oj_dep= new Outer_join_dep(outer_join, deps_map);

  Table_map_iterator it(deps_map);
  int idx;
  while ((idx= it.next_bit()) != Table_map_iterator::BITMAP_END)
  {
    Table_dep *table_dep;
    if (!(table_dep= te->table_deps[idx]))
    {
      TABLE *table= NULL;
      /* 
        Locate and create the table. The search isnt very efficient but 
        typically we won't get here as we process the ON expression first
        and that will create the Table_dep
      */
      for (uint i= 0; i < te->join->tables; i++)
      {
        if (te->join->join_tab[i].table->tablenr == (uint)idx)
        {
          table= te->join->join_tab[i].table;
          break;
        }
      }
      DBUG_ASSERT(table);
      table_dep= get_table_dep(te, table);
    }

    if (!table_dep->outer_join_dep)
      table_dep->outer_join_dep= oj_dep;
    else
    {
      Outer_join_dep *oj= table_dep->outer_join_dep;
      while (oj->parent)
        oj= oj->parent;
      oj->parent=oj_dep;
    }

  }
  return oj_dep;
}


/*
  Perform table elimination in a given join list

  SYNOPSIS
    collect_funcdeps_for_join_list()
      te                      Table elimination context.
      join_list               Join list to work on 
      its_outer_join          TRUE <=> the join_list is an inner side of an 
                                       outer join 
                              FALSE <=> otherwise (this is top-level join 
                                        list, simplify_joins flattens out all
                                        other kinds of join lists)
                                                    
      tables_in_list          Bitmap of tables embedded in the join_list.
      tables_used_elsewhere   Bitmap of tables that are referred to from
                              somewhere outside of the join list (e.g.
                              select list, HAVING, etc).

  DESCRIPTION
    Perform table elimination for a join list.
    Try eliminating children nests first.
    The "all tables in join nest can produce only one matching record
    combination" property checking is modeled after constant table detection,
    plus we reuse info attempts to eliminate child join nests.

  RETURN
    Number of children left after elimination. 0 means everything was
    eliminated.
*/

static uint
collect_funcdeps_for_join_list(Table_elimination *te,
                               List<TABLE_LIST> *join_list,
                               bool build_eq_deps,
                               table_map tables_used_elsewhere,
                               table_map *eliminable_tables,
                               Equality_dep **eq_dep)
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
        collect_funcdeps_for_join_list(te, &tbl->nested_join->join_list,
                                       eliminable || build_eq_deps,
                                       outside_used_tables,
                                       eliminable_tables,
                                       eq_dep);
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
        build_funcdeps_for_cond(te, eq_dep, &and_level, tbl->on_expr, 
                                *eliminable_tables);
      }

      if (eliminable)
        te->oj_deps.push_back(get_outer_join_dep(te, tbl, cur_map));

      tables_used_on_left |= tbl->on_expr->used_tables();
    }
  }
  return 0;
}

/*
  Analyze exising FUNC_DEP array and add elements for tables and uniq keys

  SYNOPSIS
  
  DESCRIPTION
    Add FUNC_DEP elements

  RETURN
    .
*/

class Field_dependency_setter : public Field_enumerator
{
public:
  Field_dependency_setter(Table_elimination *te_arg): te(te_arg)
  {}
  
  void see_field(Field *field)
  {
    Table_dep *tbl_dep;
    if ((tbl_dep= te->table_deps[field->table->tablenr]))
    {
      for (Field_dep *field_dep= tbl_dep->fields; field_dep; 
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
      /* We didn't find the field. Bump the dependency anyway */
      te->equality_deps[expr_offset].unknown_args++;
    }
  }
  Table_elimination *te;
  uint expr_offset; /* Offset of the expression we're processing */
};


static 
bool setup_equality_deps(Table_elimination *te, Func_dep **bound_deps_list)
{
  DBUG_ENTER("setup_equality_deps");
  
  uint offset= 0;
  for (Table_dep **tbl_dep=te->table_deps; 
       tbl_dep < te->table_deps + MAX_TABLES;
       tbl_dep++)
  {
    if (*tbl_dep)
    {
      for (Field_dep *field_dep= (*tbl_dep)->fields;
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
    Walk through all field=expr elements and collect all fields.
  */
  Func_dep *bound_dep= NULL;
  Field_dependency_setter deps_setter(te);
  for (Equality_dep *eq_dep= te->equality_deps; 
       eq_dep < te->equality_deps + te->n_equality_deps;
       eq_dep++)
  {
    deps_setter.expr_offset= eq_dep - te->equality_deps;
    eq_dep->unknown_args= 0;
    eq_dep->val->walk(&Item::check_column_usage_processor, FALSE, 
                      (uchar*)&deps_setter);
    if (!eq_dep->unknown_args)
    {
      eq_dep->next= bound_dep;
      bound_dep= eq_dep;
      eq_dep->bound= TRUE;
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
    if (!(te.equality_deps= new Equality_dep[max_elems]))
      DBUG_VOID_RETURN;
    Equality_dep *eq_deps_end= te.equality_deps;
    table_map eliminable_tables= 0;
    collect_funcdeps_for_join_list(&te, join->join_list,
                                   FALSE,
                                   used_tables,
                                   &eliminable_tables,
                                   &eq_deps_end);
    te.n_equality_deps= eq_deps_end - te.equality_deps;
    Func_dep *bound_dep;
    setup_equality_deps(&te, &bound_dep);

    /* 
      Run the wave.
      All Func_dep-derived objects are divided into three classes:
      - Those that have bound=FALSE
      - Those that have bound=TRUE 
      - Those that have bound=TRUE and are in the list..

    */
    while (bound_dep)
    {
      Func_dep *next= bound_dep->next;
      //e= list.remove_first();
      switch (bound_dep->type)
      {
        case Func_dep::FD_EXPRESSION:
        {
          /*  It's a field=expr and we got to know the expr, so we know the field */
          Equality_dep *eq_dep= (Equality_dep*)bound_dep;
          if (!eq_dep->field->bound)
          {
            /* Mark as bound and add to the list */
            eq_dep->field->bound= TRUE;
            eq_dep->field->next= next;
            next= eq_dep->field;
          }
          break;
        }
        case Func_dep::FD_FIELD:
        {
          /*
            Field became known. Check out
            - unique keys we belong to
            - expressions that depend on us.
          */
          Field_dep *field_dep= (Field_dep*)bound_dep;
          for (Key_dep *key_dep= field_dep->table->keys; key_dep;
               key_dep= key_dep->next_table_key)
          {
            DBUG_PRINT("info", ("key %s.%s is now bound",
                                key_dep->table->table->alias, 
                                key_dep->table->table->key_info[key_dep->keyno].name));
            if (field_dep->field->part_of_key.is_set(key_dep->keyno) && 
                !key_dep->bound)
            {
              if (!--key_dep->n_missing_keyparts)
              {
                /* Mark as bound and add to the list */
                key_dep->bound= TRUE;
                key_dep->next= next;
                next= key_dep;
              }
            }
          }

          /* Now, expressions */
          for (uint i=0; i < te.n_equality_deps; i++)
          {
            if (bitmap_is_set(&te.expr_deps, field_dep->bitmap_offset + i))
            {
              Equality_dep* eq_dep= &te.equality_deps[i];
              if (!--eq_dep->unknown_args)
              {
                /* Mark as bound and add to the list */
                eq_dep->bound= TRUE;
                eq_dep->next= next;
                next= eq_dep;
              }
            }
          }
          break;
        }
        case Func_dep::FD_UNIQUE_KEY:
        {
          /* Unique key is known means the table is known */
          Table_dep *table_dep=((Key_dep*)bound_dep)->table;
          if (!table_dep->bound)
          {
            /* Mark as bound and add to the list */
            table_dep->bound= TRUE;
            table_dep->next= next;
            next= table_dep;
          }
          break;
        }
        case Func_dep::FD_TABLE:
        {
          Table_dep *table_dep=(Table_dep*)bound_dep; 
          DBUG_PRINT("info", ("table %s is now bound",
                              table_dep->table->alias));
          /*
            Table is known means
            - all its fields are known
            - one more element in outer join nest is known
          */
          for (Field_dep *field_dep= table_dep->fields; field_dep; 
               field_dep= field_dep->next_table_field)
          {
            if (!field_dep->bound)
            {
              /* Mark as bound and add to the list */
              field_dep->bound= TRUE;
              field_dep->next= next;
              next= field_dep;
            }
          }
          Outer_join_dep *outer_join_dep= table_dep->outer_join_dep;
          if (!(outer_join_dep->missing_tables &= ~table_dep->table->map))
          {
            /* Mark as bound and add to the list */
            outer_join_dep->bound= TRUE;
            outer_join_dep->next= next;
            next= outer_join_dep;
          }
          break;
        }
        case Func_dep::FD_OUTER_JOIN:
        {
          Outer_join_dep *outer_join_dep= (Outer_join_dep*)bound_dep;
          mark_as_eliminated(te.join, outer_join_dep->table_list);
          Outer_join_dep *parent= outer_join_dep->parent;
          if (parent && 
              !(parent->missing_tables &= ~outer_join_dep->all_tables))
          {
            /* Mark as bound and add to the list */
            parent->bound= TRUE;
            parent->next= next;
            next= parent;
          }
          break;
        }
        case Func_dep::FD_MULTI_EQUALITY:
        default:
          DBUG_ASSERT(0);
      }
      bound_dep= next;
    }
  }
  DBUG_VOID_RETURN;
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
  for (Equality_dep *eq_dep= te->equality_deps; 
       eq_dep != te->equality_deps + te->n_equality_deps; eq_dep++)
  {
    char buf[128];
    String str(buf, sizeof(buf), &my_charset_bin);
    str.length(0);
    eq_dep->val->print(&str, QT_ORDINARY);
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
    Table_dep *table_dep;
    if ((table_dep= te->table_deps[i]))
    {
      /* Print table */
      fprintf(DBUG_FILE, "  table %s\n", table_dep->table->alias);
      /* Print fields */
      for (Field_dep *field_dep= table_dep->fields; field_dep; 
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

