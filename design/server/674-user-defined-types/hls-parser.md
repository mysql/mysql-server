
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
  CREATE TYPE `complex`(
    DOUBLE `real`;
    DOUBLE `imaginary`;
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
- USAGE

Type objects can be specified as:

- `TYPE *.*`
- `TYPE db.*`
- `TYPE db.type`

```sql
  GRANT ALL PRIVILEGES ON TYPE *.* TO ...
  GRANT USAGE ON TYPE *.* TO ...
  GRANT ALL PRIVILEGES ON TYPE db.* TO ...
  GRANT USAGE ON TYPE db.* TO ...
  GRANT ALL PRIVILEGES ON TYPE db.type TO ...
  GRANT USAGE ON TYPE db.type TO ...
```

# REVOKE statement

The revoke statement is extended to match grant

```sql
  REVOKE ALL PRIVILEGES ON TYPE *.* FROM ...
  REVOKE USAGE ON TYPE *.* FROM ...
  REVOKE ALL PRIVILEGES ON TYPE db.* FROM ...
  REVOKE USAGE ON TYPE db.* FROM ...
  REVOKE ALL PRIVILEGES ON TYPE db.type FROM ...
  REVOKE USAGE ON TYPE db.type FROM ...
```

# Method invocation

TODO
