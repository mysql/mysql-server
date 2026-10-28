
# Requirements

## MySQL Editions

FR:

Support for User Data Type (UDT) is available in all editions of the server:

* Community edition
* Enterprise edition
* Cloud editions

NFR:

When a party provides a specific User Data Type implementation,
the party owns the implementation, and decides how to license the type
provided.

## MySQL Versions

The target for this project is the main branch only, release 27.x

No backports are planned to older releases.

NFR:

If at all possible, this project should try to mitigate disruptions
when porting fixes from older releases to the main branch.

Design constraints include:

* to isolate User Defined Type specific code to dedicated files,
  to prevent merge collisions (when practical).
* to isolate User Defined Type specific tests to dedicated files,
  to prevent merge collisions (when practical).

## Deployment

Deployment consist of:

* adding a specific User Data Type to a MySQL server instance
* removing a specific User Data Type from a MySQL server instance

FR:

Deployment is dynamic, and does not require a server restart.

FR:

Deployment is a privileged operation.

## Discoverability

### Type definition discoverability

Because User Data Types installed in a server are optional, users need to discover what is available.

FR:

A table INFORMATION_SCHEMA.TYPES is exposed by the server
to list all available User Data Types.

FR:

A table INFORMATION_SCHEMA.TYPE_OPERATIONS is exposed by the server
to list all operations available for a given User Data Type.

FR:

A table INFORMATION_SCHEMA.TYPE_OPERATION_PARAMETERS is exposed by the server
to describe the full operation signature for a given operation on a given User Data Type.

### Type usage discoverability

Users need to find out where a given type is in use,
to maintain an application in general.

FR: 

The table INFORMATION_SCHEMA.COLUMNS is extended to expose
the User Data Type used by a column in a table, if applicable

FR:

The table INFORMATION_SCHEMA.ROUTINES is extended to expose
the User Data Type used by the return type of a stored function, if applicable

FR:

The table INFORMATION_SCHEMA.PARAMETERS is extended to expose
the User Data Type used by a parameter in a stored program, if applicable

Note:

A type can also be used inside a stored program body.
Reporting types used by stored programs body was considered,
but discarded.

## SQL Language

### Type DDL

FR:

A TYPE is a SQL object.
It belongs to a SCHEMA.

FR:

The syntax for DDL operations on a type is a subset of the ANSI SQL syntax.

New statements supported are:

* CREATE TYPE
* DROP TYPE

### Type DML

FR:

Invoking an operation on a type is a valid SQL expression.

The syntax for method invocations on a type is a subset of the ANSI SQL
syntax.

New expressions supported are:

* Invoking a static method on a TYPE
* Invoking a dynamic method on a TYPE

### Type usage

FR:

A User Defined Type can be used everywhere a builtin MySQL type is used,
except for the following restrictions:

* User Defined Types can not be used in an INDEX
* User Defined Types can not be used in an PRIMARY KEY
* User Defined Types can not be used in an FOREIGN KEY

Note:

These restrictions are due to initial project scoping,
and may be revised later (User Defined Indexes).

### Type expression validity

FR:

Expressions invoking a method on a type are checked against the type,
method, and parameters.

Invalid invocations:

* using a type that does not exists
* invoking a method on a SQL object that is not typed with a User Defined Type
* using a method that does not exists
* invoking a static method with the dynamic syntax
* invoking a dynamic method with the static syntax
* using parameters that do not comply with the method signature

are detected and reported as errors.

FR:

Verification of the expression validity is NOT performed during parsing.

FR:

Verification of the expression validity is performed during runtime
execution.

As a consequence:

* an invalid expression that is not reachable (IFNULL, CASE expressions,
  impossible WHERE clauses) is not detected at parse time
* an invalid expression in a prepared statement is not detected during PREPARE
* in invalid expression in a stored program is not detected during CREATE

### DDL constraints

FR:

DROP TYPE fails with an error if the type is used by a column in a table.

FR:

DROP TYPE does not enforce that no stored programs uses the type.

This is a deviation from the SQL standard.
The rationale for this behavior is consistency with other objects in MySQL:
DROP TABLE is allowed even when a stored program uses the table.

FR:

DROP SCHEMA implicitly executes DROP TYPE
for all types present in the schema (cascade behavior)

## Type implementation

### Components

NFR:

Types are implemented as a COMPONENT, implemented in C++.

This choice is driven by performance considerations.

Note that a component is considered trusted code when loaded into the server.

There are no safeguards such as an internal container to constrain code
execution, to prevent failures caused by third party code.

FR:

The server supports adding a type:

* from a component dynamically installed, using INSTALL COMPONENT.
  * This allows a third party to provide optional types.
  * This allows MySQL to provide optional types.

NFR:

The server supports adding a type:

* from a component statically linked with the server.
  * This allows MySQL to provide additional builtin types,
    while implementing the builtin using the same infrastructure
    as user defined types.
  * This allows MySQL to promote optional types to builtin types,
    with minimal code changes.

## Services

NFR:

The server exposes the services needed for a third party to implement a
type.

NFR:

An implementation of a type in a component should depend only on exposed
services. Dependencies on server internals are forbidden.

NFR:

As new features for User Defined Types are implemented over time:

* new services may be added in future releases
* existing services maybe extended in future releases

Service definitions are versioned and provide a stable interface,
for backward compatibility (once a service is declared stable).

## No registration authority

NFR:

A third party implementing a type MUST NOT be required to register a name or
identifier with a central authority to provide a type implementation.

NFR:

The doxygen technical documentation, for type implementers,
describes guidelines and best practices to avoid naming collisions,
to favor a healthy ecosystem of types provided by various vendors.

## Replication

TODO

## Install / Upgrade

TODO

## Backup / Restore

TODO

## Client

TODO

## Degraded behavior

TODO, behavior when a type is declared, but implementation is not loaded.

## Observability

At runtime, the server will execute code provided as a User Defined Type to perform operations on a type.

FR:

The server exposes a performance_schema table to report statistics about individual operations executions.

FR:

The server exposes a performance_schema table to aggregates statistics for all executions of a given operation.

FR:

The server exposes a performance_schema table to aggregates statistics for all operations on a given type.

## Documentation

### Reference Manual

NFR:

Every aspect related to using a type in an application (Installing a type, DDL, DML, GRANT, REVOKE)
is documented in the reference manual.

For reference: https://dev.mysql.com/doc/refman/26.10/en/

### Doxygen documentation

NFR:

Every aspect related to providing a type to an application (implementing
a component) is documented in the internal doxygen documentation.

For reference: https://dev.mysql.com/doc/dev/mysql-server/latest/

