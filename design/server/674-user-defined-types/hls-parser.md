
# CREATE TYPE statement

## Syntax for basic types

```sql
<create_type_stmt>:
  CREATE TYPE <type_ident> AS <builtin_type>
  ;
```

```sql
<type_ident>:
    IDENT_sys
  | IDENT_sys '.' IDENT_sys
  ;
```

```sql
<builtin_type>:
  // existing MySQL builtin types
  ;
```

For example:

```sql
  // complex, using a basic type
  CREATE TYPE `complex` AS BINARY(16);
```

This syntax defines a type as `basic type`,
encapsulated in an underlying storage type.

This is one of the possible options defined in the SQL spec.

## Syntax for composite types

The SQL specification also defines structured types:

```sql
<create_type_stmt>:
  CREATE TYPE <type_ident> AS ( <member_list> )
  ;
```

For example:

```sql
  // complex, using a structured type
  CREATE TYPE `complex` AS (
    `real` DOUBLE;
    `imaginary` DOUBLE;
  );
```

This is a valid use case, to save duplication for the type definition
itself, when used in multiple places.

This form is out of scope for the first release.
Decision to support this, or not, is postponed.

# ALTER TYPE statement

In the SQL specification, ALTER TYPE is used to:
- ADD/DROP attributes
- ADD/DROP methods

We do not have attributes, since composite types are not supported.

For methods available on a type, these are not declared using the SQL
language, but provided by the component implementing a type directly,
using a service exposed by the MySQL server.

As a result, there is no ALTER TYPE statement implemented,
as there is nothing to alter currently.

# DROP TYPE statement

```sql
<drop_type_stmt>:
  DROP TYPE <type_ident>
  ;
```

For example:

```sql
  DROP TYPE `complex`;
```

# GRANT statement

A TYPE is a SQL object.

The GRANT statement is extended to user defined types.

Privileges that can be granted are:

- ALL PRIVILEGES
- CREATE
- DROP
- USAGE

Type objects can be specified as:

- `TYPE *.*`
- `TYPE db.*`
- `TYPE db.type`

```sql
  GRANT ALL PRIVILEGES ON TYPE *.* TO ...
  GRANT CREATE ON TYPE *.* TO ...
  GRANT DROP ON TYPE *.* TO ...
  GRANT USAGE ON TYPE *.* TO ...

  GRANT ALL PRIVILEGES ON TYPE db.* TO ...
  GRANT CREATE ON TYPE db.* TO ...
  GRANT DROP ON TYPE db.* TO ...
  GRANT USAGE ON TYPE db.* TO ...

  GRANT ALL PRIVILEGES ON TYPE db.type TO ...
  GRANT CREATE ON TYPE db.type TO ...
  GRANT DROP ON TYPE db.type TO ...
  GRANT USAGE ON TYPE db.type TO ...
```

# REVOKE statement

The revoke statement is extended to match grant

```sql
  REVOKE ALL PRIVILEGES ON TYPE *.* FROM ...
  REVOKE CREATE ON TYPE *.* FROM ...
  REVOKE DROP ON TYPE *.* FROM ...
  REVOKE USAGE ON TYPE *.* FROM ...

  REVOKE ALL PRIVILEGES ON TYPE db.* FROM ...
  REVOKE CREATE ON TYPE db.* FROM ...
  REVOKE DROP ON TYPE db.* FROM ...
  REVOKE USAGE ON TYPE db.* FROM ...

  REVOKE ALL PRIVILEGES ON TYPE db.type FROM ...
  REVOKE CREATE ON TYPE db.type FROM ...
  REVOKE DROP ON TYPE db.type FROM ...
  REVOKE USAGE ON TYPE db.type FROM ...
```

# Method invocation

## Static method invocation

The syntax for expressions is augmented to cover static method invocations.

```sql
  <simple_expr>:
    <function_call_static_method>
    ;
```

```sql
  <function_call_static_method>:
    <type_ident> <::> <ident> ( <opt_expr_list> )
    ;
```

Examples:

```sql
  complex_col = complex::from_string("1+2i");
```

```sql
  // 1+2i
  complex_col = complex::from_cartesian(1.0, 2.0);
```

```sql
  // 0+1i
  complex_col = complex::from_polar(1.0, pi/2);
```

```sql
  complex_col = complex::add(col_a, col_b);
```

## method invocation

The existing syntax for expressions is augmented to cover instance method invocations.

```sql
  <simple_expr>:
    <function_call_generic>
    ;
```

```sql
  <function_call_generic>:
      IDENT_sys '(' opt_udf_expr_list ')'
      {
         // existing PTI_function_call_generic_ident_sys, unchanged
      }
    | ident '.' ident '(' opt_expr_list ')'
      {
         // existing PTI_function_call_generic_2d, unchanged
      }
    | ident '.' ident '.' ident '(' opt_expr_list ')'
      {
         // new PTI_function_call_generic_3d
      }
    ;
```

In the parse tree, PTI_function_call_generic_2d represents both:
- calls to stored functions schema.name()
- calls to user defined types methods in the default schema,
  type.method()
Stored functions take precedence on name collisions.

In the parse tree, PTI_function_call_generic_3d represents
a call to a fully qualified type method, schema.type.method().

Examples:

```sql
  string_col = complex_col.to_string();
```

```sql
  double_col = complex_col.`real`();
```

```sql
  double_col = complex_col.`imaginary`();
```

# global function invocation

There is no syntax for a global function, such as:

```sql
  // Not supported
  complex_col = complex_from_string("1+2i");
```

The rationale is to force users to invoke types that belong to a schema,
to enforce per schema namespaces instead of having naming collisions
in a global namespace, shared by all types.

```sql
  // Supported instead
  CREATE TYPE mysql.complex AS ...;

  use mysql;
  complex_col = complex::from_string("1.2i");
  double_col = complex_col.`real`(); // 1.0

  use test;
  complex_col = mysql.complex::from_string("1+2i");
  double_col = complex_col.`imaginary`(); // 2.0
```

This also avoids naming collisions between:

- native functions provided by MySQL
- global user defined type functions provided by third parties

