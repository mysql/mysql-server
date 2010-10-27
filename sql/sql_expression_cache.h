#ifndef SQL_EXPRESSION_CACHE_INCLUDED
#define SQL_EXPRESSION_CACHE_INCLUDED

#include "sql_select.h"

/**
  Interface for expression cache

  @note
  Parameters of an expression cache interface are set on the creation of the
  cache. They are passed when a cache object of the implementation class is
  constructed. That's why they are not visible in this interface.
*/

extern ulong subquery_cache_miss, subquery_cache_hit;

class Expression_cache :public Sql_alloc
{
public:
  enum result {ERROR, HIT, MISS};

  Expression_cache(){};
  virtual ~Expression_cache() {};
  /**
    Shall check the presence of expression value in the cache for a given
    set of values of the expression parameters.  Return the result of the
    expression if it's found in the cache.
  */
  virtual result check_value(Item **value)= 0;
  /**
    Shall put the value of an expression for given set of its parameters
    into the expression cache
  */
  virtual my_bool put_value(Item *value)= 0;

  /**
    Print cache parameters
  */
  virtual void print(String *str, enum_query_type query_type)= 0;
};

struct st_table_ref;
struct st_join_table;
class Item_field;


/**
  Implementation of expression cache over a temporary table
*/

class Expression_cache_tmptable :public Expression_cache
{
public:
  Expression_cache_tmptable(THD *thd, List<Item*> &dependants, Item *value);
  virtual ~Expression_cache_tmptable();
  virtual result check_value(Item **value);
  virtual my_bool put_value(Item *value);

  void print(String *str, enum_query_type query_type);

private:
  void init();

  /* tmp table parameters */
  TMP_TABLE_PARAM cache_table_param;
  /* temporary table to store this cache */
  TABLE *cache_table;
  /* Thread handle for the temporary table */
  THD *table_thd;
  /* TABLE_REF for index lookup */
  struct st_table_ref ref;
  /* Cached result */
  Item_field *cached_result;
  /* List of references to the parameters of the expression */
  List<Item*> *list;
  /* List of items */
  List<Item> items;
  /* Value Item example */
  Item *val;
  /* Set on if the object has been succesfully initialized with init() */
  bool inited;
};

#endif /* SQL_EXPRESSION_CACHE_INCLUDED */
