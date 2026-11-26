
# MySQL Optimizer

## Data Type Descriptor

A new structure, `DataTypeDescriptor`, collects all the type information
required to fully represent a type.

This includes:

* the existing `enum_field_types` enum
* type flags, to represent signed/unsigned properties
* size_t m_length, to represent length associated with a type
* bool m_length_specified, to indicate if the length is explicitly defined
* size_t m_dec, to represent the number of decimals associated with a type
* bool m_dec_specified, to indicate if the decimal length is explicitly defined
* a CHARSET_INFO, to representa character set and collation
* bool m_has_explicit_collation, to indicate if a collation is set
  explicitly
* uint m_geo_type, to represent geometry sub types
* the list of values for SET and ENUM types
* The schema name and type name for user defined types.

The code representation is

```c++
struct DataTypeDescriptor {
  enum_field_types m_type{MYSQL_TYPE_INVALID};
  ulong m_type_flags{0};
  bool m_length_specified{false};
  size_t m_length{0};
  bool m_dec_specified{false};
  size_t m_dec{0};
  const CHARSET_INFO *m_charset{nullptr};
  bool m_has_explicit_collation{false};
  uint m_geo_type{0};
  List<String> *m_internal_list{nullptr};
  const Type_ident *m_type_ident{nullptr};
};
```

Every single Item in the item tree has a DataTypeDescriptor
that represents the actual type for the item.

Because boolean predicates are items, it follows that the
DataTypeDescriptor for a boolean predicate needs to represent a BOOL type.

Therefore, MYSQL_TYPE_BOOL is actually used internally.

Types represented by a DataTypeDescriptor in general consist of:

* `Builtin Types`
* `Enum Types`
* `Set Types`
* `User Defined Types`

## Type coercion

TODO

## AST itemization

After invoking the parser, a SQL query is represented by an Abstract Syntax
Tree (AST).

The process is AST itemization consists of transforming abstract nodes
into concrete items, based on the context (types known in the data
dictionary).

The itemization process is affected by User Defined Types.

### Equality 

Operations involving Equality consist of:

* `a = b` predicate expressions
* `a != b` predicate expressions
* `a IN (...)` predicate expressions
* `GROUP BY a` expressions

The itemization process:

* evaluates the DataTypeDescriptor of arguments
* based on the DTD types seen
  * Applies type coercion if required
  * narrow downs the operation for `Builtin Types`
  * narrow downs the operation for `User Defined Types`

Itemization can lead to:

* an equality operation involving builtin types
* an equality operation involving user defined types

The Item class hierarchy is changed as follows:

Before:

```c++
class Item_bool_func2;
class Item_func_comparison: public Item_bool_func2;
class Item_eq_base: public Item_func_comparison;
class Item_func_ne: public Item_func_comparison;
class Item_func_eq: piblic Item_eq_base;
class Item_func_equal: public Item_eq_base;
```

After:

```c++
class Item_bool_func2;
class Item_func_comparison: public Item_bool_func2;

class Item_builtin_func_comparison: public Item_func_comparison;

class Item_builtin_eq_base: public Item_builtin_func_comparison;
class Item_builtin_func_ne: public Item_builtin_func_comparison;
class Item_builtin_func_eq: public Item_builtin_eq_base;
class Item_builtin_func_equal: public Item_builtin_eq_base;

class Item_udt_func_comparison: public Item_func_comparison;

class Item_udt_eq_base: public Item_udt_func_comparison;
class Item_udt_func_ne: public Item_udt_func_comparison;
class Item_udt_func_eq: public Item_udt_eq_base;
class Item_udt_func_equal: public Item_udt_eq_base;
```

Classes `Item_builtin_*` are a renaming of the existing code.

Classes `Item_udt_*` are new code, which invoke the user defined type
implementation during runtime evaluation.

Further itemization of `Item_udt_*` checks whether the User Defined Type
involved does / does not support an ORDERING method.

A query that:

* uses an equality operation
* for a user defined type
* that does not have an ordering method defined

fails with an error.

### Comparison

Operations involving Comparison consist of:

* `a < b` predicate expressions
* `a <= b` predicate expressions
* `a > b` predicate expressions
* `a >= b` predicate expressions
* `ORDER BY a` expressions

The itemization process is similar as equality operations.

Refactoring of the item tree leads to:

```c++
class Item_bool_func2;
class Item_func_comparison: public Item_bool_func2;

class Item_builtin_func_comparison: public Item_func_comparison;

class Item_builtin_ge: public Item_builtin_func_comparison;
class Item_builtin_gt: public Item_builtin_func_comparison;
class Item_builtin_le: public Item_builtin_func_comparison;
class Item_builtin_lt: public Item_builtin_func_comparison;

class Item_udt_func_comparison: public Item_func_comparison;

class Item_udt_ge: public Item_udt_func_comparison;
class Item_udt_gt: public Item_udt_func_comparison;
class Item_udt_le: public Item_udt_func_comparison;
class Item_udt_lt: public Item_udt_func_comparison;
```

Further itemization of `Item_udt_*` checks whether the User Defined Type
involved does / does not support an ORDERING method,
implementing a total order.

A query that:

* uses a comparison operation
* for a user defined type
* that does not have an ordering method defined
* or has an ordering method not supporting full order

fails with an error.

